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
**	src/t2r_ctl.c): it sends one request PDU of the AVProto TLV protocol (src/t2r_ctlproto.h), reads
**	the answer PDU and shows its records as tables. The command line is in the DCL style of the
**	StarLet CLI routines: the keywords may be shortened, the qualifiers start with "/" (or "--"):
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
**	07-NOV-2026	CLD	V.01-01 / REV: 01.01.00 - The requests and the answers are AVProto TLV PDUs
**				(src/t2r_ctlproto.h), no text lines; the connection selector of RESET is
**				checked here (CTLBADSEL); a cut answer is told (CTLCUT).
**
**--
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<inttypes.h>
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
#include	"t2r_ctlproto.h"


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
#define	MBUS$K_TLVHDR		4						/* w_tag + w_len of a TLV			*/

static volatile sig_atomic_t	s_exit_flag;					/* Ctrl-C: stop /CONTINUOUS			*/
static	uint32_t		s_seq;						/* u_seq of the next request			*/

static const char	*s_state_name [T2R$K_CTL_STATE_MAX] = {"DOWN", "UP", "CLOSED", "ABSENT", "OPEN"};


/*
 * A request: one PDU of at most T2R$K_CTL_REQSZ octets
 */
typedef union	mbus_req_t {
	AVPROTO_PDU	pdu;
	uint8_t		raw[T2R$K_CTL_REQSZ];
} MBUS$_REQ;


/*
 * A record of the answer: its type and where its fields are - the offsets in the TLV area of the
 * PDU (as the <context> of __tlv$lookup()), from the TLV after REC up to the next REC
 */
typedef struct	mbus_rec_t {
	uint16_t	type;							/* T2R$K_CTL_REC_*				*/
	int		off,
			end;
} MBUS$_REC;


static const MBUS$_REC	s_whole = {.off = 0, .end = INT32_MAX};		/* The whole answer as a "record": COUNT, ...	*/


/*
 * An answer of the gateway: the whole PDU and its records
 */
typedef struct	mbus_ans_t {
	AVPROTO_PDU	*pdu;
	unsigned	pdusz;							/* The header and the TLVs			*/
	MBUS$_REC	*rec;
	int		nr;
	int		cut;							/* The gateway has put CUT			*/
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
	free(a_ans->pdu);
	free(a_ans->rec);
	*a_ans = (MBUS$_ANS) {0};
}



/*
 *   DESCRIPTION: Start a request PDU: the header with the command code and a new sequence number.
 *	The arguments are put by __tlv$put() after it.
 *
 *   INPUTS:
 *	a_cmd:		T2R$K_CTL_CMD_*
 *
 *   OUTPUTS:
 *	a_req:		The request
 *
 *   RETURNS:
 *	condition code of __tlv$hset()
 */
static int	s_req_init (
		MBUS$_REQ	*a_req,
		uint32_t	a_cmd
		)
{
	if ( !s_seq )
		s_seq = (uint32_t) getpid() << 16;

	memset(a_req, 0, sizeof(MBUS$_REQ));

	return	__tlv$hset(&a_req->pdu, (const unsigned char *) T2R$K_CTL_MAGIC, a_cmd, ++s_seq);
}



/*
 *   DESCRIPTION: Read exactly <a_len> octets; SO_RCVTIMEO of the socket limits the wait.
 *
 *   INPUTS:
 *	a_sd:		A socket
 *	a_len:		The octets to read
 *
 *   OUTPUTS:
 *	a_buf:		The data
 *
 *   RETURNS:
 *	0 - all is read, -1 - EOF (errno = 0) or an error (errno)
 */
static int	s_recvn (
		int	a_sd,
		void	*a_buf,
		size_t	a_len
		)
{
ssize_t	l_got;

	for (size_t l_off = 0; l_off < a_len; )
		{
		if ( 0 > (l_got = recv(a_sd, (char *) a_buf + l_off, a_len - l_off, 0)) )
			{
			if ( errno == EINTR )
				continue;

			return	-1;
			}

		if ( !l_got )
			return	errno = 0, -1;

		l_off += (size_t) l_got;
		}

	return	0;
}



/*
 *   DESCRIPTION: Find the records of an answer: every REC tag starts one, it lasts up to the next REC.
 *
 *   INPUTS:
 *	a_ans:		The answer: .pdu, .pdusz
 *
 *   OUTPUTS:
 *	a_ans:		.rec, .nr, .cut
 *
 *   RETURNS:
 *	STS$K_SUCCESS, T2R$__CTLBADANS (signalled)
 */
static int	s_ans_parse (
		MBUS$_ANS	*a_ans
		)
{
int	l_ctx = -1, l_max = 0;
unsigned	l_type = 0, l_len = 0;
void	*l_val = NULL;
uint16_t	l_w;
MBUS$_REC	*l_new;

	while ( 1 & __tlv$lookup(a_ans->pdu, a_ans->pdusz, &l_ctx, T2R$K_CTL_TAG_REC, &l_type, &l_val, &l_len) )
		{
		if ( (l_type != TAG$K_WORD) || (l_len != sizeof(l_w)) )
			return	$PUTMSG_FAO(T2R$__CTLBADANS, "a REC tag is not a WORD");

		if ( a_ans->nr == l_max )
			{
			if ( !(l_new = realloc(a_ans->rec, (size_t) (l_max += 256) * sizeof(MBUS$_REC))) )
				return	$PUTMSG_FAO(T2R$__CTLBADANS, "out of memory");

			a_ans->rec = l_new;
			}

		if ( a_ans->nr )						/* The previous one ends at this REC */
			a_ans->rec[a_ans->nr - 1].end = l_ctx - (MBUS$K_TLVHDR + sizeof(l_w));

		memcpy(&l_w, l_val, sizeof(l_w));
		a_ans->rec[a_ans->nr++] = (MBUS$_REC) {.type = be16toh(l_w), .off = l_ctx, .end = INT32_MAX};
		}

	if ( a_ans->nr )
		a_ans->rec[a_ans->nr - 1].end = (int) (a_ans->pdusz - sizeof(AVPROTO_HDR));

	a_ans->cut = (1 & __tlv$lookup(a_ans->pdu, a_ans->pdusz, NULL, T2R$K_CTL_TAG_CUT, &l_type, &l_val, &l_len));

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Send one request PDU to the gateway and read its answer PDU: the header, then exactly
 *	u_len octets of the TLVs.
 *
 *   INPUTS:
 *	a_path:		The control socket
 *	a_req:		The request (s_req_init() + __tlv$put())
 *	a_quiet:	1 - do not signal the "cannot reach" (a repeating display retries)
 *
 *   OUTPUTS:
 *	a_ans:		The answer (s_ans_free() it)
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the status of the answer is a success
 *	T2R$__CTLANSERR	- a failure status, REASON is signalled; the records are still there
 *	T2R$__CTLNOGW	- cannot connect, or no answer
 *	T2R$__CTLBADANS	- not a PDU of the control channel (signalled)
 */
static int	s_request (
	const char	*a_path,
		MBUS$_REQ *a_req,
		int	a_quiet,
		MBUS$_ANS *a_ans
		)
{
int	l_sd, l_rc;
unsigned	l_len = 0, l_csr = 0, l_seq = 0, l_type = 0, l_rlen = 0, l_reqsz;
size_t	l_off;
ssize_t	l_sent;
void	*l_reason = NULL;
char	l_text[T2R$K_CTL_RECMAX];
AVPROTO_HDR	l_hdr;
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

	l_reqsz = sizeof(AVPROTO_HDR) + be32toh(a_req->pdu.r_hdr.u_len);

	for (l_off = 0; l_off < l_reqsz; l_off += (size_t) l_sent)
		{
		if ( 0 > (l_sent = send(l_sd, a_req->raw + l_off, l_reqsz - l_off, MSG_NOSIGNAL)) )
			{
			l_rc = errno;
			close(l_sd);
			return	a_quiet ? (errno = l_rc, T2R$__CTLNOGW) : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc);
			}
		}

	/* The header: the magic, our sequence number, a sane length */
	if ( s_recvn(l_sd, &l_hdr, sizeof(l_hdr)) )
		{
		l_rc = errno ? errno : ECONNRESET;				/* EOF: the gateway has dropped us */
		close(l_sd);
		return	a_quiet ? (errno = l_rc, T2R$__CTLNOGW) : $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc);
		}

	if ( !(1 & __tlv$hget(&l_hdr, (const unsigned char *) T2R$K_CTL_MAGIC, &l_len, &l_csr, &l_seq)) )
		return	close(l_sd), $PUTMSG_FAO(T2R$__CTLBADANS, "wrong magic");

	if ( l_seq != be32toh(a_req->pdu.r_hdr.u_seq) )
		return	close(l_sd), $PUTMSG_FAO(T2R$__CTLBADANS, "wrong sequence number");

	if ( l_len > (T2R$K_CTL_ANSSZ - sizeof(AVPROTO_HDR)) )
		return	close(l_sd), $PUTMSG_FAO(T2R$__CTLBADANS, "too long");

	a_ans->pdusz = (unsigned) sizeof(AVPROTO_HDR) + l_len;

	if ( !(a_ans->pdu = malloc(a_ans->pdusz)) )
		return	close(l_sd), $PUTMSG_FAO(T2R$__CTLNOGW, a_path, ENOMEM);

	memcpy(&a_ans->pdu->r_hdr, &l_hdr, sizeof(l_hdr));

	if ( l_len && s_recvn(l_sd, a_ans->pdu->r_tlv, l_len) )
		{
		l_rc = errno;
		close(l_sd);
		return	l_rc ? $PUTMSG_FAO(T2R$__CTLNOGW, a_path, l_rc) : $PUTMSG_FAO(T2R$__CTLBADANS, "too short");
		}

	close(l_sd);

	if ( !(1 & (l_rc = s_ans_parse(a_ans))) )
		return	l_rc;

	if ( 1 & l_csr )
		return	STS$K_SUCCESS;

	/* A failure: why */
	if ( 1 & __tlv$lookup(a_ans->pdu, a_ans->pdusz, NULL, T2R$K_CTL_TAG_REASON, &l_type, &l_reason, &l_rlen) )
		snprintf(l_text, sizeof(l_text), "%.*s", (int) l_rlen, (char *) l_reason);
	else	snprintf(l_text, sizeof(l_text), "status %u, no reason given", l_csr);

	return	$PUTMSG_FAO(T2R$__CTLANSERR, l_text);
}



/*
 *   DESCRIPTION: Find a field of a record: __tlv$lookup() from the start of the record, the found TLV
 *	must end inside of it.
 *
 *   INPUTS:
 *	a_ans:		The answer
 *	a_rec:		A record of it
 *	a_tag:		T2R$K_CTL_TAG_*
 *
 *   OUTPUTS:
 *	a_type:		The type of the value
 *	a_val:		The value, inside of the PDU, in NBO
 *	a_len:		Its length
 *
 *   RETURNS:
 *	STS$K_SUCCESS, STS$K_WARN - the record has no such field
 */
static int	s_fld (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec,
		unsigned	a_tag,
		unsigned	*a_type,
		void		**a_val,
		unsigned	*a_len
		)
{
int	l_ctx = a_rec->off;

	if ( !(1 & __tlv$lookup(a_ans->pdu, a_ans->pdusz, &l_ctx, a_tag, a_type, a_val, a_len)) || (l_ctx > a_rec->end) )
		return	STS$K_WARN;

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: The number of a field (WORD, LONGWORD, QWORD); 0 if there is no such field or its
 *	length does not match the type.
 *
 *   INPUTS:
 *	a_ans, a_rec:	The answer and a record of it
 *	a_tag:		T2R$K_CTL_TAG_*
 *
 *   RETURNS:
 *	the value
 */
static uint64_t	s_num (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec,
		unsigned	a_tag
		)
{
unsigned	l_type = 0, l_len = 0;
void	*l_val = NULL;
uint16_t	l_w;
uint32_t	l_l;
uint64_t	l_q;

	if ( !(1 & s_fld(a_ans, a_rec, a_tag, &l_type, &l_val, &l_len)) )
		return	0;

	if ( (l_type == TAG$K_WORD) && (l_len == sizeof(l_w)) )
		return	memcpy(&l_w, l_val, sizeof(l_w)), be16toh(l_w);

	if ( (l_type == TAG$K_LONGWORD) && (l_len == sizeof(l_l)) )
		return	memcpy(&l_l, l_val, sizeof(l_l)), be32toh(l_l);

	if ( (l_type == TAG$K_QWORD) && (l_len == sizeof(l_q)) )
		return	memcpy(&l_q, l_val, sizeof(l_q)), be64toh(l_q);

	return	0;
}



/*
 *   DESCRIPTION: The text of a field (BBLOCK) as ASCIZ; "-" if there is no such field or it is empty.
 *
 *   INPUTS:
 *	a_ans, a_rec:	The answer and a record of it
 *	a_tag:		T2R$K_CTL_TAG_*
 *	a_bufsz:	The size of the output buffer
 *
 *   OUTPUTS:
 *	a_buf:		The text
 *
 *   RETURNS:
 *	a_buf
 */
static const char *	s_str (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec,
		unsigned	a_tag,
		char		*a_buf,
		size_t		a_bufsz
		)
{
unsigned	l_type = 0, l_len = 0;
void	*l_val = NULL;

	if ( !(1 & s_fld(a_ans, a_rec, a_tag, &l_type, &l_val, &l_len)) || (l_type != TAG$K_BBLOCK) || !l_len )
		snprintf(a_buf, a_bufsz, "-");
	else	snprintf(a_buf, a_bufsz, "%.*s", (int) l_len, (char *) l_val);

	return	a_buf;
}



/*
 *   DESCRIPTION: "a.b.c.d:port" of the ADDR and PORT fields of a record.
 *
 *   INPUTS:
 *	a_ans, a_rec:	The answer and a record of it
 *
 *   OUTPUTS:
 *	a_buf:		The text, at least 24 octets
 *
 *   RETURNS:
 *	a_buf
 */
static const char *	s_addr (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec,
		char		*a_buf
		)
{
uint32_t	l_ip = (uint32_t) s_num(a_ans, a_rec, T2R$K_CTL_TAG_ADDR);

	snprintf(a_buf, 24, "%u.%u.%u.%u:%u", (l_ip >> 24) & 0xff, (l_ip >> 16) & 0xff, (l_ip >> 8) & 0xff, l_ip & 0xff,
		(unsigned) s_num(a_ans, a_rec, T2R$K_CTL_TAG_PORT));

	return	a_buf;
}



/*
 *   DESCRIPTION: The name of a STATE field value.
 *
 *   INPUTS:
 *	a_ans, a_rec:	The answer and a record of it
 *
 *   RETURNS:
 *	the name
 */
static const char *	s_state (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec
		)
{
uint64_t	l_st = s_num(a_ans, a_rec, T2R$K_CTL_TAG_STATE);

	return	(l_st < T2R$K_CTL_STATE_MAX) ? s_state_name[l_st] : "?";
}



/*
 *   DESCRIPTION: The name of a MODE field value.
 *
 *   INPUTS:
 *	a_ans, a_rec:	The answer and a record of it
 *
 *   RETURNS:
 *	"RAW" or "MODBUS"
 */
static const char *	s_mode (
		MBUS$_ANS	*a_ans,
	const	MBUS$_REC	*a_rec
		)
{
	return	(s_num(a_ans, a_rec, T2R$K_CTL_TAG_MODE) == T2R$K_CTL_MODE_RAW) ? "RAW" : "MODBUS";
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
 *	a_ans:		The answer: LISTENER and CONN records
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_conn (
		MBUS$_ANS	*a_ans
		)
{
char	l_v[4][128], l_t[48], l_d[48];
int	l_nconn, l_total = 0, l_raw;
uint64_t	l_lsn;
long long	l_since;
time_t	l_now = time(NULL);
const MBUS$_REC	*l_rec, *l_conn;

	printf("Connections of the MODBUS gateway at %s\n", s_vmstime(l_now, l_t));

	for (int i = 0; i < a_ans->nr; i++)
		{
		if ( (l_rec = &a_ans->rec[i])->type != T2R$K_CTL_REC_LISTENER )
			continue;

		l_lsn = s_num(a_ans, l_rec, T2R$K_CTL_TAG_LSN);
		l_raw = (s_num(a_ans, l_rec, T2R$K_CTL_TAG_MODE) == T2R$K_CTL_MODE_RAW);

		printf("\nListener #%" PRIu64 "  %s  %s -> %s  %s, %" PRIu64 " of %" PRIu64 " connection(s), accepted %" PRIu64
			", rejected %" PRIu64 "\n",
			l_lsn, s_addr(a_ans, l_rec, l_v[0]), s_mode(a_ans, l_rec),
			s_str(a_ans, l_rec, T2R$K_CTL_TAG_TARGET, l_v[1], sizeof(l_v[1])), s_state(a_ans, l_rec),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_SESSIONS), s_num(a_ans, l_rec, T2R$K_CTL_TAG_CONNLM),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_ACCEPTED), s_num(a_ans, l_rec, T2R$K_CTL_TAG_REJECTED));

		l_nconn = 0;

		for (int j = a_ans->nr - 1; j >= 0; j--)				/* The gateway gives the newest first */
			{
			l_conn = &a_ans->rec[j];

			if ( (l_conn->type != T2R$K_CTL_REC_CONN) || (s_num(a_ans, l_conn, T2R$K_CTL_TAG_LSN) != l_lsn) )
				continue;

			if ( !l_nconn++ )
				printf("  %6s %4s  %-21s  %-20s  %-10s  %8s  %8s  %6s  %10s  %10s\n",
					"Id", "Sd", "Client", "Connected", "Duration", "Requests", "Answers", "Errors",
					l_raw ? "To line" : "Bytes in", l_raw ? "From line" : "Bytes out");

			if ( l_raw )
				snprintf(l_v[2], sizeof(l_v[2]), "%8s  %8s  %6s", "-", "-", "-");
			else	snprintf(l_v[2], sizeof(l_v[2]), "%8" PRIu64 "  %8" PRIu64 "  %6" PRIu64, s_num(a_ans, l_conn, T2R$K_CTL_TAG_NREQ),
					s_num(a_ans, l_conn, T2R$K_CTL_TAG_NANS), s_num(a_ans, l_conn, T2R$K_CTL_TAG_NERR));

			l_since = (long long) s_num(a_ans, l_conn, T2R$K_CTL_TAG_SINCE);

			printf("  %6" PRIu64 " %4" PRIu64 "  %-21s  %-20s  %-10s  %s  %10" PRIu64 "  %10" PRIu64 "%s\n",
				s_num(a_ans, l_conn, T2R$K_CTL_TAG_ID), s_num(a_ans, l_conn, T2R$K_CTL_TAG_SD),
				s_addr(a_ans, l_conn, l_v[3]), s_vmstime((time_t) l_since, l_t), s_delta(l_now - l_since, l_d), l_v[2],
				s_num(a_ans, l_conn, T2R$K_CTL_TAG_NRX), s_num(a_ans, l_conn, T2R$K_CTL_TAG_NTX),
				s_num(a_ans, l_conn, T2R$K_CTL_TAG_RESET) ? "  (reset)" : "");
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
 *	a_ans:		The answer: LINE records
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_lines (
		MBUS$_ANS	*a_ans
		)
{
char	l_v[2][128], l_t[48];
const MBUS$_REC	*l_rec;

	printf("Serial lines of the MODBUS gateway at %s\n\n", s_vmstime(time(NULL), l_t));
	printf("  %3s  %-24s  %-16s  %6s  %6s  %5s  %-6s  %9s  %11s\n",
		"#", "Device", "Chars", "Iotmo", "Lategd", "RS485", "State", "Listeners", "Connections");

	for (int i = 0; i < a_ans->nr; i++)
		{
		if ( (l_rec = &a_ans->rec[i])->type != T2R$K_CTL_REC_LINE )
			continue;

		snprintf(l_v[1], sizeof(l_v[1]), "%" PRIu64 ",%" PRIu64 ",%c,%" PRIu64, s_num(a_ans, l_rec, T2R$K_CTL_TAG_BAUD),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_DATABITS), (char) s_num(a_ans, l_rec, T2R$K_CTL_TAG_PARITY),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_STOPBITS));

		printf("  %3" PRIu64 "  %-24s  %-16s  %6" PRIu64 "  %6" PRIu64 "  %5s  %-6s  %9" PRIu64 "  %11" PRIu64 "\n",
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_LINE), s_str(a_ans, l_rec, T2R$K_CTL_TAG_DEVICE, l_v[0], sizeof(l_v[0])), l_v[1],
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_IOTMO), s_num(a_ans, l_rec, T2R$K_CTL_TAG_LATEGD),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_RS485) ? "yes" : "no", s_state(a_ans, l_rec),
			s_num(a_ans, l_rec, T2R$K_CTL_TAG_LISTENERS), s_num(a_ans, l_rec, T2R$K_CTL_TAG_SESSIONS));
		}
}



/*
 *   DESCRIPTION: Show the answer of SHOW GATEWAY.
 *
 *   INPUTS:
 *	a_ans:		The answer: GATEWAY and LISTENER records
 *
 *   RETURNS:
 *	NONE
 */
static void	s_disp_gw (
		MBUS$_ANS	*a_ans
		)
{
char	l_v[6][256], l_t[48], l_d[48];
long long	l_started;
const MBUS$_REC	*l_rec;

	for (int i = 0; i < a_ans->nr; i++)
		{
		l_rec = &a_ans->rec[i];

		if ( l_rec->type == T2R$K_CTL_REC_GATEWAY )
			{
			l_started = (long long) s_num(a_ans, l_rec, T2R$K_CTL_TAG_STARTED);

			printf("MODBUS gateway mbusgw-t2r %s (REV %s, %s), pid %" PRIu64 "\n",
				s_str(a_ans, l_rec, T2R$K_CTL_TAG_IDENT, l_v[0], sizeof(l_v[0])),
				s_str(a_ans, l_rec, T2R$K_CTL_TAG_REV, l_v[1], sizeof(l_v[1])),
				s_str(a_ans, l_rec, T2R$K_CTL_TAG_ARCH, l_v[2], sizeof(l_v[2])), s_num(a_ans, l_rec, T2R$K_CTL_TAG_PID));
			printf("  Started:       %s, up %s\n", s_vmstime((time_t) l_started, l_t),
				s_delta((long long) s_num(a_ans, l_rec, T2R$K_CTL_TAG_NOW) - l_started, l_d));
			printf("  Settings:      %s\n", s_str(a_ans, l_rec, T2R$K_CTL_TAG_SETTINGS, l_v[3], sizeof(l_v[3])));
			printf("  Control:       %s\n", s_str(a_ans, l_rec, T2R$K_CTL_TAG_CONTROL, l_v[4], sizeof(l_v[4])));
			printf("  Serial lines:  %" PRIu64 ", listeners: %" PRIu64 " (%" PRIu64 " up), connections: %" PRIu64 "\n\n",
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_LINES), s_num(a_ans, l_rec, T2R$K_CTL_TAG_LISTENERS),
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_UP), s_num(a_ans, l_rec, T2R$K_CTL_TAG_CONNECTIONS));
			printf("  %3s  %-21s  %-6s  %-24s  %-5s  %11s  %8s  %8s  %8s\n",
				"#", "Bind", "Mode", "Target", "State", "Connections", "Idle tmo", "Accepted", "Rejected");
			}
		else if ( l_rec->type == T2R$K_CTL_REC_LISTENER )
			{
			snprintf(l_v[5], sizeof(l_v[5]), "%" PRIu64 "/%" PRIu64, s_num(a_ans, l_rec, T2R$K_CTL_TAG_SESSIONS),
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_CONNLM));

			printf("  %3" PRIu64 "  %-21s  %-6s  %-24s  %-5s  %11s  %8" PRIu64 "  %8" PRIu64 "  %8" PRIu64 "\n",
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_LSN), s_addr(a_ans, l_rec, l_v[0]), s_mode(a_ans, l_rec),
				s_str(a_ans, l_rec, T2R$K_CTL_TAG_TARGET, l_v[2], sizeof(l_v[2])), s_state(a_ans, l_rec), l_v[5],
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_IDLETMO), s_num(a_ans, l_rec, T2R$K_CTL_TAG_ACCEPTED),
				s_num(a_ans, l_rec, T2R$K_CTL_TAG_REJECTED));
			}
		}
}



/*
 *   DESCRIPTION: The first record of a type in an answer.
 *
 *   INPUTS:
 *	a_ans:		The answer
 *	a_type:		T2R$K_CTL_REC_*
 *
 *   RETURNS:
 *	the record, NULL - there is none
 */
static const MBUS$_REC *	s_rec_first (
		MBUS$_ANS	*a_ans,
		uint16_t	a_type
		)
{
	for (int i = 0; i < a_ans->nr; i++)
		{
		if ( a_ans->rec[i].type == a_type )
			return	&a_ans->rec[i];
		}

	return	NULL;
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
 *	a_cmd:		The command, T2R$K_CTL_CMD_SHOW_*
 *	a_disp:		The display routine
 *
 *   RETURNS:
 *	STS$K_SUCCESS, or the status of the request (a single shot)
 */
static int	s_show (
		CLI_CTX		*a_ctx,
		uint32_t	a_cmd,
		void		(*a_disp) (MBUS$_ANS *)
		)
{
int	l_rc, l_secs, l_tty = isatty(STDOUT_FILENO), l_lost = 0;
unsigned	l_state = 0;
char	l_path[ASC$K_SZ + 1], l_t[48];
MBUS$_REQ	l_req;
MBUS$_ANS	l_ans;
struct timespec	l_ts;

	s_get_socket(a_ctx, &s_show_quals[MBUS$K_QUAL_SOCKET], l_path);

	if ( !(1 & __cli$present(a_ctx, &s_show_quals[MBUS$K_QUAL_CONT], &l_state)) )
		{
		s_req_init(&l_req, a_cmd);

		if ( (1 & (l_rc = s_request(l_path, &l_req, 0, &l_ans))) )
			{
			a_disp(&l_ans);

			if ( l_ans.cut )
				fflush(stdout), $PUTMSG_FAO(T2R$__CTLCUT, T2R$K_CTL_ANSSZ);
			}

		s_ans_free(&l_ans);

		return	l_rc;
		}

	if ( 0 >= (l_secs = s_get_num(a_ctx, &s_show_quals[MBUS$K_QUAL_CONT])) )
		l_secs = 1;

	while ( !s_exit_flag )
		{
		s_req_init(&l_req, a_cmd);
		l_rc = s_request(l_path, &l_req, 1, &l_ans);

		if ( l_tty )
			fputs("\033[H\033[2J", stdout);				/* Home, clear the screen */

		if ( 1 & l_rc )
			{
			a_disp(&l_ans);

			if ( l_ans.cut )
				printf("\n(the answer is cut at %u octets: not all the records are shown)\n", T2R$K_CTL_ANSSZ);

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
	return	s_show(a_ctx, T2R$K_CTL_CMD_SHOW_CONNECTIONS, s_disp_conn);
}

static int	s_act_show_lines (CLI_CTX *a_ctx, void *a_arg)
{
	return	s_show(a_ctx, T2R$K_CTL_CMD_SHOW_LINES, s_disp_lines);
}

static int	s_act_show_gw (CLI_CTX *a_ctx, void *a_arg)
{
	return	s_show(a_ctx, T2R$K_CTL_CMD_SHOW_GATEWAY, s_disp_gw);
}



/*
 *   DESCRIPTION: Put the connection selector of RESET CONNECTIONS into a request: "ALL", an id, or
 *	"a.b.c.d[:port]".
 *
 *   INPUTS:
 *	a_sel:		The selector, ASCIZ
 *
 *   OUTPUTS:
 *	a_req:		The request: ALL, ID, or ADDR [PORT]
 *
 *   RETURNS:
 *	STS$K_SUCCESS, T2R$__CTLBADSEL (signalled)
 */
static int	s_put_selector (
	const	char		*a_sel,
		MBUS$_REQ	*a_req
		)
{
char	l_ip[INET_ADDRSTRLEN], *l_end;
const char	*l_colon;
unsigned long	l_val;
uint32_t	l_l;
uint16_t	l_w;
size_t	l_len;
struct in_addr	l_addr;

	if ( !strcasecmp(a_sel, "ALL") )
		{
		l_l = 1;
		return	__tlv$put(&a_req->pdu, sizeof(MBUS$_REQ), T2R$K_CTL_TAG_ALL, TAG$K_LONGWORD, &l_l, sizeof(l_l));
		}

	if ( isdigit((unsigned char) *a_sel) && !strchr(a_sel, '.') )		/* A connection id */
		{
		errno = 0;
		l_val = strtoul(a_sel, &l_end, 10);

		if ( errno || *l_end || !l_val || (l_val > UINT32_MAX) )
			return	$PUTMSG_FAO(T2R$__CTLBADSEL, a_sel);

		l_l = (uint32_t) l_val;

		return	__tlv$put(&a_req->pdu, sizeof(MBUS$_REQ), T2R$K_CTL_TAG_ID, TAG$K_LONGWORD, &l_l, sizeof(l_l));
		}

	/* a.b.c.d or a.b.c.d:port */
	l_len = (l_colon = strchr(a_sel, ':')) ? (size_t) (l_colon - a_sel) : strlen(a_sel);

	if ( l_len >= sizeof(l_ip) )
		return	$PUTMSG_FAO(T2R$__CTLBADSEL, a_sel);

	memcpy(l_ip, a_sel, l_len);
	l_ip[l_len] = '\0';

	if ( 1 != inet_pton(AF_INET, l_ip, &l_addr) )
		return	$PUTMSG_FAO(T2R$__CTLBADSEL, a_sel);

	l_l = ntohl(l_addr.s_addr);
	__tlv$put(&a_req->pdu, sizeof(MBUS$_REQ), T2R$K_CTL_TAG_ADDR, TAG$K_LONGWORD, &l_l, sizeof(l_l));

	if ( !l_colon )
		return	STS$K_SUCCESS;

	l_val = strtoul(l_colon + 1, &l_end, 10);

	if ( *l_end || !$ISINRANGE(l_val, T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
		return	$PUTMSG_FAO(T2R$__CTLBADSEL, a_sel);

	l_w = (uint16_t) l_val;

	return	__tlv$put(&a_req->pdu, sizeof(MBUS$_REQ), T2R$K_CTL_TAG_PORT, TAG$K_WORD, &l_w, sizeof(l_w));
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
int	l_rc;
unsigned	l_state = 0;
ASC	l_val = {0};
char	l_path[ASC$K_SZ + 1], l_sel[ASC$K_SZ + 1], l_peer[24];
MBUS$_REQ	l_req;
MBUS$_ANS	l_ans;
const MBUS$_REC	*l_rec;

	s_get_socket(a_ctx, &s_reset_quals[MBUS$K_QUAL_SOCKET], l_path);

	if ( 1 & __cli$present(a_ctx, &s_reset_quals[MBUS$K_QUAL_ALL], &l_state) )
		snprintf(l_sel, sizeof(l_sel), "ALL");
	else if ( (1 & __cli$get_value(a_ctx, &s_reset_params[0], &l_val)) && $ASCLEN(&l_val) )
		snprintf(l_sel, sizeof(l_sel), "%.*s", $ASC(&l_val));
	else	return	$PUTMSG_FAO(T2R$__CTLBADSEL, "(none)");

	s_req_init(&l_req, T2R$K_CTL_CMD_RESET_CONNECTIONS);

	if ( !(1 & (l_rc = s_put_selector(l_sel, &l_req))) )
		return	l_rc;

	if ( (1 & (l_rc = s_request(l_path, &l_req, 0, &l_ans))) )
		{
		for (int i = 0; i < l_ans.nr; i++)
			{
			if ( (l_rec = &l_ans.rec[i])->type != T2R$K_CTL_REC_RESET )
				continue;

			printf("  connection %" PRIu64 " from %s --- reset\n", s_num(&l_ans, l_rec, T2R$K_CTL_TAG_ID),
				s_addr(&l_ans, l_rec, l_peer));
			}

		fflush(stdout);							/* The table first, then the message */
		$PUTMSG_FAO(T2R$__CTLRESETN, (unsigned) s_num(&l_ans, &s_whole, T2R$K_CTL_TAG_COUNT));
		}

	s_ans_free(&l_ans);

	return	l_rc;
}



/*
 *   DESCRIPTION: The start time and the pid of the running gateway (SHOW GATEWAY).
 *
 *   INPUTS:
 *	a_path:		The control socket
 *	a_quiet:	1 - do not signal the "cannot reach"
 *
 *   OUTPUTS:
 *	a_started:	The start time
 *	a_pid:		The process id
 *	a_up:		The listeners which are up
 *
 *   RETURNS:
 *	condition code of s_request(), T2R$__CTLBADANS - no GATEWAY record
 */
static int	s_gw_started (
	const char	*a_path,
		int	a_quiet,
		uint64_t *a_started,
		uint64_t *a_pid,
		uint64_t *a_up
		)
{
int	l_rc;
MBUS$_REQ	l_req;
MBUS$_ANS	l_ans;
const MBUS$_REC	*l_rec;

	s_req_init(&l_req, T2R$K_CTL_CMD_SHOW_GATEWAY);

	if ( (1 & (l_rc = s_request(a_path, &l_req, a_quiet, &l_ans))) )
		{
		if ( (l_rec = s_rec_first(&l_ans, T2R$K_CTL_REC_GATEWAY)) )
			{
			*a_started = s_num(&l_ans, l_rec, T2R$K_CTL_TAG_STARTED);
			*a_pid = s_num(&l_ans, l_rec, T2R$K_CTL_TAG_PID);
			*a_up = s_num(&l_ans, l_rec, T2R$K_CTL_TAG_UP);
			}
		else	l_rc = a_quiet ? T2R$__CTLBADANS : $PUTMSG_FAO(T2R$__CTLBADANS, "no GATEWAY record");
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
uint64_t	l_started = 0, l_now = 0, l_pid = 0, l_up = 0;
char	l_path[ASC$K_SZ + 1], l_text[T2R$K_CTL_RECMAX];
MBUS$_REQ	l_req;
MBUS$_ANS	l_ans;
time_t	l_t0;

	s_get_socket(a_ctx, &s_restart_quals[MBUS$K_QUAL_SOCKET], l_path);

	/* The start time of the running gateway: a new one tells that it has restarted */
	if ( !(1 & (l_rc = s_gw_started(l_path, 0, &l_started, &l_pid, &l_up))) )
		return	l_rc;

	s_req_init(&l_req, T2R$K_CTL_CMD_RESTART_GATEWAY);
	l_rc = s_request(l_path, &l_req, 0, &l_ans);

	for (int i = 0; i < l_ans.nr; i++)					/* Why the settings are bad, or warnings */
		{
		if ( l_ans.rec[i].type == T2R$K_CTL_REC_CHECK )
			printf("  %s\n", s_str(&l_ans, &l_ans.rec[i], T2R$K_CTL_TAG_TEXT, l_text, sizeof(l_text)));
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

		if ( !(1 & s_gw_started(l_path, 1, &l_now, &l_pid, &l_up)) || (l_now == l_started) )
			continue;						/* Not yet */

		return	$PUTMSG_FAO(T2R$__CTLBACK, (int) (time(NULL) - l_t0), (unsigned) l_pid, (unsigned) l_up), STS$K_SUCCESS;
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
