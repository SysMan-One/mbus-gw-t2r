
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
#define		T2R$K_LATEGD_MAX	60000				/* <lategd>, msec; the default is <iotmo> / 4	*/
#define		T2R$K_USBGAP_MSEC	20				/* A gap inside of an answer from an USB adapter: 16 msec
									   of the FTDI latency_timer plus a margin */

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
			sessions;					/* Active sessions, __atomic_*() only */
	struct sockaddr_in sk;						/* Socket address structure: TCP, 0.0.0.0:502 */
		char target[T2R$K_TTY_DEVNAME + 1];			/* "COM1", "/dev/ttyS0" */

	T2R$_SERIAL	*serial;					/* A context to has been defined serial device */

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
} T2R$_SESSION;


int	t2r$net_start_listeners (void);
int	t2r$net_stop_listeners (void);


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

#ifdef __cplusplus
}
#endif

#endif	/*	__MODBUS_TCP2RTU_DEFS__	*/
