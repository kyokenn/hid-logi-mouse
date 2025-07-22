// SPDX-License-Identifier: GPL-2.0+
/*
 * HID driver for Logitech mice
 *
 * Copyright (c) 2022 Kyoken <kyoken@kyoken.ninja>
 */

#include <linux/types.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/timer.h>

/* #include "hid-ids.h" */
#include "hid-logi-mouse.h"

static struct input_dev *logi_mouse_input;
static struct logi_mouse_state logi_mouse_state;

static void input_repeat_key(struct timer_list *t) {
	int ms;
	u64 dt;
	unsigned int wheel_factor;
	short wheel_res = 0;

	if (logi_mouse_input->repeat_key) {
		dt = ktime_get_ns() - logi_mouse_state.ktime_start;
		dt = (dt > LOGI_MOUSE_KP_WHEEL_TIME_NS) ? LOGI_MOUSE_KP_WHEEL_TIME_NS : dt;
		wheel_factor = dt * 65536 / LOGI_MOUSE_KP_WHEEL_TIME_NS;  /* 2 ** 16 */
		wheel_res = ((LOGI_MOUSE_KP_WHEEL_RES_MIN * (65536 - wheel_factor)) +
					 (LOGI_MOUSE_KP_WHEEL_RES_MAX * wheel_factor)) >> 16;
	}

	switch(logi_mouse_input->repeat_key) {
	case KEY_KP4:
		input_report_rel(logi_mouse_input, REL_HWHEEL_HI_RES, wheel_res);
		break;
	case KEY_KP6:
		input_report_rel(logi_mouse_input, REL_HWHEEL_HI_RES, -wheel_res);
		break;
	case KEY_KP2:
		input_report_rel(logi_mouse_input, REL_WHEEL_HI_RES, -wheel_res);
		break;
	case KEY_KP8:
		input_report_rel(logi_mouse_input, REL_WHEEL_HI_RES, wheel_res);
		break;
	default:
		break;
	}

	input_sync(logi_mouse_input);

	ms = logi_mouse_input->rep[REP_PERIOD];
	if (ms && logi_mouse_input->repeat_key)
		mod_timer(&logi_mouse_input->timer, jiffies + msecs_to_jiffies(ms));
}

static void logi_mouse_handle_mouse(
		struct logi_mouse_data *drv_data, struct hid_report *report, u8 *data, int size) {
#ifdef LOGI_MOUSE_DEBUG
	if (size == 9) {
		printk(KERN_INFO "hid-logi-mouse: MOUSE %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			   data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
	} else if (size == 13) {
		printk(KERN_INFO "hid-logi-mouse: MOUSE %02X %02X %02X %02X %02X %02X %02X %02X",
			   data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		printk(KERN_INFO "					  %02X %02X %02X %02X %02X",
			 data[8], data[9], data[10], data[11], data[12]);
	} else {
		printk(KERN_INFO "hid-logi-mouse: MOUSE X %d", size);
	}
#endif

	s8 btn = 0;
	s8 whl = 0;
	s16 x = 0;
	s16 y = 0;

	switch(size) {
	case 9:
		x = data[3] | (u16)data[4] << 8;
		y = data[5] | (u16)data[6] << 8;
		btn = data[1];
		whl = data[7];
		break;
	case 13:
		x = data[2] | (u16)data[3] << 8;
		y = data[4] | (u16)data[5] << 8;
		btn = data[0];
		whl = data[6];
		break;

	default:
		break;
	}

#ifdef LOGI_MOUSE_DEBUG
	printk(KERN_INFO "hid-logi-mouse: X=%d Y=%d BTN=%02X WHL=%d", x, y, btn, whl);
#endif

	input_report_key(drv_data->input, BTN_LEFT, (btn & (1 << 0)) != 0);
	input_report_key(drv_data->input, BTN_RIGHT, (btn & (1 << 1)) != 0);
	input_report_key(drv_data->input, BTN_MIDDLE, (btn & (1 << 2)) != 0);
	input_report_key(drv_data->input, BTN_FORWARD, (btn & (1 << 3)) != 0);
	input_report_key(drv_data->input, BTN_BACK, (btn & (1 << 4)) != 0);
	input_report_rel(drv_data->input, REL_X, x);
	input_report_rel(drv_data->input, REL_Y, y);
	input_report_rel(drv_data->input, REL_WHEEL_HI_RES, whl * LOGI_MOUSE_MOUSE_WHEEL_RES);
	input_sync(drv_data->input);
}

static void logi_mouse_handle_keyboard(
		struct logi_mouse_data *drv_data, struct hid_report *report, u8 *data, int size) {
#ifdef LOGI_MOUSE_DEBUG
	if (size == 8) {
		printk(KERN_INFO "hid-logi-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X",
			   data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	} else if (size == 9) {
		printk(KERN_INFO "hid-logi-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			   data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
	} else if (size == 12) {
		printk(KERN_INFO "hid-logi-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X",
			   data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		printk(KERN_INFO "					 %02X %02X %02X %02X",
			   data[8], data[9], data[10], data[11]);
	} else {
		printk(KERN_INFO "hid-logi-mouse: KEYS X %d", size);
	}
#endif

	int i, bit, code, logi_code, key_code, offset;
	u32 bitmask[LOGI_MOUSE_DATA_KEY_STATE_NUM];
	u32 modified;
	for (i = 0; i < LOGI_MOUSE_DATA_KEY_STATE_NUM; i++)
		bitmask[i] = 0;

	/* build bitmask from array of active key codes */

	offset = 2;
	if (size == 8)
		offset--;

	for (; offset < size; offset++) {
		code = data[offset];
		if (!code)
			continue;
		i = LOGI_MOUSE_DATA_KEY_STATE_NUM - (code / LOGI_MOUSE_DATA_KEY_STATE_BITS) - 1;
		bitmask[i] |= 1 << (code % LOGI_MOUSE_DATA_KEY_STATE_BITS);
	}

#ifdef LOGI_MOUSE_DEBUG
	printk(KERN_INFO "hid-logi-mouse: STAT %08X %08X %08X %08X",
		   drv_data->key_state[0], drv_data->key_state[1], drv_data->key_state[2], drv_data->key_state[3]);
	printk(KERN_INFO "hid-logi-mouse: MASK %08X %08X %08X %08X",
		   bitmask[0], bitmask[1], bitmask[2], bitmask[3]);
#endif

	/* get key codes and send key events */

	for (i = 0; i < LOGI_MOUSE_DATA_KEY_STATE_NUM; i++) {
		modified = drv_data->key_state[LOGI_MOUSE_DATA_KEY_STATE_NUM - i - 1] ^
			bitmask[LOGI_MOUSE_DATA_KEY_STATE_NUM - i - 1];
		for (bit = 0; bit < LOGI_MOUSE_DATA_KEY_STATE_BITS; bit += 1) {
			if (!(modified & (1 << bit)))
				continue;

			logi_code = i * LOGI_MOUSE_DATA_KEY_STATE_BITS + bit;
			if (logi_code >= LOGI_MOUSE_MAPPING_SIZE)
				continue;

			key_code = logi_mouse_key_mapping[logi_code];

			if (bitmask[LOGI_MOUSE_DATA_KEY_STATE_NUM - i - 1] & (1 << bit)) { /* key pressed */
#ifdef LOGI_MOUSE_DEBUG
				printk(KERN_INFO "hid-logi-mouse: PRES LOGI=0x%02X (%d) LINUX=0x%02X (%d) '%c'",
					   logi_code, logi_code, key_code, key_code, key_code);
#endif
				switch(key_code) {
				case KEY_KP4:
				case KEY_KP6:
				case KEY_KP2:
				case KEY_KP8:
					/* start repeating key events */
					logi_mouse_input->repeat_key = key_code;
					logi_mouse_state.ktime_start = ktime_get_ns();
					input_repeat_key(NULL);
					break;
				default:
					/* send regular key press event */
					input_report_key(drv_data->input, key_code, 1);
					input_sync(drv_data->input);
					break;
				}
			} else { /* key released */
#ifdef LOGI_MOUSE_DEBUG
				printk(KERN_INFO "hid-logi-mouse: RELE LOGI=0x%02X (%d) LINUX=0x%02X (%d) '%c'",
					   logi_code, logi_code, key_code, key_code, key_code);
#endif
				switch(key_code) {
				case KEY_KP4:
				case KEY_KP6:
				case KEY_KP2:
				case KEY_KP8:
					/* stop repeating key events */
					logi_mouse_input->repeat_key = 0;
					logi_mouse_state.ktime_start = 0;
					break;
				default:
					/* send regular key release event */
					input_report_key(drv_data->input, key_code, 0);
					input_sync(drv_data->input);
					break;
				}
			}
		}
	}

	/* save current keys state for tracking released keys */

	for (i = 0; i < LOGI_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = bitmask[i];
}

static int logi_mouse_raw_event(
		struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
	struct usb_interface *iface = NULL;

	struct logi_mouse_data *drv_data = hid_get_drvdata(hdev);
	if (!drv_data)
		return 0;

	if (hdev->dev.parent != NULL)
		iface = to_usb_interface(hdev->dev.parent);

#ifdef LOGI_MOUSE_DEBUG
	printk(KERN_INFO "hid-logi-mouse: RAW FROM=%d SIZE=%d TYPE=%d APP=%d",
		   (iface != NULL) ? iface->cur_altsetting->desc.bInterfaceProtocol : 0,
		   size, report->type, report->application);

	for (int i = 0; i < report->maxfield; i++) {
		printk(KERN_INFO "hid-logi-mouse: FLD %d PHY=%d LOG=%d APP=%d",
			   i, report->field[i]->physical, report->field[i]->logical,
			   report->field[i]->application);
	/* for (int j = 0; j < report->field[i]->maxusage; j++) */
	/*   hid_resolv_usage(report->field[i]->usage[j].hid); */
	}
#endif

	switch(report->application) {
	case HID_GD_MOUSE:
		logi_mouse_handle_mouse(drv_data, report, data, size);
		break;
	case HID_GD_KEYBOARD:
		logi_mouse_handle_keyboard(drv_data, report, data, size);
		break;
	default:
		break;
	}

	return 0;
}

static int logi_mouse_probe(struct hid_device *hdev, const struct hid_device_id *id) {
	struct logi_mouse_data *drv_data;
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "%s:parse of hid interface failed\n", __func__);
		return ret;
	}

	drv_data = devm_kzalloc(&hdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data) {
		hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
		return -ENOMEM;
	}
	drv_data->input = logi_mouse_input;
	for (int i = 0; i < LOGI_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = 0;
	hid_set_drvdata(hdev, drv_data);

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret) {
		hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
		return ret;
	}

	ret = hid_hw_open(hdev);
	if (ret) {
		hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void logi_mouse_remove(struct hid_device *hdev) {
	struct logi_mouse_data *drv_data = hid_get_drvdata(hdev);
	if (!drv_data) {
		hid_hw_stop(hdev);
		return;
	}

	hid_hw_stop(hdev);
	drv_data->input = NULL;
}

static const struct hid_device_id logi_mouse_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH, USB_DEVICE_ID_LOGITECH_USB_RECEIVER) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH, USB_DEVICE_ID_LOGITECH_G_PRO2) },
	{ }
};
MODULE_DEVICE_TABLE(hid, logi_mouse_devices);

static struct hid_driver logi_mouse_driver = {
	.name = "hid-logi-mouse",
	.id_table = logi_mouse_devices,
	.probe = logi_mouse_probe,
	.remove = logi_mouse_remove,
	.raw_event = logi_mouse_raw_event,
};

static int __init logi_mouse_init(void) {
	int ret;
	pr_info("hid-logi-mouse: loading Logi mouse driver\n");

	logi_mouse_state.ktime_start = 0;

	logi_mouse_input = input_allocate_device();
	if (!logi_mouse_input) {
		return -ENOMEM;
	}

	logi_mouse_input->name = "Logi mouse input";
	logi_mouse_input->phys = "hid-logi-mouse";

	logi_mouse_input->id.bustype = BUS_VIRTUAL;
	logi_mouse_input->id.product = 0x0000;
	logi_mouse_input->id.vendor = 0x0000;
	logi_mouse_input->id.version = 0x0000;

	logi_mouse_input->keybit[3] = 0x1000300000007;
	logi_mouse_input->keybit[2] = 0xff800078000007ff;
	logi_mouse_input->keybit[1] = 0xfebeffdff3cfffff;
	logi_mouse_input->keybit[0] = 0xfffffffffffffffe;

	set_bit(EV_REL, logi_mouse_input->evbit);
	set_bit(EV_KEY, logi_mouse_input->evbit);

	set_bit(REL_X, logi_mouse_input->relbit);
	set_bit(REL_Y, logi_mouse_input->relbit);
	set_bit(REL_WHEEL, logi_mouse_input->relbit);
	set_bit(REL_WHEEL_HI_RES, logi_mouse_input->relbit);
	set_bit(REL_HWHEEL, logi_mouse_input->relbit);
	set_bit(REL_HWHEEL_HI_RES, logi_mouse_input->relbit);

	set_bit(BTN_LEFT, logi_mouse_input->keybit);
	set_bit(BTN_RIGHT, logi_mouse_input->keybit);
	set_bit(BTN_MIDDLE, logi_mouse_input->keybit);
	set_bit(BTN_BACK, logi_mouse_input->keybit);
	set_bit(BTN_FORWARD, logi_mouse_input->keybit);
	set_bit(BTN_TOOL_DOUBLETAP, logi_mouse_input->keybit);
	set_bit(KEY_BACK, logi_mouse_input->keybit);
	set_bit(KEY_FORWARD, logi_mouse_input->keybit);

	logi_mouse_input->rep[REP_PERIOD] = 16;
	timer_setup(&logi_mouse_input->timer, input_repeat_key, 0);

	ret = input_register_device(logi_mouse_input);
	if (ret) {
		input_free_device(logi_mouse_input);
		return ret;
	}

	return hid_register_driver(&logi_mouse_driver);
}

static void __exit logi_mouse_exit(void) {
	pr_info("hid-logi-mouse: unloading Logi mouse driver\n");

	logi_mouse_input->repeat_key = 0;
	timer_delete(&logi_mouse_input->timer);

	hid_unregister_driver(&logi_mouse_driver);
	input_unregister_device(logi_mouse_input);
}

module_init(logi_mouse_init);
module_exit(logi_mouse_exit);

MODULE_AUTHOR("Kyoken <kyoken@kyoken.ninja>");
MODULE_DESCRIPTION("Logi Mouse");
MODULE_LICENSE("GPL");
