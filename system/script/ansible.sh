#! /bin/bash -e

# Install `python3-minimal` for Ansible
apt-get install -y --no-install-recommends python3-minimal

# Enable the `apt` module
apt-get install -y --no-install-recommends aptitude python3-apt
