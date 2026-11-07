#!/usr/bin/env python3
#
# The control channel (T2R-CTL) and the mbus-ctl utility against a running gateway on a pty pair (a fake RTU
# slave on the master side), loopback only, free ports, the control socket in a temporary directory:
#
#   tests/ctl_test.py [--gw ./build-ctl/mbusgw-t2r] [--ctl ./build-ctl/mbus-ctl]
#
#   proto	- the bare socket protocol (AVProto TLV, src/t2r_ctlproto.h): SHOW GATEWAY/LINES/CONNECTIONS, the sequence
#		  number, an unknown command code, a bad argument type, a wrong magic, a too long PDU and a text
#		  line are not answered, the counters
#   util	- mbus-ctl: the abbreviations (sho conn, sho gw, sho ser, sho lin), the tables, exit statuses
#   cont	- mbus-ctl SHOW CONNECTIONS /CONTINUOUS=1 redraws until SIGINT, exits 0
#   reset	- RESET CONNECTIONS by id, by address:port, by address, /ALL; the clients get a FIN, CTLRESET in the log
#   raw		- a RAW listener: its session and the live octet counters
#   restart	- RESTART GATEWAY: the same pid, a new start time, the clients are dropped, a new client works;
#		  a broken settings file: RESTART is refused, the gateway goes on
#   sock	- the socket rights 0660, a stale socket of a killed gateway is replaced, a second gateway on the same
#		  socket goes on without the control channel (CTLBUSY) and does not steal it; /CONTROL=NONE
#
# SPDX-License-Identifier: Apache-2.0
#
# MODIFICATION HISTORY:
#	06-NOV-2026	CLD	V.01-01: Initial revision.
#	07-NOV-2026	CLD	V.01-01: The control channel speaks AVProto TLV: request() builds and parses the PDUs,
#				the socat test is gone (no text protocol), proto checks the PDU framing.
#
import argparse, os, pty, re, select, shutil, signal, socket, stat, struct, subprocess, sys, tempfile, threading, time, tty

ap = argparse.ArgumentParser()
ap.add_argument('--gw', default='./build-ctl/mbusgw-t2r')
ap.add_argument('--ctl', default='./build-ctl/mbus-ctl')
ap.add_argument('tests', nargs='*', default=['proto', 'util', 'cont', 'reset', 'raw', 'restart', 'sock'])
a = ap.parse_args()
GW, CTL = os.path.abspath(a.gw), os.path.abspath(a.ctl)
TMP = tempfile.mkdtemp(prefix='t2r_ctl_')


def crc16(d):
    crc = 0xFFFF
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Line:
    """A pty pair: the gateway opens the slave side by name, the fake device works on the master side."""

    def __init__(self):
        self.m, self.s = pty.openpty()
        tty.setraw(self.m)
        tty.setraw(self.s)
        self.name = os.ttyname(self.s)
        self.stop = False

    def close(self):
        self.stop = True
        time.sleep(0.1)
        for fd in (self.m, self.s):
            try:
                os.close(fd)
            except OSError:
                pass


def rtu_slave(line):
    """A fake RTU device, unit 1: FC 3 answers the register numbers; runs in a thread."""
    def run():
        buf = b''
        while not line.stop:
            if not select.select([line.m], [], [], 0.05)[0]:
                continue
            try:
                buf += os.read(line.m, 512)
            except OSError:
                return
            while len(buf) >= 8:
                req, buf = buf[:8], buf[8:]
                if crc16(req[:-2]) != req[-2] | (req[-1] << 8):
                    buf = b''
                    break
                sa, qty = struct.unpack('>HH', req[2:6])
                pdu = bytes([req[0], 3, 2 * qty]) + b''.join(struct.pack('>H', sa + i) for i in range(qty))
                c = crc16(pdu)
                try:
                    os.write(line.m, pdu + bytes([c & 0xFF, c >> 8]))
                except OSError:
                    return
    threading.Thread(target=run, daemon=True).start()


def echo_dev(line):
    """A RAW device: echoes everything back."""
    def run():
        while not line.stop:
            if select.select([line.m], [], [], 0.05)[0]:
                try:
                    os.write(line.m, os.read(line.m, 512))
                except OSError:
                    return
    threading.Thread(target=run, daemon=True).start()


class Gateway:
    def __init__(self, name, lines, raw=None, control=None, extra_args=()):
        """lines: [(pty name, tcp port)] MODBUS listeners; raw: (pty name, tcp port) of a RAW listener."""
        self.conf = os.path.join(TMP, f'{name}.conf')
        self.log = os.path.join(TMP, f'{name}.log')
        self.sock = control if control is not None else os.path.join(TMP, f'{name}.sock')
        lsn = [f'{{ bind = "TCP:127.0.0.1:{p}"; connlm = 8; target = "{d}"; }}' for d, p in lines]
        ser = [f'{{ device = "{d}"; chars = "115200, 8, N, 1"; iotmo = 500; }}' for d, _ in lines]
        if raw:
            lsn.append(f'{{ bind = "TCP:127.0.0.1:{raw[1]}"; mode = "RAW"; target = "{raw[0]}"; }}')
            ser.append(f'{{ device = "{raw[0]}"; chars = "115200, 8, N, 1"; }}')
        self.write_conf(f'listeners = ( {", ".join(lsn)} );\nserials = ( {", ".join(ser)} );\n')
        self.p = subprocess.Popen([GW, '/settings=' + self.conf, '/control=' + self.sock, *extra_args],
                                  stdout=open(self.log, 'w'), stderr=subprocess.STDOUT)
        self.wait_log('CTLREADY|CTLBUSY|CTLOFF|CTLERR')

    def write_conf(self, text):
        open(self.conf, 'w').write(text)

    def wait_log(self, pattern, secs=5, count=1):
        t0 = time.time()
        while time.time() - t0 < secs:
            if len(re.findall(pattern, self.text())) >= count:
                return True
            time.sleep(0.05)
        return False

    def text(self):
        return open(self.log, errors='replace').read()

    def stop(self, sig=signal.SIGTERM):
        self.p.send_signal(sig)
        try:
            self.p.wait(10)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()


# The control protocol, see src/t2r_ctlproto.h: HEADER = magic[8] u_len u_csr u_seq, TLV = [type:4|id:12] len value
MAGIC = b'MBUSCTL'
HDR = struct.Struct('>8sIII')
WORD, LONGWORD, QWORD, BBLOCK = 3, 4, 5, 0
CMD = dict(SHOW_GATEWAY=1, SHOW_LINES=2, SHOW_CONNECTIONS=3, RESET_CONNECTIONS=4, RESTART_GATEWAY=5)
REC = {1: 'GATEWAY', 2: 'LISTENER', 3: 'LINE', 4: 'CONN', 5: 'RESET', 6: 'CHECK'}
STATE = ['DOWN', 'UP', 'CLOSED', 'ABSENT', 'OPEN']
TAGS = {1: 'rec', 2: 'reason', 3: 'text', 4: 'count', 5: 'all', 6: 'cut',
        16: 'ident', 17: 'rev', 18: 'arch', 19: 'pid', 20: 'started', 21: 'now', 22: 'settings', 23: 'control',
        24: 'lines', 25: 'listeners', 26: 'up', 27: 'connections',
        32: 'lsn', 33: 'addr', 34: 'port', 35: 'mode', 36: 'target', 37: 'state', 38: 'connlm', 39: 'idletmo',
        40: 'sessions', 41: 'accepted', 42: 'rejected',
        48: 'line', 49: 'device', 50: 'baud', 51: 'databits', 52: 'parity', 53: 'stopbits', 54: 'iotmo', 55: 'lategd',
        56: 'rs485', 57: 'addts',
        64: 'id', 65: 'sd', 66: 'since', 67: 'reset', 68: 'nreq', 69: 'nans', 70: 'nerr', 71: 'nrx', 72: 'ntx'}
TAG = {v: k for k, v in TAGS.items()}
FMT = {WORD: '>H', LONGWORD: '>I', QWORD: '>Q'}


def tlv(name, typ, val):
    v = val.encode() if isinstance(val, str) else val if typ == BBLOCK else struct.pack(FMT[typ], val)
    return struct.pack('>HH', (typ << 12) | TAG[name], len(v)) + v


def pdu(csr, *tlvs, seq=1, magic=MAGIC):
    body = b''.join(tlvs)
    return HDR.pack(magic, len(body), csr, seq) + body


def exchange(path, data, timeout=15, shut=False):
    """Send octets, return all the octets of the answer (b'' - closed without an answer)."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(path)
    s.sendall(data)
    if shut:
        s.shutdown(socket.SHUT_WR)
    d = b''
    while True:
        c = s.recv(65536)
        if not c:
            break
        d += c
    s.close()
    return d


def parse(d):
    """An answer PDU -> (status, seq, records [(type, {field: value})], {reason, count, cut})."""
    magic, ln, csr, seq = HDR.unpack(d[:HDR.size])
    assert magic == MAGIC.ljust(8, b'\0') and ln == len(d) - HDR.size, f'bad header {magic} {ln} {len(d)}'
    recs, top, pos, body = [], {}, 0, d[HDR.size:]
    while pos < len(body):
        tag, vl = struct.unpack('>HH', body[pos:pos + 4])
        typ, name, v = tag >> 12, TAGS.get(tag & 0xfff, tag & 0xfff), body[pos + 4:pos + 4 + vl]
        pos += 4 + vl
        v = v.decode(errors='replace') if typ == BBLOCK else struct.unpack(FMT[typ], v)[0]
        if name == 'rec':
            recs.append((REC.get(v, v), {}))
        elif name in ('reason', 'count', 'cut') or not recs:
            top[name] = v
        else:
            recs[-1][1][name] = v
    return csr, seq, recs, top


def request(path, cmd, *tlvs, seq=7, timeout=15):
    """One request: return (status, [records], {top fields}); the sequence number must come back."""
    csr, s, recs, top = parse(exchange(path, pdu(CMD[cmd], *tlvs, seq=seq), timeout))
    assert s == seq, f'seq {s} != {seq}'
    return csr, recs, top


def recs_of(recs, typ):
    return [f for t, f in recs if t == typ]


def gwrec(gw):
    """The GATEWAY record of SHOW GATEWAY."""
    csr, recs, _ = request(gw.sock, 'SHOW_GATEWAY')
    g = recs_of(recs, 'GATEWAY')
    return g[0] if csr & 1 and g else {}


def ip(f):
    a = f.get('addr', 0)
    return f'{a >> 24 & 255}.{a >> 16 & 255}.{a >> 8 & 255}.{a & 255}:{f.get("port")}'


class Client:
    def __init__(self, port):
        self.sk = socket.create_connection(('127.0.0.1', port), timeout=5)
        self.port = self.sk.getsockname()[1]
        self.tid = 0

    def read(self, sa, qty=1):
        self.tid += 1
        self.sk.sendall(struct.pack('>HHHBBHH', self.tid, 0, 6, 1, 3, sa, qty))
        hdr = self.recvn(7)
        ln = struct.unpack('>H', hdr[4:6])[0]
        return hdr + self.recvn(ln - 1)

    def recvn(self, n):
        d = b''
        while len(d) < n:
            c = self.sk.recv(n - len(d))
            if not c:
                raise ConnectionError('closed by the gateway')
            d += c
        return d

    def closed(self, secs=3):
        """True if the gateway has closed the connection (EOF or reset) in <secs>."""
        self.sk.settimeout(secs)
        try:
            return self.sk.recv(1) == b''
        except ConnectionResetError:
            return True
        except socket.timeout:
            return False

    def close(self):
        self.sk.close()


def ctl(*args, timeout=60):
    r = subprocess.run([CTL, *args], capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


RESULTS = []


def check(name, ok, text=''):
    RESULTS.append(ok)
    print(f'PASS  {name}' if ok else f'FAIL  {name}: {text}', flush=True)		# The details of a failure only
    return ok


def setup(name, raw=False, **kw):
    ln = Line()
    rtu_slave(ln)
    port = free_port()
    rl = rport = None
    if raw:
        rl = Line()
        echo_dev(rl)
        rport = free_port()
    gw = Gateway(name, [(ln.name, port)], raw=(rl.name, rport) if raw else None, **kw)
    return gw, ln, port, rl, rport


# ---------------------------------------------------------------------------------------------------------------------
def t_proto():
    gw, ln, port, _, _ = setup('proto')
    csr, recs, _ = request(gw.sock, 'SHOW_GATEWAY', seq=12345)
    g = recs_of(recs, 'GATEWAY')
    g = g[0] if g else {}
    check('proto SHOW GATEWAY (status, seq echoed)', csr & 1 and g.get('pid') == gw.p.pid and g.get('control') == gw.sock
          and g.get('up') == 1 and g.get('ident', '').startswith('V.'), str(recs[:1]))
    lsn = recs_of(recs, 'LISTENER')
    check('proto SHOW GATEWAY LISTENER', len(lsn) == 1 and ip(lsn[0]) == f'127.0.0.1:{port}' and STATE[lsn[0]['state']] == 'UP', str(lsn))
    csr, recs, _ = request(gw.sock, 'SHOW_LINES')
    lines = recs_of(recs, 'LINE')
    check('proto SHOW LINES', csr & 1 and len(lines) == 1 and lines[0].get('device') == ln.name
          and STATE[lines[0]['state']] == 'OPEN' and lines[0].get('baud') == 115200 and chr(lines[0].get('parity', 0)) == 'N', str(lines))
    csr, s_, recs, top = parse(exchange(gw.sock, pdu(99, seq=5)))
    check('proto unknown command code: a failure status and REASON', not csr & 1 and 'unknown' in top.get('reason', '') and s_ == 5, str(top))
    csr, recs, top = request(gw.sock, 'RESET_CONNECTIONS', tlv('id', BBLOCK, '7'))
    check('proto a bad argument type: refused', not csr & 1 and 'bad ID' in top.get('reason', ''), str(top))
    check('proto wrong magic: not answered', exchange(gw.sock, pdu(1, magic=b'NOTCTL')) == b'')
    check('proto too long PDU: not answered', exchange(gw.sock, HDR.pack(MAGIC, 5000, 1, 1)) == b'')
    check('proto a text line is not answered', exchange(gw.sock, b'SHOW CONNECTIONS\n', shut=True) == b'')

    c1, c2 = Client(port), Client(port)
    for _ in range(5):
        c1.read(0, 2)
    c2.read(10, 1)
    csr, recs, _ = request(gw.sock, 'SHOW_CONNECTIONS')
    conns = {f['port']: f for f in recs_of(recs, 'CONN')}
    a1, a2 = conns.get(c1.port, {}), conns.get(c2.port, {})
    # A request: 12 octets; an answer of 2 registers: 7 + 1 + 1 + 4 = 13, of 1 register: 11
    check('proto SHOW CONNECTIONS counters', csr & 1 and len(conns) == 2
          and (a1.get('nreq'), a1.get('nans'), a1.get('nerr'), a1.get('nrx'), a1.get('ntx')) == (5, 5, 0, 60, 65)
          and (a2.get('nreq'), a2.get('nrx'), a2.get('ntx')) == (1, 12, 11) and ip(a1) == f'127.0.0.1:{c1.port}', f'{a1} {a2}')
    lsn = recs_of(recs, 'LISTENER')
    check('proto LISTENER accepted/sessions', len(lsn) == 1 and lsn[0]['sessions'] == 2 and lsn[0]['accepted'] == 2, str(lsn))
    c1.close(); c2.close()
    gw.stop(); ln.close()
    check('proto socket removed at the exit', not os.path.exists(gw.sock))


def t_util():
    gw, ln, port, _, _ = setup('util')
    c = Client(port)
    c.read(0, 1)
    rc, out = ctl('sho', 'conn', '/socket=' + gw.sock)
    check('util sho conn', rc == 0 and f'127.0.0.1:{c.port}' in out and 'Total: 1 connection(s)' in out, out)
    rc, out = ctl('show', 'connections', '--socket=' + gw.sock)
    check('util --socket=', rc == 0 and f'127.0.0.1:{c.port}' in out, out[-200:])
    rc, out = ctl('sho', 'gw', '/soc=' + gw.sock)
    check('util sho gw', rc == 0 and f'pid {gw.p.pid}' in out and f'127.0.0.1:{port}' in out, out)
    rc, out = ctl('sh', 'gat', '/socket=' + gw.sock)
    check('util sh gat', rc == 0 and f'pid {gw.p.pid}' in out, out[:200])
    for w in ('lin', 'ser', 'serials'):
        rc, out = ctl('sho', w, '/socket=' + gw.sock)
        check(f'util sho {w}', rc == 0 and ln.name in out and 'OPEN' in out, out)
    rc, out = ctl('sho', 'conn', '/socket=' + os.path.join(TMP, 'nope.sock'))
    check('util no gateway: CTLNOGW, exit 1', rc == 1 and 'CTLNOGW' in out, out)
    rc, out = ctl('res', 'conn', '1', '/socket=' + gw.sock)
    check('util "res" is ambiguous (RESET/RESTART)', rc != 0, out[-200:])
    rc, out = ctl('help')
    check('util help', rc == 0 and 'RESTART GATEWAY' in out)
    rc, out = ctl('show', '/help')
    check('util show /help', rc == 0 and 'CONNECTIONS' in out, out)
    c.close()
    gw.stop(); ln.close()


def t_cont():
    gw, ln, port, _, _ = setup('cont')
    c = Client(port)
    p = subprocess.Popen([CTL, 'sho', 'conn', '/cont=1', '/socket=' + gw.sock], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(3.5)
    p.send_signal(signal.SIGINT)
    try:
        out, _ = p.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
    frames = out.count('Connections of the MODBUS gateway')
    check('cont /cont=1 redraws until Ctrl-C', p.returncode == 0 and frames >= 3, f'rc {p.returncode}, {frames} frames')
    # bare /CONTINUOUS (the default interval), the gateway goes away under it: it waits, does not exit
    p = subprocess.Popen([CTL, 'sho', 'conn', '/continuous', '/socket=' + gw.sock], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(1)
    c.close()
    gw.stop()
    time.sleep(3)
    alive = p.poll() is None
    p.send_signal(signal.SIGINT)
    out, _ = p.communicate(timeout=5)
    check('cont bare /continuous survives the gateway exit', alive and p.returncode == 0 and 'waiting' in out, out[-300:])
    ln.close()


def t_reset():
    gw, ln, port, _, _ = setup('reset')
    cs = [Client(port) for _ in range(4)]
    for c in cs:
        c.read(0)
    _, recs, _ = request(gw.sock, 'SHOW_CONNECTIONS')
    ids = {f['port']: str(f['id']) for f in recs_of(recs, 'CONN')}
    rc, out = ctl('reset', 'conn', ids[cs[0].port], '/socket=' + gw.sock)
    check('reset conn <id>', rc == 0 and '1 connection(s) reset' in out and cs[0].closed(), out)
    rc, out = ctl('rese', 'connection', f'127.0.0.1:{cs[1].port}', '/socket=' + gw.sock)
    check('reset conn <addr:port>', rc == 0 and cs[1].closed() and not cs[2].closed(0.3), out)
    rc, out = ctl('reset', 'connect', '9999', '/socket=' + gw.sock)
    check('reset unknown id: ERROR, exit 1', rc == 1 and 'no connection matches' in out, out)
    rc, out = ctl('reset', 'conn', '/socket=' + gw.sock)
    check('reset without a selector: CTLBADSEL', rc == 1 and 'CTLBADSEL' in out, out)
    rc, out = ctl('reset', 'conn', '1.2.3', '/socket=' + gw.sock)
    check('reset bad address: CTLBADSEL', rc == 1 and 'CTLBADSEL' in out, out)
    rc, out = ctl('reset', 'conn', '127.0.0.1', '/socket=' + gw.sock)
    check('reset conn <addr> (2 left)', rc == 0 and '2 connection(s) reset' in out and cs[2].closed() and cs[3].closed(), out)
    cs = [Client(port) for _ in range(3)]
    rc, out = ctl('reset', 'conn', '/all', '/socket=' + gw.sock)
    check('reset conn /all', rc == 0 and '3 connection(s) reset' in out and all(c.closed() for c in cs), out)
    time.sleep(0.5)
    _, recs, _ = request(gw.sock, 'SHOW_CONNECTIONS')
    check('reset: the registry is empty', not recs_of(recs, 'CONN'), str(recs))
    t = gw.text()
    check('reset: CTLRESET logged, no NETPOLLEV', t.count('CTLRESET') == 7 and 'NETPOLLEV' not in t,
          f'CTLRESET {t.count("CTLRESET")}, NETPOLLEV {t.count("NETPOLLEV")}')
    c = Client(port)
    check('reset: a new client works', len(c.read(5, 1)) == 11)
    c.close()
    gw.stop(); ln.close()


def t_raw():
    gw, ln, port, rl, rport = setup('raw', raw=True)
    sk = socket.create_connection(('127.0.0.1', rport), timeout=5)
    sk.sendall(b'x' * 100)
    got = b''
    while len(got) < 100:
        got += sk.recv(1000)
    time.sleep(0.2)
    _, recs, _ = request(gw.sock, 'SHOW_CONNECTIONS')
    conn = [f for f in recs_of(recs, 'CONN') if f['mode'] == 1]
    check('raw live counters', len(conn) == 1 and conn[0]['nrx'] == 100 and conn[0]['ntx'] == 100, str(conn))
    rc, out = ctl('sho', 'conn', '/socket=' + gw.sock)
    check('raw display', rc == 0 and 'RAW' in out and 'To line' in out, out)
    rc, out = ctl('reset', 'conn', str(conn[0]['id']) if conn else '0', '/socket=' + gw.sock)
    sk.settimeout(3)
    try:
        eof = sk.recv(1) == b''
    except (ConnectionResetError, socket.timeout):
        eof = False
    check('raw reset', rc == 0 and eof, out)
    sk.close()
    gw.stop(); ln.close(); rl.close()


def t_restart():
    gw, ln, port, _, _ = setup('restart')
    started = gwrec(gw)['started']
    time.sleep(1.1)						# A new start time must differ by a second at least
    c = Client(port)
    c.read(0)
    t0 = time.time()
    rc, out = ctl('restart', 'gw', '/socket=' + gw.sock)
    dt = time.time() - t0
    g = gwrec(gw)
    check('restart gw: back, the same pid, a new start time', rc == 0 and 'CTLBACK' in out and g['pid'] == gw.p.pid
          and g['started'] != started and gw.p.poll() is None, f'{dt:.1f} s, {out.strip()}')
    check('restart: the old client is dropped', c.closed(1))
    c = Client(port)
    check('restart: a new client works', len(c.read(1, 1)) == 11)
    c.close()
    check('restart: logged CTLRESTART, RESTARTING, 2 x REVISNF', gw.wait_log('REVISNF', count=2)
          and 'CTLRESTART' in gw.text() and 'RESTARTING' in gw.text())

    good = open(gw.conf).read()
    gw.write_conf(good.replace('listeners', 'listeners = ( {', 1))	# A syntax error
    rc, out = ctl('restart', 'gateway', '/socket=' + gw.sock)
    check('restart with broken settings: refused, exit 1', rc == 1 and 'check has failed' in out and 'CFGPARSE' in out, out)
    time.sleep(4)
    check('restart refused: the gateway goes on', gw.p.poll() is None and gwrec(gw)['pid'] == gw.p.pid)
    gw.write_conf(good)
    rc, out = ctl('restart', 'gw', '/nowait', '/socket=' + gw.sock)
    check('restart /nowait', rc == 0 and 'CTLWAIT' not in out, out)
    time.sleep(0.5)
    ok = False
    for _ in range(40):
        try:
            if gwrec(gw).get('up') == 1:
                ok = True
                break
        except (OSError, AssertionError, struct.error):
            pass
        time.sleep(0.5)
    check('restart /nowait: back', ok and gw.p.poll() is None)
    gw.stop(); ln.close()


def t_sock():
    gw, ln, port, _, _ = setup('sock')
    mode = stat.S_IMODE(os.stat(gw.sock).st_mode)
    check('sock rights 0660', mode == 0o660, oct(mode))
    # A second gateway on the same control socket (other ports, other line): CTLBUSY, the first one keeps it
    ln2 = Line(); rtu_slave(ln2)
    gw2 = Gateway('sock2', [(ln2.name, free_port())], control=gw.sock)
    check('sock second gateway: CTLBUSY, the socket stays with the first', 'CTLBUSY' in gw2.text() and gw2.p.poll() is None
          and gwrec(gw)['pid'] == gw.p.pid, gw2.text()[-300:])
    gw2.stop(); ln2.close()
    # A killed gateway leaves its socket: a new one replaces it
    gw.stop(signal.SIGKILL)
    check('sock kill -9 leaves the socket', os.path.exists(gw.sock))
    gw3 = Gateway('sock3', [(ln.name, port)], control=gw.sock)
    check('sock stale socket is replaced', 'CTLREADY' in gw3.text() and gwrec(gw3)['pid'] == gw3.p.pid)
    gw3.stop()
    # A regular file at the path is never removed
    reg = os.path.join(TMP, 'regular.file')
    open(reg, 'w').write('keep me')
    gw4 = Gateway('sock4', [(ln.name, port)], control=reg)
    check('sock a regular file is left alone: CTLERR, the gateway runs', 'CTLERR' in gw4.text() and open(reg).read() == 'keep me'
          and gw4.p.poll() is None)
    gw4.stop()
    gw5 = Gateway('sock5', [(ln.name, port)], control='NONE')
    check('sock /CONTROL=NONE', 'CTLOFF' in gw5.text() and gw5.p.poll() is None)
    gw5.stop()
    ln.close()


for t in a.tests:
    try:
        globals()['t_' + t]()
    except Exception as e:
        check(t, False, f'exception {e!r}')

print(f'\n{sum(RESULTS)} of {len(RESULTS)} checks passed, logs in {TMP}')
sys.exit(0 if all(RESULTS) else 1)
