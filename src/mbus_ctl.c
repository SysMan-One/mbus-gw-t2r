#define	__MODULE__	"MBUS-CTL"
#define	__IDENT__	"V.01-01"
#define	__REV__		"01.01.00"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS - the monitoring and control utility
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: mbus-ctl talks to a running gateway over its local control socket (see T2R-CTL,
**	src/t2r_ctl.c): it sends one text request line, reads the answer lines and shows them as tables.
**	The command line is in the DCL style of the StarLet CLI routines: the keywords may be shortened,
**	the qualifiers start with "/" (or "--"):
**
**	    $ mbus-ctl SHOW CONNECTIONS [/CONTINUOUS[=<sec>]]	sho conn /cont
**	    $ mbus-ctl SHOW LINES | SERIALS			sho lin
**	    $ mbus-ctl SHOW GATEWAY | GW			sho gw
**	    $ mbus-ctl RESET CONNECTIONS <id | a.b.c.d[:port]> | /ALL	reset conn 7
**	    $ mbus-ctl RESTART GATEWAY | GW [/WAIT=<sec> | /NOWAIT]	restart gw
**	    $ mbus-ctl HELP
**
**	    every command takes /SOCKET=<path> (default /var/run/mbusgw-t2r.sock, the /CONTROL= of
**	    the gateway).
**
**	The tables go to stdout as the data (printf), the diagnostics - by $PUTMSG_FAO with the codes of
**	the T2R message catalogue, the same one the gateway uses.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  06-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	06-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - Initial revision.
**
**--
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<string.h>
#include	<strings.h>
#include	<ctype.h>
#include	<errno.h>
#include	<unistd.h>
#include	<signal.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/un.h>


#define		__FAC__	"MBUS-CTL"
#define		__TFAC__ __FAC__ ": "

#include	"utility_routines.h"
#include	"cli_routines.h"
#include	"t2r_defs.h"							/* T2R$K_CTL_* */
#include	"t2r_msgs.h"


#ifndef	__ARCH__NAME__
#define	__ARCH__NAME__	"VAX"
#endif

int	g_trace = 0;								/* t2r_defs.h refers to it */

static	EMSG_RECORD __t2r_msgs__ [] = {					/* The T2R catalogue, as in T2R-MAIN */
	#define	$DEF_MSG(f, s, c, t)	{ .sts = f##$__##c, .textl = 0, .text = #c ", " t },
	__DEF_MESSAGES__
	#undef	$DEF_MSG
};

static	EMSG_RECORD_DESC __t2r_msgs_desc__  = {
	.link = NULL,
	.facno = T2R,
	.msgnr = $ARRSZ(__t2r_msgs__),
	.msgrec = __t2r_msgs__,
	.facname = "T2R"
};


#define	MBUS$K_ANSTMO_SEC	15						/* Wait for an answer (RESTART checks first)	*/
#define	MBUS$K_CONT_DEF		"2"						/* /CONTINUOUS default, seconds			*/
#define	MBUS$K_WAIT_DEF		"30"						/* RESTART /WAIT default, seconds		*/
#define	MBUS$K_MAXLINES		65536						/* An answer is never longer than this		*/

static volatile sig_atomic_t	s_exit_flag;					/* Ctrl-C: stop /CONTINUOUS			*/


/*
 * An answer of the gateway: the whole text and its lines (split in place)
 */
typedef struct	mbus_ans_t {
	char	*data;
	size_t	len;
	char	**line;								/* The data lines, the final one is not here	*/
	int	nr;
	char	*final;								/* "OK ..." or "ERROR ..."			*/
} MBUS$_ANS;


/*
 * The command tables: SHOW/RESET/RESTART <object>. SERIALS and GW are the synonyms (the same routine,
 * parameters and qualifiers) - "gw" is not an abbreviation of GATEWAY, it is a word of its own.
 */
static	CLI_PQDESC	s_show_quals [] = {
	{ .name = {$ASCINI("SOCKET")},		.type = CLI$K_FILE,	.pn = CLI$K_QUAL, .defval = {$ASCINI(T2R$K_CTL_SOCK)} },
	{ .name = {$ASCINI("CONTINUOUS")},	.type = CLI$K_NUM,	.pn = CLI$K_QUAL, .defval = {$ASCINI(MBUS$K_CONT_DEF)} },
	{ .name = {0} }
};

enum	{ MBUS$K_QUAL_SOCKET = 0, MBUS$K_QUAL_CONT = 1 };			/* s_show_quals[] by the index		*/

static	CLI_PQDESC	s_reset_params [] = {
	{ .name = {$ASCINI("CONNECTION")},	.type = CLI$K_QSTRING,	.pn = CLI$K_P1 },
	{ .name = {0} }
};

static	CLI_PQDESC	s_reset_quals [] = {
	{ .name = {$ASCINI("SOCKET")},		.type = CLI$K_FILE,	.pn = CLI$K_QUAL, .defval = {$ASCINI(T2R$K_CTL_SOCK)} },
	{ .name = {$ASCINI("ALL")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL },
	{ .name = {0} }
};

enum	{ MBUS$K_QUAL_ALL = 1 };						/* s_reset_quals[1]			*/

static	CLI_PQDESC	s_restart_quals [] = {
	{ .name = {$ASCINI("SOCKET")},		.type = CLI$K_FILE,	.pn = CLI$K_QUAL, .defval = {$ASCINI(T2R$K_CTL_SOCK)} },
	{ .name = {$ASCINI("WAIT")},		.type = CLI$K_NUM,	.pn = CLI$K_QUAL, .flag = CLI$M_NEGATABLE,
		.defval = {$ASCINI(MBUS$K_WAIT_DEF)} },
	{ .name = {0} }
};

enum	{ MBUS$K_QUAL_WAIT = 1 };						/* s_restart_quals[1]			*/

static	int	s_act_show_conn (CLI_CTX *a_ctx, void *a_arg);
static	int	s_act_show_lines (CLI_CTX *a_ctx, void *a_arg);
static	int	s_act_show_gw (CLI_CTX *a_ctx, void *a_arg);
static	int	s_act_reset_conn (CLI_CTX *a_ctx, void *a_arg);
static	int	s_act_restart_gw (CLI_CTX *a_ctx, void *a_arg);

static	CLI_VERB	s_show_verbs [] = {
	{ .name = {$ASCINI("CONNECTIONS")},	.quals = s_show_quals,	.act_rtn = s_act_show_conn },
	{ .name = {$ASCINI("LINES")},		.quals = s_show_quals,	.act_rtn = s_act_show_lines },
	{ .name = {$ASCINI("SERIALS")},		.quals = s_show_quals,	.act_rtn = s_act_show_lines },
	{ .name = {$ASCINI("GATEWAY")},		.quals = s_show_quals,	.act_rtn = s_act_show_gw },
	{ .name = {$ASCINI("GW")},		.quals = s_show_quals,	.act_rtn = s_act_show_gw },
	{ .name = {0} }
};

static	CLI_VERB	s_reset_verbs [] = {
	{ .name = {$ASCINI("CONNECTIONS")},	.params = s_reset_params, .quals = s_reset_quals, .act_rtn = s_act_reset_conn },
	{ .name = {0} }
};

static	CLI_VERB	s_restart_verbs [] = {
	{ .name = {$ASCINI("GATEWAY")},		.quals = s_restart_quals, .act_rtn = s_act_restart_gw },
	{ .name = {$ASCINI("GW")},		.quals = s_restart_quals, .act_rtn = s_act_restart_gw },
	{ .name = {0} }
};

static	CLI_VERB	s_verbs [] = {
	{ .name = {$ASCINI("SHOW")},		.next = s_show_verbs },
	{ .name = {$ASCINI("RESET")},		.next = s_reset_verbs },
	{ .name = {$ASCINI("RESTART")},		.next = s_restart_verbs },
	{ .name = {0} }
};


static const char	s_usage[] =
	"\n"
	"  mbus-ctl - the monitoring and control utility of the MODBUS gateway mbusgw-t2r, " __IDENT__ "\n"
	"\n"
	"  Usage:\n"
	"      mbus-ctl SHOW CONNECTIONS [/CONTINUOUS[=<sec>]]   the TCP clients and their counters\n"
	"      mbus-ctl SHOW LINES       [/CONTINUOUS[=<sec>]]   the serial lines (SERIALS is the same)\n"
	"      mbus-ctl SHOW GATEWAY     [/CONTINUOUS[=<sec>]]   the revision, uptime, listeners (GW is the same)\n"
	"      mbus-ctl RESET CONNECTIONS <id | a.b.c.d | a.b.c.d:port>   drop TCP client(s)\n"
	"      mbus-ctl RESET CONNECTIONS /ALL                      drop all TCP clients\n"
	"      mbus-ctl RESTART GATEWAY  [/WAIT=<sec> | /NOWAIT]     check the settings, restart the gateway\n"
	"      mbus-ctl HELP\n"
	"\n"
	"  Every command takes /SOCKET=<path>, default " T2R$K_CTL_SOCK " (/CONTROL= of the gateway).\n"
	"  The words may be shortened: sho conn /cont, sho gw, reset conn 7, restart gw. --socket=<path> is\n"
	"  the same as /SOCKET=<path>. /CONTINUOUS redraws the display every <sec> seconds (default "
	MBUS$K_CONT_DEF ")\n"
	"  until Ctrl-C and waits for the gateway if it is not there.\n"
	"\n"
	"  The same requests by hand: echo \"SHOW CONNECTIONS\" | socat - UNIX-CONNECT:" T2R$K_CTL_SOCK "\n"
	"\n";



/*
 *   DESCRIPTION: Ctrl-C/SIGTERM: stop a repeating display after the current frame.
 *
 *   INPUTS:
 *	a_signo:	The signal
 *
 *   RETURNS:
 *	NONE
 */
static void	s_sig_handler (
		int	a_signo
		)
{
	s_exit_flag = 1;
}



/*
 *   DESCRIPTION: Release an answer.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ans_free (
		MBUS$_ANS	*a_ans
		)
{
	free(a_ans->data);
	free(a_ans->line);
	*a_ans = (MBUS$_ANS) {0};
}



/*
 *   DESCRIPTION: Send one request line to the gateway and read its whole answer: the data lines and the
 *	final "OK"/"ERROR" line.
 *
 *   INPUTS:
 *	a_path:		The control socket
 *	a_req:		The request line, without the new line
 *	a_quiet:	1 - do not signal the "cannot reach" (a repeating display retries)
 *
 *   OUTPUTS:
 *	a_ans:		The answer (s_ans_free() it)
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- "OK"
 *	T2R$__CTLANSERR	- "ERROR <text>" (signalled), the data lines are still there
 *	T2R$__CTLNOGW	- cannot connect, or no answer
 *	T2R$__CTLBADANS	- no final line (signalled)
 */
static int	s_request (
	const char	*a_path,
	const char	*a_req,
		int	a_quiet,
		MBUS$_ANS *a_ans
		)
{
int	l_sd, l_rc, l_nr = 0;
size_t	l_size = 0;
ssize_t	l_got;
char	*l_new, *l_p, *l_nl;
struct sockaddr_un	l_sun = {.sun_family = AF_UNIX};
struct timeval	l_tmo = {.tv_sec = MBUS$K_ANSTMO_SEC};

	*a_ans = (MBUS$_ANS) {0};

	if ( strlen(a_path) >= sizeof(l_sun.sun_path) )
		return	a_quiet ? T2R$__CTLNOGW : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, ENAMETOOLONG);

	strcpy(l_sun.sun_path, a_path);

	if ( 0 > (l_sd = socket(AF_UNIX, SOCK_STREAM, 0)) )
		return	$PUTMSG_FAO(T2R$__CTLNOGW, a_path, errno);

	setsockopt(l_sd, SOL_SOCKET, SO_RCVTIMEO, &l_tmo, sizeof(l_tmo));

	if ( connect(l_sd, (struct sockaddr *) &l_sun, sizeof(l_sun)) )
		{
		l_rc = errno;
		close(l_sd);
		return	a_quiet ? (errno = l_rc, T2R$__CTLNOGW) : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc);	/* errno: for the caller */
		}

	if ( (0 > send(l_sd, a_req, strlen(a_req), MSG_NOSIGNAL)) || (0 > send(l_sd, "\n", 1, MSG_NOSIGNAL)) )
		{
		l_rc = errno;
		close(l_sd);
		return	a_quiet ? (errno = l_rc, T2R$__CTLNOGW) : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc);	/* errno: for the caller */
		}

	/* Read until the gateway closes the connection */
	for ( ;; )
		{
		if ( (l_size - a_ans->len) < 1024 )
			{
			if ( !(l_new = realloc(a_ans->data, l_size + 16384)) )
				{
				close(l_sd);
				return	$PUTMSG_FAO(T2R$__CTLNOGW, a_path, ENOMEM);
				}

			a_ans->data = l_new;
			l_size += 16384;
			}

		if ( 0 > (l_got = recv(l_sd, a_ans->data + a_ans->len, l_size - a_ans->len - 1, 0)) )
			{
			if ( errno == EINTR )
				continue;

			l_rc = errno;
			close(l_sd);
			return	a_quiet ? (errno = l_rc, T2R$__CTLNOGW) : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc);	/* errno: for the caller */
			}

		if ( !l_got )
			break;

		a_ans->len += (size_t) l_got;
		}

	close(l_sd);
	a_ans->data[a_ans->len] = '\0';

	/* Split into the lines, the last non-empty one is the final */
	for (l_p = a_ans->data; *l_p; l_p++)
		l_nr += (*l_p == '\n');

	if ( !(a_ans->line = calloc((size_t) l_nr + 1, sizeof(char *))) )
		return	$PUTMSG_FAO(T2R$__CTLNOGW, a_path, ENOMEM);

	for (l_p = a_ans->data; l_p && *l_p && (a_ans->nr < MBUS$K_MAXLINES); l_p = l_nl)
		{
		if ( (l_nl = strchr(l_p, '\n')) )
			*(l_nl++) = '\0';

		a_ans->line[a_ans->nr++] = l_p;
		}

	if ( !a_ans->nr )
		return	$PUTMSG_FAO(T2R$__CTLBADANS, "(nothing)");

	a_ans->final = a_ans->line[--a_ans->nr];

	if ( !strncmp(a_ans->final, "OK", 2) )
		return	STS$K_SUCCESS;

	if ( !strncmp(a_ans->final, "ERROR", 5) )
		return	$PUTMSG_FAO(T2R$__CTLANSERR, a_ans->final + ((a_ans->final[5] == ' ') ? 6 : 5));

	return	$PUTMSG_FAO(T2R$__CTLBADANS, a_ans->final);
}



/*
 *   DESCRIPTION: Get the value of "key=value" from a data line: "CONN id=3 sd=7 ...".
 *
 *   INPUTS:
 *	a_line:		A data line
 *	a_key:		A key
 *	a_valsz:	The size of the output buffer
 *
 *   OUTPUTS:
 *	a_val:		The value, "-" if there is no such key
 *
 *   RETURNS:
 *	a_val
 */
static const char *	s_kv (
	const char	*a_line,
	const char	*a_key,
		char	*a_val,
		size_t	a_valsz
		)
{
size_t	l_klen = strlen(a_key), l_len;
const char	*l_p;

	for (l_p = strchr(a_line, ' '); l_p; l_p = strchr(l_p + 1, ' '))
		{
		if ( strncmp(l_p + 1, a_key, l_klen) || (l_p[1 + l_klen] != '=') )
			continue;

		l_p += 2 + l_klen;
		l_len = strcspn(l_p, " ");
		l_len = $MIN(l_len, a_valsz - 1);
		memcpy(a_val, l_p, l_len);
		a_val[l_len] = '\0';

		return	a_val;
		}

	snprintf(a_val, a_valsz, "-");

	return	a_val;
}



/*
 *   DESCRIPTION: The number of "key=<number>" from a data line, 0 if there is no such key.
 *
 *   INPUTS:
 *	a_line:		A data line
 *	a_key:		A key
 *
 *   RETURNS:
 *	the value
 */
static long long	s_kvn (
	const char	*a_line,
	const char	*a_key
		)
{
char	l_val[32];

	return	strtoll(s_kv(a_line, a_key, l_val, sizeof(l_val)), NULL, 10);
}



/*
 *   DESCRIPTION: Format a time in the VMS style: 06-NOV-2026 23:50:01.
 *
 *   INPUTS:
 *	a_time:		Seconds since the Epoch
 *
 *   OUTPUTS:
 *	a_buf:		The text, at least 48 octets
 *
 *   RETURNS:
 *	a_buf
 */
static const char *	s_vmstime (
		time_t	a_time,
		char	*a_buf
		)
{
struct tm	l_tm;
static const char	l_mon[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

	localtime_r(&a_time, &l_tm);
	snprintf(a_buf, 48, "%02d-%s-%04d %02d:%02d:%02d", l_tm.tm_mday, l_mon[l_tm.tm_mon], 1900 + l_tm.tm_year,
		l_tm.tm_hour, l_tm.tm_min, l_tm.tm_sec);

	return	a_buf;
}



/*
 *   DESCRIPTION: Format a duration: "00:01:05" or "3 02:00:15" (days first, as the VMS delta time).
 *
 *   INPUTS:
 *	a_secs:		Seconds
 *
 *   OUTPUTS:
 *	a_buf:		The text, at least 48 octets
 *
 *   RETURNS:
 *	a_buf
 */
static const char *	s_delta (
		long long a_secs,
		char	*a_buf
		)
{
	if ( a_secs < 0 )
		a_secs = 0;

	if ( a_secs >= 86400 )
		snprintf(a_buf, 48, "%lld %02lld:%02lld:%02lld", a_secs / 86400, (a_secs % 86400) / 3600, (a_secs % 3600) / 60, a_secs % 60);
	else	snprintf(a_buf, 48, "%02lld:%02lld:%02lld", a_secs / 3600, (a_secs % 3600) / 60, a_secs % 60);

	return	a_buf;
}



/*
 *   DESCRIPTION: Show the answer of SHOW CONNECTIONS: per listener, its clients with the counters.
 *
 *   INPUTS:
 *	a_ans:		The answer: LISTENER and CONN lines
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_conn (
		MBUS$_ANS	*a_ans
		)
{
char	l_lsn[16], l_v[8][128], l_t[48], l_d[48];
int	l_nconn, l_total = 0, l_raw;
time_t	l_now = time(NULL);

	printf("Connections of the MODBUS gateway at %s\n", s_vmstime(l_now, l_t));

	for (int i = 0; i < a_ans->nr; i++)
		{
		if ( strncmp(a_ans->line[i], "LISTENER ", 9) )
			continue;

		s_kv(a_ans->line[i], "lsn", l_lsn, sizeof(l_lsn));
		l_raw = !strcmp(s_kv(a_ans->line[i], "mode", l_v[1], sizeof(l_v[1])), "RAW");

		printf("\nListener #%s  %s  %s -> %s  %s, %lld of %lld connection(s), accepted %lld, rejected %lld\n",
			l_lsn, s_kv(a_ans->line[i], "bind", l_v[0], sizeof(l_v[0])), l_v[1],
			s_kv(a_ans->line[i], "target", l_v[2], sizeof(l_v[2])), s_kv(a_ans->line[i], "state", l_v[3], sizeof(l_v[3])),
			s_kvn(a_ans->line[i], "sessions"), s_kvn(a_ans->line[i], "connlm"),
			s_kvn(a_ans->line[i], "accepted"), s_kvn(a_ans->line[i], "rejected"));

		l_nconn = 0;

		for (int j = a_ans->nr - 1; j >= 0; j--)				/* The gateway gives the newest first */
			{
			if ( strncmp(a_ans->line[j], "CONN ", 5) || strcmp(s_kv(a_ans->line[j], "lsn", l_v[4], sizeof(l_v[4])), l_lsn) )
				continue;

			if ( !l_nconn++ )
				printf("  %6s %4s  %-21s  %-20s  %-10s  %8s  %8s  %6s  %10s  %10s\n",
					"Id", "Sd", "Client", "Connected", "Duration", "Requests", "Answers", "Errors",
					l_raw ? "To line" : "Bytes in", l_raw ? "From line" : "Bytes out");

			if ( l_raw )
				snprintf(l_v[5], sizeof(l_v[5]), "%8s  %8s  %6s", "-", "-", "-");
			else	snprintf(l_v[5], sizeof(l_v[5]), "%8lld  %8lld  %6lld", s_kvn(a_ans->line[j], "req"),
					s_kvn(a_ans->line[j], "ans"), s_kvn(a_ans->line[j], "err"));

			printf("  %6lld %4lld  %-21s  %-20s  %-10s  %s  %10lld  %10lld%s\n",
				s_kvn(a_ans->line[j], "id"), s_kvn(a_ans->line[j], "sd"),
				s_kv(a_ans->line[j], "peer", l_v[6], sizeof(l_v[6])),
				s_vmstime((time_t) s_kvn(a_ans->line[j], "since"), l_t),
				s_delta(l_now - s_kvn(a_ans->line[j], "since"), l_d), l_v[5],
				s_kvn(a_ans->line[j], "rx"), s_kvn(a_ans->line[j], "tx"),
				s_kvn(a_ans->line[j], "reset") ? "  (reset)" : "");
			}

		if ( !l_nconn )
			printf("  no connections\n");

		l_total += l_nconn;
		}

	printf("\nTotal: %d connection(s)\n", l_total);
}



/*
 *   DESCRIPTION: Show the answer of SHOW LINES.
 *
 *   INPUTS:
 *	a_ans:		The answer: LINE lines
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_lines (
		MBUS$_ANS	*a_ans
		)
{
char	l_v[4][128], l_t[48];

	printf("Serial lines of the MODBUS gateway at %s\n\n", s_vmstime(time(NULL), l_t));
	printf("  %3s  %-24s  %-16s  %6s  %6s  %5s  %-6s  %9s  %11s\n",
		"#", "Device", "Chars", "Iotmo", "Lategd", "RS485", "State", "Listeners", "Connections");

	for (int i = 0; i < a_ans->nr; i++)
		{
		if ( strncmp(a_ans->line[i], "LINE ", 5) )
			continue;

		printf("  %3lld  %-24s  %-16s  %6lld  %6lld  %5s  %-6s  %9lld  %11lld\n",
			s_kvn(a_ans->line[i], "line"), s_kv(a_ans->line[i], "dev", l_v[0], sizeof(l_v[0])),
			s_kv(a_ans->line[i], "chars", l_v[1], sizeof(l_v[1])),
			s_kvn(a_ans->line[i], "iotmo"), s_kvn(a_ans->line[i], "lategd"),
			s_kvn(a_ans->line[i], "rs485") ? "yes" : "no", s_kv(a_ans->line[i], "state", l_v[2], sizeof(l_v[2])),
			s_kvn(a_ans->line[i], "listeners"), s_kvn(a_ans->line[i], "sessions"));
		}
}



/*
 *   DESCRIPTION: Show the answer of SHOW GATEWAY.
 *
 *   INPUTS:
 *	a_ans:		The answer: GATEWAY and LISTENER lines
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_gw (
		MBUS$_ANS	*a_ans
		)
{
char	l_v[8][256], l_t[48], l_d[48];
const char	*l_line;

	for (int i = 0; i < a_ans->nr; i++)
		{
		l_line = a_ans->line[i];

		if ( !strncmp(l_line, "GATEWAY ", 8) )
			{
			printf("MODBUS gateway mbusgw-t2r %s (REV %s, %s), pid %lld\n",
				s_kv(l_line, "ident", l_v[0], sizeof(l_v[0])), s_kv(l_line, "rev", l_v[1], sizeof(l_v[1])),
				s_kv(l_line, "arch", l_v[2], sizeof(l_v[2])), s_kvn(l_line, "pid"));
			printf("  Started:       %s, up %s\n", s_vmstime((time_t) s_kvn(l_line, "started"), l_t),
				s_delta(s_kvn(l_line, "now") - s_kvn(l_line, "started"), l_d));
			printf("  Settings:      %s\n", s_kv(l_line, "settings", l_v[3], sizeof(l_v[3])));
			printf("  Control:       %s\n", s_kv(l_line, "control", l_v[4], sizeof(l_v[4])));
			printf("  Serial lines:  %lld, listeners: %lld (%lld up), connections: %lld\n\n",
				s_kvn(l_line, "lines"), s_kvn(l_line, "listeners"), s_kvn(l_line, "up"), s_kvn(l_line, "connections"));
			printf("  %3s  %-21s  %-6s  %-24s  %-5s  %11s  %8s  %8s  %8s\n",
				"#", "Bind", "Mode", "Target", "State", "Connections", "Idle tmo", "Accepted", "Rejected");
			}
		else if ( !strncmp(l_line, "LISTENER ", 9) )
			{
			snprintf(l_v[5], sizeof(l_v[5]), "%lld/%lld", s_kvn(l_line, "sessions"), s_kvn(l_line, "connlm"));

			printf("  %3lld  %-21s  %-6s  %-24s  %-5s  %11s  %8lld  %8lld  %8lld\n",
				s_kvn(l_line, "lsn"), s_kv(l_line, "bind", l_v[0], sizeof(l_v[0])),
				s_kv(l_line, "mode", l_v[1], sizeof(l_v[1])), s_kv(l_line, "target", l_v[2], sizeof(l_v[2])),
				s_kv(l_line, "state", l_v[3], sizeof(l_v[3])), l_v[5], s_kvn(l_line, "idletmo"),
				s_kvn(l_line, "accepted"), s_kvn(l_line, "rejected"));
			}
		}
}



/*
 *   DESCRIPTION: Get /SOCKET=<path> of the command, the default if it is not given.
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_qual:		The /SOCKET descriptor of the verb
 *
 *   OUTPUTS:
 *	a_path:		The path, ASCIZ
 *
 *   RETURNS:
 *	a_path
 */
static const char *	s_get_socket (
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_qual,
		char		*a_path
		)
{
ASC	l_val = {0};

	if ( !(1 & __cli$get_value(a_ctx, a_qual, &l_val)) || !$ASCLEN(&l_val) )
		l_val = a_qual->defval;

	snprintf(a_path, ASC$K_SZ + 1, "%.*s", $ASC(&l_val));

	return	a_path;
}



/*
 *   DESCRIPTION: Get the number of a NUM qualifier: its value, or the default when it is given bare
 *	(/CONTINUOUS) or absent.
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_qual:		The qualifier descriptor
 *
 *   RETURNS:
 *	the number
 */
static int	s_get_num (
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_qual
		)
{
ASC	l_val = {0};
char	l_buf[ASC$K_SZ + 1];

	if ( !(1 & __cli$get_value(a_ctx, a_qual, &l_val)) || !$ASCLEN(&l_val) )
		l_val = a_qual->defval;

	snprintf(l_buf, sizeof(l_buf), "%.*s", $ASC(&l_val));

	return	(int) strtol(l_buf, NULL, 0);
}



/*
 *   DESCRIPTION: The common SHOW: one request, one display; with /CONTINUOUS - again and again every
 *	<sec> seconds until Ctrl-C, waiting for the gateway while it is not there (a restart).
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_req:		The request line
 *	a_disp:		The display routine
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or the status of the request (a single shot)
 */
static int	s_show (
		CLI_CTX		*a_ctx,
	const char		*a_req,
		void		(*a_disp) (MBUS$_ANS *)
		)
{
int	l_rc, l_secs, l_tty = isatty(STDOUT_FILENO), l_lost = 0;
unsigned	l_state = 0;
char	l_path[ASC$K_SZ + 1], l_t[48];
MBUS$_ANS	l_ans;
struct timespec	l_ts;

	s_get_socket(a_ctx, &s_show_quals[MBUS$K_QUAL_SOCKET], l_path);

	if ( !(1 & __cli$present(a_ctx, &s_show_quals[MBUS$K_QUAL_CONT], &l_state)) )
		{
		if ( (1 & (l_rc = s_request(l_path, a_req, 0, &l_ans))) )
			a_disp(&l_ans);

		s_ans_free(&l_ans);

		return	l_rc;
		}

	if ( 0 >= (l_secs = s_get_num(a_ctx, &s_show_quals[MBUS$K_QUAL_CONT])) )
		l_secs = 1;

	while ( !s_exit_flag )
		{
		l_rc = s_request(l_path, a_req, 1, &l_ans);

		if ( l_tty )
			fputs("\033[H\033[2J", stdout);				/* Home, clear the screen */

		if ( 1 & l_rc )
			{
			a_disp(&l_ans);
			l_lost = 0;
			}
		else if ( l_rc == (int) T2R$__CTLNOGW )
			{
			if ( !l_lost++ )
				$PUTMSG_FAO(T2R$__CTLNOGW, l_path, errno);

			printf("%s  the gateway is not reachable by %s, waiting ...\n", s_vmstime(time(NULL), l_t), l_path);
			}

		s_ans_free(&l_ans);

		printf("\n(every %d sec, Ctrl-C to stop)\n", l_secs);
		fflush(stdout);

		l_ts.tv_sec = l_secs;
		l_ts.tv_nsec = 0;

		while ( !s_exit_flag && nanosleep(&l_ts, &l_ts) && (errno == EINTR) );
		}

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW CONNECTIONS, SHOW LINES (SERIALS), SHOW GATEWAY (GW) - the CLI action routines.
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	condition code
 */
static int	s_act_show_conn (CLI_CTX *a_ctx, void *a_arg)
{
	return	s_show(a_ctx, "SHOW CONNECTIONS", s_disp_conn);
}

static int	s_act_show_lines (CLI_CTX *a_ctx, void *a_arg)
{
	return	s_show(a_ctx, "SHOW LINES", s_disp_lines);
}

static int	s_act_show_gw (CLI_CTX *a_ctx, void *a_arg)
{
	return	s_show(a_ctx, "SHOW GATEWAY", s_disp_gw);
}



/*
 *   DESCRIPTION: RESET CONNECTIONS <id | a.b.c.d | a.b.c.d:port> | /ALL - drop TCP client(s).
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	condition code
 */
static int	s_act_reset_conn (
		CLI_CTX		*a_ctx,
		void		*a_arg
		)
{
int	l_rc, l_count = 0;
unsigned	l_state = 0;
ASC	l_val = {0};
char	l_path[ASC$K_SZ + 1], l_req[ASC$K_SZ + 64], l_v[2][64];
MBUS$_ANS	l_ans;

	s_get_socket(a_ctx, &s_reset_quals[MBUS$K_QUAL_SOCKET], l_path);

	if ( 1 & __cli$present(a_ctx, &s_reset_quals[MBUS$K_QUAL_ALL], &l_state) )
		snprintf(l_req, sizeof(l_req), "RESET CONNECTIONS ALL");
	else if ( (1 & __cli$get_value(a_ctx, &s_reset_params[0], &l_val)) && $ASCLEN(&l_val) )
		snprintf(l_req, sizeof(l_req), "RESET CONNECTIONS %.*s", $ASC(&l_val));
	else	{
		fputs("Which connection? Give an id or an address (see SHOW CONNECTIONS), or /ALL\n", stdout);
		return	STS$K_ERROR;
		}

	if ( (1 & (l_rc = s_request(l_path, l_req, 0, &l_ans))) )
		{
		for (int i = 0; i < l_ans.nr; i++)
			{
			if ( strncmp(l_ans.line[i], "RESET ", 6) )
				continue;

			printf("  connection %s from %s --- reset\n", s_kv(l_ans.line[i], "id", l_v[0], sizeof(l_v[0])),
				s_kv(l_ans.line[i], "peer", l_v[1], sizeof(l_v[1])));
			l_count++;
			}

		fflush(stdout);							/* The table first, then the message */
		$PUTMSG_FAO(T2R$__CTLRESETN, l_count);
		}

	s_ans_free(&l_ans);

	return	l_rc;
}



/*
 *   DESCRIPTION: RESTART GATEWAY [/WAIT=<sec> | /NOWAIT] - the gateway checks its settings and restarts
 *	itself; mbus-ctl waits until it answers again with a new start time.
 *
 *   INPUTS:
 *	a_ctx:		The parsed command line
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	condition code
 */
static int	s_act_restart_gw (
		CLI_CTX		*a_ctx,
		void		*a_arg
		)
{
int	l_rc, l_wait;
unsigned	l_state = 0;
long long	l_started = 0, l_pid = 0;
char	l_path[ASC$K_SZ + 1];
MBUS$_ANS	l_ans;
time_t	l_t0;

	s_get_socket(a_ctx, &s_restart_quals[MBUS$K_QUAL_SOCKET], l_path);

	/* The start time of the running gateway: a new one tells that it has restarted */
	if ( !(1 & (l_rc = s_request(l_path, "SHOW GATEWAY", 0, &l_ans))) )
		return	s_ans_free(&l_ans), l_rc;

	for (int i = 0; i < l_ans.nr; i++)
		{
		if ( !strncmp(l_ans.line[i], "GATEWAY ", 8) )
			l_started = s_kvn(l_ans.line[i], "started");
		}

	s_ans_free(&l_ans);

	l_rc = s_request(l_path, "RESTART GATEWAY", 0, &l_ans);

	for (int i = 0; i < l_ans.nr; i++)					/* Why the settings are bad, or warnings */
		{
		if ( !strncmp(l_ans.line[i], "CHECK ", 6) )
			printf("  %s\n", l_ans.line[i] + 6);
		}

	fflush(stdout);

	s_ans_free(&l_ans);

	if ( !(1 & l_rc) )
		return	l_rc;

	if ( (1 & __cli$present(a_ctx, &s_restart_quals[MBUS$K_QUAL_WAIT], &l_state)) && (l_state & CLI$M_NEGATED) )
		return	STS$K_SUCCESS;						/* /NOWAIT */

	if ( 0 >= (l_wait = s_get_num(a_ctx, &s_restart_quals[MBUS$K_QUAL_WAIT])) )
		l_wait = atoi(MBUS$K_WAIT_DEF);

	$PUTMSG_FAO(T2R$__CTLWAIT, l_wait);

	for (l_t0 = time(NULL); !s_exit_flag && ((time(NULL) - l_t0) <= l_wait); )
		{
		usleep(500000);

		if ( !(1 & s_request(l_path, "SHOW GATEWAY", 1, &l_ans)) )
			{
			s_ans_free(&l_ans);
			continue;						/* Not yet */
			}

		for (int i = 0; i < l_ans.nr; i++)
			{
			if ( strncmp(l_ans.line[i], "GATEWAY ", 8) || (s_kvn(l_ans.line[i], "started") == l_started) )
				continue;

			l_pid = s_kvn(l_ans.line[i], "pid");
			$PUTMSG_FAO(T2R$__CTLBACK, (int) (time(NULL) - l_t0), l_pid, s_kvn(l_ans.line[i], "up"));
			s_ans_free(&l_ans);

			return	STS$K_SUCCESS;
			}

		s_ans_free(&l_ans);
		}

	return	$PUTMSG_FAO(T2R$__CTLNOBACK, l_wait);
}



/*
 *   DESCRIPTION: The entry point: the help, the parse of the command line, the dispatch to an action
 *	routine.
 *
 *   INPUTS:
 *	argc, argv:	The command line
 *
 *   RETURNS:
 *	EXIT_SUCCESS, EXIT_FAILURE
 */
int	main (
		int	argc,
		char	**argv
		)
{
int	l_rc;
void	*l_ctx = NULL;
struct sigaction	l_sa = {0};

	__util$deflog(NULL, NULL);						/* The diagnostics go to the terminal */
	__util$inimsg(&__t2r_msgs_desc__);

	if ( (argc < 2) || !strcasecmp(argv[1], "HELP") || !strcasecmp(argv[1], "/HELP")
		|| !strcasecmp(argv[1], "--help") || !strcasecmp(argv[1], "-h") )
		{
		fputs(s_usage, stdout);
		return	(argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
		}

	/* No SA_RESTART: Ctrl-C must break a sleep or a wait for an answer at once */
	l_sa.sa_handler = s_sig_handler;
	sigemptyset(&l_sa.sa_mask);

	if ( (l_rc = sigaction(SIGINT, &l_sa, NULL)) || (l_rc = sigaction(SIGTERM, &l_sa, NULL)) )
		$LOG_FAO(STS$K_WARN, "sigaction()->!SL, errno: !SL", l_rc, errno);

	signal(SIGPIPE, SIG_IGN);

	/* argv[1] is the verb: the parser takes the vector past the image name */
	if ( !(1 & (l_rc = __cli$parse(s_verbs, CLI$M_OPSIGNAL, argc - 1, argv + 1, &l_ctx))) )
		return	(l_rc == STS$K_WARN) ? EXIT_SUCCESS : (fputs(s_usage, stdout), EXIT_FAILURE);	/* WARN: a help has been shown */

	l_rc = __cli$dispatch(l_ctx);
	__cli$cleanup(l_ctx);

	return	(1 & l_rc) ? EXIT_SUCCESS : EXIT_FAILURE;
}
