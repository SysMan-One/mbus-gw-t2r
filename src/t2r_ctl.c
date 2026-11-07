#define	__MODULE__	"T2R-CTL"
#define	__IDENT__	"V.01-01"
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
**	The protocol is the StarLet AVProto TLV, one request per connection; the PDU, the command codes
**	and the tags are described in src/t2r_ctlproto.h, the client is mbus-ctl (src/mbus_ctl.c):
**
**	    the client sends ONE PDU:	magic "MBUSCTL", u_csr = T2R$K_CTL_CMD_*, the arguments as TLVs
**	    the gateway answers:	ONE PDU: u_csr = the status (the low bit - success), the same u_seq,
**					the data as the records (REC=GATEWAY/LISTENER/LINE/CONN/RESET/CHECK)
**					and REASON on a failure; then closes the connection.
**
**	A PDU with a wrong magic or too long is not answered: the connection is closed at once.
**
**	The commands (see s_ctl_cmds[]):
**		SHOW_GATEWAY		- GATEWAY + LISTENER records
**		SHOW_LINES		- LINE records (the serial lines)
**		SHOW_CONNECTIONS	- LISTENER + CONN records (the TCP clients and their counters)
**		RESET_CONNECTIONS	- ALL | ID | ADDR [PORT]: shutdown() of the client sockets
**		RESTART_GATEWAY		- check the settings by a child "/CHECK" run, then re-exec
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
**	06-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - Initial revision.
**
**	07-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - The protocol is the AVProto TLV (src/t2r_ctlproto.h),
**				the text request lines and the HELP request are gone.
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
#include	"t2r_ctlproto.h"


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
 * An answer is composed in memory as one AVProto PDU of T2R$K_CTL_ANSSZ octets, then sent at once:
 * the registry lock is never held over a write to a slow client.
 */
typedef struct	t2r_ctl_ans_t {
	AVPROTO_PDU	*pdu;							/* T2R$K_CTL_ANSSZ octets		*/
	int		cut;							/* No room for one more record		*/
} T2R$_CTL_ANS;



/*
 *   DESCRIPTION: Append one TLV to the answer. Once the answer is cut, nothing more is put.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_tag:		A tag id, T2R$K_CTL_TAG_*
 *	a_type:		TAG$K_WORD/LONGWORD/QWORD/BBLOCK
 *	a_val:		The value (a number - in the host byte order)
 *	a_valsz:	The size of the value
 *
 *   OUTPUTS:
 *	a_ans:		.pdu; .cut if there is no room
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ctl_put (
		T2R$_CTL_ANS	*a_ans,
		unsigned	a_tag,
		unsigned	a_type,
	const	void		*a_val,
		unsigned	a_valsz
		)
{
	if ( a_ans->cut )
		return;

	if ( !(1 & __tlv$put(a_ans->pdu, T2R$K_CTL_ANSSZ, a_tag, a_type, (void *) a_val, a_valsz)) )
		a_ans->cut = 1;
}


/*
 *   DESCRIPTION: The typed forms of s_ctl_put(): a WORD, a LONGWORD, a QWORD, a text (at most
 *	T2R$K_CTL_RECMAX / 2 octets).
 */
static inline void	s_ctl_putw (T2R$_CTL_ANS *a_ans, unsigned a_tag, uint16_t a_val)
{
	s_ctl_put(a_ans, a_tag, TAG$K_WORD, &a_val, sizeof(a_val));
}

static inline void	s_ctl_putl (T2R$_CTL_ANS *a_ans, unsigned a_tag, uint32_t a_val)
{
	s_ctl_put(a_ans, a_tag, TAG$K_LONGWORD, &a_val, sizeof(a_val));
}

static inline void	s_ctl_putq (T2R$_CTL_ANS *a_ans, unsigned a_tag, uint64_t a_val)
{
	s_ctl_put(a_ans, a_tag, TAG$K_QWORD, &a_val, sizeof(a_val));
}

static inline void	s_ctl_puts (T2R$_CTL_ANS *a_ans, unsigned a_tag, const char *a_val)
{
	s_ctl_put(a_ans, a_tag, TAG$K_BBLOCK, a_val, (unsigned) strnlen(a_val, T2R$K_CTL_RECMAX / 2));
}



/*
 *   DESCRIPTION: Start a record: the REC tag. With less than T2R$K_CTL_RECMAX octets of room (and the
 *	room for CUT and REASON) the answer is cut here: whole records only.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_rec:		T2R$K_CTL_REC_*
 *
 *   OUTPUTS:
 *	a_ans:		.cut
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ctl_rec (
		T2R$_CTL_ANS	*a_ans,
		uint16_t	a_rec
		)
{
	if ( (sizeof(AVPROTO_HDR) + be32toh(a_ans->pdu->r_hdr.u_len) + 2 * T2R$K_CTL_RECMAX) > T2R$K_CTL_ANSSZ )
		a_ans->cut = 1;

	s_ctl_putw(a_ans, T2R$K_CTL_TAG_REC, a_rec);
}



/*
 *   DESCRIPTION: Put the REASON of a failure (snprintf() style: the protocol data, not a message of
 *	the log).
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_fmt, ...:	A format and its arguments
 *
 *   RETURNS:
 *	STS$K_ERROR - for "return s_ctl_fail(...)"
 */
static int	s_ctl_fail (
		T2R$_CTL_ANS	*a_ans,
	const	char	*a_fmt,
		...
		)
{
va_list	l_ap;
char	l_text[T2R$K_CTL_RECMAX / 2];
int	l_cut = a_ans->cut;

	va_start(l_ap, a_fmt);
	vsnprintf(l_text, sizeof(l_text), a_fmt, l_ap);
	va_end(l_ap);

	a_ans->cut = 0;								/* The room for it is kept by s_ctl_rec() */
	s_ctl_puts(a_ans, T2R$K_CTL_TAG_REASON, l_text);
	a_ans->cut = l_cut;

	return	STS$K_ERROR;
}



/*
 *   DESCRIPTION: Get a number argument of a request: a WORD, LONGWORD or QWORD TLV. __tlv$lookup(),
 *	not __tlv$get(): the wire length must be checked against the type (StarLet 1.6 does not).
 *
 *   INPUTS:
 *	a_req:		The request PDU
 *	a_reqsz:	Its real size
 *	a_tag:		A tag id
 *
 *   OUTPUTS:
 *	a_val:		The value
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_WARN - absent, STS$K_ERROR - not a number or ill-formed
 */
static int	s_ctl_getnum (
		void		*a_req,
		unsigned	a_reqsz,
		unsigned	a_tag,
		uint64_t	*a_val
		)
{
int	l_rc;
unsigned	l_type = 0, l_len = 0;
void	*l_p = NULL;
uint16_t	l_w;
uint32_t	l_l;
uint64_t	l_q;

	if ( !(1 & (l_rc = __tlv$lookup(a_req, a_reqsz, NULL, a_tag, &l_type, &l_p, &l_len))) )
		return	l_rc;

	if ( (l_type == TAG$K_WORD) && (l_len == sizeof(l_w)) )
		memcpy(&l_w, l_p, sizeof(l_w)), *a_val = be16toh(l_w);
	else if ( (l_type == TAG$K_LONGWORD) && (l_len == sizeof(l_l)) )
		memcpy(&l_l, l_p, sizeof(l_l)), *a_val = be32toh(l_l);
	else if ( (l_type == TAG$K_QWORD) && (l_len == sizeof(l_q)) )
		memcpy(&l_q, l_p, sizeof(l_q)), *a_val = be64toh(l_q);
	else	return	STS$K_ERROR;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Put the ADDR and PORT fields of a socket address.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_sk:		A socket address
 *
 *   RETURNS:
 *	NONE
 */
static void	s_ctl_put_addr (
		T2R$_CTL_ANS	*a_ans,
	const struct sockaddr_in *a_sk
		)
{
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_ADDR, ntohl(a_sk->sin_addr.s_addr));
	s_ctl_putw(a_ans, T2R$K_CTL_TAG_PORT, ntohs(a_sk->sin_port));
}



/*
 *   DESCRIPTION: Put a LISTENER record per listener.
 *
 *   INPUTS:
 *	a_ans:		An answer
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
T2R$_LISTENER	*l_lsnr;

	for (int i = 0; i < g_listeners_nr; i++)
		{
		l_lsnr = &g_listeners[i];

		s_ctl_rec(a_ans, T2R$K_CTL_REC_LISTENER);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_LSN, (uint32_t) i);
		s_ctl_put_addr(a_ans, &l_lsnr->sk);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_MODE, (l_lsnr->mode == T2R$K_MODE_RAW) ? T2R$K_CTL_MODE_RAW : T2R$K_CTL_MODE_MODBUS);
		s_ctl_puts(a_ans, T2R$K_CTL_TAG_TARGET, l_lsnr->target);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_STATE, (0 <= l_lsnr->fd) ? T2R$K_CTL_STATE_UP : T2R$K_CTL_STATE_DOWN);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_CONNLM, (uint32_t) l_lsnr->connlm);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_IDLETMO, (uint32_t) l_lsnr->idletmo);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_SESSIONS, (uint32_t) __atomic_load_n(&l_lsnr->sessions, __ATOMIC_ACQUIRE));
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_ACCEPTED, l_lsnr->accepted);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_REJECTED, l_lsnr->rejected);
		}
}



/*
 *   DESCRIPTION: A t2r$net_sess_walk() routine of SHOW CONNECTIONS: put a CONN record of a session.
 *
 *   INPUTS:
 *	a_session:	A session (the registry lock is held)
 *	a_arg:		T2R$_CTL_ANS * - the answer
 *
 *   RETURNS:
 *	STS$K_SUCCESS - go on
 */
static int	s_ctl_conn_rec (
		T2R$_SESSION	*a_session,
		void		*a_arg
		)
{
T2R$_CTL_ANS	*l_ans = a_arg;

	s_ctl_rec(l_ans, T2R$K_CTL_REC_CONN);
	s_ctl_putl(l_ans, T2R$K_CTL_TAG_ID, a_session->id);
	s_ctl_putl(l_ans, T2R$K_CTL_TAG_SD, (uint32_t) a_session->sd);
	s_ctl_putl(l_ans, T2R$K_CTL_TAG_LSN, (uint32_t) (a_session->listener - g_listeners));
	s_ctl_putw(l_ans, T2R$K_CTL_TAG_MODE, (a_session->listener->mode == T2R$K_MODE_RAW) ? T2R$K_CTL_MODE_RAW : T2R$K_CTL_MODE_MODBUS);
	s_ctl_put_addr(l_ans, &a_session->sk);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_SINCE, (uint64_t) a_session->start);
	s_ctl_putw(l_ans, T2R$K_CTL_TAG_RESET, (uint16_t) !!a_session->reset);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_NREQ, a_session->nreq);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_NANS, a_session->nans);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_NERR, a_session->nerr);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_NRX, a_session->nrx);
	s_ctl_putq(l_ans, T2R$K_CTL_TAG_NTX, a_session->ntx);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW CONNECTIONS - the listeners and the TCP clients with their counters.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_req, a_reqsz:	The request PDU (no arguments)
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_conn (
		T2R$_CTL_ANS	*a_ans,
		void		*a_req,
		unsigned	a_reqsz
		)
{
	s_ctl_put_listeners(a_ans);
	t2r$net_sess_walk(s_ctl_conn_rec, a_ans);

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
 *	a_ans:		An answer
 *	a_req, a_reqsz:	The request PDU (no arguments)
 *
 *   IMPLICIT INPUTS:
 *	g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_lines (
		T2R$_CTL_ANS	*a_ans,
		void		*a_req,
		unsigned	a_reqsz
		)
{
int	l_sessions[T2R$K_MAX_SERIALS] = {0}, l_lsnrs;
uint16_t	l_state;
T2R$_SERIAL	*l_serial;

	t2r$net_sess_walk(s_ctl_line_count, l_sessions);

	for (int i = 0; i < g_serials_nr; i++)
		{
		l_serial = &g_serials[i];

		l_lsnrs = 0;

		for (int j = 0; j < g_listeners_nr; j++)
			l_lsnrs += (g_listeners[j].serial == l_serial);

		if ( l_serial->state == T2R$K_STATE_IDLE )
			l_state = T2R$K_CTL_STATE_CLOSED;			/* No listener has opened it */
		else	l_state = l_serial->nodev ? T2R$K_CTL_STATE_ABSENT : T2R$K_CTL_STATE_OPEN;

		s_ctl_rec(a_ans, T2R$K_CTL_REC_LINE);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_LINE, (uint32_t) i);
		s_ctl_puts(a_ans, T2R$K_CTL_TAG_DEVICE, l_serial->devname);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_BAUD, (uint32_t) l_serial->baud);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_DATABITS, (uint16_t) l_serial->databits);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_PARITY, (uint16_t) l_serial->parity);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_STOPBITS, (uint16_t) l_serial->stopbits);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_IOTMO, (uint32_t) l_serial->anstmo_msec);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_LATEGD, (uint32_t) l_serial->lategd_msec);
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_RS485, (uint16_t) !!(l_serial->flags & T2R$M_SERIAL_RS485));
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_ADDTS, (uint16_t) !!(l_serial->flags & T2R$M_SERIAL_ADDTS));
		s_ctl_putw(a_ans, T2R$K_CTL_TAG_STATE, l_state);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_LISTENERS, (uint32_t) l_lsnrs);
		s_ctl_putl(a_ans, T2R$K_CTL_TAG_SESSIONS, (uint32_t) l_sessions[i]);
		}

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: SHOW GATEWAY - the revision, the process, the uptime and the listeners.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_req, a_reqsz:	The request PDU (no arguments)
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static int	s_ctl_show_gw (
		T2R$_CTL_ANS	*a_ans,
		void		*a_req,
		unsigned	a_reqsz
		)
{
uint32_t	l_up = 0;

	for (int i = 0; i < g_listeners_nr; i++)
		l_up += (0 <= g_listeners[i].fd);

	s_ctl_rec(a_ans, T2R$K_CTL_REC_GATEWAY);
	s_ctl_puts(a_ans, T2R$K_CTL_TAG_IDENT, g_t2r_ident);
	s_ctl_puts(a_ans, T2R$K_CTL_TAG_REV, g_t2r_rev);
	s_ctl_puts(a_ans, T2R$K_CTL_TAG_ARCH, __ARCH__NAME__);
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_PID, (uint32_t) getpid());
	s_ctl_putq(a_ans, T2R$K_CTL_TAG_STARTED, (uint64_t) g_start_time);
	s_ctl_putq(a_ans, T2R$K_CTL_TAG_NOW, (uint64_t) time(NULL));
	s_ctl_put(a_ans, T2R$K_CTL_TAG_SETTINGS, TAG$K_BBLOCK, $ASCPTR(&g_settings), $ASCLEN(&g_settings));
	s_ctl_put(a_ans, T2R$K_CTL_TAG_CONTROL, TAG$K_BBLOCK, s_ctl_path, (unsigned) strlen(s_ctl_path));
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_LINES, (uint32_t) g_serials_nr);
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_LISTENERS, (uint32_t) g_listeners_nr);
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_UP, l_up);
	s_ctl_putl(a_ans, T2R$K_CTL_TAG_CONNECTIONS, (uint32_t) t2r$net_sess_count());

	s_ctl_put_listeners(a_ans);

	return	STS$K_SUCCESS;
}



/*
 * RESET CONNECTIONS: what is to be reset
 */
typedef struct	t2r_ctl_sel_t {
	int		all;							/* ALL				*/
	uint32_t	id;							/* A session id, 0 - not given	*/
	struct in_addr	addr;							/* A client address ...		*/
	int		port;							/* ... and its port, 0 - any	*/
	int		count;							/* Has been reset		*/
	T2R$_CTL_ANS	*ans;
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

	s_ctl_rec(l_sel->ans, T2R$K_CTL_REC_RESET);
	s_ctl_putl(l_sel->ans, T2R$K_CTL_TAG_ID, a_session->id);
	s_ctl_put_addr(l_sel->ans, &a_session->sk);
	l_sel->count++;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: RESET CONNECTIONS - drop the TCP clients. The arguments: ALL, or ID, or ADDR with an
 *	optional PORT.
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_req, a_reqsz:	The request PDU
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_ERROR - no or bad selector, nothing matches
 */
static int	s_ctl_reset_conn (
		T2R$_CTL_ANS	*a_ans,
		void		*a_req,
		unsigned	a_reqsz
		)
{
T2R$_CTL_SEL	l_sel = {.ans = a_ans};
uint64_t	l_val = 0;
int	l_rc;
char	l_ip[INET_ADDRSTRLEN] = "?";

	if ( (1 & (l_rc = s_ctl_getnum(a_req, a_reqsz, T2R$K_CTL_TAG_ALL, &l_val))) && l_val )
		l_sel.all = 1;
	else if ( (l_rc == STS$K_ERROR) )
		return	s_ctl_fail(a_ans, "bad ALL argument");
	else if ( 1 & (l_rc = s_ctl_getnum(a_req, a_reqsz, T2R$K_CTL_TAG_ID, &l_val)) )
		{
		if ( !l_val || (l_val > UINT32_MAX) )
			return	s_ctl_fail(a_ans, "bad connection id %" PRIu64, l_val);

		l_sel.id = (uint32_t) l_val;
		}
	else if ( (l_rc == STS$K_ERROR) )
		return	s_ctl_fail(a_ans, "bad ID argument");
	else if ( 1 & (l_rc = s_ctl_getnum(a_req, a_reqsz, T2R$K_CTL_TAG_ADDR, &l_val)) )
		{
		if ( l_val > UINT32_MAX )
			return	s_ctl_fail(a_ans, "bad address");

		l_sel.addr.s_addr = htonl((uint32_t) l_val);

		if ( 1 & (l_rc = s_ctl_getnum(a_req, a_reqsz, T2R$K_CTL_TAG_PORT, &l_val)) )
			{
			if ( !$ISINRANGE(l_val, T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
				return	s_ctl_fail(a_ans, "bad port %" PRIu64, l_val);

			l_sel.port = (int) l_val;
			}
		else if ( l_rc == STS$K_ERROR )
			return	s_ctl_fail(a_ans, "bad PORT argument");
		}
	else	return	s_ctl_fail(a_ans, "which connection? Give an id, an address, address:port or ALL");

	t2r$net_sess_walk(s_ctl_reset_one, &l_sel);

	if ( !l_sel.count )
		{
		if ( l_sel.id )
			return	s_ctl_fail(a_ans, "no connection matches <%" PRIu32 ">", l_sel.id);

		inet_ntop(AF_INET, &l_sel.addr, l_ip, sizeof(l_ip));

		return	l_sel.port ? s_ctl_fail(a_ans, "no connection matches <%s:%d>", l_ip, l_sel.port)
			: s_ctl_fail(a_ans, "no connection matches <%s>", l_ip);
		}

	s_ctl_putl(a_ans, T2R$K_CTL_TAG_COUNT, (uint32_t) l_sel.count);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Check the settings file by a child run of the same program: "<image> /SETTINGS=<file>
 *	/CHECK" parses the settings exactly as a start does and exits with its status. The live tables
 *	of the gateway are not touched at all. The WARNING/ERROR/FATAL lines of the child are put into
 *	the answer as CHECK records: the operator sees why.
 *
 *   INPUTS:
 *	a_ans:		An answer
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
		return	$PUTMSG_FAO(T2R$__CTLSYSWRN, "pipe()", -1, errno), s_ctl_fail(a_ans, "pipe() failed");

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
		return	s_ctl_fail(a_ans, "cannot run the settings check, error %d", l_rc);
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
			{
			s_ctl_rec(a_ans, T2R$K_CTL_REC_CHECK);
			s_ctl_puts(a_ans, T2R$K_CTL_TAG_TEXT, l_line);
			}
		}

	if ( WIFEXITED(l_status) && (WEXITSTATUS(l_status) == EXIT_SUCCESS) )
		return	STS$K_SUCCESS;

	$PUTMSG_FAO(T2R$__CTLCHKFAIL, l_status);

	return	s_ctl_fail(a_ans, "the settings check has failed, the gateway goes on as it is");
}



/*
 *   DESCRIPTION: RESTART GATEWAY - check the settings, answer, then let main() restart the process
 *	(the same PID: systemd and procd see nothing, the listeners and the lines are opened anew).
 *
 *   INPUTS:
 *	a_ans:		An answer
 *	a_req, a_reqsz:	The request PDU (no arguments)
 *
 *   IMPLICIT OUTPUTS:
 *	s_ctl_restart
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or the status of the settings check
 */
static int	s_ctl_restart_gw (
		T2R$_CTL_ANS	*a_ans,
		void		*a_req,
		unsigned	a_reqsz
		)
{
int	l_rc;

	if ( !(1 & (l_rc = s_ctl_check_settings(a_ans))) )
		return	l_rc;

	$PUTMSG_FAO(T2R$__CTLRESTART);

	s_ctl_restart = 1;							/* The flags are raised after the answer */

	return	STS$K_SUCCESS;
}



/*
 * The commands: a code of u_csr -> a routine
 */
typedef struct	t2r_ctl_cmd_t {
	uint32_t	code;							/* T2R$K_CTL_CMD_*			*/
	const char	*name;							/* For the trace			*/
	int		(*rtn) (T2R$_CTL_ANS *a_ans, void *a_req, unsigned a_reqsz);
} T2R$_CTL_CMD;

static const T2R$_CTL_CMD	s_ctl_cmds [] = {
	{ T2R$K_CTL_CMD_SHOW_GATEWAY,		"SHOW GATEWAY",		s_ctl_show_gw },
	{ T2R$K_CTL_CMD_SHOW_LINES,		"SHOW LINES",		s_ctl_show_lines },
	{ T2R$K_CTL_CMD_SHOW_CONNECTIONS,	"SHOW CONNECTIONS",	s_ctl_show_conn },
	{ T2R$K_CTL_CMD_RESET_CONNECTIONS,	"RESET CONNECTIONS",	s_ctl_reset_conn },
	{ T2R$K_CTL_CMD_RESTART_GATEWAY,	"RESTART GATEWAY",	s_ctl_restart_gw },
};



/*
 *   DESCRIPTION: Read exactly <a_len> octets from the client; a client which stays silent for
 *	T2R$K_CTL_TMO_MSEC is dropped.
 *
 *   INPUTS:
 *	a_sd:		A client socket
 *	a_len:		The octets to read
 *
 *   OUTPUTS:
 *	a_buf:		The data
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_WARN - a timeout or the client has gone
 */
static int	s_ctl_recvn (
		int	a_sd,
		void	*a_buf,
		size_t	a_len
		)
{
ssize_t	l_got;
struct pollfd	l_pfd = {.fd = a_sd, .events = POLLIN};

	for (size_t l_off = 0; l_off < a_len; l_off += (size_t) l_got)
		{
		if ( 0 >= poll(&l_pfd, 1, T2R$K_CTL_TMO_MSEC) )
			return	STS$K_WARN;

		if ( 0 >= (l_got = recv(a_sd, (char *) a_buf + l_off, a_len - l_off, 0)) )
			return	STS$K_WARN;
		}

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Serve one client: read one request PDU (the header, then exactly u_len octets), run
 *	the command, send the answer PDU. A wrong magic or a too long PDU is not answered. The caller
 *	closes the connection.
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
union	{
	AVPROTO_PDU	pdu;
	uint8_t		raw[T2R$K_CTL_REQSZ];
	} l_req = {0};
unsigned	l_len = 0, l_csr = 0, l_seq = 0;
int	l_rc = STS$K_ERROR;
size_t	l_off, l_total;
ssize_t	l_sent;
T2R$_CTL_ANS	l_ans = {0};
const T2R$_CTL_CMD	*l_cmd = NULL;

	if ( !(1 & s_ctl_recvn(a_sd, &l_req.pdu.r_hdr, sizeof(AVPROTO_HDR))) )
		return	STS$K_WARN;

	if ( !(1 & __tlv$hget(&l_req.pdu, (const unsigned char *) T2R$K_CTL_MAGIC, &l_len, &l_csr, &l_seq)) )
		return	$IFTRACE(g_trace, "Control request: wrong magic, dropped"), STS$K_WARN;

	if ( l_len > (T2R$K_CTL_REQSZ - sizeof(AVPROTO_HDR)) )
		return	$IFTRACE(g_trace, "Control request: %u octets is too long, dropped", l_len), STS$K_WARN;

	if ( l_len && !(1 & s_ctl_recvn(a_sd, l_req.pdu.r_tlv, l_len)) )
		return	STS$K_WARN;

	/* The answer: the header now, the status into u_csr at the end */
	if ( !(l_ans.pdu = malloc(T2R$K_CTL_ANSSZ)) )
		return	$PUTMSG_FAO(T2R$__CTLSYSWRN, "malloc()", -1, errno), STS$K_WARN;

	__tlv$hset(l_ans.pdu, (const unsigned char *) T2R$K_CTL_MAGIC, 0, l_seq);

	for (size_t i = 0; i < $ARRSZ(s_ctl_cmds); i++)
		{
		if ( s_ctl_cmds[i].code == l_csr )
			{
			l_cmd = &s_ctl_cmds[i];
			break;
			}
		}

	if ( l_cmd )
		{
		$IFTRACE(g_trace, "Control request: %s, %u octets of arguments", l_cmd->name, l_len);

		l_rc = l_cmd->rtn(&l_ans, &l_req.pdu, (unsigned) (sizeof(AVPROTO_HDR) + l_len));
		}
	else	l_rc = s_ctl_fail(&l_ans, "unknown command code %u", l_csr);

	if ( l_ans.cut )							/* The room for it is kept by s_ctl_rec() */
		{
		l_ans.cut = 0;
		s_ctl_putw(&l_ans, T2R$K_CTL_TAG_CUT, 1);
		}

	l_ans.pdu->r_hdr.u_csr = htobe32((uint32_t) l_rc);
	l_total = sizeof(AVPROTO_HDR) + be32toh(l_ans.pdu->r_hdr.u_len);

	for (l_off = 0; l_off < l_total; l_off += (size_t) l_sent)
		{
		if ( 0 >= (l_sent = send(a_sd, (char *) l_ans.pdu + l_off, l_total - l_off, MSG_NOSIGNAL)) )
			break;							/* The client has gone: not our problem */
		}

	free(l_ans.pdu);

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
