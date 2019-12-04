#! /usr/bin/ruby

require 'logger'
require 'nokogiri'

document = Nokogiri::XML(STDIN) { |c| c.strict.noblanks }

# If the document's root node isn't a domain node or if the domain's type isn't
# KVM then this hook shouldn't be triggered
logger.warn(<<~WARN) and exit if document.root.name != 'domain'
  Expected document root node type #{document.root.name.inspect} to be "domain"
WARN

logger.warn(<<~WARN) and exit if document.root[:type] != 'kvm'
  Expected domain type #{document.root[:type].inspect} to be "kvm"
WARN

# From https://libvirt.org/formatdomain.html#elementsHostDev:
#
#   PCI devices can only be described by their address.
#
# From https://libvirt.org/formatdomain.html#elementsAddress:
#
#   PCI addresses have the following additional attributes: domain (a 2-byte hex
#   integer, not currently used by qemu), bus (a hex value between 0 and 0xff,
#   inclusive), slot (a hex value between 0x0 and 0x1f, inclusive), and function
#   (a value between 0 and 7, inclusive).

search = '/domain/devices/hostdev[@type="pci"]/source/address'
device_list = document.xpath(search).map do |result|
  '%04x:%02x:%02x.%x' % (
    %i[domain bus slot function].map { |x| Integer(result[x]) }
  )
end

device_list.each do |device|
  device_path = File.join('/sys/bus/pci/devices', device)

  # If the device is absent then disregard it here to let QEMU deal with the
  # situation. On the chance that the device is configured to be optional in
  # some manner then we shouldn't fail here.
  logger.warn(<<~WARN) and next unless File.directory?(device_path)
    No PCI device #{device} available at /sys/bus/pci/devices/#{device}
  WARN

  driver_path = File.join(device_path, 'driver')

  if File.symlink?(driver_path)
    driver_name = File.basename(File.readlink(driver_path))
  else
    driver_name = nil
  end

  if !driver_name.nil? && driver_name != 'vfio-pci'
    fail <<~FAIL unless File.directory?(driver_path)
      Expected device driver at #{driver_path} to be a directory
    FAIL

    File.write(File.join(driver_path, 'unbind'), device)
    sleep(0.1)

    fail <<~FAIL if File.directory?(driver_path)
      Unable to unbind driver #{driver_name.inspect} from device #{device}
    FAIL
  end

  File.write(File.join(device_path, 'driver_override'), 'vfio-pci')
  File.write('/sys/bus/pci/drivers_probe', device)
  sleep(0.1)

  fail <<~FAIL unless File.directory?(driver_path)
    Unable to reassociate device #{device} with driver "vfio-pci"
  FAIL

  fail <<~FAIL unless File.symlink?(driver_path)
    Driver of device #{device} isn't a symlink
  FAIL

  driver_name = File.basename(File.readlink(driver_path))
  fail <<~FAIL if driver_name != 'vfio-pci'
    Expected device #{device}'s driver #{driver_name.inspect} to be "vfio-pci"
  FAIL
end
