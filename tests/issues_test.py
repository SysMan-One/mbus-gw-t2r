#!/usr/bin/env python3
#
# Self-contained reproducers of the field reports and fixed bugs (pty pair + fake RTU slave + gateway + MBAP clients):
#
#   tests/issues_test.py [--gw ./build/mbusgw-t2r] icrnl|late|replug|rawreplug|gone|flow|settings|all
#
#   icrnl	- the gateway's tty is left with ICRNL (a fresh port), the answers carry 0x0D octets: no CRC16ERR expected
#   late	- the slave answers a bit later than <iotmo>, two clients write and read back their own registers:
#		  no foreign data with the success status is expected
#   rawreplug	- the same for a RAW listener: no device - the client gets a FIN at once, a running session is closed
#   gone	- V.01-01ECO01: a client which closes while its request waits for a busy line (a dead unit ahead
#		  of it) is dropped - no line I/O, its <connlm> slot is free for the next client; a half-closed
#		  client (shutdown(SHUT_WR)) gets its answer on a free line, not after such a wait; <idletmo>
#		  still ends an idle session after an answer
#   flow	- V.01-01ECO04: <flow> NONE, RTS/CTS, XON/XOFF (RAW) - the termios of the gateway's tty (a pty has no CTS
#		  line: the handshake itself is not tested); a RAW client closed (a FIN) while an XOFF stops the pty
#		  frees it (RAWSTALL; a real UART takes a page into its driver first) and the next session goes out; DSR/DTR, an unknown value, a number, RTS/CTS + rs485 and XON/XOFF under MODBUS are refused
#   settings	- V.01-03: a file with several mistakes - all are told with their lines (yaLIBCFG schema, CFG-E-...)
#		  and the file is refused (CFGREJECT); names regardless of case, comments
#   replug	- the device behind a symlink disappears (the pty master is closed) and comes back (a new pty): the busy
#		  and the idle cases, and the device absent at the start; exception 0x0A while it is away, normal answers after
#
# SPDX-License-Identifier: Apache-2.0
#
# MODIFICATION HISTORY:
#	02-NOV-2026	CLD	X.00-13ECO07: Initial revision.
#	02-NOV-2026	CLD	X.00-14ECO02: rawreplug - a RAW listener with an absent / unplugged / returned device.
#	17-NOV-2026	CLD	V.01-01ECO01: gone - a client gone while waiting for a busy line, half-close, idletmo.
#	22-NOV-2026	CLD	V.01-01ECO04: flow - the <flow> key of a serial.
#	22-NOV-2026	CLD	V.01-02ECO01: flow - the RAW client closes by itself (a FIN), no takeover before RAWSTALL.
#	22-NOV-2026	CLD	V.01-03: flow - the refused settings are told by the yaLIBCFG schema (CFG-E-..., CFGREJECT).
#
import argparse, os, pty, random, select, socket, struct, subprocess, sys, tempfile, termios, threading, time, tty

ap = argparse.ArgumentParser()
ap.add_argument('--gw', default='./build/mbusgw-t2r')
ap.add_argument('--port', type=int, default=5602)
ap.add_argument('--secs', type=int, default=30)
ap.add_argument('--lategd', type=int, default=None, help='late: <lategd> of the serial, the gateway default if omitted')
ap.add_argument('test', choices=['icrnl', 'late', 'replug', 'rawreplug', 'gone', 'flow', 'settings', 'all'])
a = ap.parse_args()
GW = os.path.abspath(a.gw)
TMP = tempfile.mkdtemp(prefix='t2r_issues_')


def crc16(d):
    crc = 0xFFFF
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def rtu(pdu):
    c = crc16(pdu)
    return pdu + bytes([c & 0xFF, c >> 8])


class Line:
    """A pty pair: the gateway opens the slave side by name, the fake RTU device works on the master side."""

    def __init__(self, raw_slave=True):
        self.m, self.s = pty.openpty()
        tty.setraw(self.m)
        if raw_slave:
            tty.setraw(self.s)
        else:					# A fresh port: cooked, ICRNL is on
            at = termios.tcgetattr(self.s)
            at[0] |= termios.ICRNL
            termios.tcsetattr(self.s, termios.TCSANOW, at)
        self.name = os.ttyname(self.s)

    def close(self):
        if getattr(self, 'slave', None):	# A read() blocked in another thread keeps the master open: stop it first
            self.slave.stop = True
            self.slave.join()
        for fd in (self.m, self.s):
            try:
                os.close(fd)
            except OSError:
                pass


class Slave(threading.Thread):
    """A fake RTU device (unit 1): holding registers, FC 3, 6, 16; an optional answer delay; units in mute never answer."""

    def __init__(self, line, regs=None, delay=None, mute=()):
        super().__init__(daemon=True)
        self.line, self.regs, self.delay, self.mute = line, dict(regs or {}), delay, set(mute)
        self.answered, self.stop = 0, False
        self.muted = threading.Event()		# Set when a frame to a mute unit has arrived
        line.slave = self
        self.start()

    def run(self):
        buf = b''
        while not self.stop:
            if not select.select([self.line.m], [], [], 0.05)[0]:
                continue
            try:
                d = os.read(self.line.m, 512)
            except OSError:
                return				# The master is closed - the device is "unplugged"
            buf += d
            while len(buf) >= 8:
                fc = buf[1]
                n = 9 + buf[6] if fc == 16 else 8
                if len(buf) < n:
                    break
                req, buf = buf[:n], buf[n:]
                if crc16(req[:-2]) != req[-2] | (req[-1] << 8):
                    buf = b''
                    continue
                if req[0] in self.mute:
                    self.muted.set()
                    continue			# A dead device
                if self.delay:
                    time.sleep(self.delay())
                unit, sa, qty = req[0], *struct.unpack('>HH', req[2:6])
                if fc == 3:
                    pdu = bytes([unit, 3, 2 * qty]) + b''.join(struct.pack('>H', self.regs.get(sa + i, 0)) for i in range(qty))
                elif fc == 6:
                    self.regs[sa] = qty
                    pdu = req[:6]
                elif fc == 16:
                    for i in range(qty):
                        self.regs[sa + i] = struct.unpack('>H', req[7 + 2 * i:9 + 2 * i])[0]
                    pdu = req[:6]
                else:
                    pdu = bytes([unit, fc | 0x80, 1])
                try:
                    os.write(self.line.m, rtu(pdu))
                except OSError:
                    return
                self.answered += 1


class Gateway:
    def __init__(self, device, port, iotmo=1000, extra='', lextra=''):
        self.conf = os.path.join(TMP, f'gw{port}.conf')
        self.log = os.path.join(TMP, f'gw{port}.log')
        open(self.conf, 'w').write(f'''listeners = ( {{ bind = "TCP:127.0.0.1:{port}"; {lextra or 'connlm = 8;'} target = "{device}"; }} );
serials = ( {{ device = "{device}"; chars = "115200, 8, N, 1"; iotmo = {iotmo}; rs485 = 0; {extra} }} );
''')
        self.p = subprocess.Popen([GW, '/settings=' + self.conf], stdout=open(self.log, 'w'), stderr=subprocess.STDOUT)
        time.sleep(0.5)

    def stop(self):
        self.p.terminate()
        try:
            self.p.wait(5)
        except subprocess.TimeoutExpired:
            self.p.kill()

    def count(self, code):
        return open(self.log, errors='replace').read().count(code)


class Client:
    def __init__(self, port):
        self.sk = socket.create_connection(('127.0.0.1', port), timeout=10)
        self.tid = 0

    def req(self, pdu):
        """Return (ok, pdu): ok = False and the exception code in pdu[2] for an exception answer."""
        self.tid = (self.tid + 1) & 0xFFFF
        self.sk.sendall(struct.pack('>HHH', self.tid, 0, len(pdu)) + pdu)
        hdr = self.recvn(7)
        tid, _, ln = struct.unpack('>HHH', hdr[:6])
        body = hdr[6:] + self.recvn(ln - 1)
        assert tid == self.tid, f'tid {tid} != {self.tid}'
        return not (body[1] & 0x80), body

    def recvn(self, n):
        d = b''
        while len(d) < n:
            c = self.sk.recv(n - len(d))
            if not c:
                raise ConnectionError('closed by the gateway')
            d += c
        return d

    def read(self, sa, qty=1):
        return self.req(bytes([1, 3]) + struct.pack('>HH', sa, qty))

    def write(self, sa, val):
        return self.req(bytes([1, 16]) + struct.pack('>HHB', sa, 1, 2) + struct.pack('>H', val))

    def close(self):
        self.sk.close()


def report(name, ok, text):
    print(f'{"PASS" if ok else "FAIL"}  {name}: {text}', flush=True)
    return ok


def t_icrnl():
    line = Line(raw_slave=False)
    Slave(line, regs={i: 0x0D0D for i in range(16)})
    gw = Gateway(line.name, a.port)
    c = Client(a.port)
    bad = sum(1 for _ in range(50) if not c.read(0, 8)[0])
    c.close(); gw.stop(); line.close()
    return report('icrnl', bad == 0 and gw.count('CRC16ERR') == 0, f'{bad} of 50 reads failed, CRC16ERR {gw.count("CRC16ERR")}')


def t_late():
    line = Line()
    Slave(line, delay=lambda: random.uniform(0.180, 0.240))		# iotmo 200: some answers are late by up to 40 msec
    gw = Gateway(line.name, a.port + 1, iotmo=200, extra='' if a.lategd is None else f'lategd = {a.lategd};')
    stats = {'ok': 0, 'exc': 0, 'foreign_w': 0, 'foreign_r': 0}
    lk = threading.Lock()

    def worker(k):
        c = Client(a.port + 1)
        sa, hi = 0x2000 + 4 * k, 0x8000 * k				# Disjoint value ranges: the owner is in bit 15
        end = time.time() + a.secs
        while time.time() < end:
            v = hi | random.randrange(0x8000)
            ok, r = c.write(sa, v)
            with lk:
                if not ok:
                    stats['exc'] += 1
                elif struct.unpack('>H', r[2:4])[0] != sa:
                    stats['foreign_w'] += 1
                else:
                    stats['ok'] += 1
            ok, r = c.read(sa)
            with lk:
                if not ok:
                    stats['exc'] += 1
                elif (struct.unpack('>H', r[3:5])[0] & 0x8000) != hi:
                    stats['foreign_r'] += 1
                else:
                    stats['ok'] += 1
        c.close()

    th = [threading.Thread(target=worker, args=(k,)) for k in (0, 1)]
    [t.start() for t in th]
    [t.join() for t in th]
    gw.stop(); line.close()
    expect = stats['foreign_w'] + stats['foreign_r'] == 0 if a.lategd != 0 else stats['foreign_w'] == 0	# lategd 0: the echo check only
    return report('late', expect,
                  f'{stats}, NOANSWER {gw.count("NOANSWER")}, ECHOMISM {gw.count("ECHOMISM")}, LATEANS {gw.count("LATEANS")}')


def t_replug():
    link = os.path.join(TMP, 'ttyRS485')
    res = True

    def plug():
        ln = Line()
        Slave(ln, regs={0: 0x1234})
        tmp = link + '.new'
        os.symlink(ln.name, tmp)
        os.replace(tmp, link)
        return ln

    def unplug(ln):
        ln.close()
        os.unlink(link)

    def until_ok(c, secs=5):
        t0 = time.time()
        codes = set()
        while time.time() - t0 < secs:
            ok, r = c.read(0)
            if ok:
                return time.time() - t0, codes
            codes.add(r[2])
            time.sleep(0.05)
        return None, codes

    # 1. Busy line: the device goes away under the polling, comes back
    ln = plug()
    gw = Gateway(link, a.port + 2)
    c = Client(a.port + 2)
    ok0 = c.read(0)[0]
    unplug(ln)
    codes = set()
    for _ in range(20):
        ok, r = c.read(0)
        codes.add(None if ok else r[2])
        time.sleep(0.05)
    ln = plug()
    t, _ = until_ok(c)
    res &= report('replug/busy', ok0 and codes == {0x0A} and t is not None,
                  f'answers while away {codes}, back after {t if t is None else round(t, 2)} s')

    # 2. Idle line: nobody talks to the device while it is away
    unplug(ln)
    time.sleep(3)							# The idle check must notice the hangup and close the old descriptor
    lost = gw.count('DEVLOST')
    ln = plug()
    time.sleep(2.5)
    t, codes = until_ok(c, 3)
    res &= report('replug/idle', lost >= 2 and t is not None and t < 0.5,
                  f'DEVLOST {lost} (2 expected by now), first request after the replug {t if t is None else round(t, 2)} s, {codes}')
    c.close(); gw.stop(); unplug(ln)

    # 3. The device is absent at the start: the listener is up anyway
    gw = Gateway(link, a.port + 3)
    try:
        c = Client(a.port + 3)
        ok, r = c.read(0)
        away = (not ok) and r[2] == 0x0A
        ln = plug()
        t, _ = until_ok(c)
        res &= report('replug/start', away and t is not None, f'0x0A while absent: {away}, back after {t if t is None else round(t, 2)} s')
        c.close(); unplug(ln)
    except OSError as e:
        res &= report('replug/start', False, f'no listener: {e}')
    gw.stop()
    return res


class Echo(threading.Thread):
    """A transparent device: every octet comes back."""

    def __init__(self, line):
        super().__init__(daemon=True)
        self.line, self.stop = line, False
        line.slave = self
        self.start()

    def run(self):
        while not self.stop:
            if select.select([self.line.m], [], [], 0.05)[0]:
                try:
                    os.write(self.line.m, os.read(self.line.m, 4096))
                except OSError:
                    return


def t_rawreplug():
    link = os.path.join(TMP, 'ttyRAW')
    res = True

    def plug():
        ln = Line()
        Echo(ln)
        os.symlink(ln.name, link + '.new')
        os.replace(link + '.new', link)
        return ln

    def unplug(ln):
        ln.close()
        os.unlink(link)

    def session(timeout=3):
        """Return the echo of a probe, b'' for a FIN without data."""
        sk = socket.create_connection(('127.0.0.1', a.port + 4), timeout=timeout)
        try:
            sk.sendall(b'probe\r\n')
            d = b''
            while len(d) < 7:
                c = sk.recv(64)
                if not c:
                    break
                d += c
            return d, sk
        except (socket.timeout, ConnectionResetError):
            return None, sk

    gw = Gateway(link, a.port + 4, lextra='mode = "RAW";')			# Absent at the start
    d, sk = session(); sk.close()
    res &= report('rawreplug/absent', d == b'', f'reply {d!r} (a FIN without data expected)')
    ln = plug()
    time.sleep(1.5)
    d, sk = session()
    res &= report('rawreplug/plugged', d == b'probe\r\n', f'reply {d!r}')
    unplug(ln)								# Under a running session
    sk.settimeout(3)
    try:
        c = sk.recv(64)
    except OSError as e:
        c = repr(e).encode()
    sk.close()
    time.sleep(0.5)
    res &= report('rawreplug/unplugged', c == b'' and gw.count('DEVLOST') == 1, f'session end {c!r}, DEVLOST {gw.count("DEVLOST")}')
    ln = plug()
    time.sleep(1.5)
    d, sk = session(); sk.close()
    res &= report('rawreplug/back', d == b'probe\r\n', f'reply {d!r}')
    gw.stop(); unplug(ln)
    return res


def t_gone():
    line = Line()
    sl = Slave(line, regs={0: 0x1234}, mute=(9,))
    port = a.port + 5
    gw = Gateway(line.name, port, iotmo=1500, extra='lategd = 300;', lextra='connlm = 2; idletmo = 3;')
    rd = lambda unit: bytes([unit, 3]) + struct.pack('>HH', 0, 1)
    mbap = lambda pdu: struct.pack('>HHH', 1, 0, len(pdu)) + pdu
    res = True

    def half():
        """Send a request, shutdown(SHUT_WR), read: the answer, b'' for a FIN, None if nothing came in 6 s."""
        sk = socket.create_connection(('127.0.0.1', port), timeout=6)
        try:
            sk.sendall(mbap(rd(1)))
            sk.shutdown(socket.SHUT_WR)
            return sk.recv(64)
        except socket.timeout:
            return None
        except OSError:
            return b''
        finally:
            sk.close()

    def holder():
        """A request to the dead unit 9 holds the line for iotmo + lategd; returns once the line is taken by it."""
        sl.muted.clear()
        c = Client(port)
        threading.Thread(target=lambda: (c.req(rd(9)), c.close()), daemon=True).start()
        if not sl.muted.wait(3):
            raise RuntimeError('the request to the dead unit has not reached the line')
        time.sleep(0.05)

    try:
        d = half()
        res &= report('gone/halfclose-free', d is not None and len(d) == 11 and d[7] == 3, f'answer {d.hex() if d else d}')

        time.sleep(0.5)
        holder()
        n0 = sl.answered
        b = socket.create_connection(('127.0.0.1', port), timeout=5)	# Takes the 2nd <connlm> slot, waits for the line
        b.sendall(mbap(rd(1)))
        time.sleep(0.8)							# More than one lock slice in the queue
        b.close()
        time.sleep(0.8)							# Its session notices it within a slice
        ok, nb = False, sl.answered - n0				# B's request must not have reached the line
        try:
            c = Client(port)						# Would be CONNLIMIT if B still held its slot
            try:
                ok = c.read(0)[0]
            finally:
                c.close()
        except (OSError, ConnectionError):
            pass
        time.sleep(0.3)
        res &= report('gone/queued-close', ok and nb == 0 and sl.answered - n0 == 1 and gw.count('CONNLIMIT') == 0,
                      f'next client ok {ok}, RTU answers {nb} before it and {sl.answered - n0} in all (0 and 1 expected), '
                      f'CONNLIMIT {gw.count("CONNLIMIT")}')

        time.sleep(2)
        holder()
        n0 = sl.answered
        d = half()
        time.sleep(2)							# The holder's hold is over: a request run late would show now
        res &= report('gone/halfclose-busy', d == b'' and sl.answered == n0,
                      f'reply {d.hex() if d else d} (a FIN expected), RTU answers {sl.answered - n0} (0 expected)')

        time.sleep(0.5)
        c = Client(port)
        try:
            c.read(0)
            t = time.monotonic()
            c.sk.settimeout(10)
            try:
                e = c.sk.recv(64)
            except OSError:
                e = None
            dt = time.monotonic() - t
        finally:
            c.close()
        res &= report('gone/idletmo', e == b'' and 2.5 < dt < 8, f'closed after {dt:.1f} s (idletmo 3, checked once a 3 s poll)')
    except (OSError, ConnectionError, RuntimeError, AssertionError) as x:
        res = report('gone', False, f'aborted: {x!r}')
    finally:
        gw.stop(); line.close()
    return res


def t_flow():
    """<flow>: the termios of the gateway's tty (read through the slave fd the test holds), the refused settings (/CHECK)."""
    res = True
    for flow, mode, want_c, want_i in (('NONE', 'MODBUS', 0, 0), ('rts/cts', 'MODBUS', termios.CRTSCTS, 0),
                                       ('XON/XOFF', 'RAW', 0, termios.IXON | termios.IXOFF)):
        line = Line()
        at = termios.tcgetattr(line.s)			# An inherited flow control of the other kind must be cleared
        at[2] |= termios.CRTSCTS
        at[0] |= termios.IXON | termios.IXOFF | termios.IXANY
        termios.tcsetattr(line.s, termios.TCSANOW, at)
        if mode == 'MODBUS':
            Slave(line, regs={0: 0x1113})		# XON/XOFF octets in the answer: fine without IXON
        lx = 'mode = "RAW"; connlm = 1;' if mode == 'RAW' else ''
        gw = Gateway(line.name, a.port, extra=f'flow = "{flow}";', lextra=lx)
        try:
            time.sleep(0.5)
            at = termios.tcgetattr(line.s)
            c_ok = (at[2] & termios.CRTSCTS) == want_c
            i_ok = (at[0] & (termios.IXON | termios.IXOFF | termios.IXANY)) == want_i
            x_ok = flow != 'XON/XOFF' or (at[6][termios.VSTART] == b'\x11' and at[6][termios.VSTOP] == b'\x13')
            d_ok = gw.count('Flow: ' + flow.upper()) == 1
            io = True
            if mode == 'RAW':				# The device sends XOFF, the client goes, the next one must get through
                c = socket.create_connection(('127.0.0.1', a.port), timeout=5)
                time.sleep(0.3)
                os.write(line.m, b'\x13')
                time.sleep(0.3)
                c.sendall(b'stuck')
                time.sleep(0.3)
                c.close()				# A plain close (a FIN), no takeover: RAWSTALL must come by itself
                t = time.monotonic() + 18		# iotmo + 10 s + 1 + 2048 octets at 115200 (1 s)
                while gw.count('RAWSTALL') == 0 and time.monotonic() < t:
                    time.sleep(0.5)
                stall = gw.count('RAWSTALL') == 1 and gw.count('RAWKICK') == 0
                c = socket.create_connection(('127.0.0.1', a.port), timeout=5)
                time.sleep(0.3)
                c.sendall(b'probe')
                got, t = b'', time.monotonic() + 5
                while b'probe' not in got and time.monotonic() < t:
                    if select.select([line.m], [], [], 0.2)[0]:
                        got += os.read(line.m, 256)
                c.close()
                io = b'probe' in got and stall
            if mode == 'MODBUS':
                c = Client(a.port)
                ok, pdu = c.read(0)
                c.close()
                io = ok and pdu[3:5] == b'\x11\x13'
            res &= report(f'flow/{flow}', c_ok and i_ok and x_ok and d_ok and io and gw.count('FLOWNOTSET') == 0,
                          f'CRTSCTS {c_ok}, IXON/IXOFF/IXANY {i_ok}, VSTART/VSTOP {x_ok}, DEVADDED {d_ok}, I/O {io}')
        except (OSError, ConnectionError, AssertionError) as x:
            res &= report(f'flow/{flow}', False, f'aborted: {x!r}')
        finally:
            gw.stop(); line.close()

    for name, ser, lsn, code in (('dsr', 'flow = "DSR/DTR";', '', 'CFG-E-BADKWD'),		# V.01-03: yaLIBCFG, the schema
                                 ('bogus', 'flow = "HW";', '', 'CFG-E-BADKWD'),
                                 ('number', 'flow = 1;', '', 'CFG-E-VTYPE'),
                                 ('rs485', 'flow = "RTS/CTS"; rs485 = 1;', '', 'RS-485 mode drives RTS'),
                                 ('xonmodbus', 'flow = "XON/XOFF";', '', 'a MODBUS listener needs NONE or RTS/CTS')):
        conf = os.path.join(TMP, f'flow_{name}.conf')
        open(conf, 'w').write(f'''listeners = ( {{ bind = "TCP:127.0.0.1:{a.port}"; {lsn} target = "/dev/null"; }} );
serials = ( {{ device = "/dev/null"; chars = "9600, 8, N, 1"; {ser} }} );
''')
        p = subprocess.run([GW, '/settings=' + conf, '/check', '/control=NONE'], capture_output=True, text=True, timeout=10)
        out = p.stdout + p.stderr
        if name == 'rs485' and 'SERNORS485' in out:	# A build without HAVE_TIOCRS485: rs485 is ignored, RTS/CTS is fine
            res &= report('flow/refused-rs485', p.returncode == 0, f'no RS485 support in this build, exit {p.returncode}')
            continue
        res &= report(f'flow/refused-{name}', p.returncode != 0 and code in out and 'CFGREJECT' in out, f'exit {p.returncode}, {code} {code in out}')
    return res


def t_settings():
    """V.01-03: the settings are checked by the yaLIBCFG schema - every problem at once, with the line, and the file is refused."""
    bad = '''listeners = (
  { bind = "TCP:127.0.0.1:5999"; target = "/dev/ttyQ9"; },                  # line 2: an undefined target
  { bind = "UDP:127.0.0.1:5998"; target = "/dev/null"; conlm = 4; }       # line 3: UDP, a typo (conlm)
);
serials = (
  { device = "/dev/null"; chars = "9600, 9, N, 1"; iotmo = 0; },            # line 6: data bits, iotmo
  { device = "/dev/null"; chars = "9600, 8, X, 1"; }                        # line 7: a duplicate device, parity
);
'''
    good = '''# comments stay comments
Listeners = ( { BIND = "TCP:127.0.0.1:5997"; Target = "/dev/null"; Mode = "raw"; } );
SERIALS = ( { Device = "/dev/null"; Chars = "115200,8,e,1"; Flow = "rts/cts"; lategd = 0; } );
'''
    res = True
    case = '''listeners = ( { bind = "TCP:127.0.0.1:5996"; target = "/DEV/null"; } );
serials = ( { device = "/dev/null"; chars = "9600,8,N,1"; }, { device = "/DEV/NULL"; chars = "9600,8,N,1"; } );
'''
    for name, text in (('bad', bad), ('good', good), ('case', case)):
        conf = os.path.join(TMP, f'settings_{name}.conf')
        open(conf, 'w').write(text)
        p = subprocess.run([GW, '/settings=' + conf, '/check', '/control=NONE'], capture_output=True, text=True, timeout=10)
        out = p.stdout + p.stderr
        if name == 'case':					# Two devices told apart by the case only: a target could not choose
            res &= report('settings/case', p.returncode != 0 and 'differ by the case only' in out and 'CFGREJECT' in out,
                          f'exit {p.returncode}, refused {"differ by the case only" in out}')
            continue
        if name == 'good':
            res &= report('settings/good', p.returncode == 0 and 'CHECKOK' in out, f'exit {p.returncode}, CHECKOK {"CHECKOK" in out}')
            continue
        want = [('target', 'is not a <device> of <serials>', 'line 2'), ('udp', 'UDP is not implemented', 'line 3'),
                ('typo', 'CFG-E-UNKNOWN', 'conlm'), ('databits', 'data bits 9 is out of range', 'line 6'),
                ('iotmo', 'CFG-E-RANGE', 'iotmo'), ('dupdevice', 'CFG-E-DUPKEY', 'line 7'), ('parity', "parity 'X' is unknown", 'line 7')]
        lines = out.splitlines()
        miss = [w for w, a, b in want if not any(a in l and b in l for l in lines)]
        res &= report('settings/bad', p.returncode != 0 and not miss and 'CFGREJECT' in out and 'DEVADDED' not in out,
                      f'exit {p.returncode}, missing {miss or "none"}, CFGREJECT {"CFGREJECT" in out}')
    return res


tests = {'icrnl': t_icrnl, 'late': t_late, 'replug': t_replug, 'rawreplug': t_rawreplug, 'gone': t_gone, 'flow': t_flow, 'settings': t_settings}
rc = all([tests[t]() for t in (tests if a.test == 'all' else [a.test])])
print(f'logs in {TMP}')
sys.exit(0 if rc else 1)
