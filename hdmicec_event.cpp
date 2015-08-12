/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution, Apache license notifications and license are
 * retained for attribution purposes only.

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

#include <hardware_legacy/uevent.h>
#include <utils/Log.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <hardware/hdmi_cec.h>
#include <hdmicec.h>

#define HDMI_CEC_UEVENT_THREAD_NAME "HdmiCecThread"

static void *uevent_loop(void *param)
{
	const int MAX_DATA = 64;
	static char vdata[MAX_DATA];
	hdmi_cec_context_t * ctx = reinterpret_cast<hdmi_cec_context_t *>(param);
	char thread_name[64] = HDMI_CEC_UEVENT_THREAD_NAME;
	hdmi_event_t cec_event;
	struct pollfd pfd[2];
	int fd[2];

	prctl(PR_SET_NAME, (unsigned long) &thread_name, 0, 0, 0);
	setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

	fd[0] = open("/sys/devices/virtual/misc/cec/stat", O_RDONLY);
	if (fd[0] < 0) {
		ALOGE ("%s:not able to open cec state node", __func__);
		return NULL;
	}

	// Read once from the fds to clear the first notify
	pread(fd[0], vdata , MAX_DATA, 0);
	pfd[0].fd = fd[0];
        if (pfd[0].fd >= 0)
        	pfd[0].events = POLLPRI | POLLERR;
	do {
		int err = poll(pfd, 1, -1);
		if(err > 0) {
			if (pfd[0].revents & POLLPRI) {
				int len = pread(pfd[0].fd, vdata, MAX_DATA, 0);
				if (len < 0) {
					// If the read was just interrupted - it is not a
					// fatal error. Just continue in this case
					ALOGE ("%s: Unable to read cec state %s",
					    __FUNCTION__, strerror(errno));
					continue;
				}
				const char *str = vdata;
				int state = strtoull(str, NULL, 0);
				ALOGD("cec state is %d", state);
				if (state == 0 || state == 1) {
					ALOGD("%s sending hotplug: connected = %d ",
				 	      __FUNCTION__, state);
					cec_event.type = HDMI_EVENT_HOT_PLUG;
					cec_event.dev = &ctx->device;
					cec_event.hotplug.connected = state;
					cec_event.hotplug.port_id = HDMI_CEC_PORT_ID;
					if (ctx->event_callback)
						ctx->event_callback(&cec_event, ctx->cec_arg);
				} else if (state == 2) {
					if (ctx->fd < 0) {
						ALOGE("%s hdmi cec open error", __FUNCTION__);
						continue;
					}
					struct cec_framedata cecframe;	
					int ret = ioctl(ctx->fd, HDMI_IOCTL_CECREAD, &cecframe);
					if (ret < 0) {
						ALOGE("%s hdmi cec read error", __FUNCTION__);
						continue;
					}				
					cec_event.type = HDMI_EVENT_CEC_MESSAGE;
					cec_event.dev = &ctx->device;
					cec_event.cec.initiator = (cec_logical_address_t)(cecframe.srcdestaddr >> 4);
					cec_event.cec.destination = (cec_logical_address_t)(cecframe.srcdestaddr & 0x0f);
					cec_event.cec.length = cecframe.argcount + 1;
					cec_event.cec.body[0] = cecframe.opcode;
					for (ret = 0; ret < cecframe.argcount; ret++) {
						cec_event.cec.body [ret + 1] = cecframe.args[ret];
					}
					if (ctx->event_callback)
						ctx->event_callback(&cec_event, ctx->cec_arg);
				}
			}
		
		} else {
			ALOGE("%s: cec poll failed errno: %s", __FUNCTION__,
		              strerror(errno));
			continue;
		}
        } while (true);

	return NULL;
}

void init_uevent_thread(hdmi_cec_context_t* ctx)
{
	pthread_t uevent_thread;
	int ret;
	
	ALOGI("Initializing UEVENT Thread");
	ret = pthread_create(&uevent_thread, NULL, uevent_loop, (void*) ctx);
	if (ret) {
		ALOGE("%s: failed to create %s: %s", __FUNCTION__,
			HDMI_CEC_UEVENT_THREAD_NAME, strerror(ret));
	}
}

