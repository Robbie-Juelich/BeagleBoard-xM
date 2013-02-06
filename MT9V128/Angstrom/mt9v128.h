/* mt9v128 Camera
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MT9V128_H__
#define __MT9V128_H__

#define MT9V128_I2C_ADDR	0x48 //(0x90 >> 1)
//#define MT9V128_I2C_ADDR	0x5D //(0xBA >> 1)

struct v4l2_subdev;

enum {
	MT9V128_COLOR_VERSION,
	MT9V128_MONOCHROME_VERSION,
};

struct mt9v128_platform_data {
       int (*set_xclk)(struct v4l2_subdev *subdev, int hz);
       int (*reset)(struct v4l2_subdev *subdev, int active);
       int ext_freq; /* input frequency to the mt9v128 for PLL dividers */
       int target_freq; /* frequency target for the PLL */
       int version;
};
#endif /* __MT9V128_H__ */
