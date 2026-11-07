#!/usr/bin/env python3
#
# The control channel (T2R-CTL) and the mbus-ctl utility against a running gateway on a pty pair (a fake RTU
# slave on the master side), loopback only, free ports, the control socket in a temporary directory:
#
#   tests/ctl_test.py [--gw ./build-ctl/mbusgw-t2r] [--ctl ./build-ctl/mbus-ctl]
#
#   proto	- the bare socket protocol: HELP, SHOW GATEWAY/LINES/CONNECTIONS, an unknown request, the counters
#   util	- mbus-ctl: the abbreviations (sho conn, sho gw, sho ser, sho lin), the tables, exit statuses
#   cont	- mbus-ctl SHOW CONNECTIONS /CONTINUOUS=1 redraws until SIGINT, exits 0
#   reset	- RESET CONNECTIONS by id, by address:port, by address, /ALL; the clients get a FIN, CTLRESET in the log
#   raw		- a RAW listener: its session and the live octet counters
#   restart	- RESTART GATEWAY: the same pid, a new start time, the clients are dropped, a new client works;
#		  a broken settings file: RESTART is refused, the gateway goes on
#   sock	- the socket rights 0660, a stale socket of a killed gateway is replaced, a second gateway on the same
#		  socket goes on without the control channel (CTLBUSY) and does not steal it; /CONTROL=NONE
#   socat	- the same requests by socat, if it is installed
#
# SPDX-License-Identifier: Apache-2.0
#
# MODIFICATION HISTORY:
#	06-NOV-2026	CLD	V.01-01: Initial revision.
#
import argparse, os, pty, re, select, shutil, signal, socket, stat, struct, subprocess, sys, tempfile, threading, time, tty

ap = argparse.ArgumentParser()
ap.add_argument('--gw', default='./build-ctl/mbusgw-t2r')
ap.add_argument('--ctl', default='./build-ctl/mbus-ctl')
ap.add_argument('tests', nargs='*', default=['proto', 'util', 'cont', 'reset', 'raw', 'restart', 'sock', 'socat'])
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


def request(path, line, timeout=15):
    """One request: return (final line, [data lines])."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(path)
    s.sendall(line.encode() + b'\n')
    d = b''
    while True:
        c = s.recv(65536)
        if not c:
            break
        d += c
    s.close()
    ls = d.decode().splitlines()
    return ls[-1], ls[:-1]


def kv(line):
    return dict(w.split('=', 1) for w in line.split()[1:] if '=' in w)


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
    fin, data = request(gw.sock, 'HELP')
    check('proto HELP', fin == 'OK' and len(data) == 6 and all(d.startswith('HELP ') for d in data), f'{len(data)} lines')
    fin, data = request(gw.sock, 'show gateway')
    g = kv(data[0]) if data else {}
    check('proto SHOW GATEWAY (case-insensitive)', fin == 'OK' and data[0].startswith('GATEWAY ')
          and g.get('pid') == str(gw.p.pid) and g.get('control') == gw.sock and g.get('up') == '1', data[0] if data else fin)
    check('proto SHOW GATEWAY LISTENER', len(data) == 2 and kv(data[1]).get('bind') == f'127.0.0.1:{port}', data[-1])
    fin, data = request(gw.sock, 'SHOW LINES')
    check('proto SHOW LINES', fin == 'OK' and len(data) == 1 and kv(data[0]).get('dev') == ln.name
          and kv(data[0]).get('state') == 'OPEN', data[0] if data else fin)
    fin, _ = request(gw.sock, 'SHOW NOTHING')
    check('proto unknown request', fin.startswith('ERROR'), fin)
    fin, _ = request(gw.sock, 'SHOW CONN')
    check('proto no abbreviations on the wire', fin.startswith('ERROR'), fin)

    c1, c2 = Client(port), Client(port)
    for _ in range(5):
        c1.read(0, 2)
    c2.read(10, 1)
    fin, data = request(gw.sock, 'SHOW CONNECTIONS')
    conns = {int(kv(d)['peer'].split(':')[1]): kv(d) for d in data if d.startswith('CONN ')}
    a1, a2 = conns.get(c1.port, {}), conns.get(c2.port, {})
    # A request: 12 octets; an answer of 2 registers: 7 + 1 + 1 + 4 = 13, of 1 register: 11
    check('proto SHOW CONNECTIONS counters', fin == 'OK' and len(conns) == 2
          and (a1.get('req'), a1.get('ans'), a1.get('err'), a1.get('rx'), a1.get('tx')) == ('5', '5', '0', '60', '65')
          and (a2.get('req'), a2.get('rx'), a2.get('tx')) == ('1', '12', '11'), f'{a1} {a2}')
    lsn = [kv(d) for d in data if d.startswith('LISTENER ')]
    check('proto LISTENER accepted/sessions', len(lsn) == 1 and lsn[0]['sessions'] == '2' and lsn[0]['accepted'] == '2', str(lsn))
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
    _, data = request(gw.sock, 'SHOW CONNECTIONS')
    ids = {int(kv(d)['peer'].split(':')[1]): kv(d)['id'] for d in data if d.startswith('CONN ')}
    rc, out = ctl('reset', 'conn', ids[cs[0].port], '/socket=' + gw.sock)
    check('reset conn <id>', rc == 0 and '1 connection(s) reset' in out and cs[0].closed(), out)
    rc, out = ctl('rese', 'connection', f'127.0.0.1:{cs[1].port}', '/socket=' + gw.sock)
    check('reset conn <addr:port>', rc == 0 and cs[1].closed() and not cs[2].closed(0.3), out)
    rc, out = ctl('reset', 'connect', '9999', '/socket=' + gw.sock)
    check('reset unknown id: ERROR, exit 1', rc == 1 and 'no connection matches' in out, out)
    rc, out = ctl('reset', 'conn', '/socket=' + gw.sock)
    check('reset without a selector', rc == 1, out)
    rc, out = ctl('reset', 'conn', '127.0.0.1', '/socket=' + gw.sock)
    check('reset conn <addr> (2 left)', rc == 0 and '2 connection(s) reset' in out and cs[2].closed() and cs[3].closed(), out)
    cs = [Client(port) for _ in range(3)]
    rc, out = ctl('reset', 'conn', '/all', '/socket=' + gw.sock)
    check('reset conn /all', rc == 0 and '3 connection(s) reset' in out and all(c.closed() for c in cs), out)
    time.sleep(0.5)
    fin, data = request(gw.sock, 'SHOW CONNECTIONS')
    check('reset: the registry is empty', not [d for d in data if d.startswith('CONN ')], str(data))
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
    _, data = request(gw.sock, 'SHOW CONNECTIONS')
    conn = [kv(d) for d in data if d.startswith('CONN ') and kv(d)['mode'] == 'RAW']
    check('raw live counters', len(conn) == 1 and conn[0]['rx'] == '100' and conn[0]['tx'] == '100', str(conn))
    rc, out = ctl('sho', 'conn', '/socket=' + gw.sock)
    check('raw display', rc == 0 and 'RAW' in out and 'To line' in out, out)
    rc, out = ctl('reset', 'conn', conn[0]['id'] if conn else '0', '/socket=' + gw.sock)
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
    _, data = request(gw.sock, 'SHOW GATEWAY')
    started = kv(data[0])['started']
    time.sleep(1.1)						# A new start time must differ by a second at least
    c = Client(port)
    c.read(0)
    t0 = time.time()
    rc, out = ctl('restart', 'gw', '/socket=' + gw.sock)
    dt = time.time() - t0
    _, data = request(gw.sock, 'SHOW GATEWAY')
    g = kv(data[0])
    check('restart gw: back, the same pid, a new start time', rc == 0 and 'CTLBACK' in out and g['pid'] == str(gw.p.pid)
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
    fin, data = request(gw.sock, 'SHOW GATEWAY')
    check('restart refused: the gateway goes on', fin == 'OK' and gw.p.poll() is None and kv(data[0])['pid'] == str(gw.p.pid))
    gw.write_conf(good)
    rc, out = ctl('restart', 'gw', '/nowait', '/socket=' + gw.sock)
    check('restart /nowait', rc == 0 and 'CTLWAIT' not in out, out)
    time.sleep(0.5)
    ok = False
    for _ in range(40):
        try:
            fin, data = request(gw.sock, 'SHOW GATEWAY')
            if fin == 'OK' and kv(data[0])['up'] == '1':
                ok = True
                break
        except OSError:
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
    _, data = request(gw.sock, 'SHOW GATEWAY')
    check('sock second gateway: CTLBUSY, the socket stays with the first', 'CTLBUSY' in gw2.text() and gw2.p.poll() is None
          and kv(data[0])['pid'] == str(gw.p.pid), gw2.text()[-300:])
    gw2.stop(); ln2.close()
    # A killed gateway leaves its socket: a new one replaces it
    gw.stop(signal.SIGKILL)
    check('sock kill -9 leaves the socket', os.path.exists(gw.sock))
    gw3 = Gateway('sock3', [(ln.name, port)], control=gw.sock)
    _, data = request(gw.sock, 'SHOW GATEWAY')
    check('sock stale socket is replaced', 'CTLREADY' in gw3.text() and kv(data[0])['pid'] == str(gw3.p.pid))
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


def t_socat():
    if not shutil.which('socat'):
        return check('socat (not installed, skipped)', True)
    gw, ln, port, _, _ = setup('socat')
    r = subprocess.run(f'echo "SHOW CONNECTIONS" | socat - UNIX-CONNECT:{gw.sock}', shell=True, capture_output=True, text=True, timeout=10)
    check('socat SHOW CONNECTIONS', r.returncode == 0 and r.stdout.rstrip().endswith('OK') and 'LISTENER ' in r.stdout, r.stdout)
    gw.stop(); ln.close()


for t in a.tests:
    try:
        globals()['t_' + t]()
    except Exception as e:
        check(t, False, f'exception {e!r}')

print(f'\n{sum(RESULTS)} of {len(RESULTS)} checks passed, logs in {TMP}')
sys.exit(0 if all(RESULTS) else 1)
