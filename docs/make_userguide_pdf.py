#!/usr/bin/env python3
#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	DESCRIPTION: Generator of the User Guide PDFs (English + Russian) in the classic
#		DEC/VSI documentation style: the title page with the document number block,
#		the legal page, a dotted-leaders table of contents, the numbered Preface,
#		chapters/sections, ruled tables, monospaced examples and vector diagrams.
#		The front matter is numbered by the roman numerals, the body - by the arabic
#		ones, exactly as the VSI manuals do.
#
#	USAGE:
#		$ python3 make_userguide_pdf.py <EN|RU> <output.pdf>
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 25-AUG-2026
#
#	MODIFICATION HISTORY:
#
#	14-OCT-2026	RRL	October 2026 edition: the Related Documents mention the GitLab CI build;
#				the Russian guide points to README_RU.md.
#
#	14-OCT-2026	RRL	X.00-13ECO01: TXDRAINTMO; the troubleshooting rows for TXDRAINTMO and for
#				DEVLOCKERR/DEVBUSY on one port.
#
#	14-OCT-2026	RRL	X.00-13ECO02: NETPOLLEV is a W; the message codes reference had 63 rows twice
#				(the X.00-11 insertion script was run again) - removed.
#
#	21-OCT-2026	RRL	X.00-13ECO03: the Legal Notice names the license (Apache License 2.0).
#
#	28-OCT-2026	RRL	X.00-13ECO04: the version follows the release (libconfig is linked statically).
#
#	28-OCT-2026	RRL	X.00-13ECO05: the version follows the release (the .deb packages).
#
#	28-OCT-2026	RRL	X.00-13ECO06: 4.2 On OpenWrt (the .ipk/.apk packages, the procd service).
#
#	02-NOV-2026	CLD	X.00-13ECO09 (November 2026 edition): lategd, iotmo with a margin, a replug of an
#				USB adapter and the permanent device names (3.1); the troubleshooting rows and
#				the codes ECHOMISM, LATEANS, DEVLOST, DEVABSENT, exception 10; ICRNL on old releases.
#
#	03-NOV-2026	CLD	X.00-14ECO02 (November 2026 edition, the 2nd issue): the RAW (transparent) listener mode (3.3),
#				the keys mode and takeover, the codes RAWSTART, RAWSTOP, RAWKICK, RAWBUSY, LSNBADMODE, LSNRAWSHR,
#				LSNRAWOPT; the binary-clean serial input; a Linux program as a RAW client (serial2net);
#				the packages in the examples are 0.14.2.
#
#	04-NOV-2026	CLD	V.01-00 (the first release): the version on the cover, the packages in the examples are 1.0.0.
#
#	06-NOV-2026	CLD	V.01-01: 4.3 mbus-ctl (watching and controlling a running gateway), the options /control
#				and /check, the codes CTLREADY .. CTLRESETN (the version on the cover is not changed yet).
#
#	07-NOV-2026	CLD	V.01-01: the control channel speaks AVProto TLV - no socat example any more;
#				the codes CTLBADSEL and CTLCUT.
#
#	17-NOV-2026	CLD	V.01-01ECO01: PEERCLOSE also tells a request dropped because its client has
#				closed while it waited for a busy line; a half-closed client.
#
#	21-NOV-2026	CLD	V.01-01ECO03: the package names in the OpenWrt examples are 1.1.3.
#
#	22-NOV-2026	CLD	V.01-01ECO04: the serials key flow (NONE, RTS/CTS, XON/XOFF), the codes SERBADFLOW,
#				FLOWNOTSET, LSNXONRTU, RAWSTALL; TXDRAINTMO on an RTS/CTS port; the package names in the examples are 1.1.4.
#
#	22-NOV-2026	CLD	V.01-02: the version on the cover, the package names in the examples are 1.2.0.
#
#	22-NOV-2026	CLD	V.01-02ECO01: TXDRAINTMO on an RTS/CTS port: a USB adapter gives NOANSWER; the package names
#				in the examples are 1.2.1.
#
#	22-NOV-2026	CLD	V.01-02ECO02: lategd is iotmo / 2 (at least 100) by default, the risk of a late answer to a read;
#				the code LATEPRETX; the package names in the examples are 1.2.2.
#
#	22-NOV-2026	CLD	V.01-03: the settings file is read and checked by yaLIBCFG: the whole file is checked by a schema
#				before anything is applied, all the problems at once (3.1), the code CFGREJECT and the CFG messages (7.3);
#				a wrong record is no longer skipped - the file is refused; the codes CFGPARSE, CFGNOSECT, CFGTRUNC,
#				SERBADREC, SERBADOPT, SERBADCHR, SERRANGE, SERBADPAR, SERBADFLOW, NOSERIALS, LSNBADREC, LSNBADBIND,
#				LSNRANGE, LSNBADADDR, LSNNOTGT, NOLSNRS, LSNOPTRNG, LSNBADMODE, LSNRAWSHR, LSNRAWOPT, LSNXONRTU are
#				removed; the new ranges (iotmo, ts_*); the package names in the examples are 1.3.0.
#
#	22-NOV-2026	CLD	V.01-03ECO01: CFGREJECT tells the number of the problems; numbers with a leading zero are octal,
#				the ! comment (yaLIBCFG X.01-03, 3.1); the package names in the examples are 1.3.1.
#
#	23-NOV-2026	CLD	V.01-04: a bad value of a setting which has a default is a warning (CFG-W, the default is taken), not a
#				refusal of the file; the new sub-section "Bad values and defaults" in 3.1; chars part by part, the speed is a real
#				line rate; the codes CFGDEFAULTS, CHARSDEF; CFGREJECT counts the errors only; yaLIBCFG X.01-04; the version
#				on the cover and the package names in the examples are 1.4.0.
#
#	23-NOV-2026	CLD	V.01-04ECO01: the program and the settings in the system paths (/usr/sbin, /etc/mbusgw-t2r;
#				/usr/local after a manual install, PR #5); the version on the cover and the package names in the examples are 1.4.1.
#
#	23-NOV-2026	CLD	V.01-04ECO02: the version on the cover and the package names in the examples are 1.4.2.
#
#	23-NOV-2026	CLD	V.01-04ECO03: the version on the cover and the package names in the examples are 1.4.3.
#
#	25-NOV-2026	CLD	V.01-04ECO04: the version on the cover and the package names in the examples are 1.4.4.
#--
import sys

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer,
	PageBreak, Table, TableStyle, Preformatted, NextPageTemplate, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.graphics.shapes import Drawing, Rect, Line, String, Polygon, Group

# ----------------------------------------------------------------------------------------------
#	Fonts: the DejaVu family covers both the Latin and the Cyrillic scripts
# ----------------------------------------------------------------------------------------------
FDIR = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Serif',   FDIR + 'DejaVuSerif.ttf'))
pdfmetrics.registerFont(TTFont('SerifB',  FDIR + 'DejaVuSerif-Bold.ttf'))
pdfmetrics.registerFont(TTFont('SerifI',  FDIR + 'DejaVuSerif-Italic.ttf'))
pdfmetrics.registerFont(TTFont('Sans',    FDIR + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('SansB',   FDIR + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono',    FDIR + 'DejaVuSansMono.ttf'))
pdfmetrics.registerFont(TTFont('MonoB',   FDIR + 'DejaVuSansMono-Bold.ttf'))

PAGE_W, PAGE_H = A4
LM, RM, TM, BM = 25*mm, 22*mm, 22*mm, 22*mm

VERSION   = 'V.01-04ECO04'
PUBDATE_EN = 'November 2026'
PUBDATE_RU = 'Ноябрь 2026'
ORG       = 'NaPi World & StarLet Squad collaboration'

# ----------------------------------------------------------------------------------------------
#	Styles (the VSI look: serif body, bold sans-ish headings, mono examples)
# ----------------------------------------------------------------------------------------------
S = {}
S['body']   = ParagraphStyle('body',   fontName='Serif',  fontSize=10, leading=13.5,
	spaceAfter=6, alignment=TA_LEFT)
S['bodyi']  = ParagraphStyle('bodyi',  parent=S['body'], fontName='SerifI')
S['chap']   = ParagraphStyle('chap',   fontName='SerifB', fontSize=20, leading=24,
	spaceBefore=0, spaceAfter=18)
S['sec']    = ParagraphStyle('sec',    fontName='SerifB', fontSize=13.5, leading=17,
	spaceBefore=14, spaceAfter=7)
S['subsec'] = ParagraphStyle('subsec', fontName='SerifB', fontSize=11.5, leading=15,
	spaceBefore=10, spaceAfter=5)
S['code']   = ParagraphStyle('code',   fontName='Mono',   fontSize=8.4, leading=10.6,
	leftIndent=6*mm, spaceBefore=4, spaceAfter=8)
S['tcell']  = ParagraphStyle('tcell',  fontName='Serif',  fontSize=9,  leading=11.6)
S['tcellm'] = ParagraphStyle('tcellm', fontName='Mono',   fontSize=8.4, leading=10.8)
S['thead']  = ParagraphStyle('thead',  fontName='SerifB', fontSize=9,  leading=11.6)
S['caption']= ParagraphStyle('caption',fontName='SerifB', fontSize=9.5, leading=12,
	spaceBefore=4, spaceAfter=10, alignment=TA_CENTER)
S['title1'] = ParagraphStyle('title1', fontName='SerifB', fontSize=24, leading=30)
S['title2'] = ParagraphStyle('title2', fontName='Serif',  fontSize=17, leading=22)
S['tmeta']  = ParagraphStyle('tmeta',  fontName='Serif',  fontSize=10.5, leading=16)
S['legal']  = ParagraphStyle('legal',  fontName='Serif',  fontSize=8.5, leading=11,
	spaceAfter=6)
S['toc0']   = ParagraphStyle('toc0', fontName='SerifB', fontSize=10.5, leading=15, leftIndent=0)
S['toc1']   = ParagraphStyle('toc1', fontName='Serif',  fontSize=9.5, leading=13, leftIndent=8*mm)

TBL_STYLE = TableStyle([
	('FONT',          (0,0), (-1,-1), 'Serif', 9),
	('LINEABOVE',     (0,0), (-1,0),  1.0, colors.black),
	('LINEBELOW',     (0,0), (-1,0),  0.6, colors.black),
	('LINEBELOW',     (0,-1),(-1,-1), 1.0, colors.black),
	('VALIGN',        (0,0), (-1,-1), 'TOP'),
	('TOPPADDING',    (0,0), (-1,-1), 3),
	('BOTTOMPADDING', (0,0), (-1,-1), 3),
	('LEFTPADDING',   (0,0), (-1,-1), 4),
	('RIGHTPADDING',  (0,0), (-1,-1), 4),
])

def tbl (a_head, a_rows, a_widths, a_mono_cols=()):
	l_data = [[Paragraph(h, S['thead']) for h in a_head]]
	for l_r in a_rows:
		l_data.append([Paragraph(c, S['tcellm'] if i in a_mono_cols else S['tcell'])
			for i, c in enumerate(l_r)])
	l_t = Table(l_data, colWidths=a_widths, repeatRows=1)
	l_t.setStyle(TBL_STYLE)
	return l_t

# ----------------------------------------------------------------------------------------------
#	Vector diagrams
# ----------------------------------------------------------------------------------------------
def s_box (a_g, a_x, a_y, a_w, a_h, a_lines, a_fs=8.5, a_fill=colors.whitesmoke, a_font='Sans'):
	a_g.add(Rect(a_x, a_y, a_w, a_h, fillColor=a_fill, strokeColor=colors.black, strokeWidth=1))
	l_n = len(a_lines)
	for i, l_t in enumerate(a_lines):
		l_ty = a_y + a_h/2 + (l_n - 1 - 2*i) * (a_fs + 2)/2 - a_fs*0.35
		a_g.add(String(a_x + a_w/2, l_ty, l_t, fontName=a_font, fontSize=a_fs,
			textAnchor='middle'))

def s_arrow (a_g, a_x1, a_y1, a_x2, a_y2, a_both=False):
	a_g.add(Line(a_x1, a_y1, a_x2, a_y2, strokeWidth=1.2))
	import math
	l_ang = math.atan2(a_y2 - a_y1, a_x2 - a_x1)
	for (l_tx, l_ty, l_a) in ([(a_x2, a_y2, l_ang)] + ([(a_x1, a_y1, l_ang + math.pi)] if a_both else [])):
		l_p1 = (l_tx - 7*math.cos(l_a - 0.42), l_ty - 7*math.sin(l_a - 0.42))
		l_p2 = (l_tx - 7*math.cos(l_a + 0.42), l_ty - 7*math.sin(l_a + 0.42))
		a_g.add(Polygon([l_tx, l_ty, l_p1[0], l_p1[1], l_p2[0], l_p2[1]],
			fillColor=colors.black, strokeColor=colors.black))

def dia_architecture (a_L):
	"""The data path: TCP clients -> gateway -> RS-485 bus with the devices."""
	l_d = Drawing(460, 170)
	g = Group(); l_d.add(g)
	# TCP clients (stacked)
	for i, l_dy in enumerate((110, 75, 40)):
		s_box(g, 6, l_dy, 92, 26, [a_L['dia_client'] + ' ' + str(i + 1)], 8)
	g.add(String(52, 20, a_L['dia_tcpside'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	# Gateway
	s_box(g, 168, 52, 130, 68, ['mbusgw-t2r', a_L['dia_queue']], 9, colors.Color(0.88, 0.92, 1))
	# arrows clients -> gateway
	for l_dy in (123, 88, 53):
		s_arrow(g, 98, l_dy, 168, 92, a_both=True)
	g.add(String(133, 128, 'MODBUS TCP', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	# serial line to the bus
	s_arrow(g, 298, 86, 356, 86, a_both=True)
	g.add(String(327, 94, 'RS-485', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	g.add(String(327, 72, 'MODBUS RTU', fontName='Sans', fontSize=7.5, textAnchor='middle'))
	# bus with devices
	g.add(Line(356, 86, 454, 86, strokeWidth=2))
	for i, l_x in enumerate((362, 396, 430)):
		g.add(Line(l_x + 10, 86, l_x + 10, 64, strokeWidth=1))
		s_box(g, l_x, 38, 22, 26, [str(i + 1)], 8)
	g.add(String(405, 20, a_L['dia_slaves'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	return l_d

def dia_lifecycle (a_L):
	"""A request lifecycle with the exception path."""
	l_d = Drawing(460, 210)
	g = Group(); l_d.add(g)
	s_box(g, 4,   150, 100, 40, [a_L['dia_client'], 'MODBUS TCP'], 8)
	s_box(g, 150, 150, 112, 40, [a_L['dia_recv'], 'MBAP'], 8)
	s_box(g, 308, 150, 118, 40, [a_L['dia_tx'], '+ CRC16'], 8)
	s_box(g, 308, 78,  118, 40, [a_L['dia_rx'], 't3.5 / ' + a_L['dia_tmo']], 8)
	s_box(g, 150, 78,  112, 40, [a_L['dia_check'], 'CRC16'], 8)
	s_box(g, 4,   78,  100, 40, [a_L['dia_send'], 'MODBUS TCP'], 8)
	s_box(g, 150, 12,  276, 34, [a_L['dia_exc']], 8, colors.Color(1, 0.92, 0.88))
	s_arrow(g, 104, 170, 150, 170)
	s_arrow(g, 262, 170, 308, 170)
	s_arrow(g, 367, 150, 367, 118)
	s_arrow(g, 308, 98,  262, 98)
	s_arrow(g, 150, 98,  104, 98)
	# error path: rx/check -> exception -> send
	s_arrow(g, 367, 78, 367, 46)
	s_arrow(g, 206, 78, 206, 46)
	s_arrow(g, 150, 29, 54, 29)
	g.add(Line(54, 29, 54, 78, strokeWidth=1.2))
	s_arrow(g, 54, 60, 54, 78)
	g.add(String(230, 196, a_L['dia_okpath'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	g.add(String(230, 2,  a_L['dia_errpath'], fontName='SerifI', fontSize=8, textAnchor='middle'))
	return l_d

def dia_ts_regs (a_L):
	"""The Time Stamp registers layout R0..R8."""
	l_d = Drawing(460, 96)
	g = Group(); l_d.add(g)
	l_x = 6
	l_lbl = ['R0', 'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7', 'R8']
	l_bit = ['63..48', '47..32', '31..16', '15..0', '63..48', '47..32', '31..16', '15..0', '']
	for i in range(9):
		l_fill = colors.Color(0.88, 0.92, 1) if i < 4 else (colors.Color(0.88, 1, 0.9) if i < 8 else colors.Color(1, 0.97, 0.85))
		s_box(g, l_x, 42, 46, 30, [l_lbl[i]], 9, l_fill)
		if l_bit[i]:
			g.add(String(l_x + 23, 30, l_bit[i], fontName='Mono', fontSize=6.6, textAnchor='middle'))
		l_x += 50
	g.add(String(6 + 2*50 - 2,   84, a_L['dia_secs'],  fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(6 + 6*50 - 2,   84, a_L['dia_nsecs'], fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(6 + 8*50 + 23,  84, a_L['dia_tz'],    fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(230, 8, a_L['dia_assemble'], fontName='Mono', fontSize=8, textAnchor='middle'))
	return l_d

def dia_msg_anatomy (a_L):
	"""The anatomy of a log message line."""
	l_d = Drawing(460, 110)
	g = Group(); l_d.add(g)
	l_msg = '%T2R-E-CRC16ERR, [#4:</dev/ttyUSB0>] RTU CRC16 check error ...'
	g.add(String(20, 62, l_msg, fontName='Mono', fontSize=9))
	# underline pieces: %T2R(4ch) -E(2) -CRC16ERR(9)
	l_cw = 5.42						# DejaVu Sans Mono advance at 9pt
	l_x0 = 20
	for (l_off, l_len, l_tx, l_lx) in (
			(0, 4,  a_L['dia_fac'], 30),
			(4, 2,  a_L['dia_sev'], 150),
			(6, 9,  a_L['dia_code'], 290)):
		l_a = l_x0 + l_off*l_cw
		l_b = l_a + l_len*l_cw
		g.add(Line(l_a, 58, l_b, 58, strokeWidth=1.4))
		g.add(Line((l_a + l_b)/2, 58, (l_a + l_b)/2, 44, strokeWidth=0.9))
		g.add(Line((l_a + l_b)/2, 44, l_lx, 36, strokeWidth=0.9))
		g.add(String(l_lx, 26, l_tx, fontName='Sans', fontSize=8, textAnchor='middle'))
	g.add(String(20, 88, a_L['dia_msgline'], fontName='SerifI', fontSize=8.5))
	return l_d

# ----------------------------------------------------------------------------------------------
#	The document template: VSI-style running heads, roman front matter / arabic body
# ----------------------------------------------------------------------------------------------
def s_roman (a_n):
	l_map = [(10, 'x'), (9, 'ix'), (5, 'v'), (4, 'iv'), (1, 'i')]
	l_out = ''
	while a_n > 0:
		for l_v, l_s in l_map:
			if a_n >= l_v:
				l_out += l_s; a_n -= l_v; break
	return l_out

class GuideDoc (BaseDocTemplate):
	def __init__ (self, a_fn, a_L, **kw):
		super().__init__(a_fn, pagesize=A4, leftMargin=LM, rightMargin=RM,
			topMargin=TM, bottomMargin=BM, **kw)
		self.m_L = a_L
		self.m_bodypage = 10**9			# The first arabic (body) page, found at pass 1
		l_frame = Frame(LM, BM, PAGE_W - LM - RM, PAGE_H - TM - BM, id='main')
		self.addPageTemplates([
			PageTemplate(id='Title', frames=[l_frame], onPage=self.s_pg_title),
			PageTemplate(id='Front', frames=[l_frame], onPage=self.s_pg_front),
			PageTemplate(id='Body',  frames=[l_frame], onPage=self.s_pg_body),
		])

	def s_head (self, a_cv):
		a_cv.setFont('Serif', 8.5)
		a_cv.drawCentredString(PAGE_W/2, PAGE_H - 14*mm, self.m_L['runhead'])
		a_cv.setLineWidth(0.5)
		a_cv.line(LM, PAGE_H - 16*mm, PAGE_W - RM, PAGE_H - 16*mm)

	def s_pg_title (self, a_cv, a_doc):
		pass

	def s_pg_front (self, a_cv, a_doc):
		self.s_head(a_cv)
		a_cv.setFont('Serif', 9)
		a_cv.drawCentredString(PAGE_W/2, 12*mm, s_roman(a_doc.page))

	def s_pg_body (self, a_cv, a_doc):
		if a_doc.page < self.m_bodypage:
			self.m_bodypage = a_doc.page
		self.s_head(a_cv)
		a_cv.setFont('Serif', 9)
		a_cv.drawCentredString(PAGE_W/2, 12*mm, str(a_doc.page - self.m_bodypage + 1))

	def afterFlowable (self, a_fl):
		if isinstance(a_fl, Paragraph) and a_fl.style.name in ('chap', 'sec'):
			l_lvl = 0 if a_fl.style.name == 'chap' else 1
			l_txt = a_fl.getPlainText()

			if l_txt == self.m_L['toc_h']:			# The Contents itself is not listed
				return

			l_key = 'k' + str(abs(hash(l_txt)) % 10**8)
			self.canv.bookmarkPage(l_key)
			self.notify('TOCEntry', (l_lvl, l_txt, self.page, l_key))

# ----------------------------------------------------------------------------------------------
#	The content, parameterized by the language pack
# ----------------------------------------------------------------------------------------------
def build (a_L, a_out):
	l_doc = GuideDoc(a_out, a_L, title=a_L['doctitle'], author=ORG)
	l_st = []

	# --- The title page (the VSI canon) ---
	l_st.append(Spacer(1, 30*mm))
	l_st.append(Paragraph('mbusgw-t2r', S['title1']))
	l_st.append(Paragraph(a_L['doctitle'], S['title2']))
	l_st.append(Spacer(1, 18*mm))
	for l_k, l_v in a_L['titleblock']:
		l_st.append(Paragraph('<b>%s</b> %s' % (l_k, l_v), S['tmeta']))
	l_st.append(Spacer(1, 42*mm))
	l_st.append(Paragraph(ORG, S['tmeta']))
	l_st.append(NextPageTemplate('Front'))
	l_st.append(PageBreak())

	# --- The legal page ---
	l_st.append(Paragraph(a_L['runhead'], S['legal']))
	l_st.append(Spacer(1, 8*mm))
	l_st.append(Paragraph(a_L['copyright'], S['legal']))
	l_st.append(Paragraph('<b>%s</b>' % a_L['legal_h'], S['legal']))
	for l_p in a_L['legal']:
		l_st.append(Paragraph(l_p, S['legal']))
	l_st.append(PageBreak())

	# --- The table of contents ---
	l_toc = TableOfContents()
	l_toc.levelStyles = [S['toc0'], S['toc1']]
	l_toc.dotsMinLevel = 0

	#
	# The displayed page number follows the VSI canon: the roman numerals for the front
	# matter, the arabic ones (restarted from 1) for the body. The body start page becomes
	# known after the first pass of multiBuild() and is applied by the subsequent passes.
	#
	def s_pgfmt (a_p):
		if l_doc.m_bodypage >= 10**9:
			return	str(a_p)
		if a_p >= l_doc.m_bodypage:
			return	str(a_p - l_doc.m_bodypage + 1)
		return	s_roman(a_p)

	l_toc.formatter = s_pgfmt
	l_st.append(Paragraph(a_L['toc_h'], S['chap']))
	l_st.append(l_toc)
	l_st.append(PageBreak())

	# --- The Preface (numbered subsections, per the VSI canon) ---
	l_st.append(Paragraph(a_L['preface_h'], S['chap']))
	for l_n, (l_h, l_paras) in enumerate(a_L['preface'], 1):
		l_st.append(Paragraph('%d. %s' % (l_n, l_h), S['sec']))
		for l_p in l_paras:
			l_st.append(Paragraph(l_p, S['body']))
	l_st.append(Paragraph('%d. %s' % (len(a_L['preface']) + 1, a_L['conv_h']), S['sec']))
	l_st.append(tbl([a_L['conv_c1'], a_L['conv_c2']], a_L['conventions'],
		[38*mm, 125*mm], a_mono_cols=(0,)))
	l_st.append(NextPageTemplate('Body'))
	l_st.append(PageBreak())

	# --- The chapters ---
	l_fig = [0]
	def fig (a_dia, a_cap):
		l_fig[0] += 1
		return KeepTogether([Spacer(1, 3*mm), a_dia,
			Paragraph(a_L['fig_w'] % l_fig[0] + a_cap, S['caption'])])

	for l_ch in a_L['chapters'](fig):
		for l_el in l_ch:
			l_st.append(l_el)
		l_st.append(PageBreak())
	del l_st[-1]

	l_doc.multiBuild(l_st)

# ----------------------------------------------------------------------------------------------
#	Helper constructors used by the language packs
# ----------------------------------------------------------------------------------------------
def P  (a_t): return Paragraph(a_t, S['body'])
def PI (a_t): return Paragraph(a_t, S['bodyi'])
def C  (a_t): return Preformatted(a_t, S['code'])
def H1 (a_t): return Paragraph(a_t, S['chap'])
def H2 (a_t): return Paragraph(a_t, S['sec'])
def H3 (a_t): return Paragraph(a_t, S['subsec'])

# ----------------------------------------------------------------------------------------------
#	The ENGLISH language pack
# ----------------------------------------------------------------------------------------------
def s_pack_en ():
	L = {}
	L['doctitle'] = 'User Guide'
	L['runhead']  = 'mbusgw-t2r User Guide'
	L['titleblock'] = [
		('Document Number:', 'DO-T2RUG-EN-01A'),
		('Publication Date:', PUBDATE_EN),
		('Revision Update Information:', 'This is a new manual.'),
		('Operating System:', 'Linux (glibc), kernel 4.x or later'),
		('Software Version:', 'mbusgw-t2r ' + VERSION),
	]
	L['copyright'] = 'Copyright © 2026 %s' % ORG
	L['legal_h'] = 'Legal Notice'
	L['legal'] = [
		'The information contained herein is subject to change without notice. '
		'%s shall not be liable for technical or editorial errors or omissions contained herein.' % ORG,
		'The software described in this manual is licensed under the Apache License, Version 2.0; '
		'see the LICENSE and NOTICE files of the distribution.',
		'The software described in this manual is distributed in the hope that it will be useful, '
		'but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS '
		'FOR A PARTICULAR PURPOSE.',
		'MODBUS is a registered trademark of Schneider Electric USA, Inc. '
		'Linux is a registered trademark of Linus Torvalds. '
		'UNIX is a registered trademark of The Open Group.',
	]
	L['toc_h'] = 'Contents'
	L['preface_h'] = 'Preface'
	L['preface'] = [
		('About This Manual', [
			'This manual describes the configuration, the operation and the troubleshooting of '
			'<b>mbusgw-t2r</b> --- a gateway which translates MODBUS TCP requests coming from the '
			'network into MODBUS RTU transactions on a serial (RS-485 or RS-232) line, and returns '
			'the answers back.']),
		('Intended Audience', [
			'This manual is written for a reader with no prior MODBUS or Linux administration '
			'experience: every step is spelled out and nothing is assumed. An experienced engineer '
			'may go straight to Chapter 3 (the settings file) and Chapter 7 (the troubleshooting).']),
		('Document Structure', [
			'Chapter 1 explains what the gateway does. Chapter 2 is the pre-flight checklist. '
			'Chapter 3 describes the settings file key by key. Chapter 4 covers the start, the '
			'command line options and the first end-to-end test. Chapter 5 describes the built-in '
			'Time Stamp pseudo device. Chapter 6 teaches how to read the log. Chapter 7 is the '
			'troubleshooting reference: the symptom, the cause, the action, and the reference of '
			'all the message codes.']),
		('Related Documents', [
			'<i>README.md</i> --- the installation sequence (the StarLet package, the build, '
			'<font face="Mono">make install</font>) and the automatic build in GitLab CI '
			'(<font face="Mono">.gitlab-ci.yml</font>). '
			'<i>MODBUS over Serial Line Specification and Implementation Guide</i> and '
			'<i>MODBUS Application Protocol Specification</i> --- the protocol itself, '
			'available from modbus.org.']),
	]
	L['conv_h'] = 'Conventions'
	L['conv_c1'] = 'Convention'; L['conv_c2'] = 'Meaning'
	L['conventions'] = [
		('Monospace type', 'Code examples, file names, commands and interactive screen displays.'),
		('%T2R-E-CODE',    'A message code as it appears in the gateway log; see Section 7.2.'),
		('<...>',          'A placeholder to be replaced by an actual value, e.g. a device name.'),
		('$',              'The shell prompt of an unprivileged user; do not type it.'),
		('Ctrl/C',         'Hold down the key labeled Ctrl while you press the key C.'),
	]
	# Diagram labels
	L.update({
		'dia_client': 'TCP client', 'dia_tcpside': 'SCADA, mbpoll, scripts ...',
		'dia_queue': '(the request queue)', 'dia_slaves': 'RTU devices (slaves)',
		'dia_recv': 'Receive and check', 'dia_tx': 'Transmit to the line',
		'dia_rx': 'Receive the answer', 'dia_tmo': 'timeout', 'dia_check': 'Check the answer',
		'dia_send': 'Answer the client', 'dia_exc': 'Form a MODBUS exception (EXCRPT is logged)',
		'dia_okpath': 'The normal path', 'dia_errpath': 'The error path: any failure below becomes an exception answer',
		'dia_secs': 'Seconds (64 bits, the MSW first)', 'dia_nsecs': 'Nanoseconds (64 bits)',
		'dia_tz': 'TZ, min', 'dia_assemble': 'seconds = (R0<<48) | (R1<<32) | (R2<<16) | R3',
		'dia_fac': 'the facility', 'dia_sev': 'the severity: S I W E F', 'dia_code': 'the message code (Section 7.2)',
		'dia_msgline': 'Every important event is one line with a code:',
	})
	L['fig_w'] = 'Figure %d.  '

	def chapters (fig):
		return [
		# --- Chapter 1 ---
		[H1('Chapter 1.  What the Gateway Does'),
		 P('You have a device (an electricity meter, a temperature controller, a PLC ...) which '
		   'talks <b>MODBUS RTU</b> over a serial line. And you have a program (a SCADA, a script, '
		   '<font face="Mono">mbpoll</font> ...) which talks <b>MODBUS TCP</b> over the network. '
		   'The gateway sits between them and translates, as Figure 1 shows.'),
		 fig(dia_architecture(L), 'The data path of the gateway'),
		 P('Several TCP clients may connect at the same time: the gateway queues them so that '
		   'only one request at a time goes to the serial line --- an RTU line physically cannot '
		   'carry two transactions at once.'),
		 P('Several devices may share one RS-485 line. They are told apart by the <i>slave '
		   'address</i> inside the MODBUS request itself; the gateway does not need to know '
		   'their addresses at all.')],
		# --- Chapter 2 ---
		[H1('Chapter 2.  Before You Start'),
		 P('Walk this checklist once --- it saves an hour of guessing later.'),
		 P('<b>1.</b> The gateway is installed (see <i>README.md</i>). The package installs '
		   'the program <font face="Mono">/usr/sbin/mbusgw-t2r</font> and the settings file '
		   '<font face="Mono">/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>. A manual CMake install '
		   'uses <font face="Mono">/usr/local/sbin/</font> and <font face="Mono">/usr/local/etc/mbusgw-t2r/</font>.'),
		 P('<b>2.</b> You know which serial port the device is connected to. On Linux it is a file like '
		   '<font face="Mono">/dev/ttyS0</font> or <font face="Mono">/dev/ttyUSB0</font>. If unsure, '
		   'plug the USB adapter out and in, then run:'),
		 C('$ dmesg | tail'),
		 P('<b>3.</b> You know the line parameters of the device: the speed, the data bits, the parity '
		   'and the stop bits. They are in the device manual; the most common set is '
		   '<font face="Mono">9600, 8, N, 1</font>.'),
		 P('<b>4.</b> Your user may open the port. Check the group of the port file:'),
		 C('$ ls -l /dev/ttyUSB0\n'
		   'crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0'),
		 P('If the group is <font face="Mono">dialout</font>, add yourself to it and re-login '
		   '(or simply run the gateway with <font face="Mono">sudo</font>):'),
		 C('$ sudo usermod -a -G dialout $USER')],
		# --- Chapter 3 ---
		[H1('Chapter 3.  The Settings File'),
		 P('The settings file has two sections: <font face="Mono">serials</font> (the serial '
		   'lines) and <font face="Mono">listeners</font> (the TCP ports). A minimal working '
		   'example:'),
		 C('serials = (\n'
		   '\t{\tdevice = "/dev/ttyUSB0";\n'
		   '\t\tchars  = "9600, 8, N, 1";\n'
		   '\t\tiotmo  = 1000;\n'
		   '\t}\n'
		   ');\n\n'
		   'listeners = (\n'
		   '\t{\tbind   = "TCP:0.0.0.0:502";\n'
		   '\t\ttarget = "/dev/ttyUSB0";\n'
		   '\t\tconnlm = 8;\n'
		   '\t}\n'
		   ');'),
		 P('Read it as: <i>"open the port /dev/ttyUSB0 at 9600-8-N-1 and wait for an answer up '
		   'to 1000 ms; listen for TCP clients on every interface, port 502, and forward them '
		   'to that port"</i>.'),
		 H2('3.1.  The serials Section'),
		 P('One record per serial port. Table 3-1 lists every key.'),
		 tbl(['Key', 'Required', 'Meaning and the allowed values'], [
			('device', 'yes', 'The serial port file, e.g. /dev/ttyUSB0.'),
			('chars', 'yes', 'The line parameters: speed, data bits, parity, stop bits. '
				'Speed - a real line rate of the serial driver (see Bad values and defaults), 50..4000000; data bits 5..8; parity N (none), E (even), O (odd); stop bits 1..2.'),
			('iotmo', 'no', 'How long to wait for the answer of the device, milliseconds. Default 1000. '
				'Take it with a margin over the slowest device on the line: an answer which comes later is lost. Range 1..3600000.'),
			('lategd', 'no', 'After a failed request (no answer, a broken answer) listen to the line for this many '
				'milliseconds more and throw away what comes (LATEANS): a late answer must not go to the next '
				'request. Costs time only after a failure: a request to a dead device takes iotmo + lategd. '
				'0..60000, default iotmo / 2, but not less than 100; 0 --- only the 3.5 character pause, as before X.00-13ECO08. '
				'An answer to a read carries nothing of its request: a device later than iotmo + lategd can give one client '
				'the data of another - take lategd with a margin.'),
			('rs485', 'no', '1 --- ask the kernel to drive the RS-485 direction control '
				'(only for the ports which support it). Default 0.'),
			('flow', 'no', 'The flow control of the line. NONE --- none (the default). RTS/CTS --- the hardware '
				'handshake: the gateway sends only while the device holds CTS on; needs the RTS and CTS wires; not '
				'with rs485 = 1. XON/XOFF --- the device stops and starts the gateway by XOFF (0x13) and XON (0x11); '
				'only for a RAW listener: MODBUS RTU frames carry these octets, so a MODBUS listener on such a port is an error. Not case-sensitive; '
				'another value or a number (DSR/DTR too: Linux has no such flow control) is an error.'),
			('ts_enabled', 'no', '1 --- enable the Time Stamp pseudo device (Chapter 5). Default 0.'),
			('ts_unit', 'no', 'The slave address the pseudo device answers on. Range 0..255. Default 135.'),
			('ts_fncode', 'no', 'The function code it answers to. Range 1..127. Default 3.'),
			('ts_base_reg0', 'no', 'The first register of the time stamp. Range 0..65535. Default 8135.'),
		 ], [30*mm, 18*mm, 115*mm], a_mono_cols=(0,)),
		 P('The whole settings file is checked <b>before anything is applied</b>. The gateway finds the '
		   'problems at once and prints each of them with the setting path, the file, the line and the column, '
		   'for example:'),
		 C('%CFG-E-CHECK, listeners.[0]: <target> "/dev/ttyQ9" is not a <device> of <serials>,\n    settings.conf line 2 column 3\n%CFG-E-CHECK, listeners.[1].bind: UDP is not implemented, use TCP,\n    settings.conf line 3 column 5\n%CFG-E-UNKNOWN, unknown setting: listeners.[1].conlm, settings.conf line 3 column 56\n%CFG-E-DUPKEY, duplicate key: serials.[device="/dev/null"], the same as line 6,\n    settings.conf line 7 column 3\n%T2R-E-CFGREJECT, Settings <settings.conf> are refused: 4 problem(s),\n    see the CFG messages above --- nothing is applied'),
		 P('If there is at least one <b>error</b>, the <b>whole file is refused</b>: nothing is applied and the gateway '
		   'does not start (/check exits with a failure; restart gateway of mbus-ctl is refused and the gateway '
		   'goes on as it is). Fix every line shown and try again. A check which compares several settings (a '
		   'target against serials, two RAW listeners on one line, XON/XOFF under MODBUS) is made when those '
		   'settings themselves are right, so such a problem may show up only after the first ones are fixed.'),
		 P('<b>This has changed in V.01-03 and V.01-04.</b> Before V.01-03, a wrong record was skipped with a '
		   '%T2R-E-... message and the gateway went on with the rest of the file. In V.01-03 any problem refused the '
		   'whole file. Since V.01-04 only an <b>error</b> refuses the file: a bad value of a setting which has a '
		   'default is only a warning (see <i>Bad values and defaults</i> below). A file which worked before may '
		   'still be refused after an upgrade (for example because of a setting the gateway does not know): check it '
		   'with mbusgw-t2r /settings=&lt;file&gt; /check before you restart the gateway.'),
		 H3('Bad values and defaults'),
		 P('Many settings have a default. If the value of such a setting is bad (out of its range, a wrong word, a '
		   'wrong type), the gateway does <b>not</b> refuse the file. It prints a <b>warning</b> and uses the '
		   'default. The warning is a %CFG-W-... message with the file, the line and the column; it ends with - the '
		   'default is taken. After them you see %T2R-W-CFGDEFAULTS with the number of such values. The gateway '
		   'starts.'),
		 P('<b>Settings with a default</b> (a bad value is only a warning):'),
		 P('&bull; serials: chars (part by part, see below), flow (default NONE), iotmo (default 1000 msec), lategd '
		   '(default iotmo / 2, at least 100), rs485 (default 0), ts_enabled, ts_unit (default 135), ts_fncode '
		   '(default 3), ts_base_reg0 (default 8135), desc;'),
		 P('&bull; listeners: connlm (default 32; RAW: 1), idletmo (default 60 s; RAW: the maximum), iotmo, takeover '
		   '(default 1).'),
		 P('<b>Errors</b> (there is no sensible default, so the whole file is still refused): a bad device of a '
		   'serial; a bad bind, target or mode of a listener; a setting which the gateway does not know (a typo); a '
		   'missing required setting (listeners, serials, chars, bind, target, device); a duplicate device; the cross '
		   'checks (RTS/CTS with rs485 = 1; an XON/XOFF line under a MODBUS listener; the rules of a RAW listener; '
		   'two devices which differ only by capital and small letters); a syntax error.'),
		 P('<b>chars part by part.</b> chars must be in the file (a missing chars is an error), but its four parts '
		   'are checked one by one: speed, data bits, parity, stop bits. What is good stays. Each bad part gets its '
		   'default: speed 19200, data bits 8, parity E, stop bits 1 (the default of the MODBUS over Serial Line '
		   'specification). %T2R-W-CHARSDEF tells which parts were bad and what was taken.'),
		 P('The speed must be a real line rate of the serial driver: 50, 75, 110, ... 1200, 2400, 4800, 9600, 19200, '
		   '38400, 57600, 115200, 230400 ... 4000000. Before V.01-04 any number from 50 to 4000000 was taken, and a '
		   'non-standard one was silently replaced by 9600 when the port was opened (BADSPEED). Now, for example, '
		   '19201 is a warning and 19200 is taken.'),
		 P('A real output of the check of a file with bad values:'),
		 C('%CFG-W-CHECK, serials.[device="/dev/null"].chars: speed 19201 is not a line rate of\n    the serial driver (1200, 9600, 19200, 115200 ...) - the default is taken,\n    /tmp/soft.conf line 2 column 37\n%CFG-W-BADKWD, not an allowed value: serials.[device="/dev/null"].flow = "HW",\n    allowed: NONE, XON/XOFF, RTS/CTS - the default is taken,\n    /tmp/soft.conf line 2 column 63\n%CFG-W-RANGE, out of range: serials.[device="/dev/null"].iotmo = 0,\n    allowed 1..3600000 - the default is taken, /tmp/soft.conf line 2 column 76\n%T2R-W-CFGDEFAULTS, Settings </tmp/soft.conf>: 3 value(s) not usable,\n    see the CFG-W messages above --- the defaults are taken\n%T2R-W-CHARSDEF, [serial #00:</dev/null>] <chars> "19201,8,X,1" is not usable: speed,\n    parity --- taken: 19200, 8, E, 1\n%T2R-I-DEVADDED, Added device #00 [</dev/null>, Chars: <19200, 8, E, 1>, Flow: NONE,\n    Answer tmo: 1000 msec, Inter RTU PDU: 2002 usec] --- added\n%T2R-S-CHECKOK, Settings </tmp/soft.conf> --- are good: 1 serial(s), 1 listener(s)'),
		 P('<b>Be careful.</b> A default may be wrong for your line: with the default speed the gateway may not talk '
		   'to your devices at all. So look for CFG-W and CFGDEFAULTS in the log, and run mbusgw-t2r '
		   '/settings=&lt;file&gt; /check after every edit of the file. /check exits with 0 when there are only '
		   'warnings.'),
		 P('<b>Numbers.</b> Since V.01-03ECO01 a number with a leading zero is <b>octal</b>, as libconfig 1.6 and newer read '
		   'it: <font face="Mono">ts_base_reg0 = 0135;</font> is 93, not 135, and 08 or 09 is an error (CFG-E-BADNUM). '
		   'Write numbers without leading zeros (135). After an upgrade a file which worked before keeps working but '
		   'may change its meaning without a word: before V.01-03ECO01 0135 was 135 - look for = 0 followed by a digit. '
		   'Hexadecimal is 0x87 or %X87; %D, %B and %O give decimal, binary '
		   'and octal. Besides #, // and /* */, an exclamation mark ! starts a comment to the end of the line, as in DCL.'),
		 P('In the setting path, listeners.[0] is the first record of the section (the records count from zero, in '
		   'the order they appear in the file); serials.[device="/dev/null"] is the record with this device. '
		   'What the check also catches:'),
		 P('&bull; A setting which the gateway does not know is an error --- a typo never passes. The names of the '
		   'settings are not case-sensitive.'),
		 P('&bull; Two serials records with the same device are an error (DUPKEY), and so are two whose device '
		   'differs by capital and small letters only (CHECK): a target could not tell them apart.'),
		 P('&bull; At most 128 serials records and 128 listeners records.'),
		 P('&bull; The target of a listener must be a device of serials. The sample file modbus-t2r_settings.conf has '
		   'its two listeners of /dev/ttyS1 and /dev/ttyS0 commented out for this reason: add the serials records '
		   'for them first, then remove the comment marks.'),
		 P('The allowed values are in the tables of this chapter. The messages are explained in section 7.3.'),
		 P('<b>USB adapters: unplugging is fine.</b> Since X.00-13ECO09 the gateway notices that the adapter has '
		   'gone (DEVLOST) --- also when nobody is polling --- and closes the port at once. It tries to open the '
		   'port again by its name every second; meanwhile the clients get the exception 10 '
		   '(GATEWAY_PATH_UNAVAILABLE), not 4. When the adapter is back, DEVREADY is printed and the work goes '
		   'on, no restart is needed. An adapter absent at the start does not stop its TCP port (DEVABSENT).'),
		 P('<b>Use a permanent name.</b> The kernel numbers ttyUSB0, ttyUSB1, ... in the order the adapters '
		   'appear, so after a replug the same adapter may get another number. Put a permanent name into device '
		   '(and into target of the listener): the name by the USB socket from ls -l /dev/serial/by-path/ '
		   '(the ports of a 4-port FT4232H differ by the :1.0 ... :1.3 part), or your own name made by a udev rule:'),
		 C('SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-xhci-hcd.0-usb-0:1.1:1.0", SYMLINK+="rs485-1"'),
		 P('in /etc/udev/rules.d/99-rs485.rules gives /dev/rs485-1 (see udevadm info /dev/ttyUSB0 for the values).'),
		 H2('3.2.  The listeners Section'),
		 tbl(['Key', 'Required', 'Meaning and the allowed values'], [
			('bind', 'yes', 'Where to listen: TCP:&lt;IP address&gt;:&lt;port&gt;. The port is 1..65535; '
				'0.0.0.0 means every interface. UDP is not supported.'),
			('target', 'yes', 'Which serial port to forward to. Must match a device from serials '
				'(capital and small letters are not told apart); if there is none, the whole file is refused.'),
			('connlm', 'no', 'How many TCP clients may be connected to this port at the same time; one more is '
				'disconnected at once (CONNLIMIT). Default 32, range 1..512.'),
			('idletmo', 'no', 'A client which sends nothing for this many seconds is disconnected (IDLETMO). '
				'Default 60, range 1..86400.'),
			('mode', 'no', 'What the listener does: "MODBUS" (the default) is the MODBUS TCP to RTU gateway, '
				'"RAW" is a transparent stream of octets (see 3.3). Capital or small letters do not matter; '
				'any other word is an error.'),
			('takeover', 'no', 'RAW only. 1 (the default): a new client kicks the current client out (RAWKICK). '
				'0: a new client is rejected while another one is connected (RAWBUSY). In the MODBUS mode '
				'this key is an error.'),
			('iotmo', 'no', 'Reserved, not used yet.'),
		 ], [30*mm, 18*mm, 115*mm], a_mono_cols=(0,)),
		 P('Port <b>502</b> is the standard MODBUS TCP port, but on Linux the ports below 1024 '
		   'need the root privileges. If you do not want to run as root, take e.g. '
		   '<font face="Mono">5502</font> and point your client there. Several MODBUS listeners may '
		   'share one target: the gateway serializes their requests. In the RAW mode two defaults are '
		   'different: connlm may only be 1, and idletmo is 86400 (not 60). A RAW listener needs its serial '
		   'port for itself.'),
		 H2('3.3.  The RAW (Transparent) Mode'),
		 P('In the MODBUS mode the gateway understands MODBUS. In the RAW mode it does <b>not</b>. It just '
		   'carries octets from the TCP client to the serial line and from the serial line to the client. '
		   'It changes nothing. This is a "virtual COM port over the network".'),
		 P('Typical use: a Windows PC has a program for some old device. The program wants a COM port. '
		   'You install a COM-over-TCP program on the PC (it makes a virtual COM port and connects it to '
		   'the gateway by TCP). The program thinks it works with a real COM port. The device can speak '
		   '<b>any</b> serial protocol, not only MODBUS.'),
		 C('[Program on the PC] --- virtual COM --- TCP --->\n'
		   '        [mbusgw-t2r, mode RAW] --- serial line ---> [Device]'),
		 P('A settings example:'),
		 C('listeners = (\n'
		   '    { bind = "TCP:0.0.0.0:4001"; mode = "RAW"; target = "/dev/ttyUSB1"; }\n'
		   ');\n'
		   'serials = (\n'
		   '    { device = "/dev/ttyUSB1"; chars = "19200, 8, E, 1"; flow = "NONE"; iotmo = 1000; }\n'
		   ');'),
		 P('The rules of the RAW mode:'),
		 P('&bull; <b>One client at a time.</b> connlm may only be 1.'),
		 P('&bull; <b>A new client takes over</b> (takeover = 1, the default). The old connection is closed, '
		   'the new one gets the line; the log says RAWKICK. With takeover = 0 the old client stays and the '
		   'new one is refused (RAWBUSY).'),
		 P('&bull; <b>The serial port is only for this listener.</b> No other listener, MODBUS or RAW, may use '
		   'the same target (an error; the order of the records does not matter). MODBUS listeners may still '
		   'share a port among themselves.'),
		 P('&bull; <b>The speed and the parity are taken from the gateway</b>, from the serials record. If the '
		   'program on the PC sets another speed on its virtual COM port, this is <b>not</b> passed to the '
		   'line. Set the right chars in the serials record.'),
		 P('&bull; <b>A long silence is normal.</b> The default idletmo is 86400 seconds (one day). A client '
		   'which is dead (the PC crashed, the cable was cut) is found by the TCP keepalive and dropped in '
		   'about 1 minute (30 seconds of silence, then 3 probes, 10 seconds apart).'),
		 P('&bull; <b>No Time Stamp.</b> The ts_* pseudo device (Chapter 5) does not work in the RAW mode. '
		   'On a start of a session the old octets in the serial port buffers are thrown away.'),
		 P('&bull; <b>No serial port --- no session.</b> If the USB adapter is absent or has been unplugged '
		   '(DEVABSENT, DEVLOST, 3.1), a new client is disconnected at once, without any data, and a running '
		   'session is closed. When the adapter is back, the client simply connects again.'),
		 P('<b>A Linux program as the client.</b> The pseudo-serial driver serial2net (NaPi Lab, X.00-08ECO1 '
		   'and newer) creates /dev/ser2net00, /dev/ser2net01 and so on, and carries everything written there '
		   'to a TCP peer as it is. Point the peer to a RAW listener (p_ser2net_peer="127.0.0.1:4001"), and any '
		   'program which wants a serial device (mbpoll -m rtu, minicom, pyserial) works with the remote line '
		   'as with a local port. The speed is still taken from the serials record of the gateway. Tested: '
		   '1 MiB each way in blocks with CRC32 (no loss), four ports at once with mbpoll, 115200.'),
		 P('A note for both modes: the serial input is "binary-clean" since X.00-14. The port does not change '
		   'CR into LF and does not cut the 8th bit any more. Before, an octet 0x0D could arrive as 0x0A on a '
		   'port with the default settings.')],
		# --- Chapter 4 ---
		[H1('Chapter 4.  Running the Gateway'),
		 P('By hand, in a terminal (the messages go straight to the screen):'),
		 C('$ /usr/sbin/mbusgw-t2r /settings=/etc/mbusgw-t2r/modbus-t2r_settings.conf'),
		 tbl(['Option', 'Meaning'], [
			('/settings=&lt;file&gt;', 'The settings file (Chapter 3).'),
			('/trace', 'The verbose tracing: every request and answer is dumped in hex. '
				'Priceless at the first run.'),
			('/logfile=&lt;file&gt;', 'Write the log to a file instead of the screen.'),
			('/logsize=&lt;octets&gt;', 'Rotate the log file above this size.'),
			('/control=&lt;file&gt;', 'The socket file for mbus-ctl (section 4.3), NONE --- none.'),
			('/check', 'Only check the settings file and exit (section 4.3).'),
		 ], [42*mm, 121*mm], a_mono_cols=(0,)),
		 P('A healthy start prints (shortened):'),
		 C('%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)\n'
		   '%T2R-I-DEVADDED, Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added\n'
		   '%T2R-I-LSNADDED, Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added\n'
		   '%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready\n'
		   '%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready'),
		 P('Two lines matter most: <b>DEVREADY</b> (the port is open) and <b>LSNRRDY</b> (the '
		   'TCP port is listening). If you see both, the gateway is up. Figure 2 shows what '
		   'happens to every request after that.'),
		 fig(dia_lifecycle(L), 'The lifecycle of one request'),
		 P('To stop the gateway press Ctrl/C once and wait a second. To toggle the tracing of a '
		   '<i>running</i> gateway without a restart:'),
		 C('$ kill -USR1 <pid>'),
		 H2('4.1.  The First Test with mbpoll'),
		 P('<font face="Mono">mbpoll</font> is a small free MODBUS client '
		   '(<font face="Mono">sudo apt install mbpoll</font>). Read 3 holding registers from '
		   'the device with the slave address 1:'),
		 C('$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502'),
		 P('If the numbers come back --- everything works, end to end.'),
		 H2('4.2.  On OpenWrt'),
		 P('On an OpenWrt router install the package for your board from the Releases page (the '
		   'architecture is the <font face="Mono">DISTRIB_ARCH</font> line of '
		   '<font face="Mono">/etc/openwrt_release</font>): <font face="Mono">.ipk</font> for OpenWrt '
		   '24.10, <font face="Mono">.apk</font> for OpenWrt 25.12. The package needs nothing else, the '
		   'router needs no network:'),
		 C('# opkg install ./mbusgw-t2r_1.4.4-*_mipsel_24kc.ipk\n'
		   '# apk add --allow-untrusted ./mbusgw-t2r_1.4.4-*_mipsel_24kc.apk'),
		 P('The files are at other places than on Debian: the program is '
		   '<font face="Mono">/usr/sbin/mbusgw-t2r</font>, the settings file is '
		   '<font face="Mono">/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>. The service is switched '
		   'off after the install. Tune the settings file, then switch the service on and start it:'),
		 C('# uci set mbusgw-t2r.main.enabled=1 && uci commit mbusgw-t2r\n'
		   '# /etc/init.d/mbusgw-t2r start\n'
		   '# logread -e mbusgw-t2r'),
		 P('The last command shows the messages of the gateway (they go to the system log). The USB '
		   'serial adapter needs its kernel module: <font face="Mono">kmod-usb-serial-ftdi</font>, '
		   '<font face="Mono">kmod-usb-serial-ch341</font> or <font face="Mono">kmod-usb-serial-cp210x</font>, '
		   'by the chip of the adapter.'),
		 H2('4.3.  Watching and Controlling a Running Gateway: mbus-ctl'),
		 P('<font face="Mono">mbus-ctl</font> is a small program next to the gateway '
		   '(<font face="Mono">/usr/sbin/mbus-ctl</font> in packages, '
		   '<font face="Mono">/usr/local/sbin/mbus-ctl</font> after a manual install). '
		   'It asks the running gateway what it is doing, and it can disconnect a client or restart the gateway. '
		   'It talks to the gateway through a socket <b>file</b> on the same computer, '
		   '<font face="Mono">/var/run/mbusgw-t2r.sock</font>. Nothing is opened to the network. Only root and the '
		   'group of the gateway may use the file (rights 0660), so type <font face="Mono">sudo</font> in front of it.'),
		 tbl(['Command', 'What you get'], [
			('show connections', 'Who is connected to which TCP port, since when, the requests, answers, errors and octets.'),
			('show connections /continuous', 'The same, redrawn every 2 seconds until Ctrl/C (/continuous=5 --- every 5 seconds).'),
			('show lines', 'The serial ports: the device, the speed, the timeouts, OPEN or ABSENT, how many clients use it.'),
			('show gateway', 'The version, the PID, how long it is up, the settings file, the TCP ports.'),
			('reset connection 7', 'Disconnect the client with Id 7 (from show connections).'),
			('reset connection 10.0.0.7', 'Disconnect all the clients from this address (10.0.0.7:44770 --- just one).'),
			('reset connection /all', 'Disconnect all the clients.'),
			('restart gateway', 'Check the settings file and restart the gateway with it.'),
		 ], [58*mm, 105*mm], a_mono_cols=(0,)),
		 P('You may shorten the words: <font face="Mono">sho conn /cont</font>, <font face="Mono">sho gw</font>, '
		   '<font face="Mono">reset conn 7</font>, <font face="Mono">restart gw</font>. SERIALS is the same as LINES, '
		   'GW is the same as GATEWAY. Only <font face="Mono">res</font> is too short: it can be RESET or RESTART.'),
		 C('$ sudo mbus-ctl sho conn\n'
		   'Connections of the MODBUS gateway at 07-NOV-2026 00:07:44\n\n'
		   'Listener #0  0.0.0.0:502  MODBUS -> /dev/ttyUSB0  UP, 2 of 8 connection(s)\n'
		   '    Id  Sd  Client            Connected             Requests  Answers  Errors\n'
		   '     1   8  10.0.0.7:44770    07-NOV-2026 00:07:44         7        7       0\n'
		   '     2   9  10.0.0.9:44772    07-NOV-2026 00:07:44         3        3       0\n\n'
		   'Total: 2 connection(s)'),
		 P('Id is the number of the connection for reset. Sd is the number in [#8] of the log lines of this client. '
		   'Errors are the answers with a MODBUS exception plus the requests which broke the connection.'),
		 P('<b>restart gateway</b> first checks the settings file in a separate process '
		   '(<font face="Mono">mbusgw-t2r /settings=&lt;file&gt; /check</font>). If the file has an error (a warning is not an error), nothing is '
		   'restarted: you see the error line and the gateway works on with the old settings. If the file is good, the '
		   'gateway disconnects the clients, closes the ports and starts itself again with the same command line. The '
		   'PID stays the same: systemd and procd see nothing. mbus-ctl waits until the gateway answers again '
		   '(CTLBACK), in 3--15 seconds. With systemd, <font face="Mono">systemctl reload mbusgw-t2r</font> does the same. '
		   'After an upgrade of the package use <font face="Mono">systemctl restart</font>.'),
		 P('The gateway and mbus-ctl talk a small binary protocol (TLV records, see README.md). You cannot type '
		   'to the gateway by hand: always use mbus-ctl.'),
		 P('Two gateways on one computer need two socket files: <font face="Mono">/control=&lt;file&gt;</font> for the '
		   'gateway, <font face="Mono">/socket=&lt;file&gt;</font> for mbus-ctl.')],
		# --- Chapter 5 ---
		[H1('Chapter 5.  The Time Stamp Pseudo Device'),
		 P('The gateway can answer <i>by itself</i> --- without touching the line --- to a read '
		   'request sent to a special slave address. The answer is the current time of the '
		   'gateway host. This lets a SCADA read the precise time over the plain MODBUS.'),
		 P('With <font face="Mono">ts_enabled = 1; ts_unit = 135; ts_fncode = 4; '
		   'ts_base_reg0 = 135;</font> a request "read N input registers starting at 135 from '
		   'the slave 135" returns the registers of Figure 3.'),
		 fig(dia_ts_regs(L), 'The Time Stamp registers layout'),
		 tbl(['Registers requested', 'You get'], [
			('4', 'The seconds of the UNIX epoch, 64 bits, the most significant 16-bit word first.'),
			('8', 'Plus the nanoseconds, packed the same way into R4..R7.'),
			('9', 'Plus R8 = the local timezone offset, in signed minutes (e.g. Moscow = +180).'),
		 ], [40*mm, 123*mm], a_mono_cols=(0,)),
		 P('Any other register count in a TS request is answered with the MODBUS exception '
		   'ILLEGAL_DATA_ADDRESS --- this is on purpose.')],
		# --- Chapter 6 ---
		[H1('Chapter 6.  Reading the Log'),
		 fig(dia_msg_anatomy(L), 'The anatomy of a log message'),
		 P('The severity letter is: <b>S</b> --- success, <b>I</b> --- information, '
		   '<b>W</b> --- warning, <b>E</b> --- error, <b>F</b> --- fatal. The part '
		   '<font face="Mono">[#4:</font><font face="Mono">&lt;/dev/ttyUSB0&gt;]</font> means: '
		   'the file descriptor #4, this serial port.'),
		 P('Grep the log by the code --- this is exactly why the codes exist:'),
		 C('$ grep CRC16ERR /var/log/mbusgw-t2r.log')],
		# --- Chapter 7 ---
		[H1('Chapter 7.  Troubleshooting'),
		 P('<b>The golden rule: run with /trace and read the log. The gateway always says what '
		   'it dislikes.</b>'),
		 H2('7.1.  The Symptom, the Cause, the Action'),
		 tbl(['You see', 'It means / what to do'], [
			('%T2R-E-DEVOPNERR, ... errno: 2', 'The port file does not exist: the adapter is unplugged '
				'or the name is wrong. Run dmesg | tail after plugging; fix device in the settings. Since '
				'X.00-13ECO09 the TCP port is opened anyway (DEVABSENT), the device is tried every second.'),
			('%T2R-E-DEVLOST, ...', 'The USB adapter has gone (unplugged, a hub reset, a power dip); the clients '
				'get the exception 10. Nothing to do if it comes back: DEVREADY follows. If not: dmesg | tail, '
				'the cable, the power; if it came back under another ttyUSB number, use a permanent name (3.1).'),
			('%T2R-W-DEVABSENT, ...', 'At the start the serial port could not be opened (the line above says '
				'why); its TCP port works and answers the exception 10. Plug in the adapter, or fix device.'),
			('The client gets the exception 0x0A (10)', 'The serial port of this TCP port is absent or lost: '
				'see DEVLOST, DEVABSENT.'),
			('%T2R-E-DEVOPNERR, ... errno: 13', 'Permission denied. Run with sudo, or add yourself '
				'to the dialout group.'),
			('%T2R-E-DEVOPNERR, ... errno: 16', 'The port is busy. Find the holder: '
				'sudo fuser /dev/ttyUSB0, and stop it.'),
			('%T2R-E-NOANSWER, ...', 'The request went out, nothing came back: 1) a wrong slave '
				'address in the client request; 2) the wiring (swap A/B of RS-485); 3) a wrong '
				'speed/parity --- re-check chars; 4) the device is off.'),
			('%T2R-E-CRC16ERR, ...', 'The octets arrive damaged: 1) chars does not match the device '
				'(the most common!); 2) the line noise --- the grounding, the termination, the cable '
				'length; 3) two masters on one line.'),
			('CRC16ERR on a part of the requests after a reboot or a replug', 'A gateway older than '
				'X.00-13ECO07 kept the CR-to-NL translation (icrnl) of a fresh port: an octet 0x0D became 0x0A. '
				'Update; for an old gateway: ExecStartPre=/bin/stty -F /dev/ttyUSB0 raw (one per port) in the unit.'),
			('%T2R-E-BADFRAME, ...', 'Something arrived, but it is not a sane MODBUS frame: usually '
				'the same causes as CRC16ERR; 400+ octets of garbage mean a foreign, non-MODBUS '
				'device on the line.'),
			('CRC16ERR / BADFRAME now and then, on an FTDI USB adapter only', 'The adapter gives the '
				'answer by pieces, once in 16 msec (latency_timer). Since X.00-10 the gateway waits '
				'for the whole answer of the functions 01-06, 15, 16, 23; the others are still cut. '
				'Set latency_timer to 1 msec (the answers get faster too): copy /usr/share/doc/mbusgw-t2r/99-ftdi-latency.rules '
				'to /etc/udev/rules.d/, run sudo udevadm control --reload &amp;&amp; sudo udevadm '
				'trigger --subsystem-match=usb-serial; cat /sys/bus/usb-serial/devices/ttyUSB0/'
				'latency_timer must print 1.'),
			('%T2R-W-FRAMETMO, ...', 'The answer started but never finished: a very slow or hanging '
				'device. Raise iotmo; check the cable.'),
			('%T2R-W-LATEANS, ... after NOANSWER', 'The device answered, but later than iotmo; the answer '
				'has been thrown away. Raise iotmo with a margin over the slowest device on the line.'),
			('%T2R-W-ECHOMISM, ...', 'The answer to a write (functions 5, 6, 15, 16) is not the echo of this '
				'request: usually a late answer to an earlier one (raise iotmo); with a fast device --- two '
				'masters on one line; on every write to one device --- the device does not echo as the '
				'specification wants: the write is done but reported as failed, tell us the model.'),
			('%T2R-W-EXCRPT, ... (SERVER_DEVICE_FAILURE)', 'The gateway told the client "the serial '
				'side failed". The real reason is in the previous log line (NOANSWER, CRC16ERR ...).'),
			('%T2R-W-CONNLIMIT, ...', 'The port already has connlm clients. If they are all real, raise '
				'connlm of this listener; otherwise find who keeps the connections: ss -tn.'),
			('%T2R-W-IDLETMO, ...', 'A client has sent nothing for idletmo seconds. Normal for a client which '
				'polls rarely --- it reconnects; to keep the connection, raise idletmo.'),
			('%T2R-W-RAWBUSY, ...', 'A client is already connected to the RAW port and takeover = 0. Normal: '
				'disconnect the first client, or set takeover = 1.'),
			('%T2R-E-TXDRAINTMO, ...', 'The USB adapter took the request but did not send it out; the '
				'request gets exception 4. The adapter or its USB link hangs: dmesg | tail; replug the '
				'adapter or reboot the board. Since X.00-13ECO01 the line is not blocked by it.'),
			('TXDRAINTMO on a port with flow = "RTS/CTS"', 'The device (or the cable) holds CTS off, so the gateway '
				'may not send. Check the RTS and CTS wires and that the device uses RTS/CTS; if not, set flow = "NONE". On a RAW listener the data just does not go out; if the client goes, RAWSTALL may drop its last data. With a USB adapter a short MODBUS request goes into '
				'the buffer of the adapter: NOANSWER, not TXDRAINTMO.'),
			('DEVLOCKERR / DEVBUSY on every request of one port', 'Another request holds the line and does '
				'not end. Update to X.00-13ECO01 or newer (an older gateway can hang forever in the serial '
				'driver). Before the restart save what the threads do: for t in /proc/$(pidof mbusgw-t2r)/task/*; '
				'do echo $t $(cat $t/wchan); done'),
			('%T2R-E-SESSFAIL, ... pthread_create()->11', 'Too many TCP clients at once: every client takes a thread, '
				'and systemd limits the service (TasksMax, 578 on a small board). Find who keeps so many '
				'connections (ss -tn); if they are all real, set TasksMax=2048 in the [Service] section '
				'of the unit and run systemctl daemon-reload.'),
			('%T2R-E-LSNRERR, ... errno: 98', 'The TCP port is taken: a second gateway instance, or '
				'another program. Find it: sudo ss -tlnp | grep 502.'),
			('%T2R-E-LSNRERR, ... errno: 13', 'The ports below 1024 need root. Run with sudo, or '
				'take a port above 1024 (e.g. 5502).'),
			('%T2R-E-CFGREJECT, Settings &lt;...&gt; are refused: N problem(s)', 'The settings file has errors (the warnings are not counted). Nothing is applied: the gateway does not start (/check fails; restart gateway is refused and the gateway goes on as it is). Read the %CFG-E-... lines above it: each names the setting, the file, the line and the column. Fix all of them and check with mbusgw-t2r /settings=&lt;file&gt; /check (see 7.3). After an upgrade to V.01-03 a file which worked before may be refused: for example a listener whose target is not in serials, or a setting the gateway does not know.'),
			('%T2R-W-CFGDEFAULTS, Settings &lt;...&gt;: N value(s) not usable ...', 'A value of a setting which has a default is bad (see also CHARSDEF). The gateway has started, but with the default, and the default may not fit your line (for example, the speed: the devices do not answer). Read the %CFG-W-... lines above, correct the file and check it with mbusgw-t2r /settings=&lt;file&gt; /check (see Bad values and defaults in 3.1).'),
			('The data is wrong or shifted', 'The byte order confusion on the client side: the MODBUS '
				'registers are the big-endian 16-bit words; check how the client assembles the '
				'32/64-bit values.'),
		 ], [52*mm, 111*mm], a_mono_cols=(0,)),
		 H2('7.2.  The Message Codes Reference'),
		 tbl(['Code', 'Sev', 'When it appears'], [
			('REVISNF', 'I', 'At the start: the program version. Quote it when asking for help.'),
			('DEVREADY', 'S', 'The serial port is open; the actual baud, the answer timeout and '
				'the t3.5 interval are shown.'),
			('LSNRRDY', 'S', 'The TCP port is listening; the address, the port and the target '
				'device are shown.'),
			('NETCONN', 'S', 'A TCP client has connected; its address:port and the listener '
				'are shown.'),
			('NETDISCN', 'S', 'A TCP client has disconnected; its address:port are shown.'),
			('DEVOPNERR', 'E', 'The serial port cannot be opened; errno says why (2 = no such '
				'file, 13 = the permissions, 16 = busy).'),
			('LSNRERR', 'E', 'bind()/listen() failed for a TCP port; errno says why (98 = taken, '
				'13 = the privileges).'),
			('NOANSWER', 'E', 'The device did not answer within the iotmo timeout.'),
			('FRAMETMO', 'W', 'The answer began but was not completed within the timeout.'),
			('BADFRAME', 'E', 'The received frame length is outside the sane [5..256] octets range.'),
			('CRC16ERR', 'E', 'The checksum of the received frame does not match; both values '
				'are shown.'),
			('EXCRPT', 'W', 'A MODBUS exception is returned to the TCP client; the code and its '
				'name are shown.'),
			('EXITST', 'I', 'The gateway exits; the exit flag and the final status are shown.'),
			('SYSERR', 'E', 'A system call failed; its name, the return value and errno are shown. Quote the line when asking for help.'),
			('NETSYSERR', 'E', 'A system call on a TCP connection failed (recv, send, poll, accept); the connection is closed.'),
			('NETSYSWRN', 'W', 'A socket option cannot be set; the gateway keeps working.'),
			('TTYSYSERR', 'E', 'A system call on the serial port failed (poll, read); the request gets exception 4.'),
			('TTYSYSWRN', 'W', 'A minor serial port call failed (tcflush, poll/read while flushing); the gateway keeps working.'),
			('DEVSYSERR', 'E', 'A serial port setup call failed (tcgetattr, tcsetattr, cfsetspeed, mutex); the port is not used.'),
			('DEVSYSWRN', 'W', 'Waiting for the serial line took too long (pthread_mutex_timedlock); the gateway tries again.'),
			('PEERCLOSE', 'I', 'The TCP client has closed the connection — the normal end of a session. Since V.01-01ECO01 also: a client has closed while its request waited for a busy line (e.g. behind a NOANSWER) — the request is dropped, the line is not used for it, the connlm place of the client is free within 0.5 s; a half-closed client (shutdown(SHUT_WR), nc -N) gets its answer on a free line, but not after such a wait.'),
			('ALLSENT', 'W', 'Nothing left to send on a TCP connection — harmless.'),
			('BADPROTO', 'E', 'A TCP request has a non-zero MBAP protocol id: the client does not speak MODBUS TCP; the connection is closed.'),
			('PDUSHORT', 'E', 'A TCP request is too short to be a MODBUS request; the connection is closed.'),
			('PDULONG', 'E', 'A TCP request is longer than 256 octets; the connection is closed.'),
			('IDLETMO', 'W', 'A TCP client has been silent for idletmo seconds (60 by default); the connection is closed.'),
			('NETRDTMO', 'E', 'A TCP client started a request but did not finish it in time; the connection is closed.'),
			('REQFAIL', 'W', 'A request is answered with a MODBUS exception; the previous lines say why.'),
			('NETPOLLEV', 'W', 'A TCP connection broke: the client reset it (e.g. the polling program was restarted); the connection is closed.'),
			('SESSFAIL', 'E', 'No thread for a new TCP client: too many connections at once (see Troubleshooting); the client is dropped.'),
			('LSNRABRT', 'W', 'A TCP listener is stopped — normal at the gateway exit.'),
			('TGTOPNERR', 'E', 'A TCP port is not opened because its serial port cannot be set up; the previous line says why. An absent device does not stop the TCP port since X.00-13ECO09 (DEVABSENT).'),
			('NOLSNRUP', 'E', 'Not a single TCP port could be opened; the gateway stops.'),
			('NOROOM', 'E', 'An answer does not fit into the buffer — an internal error, please report it.'),
			('MBAPLEN', 'E', 'The MBAP length field does not match the received data; the request is rejected.'),
			('REQSHORT', 'E', 'A request is too short to be forwarded to the serial line.'),
			('SERNORS485', 'W', 'rs485 = 1 is set, but this build has no RS485 support; the option is ignored.'),
			('DEVADDED', 'I', 'A serials record is accepted; the line parameters are shown.'),
			('TSENABLED', 'I', 'The Time Stamp pseudo device is on for this port.'),
			('TSDISABLED', 'I', 'The Time Stamp pseudo device is off for this port (the default).'),
			('LSNADDED', 'I', 'A listeners record is accepted; the address, the port, the target, the limits and the mode are shown (the text ends with ", Mode: MODBUS", ", Mode: RAW" or ", Mode: RAW (takeover)").'),
			('CFGREJECT', 'E', 'The settings file is refused because of errors: they are in the CFG-E messages above it (the warnings are not counted), nothing is applied and the gateway does not start (see 7.3).'),
			('CFGDEFAULTS', 'W', 'N values of the settings file are not usable: see the CFG-W messages above it. The gateway starts, the defaults are taken. Read them, correct the file and run /check again (see Bad values and defaults in 3.1).'),
			('CHARSDEF', 'W', 'A part of chars (speed, data bits, parity, stop bits) is not usable; the default is taken for it (speed 19200, data bits 8, parity E, stop bits 1). Correct chars.'),
			('SIGERR', 'E', 'A signal handler cannot be set — an internal error, please report it.'),
			('CFGERR', 'E', 'The configuration cannot be processed; the previous lines say why.'),
			('CFGABORT', 'F', 'The gateway stops because of the configuration errors above.'),
			('DEVLOCKERR', 'E', 'The serial line stayed busy for 39 seconds (another request holds it); the request gets exception 4.'),
			('TXNODATA', 'W', 'An empty request was not sent — harmless.'),
			('XMITERR', 'E', 'The request cannot be written to the serial port; the request gets exception 4.'),
			('XMITSHORT', 'E', 'Only a part of the request has been written to the serial port.'),
			('RXBUFFULL', 'W', 'The device sent more than 256 octets; the rest is dropped.'),
			('RXNODATA', 'E', 'The answer started but no octet was collected.'),
			('RESPMISM', 'W', 'The answer came from another slave or for another function: two masters or two devices with one address.'),
			('BADSPEED', 'W', 'The speed is not a standard one; 9600 is used. Since V.01-04 the check of the settings does not let such a speed in: see CHARSDEF.'),
			('NOTTTY', 'W', 'The device is not a serial port.'),
			('RS485OFF', 'W', 'The RS485 mode cannot be switched on for this port; the port works without it.'),
			('RS485ON', 'S', 'The RS485 mode is on for this port.'),
			('RAWSTALL', 'W', 'A RAW client has gone, and the serial port has sent nothing of its last data for iotmo + 10 seconds + the time to send 2 KB at its speed (CTS is off, or the device has sent XOFF); the data is dropped, the port is free for the next client.'),
			('FLOWNOTSET', 'W', 'The serial driver did not take the flow setting (some USB adapters have no RTS/CTS); the port works without it.'),
			('DEVINIERR', 'E', 'The serial port cannot be set up; the previous line says why.'),
			('DEVINIT', 'I', 'The serial port is set up.'),
			('DEVCLSERR', 'E', 'The serial port cannot be closed.'),
			('DEVNOTINI', 'W', 'Closing a serial port which was never opened — harmless.'),
			('DEVBUSY', 'E', 'The serial line cannot be taken for a request; the request gets exception 4.'),
			('CONNLIMIT', 'W', 'A new TCP client is disconnected at once: the port already has connlm clients.'),
			('RAWSTART', 'I', 'A RAW session has started: the client address and the serial port are shown. Nothing to do.'),
			('RAWSTOP', 'I', 'A RAW session has ended; shows how many octets went to the line and how many came from the line. Nothing to do --- the numbers help to see if the data flowed at all.'),
			('RAWKICK', 'W', 'A new client took the RAW port over (takeover = 1): the old connection is shut down. If this happens again and again by itself, two programs fight for one port --- stop one of them or set takeover = 0.'),
			('RAWBUSY', 'W', 'A new client is refused: the RAW port is busy and takeover = 0. Wait until the first client leaves, or set takeover = 1.'),
			('TXDRAINTMO', 'E', 'The request was not sent out by the serial adapter in time; it is dropped, the request gets exception 4.'),
			('ECHOMISM', 'W', 'The answer to a write request is not its echo (another address or value); the request gets exception 4.'),
			('LATEANS', 'W', 'After a failed request something came from the line within lategd --- a late answer; it is dropped.'),
			('LATEPRETX', 'W', 'Something came from the line just before a request was sent --- a late answer to an earlier request; it is dropped.'),
			('DEVLOST', 'E', 'The serial port has gone (an USB adapter unplugged); it is closed and opened again every second, the requests get exception 10 meanwhile.'),
			('DEVABSENT', 'W', 'The serial port cannot be opened at the start; the TCP port works anyway, the serial port is opened every second.'),
			('CTLREADY', 'S', 'The control channel for mbus-ctl is open; the socket file is shown.'),
			('CTLOFF', 'I', 'The control channel is switched off by /control=NONE; mbus-ctl cannot reach this gateway.'),
			('CTLBUSY', 'W', 'Another running gateway already uses this socket file; this one works without the control channel. Give each gateway its own /control=&lt;file&gt;.'),
			('CTLERR', 'W', 'The socket file cannot be made (2 = no such directory, 13 = no rights, 88 = not a socket); the gateway works without the control channel.'),
			('CTLSYSWRN', 'W', 'A system call of the control channel failed; the gateway keeps working.'),
			('CTLRESET', 'I', 'The operator has disconnected this client (mbus-ctl reset connection). Nothing to do.'),
			('CTLRESTART', 'I', 'The operator has asked to restart, the settings file is good; the gateway restarts.'),
			('CTLCHKFAIL', 'E', 'restart gateway is refused: the settings file has errors (mbus-ctl shows them). The gateway works on. Fix the file and try again.'),
			('CHECKOK', 'S', '/check: the settings file is good (maybe with warnings).'),
			('RESTARTING', 'I', 'The gateway starts itself again with the same command line; the usual start lines follow.'),
			('EXECERR', 'F', 'The restart has failed; the gateway stops. systemd and procd start it again; by hand --- start it again.'),
			('CTLNOGW', 'E', 'mbus-ctl cannot reach the gateway. errno 2: not running or another socket file (/socket=); 13: no rights --- use sudo; 111: not running, its old file is left.'),
			('CTLANSERR', 'E', 'The gateway has refused the request; its reason is shown.'),
			('CTLBADANS', 'E', 'mbus-ctl does not understand the answer --- a gateway of another version? Quote the line.'),
			('CTLWAIT', 'I', 'mbus-ctl waits for the restarted gateway.'),
			('CTLBACK', 'S', 'The gateway is back after the restart.'),
			('CTLNOBACK', 'W', 'The gateway has not answered after the restart in time; read its log.'),
			('CTLRESETN', 'S', 'mbus-ctl reset connection has disconnected this many clients.'),
			('CTLBADSEL', 'E', 'mbus-ctl reset connection needs a connection: an Id from sho conn, a.b.c.d, a.b.c.d:port or /all.'),
			('CTLCUT', 'W', 'The answer did not fit into 64 KB (many hundreds of clients): the table is not full.'),
		 ], [26*mm, 10*mm, 127*mm], a_mono_cols=(0,)),
		 H2('7.3.  The CFG Messages (the Check of the Settings File)'),
		 P('The settings file is read and checked by the library yaLIBCFG; its messages start with %CFG-E- (an error) or '
		   '%CFG-W- (a warning) and end with the file, the line and the column of the problem. After the errors the '
		   'gateway prints CFGREJECT, after the warnings CFGDEFAULTS. The ones you will meet:'),
		 tbl(['Code', 'What it says', 'What to do'], [
			('SYNTAX', 'The file is not written in the right syntax.', 'Look at the line and the column shown (the real mistake is often just before it): a missing ; or = or a bracket or a quote.'),
			('OPENIN', 'The file cannot be opened; errno says why (2 = no such file, 13 = no rights).', 'Check the name in /settings= and the rights on the file.'),
			('UNKNOWN', 'A setting which the gateway does not know.', 'A typo? Compare the name with 3.1 and 3.2.'),
			('REQUIRED', 'A required setting is missing.', 'Add it (see the Required column of the tables in Chapter 3).'),
			('VTYPE', 'The value has a wrong type.', 'Write what the message expects: a text in quotes, a number without them, a keyword in quotes from the list.'),
			('RANGE', 'A number is out of its range; the allowed range is in the message.', 'Fix the value.'),
			('LENGTH', 'A text is too short or too long; the allowed length is in the message.', 'Fix the value.'),
			('COUNT', 'Too few or too many records in a section (1..128 are allowed).', 'Add the missing records, or cut the section.'),
			('BADKWD', 'The value is not one of the allowed words; they are listed in the message.', 'Take one of them.'),
			('DUPKEY', 'Two records have the same key (for example the same device twice); the line of the first one is shown.', 'Remove one of them.'),
			('DUPNAME', 'A setting is written twice in one group (capital and small letters are not told apart), or a section twice.', 'Remove one of them.'),
			('CHECK', 'A rule of the gateway is broken; the message says which (the form of chars or bind, UDP, a target which is not in serials, a serial port of a RAW listener used by another listener, XON/XOFF under MODBUS, RTS/CTS with rs485 = 1, ...).', 'Read the text of the message and fix the setting it names.'),
		 ], [22*mm, 74*mm, 67*mm], a_mono_cols=(0,)),
		 P('RANGE, BADKWD, VTYPE and CHECK are printed as %CFG-W-... with - the default is taken at the end, when the setting has a default: the meaning is the same, but it is only a warning (see Bad values and defaults in 3.1).'),
		 P('The full list of the CFG messages is in the yaLIBCFG manual (Appendix A).'),
		 H2('7.4.  If Nothing Helps'),
		 P('Collect and attach to your question: 1) the full start-up log with /trace (from '
		   'REVISNF to the first error); 2) your settings file; 3) the output of '
		   '<font face="Mono">ls -l &lt;device&gt;</font> and <font face="Mono">dmesg | '
		   'tail -20</font>; 4) the exact model of the RTU device and its documented line '
		   'parameters. With these four things the problem is almost always visible at a glance.')],
		]
	L['chapters'] = chapters
	return L

# ----------------------------------------------------------------------------------------------
#	The RUSSIAN language pack
# ----------------------------------------------------------------------------------------------
def s_pack_ru ():
	L = {}
	L['doctitle'] = 'Руководство пользователя'
	L['runhead']  = 'mbusgw-t2r — Руководство пользователя'
	L['titleblock'] = [
		('Номер документа:', 'DO-T2RUG-RU-01A'),
		('Дата публикации:', PUBDATE_RU),
		('Сведения о ревизии:', 'Это новое руководство.'),
		('Операционная система:', 'Linux (glibc), ядро 4.x и новее'),
		('Версия программы:', 'mbusgw-t2r ' + VERSION),
	]
	L['copyright'] = 'Copyright © 2026 %s' % ORG
	L['legal_h'] = 'Правовая информация'
	L['legal'] = [
		'Сведения в настоящем документе могут быть изменены без предварительного уведомления. '
		'%s не несёт ответственности за технические или редакционные ошибки и пропуски в настоящем документе.' % ORG,
		'Программа, описанная в настоящем руководстве, распространяется по лицензии Apache License, '
		'Version 2.0; см. файлы LICENSE и NOTICE в составе дистрибутива.',
		'Программа, описанная в настоящем руководстве, распространяется в надежде, что она будет полезной, '
		'но БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ, в том числе без подразумеваемых гарантий товарной пригодности '
		'и пригодности для конкретной цели.',
		'MODBUS — зарегистрированный товарный знак Schneider Electric USA, Inc. '
		'Linux — зарегистрированный товарный знак Линуса Торвальдса. '
		'UNIX — зарегистрированный товарный знак The Open Group.',
	]
	L['toc_h'] = 'Содержание'
	L['preface_h'] = 'Предисловие'
	L['preface'] = [
		('О настоящем руководстве', [
			'Настоящее руководство описывает настройку, эксплуатацию и поиск неисправностей '
			'<b>mbusgw-t2r</b> — шлюза, который переводит запросы MODBUS TCP, приходящие из сети, '
			'в транзакции MODBUS RTU на последовательной линии (RS-485 или RS-232) и возвращает '
			'ответы обратно.']),
		('Для кого написано', [
			'Руководство рассчитано на читателя без опыта работы с MODBUS и администрирования '
			'Linux: каждый шаг расписан, ничего не подразумевается. Опытный инженер может сразу '
			'перейти к главе 3 (файл настроек) и главе 7 (поиск неисправностей).']),
		('Структура документа', [
			'Глава 1 объясняет, что делает шлюз. Глава 2 — контрольный список перед запуском. '
			'Глава 3 описывает файл настроек ключ за ключом. Глава 4 — запуск, опции командной '
			'строки и первая сквозная проверка. Глава 5 — встроенное псевдоустройство меток '
			'времени. Глава 6 учит читать журнал. Глава 7 — справочник по поиску неисправностей: '
			'симптом, причина, действие, и справочник всех кодов сообщений.']),
		('Смежные документы', [
			'<i>README_RU.md</i> — последовательность установки (пакет StarLet, сборка, '
			'<font face="Mono">make install</font>) и автоматическая сборка в GitLab CI '
			'(<font face="Mono">.gitlab-ci.yml</font>). '
			'<i>MODBUS over Serial Line Specification and Implementation Guide</i> и '
			'<i>MODBUS Application Protocol Specification</i> — сам протокол, доступны на modbus.org.']),
	]
	L['conv_h'] = 'Соглашения'
	L['conv_c1'] = 'Обозначение'; L['conv_c2'] = 'Значение'
	L['conventions'] = [
		('Моноширинный', 'Примеры кода, имена файлов, команды и экранный вывод.'),
		('%T2R-E-КОД',   'Код сообщения, как он выглядит в журнале шлюза; см. раздел 7.2.'),
		('<...>',        'Место подстановки фактического значения, например имени устройства.'),
		('$',            'Приглашение оболочки непривилегированного пользователя; вводить его не нужно.'),
		('Ctrl/C',       'Удерживая клавишу Ctrl, нажмите клавишу C.'),
	]
	L.update({
		'dia_client': 'TCP-клиент', 'dia_tcpside': 'SCADA, mbpoll, скрипты ...',
		'dia_queue': '(очередь запросов)', 'dia_slaves': 'RTU-устройства (ведомые)',
		'dia_recv': 'Приём и проверка', 'dia_tx': 'Передача в линию',
		'dia_rx': 'Приём ответа', 'dia_tmo': 'таймаут', 'dia_check': 'Проверка ответа',
		'dia_send': 'Ответ клиенту', 'dia_exc': 'Формирование MODBUS-исключения (в журнал идёт EXCRPT)',
		'dia_okpath': 'Нормальный путь', 'dia_errpath': 'Путь ошибки: любой сбой внизу превращается в ответ-исключение',
		'dia_secs': 'Секунды (64 бита, старшее слово первым)', 'dia_nsecs': 'Наносекунды (64 бита)',
		'dia_tz': 'Пояс, мин', 'dia_assemble': 'секунды = (R0<<48) | (R1<<32) | (R2<<16) | R3',
		'dia_fac': 'подсистема', 'dia_sev': 'серьёзность: S I W E F', 'dia_code': 'код сообщения (раздел 7.2)',
		'dia_msgline': 'Каждое важное событие — одна строка с кодом:',
	})
	L['fig_w'] = 'Рисунок %d.  '

	def chapters (fig):
		return [
		[H1('Глава 1.  Что делает шлюз'),
		 P('У вас есть устройство (счётчик электроэнергии, терморегулятор, ПЛК ...), которое '
		   'говорит на <b>MODBUS RTU</b> по последовательной линии. И есть программа (SCADA, '
		   'скрипт, <font face="Mono">mbpoll</font> ...), которая говорит на <b>MODBUS TCP</b> '
		   'по сети. Шлюз стоит между ними и переводит, как показано на рисунке 1.'),
		 fig(dia_architecture(L), 'Путь данных через шлюз'),
		 P('Несколько TCP-клиентов могут подключаться одновременно: шлюз выстраивает их в '
		   'очередь, чтобы на линию уходил только один запрос за раз — линия RTU физически не '
		   'умеет нести две транзакции сразу.'),
		 P('На одной линии RS-485 может сидеть несколько устройств. Они различаются <i>адресом '
		   'ведомого</i> внутри самого MODBUS-запроса; шлюзу их адреса знать не нужно вовсе.')],
		[H1('Глава 2.  Перед началом'),
		 P('Пройдите этот список один раз — он сэкономит час гаданий потом.'),
		 P('<b>1.</b> Шлюз установлен (см. <i>README_RU.md</i>, раздел «Установка»). Пакет устанавливает программу '
		   '<font face="Mono">/usr/sbin/mbusgw-t2r</font> и файл настроек '
		   '<font face="Mono">/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>. При ручной установке CMake '
		   'используются <font face="Mono">/usr/local/sbin/</font> и <font face="Mono">/usr/local/etc/mbusgw-t2r/</font>.'),
		 P('<b>2.</b> Вы знаете, к какому последовательному порту подключено устройство. В Linux это '
		   'файл вида <font face="Mono">/dev/ttyS0</font> или <font face="Mono">/dev/ttyUSB0</font>. '
		   'Если не уверены — выдерните и вставьте USB-адаптер и выполните:'),
		 C('$ dmesg | tail'),
		 P('<b>3.</b> Вы знаете параметры линии устройства: скорость, биты данных, чётность, '
		   'стоп-биты. Они написаны в паспорте устройства; самый распространённый набор — '
		   '<font face="Mono">9600, 8, N, 1</font>.'),
		 P('<b>4.</b> Ваш пользователь имеет право открыть порт. Проверьте группу файла порта:'),
		 C('$ ls -l /dev/ttyUSB0\n'
		   'crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0'),
		 P('Если группа <font face="Mono">dialout</font> — добавьте себя в неё и перезайдите в '
		   'систему (либо просто запускайте шлюз через <font face="Mono">sudo</font>):'),
		 C('$ sudo usermod -a -G dialout $USER')],
		[H1('Глава 3.  Файл настроек'),
		 P('В файле два раздела: <font face="Mono">serials</font> (последовательные линии) и '
		   '<font face="Mono">listeners</font> (TCP-порты). Минимальный рабочий пример:'),
		 C('serials = (\n'
		   '\t{\tdevice = "/dev/ttyUSB0";\n'
		   '\t\tchars  = "9600, 8, N, 1";\n'
		   '\t\tiotmo  = 1000;\n'
		   '\t}\n'
		   ');\n\n'
		   'listeners = (\n'
		   '\t{\tbind   = "TCP:0.0.0.0:502";\n'
		   '\t\ttarget = "/dev/ttyUSB0";\n'
		   '\t\tconnlm = 8;\n'
		   '\t}\n'
		   ');'),
		 P('Читается так: <i>«открой порт /dev/ttyUSB0 на 9600-8-N-1 и жди ответа до 1000 мс; '
		   'слушай TCP-клиентов на всех интерфейсах, порт 502, и пересылай их на этот порт»</i>.'),
		 H2('3.1.  Раздел serials'),
		 P('По одной записи на каждый последовательный порт. Таблица 3-1 перечисляет все ключи.'),
		 tbl(['Ключ', 'Обяз.', 'Значение и допустимые величины'], [
			('device', 'да', 'Файл последовательного порта, например /dev/ttyUSB0.'),
			('chars', 'да', 'Параметры линии: скорость, биты данных, чётность, стоп-биты. '
				'Скорость - настоящая скорость линии драйвера (см. Плохие значения и умолчания), 50..4000000; биты данных 5..8; чётность N (нет), E (чётная), O (нечётная); '
				'стоп-биты 1..2.'),
			('iotmo', 'нет', 'Сколько ждать ответа устройства, миллисекунды. По умолчанию 1000. '
				'Берите с запасом относительно самого медленного устройства на линии: опоздавший ответ теряется. Диапазон 1..3600000.'),
			('lategd', 'нет', 'После неудачного запроса (нет ответа, битый ответ) ещё столько миллисекунд слушать '
				'линию и выбрасывать всё, что пришло (LATEANS): опоздавший ответ не должен уйти следующему запросу. '
				'Время тратится только после сбоя: запрос к неработающему устройству занимает iotmo + lategd. '
				'0..60000, по умолчанию iotmo / 2, но не меньше 100; 0 — только пауза в 3,5 символа, как до X.00-13ECO08. '
				'Ответ на чтение не привязан к запросу: устройство, ответившее позже iotmo + lategd, может отдать одному клиенту '
				'данные другого — берите lategd с запасом.'),
			('rs485', 'нет', '1 — просить ядро управлять направлением RS-485 (только для портов, '
				'которые это умеют). По умолчанию 0.'),
			('flow', 'нет', 'Управление потоком на линии. NONE — без него (по умолчанию). RTS/CTS — аппаратное: '
				'шлюз передаёт, только пока устройство держит CTS; нужны провода RTS и CTS; нельзя вместе с rs485 = 1. '
				'XON/XOFF — устройство останавливает и запускает передачу символами XOFF (0x13) и XON (0x11); только '
				'для RAW-слушателя: в кадрах MODBUS RTU эти октеты встречаются, поэтому MODBUS-слушатель на таком порту — ошибка. Регистр не важен; другое '
				'значение или число (и DSR/DTR: в Linux такого нет) — ошибка.'),
			('ts_enabled', 'нет', '1 — включить псевдоустройство меток времени (глава 5). По умолчанию 0.'),
			('ts_unit', 'нет', 'Адрес ведомого, на который отвечает псевдоустройство. Диапазон 0..255. По умолчанию 135.'),
			('ts_fncode', 'нет', 'Код функции, на который оно отвечает. Диапазон 1..127. По умолчанию 3.'),
			('ts_base_reg0', 'нет', 'Первый регистр метки времени. Диапазон 0..65535. По умолчанию 8135.'),
		 ], [30*mm, 14*mm, 119*mm], a_mono_cols=(0,)),
		 P('Весь файл настроек проверяется <b>до того, как что-либо применено</b>. Шлюз находит проблемы '
		   'сразу и печатает каждую с путём настройки, файлом, строкой и столбцом, например:'),
		 C('%CFG-E-CHECK, listeners.[0]: <target> "/dev/ttyQ9" is not a <device> of <serials>,\n    settings.conf line 2 column 3\n%CFG-E-CHECK, listeners.[1].bind: UDP is not implemented, use TCP,\n    settings.conf line 3 column 5\n%CFG-E-UNKNOWN, unknown setting: listeners.[1].conlm, settings.conf line 3 column 56\n%CFG-E-DUPKEY, duplicate key: serials.[device="/dev/null"], the same as line 6,\n    settings.conf line 7 column 3\n%T2R-E-CFGREJECT, Settings <settings.conf> are refused: 4 problem(s),\n    see the CFG messages above --- nothing is applied'),
		 P('Если есть хотя бы одна <b>ошибка</b>, <b>весь файл отклоняется</b>: ничего не применяется, и шлюз не '
		   'запускается (/check завершается неудачей; restart gateway в mbus-ctl отклоняется, а шлюз продолжает '
		   'работать как работал). Исправьте все показанные строки и повторите. Проверка, которая сравнивает '
		   'несколько настроек (target с serials, два RAW-слушателя на одной линии, XON/XOFF под MODBUS), '
		   'выполняется, когда сами эти настройки верны, поэтому такая проблема может показаться только после исправления первых.'),
		 P('<b>Это изменилось в V.01-03 и V.01-04.</b> До V.01-03 неправильная запись пропускалась с сообщением '
		   '%T2R-E-..., а шлюз работал дальше с остальным файлом. В V.01-03 любая проблема отклоняла весь файл. '
		   'Начиная с V.01-04 файл отклоняет только <b>ошибка</b>: плохое значение настройки, у которой есть '
		   'умолчание, --- лишь предупреждение (см. <i>Плохие значения и умолчания</i> ниже). Файл, который раньше '
		   'работал, после обновления всё же может быть отклонён (например, из-за настройки, которой шлюз не знает): '
		   'проверьте его командой mbusgw-t2r /settings=&lt;файл&gt; /check, прежде чем перезапускать шлюз.'),
		 H3('Плохие значения и умолчания'),
		 P('У многих настроек есть умолчание. Если значение такой настройки плохое (вне диапазона, неверное слово, '
		   'неверный тип), шлюз <b>не</b> отклоняет файл. Он печатает <b>предупреждение</b> и берёт умолчание. '
		   'Предупреждение --- сообщение %CFG-W-... с файлом, строкой и столбцом; оно заканчивается словами - the '
		   'default is taken. После них печатается %T2R-W-CFGDEFAULTS с числом таких значений. Шлюз запускается.'),
		 P('<b>Настройки с умолчанием</b> (плохое значение --- только предупреждение):'),
		 P('&bull; serials: chars (по частям, см. ниже), flow (по умолчанию NONE), iotmo (по умолчанию 1000 мсек), '
		   'lategd (по умолчанию iotmo / 2, но не меньше 100), rs485 (по умолчанию 0), ts_enabled, ts_unit (по '
		   'умолчанию 135), ts_fncode (по умолчанию 3), ts_base_reg0 (по умолчанию 8135), desc;'),
		 P('&bull; listeners: connlm (по умолчанию 32; RAW: 1), idletmo (по умолчанию 60 с; RAW: максимум), iotmo, '
		   'takeover (по умолчанию 1).'),
		 P('<b>Ошибки</b> (разумного умолчания нет, поэтому весь файл по-прежнему отклоняется): плохой device у '
		   'serials; плохой bind, target или mode у слушателя; настройка, которой шлюз не знает (опечатка); '
		   'пропущенная обязательная настройка (listeners, serials, chars, bind, target, device); повтор device; '
		   'перекрёстные проверки (RTS/CTS при rs485 = 1; линия XON/XOFF под MODBUS-слушателем; правила '
		   'RAW-слушателя; два устройства, отличающиеся только заглавными и строчными буквами); синтаксическая '
		   'ошибка.'),
		 P('<b>chars по частям.</b> chars должен быть в файле (отсутствие chars --- ошибка), но четыре его части '
		   'проверяются по одной: скорость, биты данных, чётность, стоп-биты. Что хорошо --- остаётся. Каждая плохая '
		   'часть получает умолчание: скорость 19200, биты данных 8, чётность E, стоп-биты 1 (умолчание спецификации '
		   'MODBUS over Serial Line). %T2R-W-CHARSDEF говорит, какие части были плохи и что взято.'),
		 P('Скорость должна быть настоящей скоростью линии драйвера последовательного порта: 50, 75, 110, ... 1200, '
		   '2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400 ... 4000000. До V.01-04 принималось любое число от '
		   '50 до 4000000, а нестандартное при открытии порта молча заменялось на 9600 (BADSPEED). Теперь, например, '
		   '19201 --- предупреждение, и берётся 19200.'),
		 P('Настоящий вывод проверки файла с плохими значениями:'),
		 C('%CFG-W-CHECK, serials.[device="/dev/null"].chars: speed 19201 is not a line rate of\n    the serial driver (1200, 9600, 19200, 115200 ...) - the default is taken,\n    /tmp/soft.conf line 2 column 37\n%CFG-W-BADKWD, not an allowed value: serials.[device="/dev/null"].flow = "HW",\n    allowed: NONE, XON/XOFF, RTS/CTS - the default is taken,\n    /tmp/soft.conf line 2 column 63\n%CFG-W-RANGE, out of range: serials.[device="/dev/null"].iotmo = 0,\n    allowed 1..3600000 - the default is taken, /tmp/soft.conf line 2 column 76\n%T2R-W-CFGDEFAULTS, Settings </tmp/soft.conf>: 3 value(s) not usable,\n    see the CFG-W messages above --- the defaults are taken\n%T2R-W-CHARSDEF, [serial #00:</dev/null>] <chars> "19201,8,X,1" is not usable: speed,\n    parity --- taken: 19200, 8, E, 1\n%T2R-I-DEVADDED, Added device #00 [</dev/null>, Chars: <19200, 8, E, 1>, Flow: NONE,\n    Answer tmo: 1000 msec, Inter RTU PDU: 2002 usec] --- added\n%T2R-S-CHECKOK, Settings </tmp/soft.conf> --- are good: 1 serial(s), 1 listener(s)'),
		 P('<b>Осторожно.</b> Умолчание может не подойти вашей линии: с умолчательной скоростью шлюз может вообще не '
		   'говорить с вашими устройствами. Поэтому ищите в журнале CFG-W и CFGDEFAULTS и запускайте mbusgw-t2r '
		   '/settings=&lt;файл&gt; /check после каждой правки файла. /check завершается с кодом 0, если есть только '
		   'предупреждения.'),
		 P('<b>Числа.</b> Начиная с V.01-03ECO01 число с ведущим нулём — <b>восьмеричное</b>, как его читает libconfig 1.6 и '
		   'новее: <font face="Mono">ts_base_reg0 = 0135;</font> — это 93, а не 135, а 08 или 09 — ошибка (CFG-E-BADNUM). '
		   'Пишите числа без ведущих нулей (135). После обновления файл, который раньше работал, продолжит работать, '
		   'но может молча поменять смысл: до V.01-03ECO01 0135 было 135 — поищите = 0 с цифрой после него. '
		   'Шестнадцатеричное — 0x87 или %X87; %D, %B и %O задают десятичное, '
		   'двоичное и восьмеричное. Кроме #, // и /* */, комментарий до конца строки начинает восклицательный знак !, как в DCL.'),
		 P('В пути настройки listeners.[0] — первая запись раздела (записи считаются с нуля, в порядке следования '
		   'в файле); serials.[device="/dev/null"] — запись с этим device. Что ещё ловит проверка:'),
		 P('&bull; Настройка, которой шлюз не знает, — ошибка: опечатка не пройдёт. Имена настроек не зависят от '
		   'регистра.'),
		 P('&bull; Две записи serials с одним device — ошибка (DUPKEY), как и две, чьи device отличаются только '
		   'заглавными и строчными буквами (CHECK): target не смог бы их различить.'),
		 P('&bull; Не более 128 записей serials и 128 записей listeners.'),
		 P('&bull; target слушателя должен быть device из serials. Поэтому в образце modbus-t2r_settings.conf два '
		   'слушателя для /dev/ttyS1 и /dev/ttyS0 закомментированы: сначала добавьте для них записи serials, '
		   'затем уберите знаки комментария.'),
		 P('Допустимые значения — в таблицах этой главы. Сообщения разобраны в разделе 7.3.'),
		 P('<b>USB-адаптеры: переподключать можно.</b> С X.00-13ECO09 шлюз замечает, что адаптер пропал '
		   '(DEVLOST) — даже когда никто не опрашивает устройство — и сразу закрывает порт. Раз в секунду он '
		   'пытается открыть порт снова по имени; пока адаптера нет, клиенты получают исключение 10 '
		   '(GATEWAY_PATH_UNAVAILABLE), а не 4. Когда адаптер вернулся, в журнале DEVREADY и работа '
		   'продолжается, перезапуск не нужен. Если адаптера нет при старте, TCP-порт всё равно открывается (DEVABSENT).'),
		 P('<b>Используйте постоянное имя.</b> Ядро раздаёт номера ttyUSB0, ttyUSB1, ... в порядке появления '
		   'адаптеров, и после переподключения тот же адаптер может получить другой номер. Пишите в device (и в '
		   'target листенера) постоянное имя: имя по USB-разъёму из ls -l /dev/serial/by-path/ (порты '
		   '4-портового FT4232H отличаются частью :1.0 ... :1.3) или своё имя через правило udev:'),
		 C('SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-xhci-hcd.0-usb-0:1.1:1.0", SYMLINK+="rs485-1"'),
		 P('в /etc/udev/rules.d/99-rs485.rules даёт /dev/rs485-1 (значения смотрите в udevadm info /dev/ttyUSB0).'),
		 H2('3.2.  Раздел listeners'),
		 tbl(['Ключ', 'Обяз.', 'Значение и допустимые величины'], [
			('bind', 'да', 'Где слушать: TCP:&lt;IP-адрес&gt;:&lt;порт&gt;. Порт 1..65535; '
				'0.0.0.0 — все интерфейсы. UDP не поддерживается.'),
			('target', 'да', 'На какой порт пересылать. Должен совпадать с device из serials '
				'(заглавные и строчные буквы не различаются); если такого нет, весь файл отклоняется.'),
			('connlm', 'нет', 'Сколько TCP-клиентов может быть подключено к этому порту одновременно; лишний '
				'сразу отключается (CONNLIMIT). По умолчанию 32, диапазон 1..512.'),
			('idletmo', 'нет', 'Клиент, который ничего не присылает столько секунд, отключается (IDLETMO). '
				'По умолчанию 60, диапазон 1..86400.'),
			('mode', 'нет', 'Что делает слушатель: "MODBUS" (по умолчанию) — шлюз MODBUS TCP в RTU, "RAW" — '
				'прозрачный поток октетов (см. 3.3). Заглавные или строчные буквы — не важно; любое другое '
				'слово — ошибка.'),
			('takeover', 'нет', 'Только для RAW. 1 (по умолчанию): новый клиент вытесняет текущего (RAWKICK). '
				'0: пока подключён другой клиент, новый отклоняется (RAWBUSY). В режиме MODBUS этот ключ — '
				'ошибка.'),
			('iotmo', 'нет', 'Зарезервирован, пока не используется.'),
		 ], [30*mm, 14*mm, 119*mm], a_mono_cols=(0,)),
		 P('Порт <b>502</b> — стандартный порт MODBUS TCP, но в Linux порты ниже 1024 требуют '
		   'прав root. Не хотите работать под root — возьмите, например, '
		   '<font face="Mono">5502</font> и укажите его в клиенте. Несколько MODBUS-слушателей могут '
		   'разделять один target: шлюз выстроит их запросы в очередь. В режиме RAW два умолчания другие: '
		   'connlm может быть только 1, а idletmo равен 86400 (не 60). RAW-слушателю последовательный порт '
		   'нужен целиком.'),
		 H2('3.3.  Режим RAW (прозрачный)'),
		 P('В режиме MODBUS шлюз понимает MODBUS. В режиме RAW — <b>нет</b>. Он просто переносит октеты от '
		   'TCP-клиента в последовательную линию и из линии клиенту. Ничего не меняет. Это «виртуальный '
		   'COM-порт по сети».'),
		 P('Типичный случай: на ПК с Windows есть программа для старого прибора. Программе нужен COM-порт. '
		   'Вы ставите на ПК программу «COM через TCP» (она создаёт виртуальный COM-порт и соединяет его со '
		   'шлюзом по TCP). Программа думает, что работает с настоящим COM-портом. Прибор может говорить на '
		   '<b>любом</b> последовательном протоколе, не только на MODBUS.'),
		 C('[Программа на ПК] --- виртуальный COM --- TCP --->\n'
		   '        [mbusgw-t2r, режим RAW] --- линия ---> [Прибор]'),
		 P('Пример настроек:'),
		 C('listeners = (\n'
		   '    { bind = "TCP:0.0.0.0:4001"; mode = "RAW"; target = "/dev/ttyUSB1"; }\n'
		   ');\n'
		   'serials = (\n'
		   '    { device = "/dev/ttyUSB1"; chars = "19200, 8, E, 1"; flow = "NONE"; iotmo = 1000; }\n'
		   ');'),
		 P('Правила режима RAW:'),
		 P('&bull; <b>Один клиент за раз.</b> connlm может быть только 1.'),
		 P('&bull; <b>Новый клиент вытесняет старого</b> (takeover = 1, по умолчанию). Старое соединение '
		   'закрывается, линию получает новый; в журнале будет RAWKICK. При takeover = 0 старый клиент '
		   'остаётся, а новому отказывают (RAWBUSY).'),
		 P('&bull; <b>Последовательный порт — только для этого слушателя.</b> Ни один другой слушатель, '
		   'MODBUS или RAW, не может брать тот же target (ошибка; порядок записей не важен). '
		   'MODBUS-слушатели по-прежнему могут делить порт между собой.'),
		 P('&bull; <b>Скорость и чётность берутся из шлюза</b>, из записи serials. Если программа на ПК ставит '
		   'у своего виртуального COM-порта другую скорость, в линию это <b>не</b> передаётся. Задайте верный '
		   'chars в записи serials.'),
		 P('&bull; <b>Долгое молчание — это нормально.</b> idletmo по умолчанию — 86400 секунд (сутки). '
		   'Мёртвого клиента (ПК упал, кабель оборван) находит TCP keepalive, и его отбрасывают примерно за '
		   '1 минуту (30 секунд тишины, потом 3 пробы с шагом 10 секунд).'),
		 P('&bull; <b>Меток времени нет.</b> Псевдоустройство ts_* (глава 5) в режиме RAW не работает. '
		   'В начале сессии старые октеты в буферах последовательного порта выбрасываются.'),
		 P('&bull; <b>Нет последовательного порта — нет сеанса.</b> Если USB-адаптера нет или его выдернули '
		   '(DEVABSENT, DEVLOST, 3.1), новый клиент сразу отключается, без данных, а идущий сеанс '
		   'закрывается. Когда адаптер вернётся, клиент просто подключается снова.'),
		 P('<b>Программа в Linux как клиент.</b> Драйвер псевдопоследовательного порта serial2net (NaPi Lab, '
		   'X.00-08ECO1 и новее) создаёт /dev/ser2net00, /dev/ser2net01 и так далее и переносит всё, что туда '
		   'записано, как есть на TCP-партнёра. Направьте партнёра на RAW-слушатель '
		   '(p_ser2net_peer="127.0.0.1:4001"), и любая программа, которой нужен последовательный порт '
		   '(mbpoll -m rtu, minicom, pyserial), работает с удалённой линией как с местным портом. Скорость по-'
		   'прежнему берётся из записи serials шлюза. Проверено: 1 МиБ в каждую сторону блоками с CRC32 '
		   '(без потерь), четыре порта одновременно с mbpoll, 115200.'),
		 P('Замечание для обоих режимов: начиная с X.00-14 вход последовательного порта «бинарно чистый». '
		   'Порт больше не превращает CR в LF и не обрезает 8-й бит. Раньше октет 0x0D мог прийти как 0x0A на '
		   'порту с настройками по умолчанию.')],
		[H1('Глава 4.  Запуск шлюза'),
		 P('Вручную, в терминале (сообщения идут прямо на экран):'),
		 C('$ /usr/sbin/mbusgw-t2r /settings=/etc/mbusgw-t2r/modbus-t2r_settings.conf'),
		 tbl(['Опция', 'Смысл'], [
			('/settings=&lt;файл&gt;', 'Файл настроек (глава 3).'),
			('/trace', 'Подробная трассировка: каждый запрос и ответ печатается в hex. '
				'Бесценно при первом запуске.'),
			('/logfile=&lt;файл&gt;', 'Писать журнал в файл вместо экрана.'),
			('/logsize=&lt;октет&gt;', 'Ротировать файл журнала при превышении размера.'),
			('/control=&lt;файл&gt;', 'Файл-сокет для mbus-ctl (раздел 4.3), NONE — без него.'),
			('/check', 'Только проверить файл настроек и выйти (раздел 4.3).'),
		 ], [42*mm, 121*mm], a_mono_cols=(0,)),
		 P('Здоровый запуск печатает (сокращённо):'),
		 C('%T2R-I-REVISNF, Rev: T2R X.00-06/aarch64(built at ...) (REV: 00.06.00)\n'
		   '%T2R-I-DEVADDED, Added device #00 [</dev/ttyUSB0>, Chars: <9600, 8, N, 1>, ...] --- added\n'
		   '%T2R-I-LSNADDED, Added listener #00 [Target: </dev/ttyUSB0>, Net: <TCP:0.0.0.0:502>, ...] --- added\n'
		   '%T2R-S-DEVREADY, Device </dev/ttyUSB0> [9600 baud, answer tmo: 1000 msec, t3.5: 4010 usec] --- is ready\n'
		   '%T2R-S-LSNRRDY, [#3] Listener 0.0.0.0:502 [Target: </dev/ttyUSB0>] --- is ready'),
		 P('Важнее всего две строки: <b>DEVREADY</b> (порт открыт) и <b>LSNRRDY</b> (TCP-порт '
		   'слушает). Видите обе — шлюз работает. Рисунок 2 показывает, что происходит дальше '
		   'с каждым запросом.'),
		 fig(dia_lifecycle(L), 'Жизненный цикл одного запроса'),
		 P('Остановка: один раз Ctrl/C и секунда ожидания. Переключить трассировку у <i>уже '
		   'работающего</i> шлюза без перезапуска:'),
		 C('$ kill -USR1 <pid>'),
		 H2('4.1.  Первая проверка с mbpoll'),
		 P('<font face="Mono">mbpoll</font> — маленький свободный MODBUS-клиент '
		   '(<font face="Mono">sudo apt install mbpoll</font>). Читаем 3 регистра хранения с '
		   'устройства с адресом 1:'),
		 C('$ mbpoll -a 1 -r 1 -c 3 -1 127.0.0.1 -p 502'),
		 P('Если вернулись числа — всё работает, из конца в конец.'),
		 H2('4.2.  На OpenWrt'),
		 P('На роутер с OpenWrt ставьте пакет для своей платы со страницы Releases (архитектура — '
		   'строка <font face="Mono">DISTRIB_ARCH</font> в <font face="Mono">/etc/openwrt_release</font>): '
		   '<font face="Mono">.ipk</font> для OpenWrt 24.10, <font face="Mono">.apk</font> для OpenWrt 25.12. '
		   'Пакету больше ничего не нужно, сеть роутеру не нужна.'),
		 C('# opkg install ./mbusgw-t2r_1.4.4-*_mipsel_24kc.ipk\n'
		   '# apk add --allow-untrusted ./mbusgw-t2r_1.4.4-*_mipsel_24kc.apk'),
		 P('Файлы лежат не там, где на Debian: программа — <font face="Mono">/usr/sbin/mbusgw-t2r</font>, '
		   'файл настроек — <font face="Mono">/etc/mbusgw-t2r/modbus-t2r_settings.conf</font>. После '
		   'установки служба выключена. Настройте файл настроек, затем включите службу и запустите её:'),
		 C('# uci set mbusgw-t2r.main.enabled=1 && uci commit mbusgw-t2r\n'
		   '# /etc/init.d/mbusgw-t2r start\n'
		   '# logread -e mbusgw-t2r'),
		 P('Последняя команда показывает сообщения шлюза (они идут в системный журнал). Адаптеру '
		   'USB-serial нужен модуль ядра: <font face="Mono">kmod-usb-serial-ftdi</font>, '
		   '<font face="Mono">kmod-usb-serial-ch341</font> или <font face="Mono">kmod-usb-serial-cp210x</font> — '
		   'по микросхеме адаптера.'),
		 H2('4.3.  Как смотреть за шлюзом и управлять им: mbus-ctl'),
		 P('<font face="Mono">mbus-ctl</font> — маленькая программа рядом со шлюзом '
		   '(<font face="Mono">/usr/sbin/mbus-ctl</font> в пакетах, '
		   '<font face="Mono">/usr/local/sbin/mbus-ctl</font> при ручной установке). '
		   'Она спрашивает работающий шлюз, что он делает, а ещё умеет отключить клиента или перезапустить шлюз. '
		   'Говорит она со шлюзом через <b>файл</b>-сокет на том же компьютере, '
		   '<font face="Mono">/var/run/mbusgw-t2r.sock</font>. В сеть ничего не открывается. Пользоваться файлом могут '
		   'только root и группа шлюза (права 0660), поэтому пишите перед командой <font face="Mono">sudo</font>.'),
		 tbl(['Команда', 'Что вы получите'], [
			('show connections', 'Кто подключён к какому TCP-порту, с какого времени, запросы, ответы, ошибки и октеты.'),
			('show connections /continuous', 'То же, с перерисовкой каждые 2 секунды до Ctrl/C (/continuous=5 — каждые 5 секунд).'),
			('show lines', 'Последовательные порты: устройство, скорость, таймауты, OPEN или ABSENT, сколько клиентов.'),
			('show gateway', 'Версия, PID, сколько работает, файл настроек, TCP-порты.'),
			('reset connection 7', 'Отключить клиента с Id 7 (из show connections).'),
			('reset connection 10.0.0.7', 'Отключить всех клиентов с этого адреса (10.0.0.7:44770 — только одного).'),
			('reset connection /all', 'Отключить всех клиентов.'),
			('restart gateway', 'Проверить файл настроек и перезапустить шлюз с ним.'),
		 ], [58*mm, 105*mm], a_mono_cols=(0,)),
		 P('Слова можно сокращать: <font face="Mono">sho conn /cont</font>, <font face="Mono">sho gw</font>, '
		   '<font face="Mono">reset conn 7</font>, <font face="Mono">restart gw</font>. SERIALS — то же, что LINES, '
		   'GW — то же, что GATEWAY. Слишком коротко только <font face="Mono">res</font>: это и RESET, и RESTART.'),
		 C('$ sudo mbus-ctl sho conn\n'
		   'Connections of the MODBUS gateway at 07-NOV-2026 00:07:44\n\n'
		   'Listener #0  0.0.0.0:502  MODBUS -> /dev/ttyUSB0  UP, 2 of 8 connection(s)\n'
		   '    Id  Sd  Client            Connected             Requests  Answers  Errors\n'
		   '     1   8  10.0.0.7:44770    07-NOV-2026 00:07:44         7        7       0\n'
		   '     2   9  10.0.0.9:44772    07-NOV-2026 00:07:44         3        3       0\n\n'
		   'Total: 2 connection(s)'),
		 P('Id — номер соединения для reset. Sd — число в [#8] в строках журнала этого клиента. '
		   'Errors — ответы с исключением MODBUS плюс запросы, на которых соединение оборвалось.'),
		 P('<b>restart gateway</b> сначала проверяет файл настроек в отдельном процессе '
		   '(<font face="Mono">mbusgw-t2r /settings=&lt;файл&gt; /check</font>). Если в файле ошибка (предупреждение не ошибка), ничего не '
		   'перезапускается: вы видите строку ошибки, шлюз работает дальше со старыми настройками. Если файл хороший, '
		   'шлюз отключает клиентов, закрывает порты и запускает сам себя заново с той же командной строкой. PID остаётся '
		   'прежним: systemd и procd ничего не замечают. mbus-ctl ждёт, пока шлюз снова ответит (CTLBACK), 3–15 секунд. '
		   'Под systemd то же делает <font face="Mono">systemctl reload mbusgw-t2r</font>. После обновления пакета '
		   'используйте <font face="Mono">systemctl restart</font>.'),
		 P('Шлюз и mbus-ctl говорят на маленьком двоичном протоколе (записи TLV, см. README_RU.md). Руками со '
		   'шлюзом не поговорить: пользуйтесь mbus-ctl.'),
		 P('Двум шлюзам на одном компьютере нужны два файла-сокета: <font face="Mono">/control=&lt;файл&gt;</font> для '
		   'шлюза, <font face="Mono">/socket=&lt;файл&gt;</font> для mbus-ctl.')],
		[H1('Глава 5.  Псевдоустройство меток времени'),
		 P('Шлюз умеет отвечать <i>сам</i> — не трогая линию — на запрос чтения, посланный на '
		   'специальный адрес ведомого. Ответ — текущее время машины, на которой работает шлюз. '
		   'Так SCADA может читать точное время по обычному MODBUS.'),
		 P('При <font face="Mono">ts_enabled = 1; ts_unit = 135; ts_fncode = 4; '
		   'ts_base_reg0 = 135;</font> запрос «прочитай N входных регистров начиная со 135-го '
		   'у ведомого 135» возвращает регистры рисунка 3.'),
		 fig(dia_ts_regs(L), 'Раскладка регистров метки времени'),
		 tbl(['Запрошено регистров', 'Что получаете'], [
			('4', 'Секунды эпохи UNIX, 64 бита, старшее 16-битное слово первым.'),
			('8', 'Плюс наносекунды, упакованные так же в R4..R7.'),
			('9', 'Плюс R8 — смещение местного часового пояса, в знаковых минутах (Москва = +180).'),
		 ], [40*mm, 123*mm], a_mono_cols=(0,)),
		 P('Любое другое число регистров в TS-запросе получает MODBUS-исключение '
		   'ILLEGAL_DATA_ADDRESS — это сделано намеренно.')],
		[H1('Глава 6.  Как читать журнал'),
		 fig(dia_msg_anatomy(L), 'Анатомия сообщения журнала'),
		 P('Буква серьёзности: <b>S</b> — успех, <b>I</b> — информация, <b>W</b> — '
		   'предупреждение, <b>E</b> — ошибка, <b>F</b> — фатально. Часть '
		   '<font face="Mono">[#4:</font><font face="Mono">&lt;/dev/ttyUSB0&gt;]</font> '
		   'означает: файловый дескриптор #4, вот этот последовательный порт.'),
		 P('Ищите в журнале по коду — именно для этого коды и существуют:'),
		 C('$ grep CRC16ERR /var/log/mbusgw-t2r.log')],
		[H1('Глава 7.  Поиск неисправностей'),
		 P('<b>Золотое правило: запустите с /trace и читайте журнал. Шлюз всегда говорит, что '
		   'именно ему не нравится.</b>'),
		 H2('7.1.  Симптом, причина, действие'),
		 tbl(['Вы видите', 'Что это значит / что делать'], [
			('%T2R-E-DEVOPNERR, ... errno: 2', 'Файла порта нет: адаптер выдернут или имя '
				'неверное. Выполните dmesg | tail после втыкания; поправьте device в настройках. С X.00-13ECO09 TCP-порт всё равно открывается (DEVABSENT), устройство пробуется раз в секунду.'),
			('%T2R-E-DEVLOST, ...', 'USB-адаптер пропал (выдернут, сброс хаба, просадка питания); клиенты '
				'получают исключение 10. Если вернётся — ничего делать не нужно, следом будет DEVREADY. Если нет: '
				'dmesg | tail, кабель, питание; если вернулся под другим номером ttyUSB — постоянное имя (3.1).'),
			('%T2R-W-DEVABSENT, ...', 'При старте последовательный порт не открылся (почему — в строке выше); '
				'его TCP-порт работает и отвечает исключением 10. Подключите адаптер или поправьте device.'),
			('Клиент получает исключение 0x0A (10)', 'Последовательного порта этого TCP-порта нет или он '
				'пропал: см. DEVLOST, DEVABSENT.'),
			('%T2R-E-DEVOPNERR, ... errno: 13', 'Нет прав доступа. Запустите под sudo или '
				'добавьте себя в группу dialout.'),
			('%T2R-E-DEVOPNERR, ... errno: 16', 'Порт занят. Найдите держателя: '
				'sudo fuser /dev/ttyUSB0, и остановите его.'),
			('%T2R-E-NOANSWER, ...', 'Запрос ушёл, ответа нет: 1) неверный адрес ведомого в '
				'запросе клиента; 2) провода (поменяйте A/B у RS-485); 3) не та скорость/чётность '
				'— перепроверьте chars; 4) устройство выключено.'),
			('%T2R-E-CRC16ERR, ...', 'Байты приходят повреждёнными: 1) chars не совпадает с '
				'устройством (самое частое!); 2) помехи — земля, терминаторы, длина кабеля; '
				'3) два мастера на одной линии.'),
			('CRC16ERR на части запросов после перезагрузки или переподключения', 'Шлюз старше '
				'X.00-13ECO07 оставлял у свежего порта замену CR на NL (icrnl): байт 0x0D становился 0x0A. '
				'Обновитесь; для старого шлюза: ExecStartPre=/bin/stty -F /dev/ttyUSB0 raw (на каждый порт) в юните.'),
			('%T2R-E-BADFRAME, ...', 'Что-то пришло, но это не осмысленный MODBUS-кадр: обычно '
				'те же причины, что у CRC16ERR; 400+ октетов мусора — на линии чужое, '
				'не-MODBUS устройство.'),
			('CRC16ERR / BADFRAME время от времени, только на USB-адаптере FTDI', 'Адаптер отдаёт '
				'ответ кусками, раз в 16 мс (latency_timer). С X.00-10 шлюз дожидается всего ответа '
				'для функций 01-06, 15, 16, 23; остальные по-прежнему обрезаются. Поставьте '
				'latency_timer в 1 мс (ответы заодно станут быстрее): скопируйте /usr/share/doc/mbusgw-t2r/99-ftdi-latency.rules '
				'в /etc/udev/rules.d/, выполните sudo udevadm control --reload &amp;&amp; sudo udevadm '
				'trigger --subsystem-match=usb-serial; cat /sys/bus/usb-serial/devices/ttyUSB0/'
				'latency_timer должна напечатать 1.'),
			('%T2R-W-FRAMETMO, ...', 'Ответ начался, но не завершился: очень медленное или '
				'зависшее устройство. Увеличьте iotmo; проверьте кабель.'),
			('%T2R-W-LATEANS, ... после NOANSWER', 'Устройство ответило, но позже iotmo; ответ выброшен. '
				'Увеличьте iotmo с запасом относительно самого медленного устройства на линии.'),
			('%T2R-W-ECHOMISM, ...', 'Ответ на запись (функции 5, 6, 15, 16) — не эхо этого запроса: обычно '
				'опоздавший ответ на предыдущий (увеличьте iotmo); если устройство быстрое — два мастера на '
				'линии; если на каждой записи в одно устройство — оно отвечает не эхом, как требует '
				'спецификация: запись выполнена, но считается неудачной, сообщите нам модель.'),
			('%T2R-W-EXCRPT, ... (SERVER_DEVICE_FAILURE)', 'Шлюз сообщил клиенту «на '
				'последовательной стороне сбой». Настоящая причина — в предыдущей строке журнала '
				'(NOANSWER, CRC16ERR ...).'),
			('%T2R-W-CONNLIMIT, ...', 'К порту уже подключено connlm клиентов. Если все настоящие — увеличьте '
				'connlm этого слушателя; иначе найдите, кто держит соединения: ss -tn.'),
			('%T2R-W-IDLETMO, ...', 'Клиент ничего не присылал idletmo секунд. Нормально для клиента, который '
				'опрашивает редко — он переподключится; чтобы держать соединение, увеличьте idletmo.'),
			('%T2R-W-RAWBUSY, ...', 'К RAW-порту уже подключён клиент, а takeover = 0. Нормально: отключите '
				'первого клиента или поставьте takeover = 1.'),
			('%T2R-E-TXDRAINTMO, ...', 'USB-адаптер принял запрос, но не передал его в линию; запрос получает '
				'исключение 4. Адаптер или его USB-связь зависли: dmesg | tail; переподключите адаптер или '
				'перезагрузите плату. С X.00-13ECO01 линия из-за этого не блокируется.'),
			('TXDRAINTMO на порту с flow = "RTS/CTS"', 'Устройство (или кабель) не даёт CTS, и шлюзу нельзя передавать. '
				'Проверьте провода RTS и CTS и что устройство работает с RTS/CTS; если нет — поставьте flow = "NONE". На RAW-слушателе данные просто не уходят; если клиент уйдёт, RAWSTALL может выбросить его последние данные. С USB-адаптером короткий запрос MODBUS '
				'уходит в буфер адаптера: NOANSWER, а не TXDRAINTMO.'),
			('DEVLOCKERR / DEVBUSY на каждом запросе одного порта', 'Линию держит другой запрос, который не '
				'заканчивается. Обновитесь до X.00-13ECO01 или новее (старый шлюз мог навсегда зависнуть в '
				'драйвере порта). Перед перезапуском сохраните, что делают потоки: for t in /proc/$(pidof '
				'mbusgw-t2r)/task/*; do echo $t $(cat $t/wchan); done'),
			('%T2R-E-SESSFAIL, ... pthread_create()->11', 'Слишком много TCP-клиентов сразу: каждому нужен поток, '
				'а systemd ограничивает сервис (TasksMax, на маленькой плате 578). Найдите, кто держит '
				'столько соединений (ss -tn); если все настоящие — поставьте TasksMax=2048 в разделе '
				'[Service] юнита и выполните systemctl daemon-reload.'),
			('%T2R-E-LSNRERR, ... errno: 98', 'TCP-порт занят: второй экземпляр шлюза или другая '
				'программа. Найдите: sudo ss -tlnp | grep 502.'),
			('%T2R-E-LSNRERR, ... errno: 13', 'Порты ниже 1024 требуют root. Запустите под sudo '
				'или возьмите порт выше 1024 (например 5502).'),
			('%T2R-E-CFGREJECT, Settings &lt;...&gt; are refused: N problem(s)', 'В файле настроек есть ошибки (предупреждения не считаются). Ничего не применено: шлюз не запускается (/check завершается неудачей; restart gateway отклоняется, а шлюз продолжает работать как работал). Читайте строки %CFG-E-... выше: каждая называет настройку, файл, строку и столбец. Исправьте всё и проверьте командой mbusgw-t2r /settings=&lt;файл&gt; /check (см. 7.3). После обновления до V.01-03 файл, который раньше работал, может быть отклонён: например, у слушателя target нет в serials, или в файле настройка, которой шлюз не знает.'),
			('%T2R-W-CFGDEFAULTS, Settings &lt;...&gt;: N value(s) not usable ...', 'Значение настройки, у которой есть умолчание, плохое (см. также CHARSDEF). Шлюз запустился, но с умолчанием, а оно может не подойти вашей линии (например, скорость: устройства не отвечают). Читайте строки %CFG-W-... выше, исправьте файл и проверьте командой mbusgw-t2r /settings=&lt;файл&gt; /check (см. Плохие значения и умолчания в 3.1).'),
			('Данные неверные или сдвинутые', 'Путаница порядка байт у клиента: регистры MODBUS — '
				'16-битные слова big-endian; проверьте, как клиент собирает 32/64-битные значения.'),
		 ], [52*mm, 111*mm], a_mono_cols=(0,)),
		 H2('7.2.  Справочник кодов сообщений'),
		 tbl(['Код', 'Сер.', 'Когда появляется'], [
			('REVISNF', 'I', 'При старте: версия программы. Указывайте её, когда просите помощи.'),
			('DEVREADY', 'S', 'Последовательный порт открыт; показаны фактическая скорость, '
				'таймаут ответа и интервал t3.5.'),
			('LSNRRDY', 'S', 'TCP-порт слушает; показаны адрес, порт и целевое устройство.'),
			('NETCONN', 'S', 'Подключился TCP-клиент; показаны его адрес:порт и listener.'),
			('NETDISCN', 'S', 'TCP-клиент отключился; показаны его адрес:порт.'),
			('DEVOPNERR', 'E', 'Последовательный порт не открывается; errno объясняет почему '
				'(2 — файла нет, 13 — права, 16 — занят).'),
			('LSNRERR', 'E', 'Отказ bind()/listen() для TCP-порта; errno объясняет почему '
				'(98 — занят, 13 — привилегии).'),
			('NOANSWER', 'E', 'Устройство не ответило за таймаут iotmo.'),
			('FRAMETMO', 'W', 'Ответ начался, но не был завершён за таймаут.'),
			('BADFRAME', 'E', 'Длина принятого кадра вне разумного диапазона [5..256] октетов.'),
			('CRC16ERR', 'E', 'Контрольная сумма принятого кадра не сходится; показаны оба значения.'),
			('EXCRPT', 'W', 'TCP-клиенту возвращается MODBUS-исключение; показаны код и его имя.'),
			('EXITST', 'I', 'Шлюз завершается; показаны флаг выхода и итоговый статус.'),
			('SYSERR', 'E', 'Сбой системного вызова; показаны его имя, код возврата и errno. Приложите строку к вопросу.'),
			('NETSYSERR', 'E', 'Сбой системного вызова на TCP-соединении (recv, send, poll, accept); соединение закрывается.'),
			('NETSYSWRN', 'W', 'Не удалось установить опцию сокета; шлюз продолжает работу.'),
			('TTYSYSERR', 'E', 'Сбой системного вызова на последовательном порту (poll, read); запрос получает исключение 4.'),
			('TTYSYSWRN', 'W', 'Сбой второстепенного вызова на порту (tcflush, poll/read при очистке); шлюз продолжает работу.'),
			('DEVSYSERR', 'E', 'Сбой вызова настройки порта (tcgetattr, tcsetattr, cfsetspeed, mutex); порт не используется.'),
			('DEVSYSWRN', 'W', 'Ожидание последовательной линии затянулось (pthread_mutex_timedlock); шлюз пробует снова.'),
			('PEERCLOSE', 'I', 'TCP-клиент закрыл соединение — обычное окончание сеанса. С V.01-01ECO01 ещё: клиент закрыл соединение, пока его запрос ждал занятую линию (например, за NOANSWER) — запрос отброшен, на линию не идёт, место клиента в connlm освобождается в пределах 0,5 с; клиент с полузакрытием (shutdown(SHUT_WR), nc -N) на свободной линии ответ получает, а после такого ожидания — нет.'),
			('ALLSENT', 'W', 'На TCP-соединении нечего отправлять — безвредно.'),
			('BADPROTO', 'E', 'У TCP-запроса ненулевой идентификатор протокола MBAP: клиент говорит не на MODBUS TCP; соединение закрывается.'),
			('PDUSHORT', 'E', 'TCP-запрос слишком короткий для MODBUS; соединение закрывается.'),
			('PDULONG', 'E', 'TCP-запрос длиннее 256 октетов; соединение закрывается.'),
			('IDLETMO', 'W', 'TCP-клиент молчал idletmo секунд (по умолчанию 60); соединение закрывается.'),
			('NETRDTMO', 'E', 'TCP-клиент начал запрос, но не закончил его вовремя; соединение закрывается.'),
			('REQFAIL', 'W', 'На запрос ответили исключением MODBUS; причина — в строках выше.'),
			('NETPOLLEV', 'W', 'TCP-соединение оборвалось: клиент сбросил его (например, перезапустили программу опроса); соединение закрывается.'),
			('SESSFAIL', 'E', 'Нет потока для нового TCP-клиента: слишком много соединений сразу (см. Траблшутинг); клиент сбрасывается.'),
			('LSNRABRT', 'W', 'TCP-порт закрыт — при остановке шлюза это нормально.'),
			('TGTOPNERR', 'E', 'TCP-порт не открыт, потому что не удалось подготовить его последовательный порт; причина — в строке выше. С X.00-13ECO09 отсутствие устройства TCP-порт не останавливает (DEVABSENT).'),
			('NOLSNRUP', 'E', 'Не открылся ни один TCP-порт; шлюз останавливается.'),
			('NOROOM', 'E', 'Ответ не помещается в буфер — внутренняя ошибка, сообщите о ней.'),
			('MBAPLEN', 'E', 'Поле длины MBAP не совпадает с принятыми данными; запрос отклоняется.'),
			('REQSHORT', 'E', 'Запрос слишком короткий, чтобы передать его в линию.'),
			('SERNORS485', 'W', 'Задано rs485 = 1, но эта сборка без поддержки RS485; опция игнорируется.'),
			('DEVADDED', 'I', 'Запись serials принята; показаны параметры линии.'),
			('TSENABLED', 'I', 'Псевдоустройство меток времени включено для этого порта.'),
			('TSDISABLED', 'I', 'Псевдоустройство меток времени выключено для этого порта (по умолчанию).'),
			('LSNADDED', 'I', 'Запись listeners принята; показаны адрес, порт, устройство, ограничения и режим (текст заканчивается на ", Mode: MODBUS", ", Mode: RAW" или ", Mode: RAW (takeover)").'),
			('CFGREJECT', 'E', 'Файл настроек отклонён из-за ошибок: они указаны в сообщениях CFG-E выше (предупреждения не считаются), ничего не применено, шлюз не запускается (см. 7.3).'),
			('CFGDEFAULTS', 'W', 'N значений файла настроек непригодны: см. сообщения CFG-W выше. Шлюз запускается, берутся умолчания. Прочитайте их, исправьте файл и снова запустите /check (см. Плохие значения и умолчания в 3.1).'),
			('CHARSDEF', 'W', 'Часть chars (скорость, биты данных, чётность, стоп-биты) непригодна; для неё взято умолчание (скорость 19200, биты данных 8, чётность E, стоп-биты 1). Исправьте chars.'),
			('SIGERR', 'E', 'Не удалось установить обработчик сигнала — внутренняя ошибка, сообщите о ней.'),
			('CFGERR', 'E', 'Конфигурацию не удалось обработать; причина — в строках выше.'),
			('CFGABORT', 'F', 'Шлюз останавливается из-за ошибок конфигурации выше.'),
			('DEVLOCKERR', 'E', 'Последовательная линия была занята 39 секунд (её держит другой запрос); запрос получает исключение 4.'),
			('TXNODATA', 'W', 'Пустой запрос не отправлен — безвредно.'),
			('XMITERR', 'E', 'Запрос не записался в последовательный порт; запрос получает исключение 4.'),
			('XMITSHORT', 'E', 'В последовательный порт записалась только часть запроса.'),
			('RXBUFFULL', 'W', 'Устройство прислало больше 256 октетов; остаток отброшен.'),
			('RXNODATA', 'E', 'Ответ начался, но не собрано ни одного октета.'),
			('RESPMISM', 'W', 'Ответ пришёл от другого slave или на другую функцию: два мастера или два устройства с одним адресом.'),
			('BADSPEED', 'W', 'Скорость нестандартная; используется 9600. С V.01-04 проверка настроек такую скорость не пропускает: см. CHARSDEF.'),
			('NOTTTY', 'W', 'Устройство — не последовательный порт.'),
			('RS485OFF', 'W', 'Режим RS485 для этого порта не включился; порт работает без него.'),
			('RS485ON', 'S', 'Режим RS485 для этого порта включён.'),
			('RAWSTALL', 'W', 'RAW-клиент ушёл, а порт за iotmo + 10 секунд + время передачи 2 КБ на его скорости не передал ничего из его последних данных (CTS снят или устройство прислало XOFF); данные выброшены, порт свободен для следующего клиента.'),
			('FLOWNOTSET', 'W', 'Драйвер порта не принял настройку flow (у некоторых USB-адаптеров нет RTS/CTS); порт работает без управления потоком.'),
			('DEVINIERR', 'E', 'Последовательный порт не настроился; причина — в строке выше.'),
			('DEVINIT', 'I', 'Последовательный порт настроен.'),
			('DEVCLSERR', 'E', 'Последовательный порт не закрылся.'),
			('DEVNOTINI', 'W', 'Закрытие порта, который не открывался — безвредно.'),
			('DEVBUSY', 'E', 'Последовательную линию не удалось занять под запрос; запрос получает исключение 4.'),
			('CONNLIMIT', 'W', 'Новый TCP-клиент сразу отключён: у порта уже connlm клиентов.'),
			('RAWSTART', 'I', 'RAW-сессия началась: показаны адрес клиента и последовательный порт. Делать ничего не нужно.'),
			('RAWSTOP', 'I', 'RAW-сессия закончилась; показано, сколько октетов ушло в линию и сколько пришло из линии. Делать ничего не нужно — по цифрам видно, шли ли данные вообще.'),
			('RAWKICK', 'W', 'Новый клиент занял RAW-порт (takeover = 1): старое соединение закрыто. Если это само повторяется снова и снова, две программы борются за один порт — остановите одну из них или поставьте takeover = 0.'),
			('RAWBUSY', 'W', 'Новому клиенту отказано: RAW-порт занят, а takeover = 0. Подождите, пока первый клиент уйдёт, или поставьте takeover = 1.'),
			('TXDRAINTMO', 'E', 'Адаптер не передал запрос в линию вовремя; запрос сброшен и получает исключение 4.'),
			('ECHOMISM', 'W', 'Ответ на запрос записи — не его эхо (другой адрес или значение); запрос получает исключение 4.'),
			('LATEANS', 'W', 'После неудачного запроса за lategd из линии что-то пришло — опоздавший ответ; он выброшен.'),
			('LATEPRETX', 'W', 'Перед отправкой запроса из линии что-то пришло — опоздавший ответ на прежний запрос; он выброшен.'),
			('DEVLOST', 'E', 'Последовательный порт пропал (выдернут USB-адаптер); он закрыт и открывается снова раз в секунду, запросы пока получают исключение 10.'),
			('DEVABSENT', 'W', 'При старте последовательный порт не открылся; TCP-порт всё равно работает, порт открывается раз в секунду.'),
			('CTLREADY', 'S', 'Канал управления для mbus-ctl открыт; показан файл-сокет.'),
			('CTLOFF', 'I', 'Канал управления выключен опцией /control=NONE; mbus-ctl до этого шлюза не достучится.'),
			('CTLBUSY', 'W', 'Этим файлом-сокетом уже пользуется другой работающий шлюз; этот работает без канала управления. Дайте каждому шлюзу свой /control=&lt;файл&gt;.'),
			('CTLERR', 'W', 'Файл-сокет не создаётся (2 — нет каталога, 13 — нет прав, 88 — там не сокет); шлюз работает без канала управления.'),
			('CTLSYSWRN', 'W', 'Системный вызов канала управления не удался; шлюз работает дальше.'),
			('CTLRESET', 'I', 'Оператор отключил этого клиента (mbus-ctl reset connection). Ничего делать не нужно.'),
			('CTLRESTART', 'I', 'Оператор попросил перезапуск, файл настроек хороший; шлюз перезапускается.'),
			('CTLCHKFAIL', 'E', 'restart gateway отклонён: в файле настроек ошибки (mbus-ctl их показывает). Шлюз работает дальше. Исправьте файл и повторите.'),
			('CHECKOK', 'S', '/check: файл настроек хороший (возможно, с предупреждениями).'),
			('RESTARTING', 'I', 'Шлюз запускает сам себя заново с той же командной строкой; дальше обычные строки запуска.'),
			('EXECERR', 'F', 'Перезапуск не удался; шлюз останавливается. systemd и procd запустят его снова; вручную — запустите снова.'),
			('CTLNOGW', 'E', 'mbus-ctl не достучался до шлюза. errno 2: не запущен или другой файл-сокет (/socket=); 13: нет прав — sudo; 111: не запущен, остался старый файл.'),
			('CTLANSERR', 'E', 'Шлюз отклонил запрос; показана его причина.'),
			('CTLBADANS', 'E', 'mbus-ctl не понял ответ — шлюз другой версии? Приведите строку.'),
			('CTLWAIT', 'I', 'mbus-ctl ждёт перезапущенный шлюз.'),
			('CTLBACK', 'S', 'Шлюз вернулся после перезапуска.'),
			('CTLNOBACK', 'W', 'Шлюз не ответил после перезапуска вовремя; читайте его журнал.'),
			('CTLRESETN', 'S', 'mbus-ctl reset connection отключил столько клиентов.'),
			('CTLBADSEL', 'E', 'mbus-ctl reset connection нужно указать соединение: Id из sho conn, a.b.c.d, a.b.c.d:порт или /all.'),
			('CTLCUT', 'W', 'Ответ не поместился в 64 КБ (многие сотни клиентов): таблица неполная.'),
		 ], [26*mm, 11*mm, 126*mm], a_mono_cols=(0,)),
		 H2('7.3.  Сообщения CFG (проверка файла настроек)'),
		 P('Файл настроек читает и проверяет библиотека yaLIBCFG; её сообщения начинаются с %CFG-E- (ошибка) или '
		   '%CFG-W- (предупреждение) и заканчиваются файлом, строкой и столбцом проблемы. После ошибок шлюз печатает '
		   'CFGREJECT, после предупреждений — CFGDEFAULTS. Те, с которыми вы встретитесь:'),
		 tbl(['Код', 'Что говорит', 'Что делать'], [
			('SYNTAX', 'Файл написан не по правилам синтаксиса.', 'Посмотрите строку и столбец (настоящая ошибка часто прямо перед ними): пропущены ; или = или скобка или кавычка.'),
			('OPENIN', 'Файл не открывается; errno говорит почему (2 = нет такого файла, 13 = нет прав).', 'Проверьте имя в /settings= и права на файл.'),
			('UNKNOWN', 'Настройка, которой шлюз не знает.', 'Опечатка? Сравните имя с разделами 3.1 и 3.2.'),
			('REQUIRED', 'Нет обязательной настройки.', 'Добавьте её (см. столбец «Обязателен» в таблицах главы 3).'),
			('VTYPE', 'У значения не тот тип.', 'Пишите то, что ждёт сообщение: текст в кавычках, число без них, слово из списка в кавычках.'),
			('RANGE', 'Число вне диапазона; допустимый диапазон — в сообщении.', 'Исправьте значение.'),
			('LENGTH', 'Текст слишком короткий или слишком длинный; допустимая длина — в сообщении.', 'Исправьте значение.'),
			('COUNT', 'Слишком мало или слишком много записей в разделе (допускается 1..128).', 'Добавьте недостающие записи или сократите раздел.'),
			('BADKWD', 'Значение не из разрешённых слов; они перечислены в сообщении.', 'Возьмите одно из них.'),
			('DUPKEY', 'У двух записей один и тот же ключ (например, один device дважды); показана строка первой.', 'Уберите одну из них.'),
			('DUPNAME', 'Настройка записана дважды в одной группе (заглавные и строчные буквы не различаются) или раздел дважды.', 'Уберите одну из них.'),
			('CHECK', 'Нарушено правило шлюза; сообщение говорит какое (вид chars или bind, UDP, target, которого нет в serials, последовательный порт RAW-слушателя занят другим слушателем, XON/XOFF при MODBUS, RTS/CTS при rs485 = 1, ...).', 'Прочитайте текст сообщения и исправьте названную им настройку.'),
		 ], [22*mm, 74*mm, 67*mm], a_mono_cols=(0,)),
		 P('RANGE, BADKWD, VTYPE и CHECK печатаются как %CFG-W-... с - the default is taken в конце, если у настройки есть умолчание: смысл тот же, но это только предупреждение (см. Плохие значения и умолчания в 3.1).'),
		 P('Полный список сообщений CFG — в руководстве yaLIBCFG (Приложение A).'),
		 H2('7.4.  Если ничего не помогло'),
		 P('Соберите и приложите к вопросу: 1) полный журнал запуска с /trace (от REVISNF до '
		   'первой ошибки); 2) ваш файл настроек; 3) вывод <font face="Mono">ls -l '
		   '&lt;устройство&gt;</font> и <font face="Mono">dmesg | tail -20</font>; 4) точную '
		   'модель RTU-устройства и его паспортные параметры линии. С этими четырьмя вещами '
		   'проблема почти всегда видна с первого взгляда.')],
		]
	L['chapters'] = chapters
	return L


if __name__ == '__main__':
	l_lang, l_out = sys.argv[1], sys.argv[2]
	build (s_pack_en() if l_lang == 'EN' else s_pack_ru(), l_out)
	print ('%s -> %s' % (l_lang, l_out))
