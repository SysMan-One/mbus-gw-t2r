#define	__MODULE__	"T2R-NET"
#define	__IDENT__	"X.00-13ECO09"
#define	__REV__		"00.13.09"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: A set of routines related to network I/O
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  26-SEP-2025
**
**  MODIFICATION HISTORY:
**
**	25-AUG-2026	RRL	X.00-04 / REV: 00.04.00 - Audit fixes:
**				s_net_listener(): fixed a detection of the accept() failure (the condition
**				was always false), calloc() check, sockaddr length is reset before every
**				accept(), EINTR/error handling of poll();
**				s_net_session(): pthread_detach() - the session threads were never joined
**				(a resource leak), added the missing return statement;
**				s_net_mbap_rx(): the MBAP header is not parsed until it is received as a
**				whole (a read of an uninitialized memory), added the protocol identifier
**				and the minimal length checks, fixed the missing $LOG argument;
**				t2r$net_start_listeners(): the target device is opened BEFORE the socket
**				is published to the poll() array, the pollfd array is initialized properly,
**				the listening socket is not leaked on the error paths.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The listener lifecycle diagnostics are signalled
**				via $PUTMSG_FAO by the coded catalogue messages now: LSNRRDY, LSNRERR.
**
**	25-AUG-2026	RRL	X.00-07 / REV: 00.07.00 - The client connect/disconnect events are
**				signalled via $PUTMSG_FAO by the coded NETCONN/NETDISCN messages;
**				the duplicating "Start session" line is removed (NETCONN covers it).
**
**	14-OCT-2026	RRL	X.00-09 / REV: 00.09.00 - The "peer close connection" is an ordinary end of
**				a client session: it is logged as -I- instead of -W-; the routines still
**				return STS$K_WARN, so the session is closed as before.
**
**	14-OCT-2026	RRL	X.00-11 / REV: 00.11.00 - The logging is FAO-only: every $LOG() is replaced by
**				$PUTMSG_FAO() with a coded message (PEERCLOSE, NETSYSERR, SESSFAIL, ...);
**				the dead commented-out $LOG() in s_net_recvn() is removed.
**
**	14-OCT-2026	RRL	X.00-12 / REV: 00.12.00 - <connlm>: the listener counts its sessions (atomic) and
**				rejects one more at once (CONNLIMIT); listen() gets its own backlog
**				T2R$K_LISTEN_BACKLOG (was <connlm>); the idle timeout is <idletmo> of the
**				listener (was T2R$K_IDLE_TMO_SEC = 1200).
**
**	14-OCT-2026	RRL	X.00-12ECO01 / REV: 00.12.01 - The routine headers are brought in line with the code
**				(s_net_mbap_tx() had a copy of the s_net_mbap_rx() header, s_net_session()
**				documented a wrong parameter); LSNRABRT shows the socket before it is closed
**				(was always #-1).
**
**	14-OCT-2026	RRL	X.00-12ECO02 / REV: 00.12.02 - SO_REUSEPORT is not set on the listening sockets: a second
**				instance of the gateway was bound to the same ports silently and the kernel
**				split the clients between both processes; now its bind() fails (LSNRERR,
**				errno 98). SO_REUSEADDR is kept for a quick restart.
**
**	14-OCT-2026	RRL	X.00-13ECO01 / REV: 00.13.01 - t2r$net_stop_listeners(): DEVCLSERR was signalled
**				twice - t2r$tty_close() signals it itself.
**
**	02-NOV-2026	CLD	X.00-13ECO09 / REV: 00.13.09 - A listener is not skipped when its device is absent at
**				the start (DEVABSENT: it is opened later); the listener thread calls
**				t2r$tty_check() for every serial every T2R$K_CHECK_MSEC (a replugged USB
**				adapter of an idle line is closed and reopened); no REQFAIL for exception 10
**				(the device is absent - DEVLOST/DEVABSENT has told it once, not per request).
**--
*/

#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<unistd.h>
#include	<poll.h>
#include	<netinet/tcp.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


extern int	g_exit_flag, g_trace;


extern T2R$_LISTENER	g_listeners[];
extern int		g_listeners_nr;

extern T2R$_SERIAL	g_serials[];						/* Is used by t2r$net_stop_listeners() */
extern int		g_serials_nr;
static struct pollfd	s_pfd_lsnr[T2R$K_MAX_LISTENERS];			/* Initialized in t2r$net_start_listeners() */

static const	int	s_one = 1;


/*
 *   DESCRIPTION: Read a next portion of data from the session's TCP socket into the network
 *	buffer of the session. A single recv() call is performed: the routine is driven by the
 *	session's state machine and is called until <datalen> octets are accumulated.
 *
 *   INPUTS:
 *	a_session:	A context of the network session; .sd - socket, .datalen - octets still
 *			to be read, .netbuf_dsc - a descriptor of the accumulating buffer
 *
 *   OUTPUTS:
 *	a_session:	.netbuf_dsc->len is advanced by the has been read octets,
 *			.datalen is decreased accordingly
 *
 *   RETURNS:
 *	condition code; STS$K_WARN - the peer has closed the connection
 */
static int	s_net_recvn (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;



											/* Read a data from socket */
	if ( !(l_rc = recv(a_session->sd, l_netbuf_dsc->data + l_netbuf_dsc->len, a_session->datalen, MSG_NOSIGNAL)) )
		return	$PUTMSG_FAO(T2R$__PEERCLOSE, a_session->sd), STS$K_WARN;

	if ( 0 > l_rc )									/* Check return status */
		return	$PUTMSG_FAO(T2R$__NETSYSERR, a_session->sd, "recv()", l_rc, errno);


	l_netbuf_dsc->len += l_rc;							/* Adjust count of data in the buffer */
	a_session->datalen -= l_rc;							/* Adjust data to be read at next iteration ? */


	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Send a next portion of data from the network buffer of the session to the
 *	session's TCP socket. A single send() call is performed: the routine is called by the
 *	session's state machine until the whole buffer is transmitted.
 *
 *   INPUTS:
 *	a_session:	A context of the network session; .sd - socket, .datalen - octets already
 *			sent, .netbuf_dsc - a descriptor of the buffer to be transmitted
 *
 *   OUTPUTS:
 *	a_session:	.datalen is advanced by the has been sent octets
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a portion has been sent (or nothing is left: ALLSENT is signalled)
 *	STS$K_WARN	- send() has returned 0 (PEERCLOSE is signalled)
 *	T2R$__NETSYSERR	- send() failed
 */
static int	s_net_sendn (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;

	if ( a_session->datalen == l_netbuf_dsc->len )					/* Just to check that we need to do anything ? */
		return	$PUTMSG_FAO(T2R$__ALLSENT, a_session->sd, a_session->datalen), STS$K_SUCCESS;


											/* Send a data to socket */
	if ( !(l_rc = send(a_session->sd, l_netbuf_dsc->data + a_session->datalen, l_netbuf_dsc->len - a_session->datalen, MSG_NOSIGNAL)) )
		return	$PUTMSG_FAO(T2R$__PEERCLOSE, a_session->sd), STS$K_WARN;

	if ( 0 > l_rc )									/* Check return status */
		return	$PUTMSG_FAO(T2R$__NETSYSERR, a_session->sd, "send()", l_rc, errno);


	a_session->datalen += l_rc;							/* Adjust data to be send at next iteration ? */

	return	STS$K_SUCCESS;
}






/*
 *   DESCRIPTION: Receive the next portion of an MBAP ADU into the session buffer. Once the 7-octet
 *	MBAP header is complete, validate the protocol identifier (0) and the length field
 *	(2..MODBUS$SZ_MAXPDU + 1) and set the number of octets which are still to be read.
 *
 *   INPUTS:
 *	a_session:	A session context: .sd, .datalen, .netbuf_dsc
 *
 *   OUTPUTS:
 *	a_session:	.netbuf_dsc->len and .datalen are updated
 *
 *   RETURNS:
 *	T2R$__FULLMBAP	- a whole ADU is in the buffer
 *	STS$K_SUCCESS	- more data is expected
 *	STS$K_WARN	- the peer has closed the connection
 *	T2R$__BADPROTO, T2R$__PDUSHORT, T2R$__PDULONG, T2R$__NETSYSERR - an error
 */
static int	s_net_mbap_rx (
		T2R$_SESSION	*a_session
		)
{
int	l_rc, l_len;
MODBUS_ADU_T	*l_mbaph;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;

	a_session->datalen = l_netbuf_dsc->len ? a_session->datalen : MODBUS$SZ_MINMBAP;

	if ( !(1 & (l_rc = s_net_recvn(a_session))) )
		return	l_rc;

	/*
	 * A peer is free to deliver the header by pieces, so we do not touch any field of the MBAP
	 * until a whole header is in the buffer - otherwise we would parse an uninitialized memory.
	 */
	if ( l_netbuf_dsc->len < MODBUS$SZ_MINMBAP )
		return	STS$K_SUCCESS;							/* Keep reading the rest of the header */

	l_mbaph = (MODBUS_ADU_T *) l_netbuf_dsc->data;

	if ( ntohs(l_mbaph->proto) )							/* 0 - is the MODBUS protocol identifier */
		return	$PUTMSG_FAO(T2R$__BADPROTO, a_session->sd, ntohs(l_mbaph->proto));

	l_len = ntohs(l_mbaph->len);							/* Get length of the MBAP data part */

	if ( 2 > l_len )								/* Unit + at least a function code */
		return	$PUTMSG_FAO(T2R$__PDUSHORT, a_session->sd, l_len);

	l_len -= 1;									/* Unit field is already there */

	if ( l_len > MODBUS$SZ_MAXPDU )
		return	$PUTMSG_FAO(T2R$__PDULONG, a_session->sd, l_len, MODBUS$SZ_MAXPDU);

	if ( l_netbuf_dsc->len == (l_len + MODBUS$SZ_MINMBAP) )			     	/* Is the whole MODBAS PDU in the buffer already ? */
		return	T2R$__FULLMBAP;

	if ( l_netbuf_dsc->len ==  MODBUS$SZ_MINMBAP )					/* Did we get whole MBAP Header ? */
		a_session->datalen = l_len;

	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Send the next portion of the MBAP answer from the session buffer to the TCP
 *	client (see s_net_sendn()).
 *
 *   INPUTS:
 *	a_session:	A session context: .sd, .datalen - octets already sent, .netbuf_dsc
 *
 *   OUTPUTS:
 *	a_session:	.datalen is advanced
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or the failure status of s_net_sendn() (STS$K_WARN, T2R$__NETSYSERR)
 */
static int	s_net_mbap_tx (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;

	if ( !(1 & (l_rc = s_net_sendn(a_session))) )
		return	l_rc;

	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: A detached thread serving a single TCP client: receive an MBAP request, forward
 *	it as an RTU PDU to the target serial device, convert the RTU answer back to MBAP and send
 *	it to the client. The session is closed on: no activity for <idletmo> seconds of the
 *	listener (READY state), a request/answer exchange longer than T2R$K_NET_TMO_SEC, a peer
 *	close or an error, <g_exit_flag>. On exit the socket is closed, the <connlm> slot of the
 *	listener is released (.sessions--) and the context is freed.
 *
 *   INPUTS:
 *	a_arg:		T2R$_SESSION * - the session context (allocated by s_net_listener(), freed here)
 *
 *   IMPLICIT INPUTS:
 *	g_exit_flag, g_trace
 *
 *   IMPLICIT OUTPUTS:
 *	a_arg->listener->sessions
 *
 *   RETURNS:
 *	NULL - is required by the pthread API only
 */

static void *	s_net_session( void *a_arg)
{
int	l_rc;
struct pollfd l_pfd = {0};
T2R$_SESSION	*l_session;
struct timespec	l_now;
T2R$_SERIAL	*l_serial;
uint8_t		l_excode;

$DESCRIPTOR_S	(l_netbuf_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_req_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_resp_dsc, (2*MODBUS$SZ_MAXPDU));

	pthread_detach(pthread_self());						/* Nobody joins us: release the TCB at exit */

	l_session = a_arg;
	assert ( l_session );
	l_serial = l_session->target;
	assert ( l_serial );


	l_pfd.fd = l_session->sd;
	l_pfd.events = POLLIN;

	l_session->state = T2R$K_STATE_READY;
	l_session->netbuf_dsc = &l_netbuf_dsc;
	l_session->datalen = MODBUS$SZ_MINADU;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &l_session->lastio_ts);


	while ( !g_exit_flag )
		{
		/*
		 * Check session timeout and expiration
		 */
		clock_gettime(CLOCK_MONOTONIC_COARSE, &l_now);

		if ( l_session->state == T2R$K_STATE_READY )				/* Idle timeout of the listener: <idletmo> */
			{
			if ( (l_session->lastio_ts.tv_sec + l_session->listener->idletmo) < l_now.tv_sec)	/* <idletmo> of the listener */
				{
				$PUTMSG_FAO(T2R$__IDLETMO, l_session->sd, l_session->listener->idletmo);
				break;
				}
			}
		else if ( (l_session->lastio_ts.tv_sec + T2R$K_NET_TMO_SEC) < l_now.tv_sec)	/* T2R$K_NET_TMO_SEC for read/write of an ADU */
				{
				$PUTMSG_FAO(T2R$__NETRDTMO, l_session->sd, l_session->datalen, T2R$K_NET_TMO_SEC);
				break;
				}



		/*
		 * Do main work for session ...
		 */
		l_rc = STS$K_SUCCESS;

		switch (l_session->state )
			{
			case	T2R$K_STATE_READY:
				if ( l_pfd.revents & POLLIN )
					l_session->state = T2R$K_STATE_PDU_RECV;

				break;


			case	T2R$K_STATE_PDU_RECV:
				if ( l_pfd.revents & POLLIN )
					{
					if ( T2R$__FULLMBAP  == (l_rc = s_net_mbap_rx(l_session)) )
						{
						$MBAP_PRINT("Rcvd", l_netbuf_dsc.data, l_netbuf_dsc.len);

						l_session->state = T2R$K_STATE_PDU_ENQD;
						}
					else	break;
					}
				else	break;

				/* FALLTHRU */					/* A whole MBAP is in the buffer - process it now */
			case	T2R$K_STATE_PDU_ENQD:
				if ( !(1 & (l_rc = t2r$mbap_2_rtu_pdu (&l_netbuf_dsc, &l_rtu_req_dsc)) ) )
					break;


				if ( !(1 & (l_rc = t2r$tty_exec_req (l_serial, &l_rtu_req_dsc, &l_rtu_resp_dsc, &l_excode))) )
					break;


				if ( l_excode && (l_excode != MODBUS$K_EXCEPTION_GW_NO_PATH) )
					$PUTMSG_FAO(T2R$__REQFAIL, l_session->sd, l_excode, l_excode);

				if ( !(1 & (l_rc = t2r$rtu_pdu_2_mbap (&l_rtu_resp_dsc, &l_netbuf_dsc)) ) )
					break;



				l_session->datalen = 0;				/* Set "sent octets" to zero */
				l_pfd.events |= POLLOUT;			/* Enable checking network socket for output data */
				l_session->state = T2R$K_STATE_PDU_XMIT;	/* State transition to next state */

				/* FALLTHRU */					/* Try to send the answer immediately */
			case T2R$K_STATE_PDU_XMIT:

				$MBAP_PRINT("Sending", l_netbuf_dsc.data, l_netbuf_dsc.len);

				if ( l_session->datalen == l_netbuf_dsc.len )	/* All data is sent to remote TCP client ? */
					{
					$MBAP_PRINT("Sent", l_netbuf_dsc.data, l_netbuf_dsc.len);

					l_pfd.events &= ~POLLOUT;		/* we don't need to check on socket for output */
					l_session->state = T2R$K_STATE_READY;	/* Do transition to initial state */
					l_session->datalen = l_netbuf_dsc.len = 0;

					continue;
					}

				if ( !(1 & (l_rc = s_net_mbap_tx (l_session))) )
					break;
			}


		if ( !(1 & l_rc) )
			{
			$IFTRACE(g_trace, "[#%d] state: %d,  condition code: %#x", l_session->sd, l_session->state, l_rc);
			break;
			}


		/*
		 * Check state of network socket ...
		 */
		if ( 0 > (l_rc = poll(&l_pfd, 1, 3000 /* 3 secs */)) )
			{
			if ( errno == EINTR )
				continue;

			$PUTMSG_FAO(T2R$__NETSYSERR, l_session->sd, "poll()", l_rc, errno);
			break;							/* A persistent error would give a busy loop */
			}
		else if ( l_pfd.revents & (POLLERR | POLLHUP | POLLNVAL) )
			{
			$PUTMSG_FAO(T2R$__NETPOLLEV, l_session->sd, l_rc, l_pfd.revents, errno);
			break;
			}
		else if (l_rc)
			clock_gettime(CLOCK_MONOTONIC_COARSE, &l_session->lastio_ts);
		}



	$PUTMSG_FAO(T2R$__NETDISCN, l_session->sd, &l_session->sk.sin_addr,
		ntohs(l_session->sk.sin_port));


	close(l_session->sd);
	__atomic_sub_fetch(&l_session->listener->sessions, 1, __ATOMIC_RELEASE);	/* Free a slot of <connlm> */
	free( (void *) l_session);

	return	NULL;
}





/*
 *   DESCRIPTION: A thread routine of the connections dispatcher. Waits (by polling the whole
 *	table of the listening sockets) for incoming TCP connection requests and accepts them. A
 *	connection beyond the <connlm> limit of the listener (active sessions) is rejected at once
 *	(CONNLIMIT); otherwise TCP_NODELAY is set, a session context is allocated, counted in
 *	.sessions and a per-session thread is started (s_net_session(), it detaches itself). The
 *	loop is terminated by <g_exit_flag> or by a persistent poll() error.
 *
 *   INPUTS:
 *	a_arg:		Unused, is required by the pthread API only
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	s_pfd_lsnr, g_listeners, g_listeners_nr, g_exit_flag
 *
 *   IMPLICITE OUTPUTS:
 *	g_listeners[].sessions
 *
 *   RETURNS:
 *	NULL - is required by the pthread API only
 */
static void *	s_net_listener( void *a_arg)
{
int	l_rc, l_sd;
struct sockaddr_in	l_sk = {0};
socklen_t l_slen;
T2R$_SESSION	*l_session;
pthread_t	l_tid;
struct timespec	l_now_ts, l_check_ts = {0};
const struct timespec l_period_ts = {.tv_sec = T2R$K_CHECK_MSEC / 1000, .tv_nsec = (T2R$K_CHECK_MSEC % 1000) * 1000000L};

	while ( !g_exit_flag )
		{
		/*
		 * Wait for any new TCP-connection request on all ports ...
		 */
		if ( 0 > (l_rc = poll(s_pfd_lsnr, g_listeners_nr, T2R$K_CHECK_MSEC)) )
			{
			if ( errno == EINTR )
				continue;

			$PUTMSG_FAO(T2R$__SYSERR, "poll(listeners)", l_rc, errno);
			break;								/* A persistent error would give a busy loop */
			}

		/*
		 * The serial lines: a hangup of an idle one (an USB adapter has gone), the reopen of an
		 * absent one - see t2r$tty_check().
		 */
		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( 0 <= __util$cmp_time (&l_now_ts, &l_check_ts) )
			{
			__util$add_time (&l_now_ts, &l_period_ts, &l_check_ts);

			for (int i = 0; i < g_serials_nr; i++ )
				t2r$tty_check (&g_serials[i]);
			}

		if ( !l_rc )
			continue;							/* Timeout - .revents are not updated */


		/*
		 * Run over all listeners and check for POLLIN (TCP-connection request)
		 */
		for (int i = 0; i < g_listeners_nr; i++ )
			{
			if ( s_pfd_lsnr[i].revents & POLLIN )				/* Is there any connection request ? */
				{							/* Accept TCP connection */
				l_slen = sizeof(l_sk);					/* accept() updates it - reset before every call */

				if ( 0 > (l_sd = accept(s_pfd_lsnr[i].fd, (struct sockaddr *) &l_sk, &l_slen)) )
					$PUTMSG_FAO(T2R$__NETSYSERR, s_pfd_lsnr[i].fd, "accept()", l_sd, errno);
				else	{						/* Create new session context */
					$PUTMSG_FAO(T2R$__NETCONN, l_sd, &l_sk.sin_addr, ntohs(l_sk.sin_port),
						s_pfd_lsnr[i].fd);

					/*
					 * <connlm> - a limit of the simultaneous sessions of the listener. Only this
					 * thread does increment the counter, so the check-then-increment is safe;
					 * the sessions do decrement it at their exit.
					 */
					if ( g_listeners[i].connlm <= __atomic_load_n(&g_listeners[i].sessions, __ATOMIC_ACQUIRE) )
						{
						$PUTMSG_FAO(T2R$__CONNLIMIT, l_sd, &l_sk.sin_addr, ntohs(l_sk.sin_port), g_listeners[i].connlm);
						close(l_sd);
						continue;
						}

					if ( (l_rc = setsockopt(l_sd, IPPROTO_TCP, TCP_NODELAY, (char *) &s_one, sizeof(s_one))) )
						$PUTMSG_FAO(T2R$__NETSYSWRN, l_sd, "setsockopt(TCP_NODELAY)", l_rc, errno);


					if ( !(l_session = calloc(1, sizeof(T2R$_SESSION))) )
						{
						close(l_sd);
						$PUTMSG_FAO(T2R$__SYSERR, "calloc(session)", 0, errno);
						continue;
						}

					l_session->sd = l_sd;
					l_session->sk = l_sk;
					l_session->target = g_listeners[i].serial;
					l_session->listener = &g_listeners[i];

					__atomic_add_fetch(&g_listeners[i].sessions, 1, __ATOMIC_ACQ_REL);	/* Before the thread can exit */

											/* Start dedicated thread for session */
					if ( (l_rc = pthread_create(&l_tid, NULL, s_net_session, l_session)) )
						{
						$PUTMSG_FAO(T2R$__SESSFAIL, l_sd, &l_sk.sin_addr, ntohs(l_sk.sin_port), l_rc);
						close(l_sd);
						__atomic_sub_fetch(&g_listeners[i].sessions, 1, __ATOMIC_RELEASE);
						free(l_session);
						}
					}
				}
			}
		}

	return	NULL;
}



/*
 *   DESCRIPTION: Close all the has been opened listening sockets, then close all the has been
 *	opened serial devices (once per device). Is called at the exit path of the main routine.
 *	The dispatcher and the session threads are not joined: they terminate on <g_exit_flag>.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_listeners, g_listeners_nr, g_serials_nr
 *
 *   IMPLICITE OUTPUTS:
 *	s_pfd_lsnr, g_serials
 *
 *   RETURNS:
 *	STS$K_SUCCESS (always; a close error of a device is signalled only)
 */
int t2r$net_stop_listeners (void)
{
	for (int i = 0; i < g_listeners_nr; i++ )
		{
		if ( 0 > s_pfd_lsnr[i].fd )						/* This one has not been started */
			continue;

		$PUTMSG_FAO(T2R$__LSNRABRT, s_pfd_lsnr[i].fd, &g_listeners[i].sk.sin_addr, ntohs(g_listeners[i].sk.sin_port));

		close(s_pfd_lsnr[i].fd);
		s_pfd_lsnr[i].fd = -1;
		}

	/*
	 * Serial devices are closed over the table of serials: a few listeners are allowed to share
	 * a single serial device, so a per-listener close would try to close it more than once.
	 */
	for (int i = 0; i < g_serials_nr; i++ )
		{
		if ( g_serials[i].state == T2R$K_STATE_IDLE )
			continue;

		t2r$tty_close (&g_serials[i]);						/* A failure is signalled inside */
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Start the network subsystem: for every record of the listeners table open a
 *	TCP socket, bind it to the has been configured address:port, listen() with
 *	T2R$K_LISTEN_BACKLOG, open the target serial device, publish the socket to the poll()
 *	array of the dispatcher and finally start the dispatcher thread. A listener whose bind()/
 *	listen() fails or whose target device cannot be opened is skipped with a diagnostic.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_listeners, g_listeners_nr
 *
 *   IMPLICITE OUTPUTS:
 *	s_pfd_lsnr, g_listeners[].fd, g_serials (the opened devices)
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	T2R$__NOLSNRUP	- no one listener has been started
 *	T2R$__SYSERR	- socket() or pthread_create() failed
 */
int t2r$net_start_listeners (void)
{
int	l_rc = STS$K_ERROR, l_sd, l_one = 1, l_count;
T2R$_LISTENER	*l_listener;
const socklen_t l_slen = sizeof(struct sockaddr_in);
pthread_t	l_tid;


	for (int i = 0; i < T2R$K_MAX_LISTENERS; i++)				/* poll() ignores a negative descriptor */
		s_pfd_lsnr[i].fd = -1;

	l_count = 0;

	for (int i = 0; i < g_listeners_nr; i++)
		{
		l_listener = &g_listeners[i];


		if ( 0 > (l_sd = socket(AF_INET, ((l_listener->proto == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM), 0)) )
				return	$PUTMSG_FAO(T2R$__SYSERR, "socket()", l_sd, errno);

		if( 0 > setsockopt(l_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&l_one, sizeof(l_one))  )
			$PUTMSG_FAO(T2R$__NETSYSWRN, l_sd, "setsockopt(SO_REUSEADDR)", -1, errno);

		if ( 0 > bind(l_sd, (struct sockaddr*) &l_listener->sk, l_slen) )
			{
			close(l_sd);
			$PUTMSG_FAO(T2R$__LSNRERR, &l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port),
				"bind()", errno);

			continue;
			}

		if ( 0 > (l_rc = listen(l_sd, T2R$K_LISTEN_BACKLOG)) )
			{
			close(l_sd);
			$PUTMSG_FAO(T2R$__LSNRERR, &l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port),
				"listen()", errno);

			continue;
			}

		/*
		 * A target device is opened BEFORE the socket is published to the poll() array:
		 * we do not want to accept clients for a leg which does not work. An absent device
		 * (DEVABSENT: an USB adapter not plugged in yet) is not fatal - it is opened later.
		 */
		if ( !(1 & (l_rc = t2r$tty_open (l_listener->serial))) && (l_rc != T2R$__DEVABSENT) )
			{
			close(l_sd);							/* Don't leak the descriptor */
			$PUTMSG_FAO(T2R$__TGTOPNERR, &l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port), l_listener->serial->devname);

			continue;
			}


		l_listener->fd = l_sd;
		s_pfd_lsnr[i].fd = l_sd;
		s_pfd_lsnr[i].events = POLLIN;


		l_count++;

		$PUTMSG_FAO(T2R$__LSNRRDY, s_pfd_lsnr[i].fd, &l_listener->sk.sin_addr,
			ntohs(l_listener->sk.sin_port), l_listener->serial->devname);
		}


	if ( !l_count )
		return	$PUTMSG_FAO(T2R$__NOLSNRUP);


	if ( (l_rc = pthread_create(&l_tid, NULL, s_net_listener, NULL)) )
		return	$PUTMSG_FAO(T2R$__SYSERR, "pthread_create(listener)", l_rc, errno);



	return	STS$K_SUCCESS;
}
