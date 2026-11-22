#!/bin/bash
#
#++
#  FACILITY:	mbus-gw-t2r - developer tools
#
#  MODULE:	precheck.sh
#
#  ABSTRACT:
#	Mechanical checks of a change before the critic agent reviews it (the
#	review rule of 10-NOV-2026): what a script can catch, the critic does
#	not have to.  Only the lines and files the change touches are checked,
#	so old code that predates a rule does not drown the new findings.
#	Ported from yaVNA's tools/precheck.sh (311f121), by way of yaIA64Emu's.
#
#	STYLE	  added lines of src/ (tests/ are exempt): no bare $LOG/$PUTMSG,
#		  no perror/syslog/err/warn/psignal, nothing to stderr (stderr or
#		  STDERR_FILENO anywhere in code, write(2)) - messages go through
#		  $LOG_FAO/$PUTMSG_FAO; a counted string (ASC) is given to them as
#		  it is (!AC, &asc), not as $ASCPTR() for !AZ (seen only when the
#		  call is on one line).  One error per line is reported: the
#		  next one shows after the first is fixed.  A help/usage display
#		  is exempt: mark the line with the comment "precheck: help".
#		  stdout (printf,
#		  puts, fputs, write(1)) is not checked: the displays of
#		  mbus-ctl (SHOW, MONITOR ...) are its output, not messages,
#		  and the signal handler of t2r_main.c writes to fd 1 by
#		  write() because only async-signal-safe calls are allowed there.
#	REV	  a changed .c/.h of src/ that carries __REV__ has its __REV__ or
#		  __IDENT__ value changed (an error) and a new MODIFICATION
#		  HISTORY line (a warning), e.g.
#		  "**<tab>17-NOV-2026<tab>CLD<tab>V.01-01ECO01 / REV: 01.01.01 - ...";
#		  a new .c defines both.  Not an error:
#		  - a continuation: every line the change adds to or removes
#		    from the history block lies in its current entry (the one
#		    of the file's __IDENT__ - all its entries in a row, oldest
#		    first in most files here;
#		    a removed line is placed where its hunk starts in the new
#		    file), and no added or removed history line names another
#		    ident (an entry of a stage not yet closed rewritten or
#		    extended; an informational line);
#		  - a change of comments only (a warning) - on the lines with no
#		    code; a comment added or changed on a line of code counts as
#		    a change of that code.
#		  A new history line of another ident, or an edit of an older
#		  entry, together with a code change is an error as before.  Most
#		  headers carry no __REV__ (t2r_defs.h, t2r_msgs.h ...) and are
#		  not checked; tests/, openwrt/ and contrib/ are not versioned.
#		  __IDENT__ and __REV__ change together and agree: V.01-01ECO01 is
#		  1.1.1 (the old X-aa-bb form is not compared; a rename of one
#		  with the numbers agreeing, X.01-01 -> V.01-01, is not warned).
#		  A continuation cannot tell an open stage from a released one
#		  (a tag): the critic checks that.
#	Comments and string contents are not checked: /* */, // and "..."
#	are stripped from the changed lines (STYLE, and whether code changed).
#	A hunk that starts inside a comment needs '*'/'**' leaders - so a line
#	that starts with '* ' or '*/' outside a known comment counts as one.
#	WSPACE	  git diff --check: trailing blanks (a warning), conflict
#		  markers (an error); the .pdf files of docs/ are skipped.
#	ANALYZE	  gcc -fanalyzer on the changed .c of src/, with the project's
#		  own compile flags (a configure-only CMake tree in
#		  ~/.cache/mbus-gw-t2r-precheck-<hash of the checkout>, refreshed
#		  when CMakeLists.txt or cmake/ changes; StarLet is looked for
#		  where find_package (StarLet) looks, then in /opt/starlet;
#		  yaLIBCFG in /opt/yalibcfg; $CMAKE_PREFIX_PATH is added), at
#		  most nproc at a time, each cut off after 900 s.  Needs jq.
#		  /NOANALYZE turns off this check only.
#	CPPCHECK  cppcheck on the same files, if installed: the one
#		  configuration of a gcc build on this host (-D__GNUC__,
#		  __linux__, __<arch>__), the include directories of src/,
#		  yaLIBCFG's and StarLet's.  A file cppcheck cannot parse is an
#		  informational line, not a warning.  cppcheck 2.10 cannot parse
#		  the $DEF_MSG token pasting of src/t2r_msgs.h, which every .c
#		  includes - so in practice cppcheck checks nothing here and the
#		  analyzer is the check that counts.
#	SHELL	  shellcheck on changed shell scripts (not the procd init scripts
#		  of openwrt/*/files/: rc.common is theirs), if installed; off: SC1087
#		  (POSIX classes in sed/grep regexps), SC1007 (CDPATH= cd),
#		  SC1090/SC1091 (a sourced file named at run time), SC2148 (a
#		  sourced file has no "#!"), SC2183 (printf "%64s"
#		  with no argument: a run of blanks).
#
#	Usage (DCL style, qualifiers case-blind, any unique abbreviation):
#		tools/precheck.sh [/STAGED] [/BASE=ref] [/[NO]ANALYZE] [/HELP]
#	/STAGED (default): the staged diff; nothing staged - the work tree
#	against HEAD (untracked files are not seen - git add -N them).
#	/BASE=ref: the work tree against ref; the value runs to the end of
#	the argument, so /BASE=origin/main works.  The REV check reads
#	the files the diff is of (the index for staged changes, else the work
#	tree); the analyzers read the work tree, so after a partial git add -p
#	they see unstaged lines too.
#	Exit status 0 - no errors (warnings may be), 1 - errors, 2 - bad usage,
#	not a git tree or a failed scan.
#
#  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#  CREATION DATE:	17-NOV-2026
#
#  MODIFICATION HISTORY:
#
#	X.01-00	RRL	17-NOV-2026	Initial version, from yaIA64Emu's precheck.sh (yaVNA's X.01-00, 311f121).
#--
#
FAC=PRECHECK
SELF=$(realpath "$0")
if ! TOP=$(git rev-parse --show-toplevel 2>/dev/null) || ! cd "$TOP"; then
	echo "%$FAC-F-NOREPO, not inside a git work tree" >&2
	exit 2
fi

MODE=staged BASE='' ANALYZE=1
qual_match ()	# $1 - what was typed, $2.. - the keywords; prints the one it abbreviates
{
	local u=$1 w=${1^^} k hits=()
	shift
	for k; do [[ $k == "$w"* ]] && hits+=("$k"); [[ $k == "$w" ]] && { echo "$k"; return 0; }; done
	(( ${#hits[@]} == 1 )) && { echo "${hits[0]}"; return 0; }
	(( ${#hits[@]} > 1 )) && echo "%$FAC-E-ABKEYW, /$u is ambiguous: ${hits[*]}" >&2 || echo "%$FAC-E-IVKEYW, unknown qualifier /$u" >&2
	return 1
}
for a in "$@"; do
	s=$a
	while s=${s#/}; [[ -n $s ]]; do		# split on '/', but a value after '=' takes the rest
		if [[ $s == *=* && ${s%%=*} != */* ]]; then
			n=${s%%=*} v=${s#*=} s='' eq=1
		else
			n=${s%%/*} v='' eq=0
			if [[ $s == */* ]]; then s=/${s#*/}; else s=''; fi
		fi
		k=$(qual_match "$n" STAGED BASE ANALYZE NOANALYZE HELP) || exit 2
		[[ $k == BASE || $eq == 0 ]] || { echo "%$FAC-E-NOVALUE, /$k takes no value" >&2; exit 2; }
		case $k in
		STAGED)	MODE=staged ;;
		BASE)	[[ -n $v ]] || { echo "%$FAC-E-NEEDVAL, /BASE needs a ref: /BASE=HEAD~1" >&2; exit 2; }
			MODE=base BASE=$v ;;
		ANALYZE) ANALYZE=1 ;;
		NOANALYZE) ANALYZE=0 ;;
		HELP)	sed -n '/^#  ABSTRACT:/,/^#  AUTHOR:/p' "$SELF" | sed '$d; s/^#//'; exit 0 ;;
		esac
	done
done

if [[ $MODE == base ]]; then
	git rev-parse -q --verify "$BASE^{commit}" >/dev/null || { echo "%$FAC-E-BADREF, no such commit: $BASE" >&2; exit 2; }
	RANGE=("$BASE") WHAT="work tree against $BASE"
elif git diff --cached --quiet; then
	RANGE=(HEAD) WHAT="work tree against HEAD (nothing staged)"
else
	RANGE=(--cached) WHAT="staged changes"
fi

unset GIT_DIFF_OPTS GIT_EXTERNAL_DIFF					# -u5 there would override -U0
GD=(git -c core.quotePath=false diff --no-color --no-ext-diff --inter-hunk-context=0 --src-prefix=a/ --dst-prefix=b/)	# immune to the user's diff config
NERR=0 NWARN=0
msg ()	# $1 - severity letter, $2 - ident, $3.. - text
{
	local s=$1 id=$2; shift 2
	echo "%$FAC-$s-$id, $*"
	case $s in E) ((NERR++)) ;; W) ((NWARN++)) ;; esac
}

mapfile -t FILES < <("${GD[@]}" "${RANGE[@]}" --name-only --diff-filter=ACMR)
echo "%$FAC-I-SCOPE, $WHAT: ${#FILES[@]} file(s)"
(( ${#FILES[@]} )) || { echo "%$FAC-S-NOTHING, no changed files"; exit 0; }

#
# One awk (a process costs ~0.1 s here, so not one git or grep per file).  First the changed .c/.h of
# src/ as the diff has them - the index (one git cat-file --batch) for staged changes, else
# the work tree: whether a file carries __REV__, its __IDENT__, its history block and the lines of the
# entries.  Then their -U0 diff on stdin:
#	A<TAB>file<TAB>lineno<TAB>code<TAB>text	- an added line of src/: its code without comments and string
#						  contents (tabs as blanks), the line itself
#	F<TAB>file<TAB>new<TAB>identchg<TAB>revchg<TAB>hist<TAB>ident<TAB>rev<TAB>hasrev<TAB>cont<TAB>code
#		- per file: new file, the __IDENT__ / __REV__ value changed, a history line added, the
#		  added __IDENT__ and __REV__ values ("-" when none), the file carries __REV__, the change
#		  continues the current history entry (see REV above), a line of code added or removed
# A history line: "**", the date, the initials (may be absent), the ident (V.01-01ECO01, X.00-04: ...).
#
CH=() SRC=() CF=0
for f in "${FILES[@]}"; do
	[[ $f == src/*.[ch] ]] && CH+=("$f")
done
if [[ ${RANGE[0]} == --cached ]]; then
	CF=1						# the objects come back in the order asked, matched by it (PRECHECK_PATHS)
	PRECHECK_PATHS=$(printf '%s\n' "${CH[@]}"); export PRECHECK_PATHS
	(( ${#CH[@]} )) || CH=(/dev/null)		# a "missing" line, read past
else
	for f in "${CH[@]}"; do [[ -f $f ]] && SRC+=("$f"); done
fi
SCAN=$({ (( CF )) && printf ':%s\n' "${CH[@]}" | git cat-file --batch='%(objectname) %(objecttype) %(objectsize)'
	 echo "@@ precheck: the diff @@"; "${GD[@]}" "${RANGE[@]}" -U0 --diff-filter=ACMR -- src; } | LC_ALL=C awk '
	function hist(t) { return t ~ /^\*\*[ \t]+[0-9]?[0-9]-[A-Z][A-Z][A-Z]-[0-9][0-9][0-9][0-9][ \t]+([A-Z][A-Z]+[ \t]+)?[XV][.-]?[0-9][0-9]-[0-9][0-9](ECO[0-9]+)?([^0-9A-Za-z]|$)/ }
	function hid(t,   w, i) { split(t, w, /[ \t]+/); for (i = 3; i <= 4; i++) if (w[i] ~ /^[XV][.-]?[0-9][0-9]-[0-9][0-9]/) { sub(/[^0-9A-Za-z]+$/, "", w[i]); return w[i] } return "?" }
	function val(t) { return match(t, /"[^"]+"/) ? substr(t, RSTART + 1, RLENGTH - 2) : "?" }
	function src(p, t, l) {			# line l of file p
		if (t ~ /^#define[ \t]+__REV__/) { hr[p] = 1; if (!(p in cr)) cr[p] = val(t) }
		if (t ~ /^#define[ \t]+__IDENT__/ && !(p in ci)) ci[p] = val(t)
		if (!hb[p]) { if (t ~ /(MODIFICATION|REVISION) HISTORY/) hb[p] = l; return }
		if (he[p]) return
		if (t ~ /^\*\*--/) he[p] = l
		else if (hist(t)) { k = ++ne[p]; es[p, k] = l; eid[p, k] = hid(t) }
	}
	function code(x, k,   c, i, n, ch, q) {	# x without comments and string/char contents; inc[k] carried from line to line
		c = ""; q = ""; n = length(x)
		for (i = 1; i <= n; i++) {
			ch = substr(x, i, 1)
			if (inc[k]) { if (substr(x, i, 2) == "*/") { inc[k] = 0; i++; c = c " " } continue }
			if (q != "") { if (ch == "\\") i++; else if (ch == q) { q = ""; c = c ch } continue }
			if (substr(x, i, 2) == "/*") { inc[k] = 1; i++; continue }
			if (substr(x, i, 2) == "//") break
			if (ch == "\"" || ch == "\047") q = ch
			c = c ch
		}
		gsub(/\t/, " ", c)
		return c
	}
	function lead(x, k) { sub(/^[ \t]+/, "", x); return !inc[k] && x ~ /^\*\*?([ \t\/]|$)/ }	# a leader line of a comment opened before the hunk
	function cur(   k, j) {			# the range of the current entry: the run of entries of its __IDENT__ (several in a row happen)
		cs = ce = 0
		for (k = 1; k <= ne[f]; k++) if (eid[f, k] == ci[f]) {
			for (j = k + 1; j <= ne[f] && eid[f, j] == ci[f]; j++) ;
			cs = es[f, k]; ce = (j <= ne[f]) ? es[f, j] : he[f]; return
		}
	}
	function flush() {
		if (f == "") return
		cont = hx && !ho && !fo
		printf "F\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\n", f, nw, iv != "-" && iv != ivo, vv != "-" && vv != vvo, hs, iv != "-" ? iv : (f in ci ? ci[f] : "-"), vv != "-" ? vv : (f in cr ? cr[f] : "-"), hr[f] || vvo != "-", cont, cd
	}
	FILENAME != "-"	{ src(FILENAME, $0, FNR); next }		# a work-tree file
	!dif		{ if (rem > 0) { src(cp, $0, ++ln); rem -= length($0) + 1; if (!rem) sep = 1; next }	# git cat-file --batch
			  if (sep) { sep = 0; next }			# the newline git puts after an object
			  if ($0 == "@@ precheck: the diff @@") { dif = 1; next }
			  if (!np) np = split(ENVIRON["PRECHECK_PATHS"], P, "\n")
			  if ($NF == "missing") { oi++; next }
			  if ($2 == "blob") { rem = $3 + 0; cp = P[++oi]; ln = 0; if (!rem) sep = 1 }
			  next }
	/^diff --git /	{ flush(); f = ""; nw = hs = hx = ho = fo = cd = 0; iv = vv = ivo = vvo = "-"; hd = 1; next }
	hd && /^new file mode/ { nw = 1; next }
	hd && /^\+\+\+ / { f = substr($0, 7); sub(/\t$/, "", f); if (f !~ /\.[ch]$/) f = ""; st = (f ~ /^src\//); cur(); next }
	f == ""		{ next }
	/^@@/		{ hd = 0; split($3, a, ","); n = substr(a[1], 2) + 0; inc["+"] = inc["-"] = 0; next }
	/^ /		{ t = substr($0, 2)					# a context line: keeps the line number and the comment state right
			  if (!lead(t, "+")) code(t, "+")
			  if (!lead(t, "-")) code(t, "-")
			  n++; next }
	/^-/		{ t = substr($0, 2)
			  if (t ~ /^#define[ \t]+__IDENT__/) ivo = val(t)
			  if (t ~ /^#define[ \t]+__REV__/) vvo = val(t)
			  if (n > hb[f] && n <= he[f]) { if (cs && n >= cs && n < ce) hx = 1; else ho = 1 }	# n: where the hunk is in the new file
			  if (hist(t) && hid(t) != ci[f]) fo = 1
			  if (!lead(t, "-") && code(t, "-") ~ /[^ ]/) cd = 1
			  next }
	/^\+/		{ t = substr($0, 2)
			  if (t ~ /^#define[ \t]+__IDENT__/) iv = val(t)
			  if (t ~ /^#define[ \t]+__REV__/) vv = val(t)
			  if (hist(t)) { hs = 1; if (hid(t) != ci[f]) fo = 1 }
			  if (n > hb[f] && n < he[f]) { if (cs && n >= cs && n < ce) hx = 1; else ho = 1 }
			  if (lead(t, "+")) { n++; next }
			  c = code(t, "+")
			  if (c ~ /[^ ]/) { cd = 1; if (st) printf "A\t%s\t%d\t%s\t%s\n", f, n, c, t }
			  n++; next }
	END		{ flush() }' "${SRC[@]}" -) || { echo "%$FAC-F-SCANFAIL, the scan of the diff failed - nothing checked" >&2; exit 2; }

#
# STYLE - on the code of the line, the line itself shown
#
while IFS=$'\t' read -r k f ln cod txt; do
	[[ $k == A ]] || continue
	[[ $txt == *"precheck: help"* ]] && continue
	if [[ $cod =~ \$(PUTMSG|LOG)_FAO[[:space:]]*\(.*\$ASCPTR[[:space:]]*\( ]]; then
		msg E ASCIZARG "$f:$ln: a counted string (ASC) goes to the message as it is - !AC and &asc, not \$ASCPTR() with !AZ"
	elif [[ $cod =~ \$(LOG|PUTMSG)[[:space:]]*\( ]]; then
		msg E BARELOG "$f:$ln: bare \$${BASH_REMATCH[1]} - use \$${BASH_REMATCH[1]}_FAO"
	elif [[ $cod =~ (^|[^_[:alnum:]\$.>])(perror|v?syslog|v?errx?|v?warnx?|psignal|psiginfo)[[:space:]]*\( ]]; then
		msg E NOPUTMSG "$f:$ln: ${BASH_REMATCH[2]}() - messages go through \$LOG_FAO/\$PUTMSG_FAO"
	elif [[ $cod =~ (^|[^_[:alnum:]])(stderr|STDERR_FILENO)([^_[:alnum:]]|$) || $cod =~ (^|[^_[:alnum:]])(write|v?dprintf)[[:space:]]*\([[:space:]]*2[[:space:]]*, ]]; then
		msg E NOPUTMSG "$f:$ln: output to stderr - a message goes through \$PUTMSG_FAO or \$LOG_FAO (a help/usage display: mark it \"precheck: help\")"
	else
		continue
	fi
	printf '\t-> %s\n' "${txt#"${txt%%[![:space:]]*}"}"
done <<<"$SCAN"

#
# REV - a changed file only if it carries __REV__ (the headers do not)
#
rev_nums ()	# $1 - an __IDENT__ or __REV__ value; prints its three numbers "a b c", nothing for the old X-aa-bb form
{
	local v=$1
	[[ $v =~ ^[XV]- ]] && return
	if [[ $v =~ ^[XV]\.?([0-9]+)-([0-9]+)(ECO([0-9]+))?$ ]]; then
		echo "$((10#${BASH_REMATCH[1]})) $((10#${BASH_REMATCH[2]})) $((10#${BASH_REMATCH[4]:-0}))"
	elif [[ $v =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
		echo "$((10#${BASH_REMATCH[1]})) $((10#${BASH_REMATCH[2]})) $((10#${BASH_REMATCH[3]}))"
	fi
}
while IFS=$'\t' read -r k f nw idc rvc hs iv vv hr cont cd; do
	[[ $k == F ]] || continue
	if (( nw )); then
		[[ $f == *.h ]] && continue
		[[ $iv != - && $vv != - ]] || { msg E NOIDENT "$f: new module needs both #define __IDENT__ and __REV__"; continue; }
	else
		(( hr )) || continue
		if (( !idc && !rvc )); then
			if (( cont )); then
				echo "%$FAC-I-CONTINUED, $f: __REV__ kept, the current history entry (of __IDENT__) extended"
			elif (( cd )); then
				msg E REVNOTBUMPED "$f: changed, but __REV__/__IDENT__ is not"
			else
				msg W REVNOTBUMPED "$f: only comments changed, __REV__/__IDENT__ not"
			fi
			continue
		fi
		(( hs )) || msg W NOHISTORY "$f: no new MODIFICATION HISTORY line"
		a=$(rev_nums "$iv") b=$(rev_nums "$vv")
		if (( !idc || !rvc )) && [[ -n $a && $a == "$b" ]]; then
			:					# one renamed only, the numbers agree (X.01-01 -> V.01-01, REV 01.01.00)
		elif (( !idc )); then
			msg W REVPAIR "$f: __REV__ changed, __IDENT__ not - bump both together"; continue
		elif (( !rvc )); then
			msg W REVPAIR "$f: __IDENT__ changed, __REV__ not - bump both together"; continue
		fi
	fi
	a=$(rev_nums "$iv") b=$(rev_nums "$vv")
	[[ -n $a && -n $b && $a != "$b" ]] && msg W REVPAIR "$f: __IDENT__ \"$iv\" and __REV__ \"$vv\" disagree"
done <<<"$SCAN"

#
# WSPACE
#
while read -r l; do
	if [[ $l == *"conflict marker"* ]]; then
		msg E CONFLICT "$l"
	else
		msg W WSPACE "$l"
	fi
done < <("${GD[@]}" "${RANGE[@]}" --check -- . ':(exclude)*.pdf' 2>&1 | grep -av '^+')	# the PDFs of docs/ are not text (no .gitattributes)

#
# Changed C files of src/ - for ANALYZE and CPPCHECK
#
CFILES=()
for f in "${FILES[@]}"; do
	[[ $f == src/*.c && -f $f ]] && CFILES+=("$f")		# staged, but gone from the work tree: nothing to analyze
done

#
# ANALYZE
#
if (( ANALYZE && ${#CFILES[@]} )) && ! command -v jq >/dev/null; then
	echo "%$FAC-I-NOJQ, jq not installed (apt install jq) - analyzer skipped"
elif (( ANALYZE && ${#CFILES[@]} )); then
	CB=${XDG_CACHE_HOME:-$HOME/.cache}/mbus-gw-t2r-precheck-$(pwd -P | md5sum | cut -c1-8)	# one tree per checkout
	DB=$CB/compile_commands.json
	RECONF=0 PFX=() PP=
	for d in /usr/local /opt/starlet; do [[ -f $d/include/starlet.h ]] && { PP=$d; break; }; done
	[[ -f /opt/yalibcfg/include/yalibcfg.h ]] && PP=${PP:+$PP;}/opt/yalibcfg		# V.01-03: the settings file
	PP=${PP}${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}						# and any other the caller gives
	[[ -n $PP ]] && PFX=(-DCMAKE_PREFIX_PATH="$PP")
	for c in CMakeLists.txt cmake/*.cmake cmake/*/*; do [[ $c -nt $DB ]] && RECONF=1; done	# true also when there is no $DB
	if (( RECONF )); then
		echo "%$FAC-I-CONFIG, configuring $CB"
		mkdir -p "$CB"
		cmake -S . -B "$CB" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug "${PFX[@]}" >"$CB/configure.log" 2>&1 \
			|| { msg W NOCONFIG "cmake configure failed, see $CB/configure.log - analyzer skipped"; DB=/dev/null; }
	fi
	OUT=$(mktemp -d)
	trap 'rm -rf "$OUT"' EXIT
	mkdir "$OUT/f"
	NA=0 NJ=0 NP=$(nproc) T0=$SECONDS TMO=900
	for f in "${CFILES[@]}"; do
		[[ $DB == /dev/null ]] && break
		e=$(jq -c --arg f "$PWD/$f" '[.[] | select(.file == $f)][0] // empty' "$DB")
		[[ -n $e ]] || { echo "%$FAC-I-NOTBUILT, $f: not in this host's build, not analyzed"; continue; }
		(( NA )) || echo "%$FAC-I-ANALYZING, gcc -fanalyzer (/NOANALYZE skips it)"
		o=$OUT/f/$NA
		( cd "$(jq -r .directory <<<"$e")" && eval "timeout $TMO $(jq -r .command <<<"$e" | sed -E 's# -o [^ ]+# -o /dev/null#') -fanalyzer"
		  echo "$? $f" >"$o.rc" ) >"$o" 2>&1 &
		((NA++)); (( ++NJ >= NP )) && { wait -n; ((NJ--)); }	# at most nproc gcc at a time
	done
	wait
	for o in "$OUT"/f/*.rc; do
		[[ -f $o ]] || continue
		read -r rc f <"$o"
		o=${o%.rc}
		(( rc == 124 )) && echo "%$FAC-I-TOOLONG, $f: gcc -fanalyzer cut off after $TMO s, not analyzed"
		grep -E 'warning: .*\[-Wanalyzer-|error:' "$o" | grep -v 'Wanalyzer-too-complex' | sed "s#$PWD/##" | sort -u | while read -r l; do
			echo "%$FAC-W-ANALYZER, $l"
		done
	done > "$OUT/all"
	n=$(grep -c . "$OUT/all"); cat "$OUT/all"; (( NWARN += n ))
	(( NA )) && echo "%$FAC-I-ANALYZED, gcc -fanalyzer: $NA file(s), $n finding(s), $((SECONDS - T0)) s"
fi

#
# CPPCHECK
#
if (( ${#CFILES[@]} )); then
	if command -v cppcheck >/dev/null; then
		SL=/usr/local
		for d in /usr/local /opt/starlet; do [[ -f $d/include/starlet.h ]] && { SL=$d; break; }; done	# where find_package (StarLet) looks first
		SI=(-Isrc)
		IFS=';' read -ra YP <<< "${CMAKE_PREFIX_PATH:-}"
		for d in /opt/yalibcfg /usr/local "${YP[@]}"; do [[ -f $d/include/yalibcfg.h ]] && { SI+=("-I$d/include"); break; }; done	# V.01-03: yaLIBCFG
		while read -r l; do
			[[ -n $l ]] || continue
			if [[ $l == *"[internalAstError]" || $l == *"[syntaxError]" ]]; then
				echo "%$FAC-I-CPPNOPARSE, ${l%%:*}: cppcheck cannot parse it (${l#*: error: }) - the .c files including it are not checked by cppcheck"
			else
				msg W CPPCHECK "$l"
			fi
		done < <(cppcheck -j "$(nproc)" --quiet --enable=warning,portability --inline-suppr -D__GNUC__=12 -D__linux__ "-D__$(uname -m)__" \
			"${SI[@]}" "-I$SL/include" --template='{file}:{line}: {severity}: {message} [{id}]' "${CFILES[@]}" 2>&1)
	else
		echo "%$FAC-I-NOCPPCHECK, cppcheck not installed (apt install cppcheck) - skipped"
	fi
fi

#
# SHELL
#
SFILES=()
for f in "${FILES[@]}"; do
	[[ $f == openwrt/*/files/* ]] && continue		# procd init scripts: rc.common supplies START/STOP/local, shellcheck cannot see it
	[[ $f == *.sh ]] || head -1 "$f" 2>/dev/null | grep -qE '^#!.*\b(ba|da)?sh\b' && SFILES+=("$f")
done
if (( ${#SFILES[@]} )); then
	if command -v shellcheck >/dev/null; then
		while read -r l; do
			[[ -n $l ]] && msg W SHELLCHECK "$l"
		done < <(shellcheck -f gcc -S warning -e SC1007,SC1087,SC1090,SC1091,SC2148,SC2183 "${SFILES[@]}" 2>&1)
	else
		echo "%$FAC-I-NOSHELLCHECK, shellcheck not installed (apt install shellcheck) - skipped"
	fi
fi

if (( NERR )); then
	echo "%$FAC-E-FOUND, $NERR error(s), $NWARN warning(s)"
	exit 1
elif (( NWARN )); then
	echo "%$FAC-W-FOUND, no errors, $NWARN warning(s)"
else
	echo "%$FAC-S-CLEAN, nothing found"
fi
exit 0
