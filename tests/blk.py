#!/usr/bin/env python3
# Block stream test of a serial line (or of a pseudo-serial device such as /dev/ser2net00 -> a RAW listener -> a line):
# 256-octet blocks (magic, seq, 244 payload, crc32), a window of 16 blocks + an ACK, UP then DOWN, no big buffers.
# The receiver counts good / bad / missing blocks; the client also prints SUnreclaim and stops at 150 MB (a leak guard).
# SPDX-License-Identifier: Apache-2.0
#
#   tests/blk.py peer   <device> <baud> <blocks>		# the far end, start it first: tests/blk.py peer /dev/ttyUSB0 115200 4096
#   tests/blk.py client <device> <baud> <blocks>		# the near end:               tests/blk.py client /dev/ser2net00 115200 4096
#
# 4096 blocks = 1 MiB each way; at 115200 it takes ~93 s per direction. Baud: 19200 or 115200.
import os,sys,termios,tty,time,select,struct,zlib,random
role,dev,baud,NB=sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4])
W=16; BS=256
B={19200:termios.B19200,115200:termios.B115200}[baud]
fd=os.open(dev,os.O_RDWR|os.O_NOCTTY)
try:
    tty.setraw(fd); a=termios.tcgetattr(fd); a[4]=a[5]=B; termios.tcsetattr(fd,termios.TCSANOW,a); termios.tcflush(fd,termios.TCIOFLUSH)
except Exception as e: print("termios:",e,flush=True)
def block(seq):
    h=b'BLK1'+struct.pack('>I',seq)+random.Random(seq).randbytes(244)
    return h+struct.pack('>I',zlib.crc32(h))
def ack(last,good):
    h=b'ACK1'+struct.pack('>II',last,good); return h+struct.pack('>I',zlib.crc32(h))
def slab():
    try: return [l.split()[1] for l in open('/proc/meminfo') if l.startswith('SUnreclaim')][0]
    except Exception: return '?'
def guard():
    try: v=int(slab())
    except Exception: return
    if v>150000:
        print("GUARD: SUnreclaim",v,"kB - stopping to protect the box",flush=True)
        try:
            rows=[l.split() for l in open('/proc/slabinfo').read().splitlines()[2:]]
            rows.sort(key=lambda r:-int(r[1])*int(r[3]))
            for r in rows[:8]: print("  slab",r[0],"active",r[1],"objsize",r[3],"=> %.1f MB"%(int(r[1])*int(r[3])/1048576),flush=True)
        except Exception as e: print("slabinfo:",e,flush=True)
        sys.exit(3)
def rd(timeout):
    r,_,_=select.select([fd],[],[],timeout)
    return os.read(fd,4096) if r else b''
def send_stream(tag):
    t0=time.time(); lost=0
    for s in range(NB):
        os.write(fd,block(s))
        if s%W==W-1:
            buf=b''; end=time.time()+10
            while len(buf)<16 and time.time()<end: buf+=rd(0.5)
            if len(buf)<16 or zlib.crc32(buf[:12])!=struct.unpack('>I',buf[12:16])[0]: lost+=1; print(tag,"no/bad ACK at",s,flush=True)
        if s%256==255: print(tag,"sent",s+1,"blocks %.0fs"%(time.time()-t0),"SUnreclaim",slab(),"kB",flush=True)
    print(tag,"DONE sent",NB,"blocks in %.1fs, ACK problems:"%(time.time()-t0),lost,flush=True)
def recv_stream(tag):
    buf=b''; exp=0; good=bad=gap=0; t0=None; idle=time.time()
    while exp<NB:
        d=rd(1.0)
        if d: buf+=d; idle=time.time(); t0=t0 or time.time()
        elif time.time()-idle>20: print(tag,"IDLE timeout at seq",exp,flush=True); break
        while True:
            i=buf.find(b'BLK1')
            if i<0: buf=buf[-3:]; break
            if i>0: bad+=1; buf=buf[i:]
            if len(buf)<BS: break
            blk=buf[:BS]
            if zlib.crc32(blk[:252])==struct.unpack('>I',blk[252:])[0]:
                seq=struct.unpack('>I',blk[4:8])[0]
                if blk!=block(seq): bad+=1
                else:
                    good+=1
                    if seq!=exp: gap+=seq-exp
                    exp=seq+1
                    if seq%W==W-1: os.write(fd,ack(seq,good))
                    if seq%64==63: guard()
                    if seq%256==255: print(tag,"recv",good,"good",bad,"bad",gap,"missing","SUnreclaim",slab(),"kB",flush=True)
                buf=buf[BS:]
            else: bad+=1; buf=buf[1:]
            if exp>=NB: break
    print(tag,"DONE good",good,"bad",bad,"missing",gap,"of",NB,"in %.1fs"%(time.time()-(t0 or time.time())),"OK" if (good==NB and not bad and not gap) else "FAIL",flush=True)
print(role,"ready",flush=True)
if role=='client':
    send_stream("UP"); time.sleep(1); recv_stream("DOWN")
else:
    recv_stream("UP"); time.sleep(1); send_stream("DOWN")
