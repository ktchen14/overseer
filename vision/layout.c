#include <stddef.h>

#include "device.h"

static vs_device_t GPU1_VIDEO = {
  .name = "GPU1_VIDEO", .view_list = { "DualScreen", "Screen1", NULL },
  .xtra = "<rom file=\"/home/ktchen14/rom\" />",
};

static vs_device_t GPU1_AUDIO = {
  .name = "GPU1_AUDIO", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t GPU2_VIDEO = {
  .name = "GPU2_VIDEO", .view_list = { "DualScreen", "Screen2", NULL },
  .xtra = "<rom file=\"/home/ktchen14/rom\" />",
};

static vs_device_t GPU2_AUDIO = {
  .name = "GPU2_AUDIO", .view_list = { "DualScreen", "Screen2", NULL },
};

static vs_device_t SWITCH_PORT_1 = {
  .name = "SWITCH_PORT_1", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t SWITCH_PORT_2 = {
  .name = "SWITCH_PORT_2", .view_list = { "DualScreen", NULL },
};

static vs_device_t SWITCH_PORT_3 = {
  .name = "SWITCH_PORT_3", .view_list = { "DualScreen", NULL },
};

static vs_device_t SWITCH_PORT_4 = {
  .name = "SWITCH_PORT_4", .view_list = { "DualScreen", "Screen2", NULL },
};

vs_device_t *vs_device_list[] = {
  &GPU1_VIDEO, &GPU1_AUDIO,
  &GPU2_VIDEO, &GPU2_AUDIO,
  &SWITCH_PORT_1, &SWITCH_PORT_2, &SWITCH_PORT_3, &SWITCH_PORT_4,
  NULL,
};
