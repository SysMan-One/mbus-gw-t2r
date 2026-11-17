#!/usr/bin/env python3
#
#++
#  FACILITY:	mbusgw-t2r - test tools
#
#  MODULE:	soak.py
#
#  ABSTRACT:
#	A long run against the gateway with tests/devemu.py behind it: MODBUS
#	TCP requests to the units the emulator serves, every answer checked.
#	One persistent connection runs reads and writes back to back; every
#	--newconn seconds a short connection of its own does one request and is
#	closed (the accept/close path).  Writes are read back and the holding
#	registers and coils are kept in a model, so a wrong value anywhere is
#	a MISMATCH.  Input registers: 3x0 must advance, 3x4.. = unit*100+i.
#
#	Usage:	soak.py [--host 127.0.0.1] [--port 502] [--units 1,2,3]
#			[--seconds 7200] [--newconn 30] [--timeout 7]
#	One statistics line a minute and one at the end; exit 0 when no
#	request failed and nothing mismatched, else 1.
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
import argparse, random, socket, struct, sys, time

class Fail(Exception):
	pass

class Conn:
	def __init__(self, host, port, tmo):
		self.s = socket.create_connection((host, port), timeout=tmo)
		self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
		self.tid = 0

	def close(self):
		try: self.s.close()
		except OSError: pass

	def req(self, unit, pdu):
		self.tid = (self.tid + 1) & 0xFFFF
		self.s.sendall(struct.pack('>HHHB', self.tid, 0, len(pdu) + 1, unit) + pdu)
		hdr = self.recvn(7)
		tid, proto, ln, u = struct.unpack('>HHHB', hdr)
		body = self.recvn(ln - 1)
		if tid != self.tid or proto != 0 or u != unit:
			raise Fail('bad MBAP tid %d/%d unit %d/%d' % (tid, self.tid, u, unit))
		if body[0] & 0x80:
			raise Fail('exception %d to fc %d' % (body[1], body[0] & 0x7F))
		if body[0] != pdu[0]:
			raise Fail('fc %d answered by %d' % (pdu[0], body[0]))
		return body

	def recvn(self, n):
		b = b''
		while len(b) < n:
			d = self.s.recv(n - len(b))
			if not d:
				raise Fail('connection closed by the gateway')
			b += d
		return b

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--host', default='127.0.0.1')
	ap.add_argument('--port', type=int, default=502)
	ap.add_argument('--units', default='1,2,3')
	ap.add_argument('--size', type=int, default=200)
	ap.add_argument('--seconds', type=int, default=7200)
	ap.add_argument('--newconn', type=int, default=30)
	ap.add_argument('--timeout', type=float, default=7)
	o = ap.parse_args()
	units = [int(u) for u in o.units.split(',')]
	st = {'req': 0, 'ok': 0, 'fail': 0, 'mismatch': 0, 'reconnect': 0, 'newconn': 0, 'newconn_fail': 0}
	lat = []
	model_hr, model_co, last_t = {}, {}, {}
	conn = [None]

	def connect():
		if conn[0]: conn[0].close()
		conn[0] = Conn(o.host, o.port, o.timeout)

	def call(unit, pdu, c=None):
		c = c or conn[0]
		st['req'] += 1
		t = time.monotonic()
		r = c.req(unit, pdu)
		lat.append(time.monotonic() - t)
		return r

	def mismatch(what):
		st['mismatch'] += 1
		print('soak: MISMATCH %s' % what, flush=True)

	def baseline():
		for u in units:
			hr = []
			for a in range(0, o.size, 100):
				n = min(100, o.size - a)
				r = call(u, struct.pack('>BHH', 3, a, n))
				hr += list(struct.unpack('>%dH' % n, r[2:2 + 2 * n]))
			model_hr[u] = hr
			r = call(u, struct.pack('>BHH', 1, 0, o.size))
			model_co[u] = [(r[2 + i // 8] >> (i % 8)) & 1 for i in range(o.size)]

	def one(u):
		op = random.random()
		if op < 0.35:						# FC3, check against the model
			a = random.randrange(o.size); n = random.randint(1, min(60, o.size - a))
			r = call(u, struct.pack('>BHH', 3, a, n))
			got = list(struct.unpack('>%dH' % n, r[2:2 + 2 * n]))
			if r[1] != 2 * n or got != model_hr[u][a:a + n]:
				mismatch('u%d 4x%d+%d' % (u, a, n))
		elif op < 0.55:						# FC4
			r = call(u, struct.pack('>BHH', 4, 0, 10))
			v = struct.unpack('>10H', r[2:22])
			if list(v[4:]) != [(u * 100 + i) & 0xFFFF for i in range(4, 10)]:
				mismatch('u%d 3x4.. %s' % (u, v[4:]))
			if u in last_t and v[0] < last_t[u] and last_t[u] - v[0] < 60000:
				mismatch('u%d 3x0 went back %d -> %d' % (u, last_t[u], v[0]))
			last_t[u] = v[0]
		elif op < 0.70:						# FC6 + read back
			a = random.randrange(o.size); v = random.randrange(65536)
			r = call(u, struct.pack('>BHH', 6, a, v))
			if r[1:5] != struct.pack('>HH', a, v): mismatch('u%d fc6 echo' % u)
			model_hr[u][a] = v
		elif op < 0.80:						# FC16
			a = random.randrange(o.size); n = random.randint(1, min(50, o.size - a))
			vals = [random.randrange(65536) for _ in range(n)]
			r = call(u, struct.pack('>BHHB', 16, a, n, 2 * n) + struct.pack('>%dH' % n, *vals))
			if r[1:5] != struct.pack('>HH', a, n): mismatch('u%d fc16 echo' % u)
			model_hr[u][a:a + n] = vals
		elif op < 0.88:						# FC1 against the model
			a = random.randrange(o.size); n = random.randint(1, o.size - a)
			r = call(u, struct.pack('>BHH', 1, a, n))
			got = [(r[2 + i // 8] >> (i % 8)) & 1 for i in range(n)]
			if got != model_co[u][a:a + n]: mismatch('u%d 0x%d+%d' % (u, a, n))
		elif op < 0.94:						# FC5
			a = random.randrange(o.size); v = random.randrange(2)
			r = call(u, struct.pack('>BHH', 5, a, 0xFF00 if v else 0))
			model_co[u][a] = v
		else:							# FC15
			a = random.randrange(o.size); n = random.randint(1, min(64, o.size - a))
			bits = [random.randrange(2) for _ in range(n)]
			by = bytearray((n + 7) // 8)
			for i, b in enumerate(bits):
				if b: by[i // 8] |= 1 << (i % 8)
			call(u, struct.pack('>BHHB', 15, a, n, len(by)) + bytes(by))
			model_co[u][a:a + n] = bits

	def stats(why):
		l = sorted(lat) or [0]
		q = lambda p: l[min(len(l) - 1, int(p * len(l)))] * 1000
		print('soak %s: %s; latency ms p50 %.0f p90 %.0f p99 %.0f max %.0f (%d samples)' % (why,
			', '.join('%s %d' % kv for kv in st.items()), q(.5), q(.9), q(.99), l[-1] * 1000, len(l)), flush=True)
		lat.clear()

	connect()
	baseline()
	print('soak: %s:%d units %s, %d s, baseline read' % (o.host, o.port, o.units, o.seconds), flush=True)
	end = time.monotonic() + o.seconds
	next_stats = time.monotonic() + 60
	next_new = time.monotonic() + o.newconn
	while time.monotonic() < end:
		u = random.choice(units)
		try:
			one(u)
			st['ok'] += 1
		except (Fail, OSError) as e:
			st['fail'] += 1
			print('soak: FAIL u%d: %s' % (u, e), flush=True)
			try:
				connect(); st['reconnect'] += 1
				baseline()				# a write may or may not have landed
			except (Fail, OSError) as e2:
				print('soak: reconnect failed: %s' % e2, flush=True)
				time.sleep(5)
		now = time.monotonic()
		if now >= next_new:
			next_new = now + o.newconn
			st['newconn'] += 1
			try:
				c = Conn(o.host, o.port, o.timeout)
				call(random.choice(units), struct.pack('>BHH', 4, 0, 4), c)
				c.close()
			except (Fail, OSError) as e:
				st['newconn_fail'] += 1
				print('soak: FAIL new connection: %s' % e, flush=True)
		if now >= next_stats:
			stats('stats')
			next_stats = now + 60
	stats('end')
	sys.exit(0 if st['fail'] == 0 and st['mismatch'] == 0 and st['newconn_fail'] == 0 else 1)

if __name__ == '__main__':
	main()
