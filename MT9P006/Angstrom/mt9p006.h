/* mt9p006 Camera
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MT9P006_H__
#define __MT9P006_H__

//#define MT9P006_I2C_ADDR	0x48 //(0x90 >> 1)
#define MT9P006_I2C_ADDR	0x5d //(0xBA >> 1)

struct v4l2_subdev;

struct mt9p006_platform_data {
       int (*set_xclk)(struct v4l2_subdev *subdev, int hz);
       int (*reset)(struct v4l2_subdev *subdev, int active);
       int ext_freq; /* input frequency to the mt9p006 for PLL dividers */
       int target_freq; /* frequency target for the PLL */
};
#endif /* __MT9P006_H__ */
