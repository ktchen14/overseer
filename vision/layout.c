#include <stddef.h>

#include "device.h"

static vs_device_t GPU1_VIDEO = {
  .name = "GPU1_VIDEO", .view_list = { "DualScreen", "Screen1", NULL },
  .xtra = "<rom file=\"/home/ktchen14/Gigabyte.GTX1070Ti.8192.171006.rom\" />",
};

static vs_device_t GPU1_AUDIO = {
  .name = "GPU1_AUDIO", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t GPU2_VIDEO = {
  .name = "GPU2_VIDEO", .view_list = { "DualScreen", "Screen2", NULL },
  .xtra = "<rom file=\"/home/ktchen14/Gigabyte.GTX1070Ti.8192.171006.rom\" />",
};

static vs_device_t GPU2_AUDIO = {
  .name = "GPU2_AUDIO", .view_list = { "DualScreen", "Screen2", NULL },
};

// static vs_device_t SWITCH_PORT_1 = {
//   .name = "SWITCH_PORT_1", .view_list = { "DualScreen", "Screen1", NULL },
// };

static vs_device_t USBHUB1_1 = {
  .name = "USBHUB1_1", .view_list = { "DualScreen", "Screen1", NULL },
};
static vs_device_t USBHUB1_2 = {
  .name = "USBHUB1_2", .view_list = { "DualScreen", "Screen1", NULL },
};
static vs_device_t USBHUB1_3 = {
  .name = "USBHUB1_3", .view_list = { "DualScreen", "Screen1", NULL },
};
static vs_device_t USBHUB1_4 = {
  .name = "USBHUB1_4", .view_list = { "DualScreen", "Screen1", NULL },
};
static vs_device_t USBHUB1_5 = {
  .name = "USBHUB1_5", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t SWITCH_PORT_2 = {
  .name = "SWITCH_PORT_2", .view_list = { "DualScreen", NULL },
};

static vs_device_t SWITCH_PORT_3 = {
  .name = "SWITCH_PORT_3", .view_list = { "DualScreen", NULL },
};

static vs_device_t USBHUB2_1 = {
  .name = "USBHUB2_1", .view_list = { "DualScreen", "Screen2", NULL },
};
static vs_device_t USBHUB2_2 = {
  .name = "USBHUB2_2", .view_list = { "DualScreen", "Screen2", NULL },
};
static vs_device_t USBHUB2_3 = {
  .name = "USBHUB2_3", .view_list = { "DualScreen", "Screen2", NULL },
};
static vs_device_t USBHUB2_4 = {
  .name = "USBHUB2_4", .view_list = { "DualScreen", "Screen2", NULL },
};
static vs_device_t USBHUB2_5 = {
  .name = "USBHUB2_5", .view_list = { "DualScreen", "Screen2", NULL },
};

// static vs_device_t SWITCH_PORT_4 = {
//   .name = "SWITCH_PORT_4", .view_list = { "DualScreen", "Screen2", NULL },
// };

vs_device_t *vs_device_list[] = {
  &GPU1_VIDEO, &GPU1_AUDIO,
  &GPU2_VIDEO, &GPU2_AUDIO,
  &USBHUB1_1, &USBHUB1_2, &USBHUB1_3, &USBHUB1_4, &USBHUB1_5,
  &SWITCH_PORT_2, &SWITCH_PORT_3,
  &USBHUB2_1, &USBHUB2_2, &USBHUB2_3, &USBHUB2_4, &USBHUB2_5,
  NULL,
};
