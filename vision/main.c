#define _GNU_SOURCE

#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgen.h>
#include <signal.h>
#include <poll.h>

#include <libudev.h>
#include <libvirt/libvirt.h>
#include <libxml/xpath.h>

#include "device.h"
#include "status.h"

#define USAGE \
"Usage: %s {attach,detach} DEVICE\n" \
"\n" \
"Attach or detach DEVICE to/from the vision system. DEVICE must be the syspath\n" \
"of a udev initialized device.\n"

struct udev_monitor *initialize_device_list(struct udev *udev);

int initialize_domain(virDomainPtr domain, unsigned int option);

int on_detect(struct udev_device *actual);
bool on_remove(struct udev_device *device);

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  // Initialization

  struct udev *udev;
  if ((udev = udev_new()) == NULL)
    vs_except(udev_new, "udev_new(): %s\n", strerror(errno));

  struct udev_monitor *monitor;
  if ((monitor = initialize_device_list(udev)) == NULL)
    goto except_initialize_device_list;

  virConnectPtr virt;
  if ((virt = virConnectOpen("qemu:///system")) == NULL)
    goto except_open;

  int e;

  virDomainPtr *domain_list;
  if ((e = virConnectListAllDomains(virt, &domain_list, 0)) == -1)
    goto except_domain_list;
  size_t domain_list_length = e;

  // Loop through each domain
  for (size_t i = 0; i < domain_list_length; i++) {
    virDomainPtr domain = domain_list[i];

    fprintf(stderr, "Initialization of domain \"%s\"\n",
        virDomainGetName(domain));

    initialize_domain(domain, VIR_DOMAIN_AFFECT_CURRENT);

    if (virDomainIsActive(domain) != 1)
      continue;

    initialize_domain(domain, VIR_DOMAIN_AFFECT_CONFIG);
  }

  /* for (size_t i = 0; i < domain_list_length; i++) */
  /*   virDomainFree(domain_list[i]); */
  /* free(domain_list); */

  struct pollfd pollfd = {
    .fd = udev_monitor_get_fd(monitor), .events = POLLIN,
  };

  while ((e = poll(&pollfd, 1, -1)) > 0) {
    if (pollfd.revents & POLLERR || pollfd.revents & POLLNVAL)
      vs_except(poll, "Can't resume poll() on udev monitor fd\n");

    struct udev_device *actual;
    if ((actual = udev_monitor_receive_device(monitor)) == NULL)
      vs_except(poll, "udev_monitor_receive_device(monitor): %s\n",
          strerror(errno));
    const char *action = udev_device_get_action(actual);

    if (!strcmp(action, "add")) {
      if (!udev_device_has_tag(actual, "vision"))
        goto skip;
      on_detect(actual);
      for (size_t i = 0; i < domain_list_length; i++) {
        virDomainPtr domain = domain_list[i];
        initialize_domain(domain, VIR_DOMAIN_AFFECT_CURRENT);

        if (virDomainIsActive(domain) != 1)
          continue;

        initialize_domain(domain, VIR_DOMAIN_AFFECT_CONFIG);
      }
    } else if (!strcmp(action, "remove")) {
      if (on_remove(actual)) {
        for (size_t i = 0; i < domain_list_length; i++) {
          virDomainPtr domain = domain_list[i];
          initialize_domain(domain, VIR_DOMAIN_AFFECT_CURRENT);

          if (virDomainIsActive(domain) != 1)
            continue;

          initialize_domain(domain, VIR_DOMAIN_AFFECT_CONFIG);
        }
      }
    }

  skip:
    udev_device_unref(actual);
  }
except_poll:

  // Shutdown

  virConnectClose(virt);
  udev_monitor_unref(monitor);
  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];
    if (device->actual != NULL)
      device->actual = udev_device_unref(device->actual);
  }
  udev_unref(udev);

  return 0;

except_domain_list:
  virConnectClose(virt);

except_open:
  udev_monitor_unref(monitor);
  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];
    if (device->actual != NULL)
      device->actual = udev_device_unref(device->actual);
  }

except_initialize_device_list:
  udev_unref(udev);

except_udev_new:
  return 1;
}

int on_detect(struct udev_device *actual) {
  // Log an error to stderr if the device is tagged with "vision" but doesn't
  // have a VISION_NAME
  const char *vision_name;
  vision_name = udev_device_get_property_value(actual, "VISION_NAME");
  if (vision_name == NULL)
    vs_except(vision_name,
        "udev_device_get_property_value(\"%s\", \"VISION_NAME\"): %s\n",
        udev_device_get_syspath(actual), strerror(errno));

  fprintf(stderr,
      "Detected addition of udev device \"%s\" with VISION_NAME \"%s\"\n",
      udev_device_get_syspath(actual), vision_name);

  // Assign the udev device to each vision device with the same name
  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];

    if (strcmp(device->name, vision_name))
      continue;

    if (vs_device_assign(device, actual) == 0)
      continue;

    fprintf(stderr,
        "Can't assign device \"%s\" to vision device with name \"%s\"\n",
        udev_device_get_syspath(actual), vision_name);
  }

  return 0;

except_vision_name:
  return -1;
}

bool on_remove(struct udev_device *actual) {
  const char *syspath = udev_device_get_syspath(actual);
  bool change = false;

  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];
    if (device->actual == NULL)
      continue;

    if (strcmp(udev_device_get_syspath(device->actual), syspath))
      continue;

    fprintf(stderr,
        "Udev device \"%s\" will be unassigned from device with name \"%s\"\n",
        syspath, device->name);
    vs_device_unassign(device);
    change = true;
  }

  return change;
}

struct udev_monitor *initialize_device_list(struct udev *udev) {
  int e;

  // Enumerate each initialized device in udev with the "vision" tag
  struct udev_enumerate *enumerate;
  if ((enumerate = udev_enumerate_new(udev)) == NULL)
    vs_except(enumerate_new, "udev_enumerate_new(udev): %s\n", strerror(errno));
  if ((e = udev_enumerate_add_match_is_initialized(enumerate)) != 0)
    vs_except(enumerate_filter,
        "udev_enumerate_add_match_is_initialized(enumerate): %s\n",
        strerror(-e));
  if ((e = udev_enumerate_add_match_tag(enumerate, "vision")) != 0)
    vs_except(enumerate_filter,
        "udev_enumerate_add_match_tag(enumerate, \"vision\"): %s\n",
        strerror(-e));

  // Configure a monitor to receive events from initialized devices. Don't
  // filter by the "vision" tag here. We need to intercept both "add" and
  // "remove" actions on devices. When a device is removed it doesn't have any
  // tags.
  struct udev_monitor *monitor;
  if ((monitor = udev_monitor_new_from_netlink(udev, "udev")) == NULL)
    vs_except(monitor_new,
        "udev_monitor_new_from_netlink(udev, \"udev\"): %s\n",
        strerror(errno));

  // Enable receiving on the monitor and then scan the enumeration to ensure
  // that each device is seen
  if ((e = udev_monitor_enable_receiving(monitor)) != 0)
    vs_except(scan, "udev_monitor_enable_receiving(monitor): %s\n",
        strerror(-e));
  if ((e = udev_enumerate_scan_devices(enumerate)) != 0)
    vs_except(scan, "udev_enumerate_scan_devices(enumerate): %s\n",
        strerror(-e));

  struct udev_list_entry *list = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry *item;

  udev_list_entry_foreach(item, list) {
    const char *syspath = udev_list_entry_get_name(item);

    // Each item in the list is a syspath rather than a udev device. By the time
    // that we do the udev_device_new_from_syspath() the device may have been
    // removed from the system (through an event that we haven't seen yet). Thus
    // it's okay if we can't get the actual udev device for a list item or if
    // the device that we receive isn't a vision device.
    struct udev_device *actual;
    if ((actual = udev_device_new_from_syspath(udev, syspath)) == NULL)
      continue;
    if (!udev_device_has_tag(actual, "vision"))
      goto skip;
    if (!udev_device_get_is_initialized(actual))
      goto skip;

    on_detect(actual);

  skip:
    udev_device_unref(actual);
  }
  enumerate = udev_enumerate_unref(enumerate);

  // Next poll with a timeout of 0 to handle any events received during the
  // enumeration
  struct pollfd pollfd = {
    .fd = udev_monitor_get_fd(monitor), .events = POLLIN,
  };

  while ((e = poll(&pollfd, 1, 0)) > 0) {
    if (pollfd.revents & POLLERR || pollfd.revents & POLLNVAL)
      vs_except(poll, "Can't resume poll() on udev monitor fd\n");

    struct udev_device *actual;
    if ((actual = udev_monitor_receive_device(monitor)) == NULL)
      vs_except(poll, "udev_monitor_receive_device(monitor): %s\n",
          strerror(errno));
    const char *action = udev_device_get_action(actual);

    if (!strcmp(action, "add")) {
      if (udev_device_has_tag(actual, "vision"))
        on_detect(actual);
    } else if (!strcmp(action, "remove")) {
      on_remove(actual);
    }

    udev_device_unref(actual);
  }

  if (e == -1)
    vs_except(poll, "poll(): %s\n", strerror(errno));

  return monitor;

except_poll:
  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];
    if (device->actual != NULL)
      device->actual = udev_device_unref(device->actual);
  }

except_scan:
  udev_monitor_unref(monitor);

except_monitor_new:
except_enumerate_filter:
  if (enumerate != NULL) udev_enumerate_unref(enumerate);

except_enumerate_new:
  return NULL;
}

int initialize_domain(virDomainPtr domain, unsigned int option) {
  char *metadata = virDomainGetMetadata(domain,
      VIR_DOMAIN_METADATA_ELEMENT,
      "http://github.com/ktchen14/overseer/vision",
      option);

  // This isn't a vision managed domain
  if (metadata == NULL)
    return 0;

  // Create an XML document from the domain's vision metadata. XML isn't used as
  // markup here so skip blanks and reduce CDATAs. Use the domain name itself as
  // the URI (it doesn't seem to matter and we don't have a better option).
  xmlDocPtr document = xmlReadDoc(BAD_CAST metadata, virDomainGetName(domain),
      NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA);
  if (document == NULL)
    vs_except(document,
        "Can't load XML document from vision metadata of domain \"%s\"\n",
        virDomainGetName(domain));

  xmlNodePtr root;
  if ((root = xmlDocGetRootElement(document)) == NULL)
    vs_except(root, "No root element in vision metadata of domain \"%s\"\n",
        virDomainGetName(domain));

  // It's okay if no view is set on the domain. In this case we should detach
  // all vision managed devices from the domain.
  char *view = (char *) xmlGetProp(root, BAD_CAST "view");

  if (view != NULL)
    fprintf(stderr, "Domain \"%s\" is configured with view \"%s\"\n",
        virDomainGetName(domain), view);
  else
    fprintf(stderr, "Domain \"%s\" is configured with no view \n",
        virDomainGetName(domain));

  // This shouldn't fail even if no <device> elements are in the metadata
  xmlXPathContextPtr ctxt = xmlXPathNewContext(document);
  xmlXPathObjectPtr result;
  if ((result = xmlXPathEval(BAD_CAST "/vision/device", ctxt)) == NULL)
    vs_except(eval,
        "Can't read device list from vision metadata of domain \"%s\"\n",
        virDomainGetName(domain));
  if (result->type != XPATH_NODESET)
    vs_except(result,
        "Can't read device list from vision metadata of domain \"%s\"\n",
        virDomainGetName(domain));

  bool update_metadata = false; // Should the domain's metadata be updated?

  for (size_t i = 0; vs_device_list[i] != NULL; i++)
    vs_device_list[i]->action = VS_DEVICE_NONE;

  // Loop through each vision managed device in the domain's metadata
  for (int i = 0; i < xmlXPathNodeSetGetLength(result->nodesetval); i++) {
    xmlNodePtr device_node = xmlXPathNodeSetItem(result->nodesetval, i);
    if (device_node->type != XML_ELEMENT_NODE)
      continue;

    // Load the symbol for each device. If we can't then remove the device
    // element.
    char *symbol_text = (char *) xmlGetProp(device_node, BAD_CAST "symbol");
    if (symbol_text == NULL)
      vs_except(symbol_text,
          "Malformed <device> element in vision metadata of domain \"%s\"\n",
          virDomainGetName(domain));

    fprintf(stderr, "Attachment #%d on domain \"%s\" is \"%s\"\n",
        i, virDomainGetName(domain), symbol_text);

    vs_symbol_t symbol;
    if (vs_symbol_load(&symbol, symbol_text) == -1)
      vs_except(symbol,
          "Malformed <device> element in vision metadata of domain \"%s\"\n",
          virDomainGetName(domain));

    bool detach = true;

    // Loop through each vision device to determine if this device should be
    // detached from the domain
    for (size_t j = 0; vs_device_list[j] != NULL; j++) {
      vs_device_t *device = vs_device_list[j];

      if (device->actual == NULL || !vs_symbol_eq(&device->symbol, &symbol))
        continue;

      fprintf(stderr, "Attachment \"%s\" is device \"%s\"\n",
          symbol_text, device->name);

      // Don't detach this device if a vision device is active in the view
      if (vs_device_in_view(device, view)) {
        fprintf(stderr, "Device \"%s\" is active in view \"%s\"\n",
            device->name, view);
        device->action = VS_DEVICE_KEEP;
        detach = false;
      } else {
        fprintf(stderr, "Device \"%s\" is inactive in view \"%s\"\n",
            device->name, view);
        device->action = VS_DEVICE_DETACH;
      }
    }

    // This device shouldn't be detached. Move on to the next symbol.
    if (!detach) {
      xmlFree(symbol_text);
      continue;
    }

    fprintf(stderr, "Attachment \"%s\" will be detached from domain \"%s\"\n",
        symbol_text, virDomainGetName(domain));

    // Detach the device. Do a detach-device in libvirt and then remove it
    // from the domain's vision metadata.

    char *manifest;
    if ((manifest = vs_symbol_manifest(&symbol)) == NULL)
      goto except_manifest;
    virDomainDetachDeviceFlags(domain, manifest, option);
    free(manifest);

  except_manifest:
  except_symbol:
    xmlFree(symbol_text);

  except_symbol_text:
    xmlUnlinkNode(device_node);
    // TODO: The device node must be free()ed sometime. Can't do it here because
    // then the xmlXPathFreeObject(result) will fail.
    update_metadata = true;
  }

  // Loop through each vision device to determine if this device should be
  // attached to the domain
  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];

    if (device->actual == NULL)
      continue;

    // If this device is kept (from detachment) then don't append a duplicate
    // node to the vision metadata
    if (device->action == VS_DEVICE_KEEP)
      continue;

    if (vs_device_in_view(device, view)) {
      fprintf(stderr, "Device \"%s\" is active in view \"%s\"\n",
          device->name, view);
    } else {
      fprintf(stderr, "Device \"%s\" is inactive in view \"%s\"\n",
          device->name, view);
      continue;
    }

    fprintf(stderr, "Device \"%s\" will be attached to domain \"%s\"\n",
        device->name, virDomainGetName(domain));

    device->action = VS_DEVICE_ATTACH;

    xmlNodePtr device_node;
    if ((device_node = xmlNewNode(NULL, BAD_CAST "device")) == NULL)
      vs_continue("Can't create XML <device> node\n");
    char buffer[VS_SYMBOL_BUFFER_SIZE];
    vs_symbol_dump(&device->symbol, buffer);
    if (xmlSetProp(device_node, BAD_CAST "symbol", BAD_CAST buffer) == NULL)
      vs_except(attr, "Can't set \"symbol\" attribute to \"%s\"\n", buffer);
    if (xmlAddChild(root, device_node) == NULL)
      vs_except(attr, "Failed to add device node to vision metadata root\n");
    update_metadata = true;

    continue;

  except_attr:
    xmlFreeNode(device_node);
  }

  // The metadata has changed (from either detachment or attachment). Update it
  // on the domain *after* all detachments and *before* all attachments.
  if (update_metadata) {
    xmlChar *update;
    int length;
    xmlDocDumpFormatMemory(document, &update, &length, 1);

    fprintf(stderr, "Metadata in domain \"%s\" will be updated to:\n%s",
        virDomainGetName(domain), update);

    virDomainSetMetadata(domain,
        VIR_DOMAIN_METADATA_ELEMENT,
        (char *) update,
        "vision", "http://github.com/ktchen14/overseer/vision",
        option);
    free(update);
  }

  for (size_t i = 0; vs_device_list[i] != NULL; i++) {
    vs_device_t *device = vs_device_list[i];
    if (device->action == VS_DEVICE_NONE || device->action == VS_DEVICE_DETACH)
      continue;

    // Do an attach-device on each device marked VS_DEVICE_KEEP or
    // VS_DEVICE_ATTACH

    char *manifest;
    if ((manifest = vs_device_manifest(device)) == NULL)
      continue;
    virDomainAttachDeviceFlags(domain, manifest, option);
    free(manifest);
  }

  xmlXPathFreeObject(result);
  if (view != NULL)
    xmlFree(view);
  xmlXPathFreeContext(ctxt);
  xmlFreeDoc(document);
  free(metadata);

  return 0;

except_result:
  xmlXPathFreeObject(result);

except_eval:
  xmlXPathFreeContext(ctxt);
  if (view != NULL)
    xmlFree(view);

except_root:
  xmlFreeDoc(document);

except_document:
  free(metadata);
  return -1;
}
