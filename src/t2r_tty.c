#define	__MODULE__	"T2R-TTY"
#define	__IDENT__	"X.00-13ECO07"
#define	__REV__		"00.13.07"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**
**  DESCRIPTION: This module covers TTY related functionality. Serial line open and initialization, read\write and timeout processing.
**	Send MODBUS PDU to serial port and read with CRC checking.
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
**				s_tty_rtu_rx(): rewritten - honest t3.5 end-of-frame criteria restarted on
**				every read chunk, an overall deadline, EAGAIN/EINTR are not fatal anymore,
**				CLOCK_MONOTONIC instead of the _COARSE one, an upper frame length check,
**				the CRC trailer is assembled octet-by-octet;
**				s_tty_rtu_tx(): tcflush() of a stale input before TX, tcdrain() after TX,
**				the CRC trailer is sent octet-by-octet, fixed diagnostic arguments;
**				s_tty_open(): TIOCSRS485 (was TIOCGRS485 - the RS485 mode was never set);
**				s_add_ts_to_pdu(): fixed 64->4x16 bits packing shifts (48/32/16/0, were
**				overlapping 24/16/8/0), the lambda is replaced by s_put_be16() routine,
**				the timezone offset is reported in signed minutes now;
**				t2r$tty_open(): the READY state is set on successful open only;
**				t2r$tty_exec_req(): fixed swapped $LOG arguments (SIGSEGV);
**				fixed an undeclared <tspeed> in s_tty_speed2bits();
**				removed dead code: s_tty_delay(), th_func_t, s_pfd_lsnr[], <linux/i2c.h>.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The critical (for the operational troubleshooting)
**				diagnostics are signalled via $PUTMSG_FAO by the coded catalogue messages
**				now: DEVOPNERR, NOANSWER, FRAMETMO, BADFRAME, CRC16ERR, EXCRPT.
**
**	14-OCT-2026	RRL	X.00-10 / REV: 00.10.00 - s_tty_rtu_rx(): the end of the answer is detected by
**				its expected length (s_rtu_resp_len(): FC01-06, 0F, 10, 17 and the exception
**				answer), t3.5 is used for the other functions only: the USB-to-serial
**				adapters (FTDI) deliver the answer by chunks with gaps up to 16 msec, the
**				frame was cut by the t3.5 silence - CRC16ERR, BADFRAME.
**
**	14-OCT-2026	RRL	X.00-11 / REV: 00.11.00 - The logging is FAO-only: every $LOG() is replaced by
**				$PUTMSG_FAO() with a coded message (TTYSYSERR, DEVSYSERR, RESPMISM, ...);
**				s_tty_open(): the RS485 mode is set by TIOCSRS485 (the second ioctl() was
**				TIOCGRS485 again - the mode has never been switched on).
**
**	14-OCT-2026	RRL	X.00-12ECO01 / REV: 00.12.01 - The routine headers are brought in line with the code
**				(t2r$tty_close() had a copy of the t2r$tty_open() header, s_tty_flush() is a
**				fixed t3.5 window, the RETURNS sections name the real status codes).
**
**	14-OCT-2026	RRL	X.00-13ECO01 / REV: 00.13.01 - The device lock could be held forever: tcdrain()
**				has no time limit, an USB-to-serial adapter which stopped shifting out the
**				data hung the session thread in it with the lock taken - every other client
**				of the line got DEVLOCKERR/DEVBUSY until the restart, the shutdown failed by
**				DEVCLSERR. s_tty_tx_drain() replaces tcdrain(): TIOCOUTQ is polled up to the
**				frame air time plus the answer timeout (TXDRAINTMO, the output is flushed);
**				s_tty_open(): CRTSCTS is cleared explicitly (was inherited from the port).
**
**	28-OCT-2026	RRL	X.00-13ECO06 / REV: 00.13.06 - s_put_be16() stores the registers byte by byte:
**				the values of the answer PDU start at an odd offset (slave, fncode, bc), the
**				16 bits store there traps on MIPS (an unaligned access, fixed up by the kernel
**				at best) - the time stamp pseudo device on the OpenWrt/MIPS boards. The
**				seconds are shifted as int64_t: time_t of a 32 bits glibc (mipsel, armhf)
**				is 32 bits, a shift by 48 was undefined - R0, R1 got garbage.
**
**	02-NOV-2026	CLD	X.00-13ECO07 / REV: 00.13.07 - s_tty_open(): the input is binary-clean now -
**				IGNBRK, BRKINT, PARMRK, ISTRIP, INLCR, IGNCR, ICRNL, IEXTEN and ECHONL are
**				cleared (were inherited from the port: with ICRNL, the default of a fresh
**				ttyS/ttyUSB after a boot or a replug, a 0x0D octet of the answer became 0x0A -
**				CRC16ERR on ~3% of the requests, field report); after the set up the line
**				settles for T2R$K_SETTLE_MSEC and both queues are flushed (the first request
**				after the start was left unanswered on the FT4232H ports).
**--
*/
#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<unistd.h>
#include	<poll.h>
#include	<termios.h>
#include	<fcntl.h>
#include	<termios.h>
#include	<sys/uio.h>
#include	<linux/serial.h>
#include	<sys/ioctl.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


extern int	g_exit_flag, g_trace;



/*
 *   DESCRIPTION: Get an exclusive access to the serial device: a MODBUS RTU line is strictly a
 *	half duplex request-answer bus, so only one network session at a time is allowed to talk
 *	to it. A timed lock is used (13 seconds, 3 attempts) to detect a stuck holder instead of
 *	hanging forever. Every timed out attempt is signalled by DEVSYSWRN.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line, .lock - the guarding mutex
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the lock has been taken
 *	T2R$__DEVLOCKERR - the lock has not been taken after 3 attempts
 *	T2R$__SYSERR	- clock_gettime() failed
 */
static inline int s_tty_lock (
		T2R$_SERIAL	*a_serial
		)
{
struct timespec l_now, l_tmo = {.tv_sec = 13}, l_etime;
int	l_rc;

	for (int i = 0; i < 3; i++)
		{
		if ( (l_rc = clock_gettime(CLOCK_REALTIME_COARSE, &l_now)) )
			return	$PUTMSG_FAO(T2R$__SYSERR, "clock_gettime()", l_rc, errno);

		__util$add_time (&l_now, &l_tmo, &l_etime);

		if ( !(l_rc = pthread_mutex_timedlock(&a_serial->lock, &l_etime)) )
			{
			//$IFTRACE(g_trace, "Got exclusive access to <%s> ...", a_serial->devname);
			return	STS$K_SUCCESS;
			}

		$PUTMSG_FAO(T2R$__DEVSYSWRN, a_serial->devname, "pthread_mutex_timedlock()", l_rc, errno);
		}


	return	$PUTMSG_FAO(T2R$__DEVLOCKERR, a_serial->devname, 3);
}


/*
 *   DESCRIPTION: Release the exclusive access to the serial device which has been taken by
 *	s_tty_lock().
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line, .lock - the guarding mutex
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition code
 */
static inline int s_tty_unlock (
		T2R$_SERIAL	*a_serial
		)
{
int	l_rc;

	if ( (l_rc = pthread_mutex_unlock(&a_serial->lock)) )
		return	$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "pthread_mutex_unlock()", l_rc, errno);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Wait until the has been written frame has left the UART - a bounded replacement
 *	of tcdrain(). tcdrain() has no time limit: an USB-to-serial adapter which stops shifting out
 *	the data (an USB hiccup, a lost CTS) blocks the caller forever, and the caller holds the
 *	device lock - the whole line is dead until the restart. Here:
 *	1. The kernel output queue (TIOCOUTQ) is polled by 1 msec steps until it is empty.
 *	2. The rest of the frame air time (the octets in the adapter's own FIFO) is waited out:
 *	   the frame cannot leave earlier than a_len * (start + data + parity + stop bits) / baud
 *	   after the writev().
 *	The step 1 is limited by the air time plus the answer timeout; on the timeout the output
 *	queue is discarded (tcflush(TCOFLUSH)) and TXDRAINTMO is signalled. A failed TIOCOUTQ is
 *	signalled by TTYSYSWRN, then the air time only is waited out.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line: .fd, .baud, .databits, .parity, .stopbits, .anstmo_msec
 *	a_len:		A length of the has been written frame in octets
 *	a_start_ts:	CLOCK_MONOTONIC time just before the writev()
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS, T2R$__TXDRAINTMO
 */
static int	s_tty_tx_drain (
	const T2R$_SERIAL	*a_serial,
			int	a_len,
	const struct timespec	*a_start_ts
			)
{
int	l_rc, l_outq, l_bits;
int64_t	l_air_usec;
struct timespec	l_now_ts, l_end_ts, l_deadline_ts, l_tmo_ts, l_step_ts = {.tv_nsec = 1000000L};

	l_bits = 1 + a_serial->databits + (a_serial->parity != 'N') + a_serial->stopbits;	/* Bits per character */
	l_air_usec = ((int64_t) a_len * l_bits * 1000000LL) / a_serial->baud;

	l_tmo_ts.tv_sec = l_air_usec / 1000000LL;
	l_tmo_ts.tv_nsec = (l_air_usec % 1000000LL) * 1000L;
	__util$add_time (a_start_ts, &l_tmo_ts, &l_end_ts);				/* The last bit leaves the wire */

	l_tmo_ts.tv_sec = a_serial->anstmo_msec / 1000;
	l_tmo_ts.tv_nsec = (a_serial->anstmo_msec % 1000) * 1000000L;
	__util$add_time (&l_end_ts, &l_tmo_ts, &l_deadline_ts);

	for ( l_outq = 1; l_outq; nanosleep(&l_step_ts, NULL) )
		{
		if ( 0 > (l_rc = ioctl(a_serial->fd, TIOCOUTQ, &l_outq)) )
			{
			$PUTMSG_FAO(T2R$__TTYSYSWRN, a_serial->fd, a_serial->devname, "ioctl(TIOCOUTQ)", l_rc, errno);
			break;
			}

		if ( !l_outq )
			break;

		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( 0 < __util$cmp_time (&l_now_ts, &l_deadline_ts) )
			{
			tcflush(a_serial->fd, TCOFLUSH);

			return	$PUTMSG_FAO(T2R$__TXDRAINTMO, a_serial->fd, a_serial->devname,
					(unsigned) (l_air_usec / 1000) + a_serial->anstmo_msec, l_outq);
			}
		}

	/* The kernel queue is empty: wait out the rest of the air time */
	while ( EINTR == clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &l_end_ts, NULL) );

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Send a request PDU over the serial line:
 *	- an empty PDU is skipped (TXNODATA is signalled, success is returned)
 *	- compute CRC16 (the low octet goes first on the wire)
 *	- flush a stale input (tcflush(TCIFLUSH))
 *	- wait the inter-PDU interval (t3.5), writev() PDU + CRC16, s_tty_tx_drain()
 *	- wait the inter-PDU interval (t3.5) again
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_pdu:	A buffer with the PDU RTU to be sent
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS, T2R$__XMITERR, T2R$__XMITSHORT, T2R$__TXDRAINTMO
 */
static int	s_tty_rtu_tx (
	const T2R$_SERIAL	*a_serial,
		T2R$_DESC	*a_req_pdu
		)
{
int	l_rc, l_len;
uint16_t l_crc;
uint8_t	l_crcbuf[2];
enum {	IOV$K_PDU, IOV$K_CRC, IOV$K_MAX};
struct iovec	l_iov[IOV$K_MAX];
struct timespec l_inter_pdu_ts, l_start_ts;


	if ( !a_req_pdu->len )
		return	$PUTMSG_FAO(T2R$__TXNODATA, a_serial->fd, a_serial->devname),
				STS$K_SUCCESS;

	l_len = 0;									/* Total lenght of data to be sent */

	l_iov[IOV$K_PDU].iov_base = a_req_pdu->data;
	l_len += l_iov[IOV$K_PDU].iov_len = a_req_pdu->len;

	l_crc = s_modbus_crc_calculate (l_iov[IOV$K_PDU].iov_base, l_iov[IOV$K_PDU].iov_len);

	l_crcbuf[0] = (uint8_t) (l_crc & 0xFF);					/* Lo octet goes first on the wire */
	l_crcbuf[1] = (uint8_t) (l_crc >> 8);

	l_iov[IOV$K_CRC].iov_base = l_crcbuf;
	l_len += l_iov[IOV$K_CRC].iov_len = sizeof(l_crcbuf);

	/*
	 * Drop a garbage which could be left in the input FIFO by a previous timed out exchange:
	 * otherwise it will be glued to a head of the coming answer and will break the CRC of a
	 * perfectly correct frame.
	 */
	if ( 0 > (l_rc = tcflush(a_serial->fd, TCIFLUSH)) )
		$PUTMSG_FAO(T2R$__TTYSYSWRN, a_serial->fd, a_serial->devname, "tcflush()", l_rc, errno);


											/* Predelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );


	clock_gettime(CLOCK_MONOTONIC, &l_start_ts);					/* The air time is counted from here */

	if ( 0 > (l_rc = writev(a_serial->fd, l_iov,  IOV$K_MAX)) )
		l_rc = $PUTMSG_FAO(T2R$__XMITERR, a_serial->fd, a_serial->devname, l_len, l_rc, errno);
	else if ( l_rc != l_len )
		l_rc = $PUTMSG_FAO(T2R$__XMITSHORT, a_serial->fd, a_serial->devname, l_rc, l_len, errno);
	else	{
		/*
		 * Wait until the UART has really shifted out the last octet: the post-delay and the
		 * answer timeout are counted from the end of the request on the wire.
		 */
		l_rc = s_tty_tx_drain (a_serial, l_len, &l_start_ts);
		}


											/* Postdelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );

	$RTU_PRINT("Sent", a_req_pdu->data, a_req_pdu->len, l_crc);

	return	l_rc;
}


/*
 *   DESCRIPTION: Compute a length of the RTU answer (with the CRC16 trailer) which is expected to
 *	the given request. The length is known in advance for the most used functions; for the rest
 *	of them the end of the frame is detected by the t3.5 silent interval only.
 *
 *   INPUTS:
 *	a_req_pdu:	A descriptor of the request PDU: slave, function code, start address, quantity
 *
 *   OUTPUTS:
 *	a_len:		A length of the normal (not an exception) answer in octets, CRC16 is included
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the length is known
 *	STS$K_WARN	- the length cannot be predicted, use the t3.5 criteria
 */
static int	s_rtu_resp_len (
	const T2R$_DESC	*a_req_pdu,
		int	*a_len
		)
{
const MODBUS_REQ_T	*l_req = (const MODBUS_REQ_T *) a_req_pdu->data;
int	l_qty;

	*a_len = 0;

	if ( a_req_pdu->len < (int) (sizeof(MODBUS_REQ_T)) )			/* slave + fncode + sa + qty */
		return	STS$K_WARN;

	l_qty = ntohs(l_req->qty);

	switch ( l_req->fncode )
		{
		case	MODBUS$K_FN_READ_COILS:					/* slave + fncode + bc + bits + CRC16 */
		case	MODBUS$K_FN_READ_DISCRETE_INPUTS:
			*a_len = MODBUS$SZ_PDUHDR + (l_qty + 7) / 8 + 2;
			break;

		case	MODBUS$K_FN_READ_HOLDING_REGISTERS:			/* slave + fncode + bc + words + CRC16 */
		case	MODBUS$K_FN_READ_INPUT_REGISTERS:
		case	MODBUS$K_FN_READ_WRITE_MULTIPLE_REGISTERS:		/* qty is the read quantity here */
			*a_len = MODBUS$SZ_PDUHDR + 2 * l_qty + 2;
			break;

		case	MODBUS$K_FN_WRITE_SINGLE_COIL:				/* An echo: slave + fncode + sa + qty/value + CRC16 */
		case	MODBUS$K_FN_WRITE_SINGLE_REGISTER:
		case	MODBUS$K_FN_WRITE_MULTIPLE_COILS:
		case	MODBUS$K_FN_WRITE_MULTIPLE_REGISTERS:
			*a_len = (int) sizeof(MODBUS_REQ_T) + 2;
			break;

		default:
			return	STS$K_WARN;
		}

	if ( (*a_len < MODBUS$SZ_MINRTU) || (*a_len > MODBUS$SZ_MAXPDU) )	/* A senseless quantity - let the t3.5 decide */
		return	*a_len = 0, STS$K_WARN;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Read a whole RTU frame from the serial line, check a framing and the CRC16.
 *
 *	1. Wait for a first octet of the answer by using a long timeout: a target device is allowed
 *	   to take a time to process the request.
 *	2. Accumulate octets until the expected length of the answer is collected (see
 *	   s_rtu_resp_len(); an exception answer is always 5 octets) - the USB-to-serial adapters
 *	   (FTDI: latency_timer = 16 msec by default) deliver the answer by chunks with the gaps
 *	   which are much longer than t3.5, so the silence must not cut the frame.
 *	   When the length cannot be predicted - until the line keeps silence for the t3.5
 *	   interval, the end-of-frame criteria of the MODBUS over Serial Line specification. The
 *	   interval is restarted on every successfully read chunk of the data.
 *	3. The rest of the frame must be collected within the answer timeout counted from the
 *	   first octet; otherwise FRAMETMO is signalled and the partial data is validated (it
 *	   fails BADFRAME/CRC16ERR). Octets beyond the expected length are dropped, a full
 *	   buffer stops the reading (RXBUFFULL).
 *	4. Check the frame length, the CRC16, and that the slave and the function code match
 *	   the request.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_pdu:	A descriptor of the request PDU - is used to match the slave and the function code
 *	a_resp_pdu:	.data/.sz - the receive buffer
 *
 *   IMPLICIT INPUTS:
 *	g_exit_flag
 *
 *   OUTPUTS:
 *	a_resp_pdu:	.data/.len - the response PDU, the CRC16 trailer is stripped off
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or NOANSWER, TTYSYSERR, RXNODATA, BADFRAME, CRC16ERR, RESPMISM
 */
static int	s_tty_rtu_rx (
	const T2R$_SERIAL	*a_serial,
		const T2R$_DESC	*a_req_pdu,
		T2R$_DESC	*a_resp_pdu
		)
{
int	l_rc, l_tmo_msec, l_space, l_explen, l_normlen;
uint16_t	l_crc, l_crc_rcvd;
struct pollfd	l_pfd = {.fd = a_serial->fd, .events = POLLIN};
struct timespec	l_now_ts, l_silence_ts, l_deadline_ts;
MODBUS_PDU_T	*l_req_pdu, *l_resp_pdu;
uint8_t	*l_data;

	a_resp_pdu->len = 0;

	s_rtu_resp_len (a_req_pdu, &l_normlen);					/* 0 - unknown, use t3.5 */
	l_explen = l_normlen;

	/*
	 * Wait for a first octet of the answer
	 */
	if ( 0 > (l_rc = poll(&l_pfd, 1, a_serial->anstmo_msec)) )
		{
		if ( errno != EINTR )
			return	$PUTMSG_FAO(T2R$__TTYSYSERR, a_serial->fd, a_serial->devname, "poll()", l_rc, errno);
		}
	else if ( !l_rc )
		return	$PUTMSG_FAO(T2R$__NOANSWER, a_serial->fd, a_serial->devname, a_serial->anstmo_msec);

	/*
	 * An upper limit for the whole frame: we never stay here longer than the answer timeout.
	 */
	clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

	l_deadline_ts.tv_sec = a_serial->anstmo_msec / 1000;
	l_deadline_ts.tv_nsec = (a_serial->anstmo_msec % 1000) * 1000000L;
	__util$add_time (&l_now_ts, &l_deadline_ts, &l_deadline_ts);

	/*
	 * The t3.5 silent interval - it is restarted on every successfully read chunk of the data.
	 */
	__util$add_time (&l_now_ts, &a_serial->inter_pdu_ts, &l_silence_ts);

	l_tmo_msec = 1 + (a_serial->inter_pdu_usec / 1000);			/* At least 1 msec - poll() granularity */

	while ( !g_exit_flag )
		{
		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( l_explen && (a_resp_pdu->len >= l_explen) )
			break;							/* The expected length is collected */

		if ( !l_explen && a_resp_pdu->len && (0 < __util$cmp_time (&l_now_ts, &l_silence_ts)) )
			break;							/* t3.5 of silence - the frame is complete */

		if ( 0 < __util$cmp_time (&l_now_ts, &l_deadline_ts) )
			{
			$PUTMSG_FAO(T2R$__FRAMETMO, a_serial->fd, a_serial->devname, a_serial->anstmo_msec,
				a_resp_pdu->len);
			break;
			}

		if ( 0 >= (l_space = a_resp_pdu->sz - a_resp_pdu->len) )
			{
			$PUTMSG_FAO(T2R$__RXBUFFULL, a_serial->fd, a_serial->devname, a_resp_pdu->len);
			break;
			}

		if ( 0 > (l_rc = poll(&l_pfd, 1, l_tmo_msec)) )
			{
			if ( errno == EINTR )
				continue;

			return	$PUTMSG_FAO(T2R$__TTYSYSERR, a_serial->fd, a_serial->devname, "poll()", l_rc, errno);
			}

		if ( !l_rc )
			continue;						/* Nothing yet - let the t3.5 check to decide */

		if ( 0 > (l_rc = read(a_serial->fd, a_resp_pdu->data + a_resp_pdu->len, l_space)) )
			{
									/* The port is opened with O_NDELAY: these three
									   errno-s are a normal flow, not a failure	*/
			if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR) )
				continue;

			return	$PUTMSG_FAO(T2R$__TTYSYSERR, a_serial->fd, a_serial->devname, "read()", l_rc, errno);
			}

		if ( !l_rc )
			continue;

		a_resp_pdu->len += l_rc;					/* Adjust data counter in the input buffer */

		if ( l_normlen && (a_resp_pdu->len >= 2) )			/* An exception answer is shorter */
			l_explen = (((MODBUS_PDU_T *) a_resp_pdu->data)->fncode & MODBUS$M_FN_EXCEPTION)
				? (int) sizeof(MODBUS_EXC_T) + 2 : l_normlen;

		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);			/* Restart the t3.5 silent interval */
		__util$add_time (&l_now_ts, &a_serial->inter_pdu_ts, &l_silence_ts);
		}

	if ( l_explen && (a_resp_pdu->len > l_explen) )				/* A trailing garbage in the last chunk - the */
		a_resp_pdu->len = l_explen;					/* input is flushed before the next request */

	/*
	 * Validate a framing of the has been received data
	 */
	if ( !a_resp_pdu->len )
		return	$PUTMSG_FAO(T2R$__RXNODATA, a_serial->fd, a_serial->devname);

	if ( (a_resp_pdu->len < MODBUS$SZ_MINRTU) || (a_resp_pdu->len > MODBUS$SZ_MAXPDU) )
		return	$PUTMSG_FAO(T2R$__BADFRAME, a_serial->fd, a_serial->devname, a_resp_pdu->len,
				MODBUS$SZ_MINRTU, MODBUS$SZ_MAXPDU);

	$RTU_PRINT("Rcvd", a_resp_pdu->data, a_resp_pdu->len, 0);

	/*
	 * Check the CRC16 trailer: the low octet goes first on the wire, so the value is assembled
	 * octet-by-octet - it is neither alignment nor byte order dependent.
	 */
	l_data = (uint8_t *) a_resp_pdu->data;

	l_crc_rcvd = (uint16_t) l_data[a_resp_pdu->len - 2]
		| ((uint16_t) l_data[a_resp_pdu->len - 1] << 8);

	l_crc = s_modbus_crc_calculate (l_data, a_resp_pdu->len - 2);

	if ( l_crc != l_crc_rcvd )
		return	$PUTMSG_FAO(T2R$__CRC16ERR, a_serial->fd, a_serial->devname, l_crc_rcvd, l_crc);

	/*
	 * Check that the answer belongs to the has been sent request
	 */
	l_req_pdu = (MODBUS_PDU_T *) a_req_pdu->data;
	l_resp_pdu = (MODBUS_PDU_T *) a_resp_pdu->data;

	if ( (l_req_pdu->slave != l_resp_pdu->slave)
		|| (l_req_pdu->fncode != (l_resp_pdu->fncode & (~MODBUS$M_FN_EXCEPTION))) )
		return	$PUTMSG_FAO(T2R$__RESPMISM, a_serial->fd, a_serial->devname, l_req_pdu->slave, l_resp_pdu->slave,
				l_req_pdu->fncode, l_resp_pdu->fncode);

	a_resp_pdu->len -= 2;							/* Reduce a length of data for CRC16 */

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Translate line rate to internal representative. In error case return <B9600>
 *
 *   INPUTS:
 *	a_speed:	A data speed in bauds
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	<line_speed>
 */
static	speed_t s_tty_speed2bits(int a_speed)
{
speed_t l_speed;

	switch (a_speed)
		{
		#if defined(B50)
		case 50:	l_speed = B50;		break;
		#endif

		#if defined(B75)
		case 75:	l_speed = B75;		break;
		#endif

		#if defined(B110)
		case 110:	l_speed = B110;		break;
		#endif

		#if defined(B134)
		case 134:	l_speed = B134;		break;
		#endif

		#if defined(B150)
		case 150:	l_speed = B150;		break;
		#endif

		#if defined(B200)
		case 200:	l_speed = B200;		break;
		#endif

		#if defined(B300)
		case 300:	l_speed = B300;		break;
		#endif

		#if defined(B600)
		case 600:	l_speed = B600;		break;
		#endif

		#if defined(B1200)
		case 1200:	l_speed = B1200;	break;
		#endif

		#if defined(B1800)
		case 1800:	l_speed = B1800;	break;
		#endif

		#if defined(B2400)
		case 2400:	l_speed = B2400;	break;
		#endif

		#if defined(B4800)
		case 4800:	l_speed = B4800;	break;
		#endif

		#if defined(B7200)
		case 7200:	l_speed = B7200;		break;
		#endif

		#if defined(B9600)
		case 9600:	l_speed = B9600;	break;
		#endif

		#if defined(B12000)
		case 12000:	l_speed = B12000;	break;
		#endif

		#if defined(B14400)
		case 14400:	l_speed = B14400;	break;
		#endif

		#if defined(B19200)
		case 19200:	l_speed = B19200;	break;
		#elif defined(EXTA)
		case 19200:	l_speed = EXTA;		break;
		#endif

		#if defined(B38400)
		case 38400:	l_speed = B38400;	break;
		#elif defined(EXTB)
		case 38400:	l_speed = EXTB;		break;
		#endif

		#if defined(B57600)
		case 57600:	l_speed = B57600;	break;
		#endif

		#if defined(B115200)
		case 115200:	l_speed = B115200;	break;
		#endif

		#if defined(B230400)
		case 230400:	l_speed = B230400;	break;
		#endif

		#if defined(B460800)
		case 460800:	l_speed = B460800;	break;
		#endif

		#if defined(B500000)
		case 500000:	l_speed = B500000;	break;
		#endif

		#if defined(B576000)
		case 576000:	l_speed = B576000;	break;
		#endif

		#if defined(B921600)
		case 921600:	l_speed = B921600;	break;
		#endif

		#if defined(B1000000)
		case 1000000:	l_speed = B1000000;	break;
		#endif

		#if defined(B1152000)
		case 1152000:	l_speed = B1152000;	break;
		#endif

		#if defined(B1500000)
		case 1500000:	l_speed = B1500000;	break;
		#endif

		#if defined(B2000000)
		case 2000000:	l_speed = B2000000;	break;
		#endif

		#if defined(B2500000)
		case 2500000:	l_speed = B2500000;	break;
		#endif

		#if defined(B3000000)
		case 3000000:	l_speed = B3000000;	break;
		#endif

		#if defined(B3500000)
		case 3500000:	l_speed = B3500000;	break;
		#endif

		#if defined(B4000000)
		case 4000000:	l_speed = B4000000;	break;
		#endif

		default:
			$PUTMSG_FAO(T2R$__BADSPEED, a_speed);
			l_speed = B9600;
		}

	return l_speed;
}

/*
 *   DESCRIPTION: Close the serial device: restore the terminal attributes which has been saved
 *	at the open time, close the descriptor. A not opened device is silently ignored.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line; .fd - descriptor, .savedtios - the terminal
 *			attributes as they were before s_tty_open()
 *
 *   OUTPUTS:
 *	a_serial:	.fd is closed and set to -1 (left untouched if it is not a TTY)
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a tcsetattr(TCSAFLUSH) failure is signalled only (DEVSYSERR)
 *	T2R$__NOTTTY	- the descriptor is not a TTY, nothing is closed
 */
static int s_tty_close (
	T2R$_SERIAL	*a_serial
		)
{
int	l_rc;

	if (a_serial->fd < 0)
		return	STS$K_SUCCESS;

	if ( !(l_rc = isatty(a_serial->fd)) )
		return	$PUTMSG_FAO(T2R$__NOTTTY, a_serial->fd, l_rc, a_serial->devname);

	if ( 0 > (l_rc = tcsetattr(a_serial->fd, TCSAFLUSH, &a_serial->savedtios)))
		$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "tcsetattr()", l_rc, errno);

	close(a_serial->fd);
	a_serial->fd = -1;


	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Open (O_RDWR|O_NOCTTY|O_NDELAY|O_EXCL|O_CLOEXEC) and set up a serial port for
 *	the RTU communications: the configured rate/data bits (5..8)/parity/stop bits, raw
 *	non-canonical I/O without a software flow control, VMIN = VTIME = 0. If HAVE_TIOCRS485
 *	is defined and T2R$M_SERIAL_RS485 is set, the RS-485 mode is switched on by TIOCGRS485 +
 *	TIOCSRS485; a failure there is not fatal (RS485OFF, the flag is cleared). The input is
 *	binary-clean (no CR/NL translation, no stripping/marking). At the end the line settles for
 *	T2R$K_SETTLE_MSEC and both queues are flushed.
 *	Stolen and adopted from LIBMODBUS\MODBUS-RTU.C.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line; .devname and the line parameters (.baud,
 *			.databits, .parity, .stopbits, .flags) must be filled by the caller
 *
 *   OUTPUTS:
 *	a_serial:	.fd - a descriptor of the has been opened device (-1 on a failure),
 *			.savedtios - the original terminal attributes to be restored at close,
 *			.tios - the applied (working) terminal attributes,
 *			.flags - T2R$M_SERIAL_RS485 is cleared if the RS-485 mode cannot be set
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	T2R$__DEVOPNERR	- open() failed
 *	T2R$__DEVSYSERR	- tcgetattr()/cfset*speed()/tcsetattr() failed (the descriptor is closed)
 */
static int s_tty_open (
			       T2R$_SERIAL	*a_serial
			       )
{
speed_t l_speed = -1;
int	l_flags = 0, l_rc;
struct timespec l_settle_ts = {.tv_nsec = T2R$K_SETTLE_MSEC * 1000000L};

	l_flags = O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL;

#ifdef O_CLOEXEC
	l_flags |= O_CLOEXEC;
#endif

	if ( 0 > (a_serial->fd = open(a_serial->devname, l_flags)) )
		return	$PUTMSG_FAO(T2R$__DEVOPNERR, a_serial->devname, errno);

	memset(&a_serial->savedtios, 0, sizeof(struct termios));

	if ( (l_rc = tcgetattr(a_serial->fd , &a_serial->savedtios)) )
		return	close(a_serial->fd), a_serial->fd = -1,
			$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "tcgetattr()", l_rc, errno);

	a_serial->tios = a_serial->savedtios;						/* Make local copy */



	/*
	 * Set the baud rate
	 */
	l_speed = s_tty_speed2bits (a_serial->baud);					/* Translate bauds to internal representative */

	if ( 0 > ( l_rc = cfsetispeed(&a_serial->tios, l_speed)) )			/* Set line rate */
		return	close(a_serial->fd ), a_serial->fd = -1,
			$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "cfsetispeed()", l_rc, errno);

	if ( 0 > ( l_rc = cfsetospeed(&a_serial->tios, l_speed)) )			/* Set line rate */
		return	close(a_serial->fd ), a_serial->fd = -1,
			$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "cfsetospeed()", l_rc, errno);


	/* C_CFLAG      Control options
	   CLOCAL       Local line - do not change "owner" of port
	   CREAD        Enable receiver
	*/
	a_serial->tios.c_cflag |= (CREAD | CLOCAL);

	/* CSIZE, HUPCL, CRTSCTS (hardware flow control) */

	/* Set data bits (5, 6, 7, 8 bits)
	CSIZE        Bit mask for data bits
	*/
	a_serial->tios.c_cflag &= ~CSIZE;

	switch (a_serial->databits)							/* Translate databits to internal representative */
		{
		case 5:	a_serial->tios.c_cflag |= CS5;	break;
		case 6:	a_serial->tios.c_cflag |= CS6;	break;
		case 7:	a_serial->tios.c_cflag |= CS7;	break;

		case 8:
		default:
		a_serial->tios.c_cflag |= CS8;	break;
		}

	/* Stop bit (1 or 2) */
	if (a_serial->stopbits == 1)
		a_serial->tios.c_cflag &=~ CSTOPB;	/* 1 */
	else	a_serial->tios.c_cflag |= CSTOPB;	/* 2 */


	/* PARENB       Enable parity bit
	   PARODD       Use odd parity instead of even */
	if (a_serial->parity == 'N')
		{ /* None */ a_serial->tios.c_cflag &=~ PARENB;   }
	else if (a_serial->parity == 'E')
		{/* Even */
		a_serial->tios.c_cflag |= PARENB;
		a_serial->tios.c_cflag &=~ PARODD;
		}
	else	{ /* Odd */
		a_serial->tios.c_cflag |= PARENB;
		a_serial->tios.c_cflag |= PARODD;
		}

	if (a_serial->parity == 'N')
		a_serial->tios.c_iflag &= ~INPCK;
	else	a_serial->tios.c_iflag |= INPCK;


	/* Raw input */
	a_serial->tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN);

	/* Binary-clean input: no CR/NL translation, no 8th bit stripping, no BREAK/parity marking */
	a_serial->tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

	/* Hardware flow control is disabled: with an inherited CRTSCTS and CTS low nothing goes out */
	a_serial->tios.c_cflag &= ~CRTSCTS;

	/* Software flow control is disabled */
	a_serial->tios.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* Raw output */
	a_serial->tios.c_oflag &=~ OPOST;

	/* Unused because we use open with the NDELAY option */
	a_serial->tios.c_cc[VMIN] = 0;
	a_serial->tios.c_cc[VTIME] = 0;

	if ( tcsetattr(a_serial->fd, TCSANOW, &a_serial->tios) < 0 )
		return	close(a_serial->fd), a_serial->fd = -1,
			$PUTMSG_FAO(T2R$__DEVSYSERR, a_serial->devname, "tcsetattr()", -1, errno);


#ifdef HAVE_TIOCRS485
	if (a_serial->flags & T2R$M_SERIAL_RS485)
		{
		struct serial_rs485 rs485conf = {0};

		$IFTRACE(g_trace, "Trying to enable RS-485 support for %s", a_serial->devname);

		if ( 0 > (l_rc = ioctl(a_serial->fd, TIOCGRS485, &rs485conf)) )
			{
			a_serial->flags &=  ~T2R$M_SERIAL_RS485;
			$PUTMSG_FAO(T2R$__RS485OFF, a_serial->devname, "TIOCGRS485", l_rc, errno);
			}
		else	{
			rs485conf.flags |= SER_RS485_ENABLED;

			if ( 0 > (l_rc = ioctl(a_serial->fd, TIOCSRS485, &rs485conf)) )	/* Set, not Get */
				{
				a_serial->flags &=  ~T2R$M_SERIAL_RS485;
				$PUTMSG_FAO(T2R$__RS485OFF, a_serial->devname, "TIOCSRS485", l_rc, errno);
				}
			else	$PUTMSG_FAO(T2R$__RS485ON, a_serial->devname);
			}

		}
#endif /* HAVE_TIOCRS485 */

	/*
	 * Let the line settle (an USB-to-serial adapter, the RS-485 transceiver direction) and drop
	 * whatever has come in or is still queued from the time before the set up.
	 */
	nanosleep(&l_settle_ts, NULL);
	tcflush(a_serial->fd, TCIOFLUSH);

	return	STS$K_SUCCESS;
}








/*
 *   DESCRIPTION: Form a MODBUS exception answer PDU for the has been failed request: the slave
 *	address is copied from the request, the function code is marked by the exception flag
 *	(bit 7), the exception code is put into the data part.
 *
 *   INPUTS:
 *	a_req_dsc:	A descriptor of the request PDU
 *	a_excode:	A MODBUS exception code (MODBUS$K_EXCEPTION_* constants)
 *
 *   OUTPUTS:
 *	a_resp_dsc:	A descriptor to accept the has been formed exception PDU (3 octets)
 *
 *   RETURNS:
 *	STS$K_SUCCESS (always)
 */
static inline int s_make_exception_resp (
		     void	*a_req_dsc,
		     void	*a_resp_dsc,
		uint8_t		a_excode
		)
{
T2R$_DESC	*l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_PDU_T	*l_req_pdu = (MODBUS_PDU_T *) l_req_dsc->data;
MODBUS_EXC_T	*l_resp_pdu = (MODBUS_EXC_T *) l_resp_dsc->data;

	l_resp_pdu->slave = l_req_pdu->slave;
	l_resp_pdu->fncode = l_req_pdu->fncode | MODBUS$M_FN_EXCEPTION;		/* Set error indicator */
	l_resp_pdu->excode = a_excode;						/* Put error code */

	l_resp_dsc->len = 3;

	return	STS$K_SUCCESS;
}








/*
 *   DESCRIPTION: Put a 16 bits word into the buffer in the Network Byte Order (Big Endian),
 *	advance the buffer pointer to the next position.
 *
 *   INPUTS:
 *	a_buf:		An address of the current position in the output buffer
 *	a_word16:	A value to be stored
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	An address of the next position in the output buffer
 */
static inline uint8_t * s_put_be16 (
		uint8_t		*a_buf,
		uint16_t	a_word16
		)
{
	a_buf[0] = (uint8_t) (a_word16 >> 8);					/* Byte by byte: <a_buf> is at an odd */
	a_buf[1] = (uint8_t) a_word16;						/* offset, MIPS traps a 16 bits store there */

	return	(a_buf + 2);
}


/*
 *   DESCRIPTION: Form an answer PDU with the local Time Stamp - a "pseudo device" feature.
 *	Depending on the has been requested quantity of registers the answer is:
 *	4..7 registers - seconds (64 bits, MSW first, 4 registers only), 8 - + nanoseconds
 *	(64 bits, MSW first), 9 - + the local timezone offset in signed minutes. The time is
 *	taken from CLOCK_REALTIME_COARSE (the nanoseconds have a jiffy resolution).
 *
 *   INPUTS:
 *	a_req_dsc:	A descriptor of the request PDU
 *
 *   OUTPUTS:
 *	a_resp_dsc:	A descriptor to accept the answer PDU
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- the requested quantity is out of the 4..9 range
 */
static int s_add_ts_to_pdu (
		void	*a_req_dsc,
		void	*a_resp_dsc
		)
{
T2R$_DESC *l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_REQ_T	*l_req_pdu;
MODBUS_RESP_T	*l_resp_pdu;
int	l_nr_regs;
struct timespec l_now;
struct tm l_tm = {0};
uint8_t		*l_valptr;

	l_req_pdu = (MODBUS_REQ_T *) l_req_dsc->data;
	l_resp_pdu = (MODBUS_RESP_T *) l_resp_dsc->data;

	l_nr_regs = ntohs(l_req_pdu->qty);

	if ( (l_nr_regs < 4) || (l_nr_regs > (4 + 4 + 1) ))
	     return	STS$K_ERROR;


	l_resp_dsc->len = MODBUS$SZ_PDUHDR;						/* Preset PDU length */
	l_resp_pdu->slave = l_req_pdu->slave;						/* Copy Slave and Function code from original request */
	l_resp_pdu->fncode = l_req_pdu->fncode;
	l_resp_pdu->bc = 0;

	l_valptr = (uint8_t *) l_resp_pdu->val;							/* Set <l_valptr> to PDU's data part */


	clock_gettime(CLOCK_REALTIME_COARSE, &l_now);

	if (  l_nr_regs >= 4 )
		{
		/*
		 * A 64 bits count of seconds is packed into 4x16 bits registers, the most significant
		 * word goes first: R0 = bits 63..48, R1 = 47..32, R2 = 31..16, R3 = 15..0
		 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_sec >> 48));	/* R0 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_sec >> 32));	/* R1 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_sec >> 16));	/* R2 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_sec));		/* R3 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs >= (4 + 4) )
		{
		/* A 64 bits count of nanoseconds is packed by the same rule: R4 = 63..48 ... R7 = 15..0 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 48));	/* R4 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 32));	/* R5 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 16));	/* R6 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec));		/* R7 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs == (4 + 4 + 1) )
		{
		localtime_r(&l_now.tv_sec, &l_tm);

		/*
		 * The offset is reported in MINUTES to fit the +-840 range into a signed 16 bits value:
		 * an offset in seconds (up to +-50400) does not fit into int16_t.
		 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (int16_t) (l_tm.tm_gmtoff / 60));	/* R8 */

		l_resp_dsc->len += (1 * 2);						/* Adjust size of data to 1xbe16 */
		l_resp_pdu->bc += (1 * 2);
		}


	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Under the device lock, open and set up the serial line if it is not initialized
 *	yet; on success the state becomes READY and DEVREADY is signalled. An already initialized
 *	device is left as is (DEVINIT).
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *
 *   OUTPUTS:
 *	a_serial:	.state = READY on success; .fd/.savedtios/.tios/.flags - see s_tty_open()
 *
 *   RETURNS:
 *	T2R$__DEVINIERR	- the device lock is not taken
 *	T2R$__DEVINIT	- the device is already initialized (a success-level code)
 *	otherwise	- the s_tty_open() status
 */
int	t2r$tty_open (
			       T2R$_SERIAL	*a_serial
			       )
{
int	l_rc;

	if ( !(1 & s_tty_lock (a_serial)) )
		return	$PUTMSG_FAO(T2R$__DEVINIERR, a_serial->devname);

	if ( a_serial->state > T2R$K_STATE_IDLE )
		l_rc = $PUTMSG_FAO(T2R$__DEVINIT, a_serial->devname);
	else if ( 1 & (l_rc = s_tty_open (a_serial)) )					/* The state is changed ONLY on success */
		{
		a_serial->state = T2R$K_STATE_READY;
		$PUTMSG_FAO(T2R$__DEVREADY, a_serial->devname, a_serial->baud, a_serial->anstmo_msec,
			a_serial->inter_pdu_usec);
		}

	s_tty_unlock (a_serial);

	return	l_rc;
}



/*
 *   DESCRIPTION: Under the device lock, close an initialized serial line (restore the saved
 *	terminal attributes) and set its state to IDLE. A not initialized device is reported by
 *	DEVNOTINI.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *
 *   OUTPUTS:
 *	a_serial:	.state = IDLE, .fd = -1 (see s_tty_close())
 *
 *   RETURNS:
 *	T2R$__DEVCLSERR	- the device lock is not taken
 *	T2R$__DEVNOTINI	- the device has not been initialized
 *	otherwise	- the s_tty_close() status
 */
int	t2r$tty_close (
			       T2R$_SERIAL	*a_serial
			       )
{
int	l_rc;

	if ( !(1 & s_tty_lock (a_serial)) )
		return	$PUTMSG_FAO(T2R$__DEVCLSERR, a_serial->fd, a_serial->devname);

	if ( !a_serial->state )
		l_rc = $PUTMSG_FAO(T2R$__DEVNOTINI, a_serial->devname);
	else	l_rc = s_tty_close (a_serial), a_serial->state = T2R$K_STATE_IDLE;

	s_tty_unlock (a_serial);

	return	l_rc;
}



/*
 *   DESCRIPTION: Drop a has been left garbage from the serial line: tcflush(TCIOFLUSH) the kernel
 *	FIFOs, then read and discard the incoming data for one inter-PDU (t3.5) interval - a fixed
 *	window, it is not restarted by the arriving data.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *
 *   IMPLICIT INPUTS:
 *	g_exit_flag
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS (always; the poll()/read() errors are signalled by TTYSYSWRN)
 */
static int	s_tty_flush (
	const T2R$_SERIAL	*a_serial
		)
{
int	l_rc;
struct pollfd	l_pfd = {.fd = a_serial->fd, .events = POLLIN};
struct timespec l_now_ts, l_end_of_pdu_ts;
char	l_buf[MODBUS$SZ_MAXPDU];


	tcflush(a_serial->fd, TCIOFLUSH);

	/* Compute a time limit for draining of the line garbage */
	clock_gettime(CLOCK_MONOTONIC, &l_end_of_pdu_ts);
	__util$add_time (&l_end_of_pdu_ts, &a_serial->inter_pdu_ts, &l_end_of_pdu_ts);

	while ( !g_exit_flag )
		{
		/*
		 * Get current time and check: did we reach a time limit to read whole PDU ?
		 *
		 * 	0	- time1 == time2
		 *	0 >	- time1 < time2
		 *	0 <	- time1 > time2
		 */
		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( 0 < (l_rc = __util$cmp_time (&l_now_ts, &l_end_of_pdu_ts)) )
			break;


		if ( 0 > (l_rc = poll(&l_pfd, 1, 1 + a_serial->inter_pdu_usec / 1000)) )
			$PUTMSG_FAO(T2R$__TTYSYSWRN, a_serial->fd, a_serial->devname, "poll()", l_rc, errno);
		else	if ( !l_rc)
			continue;


		if ( 0 > (l_rc = read(a_serial->fd, l_buf, sizeof(l_buf))) )
			{
			if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR) )
				continue;

			$PUTMSG_FAO(T2R$__TTYSYSWRN, a_serial->fd, a_serial->devname, "read()", l_rc, errno);
			break;
			}
		}

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Execute a single MODBUS request-answer transaction on the RTU leg. The serial
 *	device is taken for an exclusive use for the whole transaction. A request to the has been
 *	configured Time Stamp pseudo device (T2R$M_SERIAL_ADDTS is set and the unit, the function
 *	and the start register match) is answered locally, without the lock and without touching
 *	the line; a bad quantity is answered by the ILLEGAL_DATA_ADDRESS exception. On an RTU I/O
 *	error the line is flushed and a SERVER_DEVICE_FAILURE exception PDU is formed.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_dsc:	A descriptor of the request PDU (without MBAP and without CRC)
 *
 *   OUTPUTS:
 *	a_resp_dsc:	The answer PDU (the CRC trailer is stripped off), or a 3-octet exception PDU
 *	a_excode:	The exception code of the formed exception PDU, 0 - no exception
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- an answer or an exception PDU is in a_resp_dsc (check a_excode)
 *	T2R$__DEVBUSY	- the device lock is not taken, a_resp_dsc is not filled
 */
int	t2r$tty_exec_req (
		T2R$_SERIAL	*a_serial,
			void	*a_req_dsc,
			void	*a_resp_dsc,
			uint8_t	*a_excode
			)
{
int	l_rc, l_rcv;
T2R$_DESC	*l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_REQ_T	*l_req_pdu;

	*a_excode = 0;

	l_req_pdu = (MODBUS_REQ_T *) l_req_dsc->data;

	/* Special hook to process request of local TS */
	if ( ( a_serial->flags & T2R$M_SERIAL_ADDTS )
	     && ( l_req_pdu->slave == a_serial->ts_unit_nr)
	     && ( l_req_pdu->fncode == a_serial->ts_fncode)
	     && ( ntohs(l_req_pdu->sa) == a_serial->ts_base_reg0) )
		{
		if ( !(1 & (l_rc = s_add_ts_to_pdu (a_req_dsc, a_resp_dsc))) )
			{
			l_rc = s_make_exception_resp(a_req_dsc, a_resp_dsc, *a_excode = l_rcv = MODBUS$K_EXCEPTION_ILLEGAL_DATA_ADDRESS);

			$PUTMSG_FAO(T2R$__EXCRPT, a_serial->fd, a_serial->devname, l_rcv, t2r$modbus_exc2str (l_rcv));
			}

		return	l_rc;
		}


	if ( !(1 & s_tty_lock (a_serial)) )
		return	$PUTMSG_FAO(T2R$__DEVBUSY, a_serial->fd, a_serial->devname);


	if ( 1 & (l_rc = s_tty_rtu_tx (a_serial, l_req_dsc)) )
		l_rc = s_tty_rtu_rx (a_serial, l_req_dsc, l_resp_dsc);


	if ( !(1 & l_rc) )
		{
		s_tty_flush (a_serial);
		$IFTRACE(g_trace, "[#%d:<%s>] --- do flush on I/O error", a_serial->fd, a_serial->devname);
		}

	s_tty_unlock (a_serial);

	if ( !(1 & l_rc) )
		{
		l_rc = s_make_exception_resp(a_req_dsc, a_resp_dsc, *a_excode = l_rcv = MODBUS$K_EXCEPTION_SERVER_DEVICE_FAILURE);

		$PUTMSG_FAO(T2R$__EXCRPT, a_serial->fd, a_serial->devname, l_rcv, t2r$modbus_exc2str (l_rcv));
		}

	return	l_rc;
}
