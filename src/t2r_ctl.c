#define	__MODULE__	"T2R-CTL"
#define	__IDENT__	"X.01-01"
#define	__REV__		"01.01.00"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: The control channel of the gateway: a local AF_UNIX stream socket (/CONTROL=<path>,
**	default T2R$K_CTL_SOCK, rights T2R$K_CTL_MODE), served by one thread of its own. Nothing is
**	opened to the network: who may control the gateway is decided by the rights of the socket file.
**
**	The protocol is plain text, one request per connection, so it can be tried by hand:
**
**		$ echo "SHOW CONNECTIONS" | socat - UNIX-CONNECT:/var/run/mbusgw-t2r.sock
**
**	    the client sends ONE line:	<VERB> <OBJECT> [<argument>]		(case-insensitive)
**	    the gateway answers:	zero or more data lines "<TAG> key=value key=value ..."
**					and one final line: "OK [text]" or "ERROR <text>",
**					then closes the connection.
**
**	The requests (see s_ctl_cmds[]):
**		SHOW GATEWAY		- GATEWAY + LISTENER lines
**		SHOW LINES		- LINE lines (the serial lines)
**		SHOW CONNECTIONS	- LISTENER + CONN lines (the TCP clients and their counters)
**		RESET CONNECTIONS <id | a.b.c.d | a.b.c.d:port | ALL> - shutdown() of the client sockets
**		RESTART GATEWAY		- check the settings by a child "/CHECK" run, then re-exec
**		HELP			- HELP lines
**
**	The words are the full ones: the abbreviations (sho conn /cont) are the job of mbus-ctl.
**
**	How it is plugged into the gateway: main() calls t2r$ctl_start() after the listeners are up and
**	t2r$ctl_stop() at the exit; the sessions are seen through the registry of T2R-NET
**	(t2r$net_sess_walk()); RESTART raises <g_restart_flag> and <g_exit_flag>, main() does the rest.
**	The data path (T2R-NET sessions, T2R-TTY) does not know about this module at all.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  06-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	06-NOV-2026	CLD	X.01-01 / REV: 01.01.00 - Initial revision.
**
**--
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<inttypes.h>
#include	<stdarg.h>
#include	<string.h>
#include	<strings.h>
#include	<ctype.h>
#include	<errno.h>
#include	<unistd.h>
#include	<poll.h>
#include	<spawn.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<sys/wait.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"


extern int	g_exit_flag, g_restart_flag, g_trace;
extern time_t	g_start_time;
extern const char g_t2r_ident[], g_t2r_rev[];
extern ASC	g_settings;

extern T2R$_LISTENER	g_listeners[];
extern int		g_listeners_nr;
extern T2R$_SERIAL	g_serials[];
extern int		g_serials_nr;

extern char	**environ;


static	int		s_ctl_sd = -1;						/* The listening AF_UNIX socket		*/
static	pthread_t	s_ctl_tid;						/* The control thread, joined at the stop	*/
static	char		s_ctl_path[sizeof(((struct sockaddr_un *) 0)->sun_path)];
static	int		s_ctl_restart;						/* RESTART is accepted: raise the flags	*/
										/* after the answer has been sent	*/


/*
 * An answer is composed in memory, then sent at once: the registry lock is never held over a write
 * to a slow client.
 */
typedef struct	t2r_ctl_ans_t {
	char	*data;
	size_t	len,
		size;
	int	nomem;								/* realloc() has failed: answer ERROR	*/
} T2R$_CTL_ANS;


/*
 *   DESCRIPTION: Append a formatted text to the answer (snprintf() style: the protocol data, not a
 *	message of the log).
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_fmt, ...:	A format and its arguments
 *
 *   OUTPUTS:
 *	a_ans:		.data/.len/.size; .nomem on a failure
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ctl_put (
		T2R$_CTL_ANS	*a_ans,
	const	char	*a_fmt,
		...
		)
{
va_list	l_ap;
int	l_len;
char	*l_new;

	if ( a_ans->nomem )
		return;

	for (int i = 0; i < 2; i++)
		{
		va_start(l_ap, a_fmt);
		l_len = vsnprintf(a_ans->data ? a_ans->data + a_ans->len : NULL, a_ans->size - a_ans->len, a_fmt, l_ap);
		va_end(l_ap);

		if ( l_len < 0 )
			return;

		if ( (size_t) l_len < (a_ans->size - a_ans->len) )
			{
			a_ans->len += (size_t) l_len;
			return;
			}

		if ( !(l_new = realloc(a_ans->data, a_ans->size + (size_t) l_len + 4096)) )
			{
			a_ans->nomem = 1;
			return;
			}

		a_ans->data = l_new;
		a_ans->size += (size_t) l_len + 4096;
		}
}



/*
 *   DESCRIPTION: Format "a.b.c.d:port" of a socket address.
 *
 *   INPUTS:
 *	a_sk:		A socket address
 *
 *   OUTPUTS:
 *	a_buf:		"a.b.c.d:port", at least 24 octets
 *
 *   RETURNS:
 *	a_buf
 */
static const char *	s_ctl_addr (
	const struct sockaddr_in *a_sk,
		char	*a_buf
		)
{
char	l_ip[INET_ADDRSTRLEN] = "?";

	inet_ntop(AF_INET, &a_sk->sin_addr, l_ip, sizeof(l_ip));
	snprintf(a_buf, 24, "%s:%u", l_ip, ntohs(a_sk->sin_port));

	return	a_buf;
}



/*
 *   DESCRIPTION: Put a LISTENER line per listener.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *
 *   IMPLICIT INPUTS:
 *	g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ctl_put_listeners (
		T2R$_CTL_ANS	*a_ans
		)
{
char	l_bind[24];
T2R$_LISTENER	*l_lsnr;

	for (int i = 0; i < g_listeners_nr; i++)
		{
		l_lsnr = &g_listeners[i];

		s_ctl_put(a_ans, "LISTENER lsn=%d bind=%s mode=%s target=%s state=%s connlm=%d idletmo=%d"
			" sessions=%d accepted=%u rejected=%u\n",
			i, s_ctl_addr(&l_lsnr->sk, l_bind), (l_lsnr->mode == T2R$K_MODE_RAW) ? "RAW" : "MODBUS",
			l_lsnr->target, (0 <= l_lsnr->fd) ? "UP" : "DOWN", l_lsnr->connlm, l_lsnr->idletmo,
			__atomic_load_n(&l_lsnr->sessions, __ATOMIC_ACQUIRE), l_lsnr->accepted, l_lsnr->rejected);
		}
}



/*
 *   DESCRIPTION: A t2r$net_sess_walk() routine of SHOW CONNECTIONS: put a CONN line of a session.
 *
 *   INPUTS:
 *	a_session:	A session (the registry lock is held)
 *	a_arg:		T2R$_CTL_ANS * - the answer buffer
 *
 *   RETURNS:
 *	STS$K_SUCCESS - go on
 */
static int	s_ctl_conn_line (
		T2R$_SESSION	*a_session,
		void		*a_arg
		)
{
char	l_peer[24];

	s_ctl_put((T2R$_CTL_ANS *) a_arg, "CONN id=%" PRIu32 " sd=%d lsn=%d mode=%s peer=%s since=%lld reset=%d"
		" req=%" PRIu64 " ans=%" PRIu64 " err=%" PRIu64 " rx=%" PRIu64 " tx=%" PRIu64 "\n",
		a_session->id, a_session->sd, (int) (a_session->listener - g_listeners),
		(a_session->listener->mode == T2R$K_MODE_RAW) ? "RAW" : "MODBUS",
		s_ctl_addr(&a_session->sk, l_peer), (long long) a_session->start, a_session->reset,
		a_session->nreq, a_session->nans, a_session->nerr, a_session->nrx, a_session->ntx);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW CONNECTIONS - the listeners and the TCP clients with their counters.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_conn (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
	s_ctl_put_listeners(a_ans);
	t2r$net_sess_walk(s_ctl_conn_line, a_ans);
	s_ctl_put(a_ans, "OK\n");

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: A t2r$net_sess_walk() routine of SHOW LINES: count the sessions of every serial line.
 *
 *   INPUTS:
 *	a_session:	A session (the registry lock is held)
 *	a_arg:		int [T2R$K_MAX_SERIALS] - the counters
 *
 *   RETURNS:
 *	STS$K_SUCCESS - go on
 */
static int	s_ctl_line_count (
		T2R$_SESSION	*a_session,
		void		*a_arg
		)
{
int	l_idx = (int) (a_session->target - g_serials);

	if ( $ISINRANGE(l_idx, 0, g_serials_nr - 1) )
		((int *) a_arg)[l_idx]++;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW LINES - the serial lines: settings, state, users.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		Unused
 *
 *   IMPLICIT INPUTS:
 *	g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_lines (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
int	l_sessions[T2R$K_MAX_SERIALS] = {0}, l_lsnrs;
T2R$_SERIAL	*l_serial;
const char	*l_state;

	t2r$net_sess_walk(s_ctl_line_count, l_sessions);

	for (int i = 0; i < g_serials_nr; i++)
		{
		l_serial = &g_serials[i];

		l_lsnrs = 0;

		for (int j = 0; j < g_listeners_nr; j++)
			l_lsnrs += (g_listeners[j].serial == l_serial);

		if ( l_serial->state == T2R$K_STATE_IDLE )
			l_state = "CLOSED";						/* No listener has opened it */
		else	l_state = l_serial->nodev ? "ABSENT" : "OPEN";

		s_ctl_put(a_ans, "LINE line=%d dev=%s chars=%d,%d,%c,%d iotmo=%d lategd=%d rs485=%d ts=%d state=%s"
			" listeners=%d sessions=%d\n",
			i, l_serial->devname, l_serial->baud, l_serial->databits, l_serial->parity, l_serial->stopbits,
			l_serial->anstmo_msec, l_serial->lategd_msec, !!(l_serial->flags & T2R$M_SERIAL_RS485),
			!!(l_serial->flags & T2R$M_SERIAL_ADDTS), l_state, l_lsnrs, l_sessions[i]);
		}

	s_ctl_put(a_ans, "OK\n");

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW GATEWAY - the revision, the process, the uptime and the listeners.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_gw (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
int	l_up = 0;

	for (int i = 0; i < g_listeners_nr; i++)
		l_up += (0 <= g_listeners[i].fd);

	s_ctl_put(a_ans, "GATEWAY ident=%s rev=%s arch=%s pid=%d started=%lld now=%lld settings=%s control=%s"
		" lines=%d listeners=%d up=%d connections=%d\n",
		g_t2r_ident, g_t2r_rev, __ARCH__NAME__, (int) getpid(), (long long) g_start_time, (long long) time(NULL),
		$ASCLEN(&g_settings) ? $ASCPTR(&g_settings) : "-", s_ctl_path, g_serials_nr, g_listeners_nr, l_up,
		t2r$net_sess_count());

	s_ctl_put_listeners(a_ans);
	s_ctl_put(a_ans, "OK\n");

	return	STS$K_SUCCESS;
}



/*
 * RESET CONNECTIONS: what is to be reset
 */
typedef struct	t2r_ctl_sel_t {
	int	all;								/* ALL				*/
	uint32_t id;								/* A session id, 0 - not given	*/
	struct in_addr addr;							/* A client address ...		*/
	int	port;								/* ... and its port, 0 - any	*/
	int	count;								/* Has been reset		*/
	T2R$_CTL_ANS *ans;
} T2R$_CTL_SEL;



/*
 *   DESCRIPTION: A t2r$net_sess_walk() routine of RESET CONNECTIONS: shutdown() the socket of a matching
 *	session. Its thread wakes up and ends the session as on a client's hangup (the MODBUS one after
 *	the exchange in progress, if any); it closes the socket itself, after it has left the registry.
 *
 *   INPUTS:
 *	a_session:	A session (the registry lock is held)
 *	a_arg:		T2R$_CTL_SEL * - the selection
 *
 *   OUTPUTS:
 *	a_session:	.reset = 1
 *
 *   RETURNS:
 *	STS$K_SUCCESS - go on
 */
static int	s_ctl_reset_one (
		T2R$_SESSION	*a_session,
		void		*a_arg
		)
{
T2R$_CTL_SEL	*l_sel = a_arg;
int	l_rc;
char	l_peer[24];

	if ( a_session->reset )							/* Is leaving already */
		return	STS$K_SUCCESS;

	if ( !l_sel->all )
		{
		if ( l_sel->id && (l_sel->id != a_session->id) )
			return	STS$K_SUCCESS;

		if ( !l_sel->id && ((l_sel->addr.s_addr != a_session->sk.sin_addr.s_addr)
			|| (l_sel->port && (l_sel->port != ntohs(a_session->sk.sin_port)))) )
			return	STS$K_SUCCESS;
		}

	a_session->reset = 1;

	$PUTMSG_FAO(T2R$__CTLRESET, a_session->sd, &a_session->sk.sin_addr, ntohs(a_session->sk.sin_port), a_session->id);

	if ( (l_rc = shutdown(a_session->sd, SHUT_RDWR)) )
		$PUTMSG_FAO(T2R$__NETSYSWRN, a_session->sd, "shutdown()", l_rc, errno);

	s_ctl_put(l_sel->ans, "RESET id=%" PRIu32 " peer=%s\n", a_session->id, s_ctl_addr(&a_session->sk, l_peer));
	l_sel->count++;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: RESET CONNECTIONS <id | a.b.c.d | a.b.c.d:port | ALL> - drop the TCP clients.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		The selector, may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_ERROR - no or bad selector, nothing matches
 */
static int	s_ctl_reset_conn (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
T2R$_CTL_SEL	l_sel = {.ans = a_ans};
char	l_ip[INET_ADDRSTRLEN], *l_end;
const char	*l_colon;
unsigned long	l_val;
size_t	l_len;

	if ( !a_arg || !*a_arg )
		return	s_ctl_put(a_ans, "ERROR which connection? Give an id, an address, address:port or ALL\n"), STS$K_ERROR;

	if ( !strcasecmp(a_arg, "ALL") )
		l_sel.all = 1;
	else if ( isdigit((unsigned char) *a_arg) && !strchr(a_arg, '.') )	/* A session id */
		{
		errno = 0;
		l_val = strtoul(a_arg, &l_end, 10);

		if ( errno || *l_end || !l_val || (l_val > UINT32_MAX) )
			return	s_ctl_put(a_ans, "ERROR bad connection id <%s>\n", a_arg), STS$K_ERROR;

		l_sel.id = (uint32_t) l_val;
		}
	else	{								/* a.b.c.d or a.b.c.d:port */
		l_len = (l_colon = strchr(a_arg, ':')) ? (size_t) (l_colon - a_arg) : strlen(a_arg);

		if ( l_len >= sizeof(l_ip) )
			return	s_ctl_put(a_ans, "ERROR bad address <%s>\n", a_arg), STS$K_ERROR;

		memcpy(l_ip, a_arg, l_len);
		l_ip[l_len] = '\0';

		if ( 1 != inet_pton(AF_INET, l_ip, &l_sel.addr) )
			return	s_ctl_put(a_ans, "ERROR bad address <%s>\n", a_arg), STS$K_ERROR;

		if ( l_colon )
			{
			l_val = strtoul(l_colon + 1, &l_end, 10);

			if ( *l_end || !$ISINRANGE(l_val, T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
				return	s_ctl_put(a_ans, "ERROR bad port in <%s>\n", a_arg), STS$K_ERROR;

			l_sel.port = (int) l_val;
			}
		}

	t2r$net_sess_walk(s_ctl_reset_one, &l_sel);

	if ( !l_sel.count )
		return	s_ctl_put(a_ans, "ERROR no connection matches <%s>\n", a_arg), STS$K_ERROR;

	s_ctl_put(a_ans, "OK %d\n", l_sel.count);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Check the settings file by a child run of the same program: "<image> /SETTINGS=<file>
 *	/CHECK" parses the settings exactly as a start does and exits with its status. The live tables
 *	of the gateway are not touched at all. The WARNING/ERROR/FATAL lines of the child are put into
 *	the answer as CHECK lines: the operator sees why.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *
 *   IMPLICIT INPUTS:
 *	g_settings
 *
 *   RETURNS:
 *	STS$K_SUCCESS - the settings are good, a failure status otherwise
 */
static int	s_ctl_check_settings (
		T2R$_CTL_ANS	*a_ans
		)
{
int	l_rc, l_pipe[2], l_status = -1, l_len = 0;
pid_t	l_pid;
char	l_settings[ASC$K_SZ + 16], l_out[8192], *l_line, *l_nl;
char	*l_argv[] = { (char *) "mbusgw-t2r", l_settings, (char *) "/check", NULL };
posix_spawn_file_actions_t	l_fa;
struct pollfd	l_pfd;

	snprintf(l_settings, sizeof(l_settings), "/settings=%.*s", $ASC(&g_settings));

	if ( pipe(l_pipe) )
		return	$PUTMSG_FAO(T2R$__CTLSYSWRN, "pipe()", -1, errno), s_ctl_put(a_ans, "ERROR pipe() failed\n"), STS$K_ERROR;

	/* posix_spawn(), not fork(): the gateway is multithreaded */
	posix_spawn_file_actions_init(&l_fa);
	posix_spawn_file_actions_addclose(&l_fa, l_pipe[0]);
	posix_spawn_file_actions_adddup2(&l_fa, l_pipe[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&l_fa, l_pipe[1], STDERR_FILENO);

	l_rc = posix_spawn(&l_pid, "/proc/self/exe", &l_fa, NULL, l_argv, environ);

	posix_spawn_file_actions_destroy(&l_fa);
	close(l_pipe[1]);

	if ( l_rc )
		{
		close(l_pipe[0]);
		$PUTMSG_FAO(T2R$__CTLSYSWRN, "posix_spawn(/proc/self/exe)", l_rc, l_rc);
		return	s_ctl_put(a_ans, "ERROR cannot run the settings check, error %d\n", l_rc), STS$K_ERROR;
		}

	/* Collect the output until EOF, at most T2R$K_CTL_TMO_MSEC between the portions */
	l_pfd.fd = l_pipe[0];
	l_pfd.events = POLLIN;

	while ( 0 < poll(&l_pfd, 1, T2R$K_CTL_TMO_MSEC) )
		{
		if ( 0 >= (l_rc = read(l_pipe[0], l_out + l_len, sizeof(l_out) - 1 - l_len)) )
			break;

		if ( (l_len += l_rc) >= (int) sizeof(l_out) - 1 )
			break;							/* Enough to tell the reason */
		}

	l_out[l_len] = '\0';
	close(l_pipe[0]);

	if ( 0 > waitpid(l_pid, &l_status, 0) )
		$PUTMSG_FAO(T2R$__CTLSYSWRN, "waitpid()", -1, errno);

	for (l_line = l_out; l_line && *l_line; l_line = l_nl)
		{
		if ( (l_nl = strchr(l_line, '\n')) )
			*(l_nl++) = '\0';

		if ( strstr(l_line, "-W-") || strstr(l_line, "-E-") || strstr(l_line, "-F-") )
			s_ctl_put(a_ans, "CHECK %s\n", l_line);
		}

	if ( WIFEXITED(l_status) && (WEXITSTATUS(l_status) == EXIT_SUCCESS) )
		return	STS$K_SUCCESS;

	$PUTMSG_FAO(T2R$__CTLCHKFAIL, l_status);
	s_ctl_put(a_ans, "ERROR the settings check has failed, the gateway goes on as it is\n");

	return	STS$K_ERROR;
}



/*
 *   DESCRIPTION: RESTART GATEWAY - check the settings, answer, then let main() restart the process
 *	(the same PID: systemd and procd see nothing, the listeners and the lines are opened anew).
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		Unused
 *
 *   IMPLICIT OUTPUTS:
 *	s_ctl_restart
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or the status of the settings check
 */
static int	s_ctl_restart_gw (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
int	l_rc;

	if ( !(1 & (l_rc = s_ctl_check_settings(a_ans))) )
		return	l_rc;

	$PUTMSG_FAO(T2R$__CTLRESTART);

	s_ctl_put(a_ans, "OK restarting pid=%d\n", (int) getpid());
	s_ctl_restart = 1;							/* The flags are raised after the answer */

	return	STS$K_SUCCESS;
}



static int	s_ctl_help (T2R$_CTL_ANS *a_ans, const char *a_arg);

/*
 * The requests: <verb> <object> -> a routine. The words are exact (case-insensitive).
 */
typedef struct	t2r_ctl_cmd_t {
	const char	*verb,
			*object;
	int		(*rtn) (T2R$_CTL_ANS *a_ans, const char *a_arg);
	const char	*help;
} T2R$_CTL_CMD;

static const T2R$_CTL_CMD	s_ctl_cmds [] = {
	{ "SHOW",	"GATEWAY",	s_ctl_show_gw,		"the revision, the uptime, the listeners" },
	{ "SHOW",	"LINES",	s_ctl_show_lines,	"the serial lines" },
	{ "SHOW",	"CONNECTIONS",	s_ctl_show_conn,	"the TCP clients and their counters" },
	{ "RESET",	"CONNECTIONS",	s_ctl_reset_conn,	"<id | a.b.c.d | a.b.c.d:port | ALL> - drop TCP clients" },
	{ "RESTART",	"GATEWAY",	s_ctl_restart_gw,	"check the settings and restart the gateway process" },
	{ "HELP",	NULL,		s_ctl_help,		"this list" },
};



/*
 *   DESCRIPTION: HELP - the list of the requests.
 *
 *   INPUTS:
 *	a_ans:		An answer buffer
 *	a_arg:		Unused
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_help (
		T2R$_CTL_ANS	*a_ans,
	const	char		*a_arg
		)
{
	for (size_t i = 0; i < $ARRSZ(s_ctl_cmds); i++)
		s_ctl_put(a_ans, "HELP %s %s - %s\n", s_ctl_cmds[i].verb, s_ctl_cmds[i].object ? s_ctl_cmds[i].object : "",
			s_ctl_cmds[i].help);

	s_ctl_put(a_ans, "OK\n");

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Serve one client: read one request line, run it, send the answer. The caller closes
 *	the connection.
 *
 *   INPUTS:
 *	a_sd:		A connected client socket
 *
 *   RETURNS:
 *	condition code
 */
static int	s_ctl_serve (
		int	a_sd
		)
{
char	l_line[T2R$K_CTL_LINESZ + 1], *l_words[4] = {0}, *l_save = NULL, *l_nl = NULL;
int	l_rc, l_len = 0, l_nw = 0;
size_t	l_off;
ssize_t	l_sent;
struct pollfd	l_pfd = {.fd = a_sd, .events = POLLIN};
T2R$_CTL_ANS	l_ans = {0};
const T2R$_CTL_CMD	*l_cmd = NULL;

	/* Read up to the end of the line; a client which stays silent is dropped */
	while ( !l_nl && (l_len < T2R$K_CTL_LINESZ) )
		{
		if ( 0 >= (l_rc = poll(&l_pfd, 1, T2R$K_CTL_TMO_MSEC)) )
			return	STS$K_WARN;

		if ( 0 >= (l_rc = recv(a_sd, l_line + l_len, T2R$K_CTL_LINESZ - l_len, 0)) )
			break;							/* EOF: take what we have */

		l_len += l_rc;
		l_line[l_len] = '\0';
		l_nl = strpbrk(l_line, "\r\n");
		}

	l_line[l_len] = '\0';

	if ( l_nl )
		*l_nl = '\0';

	for (char *l_w = strtok_r(l_line, " \t", &l_save); l_w && (l_nw < 4); l_w = strtok_r(NULL, " \t", &l_save))
		l_words[l_nw++] = l_w;

	for (size_t i = 0; l_nw && (i < $ARRSZ(s_ctl_cmds)); i++)
		{
		if ( strcasecmp(l_words[0], s_ctl_cmds[i].verb) )
			continue;

		if ( s_ctl_cmds[i].object && ((l_nw < 2) || strcasecmp(l_words[1], s_ctl_cmds[i].object)) )
			continue;

		l_cmd = &s_ctl_cmds[i];
		break;
		}

	if ( l_cmd )
		{
		$IFTRACE(g_trace, "Control request: %s %s %s", l_words[0], l_words[1] ? l_words[1] : "", l_words[2] ? l_words[2] : "");

		l_cmd->rtn(&l_ans, l_cmd->object ? l_words[2] : NULL);
		}
	else	s_ctl_put(&l_ans, "ERROR unknown request, try HELP\n");

	if ( l_ans.nomem )
		{
		s_ctl_restart = 0;
		free(l_ans.data);
		l_ans = (T2R$_CTL_ANS) {0};
		s_ctl_put(&l_ans, "ERROR out of memory\n");
		}

	for (l_off = 0; l_ans.data && (l_off < l_ans.len); l_off += (size_t) l_sent)
		{
		if ( 0 >= (l_sent = send(a_sd, l_ans.data + l_off, l_ans.len - l_off, MSG_NOSIGNAL)) )
			break;							/* The client has gone: not our problem */
		}

	free(l_ans.data);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: The control thread: accept the clients one by one and serve them, until <g_exit_flag>.
 *	After an accepted RESTART raise <g_restart_flag> and <g_exit_flag>: main() restarts the process.
 *
 *   INPUTS:
 *	a_arg:		Unused
 *
 *   IMPLICIT INPUTS:
 *	s_ctl_sd, g_exit_flag
 *
 *   IMPLICIT OUTPUTS:
 *	g_restart_flag, g_exit_flag
 *
 *   RETURNS:
 *	NULL
 */
static void *	s_ctl_thread (
		void	*a_arg
		)
{
int	l_rc, l_sd;
struct pollfd	l_pfd = {.fd = s_ctl_sd, .events = POLLIN};

	while ( !g_exit_flag )
		{
		if ( 0 > (l_rc = poll(&l_pfd, 1, T2R$K_CTL_POLL_MSEC)) )
			{
			if ( errno == EINTR )
				continue;

			$PUTMSG_FAO(T2R$__CTLSYSWRN, "poll()", l_rc, errno);
			break;
			}

		if ( !l_rc )
			continue;

		if ( 0 > (l_sd = accept(s_ctl_sd, NULL, NULL)) )
			{
			$PUTMSG_FAO(T2R$__CTLSYSWRN, "accept()", l_sd, errno);
			continue;
			}

		s_ctl_serve(l_sd);
		close(l_sd);

		if ( s_ctl_restart )
			{
			g_restart_flag = 1;
			g_exit_flag = 1;
			}
		}

	return	NULL;
}



/*
 *   DESCRIPTION: Open the control channel: an AF_UNIX socket at <a_path> with T2R$K_CTL_MODE rights,
 *	and start the control thread. A failure here never stops the gateway: it goes on without the
 *	control channel (CTLERR/CTLBUSY). A socket file left by a killed gateway is removed; a socket
 *	which answers belongs to another running gateway and is left alone.
 *
 *   INPUTS:
 *	a_path:		A path of the socket; NULL, "" or "NONE" - the channel is disabled
 *
 *   IMPLICIT OUTPUTS:
 *	s_ctl_sd, s_ctl_tid, s_ctl_path
 *
 *   RETURNS:
 *	STS$K_SUCCESS - is ready or disabled, STS$K_WARN - the gateway goes on without it
 */
int	t2r$ctl_start (
	const char	*a_path
		)
{
int	l_rc, l_sd;
struct sockaddr_un	l_sun = {.sun_family = AF_UNIX};
struct stat	l_st;

	if ( !a_path || !*a_path || !strcasecmp(a_path, "NONE") )
		return	$PUTMSG_FAO(T2R$__CTLOFF), STS$K_SUCCESS;

	if ( strlen(a_path) >= sizeof(l_sun.sun_path) )
		return	$PUTMSG_FAO(T2R$__CTLERR, a_path, "strlen(path) >= sizeof(sun_path)", -1, ENAMETOOLONG), STS$K_WARN;

	strcpy(l_sun.sun_path, a_path);

	if ( 0 > (l_sd = socket(AF_UNIX, SOCK_STREAM, 0)) )
		return	$PUTMSG_FAO(T2R$__CTLERR, a_path, "socket()", l_sd, errno), STS$K_WARN;

	/* Is there a file already? A live socket is another gateway's, a dead one is a leftover */
	if ( !lstat(a_path, &l_st) )
		{
		if ( !S_ISSOCK(l_st.st_mode) )
			return	close(l_sd), $PUTMSG_FAO(T2R$__CTLERR, a_path, "lstat(): not a socket", -1, ENOTSOCK), STS$K_WARN;

		if ( !connect(l_sd, (struct sockaddr *) &l_sun, sizeof(l_sun)) )
			return	close(l_sd), $PUTMSG_FAO(T2R$__CTLBUSY, a_path), STS$K_WARN;

		close(l_sd);

		if ( 0 > (l_sd = socket(AF_UNIX, SOCK_STREAM, 0)) )
			return	$PUTMSG_FAO(T2R$__CTLERR, a_path, "socket()", l_sd, errno), STS$K_WARN;

		if ( unlink(a_path) )
			return	close(l_sd), $PUTMSG_FAO(T2R$__CTLERR, a_path, "unlink()", -1, errno), STS$K_WARN;
		}

	if ( (l_rc = bind(l_sd, (struct sockaddr *) &l_sun, sizeof(l_sun))) )
		return	close(l_sd), $PUTMSG_FAO(T2R$__CTLERR, a_path, "bind()", l_rc, errno), STS$K_WARN;

	/* The socket is created by the umask rights: set ours before anybody is let in by listen() */
	if ( (l_rc = chmod(a_path, T2R$K_CTL_MODE)) )
		{
		$PUTMSG_FAO(T2R$__CTLERR, a_path, "chmod()", l_rc, errno);
		return	close(l_sd), unlink(a_path), STS$K_WARN;
		}

	if ( (l_rc = listen(l_sd, 4)) )
		{
		$PUTMSG_FAO(T2R$__CTLERR, a_path, "listen()", l_rc, errno);
		return	close(l_sd), unlink(a_path), STS$K_WARN;
		}

	s_ctl_sd = l_sd;
	strcpy(s_ctl_path, a_path);

	if ( (l_rc = pthread_create(&s_ctl_tid, NULL, s_ctl_thread, NULL)) )
		{
		$PUTMSG_FAO(T2R$__CTLERR, a_path, "pthread_create()", l_rc, l_rc);
		close(s_ctl_sd);
		s_ctl_sd = -1;
		return	unlink(a_path), STS$K_WARN;
		}

	return	$PUTMSG_FAO(T2R$__CTLREADY, a_path), STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Close the control channel: wait the control thread (it sees <g_exit_flag> in at most
 *	T2R$K_CTL_POLL_MSEC + T2R$K_CTL_TMO_MSEC), close and remove the socket.
 *
 *   IMPLICIT INPUTS:
 *	s_ctl_sd, s_ctl_tid, s_ctl_path, g_exit_flag (must be set by the caller)
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
int	t2r$ctl_stop (void)
{
	if ( 0 > s_ctl_sd )
		return	STS$K_SUCCESS;

	pthread_join(s_ctl_tid, NULL);

	close(s_ctl_sd);
	s_ctl_sd = -1;

	unlink(s_ctl_path);

	return	STS$K_SUCCESS;
}
