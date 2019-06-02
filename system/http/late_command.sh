#! /bin/bash -e

# Allow sudo for overseer user without password or tty
cat > "/etc/sudoers.d/overseer" <<EOF
Defaults:overseer !requiretty
overseer ALL=(ALL) NOPASSWD: ALL
EOF
chmod 440 "/etc/sudoers.d/overseer"
