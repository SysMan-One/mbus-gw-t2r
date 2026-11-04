# MODBUS TCP to RTU gateway

*[Русская версия: README_RU.md](README_RU.md)*

A yet another gateway for MODBUS TCP to RTU (Limited Edition especialy for LAB240 and NaPiLab Teams)

###	NaPi World
This project is developed and maintained by the NaPi Lab team
and is primarily tested on NaPi industrial single-board computers based on Rockchip SoCs.

####	NaPi Boards
If you are looking for a reliable hardware platform to run MODBUS transport sowftware in production,
check out the NaPi board lineup:

Welcome to NaPi Wolrd (https://github.com/napilab/napi-boards) for more information!

Right now is available:

- NAPI2 — RK3568J, RS-485 onboard, Armbian
- NAPI-C — RK3308, compact, industrial grade


###	Introduction
A easy-to-use TCP-to-RTU gateway for MODBUS protocol is supposed to be used as a tutorial (and not only) for:

 - a development of general programming skills
 - a programming skill esspecialy for I/O on  RS323 and RS-485 ports
 - basic checking of work of MODBUS-capable devices
 - build two-directionional gateways for TCP-to-RTU and RTU-to-TCP (not implemented yet)
 - transparently pass serial I/O over TCP (available: the RAW listener mode)

###	Main features & advantages
  - serves multiple MODBUS-capable devices on single RS-485 line
  - serves single RS-485 line on multiple TCP-ports
  - support of RS-232 and RS-485 specific signaling
  - single process with multithreaded architecture for high performance
  - coordinating access between multiple TCP-clients is connected to single serial bus\device
  - robust error handling and recognizing of serial-line garbage and trouble-recovery tecnicks

###	When to choose mbusgw-t2r over mbusd

This project is an alternative to the well-known [mbusd](https://github.com/3cky/mbusd)
gateway --- for the case when a high efficiency at a lower resource consumption is the
point. Both translate MODBUS TCP to RTU; the differences which matter in practice:

  - **The serial line discipline.** The end-of-frame is detected by the honest t3.5 silence
    criteria with the specification-mandated inter-frame gap --- the RTU bus is driven at its
    physical maximum, no fixed per-request pauses are inserted.
  - **The waiting model.** All the waiting is `poll()`-driven with no timers ticking in the
    background: an idle gateway consumes practically zero CPU, which matters on a small
    industrial SBC running from a modest power budget.
  - **The diagnostics.** Every error is a coded, greppable log message with the concrete
    reason (the CRC values, the `errno`, the allowed range) --- the troubleshooting is a
    reading exercise, not a guessing one.
  - **The extras.** Several TCP ports per one line, several lines per one process, the
    built-in Time Stamp pseudo device, the RS-485 kernel direction control --- all in a
    ~100 KB binary that needs nothing beyond libc.

If mbusd already serves you well --- keep it. If you are squeezing the most out of a busy
RS-485 line on a small box, or you are tired of guessing why a request failed --- this
gateway is for you.

###	Reliability, footprint & performance

**No error slips through.** Every RTU frame coming from the line is validated before it is
believed: the CRC16 trailer is verified, the frame length is checked against the sane
[5..256] octets range, the end-of-frame is detected by the honest t3.5 silence criteria of
the MODBUS over Serial Line specification, and the answer is matched against the request by
the slave address and the function code. Wire-declared lengths are never trusted. Line
garbage --- even hundreds of octets of it --- is rejected with a clean MODBUS exception to
the TCP client, the line is flushed, and the very next request is served normally.

**No error is silent.** Every rejection produces a coded log message (`%T2R-E-CRC16ERR`,
`%T2R-E-NOANSWER`, ...) which names the reason: the received and the calculated CRC values,
the `errno` of a failed system call, the allowed range of a misconfigured parameter. The log
is designed to be grepped by the code and to make the troubleshooting a reading exercise,
not a guessing one --- see the User Guide, Chapter 7.

**Small footprint.** A single process; no runtime dependencies beyond libc. The binary is
about 100 KB; the StarLet set and libconfig are linked statically. There are no busy loops
anywhere: all the waiting is `poll()`-driven, so an idle gateway consumes practically zero
CPU. One thread exists per *active* TCP session only and is released as soon as the client
disconnects (no thread or memory leaks --- verified by the bundled test bench and by the
ASan/UBSan builds). This makes the gateway comfortable on the small industrial single-board
computers of the NaPi lineup (RK3308, RK3568J) --- and on anything larger.

**The line runs at its physical maximum.** The end-to-end latency is dominated by the serial
line itself; the TCP side adds microseconds (`TCP_NODELAY` is set, the answers are written
in one piece). Concurrent TCP clients are serialized per serial line by a mutex, so the RTU
bus is utilized back-to-back --- with the specification-mandated t3.5 inter-frame gap and
nothing more --- while every client still gets its answer in the order of arrival. Multiple
independent serial lines are served in parallel, each at its own full speed.
    



###	Who this project is for

Beyond its direct duty, this project is offered as a piece of readable engineering. It can be
recommended to students who are learning industrial programming: the whole thing is small
enough to be read end to end in an evening, yet it is a real production tool --- the serial line
disciplines, the poll() driven event loops, the ring buffers, the configuration validation, the
coded diagnostics and the graceful shutdown are all here, in their natural habitat rather than
in a textbook exercise.

It is also offered to the experienced programmers who have not yet forgotten the engineering
culture of the Digital Equipment Corporation universe: the module headers with the IDENT/REV
pair and the revision history, the condition codes checked on the low bit, the facility-coded
messages, the FAO-styled logging, the routines which return a status and deliver their values
through the output parameters. If that list reads like a homecoming rather than an oddity ---
this code was written for you.

### 	Installation

The gateway depends on two packages: **libconfig** (a settings file parser, is taken from the
distro; linked statically, `-DT2R_STATIC_LIBCONFIG=OFF` links the shared one) and **StarLet** (the UTILITY$ROUTINES set, is installed from sources as a CMake package).
The whole sequence from a bare system to a running gateway is:

####	Step 1. Install the build prerequisites

```
$ sudo apt install build-essential cmake pkg-config libconfig-dev
```
(on RPM-based distros: `sudo dnf install gcc make cmake pkgconf libconfig-devel`)

####	Step 2. Install the StarLet package

```
$ git clone https://gitlab.com/SysMan-One/utility_routines
$ cd utility_routines
$ mkdir build && cd build
$ cmake ../ -DCMAKE_BUILD_TYPE=Release
$ make -s
$ sudo cmake --install .
```

The package is put under `/usr/local`: the `libstarlet` library, the public headers and the
CMake package configuration (`find_package(StarLet)` becomes available). To install to another
prefix use `sudo cmake --install . --prefix /opt/starlet` and then pass
`-DCMAKE_PREFIX_PATH=/opt/starlet` to the gateway's cmake at the Step 3.

####	Step 3. Build the gateway

```
$ git clone <URL of repo>
$ cd mbus-gw-t2r
$ mkdir build && cd build
$ cmake ../ -DCMAKE_BUILD_TYPE=Release
$ make -s
```

Build types: `Release` (production), `Debug` (symbols, tracing), `Asan` (address/UB sanitizers
for the development).

####	Building against musl (OpenWrt, Alpine, embedded rootfs)

The gateway builds and runs against the **musl** C library as well --- this is the usual case
for OpenWrt and Alpine based images on the industrial boards. A toolchain file is bundled:

```
$ cmake ../ -DCMAKE_TOOLCHAIN_FILE=../cmake/musl.cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_PREFIX_PATH=<musl prefix>
$ make -s
```

The dependencies (StarLet and libconfig) have to be built for musl too and installed into that
prefix; point `PKG_CONFIG_LIBDIR` at its `lib/pkgconfig` so that the musl libconfig is picked up
instead of the host one.

One musl specific detail, handled by the toolchain file: musl ships its own headers but not the
kernel ones (`linux/serial.h`, `asm/ioctls.h`), so the kernel header directories are appended
with `-idirafter` --- this way the musl headers always win and `/usr/include` is consulted only
for what musl does not provide. A plain `-I` would poison the build with the glibc headers.

####	Step 4. Install the gateway

```
$ sudo make install
```

What goes where:

| What | Where |
|------|-------|
| `mbusgw-t2r` binary | `/opt/mbusgw-t2r/sbin/` |
| Configuration files | `/opt/mbusgw-t2r/etc/` |
| Docs and reference copies of the configs | `/opt/mbusgw-t2r/share/doc/mbusgw-t2r/` |

The configuration files are installed **only when they are absent**: a repeated `make install`
(an upgrade) never overwrites the has been tuned working configuration - such files are reported
as `Keeping the existing ...`.

The default prefix is `/opt/mbusgw-t2r` --- a self-contained tree, removed as a whole. To install
elsewhere give the prefix at the Step 3: `cmake ../ -DCMAKE_INSTALL_PREFIX=/usr/local ...` ---
the binary then goes to `/usr/local/sbin/`, the configuration files to
`/usr/local/etc/mbusgw-t2r/`, the docs to `/usr/local/share/doc/mbusgw-t2r/`.

**USB-to-serial adapters FTDI** (FT232/FT2232/FT4232): install the udev rule which sets their
`latency_timer` to 1 msec --- the answers get faster and are never cut:
`sudo cp /opt/mbusgw-t2r/share/doc/mbusgw-t2r/99-ftdi-latency.rules /etc/udev/rules.d/` (the details
are in the rule file and in the User Guide, *Troubleshooting*).

####	Step 5. Tune the configuration and run

```
$ sudo vi /opt/mbusgw-t2r/etc/modbus-t2r_settings.conf
$ /opt/mbusgw-t2r/sbin/mbusgw-t2r /settings=/opt/mbusgw-t2r/etc/modbus-t2r_settings.conf
```

See the *Run & Configuration rules* section below for the settings file format.

####	Uninstallation

```
$ cd mbus-gw-t2r/build
$ sudo make uninstall
```

Everything which has been put by `make install` is removed, except the configuration files
under `/opt/mbusgw-t2r/etc/` - the has been tuned working configuration always survives.

####	Ready-made packages (GitHub Releases)

Every release tag on GitHub (`v0.13.5` and later) gets `.deb` packages for amd64 and arm64 at
https://github.com/napilab/mbus-gw-t2r/releases. They are built on Debian 12 and need glibc 2.34
or newer only (tested on Debian 12, Debian 13 and Ubuntu 24.04); libconfig and StarLet are linked
statically. The package puts the gateway to `/opt/mbusgw-t2r/` and the systemd unit
`mbusgw-t2r.service`, which is installed **disabled** - tune the settings first:

```
$ sudo apt install ./mbusgw-t2r_1.0.0-1_arm64.deb
$ sudo nano /opt/mbusgw-t2r/etc/modbus-t2r_settings.conf
$ sudo systemctl enable --now mbusgw-t2r
```

The configuration files are conffiles: an upgrade keeps the tuned ones. The workflow is
`.github/workflows/release.yml`; locally the same package is made by
`cmake ../ -DCMAKE_BUILD_TYPE=Release -DT2R_SYSTEMD_UNIT=ON && make -s && cpack -G DEB`.

The `.deb` for **mipsel** and **mips64el** (`v0.13.6` and later) is a cross build for Debian 12
(Debian 13 has no MIPS ports); it is installed the same way.

####	OpenWrt packages (GitHub Releases)

From `v0.13.6` on, a release also has OpenWrt packages for `mips_24kc` (ath79 and the other
big-endian MIPS boards), `mipsel_24kc` (ramips: MT7621, MT7628/MT7688), `aarch64_cortex-a53`,
`arm_cortex-a7`, `arm_cortex-a7_neon-vfpv4` (ipq40xx, sunxi) and `x86_64`: an `.ipk` for
OpenWrt 24.10 and an `.apk` for OpenWrt 25.12. Your board's package architecture is the
`DISTRIB_ARCH` line of `/etc/openwrt_release`. libconfig and StarLet are linked statically: the
package has no dependencies, the router needs no network for the install:

```
# opkg install ./mbusgw-t2r_1.0.0-*_mipsel_24kc.ipk			# OpenWrt 24.10
# apk add --allow-untrusted ./mbusgw-t2r_1.0.0-*_mipsel_24kc.apk	# OpenWrt 25.12
```

The package puts the gateway to `/usr/sbin/mbusgw-t2r`, the configuration to `/etc/mbusgw-t2r/`
and the procd service `/etc/init.d/mbusgw-t2r`. The service is **disabled** until you tune the
settings and switch it on; its messages go to the system log:

```
# vi /etc/mbusgw-t2r/modbus-t2r_settings.conf
# uci set mbusgw-t2r.main.enabled=1 && uci commit mbusgw-t2r
# /etc/init.d/mbusgw-t2r start
# logread -e mbusgw-t2r
```

The packages are built by the official OpenWrt SDK from the feed `openwrt/` of this repository
(`mbusgw-t2r` and the build-only `starlet` and `libconfig-static`); the packages of `x86_64` and `mips_24kc` are installed
and started in the OpenWrt images before the release. To build them by your own SDK:
`echo "src-link mbusgw <this tree>/openwrt" >> feeds.conf`,
`./scripts/feeds update mbusgw && ./scripts/feeds install mbusgw-t2r`,
`make package/mbusgw-t2r/compile`.

####	Automatic build (GitLab CI)

The file `.gitlab-ci.yml` builds the gateway on every push to the GitLab repository. It does the
same steps as above, in the `gcc` Docker image:

1. installs `cmake`, `pkg-config` and `libconfig-dev`;
2. clones StarLet from `https://gitlab.com/SysMan-One/utility_routines.git`, builds it and installs it
   into `/usr/local` (the variable `STARLET_REPO` holds the address - change it to use a mirror);
3. builds the gateway in the `build/` directory (Debug).

The job only checks that the code compiles: it does not install the gateway and does not keep the
binary. If the job fails at the StarLet step, check that the StarLet repository can be reached from
the runner.

### 	User Guide

A detailed, beginner-level guide --- configuration walk-through, running, the Time Stamp
pseudo device, troubleshooting by the message codes --- is available in two languages:

- English: [docs/UserGuide_EN.md](docs/UserGuide_EN.md) (a printable DEC-styled PDF: [docs/UserGuide_EN.pdf](docs/UserGuide_EN.pdf))
- Russian: [docs/UserGuide_RU.md](docs/UserGuide_RU.md) (a printable DEC-styled PDF: [docs/UserGuide_RU.pdf](docs/UserGuide_RU.pdf))

The PDFs are generated by `docs/make_userguide_pdf.py` (the reportlab set, the DejaVu fonts).

After `make install` both are also in `/opt/mbusgw-t2r/share/doc/mbusgw-t2r/`.

### 	Run & Configuration rules

####	Quick start

##### General CLI format to start gateway
```
./build/mbusgw-t2r <CLI options> --settings=<network-n-serials-settings.fil>
```

##### Example:

```
./build/mbusgw-t2r --trace --settings=modbus-t2r-settins.conf
```

##### CLI options

| Option		|  Description
| ------		| ------------------------------------------------------------
| trace			| Enable extensible diagnostic output. Useful for for debug and troubleshouting purpose.
| logfile=\<fpsec\>	| Set a file name to accept logging output
| logsize=\<number\>	| Limit size of log file.
| settings=\<fspec\>	| Provide a rin-time configuration for network stuff and serial devices


##### Settings options
Check an example of settings file for reference of parameters and rules of configurations, or
the User Guide (chapter 3) --- every key is described there with its allowed range.

The `listeners` section --- one record per TCP port:

| Key | Required | Meaning | Values |
|-----|----------|---------|--------|
| `bind` | yes | The protocol, the address and the port to listen on | `"TCP:0.0.0.0:502"` (UDP is not supported) |
| `target` | yes | The serial device (from the `serials` section) to forward the requests to | `"/dev/ttyUSB0"` |
| `connlm` | no | How many TCP clients may be connected to this port **at the same time**; one more is disconnected at once (`%T2R-W-CONNLIMIT`) | 1..512, default 32 |
| `idletmo` | no | A client which sends nothing for this many **seconds** is disconnected (`%T2R-W-IDLETMO`); a client which polls rarely just reconnects | 1..86400, default 60 |
| `mode` | no | `"MODBUS"` --- the MODBUS TCP to RTU gateway; `"RAW"` --- a transparent octet stream (see *RAW (transparent) mode* below). Not case-sensitive; any other value rejects the record (`%T2R-E-LSNBADMODE`) | `"MODBUS"` (default), `"RAW"` |
| `takeover` | no | **RAW only.** `1`: a new client kicks the current one out (`%T2R-W-RAWKICK`); `0`: a new client is rejected while one is connected (`%T2R-W-RAWBUSY`). In the MODBUS mode the key is an error (`%T2R-E-LSNRAWOPT`) | 0..1, default 1 |
| `iotmo` | no | Reserved, not used yet | |

An out of range `connlm`/`idletmo` rejects the whole listener record (`%T2R-E-LSNOPTRNG`).
In the RAW mode `connlm` may only be 1 and the default `idletmo` is 86400 (not 60).
Every simultaneous client takes one thread: with many clients mind the `TasksMax` limit of the
systemd unit (see *Troubleshooting*, `SESSFAIL`, in the User Guide).

```
listeners = (
	{bind = "TCP:0.0.0.0:502"; target = "/dev/ttyUSB0"; connlm = 16; idletmo = 60;}
);
```

##### RAW (transparent) mode
With `mode = "RAW"` the listener does not speak MODBUS at all: it relays the octets **as they are**
between one TCP client and the serial line, in both directions. It is a virtual COM port over
TCP: a program on a Windows PC (through a virtual COM port driver which carries the octets over
TCP) sees the remote serial port as a local one. It works for **any** serial protocol, not only MODBUS.

```
listeners = (
	{bind = "TCP:0.0.0.0:4001"; mode = "RAW"; target = "/dev/ttyUSB1";}
);
serials = (
	{device = "/dev/ttyUSB1"; chars = "19200, 8, E, 1"; flow = "NONE"; iotmo = 1000;}
);
```

Limits and rules:

 - One client at a time (`connlm` may only be 1). By default a new client **takes over**: the old
   connection is shut down (`RAWKICK`). With `takeover = 0` the new client is rejected (`RAWBUSY`).
 - The serial line of a RAW listener cannot be used by any other listener, MODBUS or RAW
   (`%T2R-E-LSNRAWSHR`). MODBUS listeners may still share a line among themselves.
 - The speed, the parity and the stop bits come from the `serials` record of the gateway. Whatever
   the remote program sets on its virtual COM port is **not** transported.
 - The default `idletmo` is 86400 seconds. A client which has died silently (a crash, a cut
   cable) is dropped by the TCP keepalive in about 1 minute (30 s idle + 3 probes x 10 s).
 - The Time Stamp pseudo device (`ts_*`) is not used in the RAW mode.

**A Linux program as the client.** The pseudo-serial driver `serial2net` (NaPi Lab, X.00-08ECO1 and
newer) creates `/dev/ser2net00`, `/dev/ser2net01` and so on and carries everything written there to a TCP peer as it is.
Point the peer to a RAW listener (`p_ser2net_peer="127.0.0.1:4001"`), and any program which wants a
serial device (`mbpoll -m rtu`, `minicom`, `pyserial`) works with the remote line as with a local
port. The speed is still taken from the `serials` record of the gateway. Tested: 1 MiB each way in
blocks with CRC32 (no loss), four ports at once with `mbpoll`, 115200.

Since X.00-14 the serial input is binary-clean in **both** modes (no CR/NL translation, no 8th bit
stripping): before, an octet 0x0D could arrive as 0x0A on a port with the default settings.

## Coding style

The source is written in the OpenVMS C style (the "VMS style"). A patch is expected to follow it:

 - **Layout:** Whitesmiths braces (a brace on its own line, indented together with the body; the
   braces of a routine body are not indented). Only tabs (8 columns), no spaces. Lines up to 192 columns.
 - **Prefixes:** `g_` global, `l_` local, `s_` static (routines too), `a_` routine arguments, `m_` bit
   masks, `v_` bit fields. Variables are declared at the top of the block, grouped by type
   (`stdint.h` types).
 - **Names:** `$` separates the words of macros and constants (`T2R$K_BAUD_MAX`, `T2R$K_SETTLE_MSEC`).
   A structure is declared with the `_t` tag suffix and gets a VMS name.
 - **Every file** starts with `__MODULE__`, `__IDENT__` and `__REV__` (`X.00-14ECO02` is
   `00.14.02`), then a VMS-style header: the facility, the abstract, the modification history, and
   for every routine the inputs, outputs, implicit inputs and outputs, and the return value.
 - **Messages** go only through the message macros of the StarLet library (`$LOG_FAO`,
   `$PUTMSG_FAO` and the project wrapper over them) with the codes of the message catalogue, never
   through `printf`, `perror` or `syslog`. A routine returns a status (the low bit is the success),
   results go back through output parameters, and a failed C library call logs `errno`.
   The tests and the text of `/HELP` are exempt.
 - **Documentation** is written in two languages, Russian and English, for a beginner, with a
   troubleshooting row for every message code.

## Authors and acknowledgment
Developer: Ruslan (AKA : The BadAss SysMan) Laishev
VAX/VMS bigot,
BMF.

## License
Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE).
