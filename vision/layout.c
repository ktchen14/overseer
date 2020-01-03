#include <stddef.h>

#include "device.h"

static vs_device_t CAMERA = {
  .name = "camera", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t GPU_VIDEO_1 = {
  .name = "GPU_VIDEO_1", .view_list = { "DualScreen", "Screen1", NULL },
  .xtra = "<rom file=\"/home/ktchen14/rom\" />",
};

static vs_device_t GPU_AUDIO_1 = {
  .name = "GPU_AUDIO_1", .view_list = { "DualScreen", "Screen1", NULL },
};

static vs_device_t GPU_VIDEO_2 = {
  .name = "GPU_VIDEO_2", .view_list = { "DualScreen", "Screen2", NULL },
  .xtra = "<rom file=\"/home/ktchen14/rom\" />",
};

static vs_device_t GPU_AUDIO_2 = {
  .name = "GPU_AUDIO_2", .view_list = { "DualScreen", "Screen2", NULL },
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
  &GPU_VIDEO_1, &GPU_AUDIO_1,
  &GPU_VIDEO_2, &GPU_AUDIO_2,
  &SWITCH_PORT_1, &SWITCH_PORT_2, &SWITCH_PORT_3, &SWITCH_PORT_4,
  &CAMERA,
  NULL,
};
