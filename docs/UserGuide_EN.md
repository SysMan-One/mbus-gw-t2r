# mbusgw-t2r --- User Guide (English)

*A MODBUS TCP to RTU gateway. This guide is written for a beginner: every step is spelled out,
nothing is assumed. The Russian version of this guide is in `UserGuide_RU.md`.*

---

## 1. What this program does

You have a device (an electricity meter, a temperature controller, a PLC...) which talks
**MODBUS RTU** over a serial line (RS-485 or RS-232). And you have a program on your computer
or network (a SCADA, a script, `mbpoll`...) which talks **MODBUS TCP** over the network.

The gateway sits between them and translates:

```
 [Your TCP client] --- TCP/IP network ---> [mbusgw-t2r] --- serial line ---> [Your RTU device]
      (SCADA,                              (this program)   (RS-485/RS-232)   (meter, PLC, ...)
       mbpoll, ...)
```

- Several TCP clients may connect at the same time --- the gateway queues them so that only one
  request at a time goes to the serial line (an RTU line physically cannot do two at once).
- Several devices may sit on one RS-485 line --- they are told apart by the *slave address*
  inside the MODBUS request; the gateway does not need to know their addresses at all.

## 2. Before you start --- the checklist

1. The gateway is installed (see `README.md`, section *Installation*). After the installation
   you have:
   - the program itself: `/opt/mbusgw-t2r/sbin/mbusgw-t2r`
   - the settings file: `/opt/mbusgw-t2r/etc/modbus-t2r_settings.conf`
2. You know **which serial port** your device is connected to. On Linux it is a file like
   `/dev/ttyS0`, `/dev/ttyUSB0` or `/dev/ttyXRUSB0`. If unsure, plug the USB adapter out and
   in and run `dmesg | tail` --- the port name will be printed.
3. You know the **line parameters** of your device: speed (baud rate), data bits, parity and
   stop bits. They are in the device's manual. The most common set is `9600, 8, N, 1`.
4. Your user can open the port. The simplest check: `ls -l /dev/ttyUSB0` --- if the group is
   `dialout`, add yourself to it: `sudo usermod -a -G dialout $USER` (re-login afterwards),
   or simply run the gateway with `sudo`.

## 3. The settings file, line by line

The settings file has two sections: `serials` (the serial lines) and `listeners` (the TCP
ports). Open it in any editor:

```
$ sudo nano /opt/mbusgw-t2r/etc/modbus-t2r_settings.conf
```

A minimal working example:

```
serials = (
	{	device = "/dev/ttyUSB0";
		chars  = "9600, 8, N, 1";
		iotmo  = 1000;
	}
);

listeners = (
	{	bind   = "TCP:0.0.0.0:502";
		target = "/dev/ttyUSB0";
		connlm = 8;
	}
);
```

Read it as: *"open the port /dev/ttyUSB0 at 9600-8-N-1, wait for an answer up to 1000 ms;
listen for TCP clients on every network interface, port 502, and forward them to that port"*.

### 3.1. The `serials` section --- one record per serial port

| Key | Required? | What it means | Allowed values |
|-----|-----------|---------------|----------------|
| `device` | **yes** | The serial port file | e.g. `/dev/ttyUSB0` |
| `chars` | **yes** | The line parameters: `speed, data bits, parity, stop bits` | speed 50..4000000; data bits 5..8; parity `N` (none), `E` (even), `O` (odd); stop bits 1..2 |
| `iotmo` | no | How long to wait for the device's answer, in **milliseconds**. Default: 1000. Take it **with a margin** over the slowest device on the line: an answer which comes later than `iotmo` is lost | any positive number |
| `lategd` | no | After a failed request (no answer, a broken answer) the gateway listens to the line for this many **milliseconds** more and throws away what comes (`%T2R-W-LATEANS`): a late answer must not go to the next request. It costs time only after a failure: a request to a dead device takes `iotmo` + `lategd`. Default: `iotmo` / 4; `0` --- only the 3.5 character pause, as before X.00-13ECO08 | 0..60000 |
| `rs485` | no | `1` --- ask the kernel to drive the RS-485 direction control (only for ports that support it) | `0` or `1` |
| `ts_enabled` | no | `1` --- enable the built-in Time Stamp pseudo device (see section 6) | `0` or `1` |
| `ts_unit` | no | The slave address the pseudo device answers on | 1..247, default 135 |
| `ts_fncode` | no | The function code it answers to | default 4 |
| `ts_base_reg0` | no | The first register number of the time stamp | default 135 |

If a record is wrong, the gateway **skips it and says why** --- with the allowed range in the
message, for example:

```
%T2R-E-SERRANGE, [serial #00:</dev/ttyUSB0>] --- speed (baud) 31 is out of range [50..4000000]
```

The record number (`#00`) counts from zero, in the order the records appear in the file.

**USB adapters: unplugging is fine.** Since X.00-13ECO09 the gateway lives through a replug of a
USB-to-serial adapter (or a hub reset):

- the gateway notices that the adapter has gone (`%T2R-E-DEVLOST`) --- also when nobody is
  polling --- and closes the port at once;
- it tries to open the port again by its name every second; while the adapter is away, the
  clients get the exception **10** (`GATEWAY_PATH_UNAVAILABLE`), not 4;
- when the adapter is back, `%T2R-S-DEVREADY` is printed and the work goes on; no restart is needed;
- if the adapter is not there when the gateway starts, its TCP port is opened anyway
  (`%T2R-W-DEVABSENT`), and the port starts to work when the adapter appears.

**Use a permanent name.** The kernel gives the numbers `ttyUSB0`, `ttyUSB1`, ... in the order the
adapters appear, so after a replug the same adapter may get another number. Put a permanent
name into `device` (and into `target` of the listener) instead:

- the name by the USB socket: `ls -l /dev/serial/by-path/` --- for example
  `/dev/serial/by-path/platform-xhci-hcd.0-usb-0:1.1:1.0-port0` (for a 4-port FT4232H the
  ports differ by the `:1.0`, `:1.1`, `:1.2`, `:1.3` part);
- or your own name made by a udev rule, for example `/dev/rs485-1`:
  `SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-xhci-hcd.0-usb-0:1.1:1.0", SYMLINK+="rs485-1"`
  in `/etc/udev/rules.d/99-rs485.rules` (see `udevadm info /dev/ttyUSB0` for the values).

### 3.2. The `listeners` section --- one record per TCP port

| Key | Required? | What it means | Allowed values |
|-----|-----------|---------------|----------------|
| `bind` | **yes** | Where to listen: `TCP:<IP address>:<port>` | port 1..65535; `0.0.0.0` = all interfaces; UDP is not supported |
| `target` | **yes** | Which serial port to forward to --- must match a `device` from `serials` **exactly** | |
| `connlm` | no | How many TCP clients may be connected to this port **at the same time**; one more is disconnected at once (`CONNLIMIT`). Default: 32 | 1..512 |
| `idletmo` | no | A client which sends nothing for this many **seconds** is disconnected (`IDLETMO`). Default: 60 | 1..86400 |
| `mode` | no | What the listener does: `"MODBUS"` is the MODBUS TCP to RTU gateway, `"RAW"` is a transparent stream of octets (see 3.3). Capital or small letters --- it does not matter. Any other word is an error (`LSNBADMODE`). Default: `"MODBUS"` | `"MODBUS"`, `"RAW"` |
| `takeover` | no | **Only for `RAW`.** `1`: a new client kicks the current client out (`RAWKICK`). `0`: a new client is rejected while another one is connected (`RAWBUSY`). In the `MODBUS` mode this key is an error (`LSNRAWOPT`). Default: 1 | 0..1 |
| `iotmo` | no | Reserved, not used yet | |

In the `RAW` mode two defaults are different: `connlm` may only be 1, and `idletmo` is 86400 (not 60).

Notes for the very beginning:

- Port **502** is the standard MODBUS TCP port, but on Linux ports below 1024 need root.
  If you do not want to run as root, use e.g. `5502` and point your client there.
- Several `MODBUS` listeners may share one `target` --- the gateway will serialize their requests.
  A `RAW` listener needs its serial port for itself (see 3.3).

### 3.3. The RAW (transparent) mode

In the `MODBUS` mode the gateway understands MODBUS. In the `RAW` mode it does **not**. It just
carries octets from the TCP client to the serial line and from the serial line to the client. It
changes nothing. This is a "virtual COM port over the network".

Typical use: a Windows PC has a program for some old device. The program wants a COM port. You
install a COM-over-TCP program on the PC (it makes a virtual COM port and connects it to the
gateway by TCP). The program thinks it works with a real COM port. The device can speak **any**
serial protocol, not only MODBUS.

```
 [Program on the PC] --- virtual COM --- TCP ---> [mbusgw-t2r, mode RAW] --- serial line ---> [Device]
```

A settings example:

```
listeners = (
	{ bind = "TCP:0.0.0.0:4001"; mode = "RAW"; target = "/dev/ttyUSB1"; }
);
serials = (
	{ device = "/dev/ttyUSB1"; chars = "19200, 8, E, 1"; flow = "NONE"; iotmo = 1000; }
);
```

The rules of the RAW mode:

- **One client at a time.** `connlm` may only be 1.
- **A new client takes over** (`takeover = 1`, the default). The old connection is closed, the new
  one gets the line. The log says `RAWKICK`. With `takeover = 0` the old client stays and the
  new one is refused (`RAWBUSY`).
- **The serial port is only for this listener.** No other listener, MODBUS or RAW, may use the
  same `target` (`LSNRAWSHR`; the order of the records does not matter). MODBUS listeners may still share a port among themselves.
- **The speed and the parity are taken from the gateway.** They come from the `serials` record.
  If the program on the PC sets another speed on its virtual COM port, this is **not** passed to the line.
  Set the right `chars` in the `serials` record.
- **A long silence is normal.** The default `idletmo` is 86400 seconds (one day). The gateway
  closes a client only after that. But a client which is dead (the PC crashed, the cable was
  cut) is found by the TCP keepalive and dropped in about 1 minute (30 seconds of silence, then 3 probes, 10 seconds apart).
- **No Time Stamp.** The `ts_*` pseudo device (chapter 6) does not work in the RAW mode.
- On a start of a session the old octets in the serial port buffers are thrown away.
- **No serial port --- no session.** If the USB adapter is absent or has been unplugged
  (`DEVABSENT`, `DEVLOST`, section 3.1), a new client is disconnected at once, without any data,
  and a running session is closed. When the adapter is back, the client simply connects again.

**A Linux program as the client.** The pseudo-serial driver `serial2net` (NaPi Lab, X.00-08ECO1 and
newer) creates `/dev/ser2net00`, `/dev/ser2net01` and so on and carries everything written there to a TCP peer as it is.
Point the peer to a RAW listener (`p_ser2net_peer="127.0.0.1:4001"`), and any program which wants a
serial device (`mbpoll -m rtu`, `minicom`, `pyserial`) works with the remote line as with a local
port. The speed is still taken from the `serials` record of the gateway. Tested: 1 MiB each way in
blocks with CRC32 (no loss), four ports at once with `mbpoll`, 115200.

A note for both modes: the serial input is "binary-clean" since X.00-14. The port does not change
CR into LF and does not cut the 8th bit any more. Before, an octet `0x0D` could arrive as
`0x0A` on a port with the default settings.

## 4. Running the gateway

By hand, in a terminal (the messages go straight to the screen):

```
$ /opt/mbusgw-t2r/sbin/mbusgw-t2r /settings=/opt/mbusgw-t2r/etc/modbus-t2r_settings.conf
```

Useful options:

| Option | Meaning |
|--------|---------|
| `/settings=<file>` | The settings file (see above) |
| `/trace` | Verbose tracing: every request and answer is dumped in hex. Priceless for the first run |
| `/logfile=<file>` | Write the log to a file instead of the screen |
| `/logsize=<octets>` | Rotate the log file when it grows above this size |

A healthy start looks like this (shortened):

```
%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)
%T2R-I-DEVADDED, Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added
%T2R-I-LSNADDED, Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added
%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready
%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready
```

Two lines matter most: **DEVREADY** (the port is open) and **LSNRRDY** (the TCP port is
listening). If you see both --- the gateway is up.

To stop it: press `Ctrl+C` once and wait a second. To toggle the tracing of a *running*
gateway without a restart: `kill -USR1 <pid>`.

### 4.1. The first test with mbpoll

`mbpoll` is a small free MODBUS client (`sudo apt install mbpoll`). Read 3 holding registers
from the device with slave address 1:

```
$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502
```

If numbers come back --- everything works, end to end.

### 4.2. On OpenWrt

On an OpenWrt router install the package for your board from the Releases page (the
architecture is the `DISTRIB_ARCH` line of `/etc/openwrt_release`): `.ipk` for OpenWrt 24.10,
`.apk` for OpenWrt 25.12. The package needs nothing else, the router needs no network:

```
# opkg install ./mbusgw-t2r_0.14.2-*_mipsel_24kc.ipk
# apk add --allow-untrusted ./mbusgw-t2r_0.14.2-*_mipsel_24kc.apk
```

The files are at other places than on Debian: the program is `/usr/sbin/mbusgw-t2r`, the
settings file is `/etc/mbusgw-t2r/modbus-t2r_settings.conf`. The service is switched off after the
install. Tune the settings file, then switch the service on and start it:

```
# uci set mbusgw-t2r.main.enabled=1 && uci commit mbusgw-t2r
# /etc/init.d/mbusgw-t2r start
# logread -e mbusgw-t2r
```

The last command shows the messages of the gateway (they go to the system log). The USB
serial adapter needs its kernel module: `kmod-usb-serial-ftdi`, `kmod-usb-serial-ch341` or
`kmod-usb-serial-cp210x`, by the chip of the adapter.

## 5. Reading the log

Every important event is printed as a line with a **message code**:

```
%T2R-E-CRC16ERR, [#4:</dev/ttyUSB0>] RTU CRC16 check error (received: 0x1234, calculated: 0xABCD)
 ^   ^  ^
 |   |  +-- the code: look it up in the table of section 7
 |   +----- severity: S=success, I=info, W=warning, E=error, F=fatal
 +--------- the facility (always T2R for this program)
```

`[#4:</dev/ttyUSB0>]` means: file descriptor #4, the serial port in question.

## 6. The Time Stamp pseudo device

The gateway can answer *by itself* --- without touching the line --- to a read request sent
to a special slave address. The answer is the current time of the gateway's host. This lets a
SCADA read precise time over plain MODBUS.

With `ts_enabled = 1; ts_unit = 135; ts_fncode = 4; ts_base_reg0 = 135;` a request "read N
input registers starting at 135 from slave 135" returns:

| Registers requested | You get |
|---------------------|---------|
| 4 | seconds of the UNIX epoch, 64 bits, the most significant 16-bit word first: R0=bits 63..48, R1=47..32, R2=31..16, R3=15..0 |
| 8 | + nanoseconds, packed the same way into R4..R7 |
| 9 | + R8 = the local timezone offset, in **signed minutes** (e.g. Moscow = +180) |

Assemble the seconds as `(R0<<48)|(R1<<32)|(R2<<16)|R3`.

Any other register count in a TS request is answered with the MODBUS exception
`ILLEGAL_DATA_ADDRESS` --- this is on purpose.

## 7. Troubleshooting --- symptom, cause, action

**The golden rule: run with `/trace` and read the log. The gateway always says what it
dislikes.**

| You see | What it means | What to do |
|---------|---------------|------------|
| `%T2R-E-DEVOPNERR, Cannot open the device </dev/ttyUSB0>, errno: 2` | errno 2 = the file does not exist: the adapter is unplugged or the name is wrong | `dmesg \| tail` after plugging the adapter; fix `device` in the settings. Since X.00-13ECO09 the TCP port is opened anyway (`DEVABSENT`), the device is tried every second |
| `%T2R-E-DEVLOST, [...] tcflush(), errno: 5 --- the device is lost, it will be reopened every 1000 msec` (or `writev()`, `read()`, `hangup`) | The USB adapter has gone: unplugged, a hub reset, a power dip; the clients get the exception 10 | nothing, if it comes back: `DEVREADY` follows. If not: `dmesg \| tail`; check the cable and the power; if it came back under another `ttyUSB` number, use a permanent name (section 3.1) |
| `%T2R-W-DEVABSENT, Device <...> is absent --- it will be opened every 1000 msec ...` | At the start the serial port could not be opened (the line above says why); its TCP port works and answers the exception 10 | plug in the adapter --- `DEVREADY` follows; or fix `device` in the settings |
| The client gets the exception `0x0A` (10) | The serial port of this TCP port is absent or lost (see `DEVLOST`, `DEVABSENT`) | see `DEVLOST` above |
| the same, `errno: 13` | Permission denied | run with `sudo` or add yourself to the `dialout` group |
| the same, `errno: 16` | The port is busy: another program holds it | find it: `sudo fuser /dev/ttyUSB0`; stop it |
| `%T2R-E-NOANSWER, [...] Did not get an answer in 1000 msec` | The request went out, nothing came back | 1) wrong slave address in the client's request; 2) wrong wiring (swap A/B of RS-485); 3) wrong speed/parity --- re-check `chars`; 4) the device is simply off |
| `%T2R-E-CRC16ERR, [...] (received: X, calculated: Y)` | Octets arrive, but damaged | 1) `chars` does not match the device (most common!); 2) line noise --- check the grounding, the termination resistors, the cable length; 3) two masters on one line |
| `CRC16ERR` on a part of the requests after a reboot or a replug of the adapter; it goes away after any other program (`stty raw`, pyserial) has opened the port | A gateway older than X.00-13ECO07 kept the CR-to-NL translation (`icrnl`) of a fresh port: an octet 0x0D in the answer became 0x0A | update to X.00-13ECO07 or newer; for an older gateway: `ExecStartPre=/bin/stty -F /dev/ttyUSB0 raw` (one per port) in the unit |
| `%T2R-E-BADFRAME, [...] !UL octets, allowed [5..256]` | Something arrived, but it is not a sane MODBUS frame | usually the same causes as CRC16ERR; with 400+ octets of garbage --- a foreign, non-MODBUS device on the line |
| `%T2R-W-FRAMETMO, [...] Frame is not completed in ...` | The answer started but never finished in time | a very slow or hanging device; raise `iotmo`; check the cable |
| `CRC16ERR`, `BADFRAME` or `FRAMETMO` now and then, on a USB adapter (FTDI) only; a direct `mbpoll` on the same port is fine | The FTDI adapter gives the answer by pieces, once in 16 msec (its `latency_timer`). Since X.00-10 the gateway waits for the whole answer of the usual functions (01-06, 15, 16, 23), but the other functions are still cut | set `latency_timer` to 1 msec --- it also makes every answer faster: `sudo cp /opt/mbusgw-t2r/share/doc/mbusgw-t2r/99-ftdi-latency.rules /etc/udev/rules.d/`, then `sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=usb-serial`; check: `cat /sys/bus/usb-serial/devices/ttyUSB0/latency_timer` must print `1` |
| `%T2R-W-LATEANS, [...] N octets have arrived after the failed exchange (a late answer?) --- dropped` after `NOANSWER` | The device answered, but later than `iotmo`; the answer has been thrown away | raise `iotmo` of this serial port: take it with a margin over the slowest device on the line |
| `%T2R-W-ECHOMISM, [...] Echo of the write request: address A, value/quantity V (REQ: ...) --- mismatch` | The answer to a write (functions 5, 6, 15, 16) is not the echo of this request: usually a late answer to an earlier request; the client gets exception 4 | raise `iotmo` (see LATEANS); if it happens with a fast device --- two masters on one line; if it happens on **every** write to one device --- the device does not echo the request as the MODBUS specification wants (e.g. returns the stored, clamped value): the write is done, but reported as failed --- tell us the device model |
| `%T2R-W-EXCRPT, [...] Report exception 4 (SERVER_DEVICE_FAILURE) ...` | The gateway told the TCP client "the serial side failed" | look at the *previous* log line --- it names the real reason (NOANSWER, CRC16ERR, ...) |
| `%T2R-W-CONNLIMIT, [...] --- the listener has reached its connection limit (N) --- the connection is rejected` | The port already has `connlm` clients connected | if they are all real clients, raise `connlm` of this listener; otherwise find who keeps the connections open: `ss -tn` |
| `%T2R-W-IDLETMO, [...] No activity for the past N seconds (idletmo) --- close the connection` | A client has sent nothing for `idletmo` seconds | normal for a client which polls rarely --- it reconnects; to keep the connection, raise `idletmo` of this listener |
| `%T2R-E-LSNRAWSHR, [listener #NN] --- target <...> is used by the listener on port N too, ...` | A RAW listener wants a serial port which another listener uses | give the RAW listener its own serial port, or make the other listener use another one; or make the RAW listener a MODBUS one |
| `%T2R-W-RAWBUSY, [...] --- the raw listener is busy and <takeover> is off --- the connection is rejected` | A client is already connected to the RAW port and `takeover = 0` | normal. Disconnect the first client, or set `takeover = 1` |
| `%T2R-E-TXDRAINTMO, [...] --- the request has not been transmitted in N msec` | The USB adapter took the request but did not send it out; the request gets exception 4 | the adapter or its USB link hangs: `dmesg \| tail`; replug the adapter or reboot the board. Since X.00-13ECO01 the line is not blocked by it: the next request tries again |
| `%T2R-E-DEVLOCKERR` / `DEVBUSY` on every request of one port, the other ports are fine | Another request holds the line and does not end | update to X.00-13ECO01 or newer (an older gateway can hang forever in the serial driver). Before the restart, save what the stuck thread does: `for t in /proc/$(pidof mbusgw-t2r)/task/*; do echo $t $(cat $t/wchan); done` |
| `%T2R-E-SESSFAIL, [...] --- cannot start a session thread, pthread_create()->11` | Too many TCP clients at once: every client takes a thread, and systemd limits the service (`TasksMax`, 578 on a small board) | find who keeps so many connections open (`ss -tn`); if they are all real, raise the limit: `TasksMax=2048` in the `[Service]` section of the unit, then `systemctl daemon-reload` |
| `%T2R-E-LSNRERR, Listener 0.0.0.0:502 --- bind() error, errno: 98` | errno 98 = the TCP port is already taken | another gateway instance is running, or another program owns the port: `sudo ss -tlnp \| grep 502` |
| the same, `errno: 13` | Ports below 1024 need root | run with `sudo`, or use a port >= 1024 (e.g. 5502) |
| `%T2R-E-SERRANGE, [serial #NN:...] --- ... out of range [a..b]` (or `LSNRANGE`) | A settings value is out of its allowed range | fix the value; the allowed range is right in the message |
| `%T2R-E-NOSERIALS, No serials have been defined!` | Not a single `serials` record survived the validation | read the error lines above it --- each rejected record says why |
| The client connects but every answer is an exception `0x04` | See EXCRPT above | the trouble is on the serial side; `/trace` and read backwards |
| Everything looks fine, but the data is wrong/shifted | Byte order confusion on the client side | MODBUS registers are big-endian 16-bit words; check how your client assembles 32/64-bit values |

### 7.1. The message codes reference

| Code | Severity | When it appears |
|------|----------|-----------------|
| `REVISNF` | I | At start: the program version. Quote it when asking for help |
| `DEVREADY` | S | The serial port is open and configured; shows the actual baud, the answer timeout and the t3.5 interval |
| `LSNRRDY` | S | The TCP port is listening; shows the address, the port and the target device |
| `NETCONN` | S | A TCP client has connected; shows its address:port and the listener |
| `NETDISCN` | S | A TCP client has disconnected; shows its address:port |
| `DEVOPNERR` | E | The serial port cannot be opened; `errno` says why (2 = no such file, 13 = permissions, 16 = busy) |
| `LSNRERR` | E | `bind()`/`listen()` failed for a TCP port; `errno` says why (98 = taken, 13 = privileges) |
| `NOANSWER` | E | The device did not answer within the `iotmo` timeout |
| `FRAMETMO` | W | The answer began but was not completed within the timeout |
| `BADFRAME` | E | The received frame length is outside the sane [5..256] range |
| `CRC16ERR` | E | The checksum of the received frame does not match; both values are shown |
| `EXCRPT` | W | A MODBUS exception is being returned to the TCP client; the code and its name are shown |
| `EXITST` | I | The gateway exits; shows the exit flag and the final status |
| `SYSERR` | E | A system call failed; its name, the return value and errno are shown. Quote the line when asking for help |
| `NETSYSERR` | E | A system call on a TCP connection failed (recv, send, poll, accept); the connection is closed |
| `NETSYSWRN` | W | A socket option cannot be set; the gateway keeps working |
| `TTYSYSERR` | E | A system call on the serial port failed (poll, read); the request gets exception 4 |
| `TTYSYSWRN` | W | A minor serial port call failed (tcflush, poll/read while flushing); the gateway keeps working |
| `DEVSYSERR` | E | A serial port setup call failed (tcgetattr, tcsetattr, cfsetspeed, mutex); the port is not used |
| `DEVSYSWRN` | W | Waiting for the serial line took too long (pthread_mutex_timedlock); the gateway tries again |
| `PEERCLOSE` | I | The TCP client has closed the connection --- the normal end of a session |
| `ALLSENT` | W | Nothing left to send on a TCP connection --- harmless |
| `BADPROTO` | E | A TCP request has a non-zero MBAP protocol id: the client does not speak MODBUS TCP; the connection is closed |
| `PDUSHORT` | E | A TCP request is too short to be a MODBUS request; the connection is closed |
| `PDULONG` | E | A TCP request is longer than 256 octets; the connection is closed |
| `IDLETMO` | W | A TCP client has been silent for `idletmo` seconds (60 by default); the connection is closed |
| `NETRDTMO` | E | A TCP client started a request but did not finish it in time; the connection is closed |
| `REQFAIL` | W | A request is answered with a MODBUS exception; the previous lines say why |
| `NETPOLLEV` | W | A TCP connection broke: the client reset it (e.g. the polling program was restarted); the connection is closed |
| `SESSFAIL` | E | No thread for a new TCP client: too many connections at once (see Troubleshooting); the client is dropped |
| `LSNRABRT` | W | A TCP listener is stopped --- normal at the gateway exit |
| `TGTOPNERR` | E | A TCP port is not opened because its serial port cannot be set up; the previous line says why. An absent device does not stop the TCP port since X.00-13ECO09 (see `DEVABSENT`) |
| `NOLSNRUP` | E | Not a single TCP port could be opened; the gateway stops |
| `NOROOM` | E | An answer does not fit into the buffer --- an internal error, please report it |
| `MBAPLEN` | E | The MBAP length field does not match the received data; the request is rejected |
| `REQSHORT` | E | A request is too short to be forwarded to the serial line |
| `TGTNDEF` | E | A listener names a target which is not in the serials section |
| `CFGNOSECT` | E | The settings file has no serials or listeners section; the gateway stops |
| `CFGTRUNC` | W | Too many records in a section; only the first ones are used |
| `SERBADREC` | E | A serials record is broken or has no device; it is skipped |
| `SERBADOPT` | E | A serials record has no chars, or it is too long; the record is skipped |
| `SERNORS485` | W | rs485 = 1 is set, but this build has no RS485 support; the option is ignored |
| `SERBADCHR` | E | The chars value cannot be read; write it like "9600, 8, N, 1" |
| `SERRANGE` | E | The speed, data bits or stop bits are out of range; the allowed range is in the message |
| `SERBADPAR` | E | The parity letter is unknown; use N, E or O |
| `DEVADDED` | I | A serials record is accepted; the line parameters are shown |
| `TSENABLED` | I | The Time Stamp pseudo device is on for this port |
| `TSDISABLED` | I | The Time Stamp pseudo device is off for this port (the default) |
| `NOSERIALS` | E | Not a single serials record survived the validation; the gateway stops |
| `LSNBADREC` | E | A listeners record is broken (no bind, no target, UDP); it is skipped |
| `LSNBADBIND` | E | The bind value cannot be read; write it like "TCP:0.0.0.0:502" |
| `LSNRANGE` | E | The TCP port number is out of range [1..65535] |
| `LSNBADADDR` | E | The IP address in bind is not a valid IPv4 address |
| `LSNNOTGT` | E | The target of a listener is not in the serials section; the record is skipped |
| `LSNADDED` | I | A listeners record is accepted; the address, the port, the target, the limits and the mode are shown (the text ends with `, Mode: MODBUS`, `, Mode: RAW` or `, Mode: RAW (takeover)`) |
| `NOLSNRS` | E | Not a single listeners record survived the validation; the gateway stops |
| `CFGPARSE` | E | The settings file cannot be read or has a syntax error; the file name and the line are shown |
| `SIGERR` | E | A signal handler cannot be set --- an internal error, please report it |
| `CFGERR` | E | The configuration cannot be processed; the previous lines say why |
| `CFGABORT` | F | The gateway stops because of the configuration errors above |
| `DEVLOCKERR` | E | The serial line stayed busy for 39 seconds (another request holds it); the request gets exception 4 |
| `TXNODATA` | W | An empty request was not sent --- harmless |
| `XMITERR` | E | The request cannot be written to the serial port; the request gets exception 4 |
| `XMITSHORT` | E | Only a part of the request has been written to the serial port |
| `RXBUFFULL` | W | The device sent more than 256 octets; the rest is dropped |
| `RXNODATA` | E | The answer started but no octet was collected |
| `RESPMISM` | W | The answer came from another slave or for another function: two masters or two devices with one address |
| `BADSPEED` | W | The speed is not a standard one; 9600 is used |
| `NOTTTY` | W | The device is not a serial port |
| `RS485OFF` | W | The RS485 mode cannot be switched on for this port; the port works without it |
| `RS485ON` | S | The RS485 mode is on for this port |
| `DEVINIERR` | E | The serial port cannot be set up; the previous line says why |
| `DEVINIT` | I | The serial port is set up |
| `DEVCLSERR` | E | The serial port cannot be closed |
| `DEVNOTINI` | W | Closing a serial port which was never opened --- harmless |
| `DEVBUSY` | E | The serial line cannot be taken for a request; the request gets exception 4 |
| `CONNLIMIT` | W | A new TCP client is disconnected at once: the port already has `connlm` clients |
| `LSNOPTRNG` | E | A listeners option (`connlm`, `idletmo`, `takeover`) is out of range (in the RAW mode `connlm` may only be 1); the record is skipped |
| `TXDRAINTMO` | E | The request was not sent out by the serial adapter in time; it is dropped, the request gets exception 4 |
| `ECHOMISM` | W | The answer to a write request is not its echo (another address or value); the request gets exception 4 |
| `DEVLOST` | E | The serial port has gone (an USB adapter unplugged); it is closed and opened again every second, the requests get exception 10 meanwhile |
| `DEVABSENT` | W | The serial port cannot be opened at the start; the TCP port works anyway, the serial port is opened every second |
| `LATEANS` | W | After a failed request something came from the line within `lategd` --- a late answer; it is dropped |
| `RAWSTART` | I | A RAW session has started: the client address and the serial port are shown. Nothing to do |
| `RAWSTOP` | I | A RAW session has ended; shows how many octets went to the line and how many came from the line. Nothing to do --- the numbers help to see if the data flowed at all |
| `RAWKICK` | W | A new client took the RAW port over (`takeover = 1`): the old connection is shut down. If this happens by itself again and again, two programs fight for one port --- stop one of them, or set `takeover = 0` |
| `RAWBUSY` | W | A new client is refused: the RAW port is busy and `takeover = 0`. Wait until the first client leaves, or set `takeover = 1` |
| `LSNBADMODE` | E | The `mode` value is unknown; the record is skipped. Write `"MODBUS"` or `"RAW"` |
| `LSNRAWSHR` | E | The serial port of a RAW listener is used by another listener; the record is skipped. Give the RAW listener a port of its own |
| `LSNRAWOPT` | E | A listeners option is not allowed in this mode (`takeover` in the MODBUS mode); the record is skipped. Remove the option or set `mode = "RAW"` |

*(The `S/I/W/E` letter is a part of the printed line: `%T2R-E-CRC16ERR, ...`.)*

## 8. If nothing helps

Collect and attach to your question:

1. the full start-up log with `/trace` (from `REVISNF` to the first error),
2. your settings file,
3. the output of `ls -l <device>` and `dmesg | tail -20`,
4. the exact model of the RTU device and its documented line parameters.

With these four things the problem is almost always visible at a glance.
