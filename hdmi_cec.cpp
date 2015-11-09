/*
 * Copyright (C) 2015 The Android Open Source Project
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
 
#include <hardware/hdmi_cec.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <stdlib.h>
#include <hdmicec.h>
#include <cutils/properties.h>

static int hdmi_cec_device_open(const struct hw_module_t* module, const char* name,
				struct hw_device_t** device);

static struct hw_module_methods_t hdmi_cec_module_methods = {
	open: hdmi_cec_device_open
};

hdmi_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: HDMI_CEC_HARDWARE_MODULE_ID,
		name: "Rockchip hdmi cec module",
		author: "Rockchip",
		methods: &hdmi_cec_module_methods,
	}
};

static int hdmi_cec_add_logical_address(const struct hdmi_cec_device* dev,
					cec_logical_address_t addr)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	int ret;
	int val;

	val = 0;
	if (ctx->fd < 0)
		return -1;
	ret = ioctl(ctx->fd, HDMI_IOCTL_CECSETLA, &addr);
	return 0;
}

static void hdmi_cec_clear_logical_address(const struct hdmi_cec_device* dev)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, fd;

	ALOGI("%s", __func__);	
	if (ctx->fd < 0) {
		ALOGE("%s open error!", __func__);
		return;
	}
	ret = ioctl(ctx->fd, HDMI_IOCTL_CECCLEARLA, NULL);
	ALOGI("%s end", __func__);
}

static int hdmi_cec_get_physical_address(const struct hdmi_cec_device* dev, uint16_t* addr)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret;
	int val;

	if (addr == NULL) {
		ALOGI("%s addr is null", __func__);
		return -1;
	}
	ALOGI("%s", __func__);
	val = 0;
	if (ctx->fd < 0)
		return -1;
	ret = ioctl(ctx->fd, HDMI_IOCTL_CECPHY, &val);
	*addr = val;
	ALOGI("%s val = %x", __func__, val);
	if (!ret)
		return 0;
	else
		return -1;
}

static int hdmi_cec_send_message(const struct hdmi_cec_device* dev, const cec_message_t* message)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, fd;
	struct cec_framedata cecframe;
	int cecwakestate;
	int i;

	ALOGI("%s", __func__);
	ret = 0;
	if (ctx->fd < 0)
		return -1;

	i = 30;
	while(i--) {
		ret = ioctl(ctx->fd, HDMI_IOCTL_CECWAKESTATE, &cecwakestate);
		if (cecwakestate != 0) {
			ALOGI("cecwakestate = %d , i = %d", cecwakestate, i);
			usleep(40*1000);
		} else {
			break;
		}
		if (i == 0) {
			ALOGE("%s i = %d HDMI_RESULT_FAIL", __func__, i);
			return HDMI_RESULT_NACK;
		}
	}

	cecframe.srcdestaddr = (message->initiator << 4) | message->destination;
	cecframe.argcount = message->length - 1;
	cecframe.opcode = message->body[0];
	if (cecframe.argcount > 15)
		cecframe.argcount = 0;
	for (ret = 0; ret < cecframe.argcount; ret++)
		cecframe.args[ret] = message->body[ret + 1];
	if (cecframe.opcode == 0x90)
		cecframe.args[0] = 0;
	ret = ioctl(ctx->fd, HDMI_IOCTL_CECSEND, &cecframe);
 	if (ret)
		return HDMI_RESULT_FAIL;
	if (cecframe.returnval == 1)
		return HDMI_RESULT_NACK;
	else if (cecframe.returnval == 0)
		return HDMI_RESULT_SUCCESS;
	else if (cecframe.returnval == 2)
		return HDMI_RESULT_BUSY;
	return HDMI_RESULT_FAIL;
	ALOGI("%s end", __func__);
}

static void hdmi_cec_register_event_callback(const struct hdmi_cec_device* dev,
					     event_callback_t callback, void* arg)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	ctx->event_callback = callback;
	ctx->cec_arg = arg;
}

static void hdmi_cec_get_version(const struct hdmi_cec_device* dev, int* version)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	*version = HDMI_CEC_VERSION;
}

static void hdmi_cec_get_vendor_id(const struct hdmi_cec_device* dev, uint32_t* vendor_id)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	ALOGI("%s", __func__);
	*vendor_id = HDMI_CEC_VENDOR_ID;
}

static void hdmi_cec_get_port_info(const struct hdmi_cec_device* dev,
				   struct hdmi_port_info* list[], int* total)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, val, support;

	ret = 0;
	val = 0;
	support = 0;

	ALOGI("%s", __func__);
	if (ctx->fd > 0){
		ret = ioctl(ctx->fd, HDMI_IOCTL_CECPHY, &val);
		support = 1;
	} else {
		ALOGE("%s open HDMI_DEV_PATH error", __FUNCTION__);
	}
	list[0] = &ctx->port;
	list[0]->type = HDMI_OUTPUT;
	list[0]->port_id = HDMI_CEC_PORT_ID;
	list[0]->cec_supported = support;
	list[0]->arc_supported = 0;
	list[0]->physical_address = val;
	*total = 1;
	ALOGI("%s end", __func__);
}

static void hdmi_cec_set_option(const struct hdmi_cec_device* dev, int flag, int value)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int ret, fd;

	ALOGI("%s", __func__);
	if (flag == 1){
		if (ctx->fd < 0)
			return;
		ret = ioctl(ctx->fd, HDMI_IOCTL_CECENAB, &value);
		if (ret)
			return;
	}
}

static void hdmi_cec_set_audio_return_channel(const struct hdmi_cec_device* dev, int port_id, int flag)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	
	ALOGI("%s %d", __func__, port_id);
}

static int hdmi_cec_is_connected(const struct hdmi_cec_device* dev, int port_id)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	int hdmi_state;
	int ret, fd;
	char statebuf[2];

	ALOGI("%s", __func__);
	memset(statebuf, 0, sizeof(statebuf));
	fd= open(HDMI_STATE_PATH, O_RDONLY);
	if (fd < 0)
		return -1;
	ret = read(fd, statebuf, sizeof(statebuf));
	close(fd);
	if (ret)
		return -1;
	hdmi_state = atoi(statebuf);
	if (hdmi_state == 1)
		return HDMI_CONNECTED;
	else
		return HDMI_NOT_CONNECTED;
	return HDMI_NOT_CONNECTED;
}

static int hdmi_cec_device_close(struct hw_device_t *dev)
{
	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;

	if (ctx) {	
		close(ctx->fd);
		free(ctx);
	}

	return 0;
}

static int hdmi_cec_device_open(const struct hw_module_t* module, const char* name,
				struct hw_device_t** device)
{
	if (strcmp(name, HDMI_CEC_HARDWARE_INTERFACE))
		return -EINVAL;
    
	struct hdmi_cec_context_t *dev;
	dev = (hdmi_cec_context_t*)malloc(sizeof(*dev));
	
	/* initialize our state here */
	memset(dev, 0, sizeof(*dev));
	
	/* initialize the procs */
	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version = HDMI_CEC_DEVICE_API_VERSION_1_0;
	dev->device.common.module = const_cast<hw_module_t*>(module);
	dev->device.common.close = hdmi_cec_device_close;
	
	dev->device.add_logical_address = hdmi_cec_add_logical_address;
	dev->device.clear_logical_address = hdmi_cec_clear_logical_address;
	dev->device.get_physical_address = hdmi_cec_get_physical_address;
	dev->device.send_message = hdmi_cec_send_message;
	dev->device.register_event_callback = hdmi_cec_register_event_callback;
	dev->device.get_version = hdmi_cec_get_version;
	dev->device.get_vendor_id = hdmi_cec_get_vendor_id;
	dev->device.get_port_info = hdmi_cec_get_port_info;
	dev->device.set_option = hdmi_cec_set_option;
	dev->device.set_audio_return_channel = hdmi_cec_set_audio_return_channel;
	dev->device.is_connected = hdmi_cec_is_connected;
	dev->fd = open(HDMI_DEV_PATH,O_RDWR);
	if (dev->fd < 0) {
		ALOGE("%s open error!", __func__);
	}
	ALOGI("%s dev->fd = %d", __func__, dev->fd);
	property_set("sys.hdmicec.version",HDMI_CEC_HAL_VERSION);
	*device = &dev->device.common;
	init_uevent_thread(dev);
	
	ALOGI("rockchip hdmi cec modules loaded");
	return 0;
}
