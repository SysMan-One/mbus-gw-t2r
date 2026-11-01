#!/usr/bin/env python3
# Self-contained bench of the RAW listener mode: pty pairs + gateway + TCP clients
# SPDX-License-Identifier: Apache-2.0
#
#   tests/raw_bench.py [path-to-mbusgw-t2r]
#
import hashlib, os, pty, signal, socket, subprocess, sys, tempfile, termios, threading, time, tty as ttymod

GW = sys.argv[1] if len(sys.argv) > 1 else './build/mbusgw-t2r'
TMP = tempfile.mkdtemp(prefix='t2r_raw_')
RAWPORT, RAWPORT2, MBPORT = 5602, 5603, 5604


def crc16(d):
    crc = 0xFFFF
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def mkpty(cooked=False):
    m, s = pty.openpty()
    ttymod.setraw(m)
    if cooked:
        # A "fresh port": ICRNL and ISTRIP on - the gateway must make the line binary-clean itself
        a = termios.tcgetattr(s)
        a[0] |= termios.ICRNL | termios.ISTRIP
        termios.tcsetattr(s, termios.TCSANOW, a)
    else:
        ttymod.setraw(s)
    return m, s, os.ttyname(s)


def start_gw(conf, name):
    cf = os.path.join(TMP, name + '.conf')
    lf = os.path.join(TMP, name + '.log')
    open(cf, 'w').write(conf)
    p = subprocess.Popen([GW, '/trace', '/settings=' + cf], stdout=open(lf, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1.0)
    return p, lf


def stop_gw(p):
    if p.poll() is None:
        p.send_signal(signal.SIGTERM); time.sleep(0.3)
        p.send_signal(signal.SIGTERM); time.sleep(0.5)
    if p.poll() is None:
        p.kill()


def read_exact_fd(fd, n, tmo=10):
    out = b''; end = time.time() + tmo
    while len(out) < n and time.time() < end:
        try:
            out += os.read(fd, n - len(out))
        except BlockingIOError:
            time.sleep(0.005)
    return out


def recv_exact(s, n, tmo=10):
    s.settimeout(tmo); out = b''
    while len(out) < n:
        c = s.recv(n - len(out))
        if not c:
            break
        out += c
    return out


def serial(dev, chars='115200, 8, N, 1'):
    return f'{{ device = "{dev}"; chars = "{chars}"; iotmo = 2000; rs485 = 0; }}'


ok = True
procs = []
try:
    # ------------------------------------------------------------------ configuration checks
    ma, sa, deva = mkpty()
    mb, sb, devb = mkpty()
    checks = [
        ('C1', f'{{bind="TCP:127.0.0.1:{RAWPORT}"; mode="RAW"; target="{deva}";}}, {{bind="TCP:127.0.0.1:{MBPORT}"; target="{deva}";}}', 'LSNRAWSHR'),
        ('C2', f'{{bind="TCP:127.0.0.1:{MBPORT}"; target="{deva}";}}, {{bind="TCP:127.0.0.1:{RAWPORT}"; mode="raw"; target="{deva}";}}', 'LSNRAWSHR'),
        ('C3', f'{{bind="TCP:127.0.0.1:{MBPORT}"; takeover=1; target="{deva}";}}', 'LSNRAWOPT'),
        ('C4', f'{{bind="TCP:127.0.0.1:{RAWPORT}"; mode="TRANSPARENT"; target="{deva}";}}', 'LSNBADMODE'),
        ('C5', f'{{bind="TCP:127.0.0.1:{RAWPORT}"; mode="RAW"; connlm=2; target="{deva}";}}', 'LSNOPTRNG'),
    ]
    for tag, lsn, code in checks:
        p, lf = start_gw(f'listeners = ( {lsn} );\nserials = ( {serial(deva)} );\n', 'cfg' + tag)
        stop_gw(p)
        log = open(lf).read()
        assert ('%T2R-E-' + code) in log, f'{tag}: no {code} in the log:\n' + log[-1500:]
        print(f'{tag} PASS ({code})')
    os.close(ma); os.close(sa); os.close(mb); os.close(sb)

    # ------------------------------------------------------------------ the working gateway
    ma, sa, deva = mkpty(cooked=True)	# RAW, takeover (default)
    mc, sc, devc = mkpty()			# RAW, takeover = 0
    mb, sb, devb = mkpty()			# MODBUS, a fake RTU slave
    os.set_blocking(ma, False); os.set_blocking(mc, False)

    conf = f'''listeners = (
	{{ bind = "TCP:127.0.0.1:{RAWPORT}";  mode = "RAW"; target = "{deva}"; }},
	{{ bind = "TCP:127.0.0.1:{RAWPORT2}"; mode = "Raw"; takeover = 0; target = "{devc}"; }},
	{{ bind = "TCP:127.0.0.1:{MBPORT}";   target = "{devb}"; }}
);
serials = ( {serial(deva)}, {serial(devc)}, {serial(devb, "9600, 8, N, 1")} );
'''

    def rtu_slave():
        buf = b''
        while True:
            try:
                buf += os.read(mb, 256)
            except OSError:
                return
            while len(buf) >= 8:
                req, buf = buf[:8], buf[8:]
                qty = (req[4] << 8) | req[5]
                pdu = bytes([req[0], req[1], qty * 2]) + b''.join((1000 + i).to_bytes(2, 'big') for i in range(qty))
                c = crc16(pdu)
                os.write(mb, pdu + bytes([c & 0xFF, c >> 8]))
    threading.Thread(target=rtu_slave, daemon=True).start()

    gwp, gwlog = start_gw(conf, 'raw')
    procs.append(gwp)
    assert gwp.poll() is None, 'gateway died:\n' + open(gwlog).read()[-1500:]

    # R1: both directions, binary-clean (CR, LF, 0x00, 0xFF, 8th bit) on a "cooked" port
    s1 = socket.create_connection(('127.0.0.1', RAWPORT), timeout=10)
    time.sleep(0.3)
    probe = b'hello\r\n\x00\xff\x0d\x0a\x80\x11\x13'
    s1.sendall(probe)
    got = read_exact_fd(ma, len(probe))
    assert got == probe, f'R1 client->line {got!r}'
    os.write(ma, probe[::-1])
    got = recv_exact(s1, len(probe))
    assert got == probe[::-1], f'R1 line->client {got!r}'
    print('R1 PASS (both directions, binary-clean)')

    # R2: 1 MB each way at once, the line side reads slowly (back pressure)
    N = 1 << 20
    up, down = os.urandom(N), os.urandom(N)
    res = {}

    def line_side():
        h = hashlib.sha256(); n = 0; woff = 0; end = time.time() + 120
        while (n < N or woff < N) and time.time() < end:
            if woff < N:
                try:
                    woff += os.write(ma, down[woff:woff + 1024])
                except BlockingIOError:
                    pass
            try:
                c = os.read(ma, 4096); h.update(c); n += len(c)
            except BlockingIOError:
                time.sleep(0.001)
        res['line'] = (n, h.hexdigest())

    def client_rx():
        h = hashlib.sha256(); n = 0
        s1.settimeout(120)
        while n < N:
            c = s1.recv(65536)
            if not c:
                break
            h.update(c); n += len(c)
        res['client'] = (n, h.hexdigest())

    t0 = time.time()
    ta = threading.Thread(target=line_side); tb = threading.Thread(target=client_rx)
    ta.start(); tb.start()
    s1.sendall(up)
    ta.join(); tb.join()
    dt = time.time() - t0
    assert res['line'] == (N, hashlib.sha256(up).hexdigest()), f'R2 client->line {res["line"][0]} octets'
    assert res['client'] == (N, hashlib.sha256(down).hexdigest()), f'R2 line->client {res["client"][0]} octets'
    print(f'R2 PASS (1 MB each way, sha256 match, {dt:.1f} s)')

    # R3: takeover - a second client kicks the first one, the data flows on the second
    s2 = socket.create_connection(('127.0.0.1', RAWPORT), timeout=10)
    s1.settimeout(5)
    assert s1.recv(16) == b'', 'R3 the old client was not shut down'
    time.sleep(0.3)
    s2.sendall(b'second')
    assert read_exact_fd(ma, 6) == b'second', 'R3 client2->line'
    os.write(ma, b'back')
    assert recv_exact(s2, 4) == b'back', 'R3 line->client2'
    s1.close()
    print('R3 PASS (takeover)')

    # R4: <takeover> = 0 - the second client is rejected, the first one keeps working
    c1 = socket.create_connection(('127.0.0.1', RAWPORT2), timeout=10)
    time.sleep(0.3)
    c2 = socket.create_connection(('127.0.0.1', RAWPORT2), timeout=10)
    c2.settimeout(5)
    assert c2.recv(16) == b'', 'R4 the second client was not rejected'
    c1.sendall(b'still')
    assert read_exact_fd(mc, 5) == b'still', 'R4 the first client is broken'
    c1.close(); c2.close()
    print('R4 PASS (takeover = 0, RAWBUSY)')

    # R5: the MODBUS listener of the same process still answers
    m = socket.create_connection(('127.0.0.1', MBPORT), timeout=10)
    m.sendall(bytes.fromhex('beef000000060104000000') + b'\x02')
    r = m.recv(512)
    assert r[:2] == b'\xbe\xef' and [int.from_bytes(r[9 + i * 2:11 + i * 2], 'big') for i in range(2)] == [1000, 1001], f'R5 {r.hex()}'
    m.close()
    print('R5 PASS (MODBUS listener alongside)')

    # R6: 40 raw reconnects (each one takes the line over) - no thread leak
    s2.close()
    for i in range(40):
        c = socket.create_connection(('127.0.0.1', RAWPORT), timeout=5)
        c.sendall(b'x'); c.close()
    time.sleep(2.0)
    nthreads = len(os.listdir(f'/proc/{gwp.pid}/task'))
    assert nthreads < 10, f'R6 {nthreads} threads'
    log = open(gwlog).read()
    assert '%T2R-W-RAWKICK' in log and '%T2R-W-RAWBUSY' in log and '%T2R-I-RAWSTOP' in log, 'R6 messages'
    print(f'R6 PASS (no thread leak: {nthreads} threads)')

    print('ALL PASS')
except Exception as e:
    ok = False
    print('FAIL:', e)
    for f in sorted(os.listdir(TMP)):
        if f.startswith('raw') and f.endswith('.log'):
            print('--- ' + f + ' tail ---')
            print(open(os.path.join(TMP, f)).read()[-3000:])
finally:
    for p in procs:
        stop_gw(p)

sys.exit(0 if ok else 1)
