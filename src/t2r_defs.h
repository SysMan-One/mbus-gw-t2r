
/*
**++
**
**  FACILITY: A yet another gateway for MODBUS (TCP 2 RTU)
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: Data Srtuctures and constant definitions is supposed to be used across all MODBUS-TCP2RTU's modules
**
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  10-AUG-2025
**
**  MODIFICATION HISTORY:
**
**	25-AUG-2026	RRL	X.00-04: $MBAP_PRINT/$RTU_PRINT do check <g_trace> before the call;
**				added T2R$K_ANSTMO_MSEC - a default answer timeout;
**				T2R$_LISTENER.target is filled as a plain ASCIIZ string now.
**
**	25-AUG-2026	RRL	X.00-04ECO01 / REV: 00.04.01 - Added T2R$K_BAUD_/DATABITS_/STOPBITS_/PORT_
**				MIN/MAX limit constants for validation of the configurable parameters.
**
**	14-OCT-2026	RRL	X.00-12 / REV: 00.12.00 - T2R$K_IDLE_TMO_SEC is replaced by T2R$K_IDLETMO_*
**				(<idletmo>), added T2R$K_CONNLM_* (<connlm>) and T2R$K_LISTEN_BACKLOG;
**				T2R$_LISTENER got .idletmo and the .sessions counter, T2R$_SESSION - the
**				back link .listener.
**
**	02-NOV-2026	CLD	X.00-13ECO07 / REV: 00.13.07 - Added T2R$K_SETTLE_MSEC (a pause after the
**				serial port set up).
**
**	02-NOV-2026	CLD	X.00-13ECO08 / REV: 00.13.08 - T2R$_SERIAL got .lategd_msec (<lategd>, the guard
**				window after a failed exchange), added T2R$K_LATEGD_MAX, T2R$K_USBGAP_MSEC.
**
**	02-NOV-2026	CLD	X.00-13ECO09 / REV: 00.13.09 - T2R$_SERIAL got .nodev and .reopen_ts (the device
**				is absent or lost, the next reopen attempt), added T2R$K_REOPEN_MSEC,
**				T2R$K_CHECK_MSEC and t2r$tty_check().
**
**	01-NOV-2026	CLD	X.00-14 / REV: 00.14.00 - The RAW (transparent) listener mode: T2R$K_MODE_*,
**				T2R$K_RAW_* constants, T2R$_LISTENER got .mode, .takeover and the current
**				raw session (.rawsess under .rawlock), t2r$tty_raw_relay().
**
**	06-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - The control channel (T2R-CTL, mbus-ctl): T2R$_SESSION got
**				the registry links, a session id, the connect time and the counters
**				(requests, answers, errors, octets), the .reset mark; T2R$_LISTENER got the
**				.accepted/.rejected counters; added T2R$K_CTL_* constants, t2r$net_sess_walk(),
**				t2r$ctl_start(), t2r$ctl_stop().
**
**--
*/


#ifndef	__MODBUS_TCP2RTU_DEFS__
#define __MODBUS_TCP2RTU_DEFS__	1

#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<termios.h>
#include	<pthread.h>

#include	"utility_routines.h"					/* Starlet's Utility/General purpose routines */
#include	"starlet.h"						/* FAO: fao_prm_t for the $PUTMSG_FAO parameters */
#include	"t2r_modbus.h"




#ifdef __cplusplus
extern "C" {
#endif





#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%u.%u.%u.%u"
#define IPv4_BYTES(addr) \
		(uint8_t) ((addr) & 0xFF), \
		(uint8_t) (((addr) >> 8) & 0xFF),\
		(uint8_t) (((addr) >> 16) & 0xFF),\
		(uint8_t) (((addr) >> 24) & 0xFF)
#endif



enum {
	T2R$K_STATE_IDLE = 0,

	T2R$K_STATE_READY,						/* Idle session context has been created */

	T2R$K_STATE_PDU_RECV,						/* Receiving whole MODBUS's PDU */
	T2R$K_STATE_PDU_ENQD,						/* Received MODBUS PDU is enqueued for processing */

	T2R$K_STATE_PDU_XMIT,						/* We have got an answer from RTU leg and sending answer MODBUS PDU */
	T2R$K_STATE_SHUT,						/* Session should be closed */

	T2R$K_STATE_EOL
};


#define		T2R$SZ_BUFF	2048
#define		T2R$K_MAX_LISTENERS	128
#define		T2R$K_MAX_SERIALS	128
#define		T2R$K_IDLETMO_DEF	60				/* Idle session timeout, seconds: <idletmo>	*/
#define		T2R$K_IDLETMO_MIN	1
#define		T2R$K_IDLETMO_MAX	86400
#define		T2R$K_CONNLM_DEF	32				/* Sessions per listener: <connlm>		*/
#define		T2R$K_CONNLM_MIN	1
#define		T2R$K_CONNLM_MAX	512
#define		T2R$K_LISTEN_BACKLOG	64				/* listen() queue of not yet accepted connections */
#define		T2R$K_NET_TMO_SEC	300				/* A timeout for reading data from network socket */
#define		T2R$K_ANSTMO_MSEC	1000				/* A default timeout to wait an answer on the RTU leg */
#define		T2R$K_SETTLE_MSEC	50				/* A pause after the serial port set up, then TCIOFLUSH */
#define		T2R$K_REOPEN_MSEC	1000				/* An absent or lost device is reopened this often	*/
#define		T2R$K_CHECK_MSEC	1000				/* The idle lines are checked for a hangup this often	*/
#define		T2R$K_LATEGD_MAX	60000				/* <lategd>, msec; the default is <iotmo> / 4	*/
#define		T2R$K_USBGAP_MSEC	20				/* A gap inside of an answer from an USB adapter: 16 msec
									   of the FTDI latency_timer plus a margin */

/*
 * The control channel (T2R-CTL): a local AF_UNIX socket, one text request line per connection,
 * see src/t2r_ctl.c and the mbus-ctl utility.
 */
#define		T2R$K_CTL_SOCK		"/var/run/mbusgw-t2r.sock"	/* The default path: /CONTROL=<path> or NONE	*/
#define		T2R$K_CTL_MODE		0660				/* Socket rights: the owner and the group only	*/
#define		T2R$K_CTL_LINESZ	256				/* The longest request line			*/
#define		T2R$K_CTL_TMO_MSEC	5000				/* A client must send its line in this time	*/
#define		T2R$K_CTL_POLL_MSEC	1000				/* <g_exit_flag> check interval			*/
#define		T2R$K_CTL_DRAIN_SEC	10				/* RESTART: wait the sessions to end at most	*/

/*
 * The RAW (transparent) mode of a listener: octets are relayed as is between a single TCP client
 * and the serial line, no MODBUS framing at all.
 */
enum	{
	T2R$K_MODE_MODBUS = 0,						/* MODBUS TCP (MBAP) <-> MODBUS RTU, the default */
	T2R$K_MODE_RAW							/* A transparent octet stream TCP <-> serial line */
};

#define		T2R$K_RAW_BUFSZ		4096				/* A relay buffer per direction, octets		*/
#define		T2R$K_RAW_POLL_MSEC	1000				/* <g_exit_flag> and <idletmo> check interval	*/
#define		T2R$K_RAW_KAIDLE	30				/* TCP keepalive: idle seconds before probing	*/
#define		T2R$K_RAW_KAINTVL	10				/* TCP keepalive: seconds between probes	*/
#define		T2R$K_RAW_KACNT		3				/* TCP keepalive: unanswered probes = dead peer	*/

/*
 * Limits for validation of the has been configured parameters of a serial line and a listener
 */
#define		T2R$K_BAUD_MIN		50				/* Slowest POSIX line rate			*/
#define		T2R$K_BAUD_MAX		4000000				/* B4000000 - fastest one			*/
#define		T2R$K_DATABITS_MIN	5				/* CS5						*/
#define		T2R$K_DATABITS_MAX	8				/* CS8						*/
#define		T2R$K_STOPBITS_MIN	1
#define		T2R$K_STOPBITS_MAX	2
#define		T2R$K_PORT_MIN		1				/* TCP port range				*/
#define		T2R$K_PORT_MAX		65535
#define		T2R$K_TTY_DEVNAME	64


#define		T2R$K_TS_FNCODE	(MODBUS$K_FN_READ_HOLDING_REGISTERS)
#define		T2R$K_TS_BASE_REG0	8135
#define		T2R$K_TS_UNIT		135

typedef struct	t2r$_desc {
	int	sz,
		len;
	char	data[0];
} T2R$_DESC;

#define	$DESCRIPTOR_S(dsc_name, dsc_sz) struct {int sz; int len; char data[dsc_sz];} dsc_name;  dsc_name.sz = dsc_sz; dsc_name.len = 0;

enum {
	__SERIAL_RS485 = 0,
	__SERIAL_ADDTS
};

enum {
	T2R$M_SERIAL_RS485 = (1 << __SERIAL_RS485),			/* RS485 specific initialization */
	T2R$M_SERIAL_ADDTS = (1 << __SERIAL_ADDTS),			/* Add TimeStamp at end of response PDU */

};

typedef struct	t2r$_serial {
		int	state;						/* Session state, see T2R$K_STATE_* constants */
	pthread_mutex_t	lock;						/* Coordinate an access between multiple TCP clients */

		int	fd;						/* I/O descriptor for serial device */
	struct timespec						/* See <pdutmo> */
			inter_pdu_ts;

		char devname[T2R$K_TTY_DEVNAME + 1];			/* "COM1", "/dev/ttyS0" */


		int	flags;						/* Optional RS485 specific initialization */

		int	baud,						/* Bits-per-second rate of serial line */
			stopbits,
			databits,
			parity,
			flow;

		int	anstmo_msec,					/* A time to wait an answer for has been sent request */
			inter_pdu_usec,					/* A delay before and after PDU on RTU */
			onebyte_time_usec,				/* Estimated time to send byte over serial line in nanoseconds */
			lategd_msec;					/* A guard window after a failed exchange: <lategd> */

		int	nodev;						/* The device is absent or lost: .fd = -1, reopen by .devname */
	struct timespec	reopen_ts;					/* CLOCK_MONOTONIC of the next reopen attempt */

		uint8_t	ts_unit_nr,
			ts_fncode;

		uint16_t ts_base_reg0;

	struct termios	savedtios,
			tios;
} T2R$_SERIAL;



typedef struct	t2r$_listener {
		int	fd,						/* Network socket descriptor */
			proto,						/* IPPROTO_TCP or IPPROTO_UDP */
			iotmo,
			connlm,						/* Connection limit on this listener */
			idletmo,					/* Idle session timeout, seconds */
			sessions,					/* Active sessions, __atomic_*() only */
			mode,						/* T2R$K_MODE_MODBUS or T2R$K_MODE_RAW */
			takeover;					/* RAW: a new client kicks the current one */
	uint32_t	accepted,					/* Connections accepted / rejected since the start, */
			rejected;					/* written by the dispatcher thread only	*/
	struct sockaddr_in sk;						/* Socket address structure: TCP, 0.0.0.0:502 */
		char target[T2R$K_TTY_DEVNAME + 1];			/* "COM1", "/dev/ttyS0" */

	T2R$_SERIAL	*serial;					/* A context to has been defined serial device */

	pthread_mutex_t	rawlock;					/* RAW: guards .rawsess */
	struct t2r$__session *rawsess;					/* RAW: the current session, NULL - none */
} T2R$_LISTENER;





typedef struct	t2r$__session {
		int	state;						/* Session state, see T2R$K_STATE_* constants */

	struct timespec	lastio_ts;					/* Timestamp of last I/O operation on the session */

		int	sd;						/* Network socket descriptor */
	struct sockaddr_in sk;						/* Socket address structure */

		void	*netbuf_dsc;					/* A descriptor of buffer for network I/O */
		int	datalen;					/* A size of data to be read from the network */


	T2R$_SERIAL	*target;
	T2R$_LISTENER	*listener;					/* The listener which has accepted the session */

	/*
	 * The registry of the live sessions (T2R-NET, see t2r$net_sess_walk()): what the control channel
	 * shows and resets. The counters have a single writer - the session's own thread - and are only
	 * read by the control thread under the registry lock: no atomics (a 32-bit CPU has no cheap 64-bit
	 * ones), a torn read may at worst glitch a displayed number once.
	 */
	struct t2r$__session *reg_next,					/* The registry list, under the registry lock	*/
			*reg_prev;
		uint32_t id;						/* A session id: 1, 2, 3 ... never reused	*/
		time_t	start;						/* CLOCK_REALTIME of the accept()		*/
		int	reset;						/* Is reset by the operator (RESET CONNECTIONS)	*/
		uint64_t nreq,						/* MODBUS: whole requests received		*/
			nans,						/* MODBUS: answers sent (exceptions too)	*/
			nerr,						/* MODBUS: exception answers + failed requests	*/
			nrx,						/* Octets from the client (RAW: to the line)	*/
			ntx;						/* Octets to the client (RAW: from the line)	*/
} T2R$_SESSION;


int	t2r$net_start_listeners (void);
int	t2r$net_stop_listeners (void);
int	t2r$net_sess_walk (int (*a_rtn) (T2R$_SESSION *a_session, void *a_arg), void *a_arg);
int	t2r$net_sess_count (void);

int	t2r$ctl_start (const char *a_path);
int	t2r$ctl_stop (void);


extern	int	g_trace;							/* Declared in T2R-MAIN, is used by the dump macros */

/*
 *  Both dump macros do check <g_trace> BEFORE the call: the routines convert a whole PDU
 *  to a hex string, so we do not want to pay for it (and to touch the data) in a silent mode.
 */
#define	$MBAP_PRINT(a_pref,a_data,a_datalen)	do { if ( g_trace ) \
		t2r$mbap_print(__MODULE__, __FUNCTION__, __LINE__, a_pref, a_data, a_datalen); } while (0)

void	t2r$mbap_print (const char *a__mod, const char *a__fi, const int a__li,
		const char *a_pref, const char a_data[], int a_datalen);


#define	$RTU_PRINT(a_pref,a_data,a_datalen,a_crc)	do { if ( g_trace ) \
		t2r$rtu_print(__MODULE__, __FUNCTION__, __LINE__, a_pref, a_data, a_datalen, a_crc); } while (0)

void	t2r$rtu_print (const char *a__mod, const char *a__fi, const int a__li,
		const char *a_pref, const void *a_data, int a_datalen, uint16_t a_crc);

int	t2r$tty_open(T2R$_SERIAL *);
int	t2r$tty_close(T2R$_SERIAL *);
int	t2r$tty_exec_req(T2R$_SERIAL *, void *a_req_dsc, void *a_resp_dsc, uint8_t *a_excode);
int	t2r$tty_check(T2R$_SERIAL *);
int	t2r$tty_raw_relay(T2R$_SERIAL *, int a_sd, int a_idletmo, uint64_t *a_net2tty, uint64_t *a_tty2net);

#ifdef __cplusplus
}
#endif

#endif	/*	__MODBUS_TCP2RTU_DEFS__	*/
