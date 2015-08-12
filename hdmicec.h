/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HDMI_CEC_UEVENT_H
#define HDMI_CEC_UEVENT_H

#include <linux/ioctl.h>

struct cec_framedata {
	char srcdestaddr;
	char opcode;
	char args[CEC_MESSAGE_BODY_MAX_LENGTH];
	char argcount;
	char returnval;
};


#define HDMI_CEC_VERSION 0x05
#define HDMI_CEC_VENDOR_ID 0x000001
#define HDMI_CEC_PORT_ID 0x000001

#define HDMI_CEC_MAGIC 'N'
#define HDMI_IOCTL_CECSEND   _IOW(HDMI_CEC_MAGIC, 0 ,struct cec_framedata)
#define HDMI_IOCTL_CECENAB   _IOW(HDMI_CEC_MAGIC, 1, int)
#define HDMI_IOCTL_CECPHY    _IOR(HDMI_CEC_MAGIC, 2, int)
#define HDMI_IOCTL_CECLOGIC  _IOR(HDMI_CEC_MAGIC, 3, int)
#define HDMI_IOCTL_CECREAD   _IOR(HDMI_CEC_MAGIC, 4, struct cec_framedata)
#define HDMI_IOCTL_CECSETLA  _IOW(HDMI_CEC_MAGIC, 5, int)
#define HDMI_IOCTL_CECCLEARLA  _IOW(HDMI_CEC_MAGIC, 6, int)

#define HDMI_STATE_PATH "/sys/devices/virtual/switch/hdmi/state"
#define HDMI_DEV_PATH   "/dev/cec"
struct hdmi_cec_context_t {
    hdmi_cec_device_t device;
    /* our private state goes below here */
	event_callback_t event_callback;
	void* cec_arg;
	struct hdmi_port_info port;
	int fd;
};

void init_uevent_thread(hdmi_cec_context_t* ctx);
#endif
