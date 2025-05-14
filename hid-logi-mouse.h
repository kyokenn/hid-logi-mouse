/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __HID_LOGI_MOUSE_H
#define __HID_LOGI_MOUSE_H

/*
 * Copyright (c) 2022 Kyoken <kyoken@kyoken.ninja>
 */

/* TODO: move to hid-ids.h */
#define USB_VENDOR_ID_LOGITECH 0x046d
#define USB_DEVICE_ID_LOGITECH_USB_RECEIVER 0xc543
#define USB_DEVICE_ID_LOGITECH_G_PRO2 0xc09a

// #define LOGI_MOUSE_DEBUG 1

#define LOGI_MOUSE_MOUSE_WHEEL_RES 120  /* generic mouse wheel resolution */
#define LOGI_MOUSE_KP_WHEEL_RES_MIN 10  /* keypad emulated wheel resolution min */
#define LOGI_MOUSE_KP_WHEEL_RES_MAX 100  /* keypad emulated wheel resolution max */
#define LOGI_MOUSE_KP_WHEEL_TIME_NS 3000000000
#define LOGI_MOUSE_JOYSTICK_DEADZONE 16

#define LOGI_MOUSE_DATA_KEY_STATE_BITS 32  /* size of "key_state" item in bits */
#define LOGI_MOUSE_DATA_KEY_STATE_NUM 4  /* number of "key_state" items */
struct logi_mouse_data {
	struct input_dev *input;
	__u32 key_state[LOGI_MOUSE_DATA_KEY_STATE_NUM];
};

struct logi_mouse_state {
	u64 ktime_start;
};

#define LOGI_MOUSE_MAPPING_SIZE 98
static unsigned char logi_mouse_key_mapping[] = {
/* 00 */	0,		0,		0,		0,
/* 04 */	KEY_A,		KEY_B,		KEY_C,		KEY_D,
/* 08 */	KEY_E,		KEY_F,		KEY_G,		KEY_H,
/* 0C */	KEY_I,		KEY_J,		KEY_K,		KEY_L,
/* 0E */	KEY_M,		KEY_N,		KEY_O,		KEY_P,
/* 14 */	KEY_Q,		KEY_R,		KEY_S,		KEY_T,
/* 18 */	KEY_U,		KEY_V,		KEY_W,		KEY_X,
/* 1C */	KEY_Y,		KEY_Z,		KEY_1,		KEY_2,
/* 1E */	KEY_3,		KEY_4,		KEY_5,		KEY_6,
/* 24 */	KEY_7,		KEY_8,		KEY_9,		KEY_0,
/* 28 */	KEY_ENTER,	KEY_ESC,	KEY_BACKSPACE,	KEY_TAB,
/* 2C */	KEY_SPACE,	KEY_MINUS,	KEY_KPPLUS,	0,
/* 2E */	0,		0,		0,		0,
/* 34 */	0,		KEY_GRAVE,	KEY_EQUAL,	0,
/* 38 */	KEY_SLASH,	0,		KEY_F1,		KEY_F2,
/* 3C */	KEY_F3,		KEY_F4,		KEY_F5,		KEY_F6,
/* 3E */	KEY_F7,		KEY_F8,		KEY_F9,		KEY_F10,
/* 44 */	KEY_F11,	KEY_F12,	0,		0,
/* 48 */	0,		0,		KEY_HOME,	KEY_PAGEUP,
/* 4C */	KEY_DELETE,	0,		KEY_PAGEDOWN,	KEY_RIGHT,
/* 4E */	KEY_LEFT,	KEY_DOWN,	KEY_UP,		0,
/* 54 */	0,		0,		0,		0,
/* 58 */	0,		KEY_KP1,	KEY_KP2,	KEY_KP3,
/* 5C */	KEY_KP4,	KEY_KP5,	KEY_KP6,	KEY_KP7,
/* 5E */	KEY_KP8,	KEY_KP9,	0,
};

#endif
