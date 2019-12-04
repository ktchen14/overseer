#! /bin/bash

if [ $# -ne 4 ]; then
  cat 1>&2 <<WARN
Expected 4 arguments in invocation of hook $(basename "$0") but received $#
WARN
  exit 0
fi

if [[ $2 = prepare && $3 = begin ]]; then
  exec /opt/vision/qemu-hook-prepare.rb "$@"
elif [[ $2 = release && $3 = end ]]; then
  exec /opt/vision/qemu-hook-release.rb "$@"
fi
