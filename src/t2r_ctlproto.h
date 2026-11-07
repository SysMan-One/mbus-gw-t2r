/*
**++
**
**  FACILITY: A yet another gateway for MODBUS (TCP 2 RTU) - the control channel protocol
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: The protocol of the control channel between the gateway (T2R-CTL, src/t2r_ctl.c) and the
**	mbus-ctl utility (src/mbus_ctl.c). Both sides include this file: the command codes and the tag ids
**	are defined here only.
**
**	The protocol is binary, made of the StarLet AVProto PDU (avproto.h): a fixed header and a list of
**	TLV records. There is no text form of it. The numbers of the TLVs are read by __tlv$lookup() and
**	their wire length is checked: StarLet 1.6 __tlv$get() does not check it.
**
**	    PDU		= HEADER TLV TLV ...
**	    HEADER	= magic[8]  u_len  u_csr  u_seq			20 octets, the numbers in NBO
**			  magic	- "MBUSCTL", zero-padded		(T2R$K_CTL_MAGIC)
**			  u_len	- the octets of the TLVs after the header
**			  u_csr	- request: the command code		(T2R$K_CTL_CMD_*)
**				  answer:  the status, a VMS condition: the low bit set - success
**			  u_seq	- any number of the client, the answer carries the same one
**	    TLV		= w_tag  w_len  value				w_tag = [ type : 4 | id : 12 ], NBO
**			  type	- TAG$K_WORD/LONGWORD/QWORD (numbers, NBO), TAG$K_BBLOCK (octets, a text
**				  without a NUL)
**			  id	- T2R$K_CTL_TAG_*
**
**	One request per connection: the client connects, sends one PDU, the gateway sends one PDU back and
**	closes the connection. A PDU with a wrong magic or longer than T2R$K_CTL_REQSZ (a request) or
**	T2R$K_CTL_ANSSZ (an answer) is not answered/accepted: the connection is just closed.
**
**	The answer is a flat list of records. A record starts with the REC tag (WORD, T2R$K_CTL_REC_*),
**	its fields follow up to the next REC tag or the end of the PDU:
**
**	    REC=GATEWAY  IDENT REV ARCH PID STARTED NOW SETTINGS CONTROL LINES LISTENERS UP CONNECTIONS
**	    REC=LISTENER LSN ADDR PORT MODE TARGET STATE CONNLM IDLETMO SESSIONS ACCEPTED REJECTED
**	    REC=LINE     LINE DEVICE BAUD DATABITS PARITY STOPBITS IOTMO LATEGD RS485 ADDTS STATE LISTENERS SESSIONS
**	    REC=CONN     ID SD LSN MODE ADDR PORT SINCE RESET NREQ NANS NERR NRX NTX
**	    REC=RESET    ID ADDR PORT				a connection has been reset
**	    REC=CHECK    TEXT					a WARNING/ERROR line of the settings check
**
**	The tags of the answer as a whole may stand anywhere: REASON - the reason of a failure status,
**	COUNT - RESET CONNECTIONS: how many connections have been reset, CUT - the answer is longer than
**	T2R$K_CTL_ANSSZ, the records after this point are left out (whole records only).
**
**	The commands and their arguments (the TLVs of a request):
**
**	    SHOW_GATEWAY		-		REC=GATEWAY, REC=LISTENER ...
**	    SHOW_LINES			-		REC=LINE ...
**	    SHOW_CONNECTIONS		-		REC=LISTENER ..., REC=CONN ...
**	    RESET_CONNECTIONS		ALL | ID | ADDR [PORT]	REC=RESET ..., COUNT
**	    RESTART_GATEWAY		-		REC=CHECK ...
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  SPDX-License-Identifier: Apache-2.0
**
**  CREATION DATE:  07-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	07-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - Initial revision: the control channel speaks AVProto TLV,
**				the text protocol is gone.
**
**--
*/

#ifndef	__MODBUS_TCP2RTU_CTLPROTO__
#define	__MODBUS_TCP2RTU_CTLPROTO__	1

#include	"avproto.h"						/* Needs utility_routines.h (STS$K_*) before it	*/


#define	T2R$K_CTL_MAGIC		"MBUSCTL"				/* The signature of a PDU, up to 8 octets	*/
#define	T2R$K_CTL_REQSZ		1024					/* The longest request PDU, the header too	*/
#define	T2R$K_CTL_ANSSZ		65535					/* The longest answer PDU: the PDU size of	*/
									/* __tlv$put() of StarLet 1.6 is 16 bits wide	*/
#define	T2R$K_CTL_RECMAX	1024					/* A record is never longer: a new record is	*/
									/* not started with less room, CUT is put	*/


/*
 * The command codes: u_csr of a request
 */
enum	{
	T2R$K_CTL_CMD_SHOW_GATEWAY = 1,
	T2R$K_CTL_CMD_SHOW_LINES,
	T2R$K_CTL_CMD_SHOW_CONNECTIONS,
	T2R$K_CTL_CMD_RESET_CONNECTIONS,
	T2R$K_CTL_CMD_RESTART_GATEWAY,

	T2R$K_CTL_CMD_MAX						/* EOL marker, must be last!			*/
};


/*
 * The record types: the value of the REC tag
 */
enum	{
	T2R$K_CTL_REC_GATEWAY = 1,
	T2R$K_CTL_REC_LISTENER,
	T2R$K_CTL_REC_LINE,
	T2R$K_CTL_REC_CONN,
	T2R$K_CTL_REC_RESET,
	T2R$K_CTL_REC_CHECK,

	T2R$K_CTL_REC_MAX
};


/*
 * The values of the MODE and STATE fields (WORD)
 */
enum	{
	T2R$K_CTL_MODE_MODBUS = 0,
	T2R$K_CTL_MODE_RAW
};

enum	{
	T2R$K_CTL_STATE_DOWN = 0,					/* A listener: no listening socket		*/
	T2R$K_CTL_STATE_UP,						/* A listener: is listening			*/
	T2R$K_CTL_STATE_CLOSED,						/* A line: no listener has opened it		*/
	T2R$K_CTL_STATE_ABSENT,						/* A line: the device is absent or lost		*/
	T2R$K_CTL_STATE_OPEN,						/* A line: is open				*/

	T2R$K_CTL_STATE_MAX
};


/*
 * The tag ids. The type of every tag is fixed: (W) WORD, (L) LONGWORD, (Q) QWORD, (B) BBLOCK.
 * ADDR is an IPv4 address as a number (192.168.1.2 = 0xC0A80102), PORT - a TCP port.
 */
enum	{
	T2R$K_CTL_TAG_REC = 1,		/* (W) T2R$K_CTL_REC_*: a record starts			*/
	T2R$K_CTL_TAG_REASON,		/* (B) The reason of a failure status			*/
	T2R$K_CTL_TAG_TEXT,		/* (B) CHECK: a line of the settings check		*/
	T2R$K_CTL_TAG_COUNT,		/* (L) RESET: the connections reset			*/
	T2R$K_CTL_TAG_ALL,		/* (L) RESET: all the connections (a request)		*/
	T2R$K_CTL_TAG_CUT,		/* (W) 1 - the answer is cut, see T2R$K_CTL_ANSSZ	*/

	T2R$K_CTL_TAG_IDENT = 16,	/* (B) GATEWAY: the version, "V.01-01"			*/
	T2R$K_CTL_TAG_REV,		/* (B) GATEWAY: the revision, "01.01.00"		*/
	T2R$K_CTL_TAG_ARCH,		/* (B) GATEWAY: the architecture			*/
	T2R$K_CTL_TAG_PID,		/* (L) GATEWAY: the process id				*/
	T2R$K_CTL_TAG_STARTED,		/* (Q) GATEWAY: the start time, seconds since the Epoch	*/
	T2R$K_CTL_TAG_NOW,		/* (Q) GATEWAY: the time of the answer			*/
	T2R$K_CTL_TAG_SETTINGS,		/* (B) GATEWAY: the settings file			*/
	T2R$K_CTL_TAG_CONTROL,		/* (B) GATEWAY: the control socket			*/
	T2R$K_CTL_TAG_LINES,		/* (L) GATEWAY: the serial lines			*/
	T2R$K_CTL_TAG_LISTENERS,	/* (L) GATEWAY, LINE: the listeners			*/
	T2R$K_CTL_TAG_UP,		/* (L) GATEWAY: the listeners which are up		*/
	T2R$K_CTL_TAG_CONNECTIONS,	/* (L) GATEWAY: the TCP clients				*/

	T2R$K_CTL_TAG_LSN = 32,		/* (L) LISTENER, CONN: the listener number		*/
	T2R$K_CTL_TAG_ADDR,		/* (L) LISTENER: bind, CONN/RESET: the client		*/
	T2R$K_CTL_TAG_PORT,		/* (W) next to ADDR					*/
	T2R$K_CTL_TAG_MODE,		/* (W) LISTENER, CONN: T2R$K_CTL_MODE_*			*/
	T2R$K_CTL_TAG_TARGET,		/* (B) LISTENER: the serial device			*/
	T2R$K_CTL_TAG_STATE,		/* (W) LISTENER, LINE: T2R$K_CTL_STATE_*		*/
	T2R$K_CTL_TAG_CONNLM,		/* (L) LISTENER: <connlm>				*/
	T2R$K_CTL_TAG_IDLETMO,		/* (L) LISTENER: <idletmo>, seconds			*/
	T2R$K_CTL_TAG_SESSIONS,		/* (L) LISTENER, LINE: the TCP clients now		*/
	T2R$K_CTL_TAG_ACCEPTED,		/* (L) LISTENER: accepted since the start		*/
	T2R$K_CTL_TAG_REJECTED,		/* (L) LISTENER: rejected (over <connlm>)		*/

	T2R$K_CTL_TAG_LINE = 48,	/* (L) LINE: the line number				*/
	T2R$K_CTL_TAG_DEVICE,		/* (B) LINE: the device					*/
	T2R$K_CTL_TAG_BAUD,		/* (L) LINE: the speed					*/
	T2R$K_CTL_TAG_DATABITS,		/* (W) LINE						*/
	T2R$K_CTL_TAG_PARITY,		/* (W) LINE: 'N', 'E' or 'O'				*/
	T2R$K_CTL_TAG_STOPBITS,		/* (W) LINE						*/
	T2R$K_CTL_TAG_IOTMO,		/* (L) LINE: <iotmo>, msec				*/
	T2R$K_CTL_TAG_LATEGD,		/* (L) LINE: <lategd>, msec				*/
	T2R$K_CTL_TAG_RS485,		/* (W) LINE: 1 - RS-485					*/
	T2R$K_CTL_TAG_ADDTS,		/* (W) LINE: 1 - time stamps are added			*/

	T2R$K_CTL_TAG_ID = 64,		/* (L) CONN, RESET: the connection id; a RESET request	*/
	T2R$K_CTL_TAG_SD,		/* (L) CONN: the socket					*/
	T2R$K_CTL_TAG_SINCE,		/* (Q) CONN: the connect time, seconds since the Epoch	*/
	T2R$K_CTL_TAG_RESET,		/* (W) CONN: 1 - is being reset				*/
	T2R$K_CTL_TAG_NREQ,		/* (Q) CONN: MODBUS requests				*/
	T2R$K_CTL_TAG_NANS,		/* (Q) CONN: MODBUS answers				*/
	T2R$K_CTL_TAG_NERR,		/* (Q) CONN: MODBUS errors				*/
	T2R$K_CTL_TAG_NRX,		/* (Q) CONN: octets from the client (to the line in RAW)	*/
	T2R$K_CTL_TAG_NTX,		/* (Q) CONN: octets to the client (from the line in RAW)	*/

	T2R$K_CTL_TAG_MAX		/* EOL marker, must be last! (< 4096)			*/
};

#endif	/* #ifndef	__MODBUS_TCP2RTU_CTLPROTO__ */
