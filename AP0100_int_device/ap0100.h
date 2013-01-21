/*
 * drivers/media/video/ap0100.h
 *
 * Copyright (C) 2012 Aptina Imaging
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _AP0100_H
#define _AP0100_H
#include <media/v4l2-int-device.h>

#define AP0100_MODULE_NAME		"ap0100"

#define AP0100_I2C_ADDR  		(0xBA >>1)

#define AP0100_CLK_MAX 	(48000000) /* 48MHz */
#define AP0100_CLK_MIN	(6000000)  /* 6Mhz */

/**
 * struct ap0100_platform_data - Platform data values and access functions.
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @ifparm: Interface parameters access function.
 * @priv_data_set: Device private data (pointer) access function.
 * @clk_polarity: Clock polarity of the current interface.
 * @ hs_polarity: HSYNC Polarity configuration for current interface.
 * @ vs_polarity: VSYNC Polarity configuration for current interface.
 */
struct ap0100_platform_data {
	char *master;
	int (*power_set) (struct v4l2_int_device *s, enum v4l2_power power);
	int (*ifparm) (struct v4l2_ifparm *p);
	int (*priv_data_set) (void *);
	/* Interface control params */
	bool clk_polarity;
	bool hs_polarity;
	bool vs_polarity;
	u32 flags;
};

#endif				/* ifndef _AP0100_H */
