#define	__MODULE__	"T2R-MAIN"
#define	__IDENT__	"V.01-04ECO04"
#define	__REV__		"01.04.04"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  ABSTRACT:
**
**  DESCRIPTION: Main routine, read and process connfiguration, start workers.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  10-SEP-2025
**
**
**  USAGE:
**	$ mbusgw-t2r [options]
**		options:
**			/SETTINGS=<file>	- the settings file: serial lines and network listeners
**			/CONFIG=<file>		- a configuration options file
**			/TRACE			- enable extensible debug diagnostic output
**			/LOGFILE=<file>		- a file name for the logfile
**			/LOGSIZE=<number>	- a maximum size of the logfile in octets
**			/CONTROL=<path>		- the control channel socket (mbus-ctl), NONE - off;
**						  default /var/run/mbusgw-t2r.sock
**			/CHECK			- check the settings and exit (the exit status says)
**
**  MODIFICATION HISTORY:
**
**	13-NOV-2025	RRL	X.00-02 - pre-Release candidat
**
**	18-NOV-2025	RRL	X.00-03 - Improved TS configuratiuon section by using Unit\Slave Id to deterrminate  TS request
**
**	25-AUG-2026	RRL	X.00-04 / REV: 00.04.00 - Audit fixes:
**				s_settings_process_serials(): flat parsing with an early reject of illformed
**				records, mandatory <device> and <chars>, validation of the line parameters
**				(a division by zero at baud=0), the values of <rs485>/<ts_enabled> keys are
**				checked (a mere presence of the key was enabling the feature), a normalized
**				inter-PDU timespec (t3.5 by the spec), a bound check of the table capacity,
**				a default answer timeout T2R$K_ANSTMO_MSEC, libconfig strings are collapsed
**				in a local copy;
**				s_settings_process_listeners(): the same rework - a missing <bind> was a
**				NULL dereference, sscanf() field widths, port range and inet_pton() checks,
**				the UDP transport is rejected explicitly (was accepting and failing),
**				<target> is stored as a plain ASCIIZ string (was ASCIC into char[]);
**				settings_load_n_parse(): a failure of the serials section is not lost;
**				g_settings -> g_settings; internal routines are static now;
**				main(): returns EXIT_SUCCESS/EXIT_FAILURE (was a severity code).
**
**	25-AUG-2026	RRL	X.00-04ECO01 / REV: 00.04.01 - The validation limits are the T2R$K_* constants
**				now (magic numbers are gone), every rejection diagnostic does show the
**				allowed range of the parameter.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-05 / REV: 00.05.00 - The delivery change (no C source change): the
**				StarLet set is consumed as an installed find_package(StarLet) package
**				instead of the 3rdparty/ submodule; the <install>/<uninstall> targets;
**				the installation sequence is documented in README.md.
**
**	25-AUG-2026	RRL	X.00-05ECO01 / REV: 00.05.01 - The obsolete modbus-t2r_startup.sh is
**				removed from the delivery.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The revision banner and the exit status are
**				signalled via $PUTMSG_FAO by the coded catalogue messages (REVISNF,
**				EXITST); the bilingual (EN + RU) User Guide is added under docs/.
**
**	25-AUG-2026	RRL	X.00-06ECO01 / REV: 00.06.01 - The delivery change (no C source change):
**				the User Guides are delivered as the DEC/VSI-styled PDFs with the
**				diagrams; the organization line is "NaPi World & StarLet Squad
**				collaboration".
**
**	25-AUG-2026	RRL	X.00-07 / REV: 00.07.00 - The message catalogue is FAO-only now: the dead
**				printf-styled records are removed, NETCONN/NETDISCN are recreated as
**				the FAO records and are signalled at the T2R-NET accept/disconnect
**				points (see T2R-MSGS and T2R-NET).
**
**	25-AUG-2026	RRL	X.00-07ECO01 / REV: 00.07.01 - The delivery change (no C source change):
**				README gained the "Reliability, footprint & performance" section.
**
**	25-AUG-2026	RRL	X.00-07ECO02 / REV: 00.07.02 - The delivery change (no C source change):
**				README positions the project as an alternative to mbusd for the high
**				efficiency at a lower resource consumption case.
**
**	25-AUG-2026	RRL	X.00-07ECO03 / REV: 00.07.03 - The delivery change (no C source change):
**				README gained the "Who this project is for" section.
**
**	25-AUG-2026	RRL	X.00-07ECO04 / REV: 00.07.04 - The delivery change (no C source change):
**				README is delivered in two languages: README.md + README_RU.md.
**
**	25-AUG-2026	RRL	X.00-08 / REV: 00.08.00 - The build system change (no C source change):
**				the musl C library builds are supported and verified - added the toolchain
**				file cmake/musl.cmake, fixed the linking against a libconfig which lives in
**				a non-standard prefix; the musl build is documented in the both READMEs.
**
**	25-AUG-2026	RRL	X.00-08ECO01 / REV: 00.08.01 - Use the StarLet library facilities where the
**				code was doing the same by hand: $ISINRANGE() for the range validation,
**				$ARRSZ() for the message catalogue size, $MIN() for the dump clipping.
**
**	14-OCT-2026	RRL	X.00-09 / REV: 00.09.00 - The delivery change (no C source change): the
**				default installation prefix is /opt/mbusgw-t2r (sbin/, etc/,
**				share/doc/mbusgw-t2r/); the docs and the User Guides follow the new layout.
**				The "peer close connection" is logged as -I- (see T2R-NET).
**
**	14-OCT-2026	RRL	X.00-10 / REV: 00.10.00 - The RTU answer is framed by its expected length (see
**				T2R-TTY): the FTDI USB adapters do not cut the answers anymore; the udev rule
**				contrib/99-ftdi-latency.rules is delivered, the User Guides got the FTDI line
**				in the Troubleshooting.
**
**	14-OCT-2026	RRL	X.00-11 / REV: 00.11.00 - The logging is FAO-only now: every $LOG() is replaced
**				by $PUTMSG_FAO() with a coded catalogue message (T2R-MSGS: 63 new
**				records); the settings validation diagnostics are SERxxx/LSNxxx/CFGxxx;
**				the "Time Stamp responder ... disabled" is -I- now (it is the default);
**				the message records are initialized by the field names (no warnings with
**				any StarLet release, whatever the set of the EMSG_RECORD fields is).
**
**	14-OCT-2026	RRL	X.00-12 / REV: 00.12.00 - <connlm> is a real limit of the simultaneous sessions
**				of a listener (default 32, 1..512), the new <idletmo> listener option is the
**				idle session timeout in seconds (default 60, 1..86400; was 1200 hardcoded);
**				both are validated (LSNOPTRNG), LSNADDED shows them.
**
**	14-OCT-2026	RRL	X.00-12ECO01 / REV: 00.12.01 - The routine headers are brought in line with the code;
**				main() returns EXIT_FAILURE on a command line/settings error (was the
**				condition code - an arbitrary exit status); USAGE and help[] name /SETTINGS.
**
**	14-OCT-2026	RRL	X.00-12ECO02 / REV: 00.12.02 - SO_REUSEPORT is dropped (see T2R-NET): a second instance
**				on the same ports gets LSNRERR, errno 98.
**
**	14-OCT-2026	RRL	X.00-12ECO03 / REV: 00.12.03 - The delivery change (no C source change): the
**				both READMEs describe the <listeners> keys, <connlm> and <idletmo> included.
**
**	14-OCT-2026	RRL	X.00-13 / REV: 00.13.00 - The release of the X.00-09..X.00-12ECO03 changes: the
**				/opt layout, the FTDI framing, the FAO-only logging, <connlm>/<idletmo>,
**				no SO_REUSEPORT; the package version is 0.13.0.
**
**	14-OCT-2026	RRL	X.00-13ECO01 / REV: 00.13.01 - T2R-TTY: the device lock cannot be held forever
**				anymore (tcdrain() is replaced by the bounded s_tty_tx_drain(), TXDRAINTMO),
**				CRTSCTS is cleared at the open; T2R-NET: no double DEVCLSERR.
**
**	14-OCT-2026	RRL	X.00-13ECO02 / REV: 00.13.02 - NETPOLLEV is signalled as a WARNING (a connection
**				reset by the client).
**
**	21-OCT-2026	RRL	X.00-13ECO03 / REV: 00.13.03 - Apache License 2.0 (LICENSE, NOTICE, SPDX tags);
**				no code changes.
**
**	28-OCT-2026	RRL	X.00-13ECO04 / REV: 00.13.04 - The delivery change (no C source change):
**				libconfig is linked statically, the package does not depend on a distro's
**				libconfig.so.N anymore.
**
**	28-OCT-2026	RRL	X.00-13ECO05 / REV: 00.13.05 - The delivery change (no C source change):
**				the .deb package (CPack, the systemd unit, conffiles), built for amd64 and
**				arm64 by GitHub Actions and attached to the GitHub Release of the tag.
**
**	28-OCT-2026	RRL	X.00-13ECO06 / REV: 00.13.06 - The packages for MIPS and OpenWrt: the .deb for
**				mipsel and mips64el (a cross build on Debian 12), the OpenWrt feed openwrt/
**				(.ipk for 24.10, .apk for 25.12; mips_24kc, mipsel_24kc, aarch64_cortex-a53,
**				arm_cortex-a7, arm_cortex-a7_neon-vfpv4, x86_64), see T2R-TTY X.00-13ECO06.
**
**	02-NOV-2026	CLD	X.00-13ECO07 / REV: 00.13.07 - T2R-TTY: the serial input is binary-clean (ICRNL
**				and co. are cleared - a 0x0D octet of the answer became 0x0A on a fresh
**				port, CRC16ERR), the line settles and is flushed after the open.
**
**	02-NOV-2026	CLD	X.00-13ECO08 / REV: 00.13.08 - A late answer is not given to the next request
**				(field report: ~100 successful answers with the foreign data in 90 sec with
**				the device answering at iotmo +- 15%): the echo of a write request (FC 05,
**				06, 0F, 10) is checked (ECHOMISM), after a failed exchange the line is
**				drained for the <lategd> guard window (the new <serials> option, msec,
**				default <iotmo> / 4, 0 - t3.5 only as before; LATEANS).
**
**	02-NOV-2026	CLD	X.00-13ECO09 / REV: 00.13.09 - A replugged USB adapter is reopened by its name
**				(field report: EIO forever, exception 4 to everybody until the restart): a
**				device which has gone is closed at once (DEVLOST), reopened every second,
**				also on an idle line; the requests get exception 10 meanwhile; a device
**				absent at the start does not stop its listener (DEVABSENT).
**
**	01-NOV-2026	CLD	X.00-14 / REV: 00.14.00 - The RAW (transparent) listener mode: the new <mode>
**				("MODBUS" - default, or "RAW") and <takeover> listener options; a RAW listener
**				serves one client at a time (a new one takes the line over by default), its
**				default <idletmo> is T2R$K_IDLETMO_MAX, its serial line cannot be shared with
**				any other listener (LSNRAWSHR); see T2R-NET and T2R-TTY X.00-14.
**
**	01-NOV-2026	CLD	X.00-14ECO01 / REV: 00.14.01 - The RAW relay does not spin after a hangup of the
**				client while the line still swallows the last portion (see T2R-TTY).
**
**	02-NOV-2026	CLD	X.00-14ECO02 / REV: 00.14.02 - Rebased onto X.00-13ECO09: a RAW session reopens an
**				absent or lost serial device at its start and ends at once if it is still
**				not there; a failed line closes the device (DEVLOST, was TTYPOLLEV).
**
**	04-NOV-2026	CLD	V.01-00 / REV: 01.00.00 - The first release (package version 1.0.0); the field
**				test line X.00 is closed; no code changes since X.00-14ECO02.
**
**	06-NOV-2026	CLD	V.01-01 / REV: 01.01.00 (package version 1.1.0) - RRL: the next product version is
**				V.01-01; T2R-NET, T2R-CTL and MBUS-CTL carry the same ident.
**				The control channel and the mbus-ctl utility (see T2R-CTL
**				and MBUS-CTL): the new /CONTROL=<path> option (default T2R$K_CTL_SOCK, NONE -
**				off), the channel is opened after the listeners are up and closed at the exit;
**				the new /CHECK option (parse the settings, CHECKOK, exit) - RESTART GATEWAY runs
**				it first; an accepted RESTART raises <g_restart_flag>: main() waits the sessions
**				to end (T2R$K_CTL_DRAIN_SEC at most), closes the listeners and the lines and
**				every descriptor above stderr, then execv("/proc/self/exe") with the saved
**				command line - the same PID, so systemd and procd see nothing (RESTARTING,
**				EXECERR). g_t2r_ident/g_t2r_rev/g_start_time are given to T2R-CTL.
**
**	17-NOV-2026	CLD	V.01-01ECO01 / REV: 01.01.01 (package version 1.1.1) - T2R-TTY, T2R-NET: a
**				request of a TCP client which has closed while it waited for a busy line is
**				dropped, its session frees the <connlm> slot within a lock slice (was: a NOANSWER of a dead
**				unit turned into a cascade of CONNLIMIT/NETPOLLEV under connection churn).
**
**	17-NOV-2026	CLD	V.01-01ECO02 / REV: 01.01.02 (package version 1.1.2) - The counted strings go
**				to the messages as they are: CHECKOK takes &g_settings (!AC, was $ASCPTR() for
**				!AZ - the length was thrown away), t2r$ctl_start() takes &g_ctlsock.
**				The <target> of a listener and the <device> of a serial are kept as ASC
**				(__util$str2asc(); up to ASC$K_SZ - 1 octets, was 64; a longer one is
**				SERBADREC/LSNBADREC - so the copy is whole and NUL-terminated with any StarLet,
**				1.6.10 of the OpenWrt kits included), every message shows the device name by
**				!AC; s_settings_find_n_get_serial() compares the length and the text. s_save_argv():
**				a failed strdup() frees the copies made before it (was: a leak).
**
**	21-NOV-2026	CLD	V.01-01ECO03 / REV: 01.01.03 (package version 1.1.3) - The delivery change (no C
**				source change): the systemd unit follows the installation prefix (CMakeLists.txt,
**				contrib/mbusgw-t2r.service.in), no self-link /usr/sbin/mbus-ctl with the prefix /usr.
**
**	22-NOV-2026	CLD	V.01-01ECO04 / REV: 01.01.04 (package version 1.1.4) - The <flow> key of a serials
**				record is parsed (it was documented, but read by nobody): NONE (the default), XON/XOFF,
**				RTS/CTS, case-insensitive; another value or type rejects the record (SERBADFLOW), so does
**				RTS/CTS with <rs485> = 1; a MODBUS listener on an XON/XOFF line is rejected
**				(LSNXONRTU): the kernel would eat the 0x11/0x13 octets of the binary RTU frames.
**				DEVADDED shows the flow control.
**
**	22-NOV-2026	CLD	V.01-02 / REV: 01.02.00 (package version 1.2.0) - RRL: the product version is V.01-02
**				(the <flow> key of V.01-01ECO04 is a new feature); T2R-NET, T2R-TTY, T2R-CTL and MBUS-CTL
**				carry the same ident. No code change.
**
**	22-NOV-2026	CLD	V.01-02ECO01 / REV: 01.02.01 (package version 1.2.1) - The fix of T2R-TTY: a RAW
**				client closed while its data waited for a stopped line is seen (RAWSTALL). No change
**				in this module.
**
**	22-NOV-2026	CLD	V.01-02ECO02 / REV: 01.02.02 (package version 1.2.2) - <lategd> is <iotmo> / 2 by default,
**				not less than T2R$K_LATEGD_FLOOR (was <iotmo> / 4: a device late by more than a quarter of
**				<iotmo> gave its answer to the next read of another client, TheWorks 47-30).
**
**	22-NOV-2026	CLD	V.01-03 / REV: 01.03.00 (package version 1.3.0) - The settings file is read by yaLIBCFG
**				(was libconfig) and checked by a schema (s_cfg_settings[]): the types, ranges and keywords,
**				the settings nobody knows (a typo), a duplicate <device>, and by the check routines <chars>,
**				<bind>, an undefined <target>, RTS/CTS with <rs485>, the RAW rules, XON/XOFF under MODBUS.
**				Every problem is told at once with its file, line and column (CFG-E-...), a single one
**				refuses the whole file (CFGREJECT) - an ill-formed record was skipped and the gateway went
**				on with the rest. A check routine runs when the settings it reads are right, so a cross
**				problem may show only after the first ones are fixed. New limits: a non-positive <iotmo>
**				is an error (was ignored), <rs485>/<ts_enabled>/<takeover> 0..1 (any non-zero enabled),
**				<ts_unit> 0..255, <ts_fncode> 1..127, <ts_base_reg0> 0..65535, more than 128 serials or
**				listeners (was CFGTRUNC), two devices told apart by the case only. RRL: the product
**				version V.01-03; T2R-NET, T2R-TTY, T2R-CTL and MBUS-CTL carry the same ident.
**
**	22-NOV-2026	CLD	V.01-03ECO01 / REV: 01.03.01 (package version 1.3.1) - yaLIBCFG X.01-03ECO01 (1.3.1):
**				CFGREJECT tells the number of the problems again, of a failed parse too (cfg$errcount(), X.01-02); a number with
**				a leading zero is octal as in libconfig 1.6+ (0135 = 93, 08 is BADNUM), ! starts a comment.
**
**	23-NOV-2026	CLD	V.01-04 / REV: 01.04.00 (package version 1.4.0) - yaLIBCFG X.01-04 (1.4.0): a bad value of
**				a setting that has a default does not refuse the file any more (CFG$M_FALLBACK): it is told
**				as a warning (CFG-W-..., "- the default is taken") and the gateway runs with the default -
**				<chars> (part by part: what is good of it stays, CHARSDEF tells what is taken), <flow>,
**				<iotmo>, <lategd>, <rs485>, <ts_*>, <desc> of a serial; <connlm>, <idletmo>, <iotmo>,
**				<takeover> of a listener; CFGDEFAULTS sums them up. Still refused: <device>, <bind>, <target>,
**				<mode>, an unknown setting, a missing one, the cross checks. A speed of <chars> must be a line
**				rate of the build (t2r$tty_speed_ok(); was any number 50..4000000, the tty took 9600 for
**				another, BADSPEED). CFGREJECT counts the errors only (cfg$diagcount()). RRL: the product
**				version V.01-04; T2R-NET, T2R-TTY, T2R-CTL and MBUS-CTL carry the same ident.
**
**	23-NOV-2026	CLD	V.01-04ECO01 / REV: 01.04.01 (package version 1.4.1) - no code change: the DEB and
**				RPM packages go to the system paths (/usr/sbin, /etc/mbusgw-t2r, /usr/share/doc; was
**				/opt/mbusgw-t2r), man pages mbusgw-t2r(8) and mbus-ctl(8) (PR #5, IgorKha); the RPM
**				scriptlets: a failure of systemctl does not stop the removal.
**
**	23-NOV-2026	CLD	V.01-04ECO02 / REV: 01.04.02 (package version 1.4.2) - no code change: the .deb
**				maintainer scripts as dh_installsystemd (Debian Policy 6, 9.3): apt remove keeps the unit
**				enabled, a purge removes its links; abort-remove starts it again; a failure of systemd
**				never breaks the installation or the removal.
**
**	23-NOV-2026	CLD	V.01-04ECO03 / REV: 01.04.03 (package version 1.4.3) - s_save_argv(): the copy of the
**				command line is one block (the pointers, then the strings) instead of a strdup() of each
**				argument - nothing to unwind on a failure; gcc -fanalyzer (GCC 12) lost the strdup() copies
**				stored by an index in the loop and told a false leak [CWE-401].
**
**	25-NOV-2026	CLD	V.01-04ECO04 / REV: 01.04.04 (package version 1.4.4) - s_settings_process_listeners():
**				l_why[] is 256 octets (was 160): GCC 14 at -O3 (a Release build) stopped on format-truncation in
**				s_bind_parse(); no change of behaviour. T2R-CTL carries the same ident.
**--
*/

#include	<signal.h>
#include	<stdint.h>
#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<unistd.h>
#include	<stdarg.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"
#include	"yalibcfg.h"							/* After __FAC__: it brings utility_routines.h */

#ifndef	__ARCH__NAME__
#define	__ARCH__NAME__	"VAX"
#endif

#ifdef _DEBUG
	#ifndef	__TRACE__
		#define	__TRACE__
	#endif
#endif // DEBUG


static const	ASC	__ident__ = {$ASCINI(__FAC__ " "  __IDENT__ "/"  __ARCH__NAME__   "(built at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")")},
	__rev__ = {$ASCINI(__REV__)};



static	EMSG_RECORD __t2r_msgs__ [] = {					/* Create array of message records */
	/*
	 * The record text starts with the message abbreviation only: the %<facility>-<severity>-
	 * frame is prefixed by __util$putmsg*() itself (see the .facname below), so the final
	 * output line looks like: %T2R-E-CRC16ERR, <the record text>
	 */
	#define	$DEF_MSG(f, s, c, t)	{ .sts = f##$__##c, .textl = 0, .text = #c ", " t },	/* The rest is stamped by __util$inimsg() */
	__DEF_MESSAGES__
	#undef	$DEF_MSG
};


static	EMSG_RECORD_DESC __t2r_msgs_desc__  = {
	.link = NULL,							/* Must beee NULL here */
	.facno = T2R,
	.msgnr = $ARRSZ(__t2r_msgs__),
	.msgrec = __t2r_msgs__,					/* An address of the message records array */
	.facname = "T2R"					/* The %T2R-<S>- framing of __util$putmsg*() */
};


ASC	g_logfspec = {0, {0}},
	g_confspec = {0, {0}},
	g_settings = {0, {0}},
	g_ctlsock = {$ASCINI(T2R$K_CTL_SOCK)}				/* /CONTROL=<path>, NONE - off		*/
	;


int	g_exit_flag = 0,						/* Global flag 'all must to be stop'	*/
	g_restart_flag = 0,						/* RESTART GATEWAY: re-exec after the exit	*/
	g_check = 0,							/* /CHECK: check the settings and exit	*/
	g_trace = 0;							/* A flag to produce extensible logging	*/

time_t	g_start_time;							/* SHOW GATEWAY: the uptime		*/
const ASC  g_t2r_ident = {$ASCINI(__IDENT__)},					/* SHOW GATEWAY: the revision		*/
	g_t2r_rev = {$ASCINI(__REV__)};

static char	**s_argv;						/* A copy of the command line: RESTART	*/

int	g_logsize = 0							/* A size in octets of the log file */
	;



T2R$_SERIAL	g_serials[T2R$K_MAX_SERIALS];				/* A table of serials, filled from configuration at startup time */
int		g_serials_nr;						/* Count of records in the table */

T2R$_LISTENER	g_listeners[T2R$K_MAX_LISTENERS];			/* A table of listeners, filled from configuration at startup time */
int		g_listeners_nr;						/* Count of records in the table */




static KWDENT	s_flow_tbl[] =						/* <flow> of a serial, case-insensitive; indexed by T2R$K_FLOW_* */
{
	{ {$ASCINI("NONE")},		{.val = T2R$K_FLOW_NONE} },
	{ {$ASCINI("XON/XOFF")},	{.val = T2R$K_FLOW_XONXOFF} },
	{ {$ASCINI("RTS/CTS")},	{.val = T2R$K_FLOW_RTSCTS} }
};



static const OPTS g_optstbl [] =					/* General CLI options		*/
{
	{{$ASCINI("config")},	&g_confspec, ASC$K_SZ - 1,	OPTS$K_CONF},
	{{$ASCINI("trace")},	&g_trace, 0,			OPTS$K_OPT},
	{{$ASCINI("logfile")},	&g_logfspec, ASC$K_SZ - 1,	OPTS$K_STR},
	{{$ASCINI("logsize")},	&g_logsize, 0,			OPTS$K_INT},

	{{$ASCINI("settings")},	&g_settings,  ASC$K_SZ - 1,	OPTS$K_STR},
	{{$ASCINI("control")},	&g_ctlsock,  ASC$K_SZ - 1,	OPTS$K_STR},
	{{$ASCINI("check")},	&g_check, 0,			OPTS$K_OPT},

	OPTS_NULL
};



const char	help [] = { "Usage:\n" \
	"$ %s [<options_list>]\n\n" \
	"\t/SETTINGS=<file>   settings file: serial lines and network listeners\n" \
	"\t/CONFIG=<file>     configuration options file path\n" \
	"\t/TRACE             enable extensible diagnostic output\n" \
	"\t/LOGFILE=<file>    a specification of file to accept logging\n" \
	"\t/LOGSIZE=<number>  a maximum size of file in octets\n" \
	"\t/CONTROL=<path>    the control socket for mbus-ctl, NONE - off (default " T2R$K_CTL_SOCK ")\n" \
	"\t/CHECK             check the settings file and exit\n" \

	"\n\tExample of usage:\n\t $ %s -config=t2r_config.conf /settings=t2r_settings.conf /trace\n" };


/*
 *   DESCRIPTION: Lookup (case-insensitive) a serial context for a given device name in the global
 *	table of serials.
 *
 *   INPUTS:
 *	a_target:	A device name for looking for
 *
 *   OUTPUTS:
 *	a_serial:	A returned serial context
 *
 *   IMPLICIT INPUTS:
 *	g_serials, g_serials_nr
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the device has been found
 *	T2R$__TGTNDEF	- the device has not been defined (signalled)
 */
static int s_settings_find_n_get_serial (
			const	char	*a_target,
			T2R$_SERIAL	**a_serial
		)
{
	/*
	 * Run over has been defined array of serials - lookup by give name
	 */
	for ( int i = 0; i < g_serials_nr; i++)
		{
		if ( (strnlen(a_target, ASC$K_SZ - 1) == $ASCLEN(&g_serials[i].devname))	/* As __util$str2asc() keeps it */
			&& !strncasecmp(a_target, $ASCPTR(&g_serials[i].devname), $ASCLEN(&g_serials[i].devname)) )
			return	*a_serial = &g_serials[i], STS$K_SUCCESS;
		}

	return	$PUTMSG_FAO(T2R$__TGTNDEF, a_target);
}




/*
 * The schema of the settings file (yaLIBCFG, CFG$VALIDATE): every setting with its type, range or
 * keywords; a setting the schema does not know is an error (a typo never passes in silence). The
 * cross checks are the check routines below. The whole file is checked before anything is applied
 * and every problem is told with its file, line and column; a single error refuses the file. A
 * setting marked CFG$M_FALLBACK has a default: a bad value of it is a warning (CFG-W-...) and the
 * default is taken (cfg$get_*() gives CFG$_FALLBACK). Those without the flag have no sensible
 * default - a device, an address, the mode of a listener - or they are cross checks.
 */
static const CFG$KEYWORD	s_cfg_flow_kwd[] =				/* <flow> of a serial */
{
	{ "NONE",	T2R$K_FLOW_NONE },
	{ "XON/XOFF",	T2R$K_FLOW_XONXOFF },
	{ "RTS/CTS",	T2R$K_FLOW_RTSCTS },
	{ NULL,		0 }
};

static const CFG$KEYWORD	s_cfg_mode_kwd[] =				/* <mode> of a listener */
{
	{ "MODBUS",	T2R$K_MODE_MODBUS },
	{ "RAW",	T2R$K_MODE_RAW },
	{ NULL,		0 }
};

static unsigned	s_cfg_chk_chars (CFG$NODE *, void *, char *, uint32_t, uint32_t *);
static unsigned	s_cfg_chk_serial (CFG$NODE *, void *, char *, uint32_t, uint32_t *);
static unsigned	s_cfg_chk_bind (CFG$NODE *, void *, char *, uint32_t, uint32_t *);
static unsigned	s_cfg_chk_listener (CFG$NODE *, void *, char *, uint32_t, uint32_t *);
static unsigned	s_cfg_chk_listeners (CFG$NODE *, void *, char *, uint32_t, uint32_t *);
static unsigned	s_cfg_chk_serials (CFG$NODE *, void *, char *, uint32_t, uint32_t *);

static const CFG$SCHEMA	s_cfg_serial[] =					/* An element of <serials> */
{
	{ .name = "desc",	.type = CFG$K_TYPE_STRING,	.flags = CFG$M_FALLBACK, .help = "a free text" },
	{ .name = "device",	.type = CFG$K_TYPE_STRING,	.flags = CFG$M_KEY | CFG$M_LENGTH, .min = 1, .max = ASC$K_SZ - 1,
		.help = "the device file of the serial port" },
	{ .name = "chars",	.type = CFG$K_TYPE_STRING,	.flags = CFG$M_REQUIRED | CFG$M_FALLBACK, .check = s_cfg_chk_chars,
		.help = "speed, data bits, parity, stop bits" },
	{ .name = "flow",	.type = CFG$K_TYPE_KWD,		.flags = CFG$M_FALLBACK, .kwd = s_cfg_flow_kwd, .help = "the flow control" },
	{ .name = "iotmo",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 1, .max = T2R$K_ANSTMO_MAX,
		.help = "the answer timeout, msec" },
	{ .name = "lategd",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = T2R$K_LATEGD_MAX,
		.help = "the guard window after a failed exchange, msec" },
	{ .name = "rs485",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 1 },
	{ .name = "ts_enabled",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 1 },
	{ .name = "ts_unit",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 255 },
	{ .name = "ts_fncode",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 1, .max = 127 },
	{ .name = "ts_base_reg0", .type = CFG$K_TYPE_INT,	.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 65535 },
	{ .name = NULL }
};

static const CFG$SCHEMA	s_cfg_listener[] =					/* An element of <listeners> */
{
	{ .name = "bind",	.type = CFG$K_TYPE_STRING,	.flags = CFG$M_REQUIRED, .check = s_cfg_chk_bind,
		.help = "TCP:<address>:<port>" },
	{ .name = "target",	.type = CFG$K_TYPE_STRING,	.flags = CFG$M_REQUIRED | CFG$M_LENGTH, .min = 1, .max = ASC$K_SZ - 1,
		.help = "the <device> of a serial" },
	{ .name = "connlm",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = T2R$K_CONNLM_MIN, .max = T2R$K_CONNLM_MAX },
	{ .name = "idletmo",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = T2R$K_IDLETMO_MIN, .max = T2R$K_IDLETMO_MAX },
	{ .name = "iotmo",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK, .help = "stored, not used yet" },
	{ .name = "mode",	.type = CFG$K_TYPE_KWD,		.kwd = s_cfg_mode_kwd },
	{ .name = "takeover",	.type = CFG$K_TYPE_INT,		.flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 1 },
	{ .name = NULL }
};

static const CFG$SCHEMA	s_cfg_serial_elem = { .type = CFG$K_TYPE_GROUP, .sub = s_cfg_serial, .check = s_cfg_chk_serial };
static const CFG$SCHEMA	s_cfg_listener_elem = { .type = CFG$K_TYPE_GROUP, .sub = s_cfg_listener, .check = s_cfg_chk_listener };

static const CFG$SCHEMA	s_cfg_settings[] =					/* The file */
{
	{ .name = "listeners",	.type = CFG$K_TYPE_LIST,	.flags = CFG$M_REQUIRED | CFG$M_COUNT, .min = 1, .max = T2R$K_MAX_LISTENERS,
		.sub = &s_cfg_listener_elem, .check = s_cfg_chk_listeners },
	{ .name = "serials",	.type = CFG$K_TYPE_LIST,	.flags = CFG$M_REQUIRED | CFG$M_COUNT, .min = 1, .max = T2R$K_MAX_SERIALS,
		.sub = &s_cfg_serial_elem, .check = s_cfg_chk_serials },
	{ .name = NULL }
};



/*
 *   DESCRIPTION: Put the text of a refusal into the buffer of a check routine.
 *
 *   INPUTS:
 *	a_msgsz:	The size of the buffer
 *	a_fmt, ...:	A vsnprintf() format and its arguments (the text, not a message: the library
 *			puts it out as CFG-E-CHECK with the place in the file)
 *
 *   OUTPUTS:
 *	a_msg:		The text
 *	a_msglen:	Its length
 *
 *   RETURNS:
 *	STS$K_ERROR (always: the value is refused)
 */
static unsigned	s_cfg_refuse (
			char	*a_msg,
			uint32_t a_msgsz,
			uint32_t *a_msglen,
		const	char	*a_fmt,
			...
			)
{
va_list	l_ap;
int	l_len;

	va_start(l_ap, a_fmt);
	l_len = vsnprintf(a_msg, a_msgsz, a_fmt, l_ap);
	va_end(l_ap);

	*a_msglen = (l_len < 0) ? 0 : ((uint32_t) l_len >= a_msgsz) ? a_msgsz - 1 : (uint32_t) l_len;

	return	STS$K_ERROR;
}


/*
 *   DESCRIPTION: Take a string of a node, collapsed (the blanks are dropped) into a local buffer.
 *
 *   INPUTS:
 *	a_base:		The node, or the group of the setting
 *	a_path:		The setting in a_base, NULL - a_base itself
 *	a_bufsz:	The size of a_buf
 *
 *   OUTPUTS:
 *	a_buf:		The collapsed string, zero-terminated
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_ERROR (no such string or too long), CFG$_NOTFOUND, CFG$_TYPE;
 *	CFG$_FALLBACK - a bad value of a CFG$M_FALLBACK setting, a_buf has it all the same
 */
static unsigned	s_cfg_collapsed (
		CFG$NODE	*a_base,
	const	char		*a_path,
		char		*a_buf,
		size_t		a_bufsz
			)
{
const char *l_str = "";
uint32_t l_len = 0;							/* CFG$_FALLBACK of another type: no value, "" */
unsigned l_sts;

	if ( !(1 & (l_sts = cfg$get_string(a_base, a_path, &l_str, &l_len))) && (l_sts != CFG$_FALLBACK) )
		return	l_sts;

	if ( l_len >= a_bufsz )
		return	STS$K_ERROR;

	memcpy(a_buf, l_str, l_len);
	a_buf[l_len] = '\0';
	__util$collapse(a_buf, l_len);						/* "115200, 8, N, 1" -> "115200,8,N,1" */

	return	l_sts;
}


/*
 *   DESCRIPTION: Parse <chars> of a serial: "speed, data bits, parity, stop bits", range checked.
 *
 *   INPUTS:
 *	a_chars:	The collapsed string
 *	a_whysz:	The size of a_why
 *
 *   OUTPUTS:
 *	a_speed, a_databits, a_parity, a_stopbits:	The values (the parity is in upper case)
 *	a_why:		The reason of a refusal
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_ERROR - see a_why
 */
static int	s_chars_parse (
	const	char	*a_chars,
		int	*a_speed,
		int	*a_databits,
		char	*a_parity,
		int	*a_stopbits,
		char	*a_why,
		size_t	a_whysz
			)
{
	if ( 4 != sscanf(a_chars, "%d,%d,%c,%d", a_speed, a_databits, a_parity, a_stopbits) )
		return	snprintf(a_why, a_whysz, "<%s> is ill-formed, \"speed, data bits, parity, stop bits\" expected, e.g. \"19200, 8, E, 1\"",
				a_chars), STS$K_ERROR;

	if ( !$ISINRANGE(*a_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX) )
		return	snprintf(a_why, a_whysz, "speed %d is out of range %d..%d", *a_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX), STS$K_ERROR;

	if ( !t2r$tty_speed_ok(*a_speed) )					/* 19201: the tty has no such line rate */
		return	snprintf(a_why, a_whysz, "speed %d is not a line rate of the serial driver (1200, 9600, 19200, 115200 ...)", *a_speed),
				STS$K_ERROR;

	if ( !$ISINRANGE(*a_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX) )
		return	snprintf(a_why, a_whysz, "data bits %d is out of range %d..%d", *a_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX),
				STS$K_ERROR;

	if ( !$ISINRANGE(*a_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX) )
		return	snprintf(a_why, a_whysz, "stop bits %d is out of range %d..%d", *a_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX),
				STS$K_ERROR;

	switch ( *a_parity = (char) toupper (*a_parity) )
		{
		case	'N':
		case	'E':
		case	'O':
			return	STS$K_SUCCESS;
		}

	return	snprintf(a_why, a_whysz, "parity '%c' is unknown, allowed: N, E, O", *a_parity), STS$K_ERROR;
}


/*
 *   DESCRIPTION: Take what is good of a <chars> the schema found bad (CFG$M_FALLBACK): every part that
 *	is not usable - missing, not a number, out of range, not a line rate - is the default of its own
 *	(T2R$K_BAUD_DEF, _DATABITS_DEF, _PARITY_DEF, _STOPBITS_DEF), the others stay.
 *
 *   INPUTS:
 *	a_chars:	The collapsed string, "" - none at all
 *	a_whysz:	The size of a_why
 *
 *   OUTPUTS:
 *	a_speed, a_databits, a_parity, a_stopbits:	The values (the parity is in upper case)
 *	a_why:		The parts the defaults are taken for: "speed, parity"
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_chars_lenient (
	const	char	*a_chars,
		int	*a_speed,
		int	*a_databits,
		char	*a_parity,
		int	*a_stopbits,
		char	*a_why,
		size_t	a_whysz
			)
{
static const char *s_part[4] = { "speed", "data bits", "parity", "stop bits" };
char	l_f[4][32], *l_end;
const char *l_p = a_chars, *l_c;
size_t	l_len, l_wlen = 0;
long	l_v;
int	l_ok[4] = { 0 }, l_n;

	for ( l_n = 0; (l_n < 4) && *l_p; l_n++ )
		{
		l_len = (l_c = strchr(l_p, ',')) ? (size_t) (l_c - l_p) : strlen(l_p);

		if ( l_len >= sizeof(l_f[0]) )
			l_len = 0;						/* Too long for a part: not usable	*/

		memcpy(l_f[l_n], l_p, l_len);
		l_f[l_n][l_len] = '\0';
		l_p = l_c ? l_c + 1 : l_p + strlen(l_p);
		}

	for ( int i = l_n; i < 4; i++ )
		l_f[i][0] = '\0';

	*a_speed = T2R$K_BAUD_DEF;
	*a_databits = T2R$K_DATABITS_DEF;
	*a_parity = T2R$K_PARITY_DEF;
	*a_stopbits = T2R$K_STOPBITS_DEF;

	l_v = strtol(l_f[0], &l_end, 10);
	if ( (l_ok[0] = (*l_f[0] && !*l_end && $ISINRANGE(l_v, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX) && t2r$tty_speed_ok((int) l_v))) )
		*a_speed = (int) l_v;

	l_v = strtol(l_f[1], &l_end, 10);
	if ( (l_ok[1] = (*l_f[1] && !*l_end && $ISINRANGE(l_v, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX))) )
		*a_databits = (int) l_v;

	if ( (l_ok[2] = (l_f[2][0] && !l_f[2][1] && strchr("NEO", toupper((unsigned char) l_f[2][0])))) )
		*a_parity = (char) toupper((unsigned char) l_f[2][0]);

	l_v = strtol(l_f[3], &l_end, 10);
	if ( (l_ok[3] = (*l_f[3] && !*l_end && $ISINRANGE(l_v, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX))) )
		*a_stopbits = (int) l_v;

	a_why[0] = '\0';

	for ( int i = 0; i < 4; i++ )
		if ( !l_ok[i] && (l_wlen < a_whysz) )
			l_wlen += (size_t) snprintf(a_why + l_wlen, a_whysz - l_wlen, "%s%s", l_wlen ? ", " : "", s_part[i]);

	if ( !a_why[0] )							/* Each part is good, the whole is not (s_chars_parse()) */
		snprintf(a_why, a_whysz, "the form of it");

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Parse <bind> of a listener: "TCP:<IPv4 address>:<port>" (UDP is not implemented).
 *
 *   INPUTS:
 *	a_bind:		The collapsed string
 *	a_whysz:	The size of a_why
 *
 *   OUTPUTS:
 *	a_sk:		The address and the port
 *	a_why:		The reason of a refusal
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_ERROR - see a_why
 */
static int	s_bind_parse (
	const	char		*a_bind,
	struct	sockaddr_in	*a_sk,
		char		*a_why,
		size_t		a_whysz
			)
{
char	l_proto[16], l_laddr[32], l_port[8];
int	l_port_nr, l_rc;

	if ( 3 != sscanf(a_bind, "%15[TCPUDPtcpudp]:%31[0-9.]:%7[0-9]", l_proto, l_laddr, l_port) )
		return	snprintf(a_why, a_whysz, "<%s> is ill-formed, \"TCP:<address>:<port>\" expected, e.g. \"TCP:0.0.0.0:502\"", a_bind),
				STS$K_ERROR;

	if ( toupper(*l_proto) == 'U' )						/* listen()/accept() on SOCK_DGRAM does not work */
		return	snprintf(a_why, a_whysz, "UDP is not implemented, use TCP"), STS$K_ERROR;

	if ( toupper(*l_proto) != 'T' )
		return	snprintf(a_why, a_whysz, "<%s> is ill-formed, \"TCP:<address>:<port>\" expected", a_bind), STS$K_ERROR;

	if ( !$ISINRANGE((l_port_nr = atoi(l_port)), T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
		return	snprintf(a_why, a_whysz, "port %d is out of range %d..%d", l_port_nr, T2R$K_PORT_MIN, T2R$K_PORT_MAX), STS$K_ERROR;

	memset(a_sk, 0, sizeof(*a_sk));
	a_sk->sin_family = AF_INET;
	a_sk->sin_port = htons ((uint16_t) l_port_nr);

	if ( 1 != (l_rc = inet_pton(AF_INET, l_laddr, &a_sk->sin_addr)) )
		return	snprintf(a_why, a_whysz, "<%s> is not an IPv4 address (inet_pton()->%d, errno: %d)", l_laddr, l_rc, errno),
				STS$K_ERROR;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Take a keyword setting by the keywords of its schema entry (validated: one of them).
 *
 *   INPUTS:
 *	a_base:		The group of the setting
 *	a_path:		The setting
 *	a_kwd:		The keywords
 *	a_default:	The value for an absent setting
 *
 *   OUTPUTS:
 *	a_val:		The value of the keyword
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static unsigned	s_cfg_kwd (
		CFG$NODE	*a_base,
	const	char		*a_path,
	const	CFG$KEYWORD	*a_kwd,
		int		a_default,
		int		*a_val
			)
{
const char *l_str;
uint32_t l_len;

	*a_val = a_default;

	if ( !(1 & cfg$get_string(a_base, a_path, &l_str, &l_len)) )
		return	STS$K_SUCCESS;

	for ( ; a_kwd->name; a_kwd++ )
		{
		if ( (strlen(a_kwd->name) == l_len) && !strncasecmp(a_kwd->name, l_str, l_len) )
			return	*a_val = (int) a_kwd->value, STS$K_SUCCESS;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Find the element of <serials> whose <device> is the given name (case-insensitive).
 *
 *   INPUTS:
 *	a_root:		The root group of the settings
 *	a_name, a_len:	The device name
 *
 *   OUTPUTS:
 *	a_serial:	The element
 *
 *   RETURNS:
 *	STS$K_SUCCESS, CFG$_NOTFOUND
 */
static unsigned	s_cfg_find_serial (
		CFG$NODE	*a_root,
	const	char		*a_name,
		uint32_t	a_len,
		CFG$NODE	**a_serial
			)
{
CFG$NODE *l_list, *l_elem;
const char *l_str;
uint32_t l_len;
unsigned l_sts;

	if ( !(1 & (l_sts = cfg$lookup(a_root, "serials", &l_list))) )
		return	l_sts;

	for ( l_sts = cfg$first(l_list, &l_elem); 1 & l_sts; l_sts = cfg$next(l_elem, &l_elem) )
		{
		if ( (1 & cfg$get_string(l_elem, "device", &l_str, &l_len)) && (l_len == a_len) && !strncasecmp(l_str, a_name, a_len) )
			return	*a_serial = l_elem, STS$K_SUCCESS;
		}

	return	CFG$_NOTFOUND;
}


/* A check routine: <chars> of a serial */
static unsigned	s_cfg_chk_chars (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
char	l_chars[64], l_parity;
int	l_speed, l_databits, l_stopbits;

	if ( !(1 & s_cfg_collapsed (a_node, NULL, l_chars, sizeof(l_chars))) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "the value is too long");

	if ( !(1 & s_chars_parse (l_chars, &l_speed, &l_databits, &l_parity, &l_stopbits, a_msg, a_msgsz)) )
		return	*a_msglen = (uint32_t) strnlen(a_msg, a_msgsz), STS$K_ERROR;

	return	STS$K_SUCCESS;
}


/* A check routine: a serial as a whole - RTS/CTS is refused together with the RS-485 mode */
static unsigned	s_cfg_chk_serial (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
int	l_flow;
int32_t	l_rs485 = 0;

	s_cfg_kwd (a_node, "flow", s_cfg_flow_kwd, T2R$K_FLOW_NONE, &l_flow);
	if ( !(1 & cfg$get_int(a_node, "rs485", &l_rs485)) )			/* Absent, or bad: its default */
		l_rs485 = 0;

#ifdef HAVE_TIOCRS485
	if ( (l_flow == T2R$K_FLOW_RTSCTS) && l_rs485 )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "<flow> RTS/CTS is not allowed with <rs485> = 1: the RS-485 mode drives RTS itself");
#else
	(void) l_flow, (void) l_rs485;							/* No RS-485 mode: SERNORS485 tells it */
#endif

	return	STS$K_SUCCESS;
}


/* A check routine: <bind> of a listener */
static unsigned	s_cfg_chk_bind (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
char	l_bind[128];
struct	sockaddr_in l_sk;

	if ( !(1 & s_cfg_collapsed (a_node, NULL, l_bind, sizeof(l_bind))) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "the value is too long");

	if ( !(1 & s_bind_parse (l_bind, &l_sk, a_msg, a_msgsz)) )
		return	*a_msglen = (uint32_t) strnlen(a_msg, a_msgsz), STS$K_ERROR;

	return	STS$K_SUCCESS;
}


/*
 * A check routine: a listener as a whole (a_arg - the root group): <target> names a serial; a RAW
 * listener has <connlm> 1 only; <takeover> is for RAW only; a MODBUS listener is refused on an
 * XON/XOFF line - IXON makes the kernel eat the 0x11/0x13 octets of the binary RTU frames.
 */
static unsigned	s_cfg_chk_listener (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
CFG$NODE *l_serial, *l_dummy;
const char *l_target;
uint32_t l_len;
int	l_mode, l_flow;
int32_t	l_int;

	s_cfg_kwd (a_node, "mode", s_cfg_mode_kwd, T2R$K_MODE_MODBUS, &l_mode);

	if ( (l_mode == T2R$K_MODE_RAW) && (1 & cfg$get_int(a_node, "connlm", &l_int)) && (l_int != 1) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "<connlm> %d: a RAW listener serves one client, only 1 is allowed", l_int);

	if ( (l_mode != T2R$K_MODE_RAW) && (1 & cfg$lookup(a_node, "takeover", &l_dummy)) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "<takeover> is not allowed in the MODBUS mode");

	cfg$get_string(a_node, "target", &l_target, &l_len);

	if ( !(1 & s_cfg_find_serial (a_arg, l_target, l_len, &l_serial)) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "<target> \"%s\" is not a <device> of <serials>", l_target);

	s_cfg_kwd (l_serial, "flow", s_cfg_flow_kwd, T2R$K_FLOW_NONE, &l_flow);

	if ( (l_mode == T2R$K_MODE_MODBUS) && (l_flow == T2R$K_FLOW_XONXOFF) )
		return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "target \"%s\" has <flow> XON/XOFF: the binary RTU frames carry the 0x11/0x13 "
			"octets, a MODBUS listener needs NONE or RTS/CTS", l_target);

	return	STS$K_SUCCESS;
}


/*
 * A check routine: <listeners> as a whole - the serial line of a RAW listener belongs to it alone: a
 * raw stream and MODBUS exchanges (or two raw streams) on one line would corrupt each other.
 */
static unsigned	s_cfg_chk_listeners (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
CFG$NODE *l_a, *l_b;
const char *l_ta, *l_tb, *l_bind;
uint32_t l_la, l_lb, l_lbind;
int	l_ma, l_mb;
unsigned l_sa, l_sb;

	for ( l_sa = cfg$first(a_node, &l_a); 1 & l_sa; l_sa = cfg$next(l_a, &l_a) )
		{
		s_cfg_kwd (l_a, "mode", s_cfg_mode_kwd, T2R$K_MODE_MODBUS, &l_ma);
		cfg$get_string(l_a, "target", &l_ta, &l_la);

		for ( l_sb = cfg$next(l_a, &l_b); 1 & l_sb; l_sb = cfg$next(l_b, &l_b) )
			{
			s_cfg_kwd (l_b, "mode", s_cfg_mode_kwd, T2R$K_MODE_MODBUS, &l_mb);
			cfg$get_string(l_b, "target", &l_tb, &l_lb);

			if ( ((l_ma == T2R$K_MODE_RAW) || (l_mb == T2R$K_MODE_RAW)) && (l_la == l_lb) && !strncasecmp(l_ta, l_tb, l_la) )
				{
				cfg$get_string(l_b, "bind", &l_bind, &l_lbind);

				return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "target \"%s\" is used by the listener <%s> too, a RAW listener "
					"needs the serial line for itself", l_ta, l_bind);
				}
			}
		}

	return	STS$K_SUCCESS;
}



/*
 * A check routine: <serials> as a whole - the key <device> is unique as it is written (DUPKEY), but a
 * <target> names it regardless of case: two devices told apart by the case only are refused too.
 */
static unsigned	s_cfg_chk_serials (
		CFG$NODE	*a_node,
		void		*a_arg,
		char		*a_msg,
		uint32_t	a_msgsz,
		uint32_t	*a_msglen
			)
{
CFG$NODE *l_a, *l_b;
const char *l_da, *l_db;
uint32_t l_la, l_lb;
unsigned l_sa, l_sb;

	for ( l_sa = cfg$first(a_node, &l_a); 1 & l_sa; l_sa = cfg$next(l_a, &l_a) )
		{
		cfg$get_string(l_a, "device", &l_da, &l_la);

		for ( l_sb = cfg$next(l_a, &l_b); 1 & l_sb; l_sb = cfg$next(l_b, &l_b) )
			{
			cfg$get_string(l_b, "device", &l_db, &l_lb);

			if ( (l_la == l_lb) && !strncasecmp(l_da, l_db, l_la) )
				return	s_cfg_refuse(a_msg, a_msgsz, a_msglen, "<device> \"%s\" and \"%s\" differ by the case only, a <target> "
					"could not tell them apart", l_da, l_db);
			}
		}

	return	STS$K_SUCCESS;
}



/*
 *  DESCRIPTION: Take the <serials> section of a validated settings tree (see s_cfg_settings[]):
 *	<device>, <chars>; <rs485>, <flow> (NONE - the default), <iotmo> (the answer timeout, msec,
 *	default T2R$K_ANSTMO_MSEC), <lategd> (the guard window after a failed exchange, msec, default
 *	<iotmo> / 2, T2R$K_LATEGD_FLOOR..T2R$K_LATEGD_MAX), <ts_enabled>, <ts_fncode>, <ts_base_reg0>, <ts_unit>. Compute the octet time and
 *	t3.5, init the device lock, fill the global table of serial devices.
 *
 *  INPUTS:
 *	a_root:		The root group of the validated tree
 *
 *  IMPLICITE OUTPUTS:
 *	g_serials, g_serials_nr
 *
 *  RETURNS:
 *	STS$K_SUCCESS, T2R$__DEVSYSERR (pthread_mutex_init())
 */
static int	s_settings_process_serials (CFG$NODE *a_root)
{
CFG$NODE *l_list, *l_elem;
const char *l_str;
uint32_t l_len;
int32_t	l_int;
int	l_speed, l_databits, l_stopbits, l_rc;
unsigned l_sts, l_csts;
char	l_parity = 0, l_chars[64], l_why[160];
T2R$_SERIAL	*l_serial;

	cfg$lookup(a_root, "serials", &l_list);

	for ( int i = 0; (1 & (l_sts = (i ? cfg$next(l_elem, &l_elem) : cfg$first(l_list, &l_elem)))); i++ )
		{
		l_serial = &g_serials[g_serials_nr];
		memset(l_serial, 0, sizeof(T2R$_SERIAL) );

		cfg$get_string(l_elem, "device", &l_str, &l_len);
		__util$str2asc(l_str, &l_serial->devname);			/* NUL-terminated: open() takes its sts */

		if ( (1 & cfg$get_int(l_elem, "rs485", &l_int)) && l_int )
			{
#ifdef HAVE_TIOCRS485
			l_serial->flags |=  T2R$M_SERIAL_RS485;
#else
			$PUTMSG_FAO(T2R$__SERNORS485, i, &l_serial->devname);
#endif
			}

		/*
		 * Validated by s_cfg_chk_chars(); a bad one (CFG$_FALLBACK: the string is given all the same, of
		 * another type it is "") keeps what is good of it, the rest is the default; one too long for
		 * l_chars is all the defaults.
		 */
		if ( 1 & (l_csts = s_cfg_collapsed (l_elem, "chars", l_chars, sizeof(l_chars))) )
			s_chars_parse (l_chars, &l_speed, &l_databits, &l_parity, &l_stopbits, l_why, sizeof(l_why));
		else	{
			if ( l_csts != CFG$_FALLBACK )				/* Of another type, too long: nothing of it */
				l_chars[0] = '\0';

			s_chars_lenient (l_chars, &l_speed, &l_databits, &l_parity, &l_stopbits, l_why, sizeof(l_why));
			$PUTMSG_FAO(T2R$__CHARSDEF, i, &l_serial->devname, l_chars, l_why, l_speed, l_databits, 1, &l_parity, l_stopbits);
			}

		s_cfg_kwd (l_elem, "flow", s_cfg_flow_kwd, T2R$K_FLOW_NONE, &l_serial->flow);

		l_serial->fd = -1;
		l_serial->baud = l_speed;
		l_serial->parity = l_parity;
		l_serial->stopbits = l_stopbits;
		l_serial->databits = l_databits;

		l_serial->anstmo_msec = T2R$K_ANSTMO_MSEC;

		if ( 1 & cfg$get_int(l_elem, "iotmo", &l_int) )
			l_serial->anstmo_msec = l_int;

		l_serial->lategd_msec = l_serial->anstmo_msec / 2;		/* The guard window after a failed exchange */

		if ( l_serial->lategd_msec < T2R$K_LATEGD_FLOOR )
			l_serial->lategd_msec = T2R$K_LATEGD_FLOOR;
		else if ( l_serial->lategd_msec > T2R$K_LATEGD_MAX )		/* A long <iotmo>: as an explicit one could be */
			l_serial->lategd_msec = T2R$K_LATEGD_MAX;

		if ( 1 & cfg$get_int(l_elem, "lategd", &l_int) )
			l_serial->lategd_msec = l_int;

		/* A time to shift out a single octet: start + data + parity + stop bits */
		l_serial->onebyte_time_usec = (1000000 * (1 + l_serial->databits
			+ ((l_serial->parity == 'N') ? 0 : 1) + l_serial->stopbits)) / l_serial->baud;

		/*
		 * t3.5 - the inter-frame silent interval by the MODBUS over Serial Line specification:
		 * 3.5 character times, but not less than 1750 usec for the rates above 19200 baud.
		 */
		l_serial->inter_pdu_usec = (7 * l_serial->onebyte_time_usec) / 2;

		if ( (19200 < l_serial->baud) && (1750 > l_serial->inter_pdu_usec) )
			l_serial->inter_pdu_usec = 1750;

		l_serial->inter_pdu_ts.tv_sec = l_serial->inter_pdu_usec / 1000000;
		l_serial->inter_pdu_ts.tv_nsec = (long) (l_serial->inter_pdu_usec % 1000000) * 1000L;

		if ( (1 & cfg$get_int(l_elem, "ts_enabled", &l_int)) && l_int )
			{
			l_serial->flags |=  T2R$M_SERIAL_ADDTS;

			l_serial->ts_fncode = (1 & cfg$get_int(l_elem, "ts_fncode", &l_int)) ? (uint8_t) l_int : T2R$K_TS_FNCODE;
			l_serial->ts_base_reg0 = (1 & cfg$get_int(l_elem, "ts_base_reg0", &l_int)) ? (uint16_t) l_int : T2R$K_TS_BASE_REG0;
			l_serial->ts_unit_nr = (1 & cfg$get_int(l_elem, "ts_unit", &l_int)) ? (uint8_t) l_int : T2R$K_TS_UNIT;
			}

		if ( (l_rc = pthread_mutex_init(&l_serial->lock, NULL)) )
			return	$PUTMSG_FAO(T2R$__DEVSYSERR, &l_serial->devname, "pthread_mutex_init()", l_rc, errno);

		$PUTMSG_FAO(T2R$__DEVADDED, i, &l_serial->devname, l_serial->baud, l_serial->databits, 1, &l_parity,
			l_serial->stopbits, &s_flow_tbl[l_serial->flow].kwd, l_serial->anstmo_msec, l_serial->inter_pdu_usec);

		if ( l_serial->flags &  T2R$M_SERIAL_ADDTS )
			$PUTMSG_FAO(T2R$__TSENABLED, &l_serial->devname, l_serial->ts_unit_nr, l_serial->ts_fncode, l_serial->ts_base_reg0);
		else	$PUTMSG_FAO(T2R$__TSDISABLED, &l_serial->devname);

		g_serials_nr++;
		}

	return	STS$K_SUCCESS;
}



/*
 *  DESCRIPTION: Take the <listeners> section of a validated settings tree (see s_cfg_settings[]):
 *	<bind>, <target>; <connlm> (default T2R$K_CONNLM_DEF), <idletmo> (default T2R$K_IDLETMO_DEF),
 *	<iotmo> (is stored, not used yet), <mode> (MODBUS - the default, RAW) and, for RAW, <takeover>
 *	(default 1). A RAW listener has <connlm> 1 and <idletmo> T2R$K_IDLETMO_MAX by default. Fill
 *	the global table of listeners.
 *
 *  INPUTS:
 *	a_root:		The root group of the validated tree
 *
 *  IMPLICITE INPUTS:
 *	g_serials, g_serials_nr (see s_settings_find_n_get_serial())
 *
 *  IMPLICITE OUTPUTS:
 *	g_listeners, g_listeners_nr
 *
 *  RETURNS:
 *	STS$K_SUCCESS, T2R$__TGTNDEF, T2R$__SYSERR (pthread_mutex_init())
 */
static int	s_settings_process_listeners (CFG$NODE *a_root)
{
CFG$NODE *l_list, *l_elem;
const char *l_str;
uint32_t l_len;
int32_t	l_int;
int	l_rc;
unsigned l_sts;
char	l_bind[128], l_why[256];
T2R$_LISTENER	*l_listener;

	cfg$lookup(a_root, "listeners", &l_list);

	for ( int i = 0; (1 & (l_sts = (i ? cfg$next(l_elem, &l_elem) : cfg$first(l_list, &l_elem)))); i++ )
		{
		l_listener = &g_listeners[g_listeners_nr];
		memset(l_listener, 0, sizeof(T2R$_LISTENER));

		s_cfg_collapsed (l_elem, "bind", l_bind, sizeof(l_bind));	/* Validated by s_cfg_chk_bind() */
		s_bind_parse (l_bind, &l_listener->sk, l_why, sizeof(l_why));
		l_listener->proto = IPPROTO_TCP;

		if ( 1 & cfg$get_int(l_elem, "iotmo", &l_int) )
			l_listener->iotmo = l_int;

		s_cfg_kwd (l_elem, "mode", s_cfg_mode_kwd, T2R$K_MODE_MODBUS, &l_listener->mode);

		/*
		 * A RAW listener relays a stream of a single client: one session at a time, and the client
		 * may stay silent for hours (a terminal, a rarely polling SCADA) - a dead peer is detected
		 * by the TCP keepalive, not by the idle timeout.
		 */
		if ( l_listener->mode == T2R$K_MODE_RAW )
			{
			l_listener->connlm = 1;
			l_listener->idletmo = T2R$K_IDLETMO_MAX;
			l_listener->takeover = (1 & cfg$get_int(l_elem, "takeover", &l_int)) ? l_int : 1;
			}
		else	{
			l_listener->connlm = T2R$K_CONNLM_DEF;
			l_listener->idletmo = T2R$K_IDLETMO_DEF;
			}

		if ( 1 & cfg$get_int(l_elem, "connlm", &l_int) )
			l_listener->connlm = l_int;

		if ( 1 & cfg$get_int(l_elem, "idletmo", &l_int) )
			l_listener->idletmo = l_int;

		cfg$get_string(l_elem, "target", &l_str, &l_len);

		if ( !(1 & (l_rc = s_settings_find_n_get_serial (l_str, &l_listener->serial))) )
			return	l_rc;						/* Validated by s_cfg_chk_listener() */

		__util$str2asc(l_str, &l_listener->target);

		if ( (l_rc = pthread_mutex_init(&l_listener->rawlock, NULL)) )
			return	$PUTMSG_FAO(T2R$__SYSERR, "pthread_mutex_init(rawlock)", l_rc, errno);

		$PUTMSG_FAO(T2R$__LSNADDED, i, &l_listener->serial->devname, "TCP",
			&l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port), l_listener->connlm, l_listener->idletmo,
			(l_listener->mode == T2R$K_MODE_RAW) ? (l_listener->takeover ? "RAW (takeover)" : "RAW") : "MODBUS");

		g_listeners_nr++;
		}

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Load the settings file by yaLIBCFG (names regardless of case), validate the whole
 *	tree by s_cfg_settings[] - every problem is put out with its file, line and column (CFG-E-...),
 *	a single one refuses the file (CFGREJECT) - and take its sections: <serials> first (the
 *	listeners refer to the serial devices), then <listeners>. The tree is freed at the end: the
 *	values are copied into g_serials[] and g_listeners[].
 *
 *   INPUTS:
 *	a_settings_conf: A file specification of the settings file
 *
 *   IMPLICITE OUTPUTS:
 *	g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	condition code
 */
static int	s_settings_load_n_parse (
		const char	*a_settings_conf
		)
{
CFG$TREE *l_tree = NULL;
CFG$NODE *l_root;
uint32_t l_nerr = 0, l_nwarn = 0;
unsigned l_rc;

	cfg$inimsg();								/* The texts of CFG-x-... ; a repeated call is harmless */

	if ( !(1 & (l_rc = cfg$parse_file(a_settings_conf, CFG$M_NOCASE, &l_tree))) )
		{
		cfg$puterrs(l_tree);

		if ( !(1 & cfg$errcount(l_tree, &l_nerr, NULL)) || !l_nerr )		/* No tree: no memory */
			l_nerr = 1;

		cfg$free(l_tree);

		return	$PUTMSG_FAO(T2R$__CFGREJECT, a_settings_conf, l_nerr);
		}

	cfg$root(l_tree, &l_root);							/* The check routines look up the serials from it */

	if ( !(1 & (l_rc = cfg$validate(l_tree, s_cfg_settings, l_root, &l_nerr))) )
		{
		cfg$puterrs(l_tree);

		if ( !l_nerr )							/* Not the file: the schema, no memory ... */
			$PUTMSG_FAO(T2R$__SYSERR, "cfg$validate()", (int) l_rc, 0);
		else	cfg$diagcount(l_tree, &l_nerr, NULL);			/* The errors: the warnings refuse nothing */

		cfg$free(l_tree);

		return	$PUTMSG_FAO(T2R$__CFGREJECT, a_settings_conf, l_nerr ? l_nerr : 1);
		}

	cfg$puterrs(l_tree);							/* The warnings: the defaults taken */

	if ( (1 & cfg$diagcount(l_tree, NULL, &l_nwarn)) && l_nwarn )
		$PUTMSG_FAO(T2R$__CFGDEFAULTS, a_settings_conf, l_nwarn);

	if ( 1 & (l_rc = s_settings_process_serials (l_root)) )
		l_rc = s_settings_process_listeners (l_root);

	cfg$free(l_tree);

	return	l_rc;
}




/*
 *   DESCRIPTION: Load and parse the settings file which is given by /SETTINGS (see
 *	s_settings_load_n_parse()).
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_settings
 *
 *   IMPLICITE OUTPUTS:
 *	g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	the condition code of s_settings_load_n_parse()
 */
static int	s_config_validation	(void)
{
int l_rc;

	l_rc = s_settings_load_n_parse ($ASCPTR(&g_settings));
	return	l_rc;
}



/*
 *   DESCRIPTION: A handler of the asynchronous signals. The termination signals (SIGTERM,
 *	SIGINT, SIGQUIT) do set the <g_exit_flag>, SIGUSR1 toggles the tracing at run-time. Any
 *	signal delivered after <g_exit_flag> has been set, and any unexpected signal, terminates
 *	the process at once by _exit(<signal number>). Only the async-signal-safe calls are used
 *	here: write() instead of fprintf()/fflush().
 *
 *   INPUTS:
 *	a_signo:	A number of the has been delivered signal
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_exit_flag, g_trace
 *
 *   IMPLICITE OUTPUTS:
 *	g_exit_flag, g_trace
 *
 *   RETURNS:
 *	NONE
 */
static	void	s_sig_handler (int a_signo)
{
	/*
	 * Only the async-signal-safe calls are allowed here: write() instead of fprintf()/fflush().
	 */
	if ( g_exit_flag )
		{
		static const char l_msg [] = "Exit flag has been set, exiting ...\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}

		_exit(a_signo);
		}


	if ( (a_signo == SIGTERM) || (a_signo == SIGINT) || (a_signo == SIGQUIT))
		{
		static const char l_msg [] = "Got a termination signal, set exit_flag!\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}

		g_exit_flag = 1;
		return;
		}
	else if ( a_signo == SIGUSR1)
		{
		g_trace = !g_trace;

		if ( g_trace )
			{
			static const char l_msg [] = "Set /TRACE=ON\n";
			if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
			}
		else	{
			static const char l_msg [] = "Set /TRACE=OFF\n";
			if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
			}

		return;
		}
	else	{
		static const char l_msg [] = "Got an unexpected signal\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
		}

	_exit(a_signo);
}

/*
 *   DESCRIPTION: Establish the handler of the asynchronous signals (SIGTERM, SIGINT, SIGQUIT -
 *	the termination, SIGUSR1 - the trace toggling), ignore SIGPIPE (a write to a has been
 *	closed socket must be seen as an EPIPE error, not as a process termination).
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_trace
 *
 *   RETURNS:
 *	NONE
 */
static void	s_init_sig_handler(void)
{
const int siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0 };

	/*
	 * Establishing a signals handler
	 */
	signal(SIGPIPE, SIG_IGN);	/* We don't want to crash the server due fucking unix shit */

	for ( int i = 0; siglist[i]; i++)
		{
		if ( (signal(siglist[i], s_sig_handler)) == SIG_ERR )
			$PUTMSG_FAO(T2R$__SIGERR, siglist[i], errno);

		$IFTRACE(g_trace, "Set handler for signal %d/%#x (%s)", siglist[i], siglist[i], strsignal(siglist[i]));
		}
}



/*
 *   DESCRIPTION: Keep a copy of the command line for the RESTART: the strings of argv[] may be
 *	touched by the options parser. The copy is one block: the pointers, the NULL, then the
 *	strings - it lives as long as the process does, nothing is to be freed.
 *
 *   INPUTS:
 *	argc, argv:	The command line
 *
 *   IMPLICIT OUTPUTS:
 *	s_argv
 *
 *   RETURNS:
 *	NONE (no memory - no RESTART, s_argv stays NULL)
 */
static void	s_save_argv (
		int	argc,
		char	**argv
		)
{
size_t	l_sz = ((size_t) argc + 1) * sizeof(char *);
size_t	l_len;
char	**l_argv, *l_str;

	for (int i = 0; i < argc; i++)
		l_sz += strlen(argv[i]) + 1;

	if ( !(l_argv = malloc(l_sz)) )
		return;

	l_str = (char *) (l_argv + argc + 1);			/* The strings follow the pointers	*/

	for (int i = 0; i < argc; i++)
		{
		l_len = strlen(argv[i]) + 1;
		l_argv[i] = memcpy(l_str, argv[i], l_len);
		l_str += l_len;
		}

	l_argv[argc] = NULL;
	s_argv = l_argv;
}



/*
 *   DESCRIPTION: RESTART GATEWAY, the second half (T2R-CTL has checked the settings and raised the
 *	flags): wait the sessions to see <g_exit_flag> and end (T2R$K_CTL_DRAIN_SEC at most), close the
 *	listeners and the lines, close every descriptor above stderr (nothing is marked close-on-exec,
 *	a leftover listening socket would fail the bind() of the new image), then execv() the same
 *	program with the same command line. The PID stays the same: systemd and procd see nothing.
 *
 *   IMPLICIT INPUTS:
 *	s_argv, g_logfspec
 *
 *   RETURNS:
 *	only on a failure: EXECERR
 */
static int	s_restart (void)
{
int	l_rc, l_maxfd;

	for (int i = 0; (i < T2R$K_CTL_DRAIN_SEC * 10) && t2r$net_sess_count(); i++)
		usleep(100000);

	t2r$net_stop_listeners ();

	if ( !s_argv )
		return	$PUTMSG_FAO(T2R$__EXECERR, "/proc/self/exe", -1, ENOMEM);

	$PUTMSG_FAO(T2R$__RESTARTING, "/proc/self/exe", t2r$net_sess_count());

	if ( 0 >= (l_maxfd = (int) sysconf(_SC_OPEN_MAX)) || (l_maxfd > 65536) )
		l_maxfd = 65536;

	for (int fd = STDERR_FILENO + 1; fd < l_maxfd; fd++)
		close(fd);

	l_rc = execv("/proc/self/exe", s_argv);

	if ( $ASCLEN(&g_logfspec) )						/* The log file has been closed above */
		__util$deflog($ASCPTR(&g_logfspec), NULL);

	return	$PUTMSG_FAO(T2R$__EXECERR, "/proc/self/exe", l_rc, errno);
}



/*
 *   DESCRIPTION: Main entry point of the program: init the message catalogue, show the revision
 *	banner, parse the command line, switch the logging to /LOGFILE if it is given (the banner
 *	is repeated there), load and validate the settings, establish the signal handling, start
 *	the network subsystem and sleep until <g_exit_flag> is set, then close the listeners and
 *	the serial devices (t2r$net_stop_listeners()).
 *
 *   INPUTS:
 *	argc:		A count of the command line arguments
 *	argv:		An array of the command line arguments; see g_optstbl for the accepted
 *			options (/CONFIG, /TRACE, /LOGFILE, /LOGSIZE, /SETTINGS)
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_optstbl, g_logfspec, g_trace, g_exit_flag
 *
 *   RETURNS:
 *	EXIT_SUCCESS - a normal termination by the exit flag, EXIT_FAILURE - otherwise
 */

int	main(int argc, char **argv)
{
int l_rc = 0;

	g_start_time = time(NULL);
	s_save_argv(argc, argv);

	__util$inimsg(&__t2r_msgs_desc__);

	$PUTMSG_FAO(T2R$__REVISNF, &__ident__, &__rev__);

	/*
	 * Process command line arguments
	 */
	if ( !(1 & __util$getparams(argc, argv, g_optstbl)) )
		return	$PUTMSG_FAO(T2R$__CFGERR), EXIT_FAILURE;


	if ( $ASCLEN(&g_logfspec) )
		{
		__util$deflog($ASCPTR(&g_logfspec), NULL);

		$PUTMSG_FAO(T2R$__REVISNF, &__ident__, &__rev__);
		}

	if ( g_trace )
		__util$showparams(g_optstbl);

	if ( !(1 & s_config_validation()) )
		return	$PUTMSG_FAO(T2R$__CFGABORT), EXIT_FAILURE;

	if ( g_check )								/* /CHECK: that settings is good */
		return	$PUTMSG_FAO(T2R$__CHECKOK, &g_settings, g_serials_nr, g_listeners_nr), EXIT_SUCCESS;


	s_init_sig_handler ();

	if ( (1 & (l_rc = t2r$net_start_listeners ())) )
		{
		t2r$ctl_start (&g_ctlsock);					/* A failure is not fatal: no control only */

		while ( !g_exit_flag)
			{
			for (int delay = 3;  (delay = sleep(delay)); );
			}

		t2r$ctl_stop ();

		if ( g_restart_flag )
			l_rc = s_restart ();					/* Returns only on a failure */
		else	l_rc = t2r$net_stop_listeners ();
		}

	$PUTMSG_FAO(T2R$__EXITST, g_exit_flag, l_rc);

	return	(1 & l_rc) ? EXIT_SUCCESS : EXIT_FAILURE;
}
