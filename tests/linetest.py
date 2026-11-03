#!/usr/bin/env python3
# Raw serial line test without a gateway: one side sends 100 frames of 16 octets, the other counts the intact ones.
# SPDX-License-Identifier: Apache-2.0
#
#   tests/linetest.py rx <device> <baud> <seconds>		# on the receiving side, start it first
#   tests/linetest.py tx <device> <baud> 0			# on the sending side
#
# Baud: 9600, 19200, 115200. Both ends need the line free of any other process (an emulator, a gateway).
import os,sys,termios,tty,time,select
mode,dev,baud,dur=sys.argv[1],sys.argv[2],int(sys.argv[3]),float(sys.argv[4])
B={9600:termios.B9600,19200:termios.B19200,115200:termios.B115200}[baud]
fd=os.open(dev,os.O_RDWR|os.O_NOCTTY); tty.setraw(fd)
a=termios.tcgetattr(fd); a[4]=a[5]=B; a[2]=(a[2]&~termios.CSIZE)|termios.CS8; a[2]&=~(termios.PARENB|termios.CSTOPB|termios.CRTSCTS); a[2]|=termios.CLOCAL|termios.CREAD
termios.tcsetattr(fd,termios.TCSANOW,a); termios.tcflush(fd,termios.TCIOFLUSH)
frame=lambda i: bytes([0xA5,i&255])+bytes((i*7+k)&255 for k in range(14))
if mode=='tx':
    time.sleep(1.0)
    for i in range(100):
        os.write(fd,frame(i)); termios.tcdrain(fd); time.sleep(0.05)
else:
    buf=b''; end=time.time()+dur
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.2)
        if r: buf+=os.read(fd,512)
    good=sum(1 for i in range(100) if frame(i) in buf)
    miss=[i for i in range(100) if frame(i) not in buf]
    print("missing:",miss[:6],"..",miss[-3:] if miss else "")
    print(f"{dev} {baud}: rx {len(buf)} of 1600 bytes, intact frames {good}/100", "" if len(buf)==1600 else buf[:24].hex())
