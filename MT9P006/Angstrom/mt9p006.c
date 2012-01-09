/*
 * drivers/media/video/mt9p006.c
 *
 * Aptina MT9P006/A-51HD+ sensor driver
 *
 * Copyright (C) 2012 Aptina Imaging
 * 
 * Contributor: Abhishek Reddy Kondaveeti <areddykondaveeti@aptina.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include<linux/delay.h>
#include<linux/device.h>
#include<linux/i2c.h>
#include<linux/log2.h>
#include<linux/pm.h>
#include<linux/slab.h>
#include<media/v4l2-subdev.h>
#include<linux/videodev2.h>

#include<media/mt9p006.h>
#include<media/v4l2-chip-ident.h>
#include<media/v4l2-ctrls.h>
#include<media/v4l2-device.h>
#include<media/v4l2-subdev.h>

#define MT9P006_PIXEL_ARRAY_WIDTH			2752
#define MT9P006_PIXEL_ARRAY_HEIGHT			2004

#define MT9P006_CHIP_VERSION				0x00
#define		MT9P006_CHIP_VERSION_VALUE		0x1801
#define MT9P006_ROW_START				0x01
#define		MT9P006_ROW_START_MIN			0
#define		MT9P006_ROW_START_MAX			2004
#define		MT9P006_ROW_START_DEF			54
#define MT9P006_COLUMN_START				0x02
#define		MT9P006_COLUMN_START_MIN		0
#define		MT9P006_COLUMN_START_MAX		2750
#define		MT9P006_COLUMN_START_DEF		16
#define MT9P006_WINDOW_HEIGHT				0x03
#define		MT9P006_WINDOW_HEIGHT_MIN		2
#define		MT9P006_WINDOW_HEIGHT_MAX		2006
#define		MT9P006_WINDOW_HEIGHT_DEF		1944
#define MT9P006_WINDOW_WIDTH				0x04
#define		MT9P006_WINDOW_WIDTH_MIN		2
#define		MT9P006_WINDOW_WIDTH_MAX		2752
#define		MT9P006_WINDOW_WIDTH_DEF		2592
#define MT9P006_HORIZONTAL_BLANK			0x05
#define		MT9P006_HORIZONTAL_BLANK_MIN		0
#define		MT9P006_HORIZONTAL_BLANK_MAX		4095
#define MT9P006_VERTICAL_BLANK				0x06
#define		MT9P006_VERTICAL_BLANK_MIN		0
#define		MT9P006_VERTICAL_BLANK_MAX		4095
#define		MT9P006_VERTICAL_BLANK_DEF		25
#define MT9P006_OUTPUT_CONTROL				0x07
#define		MT9P006_OUTPUT_CONTROL_CEN		2
#define		MT9P006_OUTPUT_CONTROL_SYN		1
#define		MT9P006_OUTPUT_CONTROL_DEF		0x1f82
#define MT9P006_SHUTTER_WIDTH_UPPER			0x08
#define MT9P006_SHUTTER_WIDTH_LOWER			0x09
#define		MT9P006_SHUTTER_WIDTH_MIN		1
#define		MT9P006_SHUTTER_WIDTH_MAX		1048575
#define		MT9P006_SHUTTER_WIDTH_DEF		1943
#define	MT9P006_PLL_CONTROL				0x10
#define		MT9P006_PLL_CONTROL_PWROFF		0x0050
#define		MT9P006_PLL_CONTROL_PWRON		0x0051
#define		MT9P006_PLL_CONTROL_USEPLL		0x0052
#define	MT9P006_PLL_CONFIG_1				0x11
#define	MT9P006_PLL_CONFIG_2				0x12
#define MT9P006_PIXEL_CLOCK_CONTROL			0x0a
#define MT9P006_FRAME_RESTART				0x0b
#define MT9P006_SHUTTER_DELAY				0x0c
#define MT9P006_RST					0x0d
#define		MT9P006_RST_ENABLE			1
#define		MT9P006_RST_DISABLE			0
#define MT9P006_READ_MODE_1				0x1e
#define MT9P006_READ_MODE_2				0x20
#define		MT9P006_READ_MODE_2_ROW_MIR		(1 << 15)
#define		MT9P006_READ_MODE_2_COL_MIR		(1 << 14)
#define		MT9P006_READ_MODE_2_ROW_BLC		(1 << 6)
#define MT9P006_ROW_ADDRESS_MODE			0x22
#define MT9P006_COLUMN_ADDRESS_MODE			0x23
#define MT9P006_GLOBAL_GAIN				0x35
#define		MT9P006_GLOBAL_GAIN_MIN			8
#define		MT9P006_GLOBAL_GAIN_MAX			1024
#define		MT9P006_GLOBAL_GAIN_DEF			8
#define		MT9P006_GLOBAL_GAIN_MULT		(1 << 6)
#define MT9P006_ROW_BLACK_DEF_OFFSET			0x4b
#define MT9P006_TEST_PATTERN				0xa0
#define		MT9P006_TEST_PATTERN_SHIFT		3
#define		MT9P006_TEST_PATTERN_ENABLE		(1 << 0)
#define		MT9P006_TEST_PATTERN_DISABLE		(0 << 0)
#define MT9P006_TEST_PATTERN_GREEN			0xa1
#define MT9P006_TEST_PATTERN_RED			0xa2
#define MT9P006_TEST_PATTERN_BLUE			0xa3


#define MT9P006_GREEN_1_GAIN		0x2b
#define MT9P006_BLUE_GAIN		0x2c
#define MT9P006_RED_GAIN		0x2d
#define MT9P006_GREEN_2_GAIN		0x2e

//#define MT9P006_DEBUG
#undef MT9P006_DEBUG
#ifdef MT9P006_DEBUG
#define DPRINTK_DRIVER(format, ...)                             \
        printk(KERN_INFO "_MT9P006_DRIVER: \n" format, ## __VA_ARGS__)
#else
#define DPRINTK_DRIVER(format, ...)
#endif


struct mt9p006_pll_divs {
	u32 ext_freq;
	u32 target_freq;
	u8 m;
	u8 n;
	u8 p1;
};

struct mt9p006_frame_size {
	u16 width;
	u16 height;
};

/**************************supported sizes******************************/
const static struct mt9p006_frame_size mt9p006_sizes[] = {
	{  640, 480 },
	{ 1280, 720 },
	{ 1920, 1080 },
	{ 2048, 1536 },	//3MP
	{ 2592, 1944 },	//5MP
};

enum mt9p006_image_size {
	VGA_BIN_30FPS,
	HDV_720P_30FPS,
	HDV_1080P_30FPS,
	MT9P006_THREE_MP,
	MT9P006_FIVE_MP,
};

enum mt9p006_image_size mt9p006_current_format;

struct mt9p006 {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_rect crop;  /* Sensor window */
	struct v4l2_mbus_framefmt format;
	struct v4l2_ctrl_handler ctrls;
	struct mt9p006_platform_data *pdata;
	struct mutex power_lock; /* lock to protect power_count */
	int power_count;
	u16 xskip;
	u16 yskip;

	const struct mt9p006_pll_divs *pll;

	/* Registers cache */
	u16 output_control;
	u16 mode2;
};

static struct mt9p006 *to_mt9p006(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mt9p006, subdev);
}

/**
 * reg_read - reads the data from the given register
 * @client: pointer to i2c client
 * @reg: address of the register which is to be read
 *
 */
static int reg_read(struct i2c_client *client, const u8 reg)
{
       	s32 data = i2c_smbus_read_word_data(client, reg);
       	return data<  0 ? data : swab16(data);
}
/**
 * reg_write - writes the data into the given register
 * @client: pointer to i2c client
 * @reg: address of the register in which to write
 *
 */
static int reg_write(struct i2c_client *client, const u8 reg,
                       const u16 data)
{
       	return i2c_smbus_write_word_data(client, reg, swab16(data));
}


/**
 * mt9p006_calc_size - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static enum mt9p006_image_size mt9p006_calc_size(unsigned int width,
						 unsigned int height)
{
	enum mt9p006_image_size isize;
	unsigned long pixels = width * height;

	for (isize = VGA_BIN_30FPS; isize <= MT9P006_FIVE_MP; isize++) {
		if (mt9p006_sizes[isize].height *
					mt9p006_sizes[isize].width >= pixels) {
			
			return isize;
		}
	}

	return MT9P006_FIVE_MP;
}

/**
 * mt9p006_find_isize - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static enum mt9p006_image_size mt9p006_find_isize(unsigned int width)
{
	enum mt9p006_image_size isize;

	for (isize = VGA_BIN_30FPS; isize <= MT9P006_FIVE_MP; isize++) {
		if (mt9p006_sizes[isize].width >= width)
			break;
	}

	return isize;
}

static int mt9p006_set_output_control(struct mt9p006 *mt9p006, u16 clear,
				      u16 set)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	u16 value = (mt9p006->output_control & ~clear) | set;
	int ret;

	ret = reg_write(client, MT9P006_OUTPUT_CONTROL, value);
	if (ret < 0)
		return ret;

	mt9p006->output_control = value;
	return 0;
}

static int mt9p006_reset(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	int ret;

	/* Disable chip output, synchronous option update */
	ret = reg_write(client, MT9P006_RST, MT9P006_RST_ENABLE);
	if (ret < 0)
		return ret;
	ret = reg_write(client, MT9P006_RST, MT9P006_RST_DISABLE);
	if (ret < 0)
		return ret;

	return mt9p006_set_output_control(mt9p006, MT9P006_OUTPUT_CONTROL_CEN,
					  0);
}

static int mt9p006_v4l2_try_fmt_cap(struct mt9p006_frame_size *requstedsize) {
	enum mt9p006_image_size isize;
	int ret = 0;
		
	isize = mt9p006_calc_size(requstedsize->width,requstedsize->height);
	mt9p006_current_format = isize;
	requstedsize->width = mt9p006_sizes[isize].width;
	requstedsize->height = mt9p006_sizes[isize].height;
	
	return 0;
}

static void dump_subsample_optimization_settings(struct i2c_client* client)
{
	reg_write(client, 0x70, 0x5C);
	reg_write(client, 0x71, 0x5B00);
	reg_write(client, 0x72, 0x5900);
	reg_write(client, 0x73, 0x200);
	reg_write(client, 0x74, 0x200);
	reg_write(client, 0x75, 0x2800);
	reg_write(client, 0x76, 0x3E29);
	reg_write(client, 0x77, 0x3E29); 
	reg_write(client, 0x78, 0x583F);
	reg_write(client, 0x79, 0x5B00);
	reg_write(client, 0x7A, 0x5A00);
	reg_write(client, 0x7B, 0x5900);
	reg_write(client, 0x7C, 0x5900);
	reg_write(client, 0x7E, 0x5900);
	reg_write(client, 0x7F, 0x5900);
	reg_write(client, 0x06, 0x0);
	reg_write(client, 0x29, 0x481); 
	reg_write(client, 0x3E, 0x87);
	reg_write(client, 0x3F, 0x7);
	reg_write(client, 0x41, 0x3);
	reg_write(client, 0x48, 0x18);
	reg_write(client, 0x5F, 0x1C16);
	reg_write(client, 0x57, 0x7); 
	reg_write(client, 0x2A, 0xFF74);
	reg_write(client, 0x35, 0x000C);
	reg_write(client, 0x3E, 0x07);
	 
}

static int mt9p006_set_params(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	struct v4l2_mbus_framefmt *format = &mt9p006->format;
	const struct v4l2_rect *crop = &mt9p006->crop;
	int ret;
	enum mt9p006_image_size i;

	i = mt9p006_find_isize(format->width);
		switch(i)
			{
				case VGA_BIN_30FPS:
					ret |= reg_write(client, 0x03, 0x0778);
					ret |= reg_write(client, 0x04, 0x09F8);
				  	ret |= reg_write(client, 0x08, 0x0000);
					ret |= reg_write(client, 0x09, 0x01AC);
					ret |= reg_write(client, 0x0C, 0x0000);
					ret |= reg_write(client, 0x22, 0x0033);
					ret |= reg_write(client, 0x23, 0x0033);
					ret |= reg_write(client, 0x08, 0x0000); 
					ret |= reg_write(client, 0x09, 0x0296);
					ret |= reg_write(client, 0x0C, 0x0000);
					dump_subsample_optimization_settings(client);
					break;  
				case HDV_720P_30FPS:
					ret |= reg_write(client, 0x01, 0x0040);
                                        ret |= reg_write(client, 0x02, 0x0018);
                                        ret |= reg_write(client, 0x03, 0x059F);
					ret |= reg_write(client, 0x04, 0x09FF);
                                        ret |= reg_write(client, 0x05, 0x0000);
                                        ret |= reg_write(client, 0x06, 0x0000);
                                        ret |= reg_write(client, 0x09, 0x0400);
                                        ret |= reg_write(client, 0x22, 0x0011);
                                        ret |= reg_write(client, 0x23, 0x0011);
                                        ret |= reg_write(client, 0x20, 0x0060);
                                        ret |= reg_write(client, 0x08, 0x0000);
                                        ret |= reg_write(client, 0x09, 0x05AF);
                                        ret |= reg_write(client, 0x0C, 0x0000);
                                        dump_subsample_optimization_settings(client);
					break;
				case HDV_1080P_30FPS:
 				 	ret |= reg_write(client, 0x01, 0x1E6);
                                        ret |= reg_write(client, 0x02, 0x160);
                                        ret |= reg_write(client, 0x03, 0x0438);
                                        ret |= reg_write(client, 0x04, 0x0780);
                                        ret |= reg_write(client, 0x05, 0x121);
                                        ret |= reg_write(client, 0x06, 0x008);
                                        ret |= reg_write(client, 0x09, 0x0442);
                                        ret |= reg_write(client, 0x22, 0x0000);
                                        ret |= reg_write(client, 0x23, 0x0000);
					ret |= reg_write(client, 0x08, 0x0000);
                                        ret |= reg_write(client, 0x06, 0x0008);
                                        ret |= reg_write(client, 0x05, 0x0121);
                                        dump_subsample_optimization_settings(client);
					break;
				case MT9P006_THREE_MP:
					ret |= reg_write(client, 0x01, 0x0F6);
                                        ret |= reg_write(client, 0x02, 0x120);
                                        ret |= reg_write(client, 0x03, 0x0600);
                                        ret |= reg_write(client, 0x04, 0x0800);
                                        ret |= reg_write(client, 0x05, 0x121);
                                        ret |= reg_write(client, 0x06, 0x008);
                                        ret |= reg_write(client, 0x09, 0x060A);
                                        ret |= reg_write(client, 0x22, 0x0000);
                                        ret |= reg_write(client, 0x23, 0x0000);
                                        ret |= reg_write(client, 0x20, 0x0060);
                                        ret |= reg_write(client, 0x08, 0x0000);
                                        ret |= reg_write(client, 0x09, 0x060A);
                                        ret |= reg_write(client, 0x0C, 0x0000);
                                        dump_subsample_optimization_settings(client);
					break;
				case MT9P006_FIVE_MP:
					ret |= reg_write(client, 0x01, 0x036);
                                        ret |= reg_write(client, 0x02, 0x010);
                                        ret |= reg_write(client, 0x03, 0x0798);
                                        ret |= reg_write(client, 0x04, 0x0A20);
                                        ret |= reg_write(client, 0x05, 0x121);
                                        ret |= reg_write(client, 0x06, 0x008);
                                        ret |= reg_write(client, 0x09, 0x07A2);
                                        ret |= reg_write(client, 0x22, 0x0000);
                                        ret |= reg_write(client, 0x23, 0x0000);
                                        ret |= reg_write(client, 0x20, 0x0060);
                                        ret |= reg_write(client, 0x08, 0x0000);
                                        ret |= reg_write(client, 0x09, 0x07A2);
                                        ret |= reg_write(client, 0x0C, 0x0000);
                                        dump_subsample_optimization_settings(client);
					break;
			}
	return ret;
}

static int mt9p006_load_initialization_settings(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	int ret;
	
	ret  = reg_write(client, MT9P006_SHUTTER_WIDTH_UPPER, 0x0000);
	ret |= reg_write(client, MT9P006_SHUTTER_WIDTH_LOWER, 0x00E6);
	ret |= reg_write(client, MT9P006_SHUTTER_DELAY, 0x0613);
	ret |= reg_write(client, MT9P006_GREEN_1_GAIN, 0x0008);
	ret |= reg_write(client, MT9P006_BLUE_GAIN, 0x0012);
	ret |= reg_write(client, MT9P006_RED_GAIN, 0x000A);
	ret |= reg_write(client, MT9P006_GREEN_2_GAIN, 0x0008);
	ret |= reg_write(client, MT9P006_READ_MODE_1, 0x0006);	

	ret |= reg_write(client, 0x35, 0x000C);
	ret |= reg_write(client, 0x3E, 0x07);	
}

static int mt9p006_set_mode2(struct mt9p006 *mt9p006, u16 clear, u16 set)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	u16 value = (mt9p006->mode2 & ~clear) | set;
	int ret;

	ret = reg_write(client, MT9P006_READ_MODE_2, value);
	if (ret < 0)
		return ret;

	mt9p006->mode2 = value;
	return 0;
}

/*
 * This static table uses ext_freq and vdd_io values to select suitable
 * PLL dividers m, n and p1 which have been calculated as specifiec in p36
 * of Aptina's mt9p006 datasheet. New values should be added here.
 */
static const struct mt9p006_pll_divs mt9p006_divs[] = {
	/* ext_freq	target_freq	m	n	p1 */
	{24000000,	48000000,	26,	2,	6}
};

static int mt9p006_pll_get_divs(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(mt9p006_divs); i++) {
		if (mt9p006_divs[i].ext_freq == mt9p006->pdata->ext_freq &&
		  mt9p006_divs[i].target_freq == mt9p006->pdata->target_freq) {
			mt9p006->pll = &mt9p006_divs[i];
			return 0;
		}
	}

	dev_err(&client->dev, "Couldn't find PLL dividers for ext_freq = %d, "
		"target_freq = %d\n", mt9p006->pdata->ext_freq,
		mt9p006->pdata->target_freq);
	return -EINVAL;
}

static int mt9p006_pll_enable(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	int ret;

	ret = reg_write(client, MT9P006_PLL_CONTROL,
			    MT9P006_PLL_CONTROL_PWRON);
	if (ret < 0)
		return ret;

	ret = reg_write(client, MT9P006_PLL_CONFIG_1,
			    (mt9p006->pll->m << 8) | (mt9p006->pll->n - 1));
	if (ret < 0)
		return ret;

	ret = reg_write(client, MT9P006_PLL_CONFIG_2, mt9p006->pll->p1 - 1);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);
	ret = reg_write(client, MT9P006_PLL_CONTROL,
			    MT9P006_PLL_CONTROL_PWRON |
			    MT9P006_PLL_CONTROL_USEPLL);
	return ret;
}

static inline int mt9p006_pll_disable(struct mt9p006 *mt9p006)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);

	return reg_write(client, MT9P006_PLL_CONTROL,
			     MT9P006_PLL_CONTROL_PWROFF);
}

static int mt9p006_power_on(struct mt9p006 *mt9p006)
{
	/* Ensure RESET_BAR is low */
	if (mt9p006->pdata->reset) {
		mt9p006->pdata->reset(&mt9p006->subdev, 1);
		usleep_range(1000, 2000);
	}

	/* Emable clock */
	if (mt9p006->pdata->set_xclk)
		mt9p006->pdata->set_xclk(&mt9p006->subdev,
					 mt9p006->pdata->ext_freq);

	/* Now RESET_BAR must be high */
	if (mt9p006->pdata->reset) {
		mt9p006->pdata->reset(&mt9p006->subdev, 0);
		usleep_range(1000, 2000);
	}

	return 0;
}

static void mt9p006_power_off(struct mt9p006 *mt9p006)
{
	if (mt9p006->pdata->reset) {
		mt9p006->pdata->reset(&mt9p006->subdev, 1);
		usleep_range(1000, 2000);
	}

	if (mt9p006->pdata->set_xclk)
		mt9p006->pdata->set_xclk(&mt9p006->subdev, 0);
}

static int __mt9p006_set_power(struct mt9p006 *mt9p006, bool on)
{
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	int ret;

	if (!on) {
		mt9p006_power_off(mt9p006);
		return 0;
	}

	ret = mt9p006_power_on(mt9p006);
	if (ret < 0)
		return ret;

	ret = mt9p006_reset(mt9p006);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to reset the camera\n");
		return ret;
	}

	return 0;
}



static int mt9p006_enum_mbus_code(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);

	if (code->pad || code->index)
		return -EINVAL;

	code->code = mt9p006->format.code;
	return 0;
}

static int mt9p006_enum_frame_size(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);

	if (fse->index >= 8 || fse->code != mt9p006->format.code)
		return -EINVAL;

	fse->min_width = 640;
     	fse->max_width = 2592;
	fse->min_height = 480;
	fse->max_height = 1944;

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9p006_get_pad_format(struct mt9p006 *mt9p006, struct v4l2_subdev_fh *fh,
			 unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &mt9p006->format;
	default:
		return NULL;
	}
}

static int mt9p006_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);
	int ret;

	if (!enable) {
		/* Stop sensor readout */
		ret = mt9p006_set_output_control(mt9p006,
						 MT9P006_OUTPUT_CONTROL_CEN, 0);
		if (ret < 0)
			return ret;

		return mt9p006_pll_disable(mt9p006);
	}
	//ret = mt9p006_load_initialization_settings(mt9p006);
	ret = mt9p006_set_params(mt9p006);
	if (ret < 0)
		return ret;

	/* Switch to master "normal" mode */
	ret = mt9p006_set_output_control(mt9p006, 0,
					 MT9P006_OUTPUT_CONTROL_CEN);
	if (ret < 0)
		return ret;

	return mt9p006_pll_enable(mt9p006);
}

static int mt9p006_get_format(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_format *fmt)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);

	fmt->format = *__mt9p006_get_pad_format(mt9p006, fh, fmt->pad,
						fmt->which);
	return 0;
}

static int mt9p006_set_format(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_format *format)
{	   
	int ret=0;
        struct mt9p006 *mt9p006 = container_of(sd, struct mt9p006, subdev);
        struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	struct mt9p006_frame_size size;
	size.height = format->format.height;
	size.width = format->format.width;
	format->format.code = V4L2_MBUS_FMT_SGRBG12_1X12;	
	mt9p006_v4l2_try_fmt_cap(&size);
	mt9p006->format.width      = size.width;
	mt9p006->format.height    = size.height;	
	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev control operations
 */


static int mt9p006_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mt9p006 *mt9p006 =
			container_of(ctrl->handler, struct mt9p006, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&mt9p006->subdev);
	u16 data;
	int ret;
	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ret = reg_write(client, MT9P006_SHUTTER_WIDTH_UPPER,
				    (ctrl->val >> 16) & 0xffff);
		if (ret < 0)
			return ret;

		return reg_write(client, MT9P006_SHUTTER_WIDTH_LOWER,
				     ctrl->val & 0xffff);

	case V4L2_CID_GAIN:
		/* Gain is controlled by 2 analog stages and a digital stage.
		 * Valid values for the 3 stages are
		 *
		 * Stage                Min     Max     Step
		 * ------------------------------------------
		 * First analog stage   x1      x2      1
		 * Second analog stage  x1      x4      0.125
		 * Digital stage        x1      x16     0.125
		 *
		 * To minimize noise, the gain stages should be used in the
		 * second analog stage, first analog stage, digital stage order.
		 * Gain from a previous stage should be pushed to its maximum
		 * value before the next stage is used.
		 */
		if (ctrl->val <= 32) {
			data = ctrl->val;
		} else if (ctrl->val <= 64) {
			ctrl->val &= ~1;
			data = (1 << 6) | (ctrl->val >> 1);
		} else {
			ctrl->val &= ~7;
			data = ((ctrl->val - 64) << 5) | (1 << 6) | 32;
		}

		return reg_write(client, MT9P006_GLOBAL_GAIN, data);

	case V4L2_CID_HFLIP:
		if (ctrl->val)
			return mt9p006_set_mode2(mt9p006,
					0, MT9P006_READ_MODE_2_COL_MIR);
		else
			return mt9p006_set_mode2(mt9p006,
					MT9P006_READ_MODE_2_COL_MIR, 0);

	case V4L2_CID_VFLIP:
		if (ctrl->val)
			return mt9p006_set_mode2(mt9p006,
					0, MT9P006_READ_MODE_2_ROW_MIR);
		else
			return mt9p006_set_mode2(mt9p006,
					MT9P006_READ_MODE_2_ROW_MIR, 0);

	}
	return 0;
}

static struct v4l2_ctrl_ops mt9p006_ctrl_ops = {
	.s_ctrl = mt9p006_s_ctrl,
};
/* -----------------------------------------------------------------------------
 * V4L2 subdev core operations
 */

static int mt9p006_set_power(struct v4l2_subdev *subdev, int on)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);
	int ret = 0;

	mutex_lock(&mt9p006->power_lock);

	/* If the power count is modified from 0 to != 0 or from != 0 to 0,
	 * update the power state.
	 */
	if (mt9p006->power_count == !on) {
		ret = __mt9p006_set_power(mt9p006, !!on);
		if (ret < 0)
			goto out;
	}

	/* Update the power count. */
	mt9p006->power_count += on ? 1 : -1;
	WARN_ON(mt9p006->power_count < 0);

out:
	mutex_unlock(&mt9p006->power_lock);
	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev internal operations
 */

static int mt9p006_registered(struct v4l2_subdev *subdev)
{
	struct i2c_client *client = v4l2_get_subdevdata(subdev);
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);
	s32 data;
	int ret;

	ret = mt9p006_power_on(mt9p006);
	if (ret < 0) {
		dev_err(&client->dev, "MT9P006 power up failed\n");
		return ret;
	}

	/* Read out the chip version register */
	data = reg_read(client, MT9P006_CHIP_VERSION);
	if (data != MT9P006_CHIP_VERSION_VALUE) {
		dev_err(&client->dev, "MT9P006 not detected, wrong version "
			"0x%04x\n", data);
		return -ENODEV;
	}

	mt9p006_power_off(mt9p006);

	dev_info(&client->dev, "MT9P006 detected at address 0x%02x\n",
		 client->addr);

	return ret;
}

static int mt9p006_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	crop = v4l2_subdev_get_try_crop(fh, 0);
	crop->left = MT9P006_COLUMN_START_DEF;
	crop->top = MT9P006_ROW_START_DEF;
	crop->width = MT9P006_WINDOW_WIDTH_DEF;
	crop->height = MT9P006_WINDOW_HEIGHT_DEF;

	format = v4l2_subdev_get_try_format(fh, 0);

	format->code = V4L2_MBUS_FMT_SGRBG12_1X12;

	format->width = MT9P006_WINDOW_WIDTH_DEF;
	format->height = MT9P006_WINDOW_HEIGHT_DEF;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	mt9p006->xskip = 1;
	mt9p006->yskip = 1;
	return mt9p006_set_power(subdev, 1);
}

static int mt9p006_close(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	return mt9p006_set_power(subdev, 0);
}

static struct v4l2_subdev_core_ops mt9p006_subdev_core_ops = {
	.s_power        = mt9p006_set_power,
};

static struct v4l2_subdev_video_ops mt9p006_subdev_video_ops = {
	.s_stream       = mt9p006_s_stream,
};

static struct v4l2_subdev_pad_ops mt9p006_subdev_pad_ops = {
	.enum_mbus_code = mt9p006_enum_mbus_code,
	.enum_frame_size = mt9p006_enum_frame_size,
	.get_fmt = mt9p006_get_format,
	.set_fmt = mt9p006_set_format,
	//.get_crop = mt9p006_get_crop,
	//.set_crop = mt9p006_set_crop,
};

static struct v4l2_subdev_ops mt9p006_subdev_ops = {
	.core   = &mt9p006_subdev_core_ops,
	.video  = &mt9p006_subdev_video_ops,
	.pad    = &mt9p006_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops mt9p006_subdev_internal_ops = {
	.registered = mt9p006_registered,
	.open = mt9p006_open,
	.close = mt9p006_close,
};

/* -----------------------------------------------------------------------------
 * Driver initialization and probing
 */

static int mt9p006_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9p006_platform_data *pdata = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mt9p006 *mt9p006;
	unsigned int i;
	int ret;

	if (pdata == NULL) {
		dev_err(&client->dev, "No platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&client->dev,
			"I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	mt9p006 = kzalloc(sizeof(*mt9p006), GFP_KERNEL);
	if (mt9p006 == NULL)
		return -ENOMEM;

	mt9p006->pdata = pdata;
	mt9p006->output_control	= MT9P006_OUTPUT_CONTROL_DEF;
	mt9p006->mode2 = MT9P006_READ_MODE_2_ROW_BLC;

	v4l2_ctrl_handler_init(&mt9p006->ctrls, 4);

	v4l2_ctrl_new_std(&mt9p006->ctrls, &mt9p006_ctrl_ops,
			  V4L2_CID_EXPOSURE, MT9P006_SHUTTER_WIDTH_MIN,
			  MT9P006_SHUTTER_WIDTH_MAX, 1,
			  MT9P006_SHUTTER_WIDTH_DEF);
	v4l2_ctrl_new_std(&mt9p006->ctrls, &mt9p006_ctrl_ops,
			  V4L2_CID_GAIN, MT9P006_GLOBAL_GAIN_MIN,
			  MT9P006_GLOBAL_GAIN_MAX, 1, MT9P006_GLOBAL_GAIN_DEF);
	v4l2_ctrl_new_std(&mt9p006->ctrls, &mt9p006_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&mt9p006->ctrls, &mt9p006_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);

	mt9p006->subdev.ctrl_handler = &mt9p006->ctrls;

	if (mt9p006->ctrls.error)
		printk(KERN_INFO "%s: control initialization error %d\n",
		       __func__, mt9p006->ctrls.error);

	mutex_init(&mt9p006->power_lock);
	v4l2_i2c_subdev_init(&mt9p006->subdev, client, &mt9p006_subdev_ops);
	mt9p006->subdev.internal_ops = &mt9p006_subdev_internal_ops;

	mt9p006->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&mt9p006->subdev.entity, 1, &mt9p006->pad, 0);
	if (ret < 0)
		goto done;

	mt9p006->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	mt9p006->crop.width = MT9P006_WINDOW_WIDTH_DEF;
	mt9p006->crop.height = MT9P006_WINDOW_HEIGHT_DEF;
	mt9p006->crop.left = MT9P006_COLUMN_START_DEF;
	mt9p006->crop.top = MT9P006_ROW_START_DEF;

	mt9p006->format.code = V4L2_MBUS_FMT_SGRBG12_1X12;

	mt9p006->format.width = MT9P006_WINDOW_WIDTH_DEF;
	mt9p006->format.height = MT9P006_WINDOW_HEIGHT_DEF;
	mt9p006->format.field = V4L2_FIELD_NONE;
	mt9p006->format.colorspace = V4L2_COLORSPACE_SRGB;

	ret = mt9p006_pll_get_divs(mt9p006);

done:
	if (ret < 0) {
		v4l2_ctrl_handler_free(&mt9p006->ctrls);
		media_entity_cleanup(&mt9p006->subdev.entity);
		kfree(mt9p006);
	}

	return ret;
}

static int mt9p006_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct mt9p006 *mt9p006 = to_mt9p006(subdev);

	v4l2_ctrl_handler_free(&mt9p006->ctrls);
	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);
	kfree(mt9p006);

	return 0;
}

static const struct i2c_device_id mt9p006_id[] = {
	{ "mt9p006", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9p006_id);

static struct i2c_driver mt9p006_i2c_driver = {
	.driver = {
		.name = "mt9p006",
	},
	.probe          = mt9p006_probe,
	.remove         = mt9p006_remove,
	.id_table       = mt9p006_id,
};

static int __init mt9p006_mod_init(void)
{
	return i2c_add_driver(&mt9p006_i2c_driver);
}

static void __exit mt9p006_mod_exit(void)
{
	i2c_del_driver(&mt9p006_i2c_driver);
}

module_init(mt9p006_mod_init);
module_exit(mt9p006_mod_exit);

MODULE_DESCRIPTION("Aptina MT9P006 Camera driver");
MODULE_AUTHOR("Abhishek Kondaveeti <areddykondaveeti@aptina.com>");
MODULE_LICENSE("GPL v2");

