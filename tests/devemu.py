#!/usr/bin/env python3
#
#++
#  FACILITY:	mbusgw-t2r - test tools
#
#  MODULE:	devemu.py
#
#  ABSTRACT:
#	An emulator of MODBUS RTU slave devices on a serial line, for the bench
#	behind the gateway (gateway on one board, this on the other end of the
#	RS-232 cable).  Python 3 standard library only (termios), no pyserial.
#
#	Every unit given by --units answers on its own: 0x/1x/3x/4x tables of
#	--size entries each.  Function codes 1, 2, 3, 4, 5, 6, 15, 16, 17 (report
#	slave id); anything else - exception 1, an address out of the table -
#	exception 2, a bad quantity - exception 3.  Broadcasts (unit 0) are
#	applied to every unit and never answered.  Frames with a bad CRC are
#	counted and dropped, as a device does.
#
#	Initial values: holding register i of unit u = u * 1000 + i.  Input
#	registers move, so a poller sees a live device:
#		3x0	seconds since start (mod 65536)
#		3x1	a "temperature": 20.00 C +- 5 C sine, period 60 s, x100
#		3x2	requests served by this unit (mod 65536)
#		3x3	CRC errors seen on the line (mod 65536)
#		3x4..	unit * 100 + i
#	Discrete input i = parity of (seconds + i); coils start at 0.
#	--slow=UNIT:MS delays the answers of UNIT (a late device), --mute=UNIT
#	makes UNIT silent (a dead device) - for the gateway's timeouts.
#
#	Usage:	devemu.py --dev /dev/ttyUSB0 [--baud 9600] [--parity N|E|O]
#			[--units 1,2,3] [--size 200] [--slow U:MS] [--mute U] [--quiet]
#	Statistics go to stdout every 60 s and at exit (SIGTERM/SIGINT).
#	A frame is ended by a t3.5 gap with a good CRC; a bad CRC waits up to
#	100 ms for more octets (an FTDI adapter with latency_timer 16 delivers
#	a frame in chunks - contrib/99-ftdi-latency.rules sets it to 1).
#
#  AUTHORS:	Ruslan R. Laishev (AKA: BadAss SysMan) and Claude
#
#  CREATION DATE:	16-NOV-2026
#
#  MODIFICATION HISTORY:
#
#	RRL	16-NOV-2026	X.01-00	Initial version.
#--
#
import argparse, math, os, select, signal, struct, sys, termios, time

BAUDS = {1200: termios.B1200, 2400: termios.B2400, 4800: termios.B4800, 9600: termios.B9600,
	 19200: termios.B19200, 38400: termios.B38400, 57600: termios.B57600,
	 115200: termios.B115200, 230400: termios.B230400}

def crc16(b):
	c = 0xFFFF
	for x in b:
		c ^= x
		for _ in range(8):
			c = (c >> 1) ^ 0xA001 if c & 1 else c >> 1
	return struct.pack('<H', c)

def open_line(dev, baud, parity):
	fd = os.open(dev, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
	a = termios.tcgetattr(fd)
	a[0] = 0						# iflag
	a[1] = 0						# oflag
	a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL	# cflag, 1 stop bit
	if parity != 'N':
		a[2] |= termios.PARENB | (termios.PARODD if parity == 'O' else 0)
	a[3] = 0						# lflag: raw
	a[4] = a[5] = BAUDS[baud]
	a[6][termios.VMIN] = 0
	a[6][termios.VTIME] = 0
	termios.tcsetattr(fd, termios.TCSANOW, a)
	termios.tcflush(fd, termios.TCIOFLUSH)
	return fd

class Unit:
	def __init__(self, u, size):
		self.u, self.size = u, size
		self.hr = [(u * 1000 + i) & 0xFFFF for i in range(size)]
		self.co = [0] * size
		self.served = 0

	def ir(self, i, t):
		if i == 0: return int(t) & 0xFFFF
		if i == 1: return int(2000 + 500 * math.sin(2 * math.pi * t / 60)) & 0xFFFF
		if i == 2: return self.served & 0xFFFF
		if i == 3: return STATS['crc'] & 0xFFFF
		return (self.u * 100 + i) & 0xFFFF

	def di(self, i, t):
		return (int(t) + i) & 1

STATS = {'frames': 0, 'crc': 0, 'answered': 0, 'exceptions': 0, 'broadcast': 0, 'other': 0}

def bits(vals):
	out = bytearray((len(vals) + 7) // 8)
	for i, v in enumerate(vals):
		if v: out[i // 8] |= 1 << (i % 8)
	return bytes(out)

def handle(unit, pdu, t):
	"""-> response PDU (function code first), or None for no answer"""
	f = pdu[0]
	exc = lambda code: (STATS.__setitem__('exceptions', STATS['exceptions'] + 1), bytes([f | 0x80, code]))[1]
	if f in (1, 2, 3, 4):
		if len(pdu) != 5: return exc(3)
		a, n = struct.unpack('>HH', pdu[1:5])
		lim = 2000 if f <= 2 else 125
		if not 1 <= n <= lim: return exc(3)
		if a + n > unit.size: return exc(2)
		if f == 1: data = bits(unit.co[a:a + n])
		elif f == 2: data = bits([unit.di(a + i, t) for i in range(n)])
		elif f == 3: data = b''.join(struct.pack('>H', v) for v in unit.hr[a:a + n])
		else: data = b''.join(struct.pack('>H', unit.ir(a + i, t)) for i in range(n))
		return bytes([f, len(data)]) + data
	if f == 5:
		if len(pdu) != 5: return exc(3)
		a, v = struct.unpack('>HH', pdu[1:5])
		if v not in (0x0000, 0xFF00): return exc(3)
		if a >= unit.size: return exc(2)
		unit.co[a] = 1 if v else 0
		return pdu
	if f == 6:
		if len(pdu) != 5: return exc(3)
		a, v = struct.unpack('>HH', pdu[1:5])
		if a >= unit.size: return exc(2)
		unit.hr[a] = v
		return pdu
	if f == 15:
		if len(pdu) < 6: return exc(3)
		a, n, bc = struct.unpack('>HHB', pdu[1:6])
		if not 1 <= n <= 1968 or bc != (n + 7) // 8 or len(pdu) != 6 + bc: return exc(3)
		if a + n > unit.size: return exc(2)
		for i in range(n):
			unit.co[a + i] = (pdu[6 + i // 8] >> (i % 8)) & 1
		return pdu[:5]
	if f == 16:
		if len(pdu) < 6: return exc(3)
		a, n, bc = struct.unpack('>HHB', pdu[1:6])
		if not 1 <= n <= 123 or bc != 2 * n or len(pdu) != 6 + bc: return exc(3)
		if a + n > unit.size: return exc(2)
		unit.hr[a:a + n] = list(struct.unpack('>%dH' % n, pdu[6:6 + bc]))
		return pdu[:5]
	if f == 17:
		sid = b'devemu unit %d' % unit.u
		return bytes([f, len(sid) + 1]) + sid + b'\xff'
	return exc(1)

def main():
	ap = argparse.ArgumentParser(description='MODBUS RTU slave emulator')
	ap.add_argument('--dev', required=True)
	ap.add_argument('--baud', type=int, default=9600, choices=sorted(BAUDS))
	ap.add_argument('--parity', default='N', choices='NEO')
	ap.add_argument('--units', default='1,2,3')
	ap.add_argument('--size', type=int, default=200)
	ap.add_argument('--slow', action='append', default=[], help='UNIT:MS')
	ap.add_argument('--mute', action='append', default=[], type=int)
	ap.add_argument('--quiet', action='store_true')
	o = ap.parse_args()
	units = {int(u): Unit(int(u), o.size) for u in o.units.split(',')}
	slow = {int(s.split(':')[0]): int(s.split(':')[1]) / 1000.0 for s in o.slow}
	char_t = 11.0 / o.baud						# one character on the wire
	gap = max(3.5 * char_t, 0.00175)				# t3.5, 1.75 ms above 19200 (as the spec)
	fd = open_line(o.dev, o.baud, o.parity)
	t0 = time.monotonic()
	running = [True]
	def stop(*_): running[0] = False
	signal.signal(signal.SIGTERM, stop)
	signal.signal(signal.SIGINT, stop)
	def stats(why):
		print('devemu %s: %s; served %s' % (why, ', '.join('%s %d' % kv for kv in STATS.items()),
			', '.join('u%d=%d' % (u, x.served) for u, x in sorted(units.items()))), flush=True)
	print('devemu: %s %d %s 8 1, units %s, size %d, t3.5 %.2f ms' % (o.dev, o.baud, o.parity,
		','.join(map(str, sorted(units))), o.size, gap * 1000), flush=True)
	buf, last, first, next_stats = b'', 0.0, 0.0, time.monotonic() + 60
	while running[0]:
		try:
			r, _, _ = select.select([fd], [], [], gap if buf else 1.0)	# (a select with a partial frame wakes every t3.5)
		except InterruptedError:
			continue
		now = time.monotonic()
		if r:
			d = os.read(fd, 512)
			if d:
				if not buf: first = now
				buf += d
				last = now
				continue
		if buf and now - last >= gap:			# a frame ended
			if (len(buf) < 4 or crc16(buf[:-2]) != buf[-2:]) and now - first < 0.1 and len(buf) < 260:
				continue			# a USB adapter may deliver a frame in chunks: wait up to 100 ms for the rest
			frame, buf = buf, b''
			STATS['frames'] += 1
			if len(frame) < 4 or crc16(frame[:-2]) != frame[-2:]:
				STATS['crc'] += 1
				if not o.quiet: print('devemu: bad frame %s' % frame.hex(), flush=True)
				continue
			u, pdu, t = frame[0], frame[1:-2], now - t0
			if u == 0:
				STATS['broadcast'] += 1
				for x in units.values(): handle(x, pdu, t)
				continue
			if u not in units or u in o.mute:
				STATS['other'] += 1
				continue
			resp = handle(units[u], pdu, t)
			units[u].served += 1
			if resp is None: continue
			if u in slow: time.sleep(slow[u])
			out = bytes([u]) + resp
			os.write(fd, out + crc16(out))
			termios.tcdrain(fd)
			STATS['answered'] += 1
		if now >= next_stats:
			stats('stats')
			next_stats = now + 60
	stats('exit')
	os.close(fd)

if __name__ == '__main__':
	main()
