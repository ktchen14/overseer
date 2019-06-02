#! /bin/bash -e

rm -rf /dev/.udev

# Clean up the apt cache
apt-get -y autoremove --purge
apt-get -y clean
apt-get -y autoclean

rm -rf /tmp/*

# Clear out all logs
find /var/log -type f | while read name; do echo -n > "$name"; done

# Zero out the swap partition and remake it with the same UUID because
# /etc/fstab specifies UUIDs

set +e
uuid="$(blkid -o value -l -s UUID -t TYPE=swap)"
e=$?
[[ $e -eq 0 || $e -eq 2 ]] || exit $e
set -e

if [ -n "$uuid" ]; then
  swap="$(readlink -f "/dev/disk/by-uuid/$uuid")"
  swapoff "$swap"
  dd if=/dev/zero of="$swap" bs=1M &> /dev/null || true
  mkswap -U "$uuid" "$swap"
fi

# Zero out the unused space in the root partition to force a compaction
dd if=/dev/zero of=/zero bs=1M &> /dev/null || true
rm -f /zero
sync

history -c
