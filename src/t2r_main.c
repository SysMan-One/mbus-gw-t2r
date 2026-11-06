#define	__MODULE__	"T2R-MAIN"
#define	__IDENT__	"V.01-00"
#define	__REV__		"01.00.00"

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
**	06-NOV-2026	CLD	(not numbered yet: IDENT/REV stay V.01-00 until the number of the line after V.01-00
**				is chosen; T2R-NET, T2R-CTL and MBUS-CTL are X.01-01 as the modules)
**				The control channel and the mbus-ctl utility (see T2R-CTL
**				and MBUS-CTL): the new /CONTROL=<path> option (default T2R$K_CTL_SOCK, NONE -
**				off), the channel is opened after the listeners are up and closed at the exit;
**				the new /CHECK option (parse the settings, CHECKOK, exit) - RESTART GATEWAY runs
**				it first; an accepted RESTART raises <g_restart_flag>: main() waits the sessions
**				to end (T2R$K_CTL_DRAIN_SEC at most), closes the listeners and the lines and
**				every descriptor above stderr, then execv("/proc/self/exe") with the saved
**				command line - the same PID, so systemd and procd see nothing (RESTARTING,
**				EXECERR). g_t2r_ident/g_t2r_rev/g_start_time are given to T2R-CTL.
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
#include	<libconfig.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"

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
const char g_t2r_ident[] = __IDENT__,					/* SHOW GATEWAY: the revision		*/
	g_t2r_rev[] = __REV__;

static char	**s_argv;						/* A copy of the command line: RESTART	*/

int	g_logsize = 0							/* A size in octets of the log file */
	;



T2R$_SERIAL	g_serials[T2R$K_MAX_SERIALS];				/* A table of serials, filled from configuration at startup time */
int		g_serials_nr;						/* Count of records in the table */

T2R$_LISTENER	g_listeners[T2R$K_MAX_LISTENERS];			/* A table of listeners, filled from configuration at startup time */
int		g_listeners_nr;						/* Count of records in the table */



config_t	g_cfg;							/* Lib Config API context */

static KWDENT	s_mode_tbl[] =						/* <mode> of a listener, case-insensitive */
{
	{ {$ASCINI("MODBUS")},	{.val = T2R$K_MODE_MODBUS} },
	{ {$ASCINI("RAW")},	{.val = T2R$K_MODE_RAW} }
};



static const OPTS g_optstbl [] =					/* General CLI options		*/
{
	{{$ASCINI("config")},	&g_confspec, ASC$K_SZ,		OPTS$K_CONF},
	{{$ASCINI("trace")},	&g_trace, 0,			OPTS$K_OPT},
	{{$ASCINI("logfile")},	&g_logfspec, ASC$K_SZ,		OPTS$K_STR},
	{{$ASCINI("logsize")},	&g_logsize, 0,			OPTS$K_INT},

	{{$ASCINI("settings")},	&g_settings,  ASC$K_SZ,		OPTS$K_STR},
	{{$ASCINI("control")},	&g_ctlsock,  ASC$K_SZ,		OPTS$K_STR},
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
		if ( !strncasecmp(a_target, g_serials[i].devname, T2R$K_TTY_DEVNAME) )
			return	*a_serial = &g_serials[i], STS$K_SUCCESS;
		}

	return	$PUTMSG_FAO(T2R$__TGTNDEF, a_target);
}




/*
 *  DESCRIPTION: Process the <serials> section: mandatory <device>, <chars> ("baud, databits,
 *	parity, stopbits", range checked); optional <rs485>, <iotmo> (the answer timeout, msec,
 *	a non-positive value is ignored), <lategd> (the guard window after a failed exchange, msec,
 *	0..T2R$K_LATEGD_MAX, default <iotmo> / 4), <ts_enabled>, <ts_fncode>, <ts_base_reg0>, <ts_unit>.
 *	Compute the octet time and t3.5, init the device lock, fill the global table of serial
 *	devices (at most T2R$K_MAX_SERIALS records). An ill-formed record is rejected with a
 *	diagnostic, processing is continued from the next one.
 *
 *  INPUTS:
 *	a_cfg:		A context of LIBCONFIG for settings file
 *
 *  OUTPUTS:
 *	NONE
 *
 *  IMPLICITE OUTPUTS:
 *	g_serials, g_serials_nr
 *
 *  RETURNS:
 *	STS$K_SUCCESS	- at least one device has been accepted
 *	T2R$__CFGNOSECT	- no <serials> section
 *	T2R$__NOSERIALS	- not a single valid record
 *
 */
static int	s_settings_process_serials (config_t	*a_cfg)
{
config_setting_t *l_setting, *l_args;
const char *l_str;
int	l_count, l_int = -1, l_speed, l_databits, l_stopbits;
char	l_parity = 0;
T2R$_SERIAL	*l_serial;
char	l_chars[64];

	if ( !(l_setting = config_lookup(a_cfg, "serials")) )
		return	$PUTMSG_FAO(T2R$__CFGNOSECT, "serials", config_error_text(a_cfg));

	l_serial = g_serials;

	l_count = config_setting_length(l_setting);

	if ( l_count > T2R$K_MAX_SERIALS )					/* Never run out of the g_serials[] capacity */
		{
		$PUTMSG_FAO(T2R$__CFGTRUNC, T2R$K_MAX_SERIALS, l_count, "serials");
		l_count = T2R$K_MAX_SERIALS;
		}


	for (int i = 0; i < l_count; i++)
		{
		memset(l_serial, 0, sizeof(T2R$_SERIAL) );

		if ( !(l_args = config_setting_get_elem(l_setting, i)) )
			{
			$PUTMSG_FAO(T2R$__SERBADREC, i, "illformed or missing");
			continue;
			}

		if ( !config_setting_lookup_string(l_args, "device", &l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADREC, i, "no <device> option");
			continue;
			}

		strncpy(l_serial->devname, l_str, T2R$K_TTY_DEVNAME);		/* The tail is zeroed by the memset above */

		/*
		 * config_setting_lookup_int() reports a PRESENCE of the key, so the value must be
		 * checked separately - otherwise "rs485 = 0;" would enable the RS485 support.
		 */
		if ( config_setting_lookup_int(l_args, "rs485", &l_int) && l_int )
			{
#ifdef HAVE_TIOCRS485
			l_serial->flags |=  T2R$M_SERIAL_RS485;
#else
			$PUTMSG_FAO(T2R$__SERNORS485, i, l_serial->devname);
#endif
			}

		if ( !config_setting_lookup_string(l_args, "chars", &l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADOPT, i, l_serial->devname, "no <chars> option");
			continue;
			}

		/*
		 * The string is collapsed in a local copy: libconfig owns the original one and we
		 * must not modify it in place.
		 */
		if ( sizeof(l_chars) <= (size_t) snprintf(l_chars, sizeof(l_chars), "%s", l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADOPT, i, l_serial->devname, "<chars> value is too long");
			continue;
			}

		__util$collapse(l_chars, strlen(l_chars));			/* "115200, 8, N, 1" -> "115200,8,N,1" */

		if ( 4 != sscanf(l_chars, "%d,%d,%c,%d", &l_speed, &l_databits, &l_parity, &l_stopbits) )
			{
			$PUTMSG_FAO(T2R$__SERBADCHR, i, l_serial->devname, l_chars);
			continue;
			}

		/*
		 * Validate the line parameters BEFORE any arithmetic: <l_speed> is a divisor below.
		 */
		if ( !$ISINRANGE(l_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "speed (baud)", l_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX);
			continue;
			}

		if ( !$ISINRANGE(l_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "data bits", l_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX);
			continue;
			}

		if ( !$ISINRANGE(l_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "stop bits", l_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX);
			continue;
			}

		switch (l_parity = toupper (l_parity))
			{
			case	'N':
			case	'E':
			case	'O':
				break;

			default:
				$PUTMSG_FAO(T2R$__SERBADPAR, i, l_serial->devname, 1, &l_parity);
				continue;
			}

		/*
		 * Form context for serial device
		 */
		l_serial->fd = -1;
		l_serial->baud = l_speed;
		l_serial->parity = l_parity;
		l_serial->stopbits = l_stopbits;
		l_serial->databits = l_databits;

		l_serial->anstmo_msec = T2R$K_ANSTMO_MSEC;			/* A default which can be overriden below */

		if ( config_setting_lookup_int(l_args, "iotmo", &l_int) && (0 < l_int) )
			l_serial->anstmo_msec = l_int;

		l_serial->lategd_msec = l_serial->anstmo_msec / 4;		/* The guard window after a failed exchange */

		if ( config_setting_lookup_int(l_args, "lategd", &l_int) )
			{
			if ( (l_int < 0) || (l_int > T2R$K_LATEGD_MAX) )
				{
				$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "lategd (msec)", l_int, 0, T2R$K_LATEGD_MAX);
				continue;
				}

			l_serial->lategd_msec = l_int;
			}

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


		if ( config_setting_lookup_int(l_args, "ts_enabled", &l_int) && l_int )
			{
			l_serial->flags |=  T2R$M_SERIAL_ADDTS;

			if ( config_setting_lookup_int(l_args, "ts_fncode", &l_int) )
				l_serial->ts_fncode =  l_int;
			else	l_serial->ts_fncode = T2R$K_TS_FNCODE;

			if ( config_setting_lookup_int(l_args, "ts_base_reg0", &l_int) )
				l_serial->ts_base_reg0 =  l_int;
			else	l_serial->ts_base_reg0 = T2R$K_TS_BASE_REG0;

			if ( config_setting_lookup_int(l_args, "ts_unit", &l_int) )
				l_serial->ts_unit_nr =  l_int;
			else	l_serial->ts_unit_nr = T2R$K_TS_UNIT;
			}

		if ( (l_int = pthread_mutex_init(&l_serial->lock, NULL)) )
			{
			$PUTMSG_FAO(T2R$__DEVSYSERR, l_serial->devname, "pthread_mutex_init()", l_int, errno);
			continue;
			}

		$PUTMSG_FAO(T2R$__DEVADDED, i, l_serial->devname, l_serial->baud, l_serial->databits, 1, &l_parity,
			l_serial->stopbits, l_serial->anstmo_msec, l_serial->inter_pdu_usec);

		if ( l_serial->flags &  T2R$M_SERIAL_ADDTS )
			$PUTMSG_FAO(T2R$__TSENABLED, l_serial->devname, l_serial->ts_unit_nr, l_serial->ts_fncode, l_serial->ts_base_reg0);
		else	$PUTMSG_FAO(T2R$__TSDISABLED, l_serial->devname);


		l_serial++;
		g_serials_nr++;
		}

	return	g_serials_nr ? STS$K_SUCCESS : $PUTMSG_FAO(T2R$__NOSERIALS);
}




/*
 *  DESCRIPTION: Process the <listeners> section: mandatory <bind> ("TCP:a.b.c.d:port"; UDP is
 *	rejected, the port and the address are validated) and <target> (a defined serial device);
 *	optional <connlm> (the simultaneous sessions limit, T2R$K_CONNLM_MIN..MAX, default
 *	T2R$K_CONNLM_DEF), <idletmo> (the idle session timeout, seconds, T2R$K_IDLETMO_MIN..MAX,
 *	default T2R$K_IDLETMO_DEF) and <iotmo> (is stored, not used yet); <mode> ("MODBUS" - the
 *	default, "RAW" - a transparent octet stream) and, for RAW only, <takeover> (0/1, default 1 -
 *	a new client kicks the current one). A RAW listener: <connlm> is 1 (no other value is
 *	accepted), <idletmo> defaults to T2R$K_IDLETMO_MAX, its target serial line must not be used
 *	by any other listener (LSNRAWSHR, whatever the order of the records). Fill the global table of
 *	listeners (at most T2R$K_MAX_LISTENERS records). An ill-formed or out of range record is
 *	rejected with a diagnostic, processing is continued from the next one.
 *
 *  INPUTS:
 *	a_cfg:		A context of LIBCONFIG for settings file
 *
 *  OUTPUTS:
 *	NONE
 *
 *  IMPLICITE INPUTS:
 *	g_serials, g_serials_nr (see s_settings_find_n_get_serial())
 *
 *  IMPLICITE OUTPUTS:
 *	g_listeners, g_listeners_nr
 *
 *  RETURNS:
 *	STS$K_SUCCESS	- at least one listener has been accepted
 *	T2R$__CFGNOSECT	- no <listeners> section
 *	T2R$__NOLSNRS	- not a single valid record
 *
 */
static int	s_settings_process_listeners (config_t	*a_cfg)
{
config_setting_t *l_setting, *l_args;
const char *l_str;
int	l_count, l_int = -1, l_port_nr;
T2R$_LISTENER	*l_listener;
KWDENT	*l_kwd;
char	l_bind[128], l_proto[16], l_laddr[32], l_port[8];

	if ( !(l_setting = config_lookup(a_cfg, "listeners")) )
		return	$PUTMSG_FAO(T2R$__CFGNOSECT, "listeners", config_error_text(a_cfg));

	l_listener = g_listeners;

	l_count = config_setting_length(l_setting);

	if ( l_count > T2R$K_MAX_LISTENERS )					/* Never run out of the g_listeners[] capacity */
		{
		$PUTMSG_FAO(T2R$__CFGTRUNC, T2R$K_MAX_LISTENERS, l_count, "listeners");
		l_count = T2R$K_MAX_LISTENERS;
		}


	for (int i = 0; i < l_count; i++)
		{
		memset(l_listener, 0, sizeof(T2R$_LISTENER));			/* Never reuse a leftover of a rejected record */

		if ( !(l_args = config_setting_get_elem(l_setting, i)) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "illformed or missing");
			continue;
			}

		if ( !config_setting_lookup_string(l_args, "bind", &l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "no <bind> option");
			continue;
			}

		/*
		 * The string is collapsed in a local copy: libconfig owns the original one and we
		 * must not modify it in place.
		 */
		if ( sizeof(l_bind) <= (size_t) snprintf(l_bind, sizeof(l_bind), "%s", l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "<bind> value is too long");
			continue;
			}

		__util$collapse(l_bind, strlen(l_bind));			/* "TCP: 0.0.0.0: 502" -> "TCP:0.0.0.0:502" */

		if ( 3 != sscanf(l_bind, "%15[TCPUDPtcpudp]:%31[0-9.]:%7[0-9]", l_proto, l_laddr, l_port) )
			{
			$PUTMSG_FAO(T2R$__LSNBADBIND, i, l_bind);
			continue;
			}

		switch ( toupper (*l_proto) )
			{
			case	'T':
				l_listener->proto = IPPROTO_TCP;
				break;

			case	'U':						/* listen()/accept() on SOCK_DGRAM does not work */
				$PUTMSG_FAO(T2R$__LSNBADREC, i, "UDP transport is not implemented yet");
				continue;

			default:
				$PUTMSG_FAO(T2R$__LSNBADBIND, i, l_bind);
				continue;
			}

		if ( !$ISINRANGE((l_port_nr = atoi(l_port)), T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
			{
			$PUTMSG_FAO(T2R$__LSNRANGE, i, l_port_nr, T2R$K_PORT_MIN, T2R$K_PORT_MAX);
			continue;
			}

		l_listener->sk.sin_family = AF_INET;
		l_listener->sk.sin_port = htons ((uint16_t) l_port_nr);

		if ( 1 != (l_int = inet_pton(AF_INET, l_laddr, &l_listener->sk.sin_addr)) )
			{
			$PUTMSG_FAO(T2R$__LSNBADADDR, i, l_laddr, l_int, errno);
			continue;
			}

		if ( config_setting_lookup_int(l_args, "iotmo", &l_int) )
			l_listener->iotmo = l_int;

		l_listener->mode = T2R$K_MODE_MODBUS;					/* <mode> is optional */

		if ( config_setting_lookup_string(l_args, "mode", &l_str) )
			{
			if ( !(1 & __util$lookup_key(l_str, (int) strlen(l_str), s_mode_tbl, $ARRSZ(s_mode_tbl), &l_kwd, UTIL$K_LOOKUP_NCASE)) )
				{
				$PUTMSG_FAO(T2R$__LSNBADMODE, i, l_str);
				continue;
				}

			l_listener->mode = l_kwd->val;
			}

		/*
		 * A RAW listener relays a stream of a single client: one session at a time, and the client
		 * may stay silent for hours (a terminal, a rarely polling SCADA) - a dead peer is detected
		 * by the TCP keepalive, not by the idle timeout.
		 */
		if ( l_listener->mode == T2R$K_MODE_RAW )
			{
			l_listener->connlm = 1;
			l_listener->idletmo = T2R$K_IDLETMO_MAX;
			l_listener->takeover = 1;
			}
		else	{
			l_listener->connlm = T2R$K_CONNLM_DEF;
			l_listener->idletmo = T2R$K_IDLETMO_DEF;
			}

		if ( config_setting_lookup_int(l_args, "takeover", &l_int) )
			{
			if ( l_listener->mode != T2R$K_MODE_RAW )
				{
				$PUTMSG_FAO(T2R$__LSNRAWOPT, i, "takeover", "MODBUS");
				continue;
				}

			if ( !$ISINRANGE(l_int, 0, 1) )
				{
				$PUTMSG_FAO(T2R$__LSNOPTRNG, i, "takeover", l_int, 0, 1);
				continue;
				}

			l_listener->takeover = l_int;
			}

		if ( config_setting_lookup_int(l_args, "connlm", &l_int) )
			{
			if ( (l_listener->mode == T2R$K_MODE_RAW) && (l_int != 1) )
				{
				$PUTMSG_FAO(T2R$__LSNOPTRNG, i, "connlm", l_int, 1, 1);
				continue;
				}

			if ( !$ISINRANGE(l_int, T2R$K_CONNLM_MIN, T2R$K_CONNLM_MAX) )
				{
				$PUTMSG_FAO(T2R$__LSNOPTRNG, i, "connlm", l_int, T2R$K_CONNLM_MIN, T2R$K_CONNLM_MAX);
				continue;
				}

			l_listener->connlm = l_int;
			}

		if ( config_setting_lookup_int(l_args, "idletmo", &l_int) )
			{
			if ( !$ISINRANGE(l_int, T2R$K_IDLETMO_MIN, T2R$K_IDLETMO_MAX) )
				{
				$PUTMSG_FAO(T2R$__LSNOPTRNG, i, "idletmo", l_int, T2R$K_IDLETMO_MIN, T2R$K_IDLETMO_MAX);
				continue;
				}

			l_listener->idletmo = l_int;
			}

		if ( !config_setting_lookup_string(l_args, "target", &l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "no <target> device");
			continue;
			}

		if ( !(1 & s_settings_find_n_get_serial (l_str, &l_listener->serial)) )
			{
			$PUTMSG_FAO(T2R$__LSNNOTGT, i, l_str);
			continue;
			}

		/*
		 * A raw stream and the MODBUS request/answer exchanges (or two raw streams) on the same line
		 * would corrupt each other: a serial line of a RAW listener belongs to it alone. Both orders of
		 * the records are caught here, MODBUS listeners are still free to share a line.
		 */
		l_int = -1;

		for (int j = 0; j < g_listeners_nr; j++)
			{
			if ( (g_listeners[j].serial == l_listener->serial)
				&& ((g_listeners[j].mode == T2R$K_MODE_RAW) || (l_listener->mode == T2R$K_MODE_RAW)) )
				{
				l_int = j;
				break;
				}
			}

		if ( 0 <= l_int )
			{
			$PUTMSG_FAO(T2R$__LSNRAWSHR, i, l_str, ntohs(g_listeners[l_int].sk.sin_port));
			continue;
			}

		strncpy(l_listener->target, l_str, T2R$K_TTY_DEVNAME);		/* The tail is zeroed by the memset above */

		if ( (l_int = pthread_mutex_init(&l_listener->rawlock, NULL)) )
			{
			$PUTMSG_FAO(T2R$__SYSERR, "pthread_mutex_init(rawlock)", l_int, errno);
			continue;
			}

		$PUTMSG_FAO(T2R$__LSNADDED, i, l_listener->serial->devname, (l_listener->proto == IPPROTO_UDP) ? "UDP": "TCP",
			&l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port), l_listener->connlm, l_listener->idletmo,
			(l_listener->mode == T2R$K_MODE_RAW) ? (l_listener->takeover ? "RAW (takeover)" : "RAW") : "MODBUS");


		l_listener++;
		g_listeners_nr++;
		}


	return	g_listeners_nr ? STS$K_SUCCESS : $PUTMSG_FAO(T2R$__NOLSNRS);
}




/*
 *   DESCRIPTION: Load the settings file by the LIBCONFIG API and process its sections: the
 *	<serials> table first (the listeners refer to the serial devices), then the <listeners>
 *	table. A failure of any stage aborts the whole processing.
 *
 *   INPUTS:
 *	a_settings_conf: A file specification of the settings file
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE OUTPUTS:
 *	g_cfg, g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	condition code
 */
static int	s_settings_load_n_parse (
		const char	*a_settings_conf
		)
{
int	l_rc;

	config_init(&g_cfg);
#ifdef	CONFIG_OPTION_IGNORECASE
	config_set_options (&g_cfg, CONFIG_OPTION_IGNORECASE);
#endif

	/* Read the file; on an error report it (CFGPARSE), release the context and return */
	if( !config_read_file(&g_cfg, a_settings_conf))
		{
		$PUTMSG_FAO(T2R$__CFGPARSE, config_error_file(&g_cfg), config_error_line(&g_cfg), config_error_text(&g_cfg), errno);
		config_destroy(&g_cfg);
		return	STS$K_ERROR;
		}

	if ( !(1 & (l_rc = s_settings_process_serials (&g_cfg))) )
		return	l_rc;

	return	s_settings_process_listeners (&g_cfg);
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
 *	g_cfg, g_serials, g_serials_nr, g_listeners, g_listeners_nr
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
 *	touched by the options parser.
 *
 *   INPUTS:
 *	argc, argv:	The command line
 *
 *   IMPLICIT OUTPUTS:
 *	s_argv
 *
 *   RETURNS:
 *	NONE (no copy - no RESTART, s_argv stays NULL)
 */
static void	s_save_argv (
		int	argc,
		char	**argv
		)
{
	if ( !(s_argv = calloc((size_t) argc + 1, sizeof(char *))) )
		return;

	for (int i = 0; i < argc; i++)
		{
		if ( !(s_argv[i] = strdup(argv[i])) )
			{
			free(s_argv);
			s_argv = NULL;
			return;
			}
		}
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

	if ( g_check )								/* /CHECK: that is all */
		return	$PUTMSG_FAO(T2R$__CHECKOK, $ASCPTR(&g_settings), g_serials_nr, g_listeners_nr), EXIT_SUCCESS;


	s_init_sig_handler ();

	if ( (1 & (l_rc = t2r$net_start_listeners ())) )
		{
		t2r$ctl_start ($ASCPTR(&g_ctlsock));				/* A failure is not fatal: no control only */

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
