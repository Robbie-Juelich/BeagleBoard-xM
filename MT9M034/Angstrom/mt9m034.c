/*
 *
 * Aptina MT9M034 sensor driver
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

#include <media/mt9m034.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define MT9M034_PIXEL_ARRAY_WIDTH	1280
#define MT9M034_PIXEL_ARRAY_HEIGHT	960

#define	MT9M034_ROW_START_MIN		0
#define	MT9M034_ROW_START_MAX		960
#define	MT9M034_ROW_START_DEF		0
#define	MT9M034_COLUMN_START_MIN	0
#define	MT9M034_COLUMN_START_MAX	1280
#define	MT9M034_COLUMN_START_DEF	0
#define	MT9M034_WINDOW_HEIGHT_MIN	2
#define	MT9M034_WINDOW_HEIGHT_MAX	960
#define	MT9M034_WINDOW_HEIGHT_DEF	960
#define	MT9M034_WINDOW_WIDTH_MIN	2
#define	MT9M034_WINDOW_WIDTH_MAX	1280
#define	MT9M034_WINDOW_WIDTH_DEF	1280
#define MT9M034_ENABLE			1
#define MT9M034_DISABLE			0

#define MT9M034_CHIP_ID_REG		0x3000
#define MT9M034_CHIP_ID			0x2400

#define MT9M034_RESET_REG		0x301A
#define MT9M034_SEQ_CTRL_PORT		0x3088
#define MT9M034_SEQ_DATA_PORT		0x3086
#define MT9M034_ANALOG_REG		0x3ED6
#define MT9M034_TEST_RAW_MODE		0x307A
#define MT9M034_DARK_CTRL		0x3044
#define MT9M034_DATA_PEDESTAL		0x301E
#define MT9M034_COLUMN_CORRECTION	0x30D4

#define MT9M034_VT_SYS_CLK_DIV		0x302A
#define MT9M034_VT_PIX_CLK_DIV		0x302C
#define MT9M034_PRE_PLL_CLK_DIV		0x302E
#define MT9M034_PLL_MULTIPLIER		0x3030
#define MT9M034_DIGITAL_TEST		0x30B0

#define	MT9M034_Y_ADDR_START		0x3002
#define	MT9M034_X_ADDR_START		0x3004
#define	MT9M034_Y_ADDR_END		0x3006
#define	MT9M034_X_ADDR_END		0x3008
#define	MT9M034_FRAME_LENGTH_LINES	0x300A
#define	MT9M034_LINE_LENGTH_PCK		0x300C
#define	MT9M034_COARSE_INT_TIME		0x3012
#define MT9M034_FINE_INT_TIME		0x3014
#define	MT9M034_COARSE_INT_TIME_CB	0x3016
#define MT9M034_FINE_INT_TIME_CB	0x3018
#define	MT9M034_X_ODD_INC		0x30A2
#define	MT9M034_Y_ODD_INC		0x30A6
#define	MT9M034_READ_MODE		0x3040
#define MT9M034_TEST_PATTERN		0x3070
#define MT9M034_LLP_RECOMMENDED		1650
#define MT9M034_DIGITAL_BINNING		0x3032
#define MT9M034_HOR_AND_VER_BIN		0x0022
#define MT9M034_HOR_BIN			0x0011
#define MT9M034_DISABLE_BINNING		0x0000

#define MT9M034_AE_CTRL_REG		0x3100

#define MT9M034_GREEN1_GAIN		0x3056
#define MT9M034_BLUE_GAIN		0x3058
#define MT9M034_RED_GAIN		0x305A
#define MT9M034_GREEN2_GAIN		0x305C
#define MT9M034_GLOBAL_GAIN		0x305E
#define MT9M034_GREEN1_GAIN_CB		0x30BC
#define MT9M034_BLUE_GAIN_CB		0x30BE
#define MT9M034_RED_GAIN_CB		0x30C0
#define MT9M034_GREEN2_GAIN_CB		0x30C2
#define MT9M034_GLOBAL_GAIN_CB		0x30C4

#define MT9M034_RESET_REGISTER		0x301A
#define MT9M034_RESET			0x00D9
#define MT9M034_STREAM_OFF		0x00D8
#define MT9M034_STREAM_ON		0x00DC

#define MT9M034_ERS_PROG_START_ADDR	0x309E
#define MT9M034_MODE_CTRL		0x3082

#define MT9M034_DAC_LD_14_15		0x3EDA
#define MT9M034_DAC_LD_18_19		0x3EDE
#define MT9M034_DAC_LD_12_13		0x3ED8
#define MT9M034_DAC_LD_22_23		0x3EE2
#define MT9M034_DAC_LD_20_21		0x3EE0
#define MT9M034_DAC_LD_16_17		0x3EDC
#define MT9M034_DARK_CONTROL		0x3044
#define MT9M034_DAC_LD_26_27		0x3EE6
#define MT9M034_DAC_LD_24_25		0x3EE4
#define MT9M034_DAC_LD_10_11		0x3ED6
#define MT9M034_ADC_BITS_6_7		0x30E4
#define MT9M034_ADC_BITS_4_5		0x30E2
#define MT9M034_ADC_BITS_2_3		0x30E0
#define MT9M034_ADC_CONFIG1		0x30E6
#define MT9M034_ADC_CONFIG2		0x30E8
#define MT9M034_DIGITAL_CTRL		0x30BA
#define MT9M034_COARSE_INTEGRATION_TIME		0x3012
#define MT9M034_HDR_COMP			0x31D0

#define MT9M034_AE_DCG_EXPOSURE_HIGH_REG	0x3112
#define MT9M034_AE_DCG_EXPOSURE_LOW_REG		0x3114
#define MT9M034_AE_DCG_GAIN_FACTOR_REG		0x3116
#define MT9M034_AE_DCG_GAIN_FACTOR_INV_REG	0x3118
#define MT9M034_AE_LUMA_TARGET_REG		0x3102
#define MT9M034_AE_HIST_TARGET_REG		0x3104
#define MT9M034_AE_ALPHA_V1_REG			0x3126
#define MT9M034_AE_MAX_EXPOSURE_REG		0x311C
#define MT9M034_AE_MIN_EXPOSURE_REG		0x311E
#define MT9M034_EMBEDDED_DATA_CTRL		0x3064

#define V4L2_CID_TEST_PATTERN           (V4L2_CID_USER_BASE | 0x1001)
#define V4L2_CID_GAIN_RED		(V4L2_CID_USER_BASE | 0x1002)
#define V4L2_CID_GAIN_GREEN1		(V4L2_CID_USER_BASE | 0x1003)
#define V4L2_CID_GAIN_GREEN2		(V4L2_CID_USER_BASE | 0x1004)
#define V4L2_CID_GAIN_BLUE		(V4L2_CID_USER_BASE | 0x1005)
#define V4L2_CID_ANALOG_GAIN		(V4L2_CID_USER_BASE | 0x1006)

#define MT9M034_ANALOG_GAIN_MIN		0x0
#define MT9M034_ANALOG_GAIN_MAX		0x3
#define MT9M034_ANALOG_GAIN_DEF		0x0
#define MT9M034_ANALOG_GAIN_SHIFT	4
#define MT9M034_ANALOG_GAIN_MASK	0x0030

#define MT9M034_GLOBAL_GAIN_MIN		0x00
#define MT9M034_GLOBAL_GAIN_MAX		0xFF
#define MT9M034_GLOBAL_GAIN_DEF		0x20

#define MT9M034_EXPOSURE_MIN		1
#define MT9M034_EXPOSURE_MAX		0x02A0
#define MT9M034_EXPOSURE_DEF		0x0100


#undef MT9M034_I2C_DEBUG
#undef MT9M034_DEBUG

#define MT9M034_WRITE(ret, client, addr, data)	(ret) = __mt9m034_write((client), (addr), (data)); \
						if ((ret) < 0)	{ \
							return ((ret)); \
						}

struct mt9m034_frame_size {
	u16 width;
	u16 height;
};

struct mt9m034_pll_divs {
        u32 ext_freq;
        u32 target_freq;
        u16 m;
        u16 n;
        u16 p1;
        u16 p2;
};

struct mt9m034_priv {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_rect crop;  /* Sensor window */
	struct v4l2_mbus_framefmt format;
	struct v4l2_ctrl_handler ctrls;
	struct mt9m034_platform_data *pdata;
	struct mutex power_lock; /* lock to protect power_count */
	struct mt9m034_pll_divs *pll;
	int power_count;
	enum v4l2_exposure_auto_type autoexposure;
};

static unsigned int mt9m034_seq_data[] = {
	0x0025, 0x5050, 0x2D26, 0x0828, 0x0D17, 0x0926, 0x0028, 0x0526,
	0xA728, 0x0725, 0x8080, 0x2925, 0x0040, 0x2702, 0x1616, 0x2706,
	0x1F17, 0x3626, 0xA617, 0x0326, 0xA417, 0x1F28, 0x0526, 0x2028,
	0x0425, 0x2020, 0x2700, 0x171D, 0x2500, 0x2017, 0x1219, 0x1703,
	0x2706, 0x1728, 0x2805, 0x171A, 0x2660, 0x175A, 0x2317, 0x1122,
	0x1741, 0x2500, 0x9027, 0x0026, 0x1828, 0x002E, 0x2A28, 0x081C,
	0x1470, 0x7003, 0x1470, 0x7004, 0x1470, 0x7005, 0x1470, 0x7009,
	0x170C, 0x0014, 0x0020, 0x0014, 0x0050, 0x0314, 0x0020, 0x0314,
	0x0050, 0x0414, 0x0020, 0x0414, 0x0050, 0x0514, 0x0020, 0x2405,
	0x1400, 0x5001, 0x2550, 0x502D, 0x2608, 0x280D, 0x1709, 0x2600,
	0x2805, 0x26A7, 0x2807, 0x2580, 0x8029, 0x2500, 0x4027, 0x0216,
	0x1627, 0x0620, 0x1736, 0x26A6, 0x1703, 0x26A4, 0x171F, 0x2805,
	0x2620, 0x2804, 0x2520, 0x2027, 0x0017, 0x1D25, 0x0020, 0x1712,
	0x1A17, 0x0327, 0x0617, 0x2828, 0x0517, 0x1A26, 0x6017, 0xAE25,
	0x0090, 0x2700, 0x2618, 0x2800, 0x2E2A, 0x2808, 0x1D05, 0x1470,
	0x7009, 0x1720, 0x1400, 0x2024, 0x1400, 0x5002, 0x2550, 0x502D,
	0x2608, 0x280D, 0x1709, 0x2600, 0x2805, 0x26A7, 0x2807, 0x2580,
	0x8029, 0x2500, 0x4027, 0x0216, 0x1627, 0x0617, 0x3626, 0xA617,
	0x0326, 0xA417, 0x1F28, 0x0526, 0x2028, 0x0425, 0x2020, 0x2700,
	0x171D, 0x2500, 0x2021, 0x1712, 0x1B17, 0x0327, 0x0617, 0x2828,
	0x0517, 0x1A26, 0x6017, 0xAE25, 0x0090, 0x2700, 0x2618, 0x2800,
	0x2E2A, 0x2808, 0x1E17, 0x0A05, 0x1470, 0x7009, 0x1616, 0x1616,
	0x1616, 0x1616, 0x1616, 0x1616, 0x1616, 0x1616, 0x1616, 0x1616,
	0x1616, 0x1616, 0x1616, 0x1614, 0x0020, 0x2414, 0x0050, 0x2B2B,
	0x2C2C, 0x2C2C, 0x2C00, 0x0225, 0x5050, 0x2D26, 0x0828, 0x0D17,
	0x0926, 0x0028, 0x0526, 0xA728, 0x0725, 0x8080, 0x2917, 0x0525,
	0x0040, 0x2702, 0x1616, 0x2706, 0x1736, 0x26A6, 0x1703, 0x26A4,
	0x171F, 0x2805, 0x2620, 0x2804, 0x2520, 0x2027, 0x0017, 0x1E25,
	0x0020, 0x2117, 0x1028, 0x051B, 0x1703, 0x2706, 0x1703, 0x1747,
	0x2660, 0x17AE, 0x2500, 0x9027, 0x0026, 0x1828, 0x002E, 0x2A28,
	0x081E, 0x0831, 0x1440, 0x4014, 0x2020, 0x1410, 0x1034, 0x1400,
	0x1014, 0x0020, 0x1400, 0x4013, 0x1802, 0x1470, 0x7004, 0x1470,
	0x7003, 0x1470, 0x7017, 0x2002, 0x1400, 0x2002, 0x1400, 0x5004,
	0x1400, 0x2004, 0x1400, 0x5022, 0x0314, 0x0020, 0x0314, 0x0050,
	0x2C2C, 0x2C2C 
};

/************************************************************************
			Helper Functions
************************************************************************/
/**
 * to_mt9m034 - A helper function which returns pointer to the
 * private data structure
 * @client: pointer to i2c client
 *
 */
static struct mt9m034_priv *to_mt9m034(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			struct mt9m034_priv, subdev);
}

/**
 * mt9m034_read - reads the data from the given register
 * @client: pointer to i2c client
 * @addr: address of the register which is to be read
 *
 */
static int mt9m034_read(struct i2c_client *client, u16 addr)
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
 * __mt9m034_write - writes the data into the given register
 * @client: pointer to i2c client
 * @addr: address of the register in which to write
 * @data: data to be written into the register
 *
 */
static int __mt9m034_write(struct i2c_client *client, u16 addr,
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
#ifdef MT9M034_I2C_DEBUG
		printk(KERN_INFO"mt9m034: REG=0x%04X, 0x%04X\n", addr,data);
#endif
		return 0;
	}

	v4l_err(client, "Write failed at 0x%x error %d\n", addr, ret);
	return ret;
}

/**
 * mt9m034_sequencer_settings
 * @client: pointer to the i2c client
 *
 */
static int mt9m034_sequencer_settings(struct i2c_client *client)
{
	int i, ret;
	
	MT9M034_WRITE(ret, client, MT9M034_SEQ_CTRL_PORT, 0x8000)

	for(i = 0; i < ARRAY_SIZE(mt9m034_seq_data); i++){
		MT9M034_WRITE(ret, client, MT9M034_SEQ_DATA_PORT, mt9m034_seq_data[i])
	}

	MT9M034_WRITE(ret, client, MT9M034_ERS_PROG_START_ADDR, 0x0186)

	return ret;
}

/**
 * mt9m034_linear_mode_setup - retrigger column correction
 * @client: pointer to the i2c client
 *
 */
static int mt9m034_linear_mode_setup(struct i2c_client *client)
{
	int ret;

	/* Disable Streaming */
	MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_OFF)

	/* Operation mode control */
	MT9M034_WRITE(ret, client, MT9M034_MODE_CTRL, 0x0029)
	MT9M034_WRITE(ret, client, MT9M034_DATA_PEDESTAL, 0x00C8)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_14_15, 0x0F03)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_18_19, 0xC005)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_12_13, 0x09EF)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_22_23, 0xA46B)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_20_21, 0x067D)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_16_17, 0x0070)
	MT9M034_WRITE(ret, client, MT9M034_DARK_CONTROL, 0x0404)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_26_27, 0x8303)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_24_25, 0xD208)
	MT9M034_WRITE(ret, client, MT9M034_DAC_LD_10_11, 0x00BD)
	MT9M034_WRITE(ret, client, MT9M034_ADC_BITS_6_7, 0x6372)
	MT9M034_WRITE(ret, client, MT9M034_ADC_BITS_4_5, 0x7253)
	MT9M034_WRITE(ret, client, MT9M034_ADC_BITS_2_3, 0x5470)
	MT9M034_WRITE(ret, client, MT9M034_ADC_CONFIG1, 0xC4CC)
	MT9M034_WRITE(ret, client, MT9M034_ADC_CONFIG2, 0x8050)
	MT9M034_WRITE(ret, client, MT9M034_DIGITAL_TEST, 0x1300)
	MT9M034_WRITE(ret, client, MT9M034_COLUMN_CORRECTION, 0xE007)
	MT9M034_WRITE(ret, client, MT9M034_DIGITAL_CTRL, 0x0008)
	MT9M034_WRITE(ret, client, MT9M034_RESET_REGISTER, 0x10DC)
	MT9M034_WRITE(ret, client, MT9M034_RESET_REGISTER, 0x10D8)
	MT9M034_WRITE(ret, client, MT9M034_BLUE_GAIN, 0x003F)
	MT9M034_WRITE(ret, client, MT9M034_COARSE_INTEGRATION_TIME, 0x02A0)

	msleep(200);

	return ret;
}

/**
 * mt9m034_set_size - set the frame resolution
 * @client: pointer to the i2c client
 *
 */
static int mt9m034_set_size(struct i2c_client *client, struct mt9m034_frame_size *frame)
{
	struct mt9m034_priv *mt9m034 = to_mt9m034(client);
	int ret;
	int hratio;
	int vratio;


	hratio = DIV_ROUND_CLOSEST(mt9m034->crop.width, mt9m034->format.width);
	vratio = DIV_ROUND_CLOSEST(mt9m034->crop.height, mt9m034->format.height);
	if (hratio == 2) {
		if (vratio == 2) {
			MT9M034_WRITE(ret, client, MT9M034_DIGITAL_BINNING,
				MT9M034_HOR_AND_VER_BIN)
#ifdef MT9M034_DEBUG
			printk(KERN_INFO"mt9m034: Horizontal and Vertical binning enabled\n");
#endif
		}
		else if (vratio < 2) {
			MT9M034_WRITE(ret, client, MT9M034_DIGITAL_BINNING,
				MT9M034_HOR_BIN)
#ifdef MT9M034_DEBUG
			printk(KERN_INFO"mt9m034: Horizontal binning enabled\n");
#endif
		}
	}
	else {
		MT9M034_WRITE(ret, client, MT9M034_DIGITAL_BINNING,
			MT9M034_DISABLE_BINNING)
#ifdef MT9M034_DEBUG
		printk(KERN_INFO"mt9m034: Binning disabled\n");
#endif
	}

	MT9M034_WRITE(ret, client, MT9M034_Y_ADDR_START, mt9m034->crop.top)
	MT9M034_WRITE(ret, client, MT9M034_X_ADDR_START, mt9m034->crop.left)
	MT9M034_WRITE(ret, client, MT9M034_Y_ADDR_END, mt9m034->crop.top + mt9m034->crop.height - 1)
	MT9M034_WRITE(ret, client, MT9M034_X_ADDR_END, mt9m034->crop.left + mt9m034->crop.width - 1)
	MT9M034_WRITE(ret, client, MT9M034_FRAME_LENGTH_LINES, mt9m034->crop.height + 37)
	MT9M034_WRITE(ret, client, MT9M034_LINE_LENGTH_PCK, MT9M034_LLP_RECOMMENDED)
	MT9M034_WRITE(ret, client, MT9M034_COARSE_INT_TIME, 0x01C2)
	MT9M034_WRITE(ret, client, MT9M034_X_ODD_INC, 0x0001)
	MT9M034_WRITE(ret, client, MT9M034_Y_ODD_INC, 0x0001)

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
static struct mt9m034_pll_divs mt9m034_divs[] = {
        /* ext_freq     target_freq     M       N       p1      p2 */
        {24000000,      48000000,       32,     2,      2,      4},
        {24000000,      66000000,       44,     2,      2,      4},
        {27000000,      74250000,       44,	2,	1,	8},
        {48000000,      48000000,       40,     5,      2,      4}
};

/**
 * mt9m034_pll_setup - enable the sensor pll
 * @client: pointer to the i2c client
 *
 */
static int mt9m034_pll_setup(struct i2c_client *client)
{
	int ret;
	int i;
	struct mt9m034_priv *mt9m034 = to_mt9m034(client);

	for (i = 0; i < ARRAY_SIZE(mt9m034_divs); i++) {
		if (mt9m034_divs[i].ext_freq == mt9m034->pdata->ext_freq &&
			mt9m034_divs[i].target_freq == mt9m034->pdata->target_freq) {
			mt9m034->pll = &mt9m034_divs[i];
			goto out;
		}
	}
	dev_err(&client->dev, "Couldn't find PLL dividers for ext_freq = %d, target_freq = %d\n",
			mt9m034->pdata->ext_freq, mt9m034->pdata->target_freq);
	return -EINVAL;

out:
#ifdef MT9M034_DEBUG
	printk(KERN_INFO"mt9m034: PLL settings:M = %d, N = %d, P1 = %d, P2 = %d",
        mt9m034->pll->m, mt9m034->pll->n, mt9m034->pll->p1, mt9m034->pll->p2);
#endif
	MT9M034_WRITE(ret, client, MT9M034_VT_SYS_CLK_DIV, mt9m034->pll->p1)
	MT9M034_WRITE(ret, client, MT9M034_VT_PIX_CLK_DIV, mt9m034->pll->p2)
	MT9M034_WRITE(ret, client, MT9M034_PRE_PLL_CLK_DIV, mt9m034->pll->n)
	MT9M034_WRITE(ret, client, MT9M034_PLL_MULTIPLIER, mt9m034->pll->m)

	if (mt9m034->pdata->version == MT9M034_COLOR_VERSION)
		MT9M034_WRITE(ret, client, MT9M034_DIGITAL_TEST, 0x0000)
	else
		MT9M034_WRITE(ret, client, MT9M034_DIGITAL_TEST, 0x0080)

	msleep(100);

	return ret;
}

static int mt9m034_ae_setup(struct i2c_client *client)
{
	int ret;

	MT9M034_WRITE(ret, client, MT9M034_RESET_REGISTER, 0x10D8)
	MT9M034_WRITE(ret, client, MT9M034_HDR_COMP, 0x0001)
	MT9M034_WRITE(ret, client, MT9M034_VT_SYS_CLK_DIV, 0x0002)
	MT9M034_WRITE(ret, client, MT9M034_VT_PIX_CLK_DIV, 0x0004)
	MT9M034_WRITE(ret, client, MT9M034_PRE_PLL_CLK_DIV, 0x0002)
	MT9M034_WRITE(ret, client, MT9M034_PLL_MULTIPLIER, 0x002C)
	MT9M034_WRITE(ret, client, MT9M034_DIGITAL_TEST, 0x1300)
	MT9M034_WRITE(ret, client, MT9M034_RESET_REGISTER, 0x10DC)
	MT9M034_WRITE(ret, client, MT9M034_EMBEDDED_DATA_CTRL, 0x1982)
	MT9M034_WRITE(ret, client, MT9M034_BLUE_GAIN, 0x003F)
	MT9M034_WRITE(ret, client, MT9M034_AE_CTRL_REG, 0x001B)
	MT9M034_WRITE(ret, client, MT9M034_AE_DCG_EXPOSURE_HIGH_REG, 0x029F)
	MT9M034_WRITE(ret, client, MT9M034_AE_DCG_EXPOSURE_LOW_REG, 0x008C)
	MT9M034_WRITE(ret, client, MT9M034_AE_DCG_GAIN_FACTOR_REG, 0x02C0)
	MT9M034_WRITE(ret, client, MT9M034_AE_DCG_GAIN_FACTOR_INV_REG, 0x005B)
	MT9M034_WRITE(ret, client, MT9M034_AE_LUMA_TARGET_REG, 0x0384)
	MT9M034_WRITE(ret, client, MT9M034_AE_HIST_TARGET_REG, 0x1000)
	MT9M034_WRITE(ret, client, MT9M034_AE_ALPHA_V1_REG, 0x0080)
	MT9M034_WRITE(ret, client, MT9M034_AE_MAX_EXPOSURE_REG, 0x03DD)
	MT9M034_WRITE(ret, client, MT9M034_AE_MIN_EXPOSURE_REG, 0x0003)

	return ret;
}

static int mt9m034_is_streaming(struct i2c_client *client)
{
	u16 streaming;

	streaming = mt9m034_read(client, MT9M034_RESET_REG);
	streaming = ( (streaming >> 2) & 0x0001);

	return (streaming != 0);
}

static int mt9m034_set_autoexposure( struct i2c_client *client, enum v4l2_exposure_auto_type ae_mode )

{
	struct mt9m034_priv *mt9m034 = to_mt9m034(client);
	int streaming;
	int ret;

	/* Save the current streaming state. Used later to restore it */
	streaming = mt9m034_is_streaming(client);

	msleep(2);

	switch(ae_mode) {
	case V4L2_EXPOSURE_AUTO: /* Shutter and Apperture */
		dev_err(&client->dev, "Unsupported auto-exposure mode requested: %d\n", ae_mode);
		ret = -EINVAL;
		break;

	case V4L2_EXPOSURE_MANUAL:
		if (streaming) {
			MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_OFF)
		}

		MT9M034_WRITE(ret, client, MT9M034_EMBEDDED_DATA_CTRL, 0x1802)
		MT9M034_WRITE(ret, client, MT9M034_AE_CTRL_REG, 0x0000)
		if (streaming) {
			MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_ON)
		}
		break;

	case V4L2_EXPOSURE_SHUTTER_PRIORITY:
		if (streaming) {
			MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_OFF)
		}
		MT9M034_WRITE(ret, client, MT9M034_EMBEDDED_DATA_CTRL, 0x1982)
		MT9M034_WRITE(ret, client, MT9M034_AE_CTRL_REG, 0x0013)

		if (streaming) {
			MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_ON)

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
		mt9m034->autoexposure = ae_mode;

	return ret;
}

/**
 * mt9m034_power_on - power on the sensor
 * @mt9m034: pointer to private data structure
 *
 */
void mt9m034_power_on(struct mt9m034_priv *mt9m034)
{
	/* Ensure RESET_BAR is low */
	if (mt9m034->pdata->reset) {
		mt9m034->pdata->reset(&mt9m034->subdev, 1);
		msleep(1);
	}

	/* Enable clock */
	if (mt9m034->pdata->set_xclk) {
		mt9m034->pdata->set_xclk(&mt9m034->subdev,
		mt9m034->pdata->ext_freq);
		msleep(1);
	}

	/* Now RESET_BAR must be high */
	if (mt9m034->pdata->reset) {
		mt9m034->pdata->reset(&mt9m034->subdev, 0);
		msleep(1);
	}
}

/**
 * mt9m034_power_off - power off the sensor
 * @mt9m034: pointer to private data structure
 *
 */
void mt9m034_power_off(struct mt9m034_priv *mt9m034)
{
	if (mt9m034->pdata->set_xclk)
		mt9m034->pdata->set_xclk(&mt9m034->subdev, 0);
}

/************************************************************************
			v4l2_subdev_core_ops
************************************************************************/

static int mt9m034_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mt9m034_priv *mt9m034 = container_of(ctrl->handler,
					struct mt9m034_priv, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&mt9m034->subdev);
	u16 reg16;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_AUTO:
		ret = mt9m034_set_autoexposure(client, (enum v4l2_exposure_auto_type)ctrl->val);
		if(ret < 0)
			return ret;
		break;

	case V4L2_CID_EXPOSURE:
		MT9M034_WRITE(ret, client, MT9M034_COARSE_INT_TIME, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_COARSE_INT_TIME_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_GAIN:
		MT9M034_WRITE(ret, client, MT9M034_GLOBAL_GAIN, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_GLOBAL_GAIN_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_GAIN_GREEN1:
		MT9M034_WRITE(ret, client, MT9M034_GREEN1_GAIN, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_GREEN1_GAIN_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_GAIN_RED:
		MT9M034_WRITE(ret, client, MT9M034_RED_GAIN, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_RED_GAIN_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_GAIN_BLUE:
		MT9M034_WRITE(ret, client, MT9M034_BLUE_GAIN, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_BLUE_GAIN_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_GAIN_GREEN2:
		MT9M034_WRITE(ret, client, MT9M034_GREEN2_GAIN, ctrl->val)
		MT9M034_WRITE(ret, client, MT9M034_GREEN2_GAIN_CB, ctrl->val)
		return ret;
		break;

	case V4L2_CID_ANALOG_GAIN:
		reg16 = mt9m034_read(client, MT9M034_DIGITAL_TEST);
		reg16 = ( reg16 & ~MT9M034_ANALOG_GAIN_MASK ) | 
			( ( ctrl->val << MT9M034_ANALOG_GAIN_SHIFT ) & MT9M034_ANALOG_GAIN_MASK );
		MT9M034_WRITE(ret, client, MT9M034_DIGITAL_TEST, reg16)
		return ret;
		break;

	case V4L2_CID_HFLIP:
		reg16 = mt9m034_read(client, MT9M034_READ_MODE);
		if (ctrl->val){
			reg16 |= 0x4000;
			MT9M034_WRITE(ret, client, MT9M034_READ_MODE, reg16)

			break;
		}
		reg16 &= 0xbfff;
		MT9M034_WRITE(ret, client, MT9M034_READ_MODE, reg16)
		break;
	
	case V4L2_CID_VFLIP:
		reg16 = mt9m034_read(client, MT9M034_READ_MODE);
		if (ctrl->val) {
			reg16 |= 0x8000;
			MT9M034_WRITE(ret, client, MT9M034_READ_MODE, reg16)
			break;
		}
		reg16 &= 0x7fff;
		MT9M034_WRITE(ret, client, MT9M034_READ_MODE, reg16)
		break;

	case V4L2_CID_TEST_PATTERN:
		if (!ctrl->val){
			MT9M034_WRITE(ret, client, MT9M034_TEST_PATTERN, 0x0000)
		}
		MT9M034_WRITE(ret, client, MT9M034_TEST_PATTERN, ctrl->val)
		break;
	}
	return ret;
}

static struct v4l2_ctrl_ops mt9m034_ctrl_ops = {
	.s_ctrl = mt9m034_s_ctrl,
};

/*
MT9M034_TEST_PATTERN
0 = Disabled. Normal operation. Generate output data from pixel array
1 = Solid color test pattern",
2 = color bar test pattern",
3 = Fade to gray color bar test pattern",
256 = Walking 1s test pattern (12 bit)"
*/
static const char * const mt9m034_test_pattern_menu[] = {
	"0:Disabled",
	"1:Solid color test pattern",
	"2:color bar test pattern",
	"3:Fade to gray color bar test pattern",
	"256:Walking 1s test pattern (12 bit)"
};

struct mt9m034_control {
	u32 id;
	s32 min;
	s32 max;
	u32 step;
	s32 def;
};

static const struct mt9m034_control mt9m034_standard_ctrls[] = {
	{ V4L2_CID_GAIN,     MT9M034_GLOBAL_GAIN_MIN, MT9M034_GLOBAL_GAIN_MAX, 1, MT9M034_GLOBAL_GAIN_DEF },
	{ V4L2_CID_EXPOSURE, MT9M034_EXPOSURE_MIN,    MT9M034_EXPOSURE_MAX,    1, MT9M034_EXPOSURE_DEF    },
	{ V4L2_CID_HFLIP,    0, 1, 1, 0 },
	{ V4L2_CID_VFLIP,    0, 1, 1, 0 },
};

static const struct v4l2_ctrl_config mt9m034_custom_ctrls[] = {
	{
		.ops            = &mt9m034_ctrl_ops,
		.id             = V4L2_CID_TEST_PATTERN,
		.type           = V4L2_CTRL_TYPE_MENU,
		.name           = "Test Pattern",
		.min            = 0,
		.max            = ARRAY_SIZE(mt9m034_test_pattern_menu) - 1,
		.step           = 0,
		.def            = 0,
		.flags          = 0,
		.menu_skip_mask = 0,
		.qmenu          = mt9m034_test_pattern_menu,
	}, {
                .ops            = &mt9m034_ctrl_ops,
                .id             = V4L2_CID_GAIN_GREEN1,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Green (R)",
                .min            = MT9M034_GLOBAL_GAIN_MIN,
                .max            = MT9M034_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M034_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m034_ctrl_ops,
                .id             = V4L2_CID_GAIN_RED,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Red",
                .min            = MT9M034_GLOBAL_GAIN_MIN,
                .max            = MT9M034_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M034_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m034_ctrl_ops,
                .id             = V4L2_CID_GAIN_BLUE,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Blue",
                .min            = MT9M034_GLOBAL_GAIN_MIN,
                .max            = MT9M034_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M034_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {

                .ops            = &mt9m034_ctrl_ops,
                .id             = V4L2_CID_GAIN_GREEN2,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Green (B)",
                .min            = MT9M034_GLOBAL_GAIN_MIN,
                .max            = MT9M034_GLOBAL_GAIN_MAX,
                .step           = 1,
                .def            = MT9M034_GLOBAL_GAIN_DEF,
                .flags          = 0,
        }, {
                .ops            = &mt9m034_ctrl_ops,
                .id             = V4L2_CID_ANALOG_GAIN,
                .type           = V4L2_CTRL_TYPE_INTEGER,
                .name           = "Gain, Column",
                .min            = MT9M034_ANALOG_GAIN_MIN,
                .max            = MT9M034_ANALOG_GAIN_MAX,
                .step           = 1,
                .def            = MT9M034_ANALOG_GAIN_DEF,
                .flags          = 0,
	}
};

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9m034_g_reg(struct v4l2_subdev *sd,
				struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int data;

	reg->size = 2;
	data = mt9m034_read(client, reg->reg);
	if (data < 0)
		return data;

	reg->val = data;
	return 0;
}

static int mt9m034_s_reg(struct v4l2_subdev *sd,
				struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	MT9M034_WRITE(ret, client, reg->reg, reg->val)
	
	return ret;
}
#endif

static int mt9m034_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m034_priv *mt9m034 = container_of(sd,
				struct mt9m034_priv, subdev);
	int ret = 0;

	mutex_lock(&mt9m034->power_lock);

	/*
	* If the power count is modified from 0 to != 0 or from != 0 to 0,
	* update the power state.
	*/
	if (mt9m034->power_count == !on) {
		if (on) {
				mt9m034_power_on(mt9m034);
				ret = __mt9m034_write(client, MT9M034_RESET_REG, MT9M034_RESET);
				if (ret < 0) {
					dev_err(mt9m034->subdev.v4l2_dev->dev,
					"Failed to reset the camera\n");
					goto out;
				}
				ret = v4l2_ctrl_handler_setup(&mt9m034->ctrls);
				if (ret < 0)
					goto out;
		} else
			mt9m034_power_off(mt9m034);
	}
	/* Update the power count. */
	mt9m034->power_count += on ? 1 : -1;
	WARN_ON(mt9m034->power_count < 0);
out:
	mutex_unlock(&mt9m034->power_lock);
	return ret;
}

/***************************************************
		v4l2_subdev_video_ops
****************************************************/


static int mt9m034_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m034_frame_size frame;
	int ret;

	if (!enable){
		MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_OFF)
		return ret;
	}
	
	ret = mt9m034_sequencer_settings(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup sequencer\n",__func__);
		return ret;
	}

	msleep(200);

	ret = mt9m034_linear_mode_setup(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup linear mode\n",__func__);
		return ret;
	}

	ret = mt9m034_set_size(client, &frame);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup resolution\n",__func__);
		return ret;
	}

	ret = mt9m034_pll_setup(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup pll\n",__func__);
		return ret;
	}

	ret = mt9m034_ae_setup(client);
	if (ret < 0){
		printk(KERN_ERR"%s: Failed to setup auto-exposure\n",__func__);
		return ret;
	}

	/* start streaming */
	MT9M034_WRITE(ret, client, MT9M034_RESET_REG, MT9M034_STREAM_ON)
	
	return ret;

}


/***************************************************
		v4l2_subdev_pad_ops
****************************************************/
static int mt9m034_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);

	if (code->pad || code->index)
		return -EINVAL;

	code->code = mt9m034->format.code;
	return 0;
}

static int mt9m034_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);

	if (fse->index != 0 || fse->code != mt9m034->format.code)
		return -EINVAL;

	fse->min_width = MT9M034_WINDOW_WIDTH_MIN;
	fse->max_width = MT9M034_WINDOW_WIDTH_MAX;
	fse->min_height = MT9M034_WINDOW_HEIGHT_MIN;
	fse->max_height = MT9M034_WINDOW_HEIGHT_MAX;

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9m034_get_pad_format(struct mt9m034_priv *mt9m034, struct v4l2_subdev_fh *fh,
			unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &mt9m034->format;
	default:
		return NULL;
	}
}

static struct v4l2_rect *
__mt9m034_get_pad_crop(struct mt9m034_priv *mt9m034, struct v4l2_subdev_fh *fh,
	unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &mt9m034->crop;
	default:
		return NULL;
	}
}

static int mt9m034_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);

	fmt->format = *__mt9m034_get_pad_format(mt9m034, fh, fmt->pad,
						fmt->which);

	return 0;
}

static int mt9m034_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *format)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);
	struct mt9m034_frame_size size;
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	unsigned int wratio;
	unsigned int hratio;

	__crop = __mt9m034_get_pad_crop(mt9m034, fh, format->pad,
		format->which);
	/* Clamp the width and height to avoid dividing by zero. */
	size.width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
			MT9M034_WINDOW_WIDTH_MIN,
			MT9M034_WINDOW_WIDTH_MAX);
	size.height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
			MT9M034_WINDOW_HEIGHT_MIN,
			MT9M034_WINDOW_WIDTH_MAX);

	wratio = DIV_ROUND_CLOSEST(__crop->width, size.width);
	hratio = DIV_ROUND_CLOSEST(__crop->height, size.height);

	__format = __mt9m034_get_pad_format(mt9m034, fh, format->pad,
						format->which);
	__format->width = __crop->width / wratio;
	__format->height = __crop->height / hratio;

	printk(KERN_INFO"mt9m034: crop = %dx%d format = %dx%d\n",
	__crop->width, __crop->height, __format->width, __format->height);

	format->format = *__format;
	mt9m034->format.width	= format->format.width;
	mt9m034->format.height	= format->format.height;
	mt9m034->format.code	= V4L2_MBUS_FMT_SGRBG12_1X12;

	return 0;
}

static int mt9m034_get_crop(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_crop *crop)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);

	crop->rect = *__mt9m034_get_pad_crop(mt9m034, fh, crop->pad, crop->which);

	return 0;
}

static int mt9m034_set_crop(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_crop *crop)
{
	struct mt9m034_priv *mt9m034 = container_of(sd,
					struct mt9m034_priv, subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	struct v4l2_rect rect;

	/* Clamp the crop rectangle boundaries and align them to a multiple of 2
	* pixels to ensure a GRBG Bayer pattern.
	*/
	rect.left = clamp(ALIGN(crop->rect.left, 2), MT9M034_COLUMN_START_MIN,
			MT9M034_COLUMN_START_MAX);
	rect.top = clamp(ALIGN(crop->rect.top, 2), MT9M034_ROW_START_MIN,
			MT9M034_ROW_START_MAX);
	rect.width = clamp(ALIGN(crop->rect.width, 2),
			MT9M034_WINDOW_WIDTH_MIN,
			MT9M034_WINDOW_WIDTH_MAX);
	rect.height = clamp(ALIGN(crop->rect.height, 2),
			MT9M034_WINDOW_HEIGHT_MIN,
			MT9M034_WINDOW_HEIGHT_MAX);

	rect.width = min(rect.width, MT9M034_PIXEL_ARRAY_WIDTH - rect.left);
	rect.height = min(rect.height, MT9M034_PIXEL_ARRAY_HEIGHT - rect.top);

	__crop = __mt9m034_get_pad_crop(mt9m034, fh, crop->pad, crop->which);

	/* Reset the output image size if the crop rectangle size has
	* been modified.
	*/
	if (rect.width != __crop->width || rect.height != __crop->height) {
		__format = __mt9m034_get_pad_format(mt9m034, fh, crop->pad,
								crop->which);
		__format->width = rect.width;
		__format->height = rect.height;
	}

	*__crop = rect;
	crop->rect = rect;

	mt9m034->crop.left	= crop->rect.left;
	mt9m034->crop.top	= crop->rect.top;
	mt9m034->crop.width	= crop->rect.width;
	mt9m034->crop.height	= crop->rect.height;

	return 0;
}

/***********************************************************
	V4L2 subdev internal operations
************************************************************/
static int mt9m034_registered(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m034_priv *mt9m034 = to_mt9m034(client);
	s32 data;
	int count = 0;

	mt9m034_power_on(mt9m034);

	/* Read out the chip version register */
	data = mt9m034_read(client, MT9M034_CHIP_ID_REG);
	if (data != MT9M034_CHIP_ID) {
		while(count++ < 5){
			data = mt9m034_read(client, MT9M034_CHIP_ID_REG);
			msleep(5);
		}
		dev_err(&client->dev, "Aptina MT9M034 not detected, chip ID read:0x%4.4x\n",
				data);
		return -ENODEV;
	}
	dev_info(&client->dev, "Aptina MT9M034 detected at address 0x%02x\n", client->addr);

	mt9m034_power_off(mt9m034);

	return 0;
}

static int mt9m034_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return mt9m034_s_power(sd, 1);
}

static int mt9m034_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return mt9m034_s_power(sd, 0);
}

/***************************************************
		v4l2_subdev_ops
****************************************************/
static struct v4l2_subdev_core_ops mt9m034_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9m034_g_reg,
	.s_register	= mt9m034_s_reg,
#endif
	.s_power	= mt9m034_s_power,
};

static struct v4l2_subdev_video_ops mt9m034_subdev_video_ops = {
	.s_stream	= mt9m034_s_stream,
};

static struct v4l2_subdev_pad_ops mt9m034_subdev_pad_ops = {
	.enum_mbus_code	 = mt9m034_enum_mbus_code,
	.enum_frame_size = mt9m034_enum_frame_size,
	.get_fmt	 = mt9m034_get_format,
	.set_fmt	 = mt9m034_set_format,
	.get_crop	 = mt9m034_get_crop,
	.set_crop	 = mt9m034_set_crop,
};

static struct v4l2_subdev_ops mt9m034_subdev_ops = {
	.core	= &mt9m034_subdev_core_ops,
	.video	= &mt9m034_subdev_video_ops,
	.pad	= &mt9m034_subdev_pad_ops,
};

/*
 * Internal ops. Never call this from drivers, only the v4l2 framework can call
 * these ops.
 */
static const struct v4l2_subdev_internal_ops mt9m034_subdev_internal_ops = {
	.registered	= mt9m034_registered,
	.open		= mt9m034_open,
	.close		= mt9m034_close,
};

/***************************************************
		I2C driver
****************************************************/
static int mt9m034_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct mt9m034_platform_data *pdata = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mt9m034_priv *mt9m034;
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

	mt9m034 = devm_kzalloc(&client->dev, sizeof(struct mt9m034_priv),
				GFP_KERNEL);
	if (mt9m034 == NULL)
		return -ENOMEM;

	mt9m034->pdata = pdata;

	v4l2_ctrl_handler_init(&mt9m034->ctrls, ARRAY_SIZE(mt9m034_standard_ctrls) + 
					ARRAY_SIZE(mt9m034_custom_ctrls) + 1);
	
	v4l2_ctrl_new_std_menu(&mt9m034->ctrls, &mt9m034_ctrl_ops,
		V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_APERTURE_PRIORITY, 0,
		V4L2_EXPOSURE_SHUTTER_PRIORITY);

	for (i = 0; i < ARRAY_SIZE(mt9m034_standard_ctrls); ++i ) {
		const struct mt9m034_control *ctrl = &mt9m034_standard_ctrls[i];
	
		v4l2_ctrl_new_std(&mt9m034->ctrls, &mt9m034_ctrl_ops,
				ctrl->id, ctrl->min, ctrl->max, ctrl->step, ctrl->def);
	}

	for (i = 0; i < ARRAY_SIZE(mt9m034_custom_ctrls); i++){
		v4l2_ctrl_new_custom(&mt9m034->ctrls, &mt9m034_custom_ctrls[i], NULL);
	}
	mt9m034->subdev.ctrl_handler = &mt9m034->ctrls;

	if (mt9m034->ctrls.error) {
		ret = mt9m034->ctrls.error;
		dev_err(&client->dev, "Control initialization error: %d\n",
			ret);
		goto done;
	}

	mutex_init(&mt9m034->power_lock);
	v4l2_i2c_subdev_init(&mt9m034->subdev, client, &mt9m034_subdev_ops);
	mt9m034->subdev.internal_ops = &mt9m034_subdev_internal_ops;
	mt9m034->subdev.ctrl_handler = &mt9m034->ctrls;

	mt9m034->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&mt9m034->subdev.entity, 1, &mt9m034->pad, 0);
	if (ret < 0)
		goto done;

	mt9m034->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	mt9m034->crop.width	= MT9M034_WINDOW_WIDTH_MAX;
	mt9m034->crop.height	= MT9M034_WINDOW_HEIGHT_MAX;
	mt9m034->crop.left	= MT9M034_COLUMN_START_DEF;
	mt9m034->crop.top	= MT9M034_ROW_START_DEF;

	mt9m034->format.code		= V4L2_MBUS_FMT_SGRBG12_1X12;
	mt9m034->format.width		= MT9M034_WINDOW_WIDTH_DEF;
	mt9m034->format.height		= MT9M034_WINDOW_HEIGHT_DEF;
	mt9m034->format.field		= V4L2_FIELD_NONE;
	mt9m034->format.colorspace	= V4L2_COLORSPACE_SRGB;

done:
	if (ret < 0) {
		v4l2_ctrl_handler_free(&mt9m034->ctrls);
		media_entity_cleanup(&mt9m034->subdev.entity);
		dev_err(&client->dev, "Probe failed\n");
	}

	return ret;
}

static int mt9m034_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct mt9m034_priv *mt9m034 = to_mt9m034(client);

	v4l2_ctrl_handler_free(&mt9m034->ctrls);
	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	return 0;
}

static const struct i2c_device_id mt9m034_id[] = {
	{ "mt9m034", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9m034_id);

static struct i2c_driver mt9m034_i2c_driver = {
	.driver = {
		 .name = "mt9m034",
	},
	.probe    = mt9m034_probe,
	.remove   = mt9m034_remove,
	.id_table = mt9m034_id,
};

/* module_i2c_driver(mt9m034_i2c_driver); */
static int __init mt9m034_module_init(void)
{
	return i2c_add_driver(&mt9m034_i2c_driver);
}

static void __exit mt9m034_module_exit(void)
{
	i2c_del_driver(&mt9m034_i2c_driver);
}
module_init(mt9m034_module_init);
module_exit(mt9m034_module_exit);

MODULE_DESCRIPTION("Aptina MT9M034 Camera driver");
MODULE_AUTHOR("Aptina Imaging <drivers@aptina.com>");
MODULE_LICENSE("GPL v2");
