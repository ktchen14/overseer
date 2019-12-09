#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <regex.h>

#include <libudev.h>

#include "device.h"
#include "status.h"

/**
 * The POSIX extended regular expression used to deserialize a PCI symbol
 *
 * The PCI symbol serialization is @c "PCI-" followed by the @c PCI_SLOT_NAME.
 * Thus this is also used to generate a symbol from a udev device's
 * @c PCI_SLOT_NAME by extracting the domain, bus, slot, and function numbers.
 *
 * Each bus can host up to 32 devices, and a PCI device can have up to eight
 * functions. In more technical terms, a device's location is specified by a
 * 16-bit domain number, an 8-bit bus number, a 5-bit device number and a 3-bit
 * function number.
 */
#define PCI_SYMBOL_REGEXP "^" \
  "([[:xdigit:]]{4}):"    /* PCI domain number (0000 - ffff) */ \
  "([[:xdigit:]]{2}):"    /* PCI bus number (00 - ff) */ \
  "([01][[:xdigit:]])\\." /* PCI slot number (00 - 1f) */ \
  "([0-7])" "$"           /* PCI function number (0 - 7) */

// The number of parenthesized subexpressions in PCI_SYMBOL_REGEXP
#define PCI_SYMBOL_NSUB 4

/// A POSIX regular expression object initialized from @c PCI_SYMBOL_REGEXP
static regex_t pci_symbol_regexp;

/// The POSIX extended regular expression used to deserialize a USB symbol
#define USB_SYMBOL_REGEXP "^" \
  "([01][[:digit:]]{2}|2[0-4][[:digit:]]|25[0-5]):" /* USB busnum (0 - 255) */ \
  "([01][[:digit:]]{2}|2[0-4][[:digit:]]|25[0-5])$" /* USB devnum (0 - 255) */

// The number of parenthesized subexpressions in USB_SYMBOL_REGEXP
#define USB_SYMBOL_NSUB 2

/// A POSIX regular expression object initialized from @c USB_SYMBOL_REGEXP
static regex_t usb_symbol_regexp;

/**
 * Generate the @a symbol from the actual udev @a device. On failure this will
 * log to @c stderr and return @c -1. If the @a device's subsystem isn't @c pci
 * then the behavior is undefined.
 */
static int
device_to_symbol_pci(struct udev_device *device, vs_symbol_t *symbol)
  __attribute__((nonnull));

/**
 * Generate the @a symbol from the actual udev @a device. On failure this will
 * log to @c stderr and return @c -1. If the @a device's subsystem isn't @c usb
 * then the behavior is undefined.
 */
static int
device_to_symbol_usb(struct udev_device *device, vs_symbol_t *symbol)
  __attribute__((nonnull));

/**
 * Generate a manifest to attach the PCI device. Note that if the @a device
 * isn't a PCI device then the behavior is undefined. The returned manifest must
 * be free()ed by the caller.
 */
static char *device_manifest_pci(const vs_device_t *device)
  __attribute__((malloc, nonnull));

/**
 * Generate a manifest to attach the USB device. Note that if the @a device
 * isn't a USB device then the behavior is undefined. The returned manifest must
 * be free()ed by the caller.
 */
static char *device_manifest_usb(const vs_device_t *device)
  __attribute__((malloc, nonnull));

/**
 * Generate a manifest to detach the PCI symbol. Note that if the @a symbol's
 * subsystem isn't PCI then the behavior is undefined. The returned manifest
 * must be free()ed by the caller.
 */
char *symbol_manifest_pci(const vs_symbol_t *symbol)
  __attribute__((malloc, nonnull));

/**
 * Generate a manifest to detach the USB symbol. Note that if the @a symbol's
 * subsystem isn't USB then the behavior is undefined. The returned manifest
 * must be free()ed by the caller.
 */
char *symbol_manifest_usb(const vs_symbol_t *symbol)
  __attribute__((malloc, nonnull));

/**
 * Serialize the PCI @a symbol to the @a buffer as a null terminated string. The
 * @a buffer's size must be at least @c VS_SYMBOL_BUFFER_SIZE. If the
 * @a symbol's subsystem isn't @c VS_SUBSYSTEM_PCI then the behavior is
 * undefined.
 */
void symbol_dump_pci(const vs_symbol_t *symbol, char *buffer)
  __attribute__((nonnull));

/**
 * Serialize the USB @a symbol to the @a buffer as a null terminated string. The
 * @a buffer's size must be at least @c VS_SYMBOL_BUFFER_SIZE. If the
 * @a symbol's subsystem isn't @c VS_SUBSYSTEM_USB then the behavior is
 * undefined.
 */
void symbol_dump_usb(const vs_symbol_t *symbol, char *buffer)
  __attribute__((nonnull));

/**
 * Deserialize and load the PCI @a symbol from the null terminated string in the
 * @a text. On failure this will log to @c stderr and return @c -1. The
 * @a text must not include the @c "PCI-" prefix. If the @a text wasn't dumped
 * from a PCI symbol then this will log to @c stderr and return @c -1.
 */
int symbol_load_pci(vs_symbol_t *symbol, const char *text)
  __attribute__((nonnull));

/**
 * Deserialize and load the USB @a symbol from the null terminated string in the
 * @a text. On failure this will log to @c stderr and return @c -1. The
 * @a text must not include the @c "USB-" prefix. If the @a text wasn't dumped
 * from a USB symbol then this will log to @c stderr and return @c -1.
 */
int symbol_load_usb(vs_symbol_t *symbol, const char *text)
  __attribute__((nonnull));

/// Initialize @c pci_symboL_regexp and @c usb_symbol_regexp. On failure this
/// will exit().
static void regexp_init(void) __attribute__((constructor));

/// Free @a pci_symbol_regexp and @c usb_symbol_regexp
static void regexp_raze(void) __attribute__((destructor));

int vs_device_assign(vs_device_t *device, struct udev_device *actual) {
  if (device->actual != NULL) {
    fprintf(stderr,
        "Assignment to device \"%s\" with assigned udev device \"%s\"\n",
        device->name, udev_device_get_syspath(actual));
    device->actual = udev_device_unref(device->actual);
  }

  const char *name = udev_device_get_property_value(actual, "VISION_NAME");
  assert(name != NULL);
  assert(!strcmp(device->name, name));

  const char *subsystem;

  if ((subsystem = udev_device_get_subsystem(actual)) == NULL)
    vs_except(subsystem, "udev_device_get_subsystem(\"%s\"): %s\n",
        udev_device_get_syspath(actual), strerror(errno));

  if (!strcmp(subsystem, "pci")) {
    if (device_to_symbol_pci(actual, &device->symbol) == -1)
      return -1;
  } else if (!strcmp(subsystem, "usb")) {
    if (device_to_symbol_usb(actual, &device->symbol) == -1)
      return -1;
  } else
    vs_except(subsystem, "Can't handle subsystem \"%s\"\n", subsystem);

  device->actual = udev_device_ref(actual);

  return 0;

except_subsystem:
  return -1;
}

void vs_device_unassign(vs_device_t *device) {
  if (device->actual == NULL) {
    fprintf(stderr,
        "Unassignment on device \"%s\" with no actual udev device\n",
        device->name);
    return;
  }
  device->actual = udev_device_unref(device->actual);
}

int vs_device_update(vs_device_t *device, struct udev_device *actual) {
  if (device->actual == NULL)
    fprintf(stderr,
        "Update on device \"%s\" with no actual udev device\n",
        device->name);
  else
    vs_device_unassign(device);

  return vs_device_assign(device, actual);
}

char *vs_device_manifest(const vs_device_t *device) {
  assert(device->actual != NULL);

  if (device->symbol.subsystem == VS_SUBSYSTEM_PCI)
    return device_manifest_pci(device);

  if (device->symbol.subsystem == VS_SUBSYSTEM_USB)
    return device_manifest_usb(device);

  abort(); // Unreachable
}

bool vs_device_in_view(const vs_device_t *device, const char *view) {
  if (device->view_list == NULL || view == NULL)
    return false;

  for (size_t i = 0; device->view_list[i] != NULL; i++) {
    if (!strcmp(view, device->view_list[i]))
      return true;
  }

  return false;
}

char *vs_symbol_manifest(const vs_symbol_t *symbol) {
  if (symbol->subsystem == VS_SUBSYSTEM_PCI)
    return symbol_manifest_pci(symbol);

  if (symbol->subsystem == VS_SUBSYSTEM_USB)
    return symbol_manifest_usb(symbol);

  abort(); // Unreachable
}

bool vs_symbol_eq(const vs_symbol_t *a, const vs_symbol_t *b) {
  if (a->subsystem != b->subsystem)
    return false;

  if (a->subsystem == VS_SUBSYSTEM_PCI) {
    if (a->pci.domain != b->pci.domain)
      return false;
    if (a->pci.bus != b->pci.bus)
      return false;
    if (a->pci.slot != b->pci.slot)
      return false;
    return a->pci.function == b->pci.function;
  }

  if (a->subsystem == VS_SUBSYSTEM_USB)
    return a->usb.busnum == b->usb.busnum && a->usb.devnum == b->usb.devnum;

  abort(); // Unreachable
}

void vs_symbol_dump(const vs_symbol_t *symbol, char *buffer) {
  if (symbol->subsystem == VS_SUBSYSTEM_PCI)
    symbol_dump_pci(symbol, buffer);
  else if (symbol->subsystem == VS_SUBSYSTEM_USB)
    symbol_dump_usb(symbol, buffer);
  else
    abort(); // Unreachable
}

int vs_symbol_load(vs_symbol_t *symbol, const char *buffer) {
  if (!strncmp(buffer, "PCI-", strlen("PCI-")))
    return symbol_load_pci(symbol, buffer + strlen("PCI-"));
  else if (!strncmp(buffer, "USB-", strlen("USB-")))
    return symbol_load_usb(symbol, buffer + strlen("USB-"));

  vs_return(-1, "Can't load symbol from text \"%s\"\n", buffer);
}

int device_to_symbol_pci(struct udev_device *device, vs_symbol_t *symbol) {
  const char *subsystem = udev_device_get_subsystem(device);
  assert(subsystem != NULL);
  assert(!strcmp(subsystem, "pci"));

  symbol->subsystem = VS_SUBSYSTEM_PCI;

  // Get the PCI_SLOT_NAME of the device
  const char *slot_name;
  slot_name = udev_device_get_property_value(device, "PCI_SLOT_NAME");
  if (slot_name == NULL)
    vs_except(slot_name,
        "udev_device_get_property_value(\"%s\", \"PCI_SLOT_NAME\"): %s\n",
        udev_device_get_syspath(device), strerror(errno));

  // Execute the regular expression against the slot name
  regmatch_t result[PCI_SYMBOL_NSUB + 1];
  int e;
  e = regexec(&pci_symbol_regexp, slot_name, PCI_SYMBOL_NSUB + 1, result, 0);
  if (e != 0) {
    size_t length = regerror(e, &pci_symbol_regexp, NULL, 0);
    char buffer[length];
    regerror(e, &pci_symbol_regexp, buffer, length);
    vs_except(regexec, "regexec(&pci_symbol_regexp, \"%s\"): %s\n",
        slot_name, buffer);
  }

  // We've already validated each number in the slot name through the regular
  // expression. Each number in the slot name is followed by either a NUL or a
  // nondigit delimiter.
  symbol->pci.domain = strtoul(slot_name + result[1].rm_so, NULL, 16);
  symbol->pci.bus = strtoul(slot_name + result[2].rm_so, NULL, 16);
  symbol->pci.slot = strtoul(slot_name + result[3].rm_so, NULL, 16);
  symbol->pci.function = strtoul(slot_name + result[4].rm_so, NULL, 16);

  return 0;

except_regexec:
except_slot_name:
  return -1;
}

int device_to_symbol_usb(struct udev_device *device, vs_symbol_t *symbol) {
  const char *subsystem = udev_device_get_subsystem(device);
  assert(subsystem != NULL);
  assert(!strcmp(subsystem, "usb"));

  symbol->subsystem = VS_SUBSYSTEM_USB;

  unsigned long number;

  // Get the USB bus number (BUSNUM) of the device
  const char *busnum;
  if ((busnum = udev_device_get_property_value(device, "BUSNUM")) == NULL)
    vs_except(number,
        "udev_device_get_property_value(\"%s\", \"BUSNUM\"): %s\n",
        udev_device_get_syspath(device), strerror(errno));
  if (isspace(*busnum) || strchr("+-", *busnum) != NULL)
    vs_except(number, "busnum(\"%s\"): %s\n", busnum, strerror(EINVAL));

  // Read busnum as an unsigned char
  char *string_left;
  errno = 0;
  number = strtoul(busnum, &string_left, 10);
  if (*string_left != '\0')
    vs_except(number, "busnum(\"%s\"): %s\n", busnum, strerror(EINVAL));
  if (number == ULONG_MAX && errno == ERANGE)
    vs_except(number, "busnum(\"%s\"): %s\n", busnum, strerror(errno));
  if (number > UCHAR_MAX)
    vs_except(number, "busnum(\"%s\"): %s\n", busnum, strerror(ERANGE));
  symbol->usb.busnum = number;

  // Get the USB device number (DEVNUM) of the device
  const char *devnum;
  if ((devnum = udev_device_get_property_value(device, "DEVNUM")) == NULL)
    vs_except(number,
        "udev_device_get_property_value(\"%s\", \"DEVNUM\"): %s\n",
        udev_device_get_syspath(device), strerror(errno));
  if (isspace(*devnum) || strchr("+-", *devnum) != NULL)
    vs_except(number, "devnum(\"%s\"): %s\n", devnum, strerror(EINVAL));

  // Read devnum as an unsigned short
  errno = 0;
  number = strtoul(devnum, &string_left, 10);
  if (*string_left != '\0')
    vs_except(number, "devnum(\"%s\"): %s\n", devnum, strerror(EINVAL));
  if (number == ULONG_MAX && errno == ERANGE)
    vs_except(number, "devnum(\"%s\"): %s\n", devnum, strerror(errno));
  if (number > UCHAR_MAX)
    vs_except(number, "devnum(\"%s\"): %s\n", devnum, strerror(ERANGE));
  symbol->usb.devnum = number;

  return 0;

except_number:
  return -1;
}

char *device_manifest_pci(const vs_device_t *device) {
  assert(device->actual != NULL);
  assert(device->symbol.subsystem == VS_SUBSYSTEM_PCI);

  if (device->xtra == NULL)
    return symbol_manifest_pci(&device->symbol);

  // Allocate a buffer and generate the device manifest
  char *manifest;
  int e = asprintf(&manifest,
      "<hostdev mode=\"subsystem\" type=\"pci\" managed=\"yes\">\n"
      "  <source>\n"
      "    <address domain=\"0x%04" PRIx16 "\""
                  " bus=\"0x%02" PRIx8 "\""
                  " slot=\"0x%02" PRIx8 "\""
                  " function=\"0x%" PRIu8 "\" />\n"
      "  </source>\n"
      "  %s\n"
      "</hostdev>\n",
      device->symbol.pci.domain,
      device->symbol.pci.bus,
      device->symbol.pci.slot,
      device->symbol.pci.function,
      device->xtra);
  if (e == -1)
    vs_return(NULL, "asprintf(): %s\n", strerror(errno));

  return manifest;
}

char *device_manifest_usb(const vs_device_t *device) {
  assert(device->actual != NULL);
  assert(device->symbol.subsystem == VS_SUBSYSTEM_USB);

  if (device->xtra == NULL)
    return symbol_manifest_usb(&device->symbol);

  // Allocate a buffer and generate the device manifest
  char *manifest;
  int e = asprintf(&manifest,
      "<hostdev mode=\"subsystem\" type=\"usb\" managed=\"yes\">\n"
      "  <source>\n"
      "    <address bus=\"%hhu\" device=\"%hhu\" />\n"
      "  </source>\n"
      "  %s\n"
      "</hostdev>\n",
      device->symbol.usb.busnum, device->symbol.usb.devnum,
      device->xtra);
  if (e == -1)
    vs_return(NULL, "asprintf(): %s\n", strerror(errno));

  return manifest;
}

char *symbol_manifest_pci(const vs_symbol_t *symbol) {
  assert(symbol->subsystem == VS_SUBSYSTEM_PCI);

  char *manifest;
  int e = asprintf(&manifest,
      "<hostdev mode=\"subsystem\" type=\"pci\" managed=\"yes\">\n"
      "  <source>\n"
      "    <address domain=\"0x%04" PRIx16 "\""
                  " bus=\"0x%02" PRIx8 "\""
                  " slot=\"0x%02" PRIx8 "\""
                  " function=\"0x%" PRIu8 "\" />\n"
      "  </source>\n"
      "</hostdev>\n",
      symbol->pci.domain,
      symbol->pci.bus,
      symbol->pci.slot,
      symbol->pci.function);
  if (e == -1)
    vs_return(NULL, "asprintf(): %s\n", strerror(errno));

  return manifest;
}

char *symbol_manifest_usb(const vs_symbol_t *symbol) {
  assert(symbol->subsystem == VS_SUBSYSTEM_USB);

  char *manifest;
  int e = asprintf(&manifest,
      "<hostdev mode=\"subsystem\" type=\"usb\" managed=\"yes\">\n"
      "  <source>\n"
      "    <address bus=\"%hhu\" device=\"%hhu\" />\n"
      "  </source>\n"
      "</hostdev>\n",
      symbol->usb.busnum, symbol->usb.devnum);
  if (e == -1)
    vs_return(NULL, "asprintf(): %s\n", strerror(errno));

  return manifest;
}

void symbol_dump_pci(const vs_symbol_t *symbol, char *buffer) {
  assert(symbol->subsystem == VS_SUBSYSTEM_PCI);
  sprintf(buffer, "PCI-%04x:%02x:%02x.%d",
      symbol->pci.domain,
      symbol->pci.bus,
      symbol->pci.slot,
      symbol->pci.function);
}

void symbol_dump_usb(const vs_symbol_t *symbol, char *buffer) {
  assert(symbol->subsystem == VS_SUBSYSTEM_USB);
  sprintf(buffer, "USB-%03d:%03d", symbol->usb.busnum, symbol->usb.devnum);
}

int symbol_load_pci(vs_symbol_t *symbol, const char *text) {
  symbol->subsystem = VS_SUBSYSTEM_PCI;

  // Execute the regular expression against the text
  regmatch_t result[PCI_SYMBOL_NSUB + 1];
  int e = regexec(&pci_symbol_regexp, text, PCI_SYMBOL_NSUB + 1, result, 0);
  if (e != 0) {
    size_t length = regerror(e, &pci_symbol_regexp, NULL, 0);
    char buffer[length];
    regerror(e, &pci_symbol_regexp, buffer, length);
    vs_return(-1, "regexec(&pci_symbol_regexp, \"%s\"): %s\n", text, buffer);
  }

  // We've already validated each number through the regular expression. Each
  // number in the text is followed by either a NUL or a nondigit delimiter.
  symbol->pci.domain = strtoul(text + result[1].rm_so, NULL, 16);
  symbol->pci.bus = strtoul(text + result[2].rm_so, NULL, 16);
  symbol->pci.slot = strtoul(text + result[3].rm_so, NULL, 16);
  symbol->pci.function = strtoul(text + result[4].rm_so, NULL, 16);

  return 0;
}

int symbol_load_usb(vs_symbol_t *symbol, const char *text) {
  symbol->subsystem = VS_SUBSYSTEM_USB;

  // Execute the regular expression against the text
  regmatch_t result[USB_SYMBOL_NSUB + 1];
  int e = regexec(&usb_symbol_regexp, text, USB_SYMBOL_NSUB + 1, result, 0);
  if (e != 0) {
    size_t length = regerror(e, &usb_symbol_regexp, NULL, 0);
    char buffer[length];
    regerror(e, &usb_symbol_regexp, buffer, length);
    vs_return(-1, "regexec(&usb_symbol_regexp, \"%s\"): %s\n", text, buffer);
  }

  // We've already validated each number through the regular expression. Each
  // number in the text is followed by either a NUL or a nondigit delimiter.
  symbol->usb.busnum = strtoul(text + result[1].rm_so, NULL, 10);
  symbol->usb.devnum = strtoul(text + result[2].rm_so, NULL, 10);

  return 0;
}

static void regexp_init() {
  int e;

  // Compile PCI_SYMBOL_REGEXP as a POSIX extended regular expression
  e = regcomp(&pci_symbol_regexp, PCI_SYMBOL_REGEXP, REG_EXTENDED);
  if (e != 0) {
    size_t length = regerror(e, &pci_symbol_regexp, NULL, 0);
    char buffer[length];
    regerror(e, &pci_symbol_regexp, buffer, length);
    fprintf(stderr, "regcomp(PCI_SYMBOL_REGEXP): %s\n", buffer);
    exit(1);
  }
  assert(pci_symbol_regexp.re_nsub == PCI_SYMBOL_NSUB);

  // Compile USB_SYMBOL_REGEXP as a POSIX extended regular expression
  e = regcomp(&usb_symbol_regexp, USB_SYMBOL_REGEXP, REG_EXTENDED);
  if (e != 0) {
    size_t length = regerror(e, &usb_symbol_regexp, NULL, 0);
    char buffer[length];
    regerror(e, &usb_symbol_regexp, buffer, length);
    fprintf(stderr, "regcomp(USB_SYMBOL_REGEXP): %s\n", buffer);
    exit(1);
  }
  assert(usb_symbol_regexp.re_nsub == USB_SYMBOL_NSUB);
}

static void regexp_raze() {
  regfree(&pci_symbol_regexp);
  regfree(&usb_symbol_regexp);
}
