/* mt9p004 Camera
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MT9P004_H__
#define __MT9P004_H__

#include <media/v4l2-int-device.h>

#define MT9P004_I2C_ADDR		0x36 //(0x6C >> 1)
//#define MT9P004_I2C_ADDR		0x37 //(0x6E >> 1)	//Alternate Slave-Address

#define MT9P004_CLK_MAX			(64000000) /* 64MHz */
#define MT9P004_CLK_MIN			(2000000)  /* 2Mhz */

#define MT9P004_FLAG_PCLK_RISING_EDGE	(1 << 0)
#define MT9P004_FLAG_DATAWIDTH_8	(1 << 1) /* default width is 10 */

struct mt9p004_platform_data {
	char *master;
	int (*power_set) (struct v4l2_int_device *s, enum v4l2_power on);
	int (*ifparm) (struct v4l2_ifparm *p);
	int (*priv_data_set) (void *);
	u32 (*set_xclk) (struct v4l2_int_device *s, u32 xclkfreq);
	u32 flags;
};

#endif /* __MT9P004_H__ */
