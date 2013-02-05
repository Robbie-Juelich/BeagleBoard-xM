/*
 *
 * Aptina MT9M021 sensor driver
 *
 * Copyright (C) 2013 Aptina Imaging
 *
 * Contributor Prashanth Subramanya <sprashanth@aptina.com>
 *
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/videodev2.h>

#include <media/mt9m021.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define MT9M021_PIXEL_ARRAY_WIDTH	1280
#define MT9M021_PIXEL_ARRAY_HEIGHT	960

#define	MT9M021_ROW_START_MIN		0
#define	MT9M021_ROW_START_MAX		960
#define	MT9M021_ROW_START_DEF		0
#define	MT9M021_COLUMN_START_MIN	0
#define	MT9M021_COLUMN_START_MAX	1280
#define	MT9M021_COLUMN_START_DEF	0
#define	MT9M021_WINDOW_HEIGHT_MIN	2
#define	MT9M021_WINDOW_HEIGHT_MAX	960
#define	MT9M021_WINDOW_HEIGHT_DEF	960
#define	MT9M021_WINDOW_WIDTH_MIN	2
#define	MT9M021_WINDOW_WIDTH_MAX	1280
#define	MT9M021_WINDOW_WIDTH_DEF	1280
#define MT9M021_ENABLE			1
#define MT9M021_DISABLE			0

#define MT9M021_CHIP_ID_REG		0x3000
#define MT9M021_CHIP_ID			0x2401

#define MT9M021_RESET_REG		0x301A
#define MT9M021_SEQ_CTRL_PORT		0x3088
#define MT9M021_SEQ_DATA_PORT		0x3086
#define MT9M021_ANALOG_REG		0x3ED6
#define MT9M021_TEST_RAW_MODE		0x307A
#define MT9M021_DARK_CTRL		0x3044
#define MT9M021_DATA_PEDESTAL		0x301E
#define MT9M021_COLUMN_CORRECTION	0x30D4

#define MT9M021_VT_SYS_CLK_DIV		0x302A
#define MT9M021_VT_PIX_CLK_DIV		0x302C
#define MT9M021_PRE_PLL_CLK_DIV		0x302E
#define MT9M021_PLL_MULTIPLIER		0x3030
#define MT9M021_DIGITAL_TEST		0x30B0

#define	MT9M021_Y_ADDR_START		0x3002
#define	MT9M021_X_ADDR_START		0x3004
#define	MT9M021_Y_ADDR_END		0x3006
#define	MT9M021_X_ADDR_END		0x3008
#define	MT9M021_FRAME_LENGTH_LINES	0x300A
#define	MT9M021_LINE_LENGTH_PCK		0x300C
#define	MT9M021_COARSE_INT_TIME		0x3012
#define MT9M021_FINE_INT_TIME		0x3014
#define	MT9M021_COARSE_INT_TIME_CB	0x3016
#define MT9M021_FINE_INT_TIME_CB	0x3018
#define	MT9M021_X_ODD_INC		0x30A2
#define	MT9M021_Y_ODD_INC		0x30A6
#define	MT9M021_READ_MODE		0x3040
#define MT9M021_TEST_PATTERN		0x3070
#define MT9M021_LLP_RECOMMENDED		1650
#define MT9M021_DIGITAL_BINNING		0x3032
#define MT9M021_HOR_AND_VER_BIN		0x0022
#define MT9M021_HOR_BIN			0x0011
#define MT9M021_DISABLE_BINNING		0x0000

#define MT9M021_AE_CTRL_REG		0x3100
#define MT9M021_EMBEDDED_DATA_CTRL	0x3064

#define MT9M021_GREEN1_GAIN		0x3056
#define MT9M021_BLUE_GAIN		0x3058
#define MT9M021_RED_GAIN		0x305A
#define MT9M021_GREEN2_GAIN		0x305C
#define MT9M021_GLOBAL_GAIN		0x305E
#define MT9M021_GREEN1_GAIN_CB		0x30BC
#define MT9M021_BLUE_GAIN_CB		0x30BE
#define MT9M021_RED_GAIN_CB		0x30C0
#define MT9M021_GREEN2_GAIN_CB		0x30C2
#define MT9M021_GLOBAL_GAIN_CB		0x30C4

#define MT9M021_RESET			0x00D9
#define MT9M021_STREAM_OFF		0x00D8
#define MT9M021_STREAM_ON		0x00DC

#define V4L2_CID_TEST_PATTERN           (V4L2_CID_USER_BASE | 0x1001)
#define V4L2_CID_GAIN_RED		(V4L2_CID_USER_BASE | 0x1002)
#define V4L2_CID_GAIN_GREEN1		(V4L2_CID_USER_BASE | 0x1003)
#define V4L2_CID_GAIN_GREEN2		(V4L2_CID_USER_BASE | 0x1004)
#define V4L2_CID_GAIN_BLUE		(V4L2_CID_USER_BASE | 0x1005)
#define V4L2_CID_ANALOG_GAIN		(V4L2_CID_USER_BASE | 0x1006)

#define MT9M021_ANALOG_GAIN_MIN		0x0
#define MT9M021_ANALOG_GAIN_MAX		0x3
#define MT9M021_ANALOG_GAIN_DEF		0x0
#define MT9M021_ANALOG_GAIN_SHIFT	4
#define MT9M021_ANALOG_GAIN_MASK	0x0030

#define MT9M021_GLOBAL_GAIN_MIN		0x00
#define MT9M021_GLOBAL_GAIN_MAX		0xFF
#define MT9M021_GLOBAL_GAIN_DEF		0x20

#define MT9M021_EXPOSURE_MIN		1
#define MT9M021_EXPOSURE_MAX		0x02A0
#define MT9M021_EXPOSURE_DEF		0x0100

#undef MT9M021_I2C_DEBUG
#undef MT9M021_DEBUG

struct mt9m021_frame_size {
	u16 width;
	u16 height;
};

struct mt9m021_pll_divs {
        u32 ext_freq;
        u32 target_freq;
        u16 m;
        u16 n;
        u16 p1;
        u16 p2;
};

struct mt9m021_priv {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_rect crop;  /* Sensor window */
	struct v4l2_mbus_framefmt format;
	struct v4l2_ctrl_handler ctrls;
	struct mt9m021_platform_data *pdata;
	struct mutex power_lock; /* lock to protect power_count */
	struct mt9m021_pll_divs *pll;
	int power_count;
	enum v4l2_exposure_auto_type autoexposure;
};

static unsigned int mt9m021_seq_data[133] = {
	0x3227, 0x0101, 0x0F25, 0x0808, 0x0227, 0x0101, 0x0837, 0x2700,
	0x0138, 0x2701, 0x013A, 0x2700, 0x0125, 0x0020, 0x3C25, 0x0040,
	0x3427, 0x003F, 0x2500, 0x2037, 0x2540, 0x4036, 0x2500, 0x4031,
	0x2540, 0x403D, 0x6425, 0x2020, 0x3D64, 0x2510, 0x1037, 0x2520,
	0x2010, 0x2510, 0x100F, 0x2708, 0x0802, 0x2540, 0x402D, 0x2608,
	0x280D, 0x1709, 0x2600, 0x2805, 0x26A7, 0x2807, 0x2580, 0x8029,
	0x1705, 0x2500, 0x4027, 0x2222, 0x1616, 0x2726, 0x2617, 0x3626,
	0xA617, 0x0326, 0xA417, 0x1F28, 0x0526, 0x2028, 0x0425, 0x2020,
	0x2700, 0x2625, 0x0000, 0x171E, 0x2500, 0x0425, 0x0020, 0x2117,
	0x121B, 0x1703, 0x2726, 0x2617, 0x2828, 0x0517, 0x1A26, 0x6017,
	0xAE25, 0x0080, 0x2700, 0x2626, 0x1828, 0x002E, 0x2A28, 0x081E,
	0x4127, 0x1010, 0x0214, 0x6060, 0x0A14, 0x6060, 0x0B14, 0x6060,
	0x0C14, 0x6060, 0x0D14, 0x6060, 0x0217, 0x3C14, 0x0060, 0x0A14,
	0x0060, 0x0B14, 0x0060, 0x0C14, 0x0060, 0x0D14, 0x0060, 0x0811,
	0x2500, 0x1027, 0x0010, 0x2F6F, 0x0F3E, 0x2500, 0x0827, 0x0008,
	0x3066, 0x3225, 0x0008, 0x2700, 0x0830, 0x6631, 0x3D64, 0x2508,
	0x083D, 0xFF3D, 0x2A27, 0x083F, 0x2C00
};

static unsigned int mt9m021_analog_setting[8] = {
	0x00FD, 0x0FFF, 0x0003, 0xF87A, 0xE075, 0x077C, 0xA4EB, 0xD208
};

/************************************************************************
			Helper Functions
************************************************************************/
/**
 * to_mt9m021 - A helper function which returns pointer to the
 * private data structure
 * @client: pointer to i2c client
 *
 */
static struct mt9m021_priv *to_mt9m021(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			struct mt9m021_priv, subdev);
}

/**
 * mt9m021_read - reads the data from the given register
 * @client: pointer to i2c client
 * @addr: address of the register which is to be read
 *
 */
static int mt9m021_read(struct i2c_client *client, u16 addr)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	u16 __addr;
	u16 ret;

	/* 16 bit addressable register */
	__addr = cpu_to_be16(addr);

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *)&__addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD; /* 1 */
	msg[1].len   = 2;
	msg[1].buf   = buf;

	/*
	* if return value of this function is < 0,
	* it means error.
	* else, under 16bit is valid data.
	*/
	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret < 0) {
		v4l_err(client, "Read from offset 0x%x error %d", addr, ret);
		return ret;
	}

	return (buf[0] << 8) | buf[1];
}

/**
 * mt9m021_write - writes the data into the given register
 * @client: pointer to i2c client
 * @addr: address of the register in which to write
 * @data: data to be written into the register
 *
 */
static int mt9m021_write(struct i2c_client *client, u16 addr,
				u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	u16 __addr, __data;
	int ret;

	/* 16-bit addressable register */

	__addr = cpu_to_be16(addr);
	__data = cpu_to_be16(data);

	buf[0] = __addr & 0xff;
	buf[1] = __addr >> 8;
	buf[2] = __data & 0xff;
	buf[3] = __data >> 8;
	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 4;
	msg.buf   = buf;

	/* i2c_transfer returns message length, but function should return 0 */
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0){
#ifdef MT9M021_I2C_DEBUG
		printk(KERN_INFO"mt9m021: REG=0x%04X, 0x%04X\n", addr,data);
#endif
		return 0;
	}

	v4l_err(client, "Write failed at 0x%x error %d\n", addr, ret);
	return ret;
}

/**
 * mt9m021_sequencer_settings
 * @client: pointer to the i2c client
 *
 */
static int mt9m021_sequencer_settings(struct i2c_client *client)
{
	int i, ret;
	
	ret = mt9m021_write(client, MT9M021_SEQ_CTRL_PORT, 0x8000);
	if (ret < 0)
		return ret;

	for(i = 0; i < ARRAY_SIZE(mt9m021_seq_data); i++){
		ret = mt9m021_write(client, MT9M021_SEQ_DATA_PORT, mt9m021_seq_data[i]);
		if (ret < 0)
			return ret;
	}

	return ret;	
}

/**
 * mt9m021_col_correction - retrigger column correction
 * @client: pointer to the i2c client
 *
 */
static int mt9m021_col_correction(struct i2c_client *client)
{
	int ret;

	/* Disable Streaming */
	ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_OFF);
	if (ret < 0)
		return ret;
	/* Disable column correction */
	ret = mt9m021_write(client, MT9M021_COLUMN_CORRECTION, 0x0000);
	if (ret < 0)
		return ret;

	msleep(200);

	/* Enable Streaming */
	ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_ON);
	if (ret < 0)
		return ret;

	msleep(200);

	/* Disable Streaming */
	ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_OFF);
	if (ret < 0)
		return ret;

	/* Enable column correction */
	ret = mt9m021_write(client, MT9M021_COLUMN_CORRECTION, 0x0001);
	if (ret < 0)
		return ret;

	msleep(200);

	return ret;
}

/**
 * mt9m021_rev2_settings
 * @client: pointer to the i2c client
 *
 */
static int mt9m021_rev2_settings(struct i2c_client *client)
{
	int ret;
	int i;

	ret = mt9m021_write(client, MT9M021_TEST_RAW_MODE, 0x0000);
	if (ret < 0)
		return ret;

	ret = mt9m021_write(client, 0x30EA, 0x0C00);
	if (ret < 0)
		return ret;

	ret = mt9m021_write(client, MT9M021_DARK_CTRL, 0x0404);
	if (ret < 0)
		return ret;

	ret = mt9m021_write(client, MT9M021_DATA_PEDESTAL, 0x012C);
	if (ret < 0)
		return ret;

	ret = mt9m021_write(client, 0x3180, 0x8000);
	if (ret < 0)
		return ret;

	ret = mt9m021_write(client, MT9M021_COLUMN_CORRECTION, 0xE007);
	if (ret < 0)
		return ret;
	
	ret = mt9m021_write(client, MT9M021_FINE_INT_TIME, 0x0000);
	if (ret < 0)
		return ret;

	for(i = 0; i < ARRAY_SIZE(mt9m021_analog_setting); i++){
		ret = mt9m021_write(client, MT9M021_ANALOG_REG + 2*i, mt9m021_analog_setting[i]);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/**
 * PLL Dividers
 *
 * Calculated according to the following formula:
 *
 *    target_freq = (ext_freq x M) / (N x P1 x P2)
 *    VCO_freq    = (ext_freq x M) / N
 *
 * And subject to the following limitations:
 *
 *    Limitations of PLL parameters
 *    -----------------------------
 *    32     ≤ M        ≤ 384
 *    1      ≤ N        ≤ 64
 *    1      ≤ P1       ≤ 16
 *    4      ≤ P2       ≤ 16
 *    384MHz ≤ VCO_freq ≤ 768MHz
 *
 */

static struct mt9m021_pll_divs mt9m021_divs[] = {
        /* ext_freq     target_freq     M       N       p1      p2 */
        {24000000,      48000000,       32,     2,      2,      4},
        {24000000,      66000000,       44,     2,      2,      4},
        {27000000,      74250000,       44,	2,	1,	8},
        {48000000,      48000000,       40,     5,      2,      4}
};

/**
 * mt9m021_pll_setup - enable the sensor pll
 * @client: pointer to the i2c client
 *
 */
static int mt9m021_pll_setup(struct i2c_client *client)
{
	int ret;
	int i;
	struct mt9m021_priv *mt9m021 = to_mt9m021(client);

	for (i = 0; i < ARRAY_SIZE(mt9m021_divs); i++) {
		if (mt9m021_divs[i].ext_freq == mt9m021->pdata->ext_freq &&
			mt9m021_divs[i].target_freq == mt9m021->pdata->target_freq) {
			mt9m021->pll = &mt9m021_divs[i];
			goto out;
		}
	}
	dev_err(&client->dev, "Couldn't find PLL dividers for ext_freq = %d, target_freq = %d\n",
			mt9m021->pdata->ext_freq, mt9m021->pdata->target_freq);
	return -EINVAL;

out:
#ifdef MT9M021_DEBUG
	printk(KERN_INFO"mt9m021: PLL settings:M = %d, N = %d, P1 = %d, P2 = %d",
        mt9m021->pll->m, mt9m021->pll->n, mt9m021->pll->p1, mt9m021->pll->p2);
#endif
	ret = mt9m021_write(client, MT9M021_VT_SYS_CLK_DIV, mt9m021->pll->p1);
	if (ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_VT_PIX_CLK_DIV, mt9m021->pll->p2);
	if (ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_PRE_PLL_CLK_DIV, mt9m021->pll->n);
	if (ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_PLL_MULTIPLIER, mt9m021->pll->m);
	if (ret < 0)
		return ret;

	if (mt9m021->pdata->version == MT9M021_COLOR_VERSION)
		ret = mt9m021_write(client, MT9M021_DIGITAL_TEST, 0x0000);
	else
		ret = mt9m021_write(client, MT9M021_DIGITAL_TEST, 0x0080);
		
	if (ret < 0)
		return ret;

	msleep(100);

	return ret;
}

/**
 * mt9m021_set_size - set the frame resolution
 * @client: pointer to the i2c client
 *
 */
static int mt9m021_set_size(struct i2c_client *client, struct mt9m021_frame_size *frame)
{
	struct mt9m021_priv *mt9m021 = to_mt9m021(client);
	int ret;
	int hratio;
	int vratio;


	hratio = DIV_ROUND_CLOSEST(mt9m021->crop.width, mt9m021->format.width);
	vratio = DIV_ROUND_CLOSEST(mt9m021->crop.height, mt9m021->format.height);
	if (hratio == 2) {
		if (vratio == 2) {
			ret = mt9m021_write(client, MT9M021_DIGITAL_BINNING,
				MT9M021_HOR_AND_VER_BIN);
			if (ret < 0)
				return ret;
#ifdef MT9M021_DEBUG
			printk(KERN_INFO"mt9m021: Horizontal and Vertical binning enabled\n");
#endif
		}
		else if (vratio < 2) {
			ret = mt9m021_write(client, MT9M021_DIGITAL_BINNING,
				MT9M021_HOR_BIN);
			if (ret < 0)
				return ret;
#ifdef MT9M021_DEBUG
			printk(KERN_INFO"mt9m021: Horizontal binning enabled\n");
#endif
		}
	}
	else {
		ret = mt9m021_write(client, MT9M021_DIGITAL_BINNING,
			MT9M021_DISABLE_BINNING);
		if (ret < 0)
			return ret;
#ifdef MT9M021_DEBUG
		printk(KERN_INFO"mt9m021: Binning disabled\n");
#endif
	}

	ret = mt9m021_write(client, MT9M021_Y_ADDR_START, mt9m021->crop.top);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_X_ADDR_START, mt9m021->crop.left);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_Y_ADDR_END, mt9m021->crop.top + mt9m021->crop.height - 1);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_X_ADDR_END, mt9m021->crop.left + mt9m021->crop.width - 1);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_FRAME_LENGTH_LINES, mt9m021->crop.height + 37);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_LINE_LENGTH_PCK, MT9M021_LLP_RECOMMENDED);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_COARSE_INT_TIME, 0x01C2);
	if(ret < 0)
		return ret;
	ret = mt9m021_write(client, MT9M021_X_ODD_INC, 0x0001);
	if(ret < 0)
		return ret;
	return mt9m021_write(client, MT9M021_Y_ODD_INC, 0x0001);
}

static int mt9m021_is_streaming(struct i2c_client *client)
{
	u16 streaming;

	streaming = mt9m021_read(client, MT9M021_RESET_REG);
	streaming = ( (streaming >> 2) & 0x0001);

	return (streaming != 0);
}

static int mt9m021_set_autoexposure( struct i2c_client *client, enum v4l2_exposure_auto_type ae_mode )

{
	struct mt9m021_priv *mt9m021 = to_mt9m021(client);
	int streaming;
	int ret = 0;

	/* Save the current streaming state. Used later to restore it */
	streaming = mt9m021_is_streaming(client);

	switch(ae_mode) {
	case V4L2_EXPOSURE_AUTO: /* Shutter and Apperture */
		dev_err(&client->dev, "Unsupported auto-exposure mode requested: %d\n", ae_mode);
		ret = -EINVAL;
		break;

	case V4L2_EXPOSURE_MANUAL:
		if (streaming) {
			ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_OFF);
			if (ret < 0);
				return ret;
		}

		ret = mt9m021_write(client, MT9M021_EMBEDDED_DATA_CTRL, 0x1802);
		if (ret < 0);
			return ret;
		ret = mt9m021_write(client, MT9M021_AE_CTRL_REG, 0x0000);
		if (ret < 0);
			return ret;
		if (streaming) {
			ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_ON);
			if (ret < 0);
				return ret;
		}
		break;

	case V4L2_EXPOSURE_SHUTTER_PRIORITY:
		if (streaming) {
			ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_OFF);
			if (ret < 0);
				return ret;
		}
		ret = mt9m021_write(client, MT9M021_EMBEDDED_DATA_CTRL, 0x1982);
		if (ret < 0);
			return ret;
		ret = mt9m021_write(client, MT9M021_AE_CTRL_REG, 0x0013);
		if (ret < 0);
			return ret;

		if (streaming) {
			ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_ON);
			if (ret < 0);
				return ret;

		}
		break;

	case V4L2_EXPOSURE_APERTURE_PRIORITY:
		dev_err(&client->dev, "Unsupported auto-exposure mode requested: %d\n", ae_mode);
		ret = -EINVAL;
		break;

	default:
		dev_err(&client->dev, "Auto Exposure mode out of range: %d\n", ae_mode);
		ret = -ERANGE;
		break;
	}
	if(ret == 0)
		mt9m021->autoexposure = ae_mode;

	return ret;
}

/**
 * mt9m021_power_on - power on the sensor
 * @mt9m021: pointer to private data structure
 *
 */
void mt9m021_power_on(struct mt9m021_priv *mt9m021)
{
	/* Ensure RESET_BAR is low */
	if (mt9m021->pdata->reset) {
		mt9m021->pdata->reset(&mt9m021->subdev, 1);
		msleep(1);
	}

	/* Enable clock */
	if (mt9m021->pdata->set_xclk) {
		mt9m021->pdata->set_xclk(&mt9m021->subdev,
		mt9m021->pdata->ext_freq);
		msleep(1);
	}

	/* Now RESET_BAR must be high */
	if (mt9m021->pdata->reset) {
		mt9m021->pdata->reset(&mt9m021->subdev, 0);
		msleep(1);
	}
}

/**
 * mt9m021_power_off - power off the sensor
 * @mt9m021: pointer to private data structure
 *
 */
void mt9m021_power_off(struct mt9m021_priv *mt9m021)
{
	if (mt9m021->pdata->set_xclk)
		mt9m021->pdata->set_xclk(&mt9m021->subdev, 0);
}

/************************************************************************
			v4l2_subdev_core_ops
************************************************************************/

static int mt9m021_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mt9m021_priv *mt9m021 = container_of(ctrl->handler,
					struct mt9m021_priv, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&mt9m021->subdev);
	u16 reg16;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_AUTO:
		ret = mt9m021_set_autoexposure(client, (enum v4l2_exposure_auto_type)ctrl->val);
		if(ret < 0)
			return ret;
		break;

	case V4L2_CID_EXPOSURE:
		ret = mt9m021_write(client, MT9M021_COARSE_INT_TIME, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_COARSE_INT_TIME_CB, ctrl->val);
		break;

	case V4L2_CID_GAIN:
		ret = mt9m021_write(client, MT9M021_GLOBAL_GAIN, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_GLOBAL_GAIN_CB, ctrl->val);
		break;

	case V4L2_CID_GAIN_GREEN1:
		ret = mt9m021_write(client, MT9M021_GREEN1_GAIN, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_GREEN1_GAIN_CB, ctrl->val);
		break;

	case V4L2_CID_GAIN_RED:
		ret = mt9m021_write(client, MT9M021_RED_GAIN, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_RED_GAIN_CB, ctrl->val);
		break;

	case V4L2_CID_GAIN_BLUE:
		ret = mt9m021_write(client, MT9M021_BLUE_GAIN, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_BLUE_GAIN_CB, ctrl->val);
		break;

	case V4L2_CID_GAIN_GREEN2:
		ret = mt9m021_write(client, MT9M021_GREEN2_GAIN, ctrl->val);
		if(ret < 0)
			return ret;
		return mt9m021_write(client, MT9M021_GREEN2_GAIN_CB, ctrl->val);
		break;

	case V4L2_CID_ANALOG_GAIN:
		reg16 = mt9m021_read(client, MT9M021_DIGITAL_TEST);
		reg16 = ( reg16 & ~MT9M021_ANALOG_GAIN_MASK ) | 
			( ( ctrl->val << MT9M021_ANALOG_GAIN_SHIFT ) & MT9M021_ANALOG_GAIN_MASK );
		return mt9m021_write(client, MT9M021_DIGITAL_TEST, reg16);
		break;

	case V4L2_CID_HFLIP:
		reg16 = mt9m021_read(client, MT9M021_READ_MODE);
		if (ctrl->val){
			reg16 |= 0x4000;
			ret = mt9m021_write(client, MT9M021_READ_MODE, reg16);
			if (ret < 0)
				return ret;

			break;
		}
		reg16 &= 0xbfff;
		ret = mt9m021_write(client, MT9M021_READ_MODE, reg16);
		if (ret < 0)
			return ret;
		break;
	
	case V4L2_CID_VFLIP:
		reg16 = mt9m021_read(client, MT9M021_READ_MODE);
		if (ctrl->val) {
			reg16 |= 0x8000;
			ret = mt9m021_write(client, MT9M021_READ_MODE, reg16);
			if (ret < 0)
				return ret;
			break;
		}
		reg16 &= 0x7fff;
		ret = mt9m021_write(client, MT9M021_READ_MODE, reg16);
		if (ret < 0)
			return ret;
		break;

	case V4L2_CID_TEST_PATTERN:
		if (!ctrl->val){
			ret = mt9m021_write(client, MT9M021_TEST_PATTERN, 0x0000);
			if(ret < 0)
				return ret;
		}
		ret = mt9m021_write(client, MT9M021_TEST_PATTERN, ctrl->val);
		if(ret < 0)
			return ret;
		break;
	}
	return ret;
}

static struct v4l2_ctrl_ops mt9m021_ctrl_ops = {
	.s_ctrl = mt9m021_s_ctrl,
};

/*
MT9M021_TEST_PATTERN
0 = Disabled. Normal operation. Generate output data from pixel array
1 = Solid color test pattern",
2 = color bar test pattern",
3 = Fade to gray color bar test pattern",
256 = Walking 1s test pattern (12 bit)"
*/
static const char * const mt9m021_test_pattern_menu[] = {
	"0:Disabled",
	"1:Solid color test pattern",
	"2:color bar test pattern",
	"3:Fade to gray color bar test pattern",
	"256:Walking 1s test pattern (12 bit)"
};

struct mt9m021_control {
	u32 id;
	s32 min;
	s32 max;
	u32 step;
	s32 def;
};

static const struct mt9m021_control mt9m021_standard_ctrls[] = {
	{ V4L2_CID_GAIN,     MT9M021_GLOBAL_GAIN_MIN, MT9M021_GLOBAL_GAIN_MAX, 1, MT9M021_GLOBAL_GAIN_DEF },
	{ V4L2_CID_EXPOSURE, MT9M021_EXPOSURE_MIN,    MT9M021_EXPOSURE_MAX,    1, MT9M021_EXPOSURE_DEF    },
	{ V4L2_CID_HFLIP,    0, 1, 1, 0 },
	{ V4L2_CID_VFLIP,    0, 1, 1, 0 },
};

static const struct v4l2_ctrl_config mt9m021_custom_ctrls[] = {
	{
		.ops            = &mt9m021_ctrl_ops,
		.id             = V4L2_CID_TEST_PATTERN,
		.type           = V4L2_CTRL_TYPE_MENU,
		.name           = "Test Pattern",
		.min            = 0,
		.max            = ARRAY_SIZE(mt9m021_test_pattern_menu) - 1,
		.step           = 0,
		.def            = 0,
		.flags          = 0,
		.menu_skip_mask = 0,
		.qmenu          = mt9m021_test_pattern_menu,
	}, {
                .ops            = &mt9m021_ctrl_ops,
                .id             = V4L2_CID_GAIN_GREEN1,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Green (R)",
                .min            = MT9M021_GLOBAL_GAIN_MIN,
                .max            = MT9M021_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M021_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m021_ctrl_ops,
                .id             = V4L2_CID_GAIN_RED,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Red",
                .min            = MT9M021_GLOBAL_GAIN_MIN,
                .max            = MT9M021_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M021_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m021_ctrl_ops,
                .id             = V4L2_CID_GAIN_BLUE,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Blue",
                .min            = MT9M021_GLOBAL_GAIN_MIN,
                .max            = MT9M021_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M021_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {

                .ops            = &mt9m021_ctrl_ops,
                .id             = V4L2_CID_GAIN_GREEN2,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Green (B)",
                .min            = MT9M021_GLOBAL_GAIN_MIN,
                .max            = MT9M021_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M021_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m021_ctrl_ops,
                .id             = V4L2_CID_ANALOG_GAIN,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Column",
                .min            = MT9M021_ANALOG_GAIN_MIN,
                .max            = MT9M021_ANALOG_GAIN_MAX,
                .step           = 1,
                .def            = MT9M021_ANALOG_GAIN_DEF,
                .flags          = 0,
	}
};

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9m021_g_reg(struct v4l2_subdev *sd,
				struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int data;

	reg->size = 2;
	data = mt9m021_read(client, reg->reg);
	if (data < 0)
		return data;

	reg->val = data;
	return 0;
}

static int mt9m021_s_reg(struct v4l2_subdev *sd,
				struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return mt9m021_write(client, reg->reg, reg->val);
}
#endif

static int mt9m021_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m021_priv *mt9m021 = container_of(sd,
				struct mt9m021_priv, subdev);
	int ret = 0;

	mutex_lock(&mt9m021->power_lock);

	/*
	* If the power count is modified from 0 to != 0 or from != 0 to 0,
	* update the power state.
	*/
	if (mt9m021->power_count == !on) {
		if (on) {
				mt9m021_power_on(mt9m021);
				ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_RESET);
				if (ret < 0) {
					dev_err(mt9m021->subdev.v4l2_dev->dev,
					"Failed to reset the camera\n");
					goto out;
				}
				ret = v4l2_ctrl_handler_setup(&mt9m021->ctrls);
				if (ret < 0)
					goto out;
		} else
			mt9m021_power_off(mt9m021);
	}
	/* Update the power count. */
	mt9m021->power_count += on ? 1 : -1;
	WARN_ON(mt9m021->power_count < 0);
out:
	mutex_unlock(&mt9m021->power_lock);
	return ret;
}

/***************************************************
		v4l2_subdev_video_ops
****************************************************/


static int mt9m021_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m021_frame_size frame;
	int ret;

	if (!enable)
		return mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_OFF);
	
	/* soft reset */
/*	ret = mt9m021_write(client, MT9M021_RESET_REG, MT9M021_RESET);
	if(ret < 0)
		return ret;

	msleep(200);
*/
	ret = mt9m021_sequencer_settings(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup sequencer\n",__func__);
		return ret;
	}

	ret = mt9m021_col_correction(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup column correction\n",__func__);
		return ret;
	}

	ret = mt9m021_rev2_settings(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup Rev2 optimised settings\n",__func__);
		return ret;
	}

	ret = mt9m021_pll_setup(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup pll\n",__func__);
		return ret;
	}

	ret = mt9m021_set_size(client, &frame);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup resolution\n",__func__);
		return ret;
	}
	
	/* start streaming */
	return mt9m021_write(client, MT9M021_RESET_REG, MT9M021_STREAM_ON);

}


/***************************************************
		v4l2_subdev_pad_ops
****************************************************/
static int mt9m021_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);

	if (code->pad || code->index)
		return -EINVAL;

	code->code = mt9m021->format.code;
	return 0;
}

static int mt9m021_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);

	if (fse->index != 0 || fse->code != mt9m021->format.code)
		return -EINVAL;

	fse->min_width = MT9M021_WINDOW_WIDTH_MIN;
	fse->max_width = MT9M021_WINDOW_WIDTH_MAX;
	fse->min_height = MT9M021_WINDOW_HEIGHT_MIN;
	fse->max_height = MT9M021_WINDOW_HEIGHT_MAX;

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9m021_get_pad_format(struct mt9m021_priv *mt9m021, struct v4l2_subdev_fh *fh,
			unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &mt9m021->format;
	default:
		return NULL;
	}
}

static struct v4l2_rect *
__mt9m021_get_pad_crop(struct mt9m021_priv *mt9m021, struct v4l2_subdev_fh *fh,
	unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &mt9m021->crop;
	default:
		return NULL;
	}
}

static int mt9m021_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);

	fmt->format = *__mt9m021_get_pad_format(mt9m021, fh, fmt->pad,
						fmt->which);

	return 0;
}

static int mt9m021_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *format)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);
	struct mt9m021_frame_size size;
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	unsigned int wratio;
	unsigned int hratio;

	__crop = __mt9m021_get_pad_crop(mt9m021, fh, format->pad,
		format->which);
	/* Clamp the width and height to avoid dividing by zero. */
	size.width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
			MT9M021_WINDOW_WIDTH_MIN,
			MT9M021_WINDOW_WIDTH_MAX);
	size.height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
			MT9M021_WINDOW_HEIGHT_MIN,
			MT9M021_WINDOW_WIDTH_MAX);

	wratio = DIV_ROUND_CLOSEST(__crop->width, size.width);
	hratio = DIV_ROUND_CLOSEST(__crop->height, size.height);

	__format = __mt9m021_get_pad_format(mt9m021, fh, format->pad,
						format->which);
	__format->width = __crop->width / wratio;
	__format->height = __crop->height / hratio;

	printk(KERN_INFO"mt9m021: crop = %dx%d format = %dx%d\n",
	__crop->width, __crop->height, __format->width, __format->height);

	format->format = *__format;
	mt9m021->format.width	= format->format.width;
	mt9m021->format.height	= format->format.height;
	mt9m021->format.code	= V4L2_MBUS_FMT_SGRBG12_1X12;

	return 0;
}

static int mt9m021_get_crop(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_crop *crop)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);

	crop->rect = *__mt9m021_get_pad_crop(mt9m021, fh, crop->pad, crop->which);

	return 0;
}

static int mt9m021_set_crop(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_crop *crop)
{
	struct mt9m021_priv *mt9m021 = container_of(sd,
					struct mt9m021_priv, subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	struct v4l2_rect rect;

	/* Clamp the crop rectangle boundaries and align them to a multiple of 2
	* pixels to ensure a GRBG Bayer pattern.
	*/
	rect.left = clamp(ALIGN(crop->rect.left, 2), MT9M021_COLUMN_START_MIN,
			MT9M021_COLUMN_START_MAX);
	rect.top = clamp(ALIGN(crop->rect.top, 2), MT9M021_ROW_START_MIN,
			MT9M021_ROW_START_MAX);
	rect.width = clamp(ALIGN(crop->rect.width, 2),
			MT9M021_WINDOW_WIDTH_MIN,
			MT9M021_WINDOW_WIDTH_MAX);
	rect.height = clamp(ALIGN(crop->rect.height, 2),
			MT9M021_WINDOW_HEIGHT_MIN,
			MT9M021_WINDOW_HEIGHT_MAX);

	rect.width = min(rect.width, MT9M021_PIXEL_ARRAY_WIDTH - rect.left);
	rect.height = min(rect.height, MT9M021_PIXEL_ARRAY_HEIGHT - rect.top);

	__crop = __mt9m021_get_pad_crop(mt9m021, fh, crop->pad, crop->which);

	/* Reset the output image size if the crop rectangle size has
	* been modified.
	*/
	if (rect.width != __crop->width || rect.height != __crop->height) {
		__format = __mt9m021_get_pad_format(mt9m021, fh, crop->pad,
								crop->which);
		__format->width = rect.width;
		__format->height = rect.height;
	}

	*__crop = rect;
	crop->rect = rect;

	mt9m021->crop.left	= crop->rect.left;
	mt9m021->crop.top	= crop->rect.top;
	mt9m021->crop.width	= crop->rect.width;
	mt9m021->crop.height	= crop->rect.height;

	return 0;
}

/***********************************************************
	V4L2 subdev internal operations
************************************************************/
static int mt9m021_registered(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m021_priv *mt9m021 = to_mt9m021(client);
	s32 data;
	int count = 0;

	mt9m021_power_on(mt9m021);

	/* Read out the chip version register */
	data = mt9m021_read(client, MT9M021_CHIP_ID_REG);
	if (data != MT9M021_CHIP_ID) {
		while(count++ < 5){
			data = mt9m021_read(client, MT9M021_CHIP_ID_REG);
			msleep(5);
		}
		dev_err(&client->dev, "Aptina MT9M021 not detected, chip ID read:0x%4.4x\n",
				data);
		return -ENODEV;
	}
	dev_info(&client->dev, "Aptina MT9M021 detected at address 0x%02x\n", client->addr);

	mt9m021_power_off(mt9m021);

	return 0;
}

static int mt9m021_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return mt9m021_s_power(sd, 1);
}

static int mt9m021_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return mt9m021_s_power(sd, 0);
}

/***************************************************
		v4l2_subdev_ops
****************************************************/
static struct v4l2_subdev_core_ops mt9m021_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9m021_g_reg,
	.s_register	= mt9m021_s_reg,
#endif
	.s_power	= mt9m021_s_power,
};

static struct v4l2_subdev_video_ops mt9m021_subdev_video_ops = {
	.s_stream	= mt9m021_s_stream,
};

static struct v4l2_subdev_pad_ops mt9m021_subdev_pad_ops = {
	.enum_mbus_code	 = mt9m021_enum_mbus_code,
	.enum_frame_size = mt9m021_enum_frame_size,
	.get_fmt	 = mt9m021_get_format,
	.set_fmt	 = mt9m021_set_format,
	.get_crop	 = mt9m021_get_crop,
	.set_crop	 = mt9m021_set_crop,
};

static struct v4l2_subdev_ops mt9m021_subdev_ops = {
	.core	= &mt9m021_subdev_core_ops,
	.video	= &mt9m021_subdev_video_ops,
	.pad	= &mt9m021_subdev_pad_ops,
};

/*
 * Internal ops. Never call this from drivers, only the v4l2 framework can call
 * these ops.
 */
static const struct v4l2_subdev_internal_ops mt9m021_subdev_internal_ops = {
	.registered	= mt9m021_registered,
	.open		= mt9m021_open,
	.close		= mt9m021_close,
};

/***************************************************
		I2C driver
****************************************************/
static int mt9m021_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct mt9m021_platform_data *pdata = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mt9m021_priv *mt9m021;
	int ret;
	int i;

	if (pdata == NULL) {
		dev_err(&client->dev, "No platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&client->dev, "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	mt9m021 = devm_kzalloc(&client->dev, sizeof(struct mt9m021_priv),
				GFP_KERNEL);
	if (mt9m021 == NULL)
		return -ENOMEM;

	mt9m021->pdata = pdata;

	v4l2_ctrl_handler_init(&mt9m021->ctrls, ARRAY_SIZE(mt9m021_standard_ctrls) + 
					ARRAY_SIZE(mt9m021_custom_ctrls) + 1);
	
	v4l2_ctrl_new_std_menu(&mt9m021->ctrls, &mt9m021_ctrl_ops,
		V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_APERTURE_PRIORITY, 0,
		V4L2_EXPOSURE_MANUAL);

	for (i = 0; i < ARRAY_SIZE(mt9m021_standard_ctrls); ++i ) {
		const struct mt9m021_control *ctrl = &mt9m021_standard_ctrls[i];
	
		v4l2_ctrl_new_std(&mt9m021->ctrls, &mt9m021_ctrl_ops,
				ctrl->id, ctrl->min, ctrl->max, ctrl->step, ctrl->def);
	}

	for (i = 0; i < ARRAY_SIZE(mt9m021_custom_ctrls); i++){
		v4l2_ctrl_new_custom(&mt9m021->ctrls, &mt9m021_custom_ctrls[i], NULL);
	}
	mt9m021->subdev.ctrl_handler = &mt9m021->ctrls;

	if (mt9m021->ctrls.error) {
		ret = mt9m021->ctrls.error;
		dev_err(&client->dev, "Control initialization error: %d\n",
			ret);
		goto done;
	}

	mutex_init(&mt9m021->power_lock);
	v4l2_i2c_subdev_init(&mt9m021->subdev, client, &mt9m021_subdev_ops);
	mt9m021->subdev.internal_ops = &mt9m021_subdev_internal_ops;
	mt9m021->subdev.ctrl_handler = &mt9m021->ctrls;

	mt9m021->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&mt9m021->subdev.entity, 1, &mt9m021->pad, 0);
	if (ret < 0)
		goto done;

	mt9m021->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	mt9m021->crop.width	= MT9M021_WINDOW_WIDTH_MAX;
	mt9m021->crop.height	= MT9M021_WINDOW_HEIGHT_MAX;
	mt9m021->crop.left	= MT9M021_COLUMN_START_DEF;
	mt9m021->crop.top	= MT9M021_ROW_START_DEF;

	mt9m021->format.code		= V4L2_MBUS_FMT_SGRBG12_1X12;
	mt9m021->format.width		= MT9M021_WINDOW_WIDTH_DEF;
	mt9m021->format.height		= MT9M021_WINDOW_HEIGHT_DEF;
	mt9m021->format.field		= V4L2_FIELD_NONE;
	mt9m021->format.colorspace	= V4L2_COLORSPACE_SRGB;

done:
	if (ret < 0) {
		v4l2_ctrl_handler_free(&mt9m021->ctrls);
		media_entity_cleanup(&mt9m021->subdev.entity);
		dev_err(&client->dev, "Probe failed\n");
	}

	return ret;
}

static int mt9m021_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct mt9m021_priv *mt9m021 = to_mt9m021(client);

	v4l2_ctrl_handler_free(&mt9m021->ctrls);
	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	return 0;
}

static const struct i2c_device_id mt9m021_id[] = {
	{ "mt9m021", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9m021_id);

static struct i2c_driver mt9m021_i2c_driver = {
	.driver = {
		 .name = "mt9m021",
	},
	.probe    = mt9m021_probe,
	.remove   = mt9m021_remove,
	.id_table = mt9m021_id,
};

/* module_i2c_driver(mt9m021_i2c_driver); */

static int __init mt9m021_module_init(void)
{
	return i2c_add_driver(&mt9m021_i2c_driver);
}

static void __exit mt9m021_module_exit(void)
{
	i2c_del_driver(&mt9m021_i2c_driver);
}
module_init(mt9m021_module_init);
module_exit(mt9m021_module_exit);

MODULE_DESCRIPTION("Aptina MT9M021 Camera driver");
MODULE_AUTHOR("Aptina Imaging <drivers@aptina.com>");
MODULE_LICENSE("GPL v2");
