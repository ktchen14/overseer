#! /bin/bash -e

if [ -n "$(apt-cache search open-vm-tools)" ]; then
  apt-get install -y open-vm-tools
  mkdir /mnt/hgfs
else
  mkdir -p /mnt/vmware
  mount -o loop ~/linux.iso /mnt/vmware

  cd /tmp
  tar -xzf /mnt/vmware/VMwareTools-*.tar.gz
  umount /mnt/vmware

  vmware-tools-distrib/vmware-install.pl -d -f
  rm -rf vmware-tools-distrib
fi

# Always remove the VMware Tools ISO uploaded by Packer
rm -f ~/linux.iso
