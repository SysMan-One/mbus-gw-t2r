#!/bin/sh
set -eu

[ -d /run/systemd/system ] || { echo "Run inside a test container with /run/systemd/system" >&2; exit 1; }
[ ! -x /usr/sbin/policy-rc.d ] || { echo "Test container must not have policy-rc.d" >&2; exit 1; }

tmp=$(mktemp -d)
trap '/bin/rm -f "$tmp/systemctl" "$tmp/deb-systemd-invoke" "$tmp/deb-systemd-helper" "$tmp/actions"; /bin/rmdir "$tmp"' EXIT
cat > "$tmp/systemctl" <<'EOF'
#!/bin/sh
echo "systemctl $*" >> "$TEST_LOG"
[ "${FAIL_ACTION:-}" != "$1" ]
EOF
chmod +x "$tmp/systemctl"
TEST_LOG="$tmp/actions"
PATH="$tmp"
export TEST_LOG PATH

/bin/sh /src/cmake/deb/postinst configure
/bin/grep -qx 'systemctl restart mbusgw-t2r.service' "$TEST_LOG"
: > "$TEST_LOG"
/bin/sh /src/cmake/deb/prerm remove
/bin/grep -qx 'systemctl stop mbusgw-t2r.service' "$TEST_LOG"
/bin/grep -qx 'systemctl disable mbusgw-t2r.service' "$TEST_LOG"

FAIL_ACTION=restart
export FAIL_ACTION
if /bin/sh /src/cmake/deb/postinst configure; then
	echo "Failed restart was ignored" >&2
	exit 1
fi
FAIL_ACTION=stop
export FAIL_ACTION
if /bin/sh /src/cmake/deb/prerm remove; then
	echo "Failed stop was ignored" >&2
	exit 1
fi
FAIL_ACTION=disable
export FAIL_ACTION
if /bin/sh /src/cmake/deb/prerm remove; then
	echo "Failed disable was ignored" >&2
	exit 1
fi
unset FAIL_ACTION

/bin/cat > "$tmp/deb-systemd-invoke" <<'EOF'
#!/bin/sh
echo "invoke $*" >> "$TEST_LOG"
EOF
/bin/cat > "$tmp/deb-systemd-helper" <<'EOF'
#!/bin/sh
echo "helper $*" >> "$TEST_LOG"
EOF
/bin/chmod +x "$tmp/deb-systemd-invoke" "$tmp/deb-systemd-helper"
: > "$TEST_LOG"
/bin/sh /src/cmake/deb/postinst configure
/bin/sh /src/cmake/deb/prerm remove
/bin/grep -qx 'invoke restart mbusgw-t2r.service' "$TEST_LOG"
/bin/grep -qx 'invoke stop mbusgw-t2r.service' "$TEST_LOG"
/bin/grep -qx 'systemctl disable mbusgw-t2r.service' "$TEST_LOG"

/bin/rm "$tmp/systemctl"
: > "$TEST_LOG"
/bin/sh /src/cmake/deb/prerm remove
/bin/grep -qx 'helper disable mbusgw-t2r.service' "$TEST_LOG"
echo "Package service script checks passed"
