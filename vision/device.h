#ifndef VS_DEVICE_H
#define VS_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#define VS_SYMBOL_BUFFER_SIZE sizeof("PCI-0000:00:00.0")

/**
 * A handle that uniquely identifies a host device to libvirt.
 *
 * A symbol should be serializable and deserializable absent further information
 * from udev. Symbols that are equivalent must represent the same underlying
 * host device (at a given time), regardless of whether they're generated from a
 * udev device or a serialization.
 */
typedef struct vs_symbol_t {
  /// The subsystem of this symbol's device
  enum {
    VS_SUBSYSTEM_PCI,
    VS_SUBSYSTEM_USB,
  } subsystem;

  union {
    /// PCI device information when @a subsystem is @c VS_SUBSYSTEM_PCI
    struct {
      uint16_t domain;        ///< The PCI domain number (0000 - ffff)
      uint8_t bus;            ///< The PCI bus number (00 - ff)
      uint8_t slot     : 5;   ///< The PCI slot number (00 - 1f)
      uint8_t function : 3;   ///< The PCI function number (0 - 7)
    } pci;

    /// USB device information when @a subsystem is @c VS_SUBSYSTEM_USB
    struct {
      unsigned char busnum;   ///< The USB bus number
      unsigned char devnum;   ///< The USB device number
    } usb;
  };
} vs_symbol_t;

/**
 * A device defined in the vision system
 *
 * A vision device doesn't have to exist on the system. It's associated with an
 * actual udev device through a unique name.
 */
typedef struct vs_device_t {
  /// The device's unique name in the vision system
  const char *name;

  /**
   * Additional text to add to the device manifest when the device is attached
   *
   * This text is added to the constructed libvirt @c hostdev element. It must
   * be either @c NULL a well formed XML string.
   */
  const char *xtra;

  /// The actual udev device assigned to the vision device. If this is @c NULL
  /// the no actual device is assigned to the vision device.
  struct udev_device *actual;

  /// The symbol used to attach the device to a libvirt domain. This is unusable
  /// unless an actual udev device is assigned to the vision device.
  vs_symbol_t symbol;

  /// The action scheduled on the device for a domain (in initialize_domain())
  enum {
    VS_DEVICE_NONE,
    VS_DEVICE_DETACH,
    VS_DEVICE_KEEP,
    VS_DEVICE_ATTACH,
  } action;

  const char *view_list[];
} vs_device_t;

/// The global device list
extern vs_device_t *vs_device_list[];

/**
 * Assign the @a actual udev device to the vision @a device
 *
 * This will also generate and set the @a device's symbol; if that fails then
 * this will log to @c stderr and return @c -1. If the @a device's name is
 * different from the @a actual udev device's @c VISION_NAME attribute then the
 * behavior is undefined.
 *
 * On success this will call udev_device_ref() on @a actual.
 */
int vs_device_assign(vs_device_t *device, struct udev_device *actual)
  __attribute__((nonnull));

/// Unassign the @a device's udev device
void vs_device_unassign(vs_device_t *device);

/**
 * Change the @a device's actual device to @a actual. This does (in effect) a
 * vs_device_unassign() with a vs_device_assign(). On failure the @a device's
 * actual device is unassigned and this will log to @c stderr and return @c -1.
 */
int vs_device_update(vs_device_t *device, struct udev_device *actual);

/**
 * Return a manifest used to attach the @a device to a libvirt domain
 *
 * The returned manifest is a string of an XML document with a libvirt
 * @a hostdev element as the root node. It should be free()ed by the caller. On
 * failure this will log to @c stderr and return @c NULL.
 */
char *vs_device_manifest(const vs_device_t *device)
  __attribute__((malloc, nonnull));

/// Return whether the @a view is in the @a device's view list. If @a view is
/// @c NULL then return @c false.
bool vs_device_in_view(const vs_device_t *device, const char *view)
  __attribute__((nonnull(1)));

/**
 * Return a manifest used to detach the @a symbol from a libvirt domain
 *
 * The returned manifest is a string of an XML document with a libvirt
 * @a hostdev element as the root node. It should be free()ed by the caller. On
 * failure this will log to @c stderr and return @c NULL.
 *
 * To attach a device use vs_device_manifest() rather than this function. The
 * manifest returned here doesn't have the device's @c xtra (if configured).
 */
char *vs_symbol_manifest(const vs_symbol_t *symbol)
  __attribute__((malloc, nonnull));

/// Return whether the symbols @a a and @a b represent the same host device
bool vs_symbol_eq(const vs_symbol_t *a, const vs_symbol_t *b)
  __attribute__((nonnull));

/// Serialize the @a symbol to the @a buffer as a null terminated string. The
/// @a buffer's size must be at least @c VS_SYMBOL_BUFFER_SIZE.
void vs_symbol_dump(const vs_symbol_t *symbol, char *buffer)
  __attribute__((nonnull));

/// Deserialize and load the @a symbol from the null terminated string in the
/// @a buffer. On failure this will log to @c stderr and return @c -1.
int vs_symbol_load(vs_symbol_t *symbol, const char *buffer)
  __attribute__((nonnull));

#endif /* VS_DEVICE_H */
