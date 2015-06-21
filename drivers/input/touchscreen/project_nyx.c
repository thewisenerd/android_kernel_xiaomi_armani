/*
 * drivers/input/touchscreen/project_nyx.c
 *
 * Copyright (c) 2015, Vineeth Raj <thewisenerd@protonmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* standard module headers */
#include <linux/module.h>
#include <linux/init.h>

/* required for PAGE_SIZE */
#include <asm/page.h>

/* required for input handler */
#include <linux/input.h>

/* we shalt use fb notifier; alternatives are MDSS_NOTIFER, and early suspend */
#include <linux/notifier.h>
#include <linux/fb.h>

/* module stuff */
#define DRIVER_AUTHOR "thewisenerd <thewisenerd@protonmail.com>"
#define DRIVER_DESCRIPTION "project nyx"
#define DRIVER_VERSION "0.1"
#define LOGTAG "[nyx]: "

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPLv2");
/* module stuff (end) */

/* config variables */
#define NYX_DEFAULT  1
#define NYX_DEBUG    1
#define NYX_VERB_LVL 2
#define NYX_DELTA    5 // 10 maybe?
#define NYX_MIN      32
#define NYX_TRESHOLD (NYX_MIN - 1)
/* config variables (end) */

/* finally, import config */
#include <linux/input/nyx_config.h>

/* data variables. */
int nyx_switch = NYX_DEFAULT;
static bool scr_suspended = false;
static int x_pre = 0, y_pre = 0;
static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static unsigned nyx_count = 0;
static unsigned nyx_set_gesture = 0;
static bool lock_gesture  = false;
static bool lock_result   = false;

static u8 charbuf[NYX_SIZEOF_COORD * NYX_SIZEOF_CHARBUF];

#ifdef NYX_ISDEF_PAGENEXT
; // we'll deal with this sooner (or later)
#endif

static struct workqueue_struct *nyx_input_wq;
static struct work_struct nyx_input_work;

static struct kobject *nyx_kobj;

static struct Result {
	int   id;
	char  name[256];
} gesture, gesture_buf;;

static struct notifier_block nyx_fb_notif;
/* data variables (end) */

#define calc_delta(X, Y)  (abs(X - Y))

void nyx_reset(void) {
#ifdef NYX_DBG_LVL2
	pr_info(LOGTAG"%s: called!\n", __func__);
#endif

	if (lock_gesture) {
#ifdef NYX_DBG_LVL2
		pr_err(LOGTAG"%s: gesture locked!\n", __func__);
#endif
		return;
	}

	nyx_count = 0;
}

void nyx_proceed(void) {
	int rc;

	char *argv[3];
	char *envp[3];



	char *argv_set[] = { ONEIROI_BIN, "--set", NULL };
	char *argv_detect[] = { ONEIROI_BIN, "--detect", NULL };
	static char *envp[] = {
		"HOME=/",
		"/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin", NULL };

#ifdef NYX_DBG_LVL1
	pr_info(LOGTAG"%s: called!\n", __func__);
#endif

	/* lock gestures */
	lock_gesture = true;

	if ( call_usermodehelper( argv[0], argv, envp, UMH_NO_WAIT ) ) {
#ifdef NYX_DBG_LVL1
		pr_err(LOGTAG"%s: calling usermode helper "ONEIROI_BIN" failed!\n", __func__);
#endif
	} else {
#ifdef NYX_DBG_LVL2
		pr_err(LOGTAG"%s: calling usermode helper "ONEIROI_BIN" success!\n", __func__);
#endif
	};
}

static inline void new_touch(int *x, int *y) {
#ifdef NYX_DBG_LVL3
	pr_info(LOGTAG"%s: called!\n", __func__);
#endif
	x_pre = *x;
	y_pre = *y;
}

static inline void log_touch(int *x, int *y) {
#ifdef NYX_DBG_LVL3
	pr_info(LOGTAG"%s: logging touch %i: %i, %i\n", __func__, nyx_count, *x, *y);
#endif
 /*
  *  ___________
  * | a | b | c |
  *  -----------
  * <--x--><--y-->
  *
	*/

	if (nyx_count == NYX_SIZEOF_CHARBUF) {
#ifdef NYX_DBG_LVL2
		pr_err(LOGTAG"%s: run out of buffer!\n", __func__);
#endif
		return;
	}

	if (lock_gesture) {
#ifdef NYX_DBG_LVL2
		pr_err(LOGTAG"%s: gesture locked!\n", __func__);
#endif
		return;
	}

	/* get first 8 bits of x; store in a */
	charbuf[(nyx_count * NYX_SIZEOF_COORD)]     = ((*x & 0xfff) >> 4);

	/*
	 * 1. get last 4 bits of x; shift 4 bits to left
	 * 2. get first 4 bits of y;
	 * voila?
	 */
	charbuf[(nyx_count * NYX_SIZEOF_COORD) + 1]  = ((*x & 0x000f) << 4);
	charbuf[(nyx_count * NYX_SIZEOF_COORD) + 1] |= ((*y & 0xffff) >> 8);

	/* shift get last 8 bits of y and store in c */
	charbuf[(nyx_count * NYX_SIZEOF_COORD) + 2] = (*y & 0xff);

	/* increment nyx_count naw. */
	nyx_count++;

	/* set [x_|y_]pre */
	x_pre = *x;
	y_pre = *y;
}

static void nyx_detect(int *x, int *y) {
#ifdef NYX_DBG_LVL3
	pr_info(LOGTAG"%s: called with co-ords(%i, %i)!\n", __func__, *x, *y);
#endif
	if (!(nyx_count)) {
		new_touch(x, y);
		log_touch(x, y);
	} else {
		if ((calc_delta(*x, x_pre) > NYX_DELTA) ||
				(calc_delta(*y, y_pre) > NYX_DELTA)) {
				log_touch(x, y);
		}
	}
}

static void nyx_input_callback(struct work_struct *unused) {

	nyx_detect(&touch_x, &touch_y);

	return;
}

static void nyx_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value) {
#ifdef NYX_DBG_LVL3
	pr_info(LOGTAG"code: %s|%u, val: %i\n",
		((code==ABS_MT_POSITION_X) ? "X" :
		(code==ABS_MT_POSITION_Y) ? "Y" :
		(code==ABS_MT_TRACKING_ID) ? "ID" :
		"undef"), code, value);
#endif
	if (!scr_suspended)
		return;

	if (code == ABS_MT_SLOT) {
#ifdef NYX_DBG_LVL2
		pr_info(LOGTAG"%s: ABS_MT_SLOT; reset!\n", __func__);
#endif
		nyx_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
#ifdef NYX_DBG_LVL2
		pr_info(LOGTAG"%s: ABS_MT_TRACKING_ID; _proceed && _reset!\n", __func__);
#endif
		if (nyx_count > NYX_TRESHOLD)
			nyx_proceed();
		nyx_reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called || touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, nyx_input_wq, &nyx_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "touch")||
		strstr(dev->name, "ft5x06")) {
		return 0;
	} else {
		return 1;
	}
}

static int nyx_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "nyx";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void nyx_input_disconnect(struct input_handle *handle) {
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id nyx_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler nyx_input_handler = {
	.event      = nyx_input_event,
	.connect    = nyx_input_connect,
	.disconnect = nyx_input_disconnect,
	.name       = "nyx_inputreq",
	.id_table   = nyx_ids,
};

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		switch (*blank) {
			case FB_BLANK_UNBLANK:
				scr_suspended = false;
				nyx_reset();
				break;
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_NORMAL:
				scr_suspended = true;
				break;
		}
	}

	return NOTIFY_OK;
}


static ssize_t nyx_switch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", nyx_switch);

	return count;
}

static ssize_t nyx_switch_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[1] == '\n') {
		if (buf[0] == '0') {
			input_unregister_handler(&nyx_input_handler);
			nyx_switch = 0;
		} else if (buf[0] == '1') {
			if (!nyx_switch) {
				if (!input_register_handler(&nyx_input_handler)) {
					nyx_switch = 1;
				}
			}
		}
	}

	return count;
}
static DEVICE_ATTR(nyx_switch, (S_IWUSR|S_IRUGO),
	nyx_switch_show, nyx_switch_dump);

static ssize_t nyx_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	int i = 0;

/*
 * we have byte size 2; since max of 512.
 */

	u8 nyx_header[NYX_SIZEOF_HEADER_BUF];

	// shift 8 bits to the right; get first byte
	nyx_header[0] = (nyx_count >> 8) ? (nyx_count >> 8) : 0xff ;
	// get second byte.
	nyx_header[1] = (nyx_count & 0xff);

	// shift 8 bits to the right; get first byte
	count += snprintf(buf+count, PAGE_SIZE-count,
			"%c", nyx_header[0]);
	// get second byte.
	count += snprintf(buf+count, PAGE_SIZE-count,
			"%c", nyx_header[1]);

	for (i = 0; i <= nyx_count; i++) {
		count += snprintf(buf+count, PAGE_SIZE-count,
			"%c", charbuf[(i * NYX_SIZEOF_COORD)]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"%c", charbuf[(i * NYX_SIZEOF_COORD) + 1]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"%c", charbuf[(i * NYX_SIZEOF_COORD) + 2]);
	}

	/* unlock lock_gesture */
	lock_gesture = false;
	/* reset count */
	nyx_reset();

	return count;
}
static DEVICE_ATTR(nyx_data, (S_IWUSR|S_IRUGO),
	nyx_data_show, NULL);

static ssize_t nyx_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%03d", gesture.id);

	if (gesture.id == 0) {
		count += sprintf(buf+count, "\n");
		return count;
	}

	count += sprintf(buf+count, "|%s\n", gesture.name);

	gesture.id = 0;
	lock_result = false;

	return count;
}

static ssize_t nyx_gesture_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (lock_result)
		return count;

	sscanf(buf, "%03d|%s\n", &gesture.id, gesture.name);

	lock_result = true;
	return count;
}
static DEVICE_ATTR(nyx_gesture, (S_IWUSR|S_IRUGO),
	nyx_gesture_show, nyx_gesture_dump);

static ssize_t nyx_set_gesture_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (lock_result)
		return count;

	sscanf(buf, "%03d|%s\n", &gesture_buf.id, gesture_buf.name);

	lock_result = true;
	return count;
}
static DEVICE_ATTR(nyx_set_gesture, (S_IWUSR|S_IRUGO),
	NULL, nyx_set_gesture_dump);

static int __init nyx_init(void) {
	int rc = 0;

	nyx_input_wq = create_workqueue("nyxiwq");
	if (!nyx_input_wq) {
		pr_err("%s: Failed to create nyxiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&nyx_input_work, nyx_input_callback);
	rc = input_register_handler(&nyx_input_handler);
	if (rc)
		pr_err("%s: Failed to register nyx_input_handler\n", __func__);

	nyx_kobj = kobject_create_and_add("nyx", NULL);
	if (nyx_kobj == NULL) {
		pr_warn("%s: nyx_kobj create_and_add failed\n", __func__);
	}
	rc = sysfs_create_file(nyx_kobj, &dev_attr_nyx_switch.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for nyx_switch\n", __func__);
	}
	rc = sysfs_create_file(nyx_kobj, &dev_attr_nyx_data.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for nyx_data\n", __func__);
	}
	rc = sysfs_create_file(nyx_kobj, &dev_attr_nyx_gesture.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for nyx_gesture\n", __func__);
	}
	rc = sysfs_create_file(nyx_kobj, &dev_attr_nyx_set_gesture.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for nyx_set_gesture\n", __func__);
	}

	nyx_fb_notif.notifier_call = fb_notifier_callback;
	rc = fb_register_client(&nyx_fb_notif);
	if (rc)
		pr_err("%s: Unable to register fb_notifier: %d\n", __func__, rc);

	gesture.id = 0;

	return 0; // rc?
}

static void __exit nyx_exit(void)
{
	kobject_del(nyx_kobj);
	fb_unregister_client(&nyx_fb_notif);
	input_unregister_handler(&nyx_input_handler);
	destroy_workqueue(nyx_input_wq);
	return;
}

module_init(nyx_init);
module_exit(nyx_exit);
