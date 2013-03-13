/*
 * include/media/mt9m034.h
 *
 * Aptina MT9M034 sensor related register values
 *
 * Copyright (C) 2012 Aptina Imaging
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MT9M034_H__
#define __MT9M034_H__
#define MT9M034_I2C_ADDR		0x10 /* (0x20 >> 1) */

struct v4l2_subdev;

enum {
	MT9M034_COLOR_VERSION,
	MT9M034_MONOCHROME_VERSION,
};

/*
 * struct mt9m034_platform_data - MT9M034 platform data
 * @set_xclk: Clock frequency set callback
 * @reset: Chip reset GPIO (set to -1 if not used)
 * @ext_freq: Input clock frequency
 * @target_freq: Pixel clock frequency
 * @version: color or monochrome
 */
struct mt9m034_platform_data {
	int (*set_xclk)(struct v4l2_subdev *subdev, int hz);
	int (*reset)(struct v4l2_subdev *subdev, int active);
	int ext_freq;
	int target_freq;
	int version;
};

#endif
