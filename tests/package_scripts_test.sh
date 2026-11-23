#!/bin/sh
#	The .deb maintainer scripts (cmake/deb/postinst, prerm, postrm) with mocked systemctl,
#	deb-systemd-invoke and deb-systemd-helper: which calls are made for which dpkg argument,
#	and that a failing call never fails a script.
#
#	$ sh tests/package_scripts_test.sh [<source tree>]	(default /src, as in the CI container)
#
#	Needs /run/systemd/system (the scripts touch systemd only when it runs); the mocks are
#	the only programs on PATH, nothing of the system is called.
set -eu

src=${1:-/src}
[ -d /run/systemd/system ] || { echo "Run where /run/systemd/system exists (mkdir it in a test container)" >&2; exit 1; }

tmp=$(mktemp -d)
trap '/bin/rm -rf "$tmp"' EXIT
#	A mock logs its call and fails when FAIL_TOOL names it; deb-systemd-helper debian-installed and
#	was-enabled answer by WAS_ENABLED (0 - the unit was installed and enabled; 1, the default - not)
for l_tool in systemctl deb-systemd-invoke deb-systemd-helper; do
	/bin/cat > "$tmp/$l_tool" <<EOF
#!/bin/sh
echo "$l_tool \$*" >> "\$TEST_LOG"
case "\$*" in *debian-installed*|*was-enabled*) exit "\${WAS_ENABLED:-1}";; esac
[ "\${FAIL_TOOL:-}" != "$l_tool" ]
EOF
	/bin/chmod +x "$tmp/$l_tool"
done
TEST_LOG="$tmp/actions"
PATH="$tmp"
export TEST_LOG PATH
unset DPKG_ROOT FAIL_TOOL || true

run () {		# run <script> <args...>: a clean log, the script must succeed
	l_script=$1
	shift
	: > "$TEST_LOG"
	/bin/sh "$src/cmake/deb/$l_script" "$@" 2>/dev/null || { echo "FAIL: $l_script $* exited with $?" >&2; exit 1; }
}
has () { /bin/grep -qx "$1" "$TEST_LOG" || { echo "FAIL: no '$1' in:" >&2; /bin/cat "$TEST_LOG" >&2; exit 1; }; }
hasnt () { ! /bin/grep -q "$1" "$TEST_LOG" || { echo "FAIL: '$1' in:" >&2; /bin/cat "$TEST_LOG" >&2; exit 1; }; }

#	postinst: a fresh installation starts (deb-systemd-invoke skips a disabled unit), an upgrade restarts
run postinst configure
has 'deb-systemd-helper update-state mbusgw-t2r.service'
has 'systemctl --system daemon-reload'
has 'deb-systemd-invoke start mbusgw-t2r.service'
run postinst configure 1.4.1-1
has 'deb-systemd-invoke restart mbusgw-t2r.service'
hasnt 'deb-systemd-helper enable'
WAS_ENABLED=0; export WAS_ENABLED			# an enabled unit gets the links of the new unit file
run postinst configure 1.4.1-1
has 'deb-systemd-helper enable mbusgw-t2r.service'
has 'deb-systemd-helper update-state mbusgw-t2r.service'
unset WAS_ENABLED
run postinst abort-remove
has 'deb-systemd-invoke start mbusgw-t2r.service'
run postinst abort-upgrade 1.4.2-1
has 'deb-systemd-invoke restart mbusgw-t2r.service'
run postinst triggered
hasnt '.'

#	prerm: only a removal stops, nothing is disabled; an upgrade leaves the service to postinst
run prerm remove
has 'deb-systemd-invoke stop mbusgw-t2r.service'
hasnt 'disable'
run prerm upgrade 1.4.2-1
hasnt '.'

#	postrm: a removal reloads systemd; a purge removes the recorded links
run postrm remove
has 'systemctl --system daemon-reload'
hasnt 'deb-systemd-helper'
run postrm upgrade 1.4.2-1
hasnt '.'
run postrm purge
has 'deb-systemd-helper purge mbusgw-t2r.service'
hasnt 'systemctl'

#	A failure of any tool is told but never fails a script
for FAIL_TOOL in systemctl deb-systemd-invoke deb-systemd-helper; do
	export FAIL_TOOL
	run postinst configure
	run postinst configure 1.4.1-1
	run prerm remove
	run postrm remove
	run postrm purge
done
unset FAIL_TOOL

#	DPKG_ROOT (dpkg --root): systemd of the host is not touched
DPKG_ROOT="$tmp/root"; export DPKG_ROOT
run postinst configure 1.4.1-1
hasnt 'systemctl'
hasnt 'deb-systemd-invoke'
run prerm remove
hasnt '.'
unset DPKG_ROOT

echo "Package service script checks passed"
