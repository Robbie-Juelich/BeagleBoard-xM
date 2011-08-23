/*
 * drivers/media/video/mt9p015.c
 * 
 * mt9p015.c - Aptina mt9p015 sensor driver
 *
 * Copyright (C) 2010 Aptina Imaging
 *
 * Leverage mt9p031.c
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA02139, USA
 *  
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <media/v4l2-int-device.h>

#include <media/mt9p015.h>
#include "mt9p015_regs.h"

#define DRIVER_NAME  "mt9p015"

//#define MT9P015_DEBUG
#undef MT9P015_DEBUG

#ifdef MT9P015_DEBUG
#define DPRINTK_DRIVER(format, ...)				\
	printk(KERN_INFO "_MT9P015_DRIVER: " format, ## __VA_ARGS__)
#else
#define DPRINTK_DRIVER(format, ...)
#endif

/* MT9P015 has 8/16/32 registers */
#define MT9P015_8BIT	1
#define MT9P015_16BIT	2
#define MT9P015_32BIT	4

/* terminating token for reg list */
#define MT9P015_TOK_TERM	0xFF

/* delay token for reg list */
#define MT9P015_TOK_DELAY	100

/* The ID values we are looking for */
#define MT9P015_MOD_ID	0x2803
#define MT9P015_MFR_ID	0x0006

/* FPS Capabilities */
#define MT9P015_MIN_FPS	13
#define MT9P015_DEF_FPS	30
#define MT9P015_MAX_FPS	60

#define MT9P015_I2C_RETRY_COUNT	5

#define MT9P015_XCLK_NOM	24000000 

/* Still capture 5 MP */
#define MT9P015_IMAGE_WIDTH_MAX	2592
#define MT9P015_IMAGE_HEIGHT_MAX	1944
/* Still capture 3 MP and down to VGA, using ISP resizer */
#define MT9P015_IMAGE_WIDTH_MIN	2048
#define MT9P015_IMAGE_HEIGHT_MIN	1536

/* Video mode, mode size for 1080p mode */
#define MT9P015_VIDEO_WIDTH_1080p	1920
#define MT9P015_VIDEO_HEIGHT_1080p	1080

/* Video mode, mode size for 720p mode */
#define MT9P015_VIDEO_WIDTH_720p	1280
#define MT9P015_VIDEO_HEIGHT_720p	720

/* Sensor Video mode size for VGA mode */
#define MT9P015_VIDEO_WIDTH_VGA	640
#define MT9P015_VIDEO_HEIGHT_VGA	480

/* Global gain values */
#define MT9P015_EV_MIN_GAIN	0
#define MT9P015_EV_MAX_GAIN	159
#define MT9P015_EV_DEF_GAIN	50
#define MT9P015_EV_GAIN_STEP	1
#define MT9P015_DEF_GAIN	0x10B0

/* Exposure time values */
#define MT9P015_DEF_MIN_EXPOSURE	2000
#define MT9P015_DEF_MAX_EXPOSURE	27000
#define MT9P015_DEF_EXPOSURE	17500
#define MT9P015_EXPOSURE_STEP	1

/**
 * struct mt9p015_reg - mt9p015 register format
 * @length: length of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for MT9P015 register initialization values
 */
struct mt9p015_reg {
	u16 length;
	u16 reg;
	u32 val;
};

enum mt9p015_image_size {
	MT9P015_VGA,
	MT9P015_720p,
	MT9P015_1080p,
	MT9P015_THREE_MP,
	MT9P015_FIVE_MP
};

#define MT9P015_NUM_IMAGE_SIZES	5
#define MT9P015_NUM_PIXEL_FORMATS	1
#define MT9P015_NUM_FPS	2	/* 2 ranges */
#define MT9P015_FPS_LOW_RANGE	0
#define MT9P015_FPS_HIGH_RANGE	1

/**
 * struct capture_size - image capture size information
 * @width: image width in pixels
 * @height: image height in pixels
 */
struct mt9p015_capture_size {
	unsigned long width;
	unsigned long height;
};

/**
 * struct mt9p015_pll_settings - struct for storage of sensor pll values
 * @vt_pix_clk_div: vertical pixel clock divider
 * @vt_sys_clk_div: veritcal system clock divider
 * @pre_pll_div: pre pll divider
 * @fine_int_tm: fine resolution interval time
 * @frame_lines: number of lines in frame
 * @line_len: number of pixels in line
 * @min_pll: minimum pll multiplier
 * @max_pll: maximum pll multiplier
 */
struct mt9p015_pll_settings {
	u16 vt_pix_clk_div;
	u16 vt_sys_clk_div;
	u16 pre_pll_div;

	u16 fine_int_tm;
	u16 frame_lines;
	u16 line_len;

	u16 min_pll;
	u16 max_pll;
};

/*
 * Array of image sizes supported by MT9P015.  These must be ordered from
 * smallest image size to largest.
 */
const static struct mt9p015_capture_size mt9p015_sizes[] = {
	{  640, 480 },	/* VGA */
	{ 1280, 720 },	/* 720p */
	{ 1920, 1080 }, /* 1080p */
	{ 2048, 1536 },	/* 3 MP */
	{ 2592, 1944 },	/* 5 MP */
};

/* PLL settings for MT9P015 */
enum mt9p015_pll_type {
	MT9P015_PLL_5MP = 0,
	MT9P015_PLL_1080p_30FPS,
	MT9P015_PLL_720p_30FPS,
	MT9P015_PLL_720p_60FPS,
	MT9P015_PLL_640_30FPS,
	MT9P015_PLL_640_60FPS,
};

/* Debug functions */
static int debug;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug Enabled (0-1)");

/**
 * struct mt9p015_sensor - main structure for storage of sensor information
 * @dev:
 * @pdata: access functions and data for platform level information
 * @v4l2_int_device: V4L2 device structure structure
 * @pix: V4L2 pixel format information structure
 * @timeperframe: time per frame expressed as V4L fraction
 * @scaler:
 * @ver: mt9p015 chip version
 * @fps: frames per second value
 */
struct mt9p015_sensor {
	struct device *dev;
	struct mt9p015_platform_data *pdata;
	struct i2c_client *client;
	struct v4l2_int_device *v4l2_int_device;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	int scaler;
	int ver;
	int fps;
	int detected;
	unsigned long xclk_current;
};

#ifdef MT9P015_DEBUG
struct mt9p015_sensor sysSensor;
#endif
/* list of image formats supported by mt9p015 sensor */
const static struct v4l2_fmtdesc mt9p015_formats[] = {
	{
		.description    = "Bayer10 (GrR/BGb)",
		.pixelformat    = V4L2_PIX_FMT_SGRBG10,
	}
};

#define NUM_CAPTURE_FORMATS ARRAY_SIZE(mt9p015_formats)

const u16 MT9P015_EV_GAIN_TBL[160] = {
	/* Gain x1 - 2x */
	4128, 4129, 4130, 4131, 4132, 4133, 4134, 4135, 4136, 4137, 4138, 4139, 4140, 4141, 4142, 4143, 
	4144, 4145, 4146, 4147, 4148, 4149, 4150, 4151, 4152, 4153, 4154, 4155, 4156, 4157, 4158, 4159, 
	/* Gain 2x - 4x */
	4256, 4257, 4258, 4259, 4260, 4261, 4262, 4263, 4264, 4265, 4266, 4267, 4268, 4269, 4270, 4271, 
	4272, 4273, 4274, 4275, 4276, 4277, 4278, 4279, 4280, 4281, 4282, 4283, 4284, 4285, 4286, 4287, 
	/* Gain 4x - 8x */
	4512, 4513, 4514, 4515, 4516, 4517, 4518, 4519, 4520, 4521, 4522, 4523, 4524, 4525, 4526, 4527, 
	4528, 4529, 4530, 4531, 4532, 4533, 4534, 4535, 4536, 4537, 4538, 4539, 4540, 4541, 4542, 4543, 
	/* Gain 8x - 16x */
	4544, 4545, 4546, 4547, 4548, 4549, 4550, 4551, 4552, 4553, 4554, 4555, 4556, 4557, 4558, 4559, 
	4560, 4561, 4562, 4563, 4564, 4565, 4566, 4567, 4568, 4569, 4570, 4571, 4572, 4573, 4574, 4575, 
	4576, 4577, 4578, 4579, 4580, 4581, 4582, 4583, 4584, 4585, 4586, 4587, 4588, 4589, 4590, 4591, 
	4592, 4593, 4594, 4595, 4596, 4597, 4598, 4599, 4600, 4601, 4602, 4603, 4604, 4605, 4606, 4607
};

/* Enters soft standby, all settings are maintained */
const static struct mt9p015_reg stream_off_list[] = {
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

/* Exits soft standby */
const static struct mt9p015_reg stream_on_list[] = {
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x01},
	/* Sensor datasheet says we need 1 ms to allow PLL lock */
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 1},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

/* Structure which will set the exposure time */
static struct mt9p015_reg set_exposure_time[] = {
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	/* less than frame_lines-1 */
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME, .val = 500},
	 /* updating */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

/* Structure to set global gain */
static struct mt9p015_reg set_global_gain[] = {
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_GAIN_GREEN1,
		.val = MT9P015_DEF_GAIN},
	{.length = MT9P015_16BIT, .reg = REG_GAIN_RED,
		.val = MT9P015_DEF_GAIN},
	{.length = MT9P015_16BIT, .reg = REG_GAIN_BLUE,
		.val = MT9P015_DEF_GAIN},
	{.length = MT9P015_16BIT, .reg = REG_GAIN_GREEN2,
		.val = MT9P015_DEF_GAIN},
	{.length = MT9P015_16BIT, .reg = REG_GAIN_GLOBAL,
		.val = MT9P015_DEF_GAIN},
	 /* updating */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0},
};

/*
 * Common MT9P015 register initialization for all image sizes, pixel formats,
 * and frame rates
 */
const static struct mt9p015_reg mt9p015_common[] = {
	{MT9P015_8BIT, REG_SOFTWARE_RESET, 0x01},
	{MT9P015_TOK_DELAY, 0x00, 5}, /* Delay = 5ms, min 2400 xcks */
	{MT9P015_16BIT, REG_RESET_REGISTER, 0x10C8},
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064,
		.val = 0x0805},
	{MT9P015_8BIT, REG_GROUPED_PAR_HOLD, 0x01}, /* hold */
	{MT9P015_16BIT, REG_CCP_DATA_FORMAT, 0x0C0C},
	{MT9P015_16BIT, REG_GAIN_GREEN1, MT9P015_DEF_GAIN},
	{MT9P015_16BIT, REG_GAIN_RED, MT9P015_DEF_GAIN},
	{MT9P015_16BIT, REG_GAIN_BLUE, MT9P015_DEF_GAIN},
	{MT9P015_16BIT, REG_GAIN_GREEN2, MT9P015_DEF_GAIN},
	{MT9P015_16BIT, REG_GAIN_GLOBAL, MT9P015_DEF_GAIN},
	{MT9P015_8BIT, REG_GROUPED_PAR_HOLD, 0x00}, /* update all at once */
	{MT9P015_TOK_TERM, 0, 0}
};

/*
 * mt9p015 register configuration for all combinations of pixel format and
 * image size
 */
	/*Video mode, 640 x 480, range 45 - 60 fps*/
const static struct mt9p015_reg enter_video_640_60fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_WIDTH_VGA},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_HEIGHT_VGA},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 0xA01},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 0x781},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x15C7},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x702},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x15C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x233},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0xB60},
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x2AE},
	/* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x01},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

	/* Video mode, 640 x 480, range 16 - 30 fps */
const static struct mt9p015_reg enter_video_640_30fps[] = {
/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_WIDTH_VGA},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_HEIGHT_VGA},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 0xA01},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 0x781},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x15C7},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x702},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x15C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x233},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0xB60},
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x2AE},
	/* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

	/* Video mode, scaler off: 1280 x 720, range  60 - 30 fps*/
const static struct mt9p015_reg enter_video_720p_60fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_WIDTH_720p},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_HEIGHT_720p},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 0x18},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 0x104},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 0xA15},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 0x6A1},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x14C3},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x0702},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x015C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x0323},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0x09BC},
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x0321},
	/* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

const static struct mt9p015_reg enter_video_720p_30fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_WIDTH_720p},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_HEIGHT_720p},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 0x18},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 0x104},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 0xA15},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 0x6A1},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x14C3},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x702},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x015C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x0323},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0x1374},
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x290},
	 /* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

const static struct mt9p015_reg enter_video_1080p_30fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_WIDTH_1080p},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_VIDEO_HEIGHT_1080p},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 344},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 440},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 2263},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 1519},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x0041},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 2551},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 156},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 1175},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 3404},
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x490},
	 /* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

const static struct mt9p015_reg enter_image_mode_3MP_15fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	/* 10 fps */
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_IMAGE_WIDTH_MIN},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_IMAGE_HEIGHT_MIN},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 8},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 8},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 2599},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 1951},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x0041},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x6CB},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x009C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x7ED},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0x11C8},
	{.length = MT9P015_16BIT, .reg = REG_SCALE_M, .val = 0x0014},
	/* enable scaler */
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0002},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x7EC},
	/* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

/* Image mode, 5 MP @ 15 fps */
const static struct mt9p015_reg enter_image_mode_5MP_15fps[] = {
	/* stream off */
	{.length = MT9P015_8BIT, .reg = REG_MODE_SELECT, .val = 0x00},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 100},
	/* hold */
	{.length = MT9P015_16BIT, .reg = REG_RESERVED_MFR_3064, .val = 0x0805},
	{.length = MT9P015_16BIT, .reg = 0x31AE, .val = 0x0201},
	{.length = MT9P015_16BIT, .reg = REG_VT_PIX_CLK_DIV, .val = 6},
	{.length = MT9P015_16BIT, .reg = REG_VT_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_16BIT, .reg = REG_PRE_PLL_CLK_DIV, .val = 2},
	/* 10 fps */
	{.length = MT9P015_16BIT, .reg = REG_PLL_MULTIPLIER, .val = 60},
	{.length = MT9P015_16BIT, .reg = REG_OP_PIX_CLK_DIV, .val = 10},
	{.length = MT9P015_16BIT, .reg = REG_OP_SYS_CLK_DIV, .val = 1},
	{.length = MT9P015_TOK_DELAY, .reg = 0x00, .val = 10},
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x01},
	{.length = MT9P015_16BIT, .reg = REG_X_OUTPUT_SIZE,
		.val = MT9P015_IMAGE_WIDTH_MAX},
	{.length = MT9P015_16BIT, .reg = REG_Y_OUTPUT_SIZE,
		.val = MT9P015_IMAGE_HEIGHT_MAX},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_START, .val = 0x8},
	{.length = MT9P015_16BIT, .reg = REG_X_ADDR_END, .val = 0xA27},
	{.length = MT9P015_16BIT, .reg = REG_Y_ADDR_END, .val = 0x79F},
	{.length = MT9P015_16BIT, .reg = REG_READ_MODE, .val = 0x0041},
	{.length = MT9P015_16BIT, .reg = REG_FINE_INT_TIME, .val = 0x6CB},
	{.length = MT9P015_16BIT, .reg = REG_FINE_CORRECT, .val = 0x009C},
	{.length = MT9P015_16BIT, .reg = REG_FRAME_LEN_LINES, .val = 0x7ED},
	{.length = MT9P015_16BIT, .reg = REG_LINE_LEN_PCK, .val = 0x11C8},
	{.length = MT9P015_16BIT, .reg = REG_SCALE_M, .val = 0x0010},
	/* disable scaler */
	{.length = MT9P015_16BIT, .reg = REG_SCALING_MODE, .val = 0x0000},
	{.length = MT9P015_16BIT, .reg = REG_COARSE_INT_TIME,
		.val = 0x7EC},
	/* update */
	{.length = MT9P015_8BIT, .reg = REG_GROUPED_PAR_HOLD, .val = 0x00},
	{.length = MT9P015_TOK_TERM, .reg = 0, .val = 0}
};

static u32 min_exposure_time;
static u32 max_exposure_time;
static u32 pix_clk_freq;

/**
 * struct mt9p015_pll_settings - struct for storage of sensor pll values
 * @vt_pix_clk_div: vertical pixel clock divider
 * @vt_sys_clk_div: veritcal system clock divider
 * @pre_pll_div: pre pll divider
 * @fine_int_tm: fine resolution interval time
 * @frame_lines: number of lines in frame
 * @line_len: number of pixels in line
 * @min_pll: minimum pll multiplier
 * @max_pll: maximum pll multiplier
 */
const static struct mt9p015_pll_settings all_pll_settings[] = {
	/* PLL_5MP */
	{.vt_pix_clk_div = 6, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 0x6CB, .frame_lines = 0x7ED, .line_len = 0x11C8,
	.min_pll = 36, .max_pll = 64},
	/* PLL_1080p_30FPS */
	{.vt_pix_clk_div = 6, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 2551, .frame_lines = 1175, .line_len = 3404,
	.min_pll = 36, .max_pll = 64},
	/* PLL_720p_60FPS */
	{.vt_pix_clk_div = 6, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 0x702, .frame_lines = 0x323, .line_len = 0x9BC,
	.min_pll = 36, .max_pll = 64},
	/* PLL_720p_30FPS */
	{.vt_pix_clk_div = 8, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 0x702, .frame_lines = 0x323, .line_len = 0x1374,
	.min_pll = 36, .max_pll = 64},
	/* PLL_640_60FPS */
	{.vt_pix_clk_div = 6, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 0x702, .frame_lines = 0x233, .line_len = 0xB60,
	.min_pll = 36, .max_pll = 64},
	/* PLL_640_30FPS */
	{.vt_pix_clk_div = 6, .vt_sys_clk_div = 1, .pre_pll_div = 2,
	.fine_int_tm = 0x702, .frame_lines = 0x233, .line_len = 0xB60,
	.min_pll = 36, .max_pll = 64},
};

static enum mt9p015_pll_type current_pll_video;

const static struct mt9p015_reg
		*mt9p015_reg_init[MT9P015_NUM_FPS][MT9P015_NUM_IMAGE_SIZES] = {
	{
		enter_video_640_30fps,
		enter_video_720p_30fps,
		enter_video_1080p_30fps,
		enter_image_mode_3MP_15fps,
		enter_image_mode_5MP_15fps
	},
	{
		enter_video_640_60fps,
		enter_video_720p_60fps,
		enter_video_1080p_30fps,
		enter_image_mode_3MP_15fps,
		enter_image_mode_5MP_15fps
	},
};

/**
 * struct vcontrol - Video controls
 * @v4l2_queryctrl: V4L2 VIDIOC_QUERYCTRL ioctl structure
 * @current_value: current value of this control
 */
static struct vcontrol {
	struct v4l2_queryctrl qc;
	int current_value;
} mt9p015_video_control[] = {
	{
		{
			.id = V4L2_CID_EXPOSURE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Exposure",
			.minimum = MT9P015_DEF_MIN_EXPOSURE,
			.maximum = MT9P015_DEF_MAX_EXPOSURE,
			.step = MT9P015_EXPOSURE_STEP,
			.default_value = MT9P015_DEF_EXPOSURE,
		},
		.current_value = MT9P015_DEF_EXPOSURE,
	},
	{
		{
			.id = V4L2_CID_GAIN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Gain",
			.minimum = MT9P015_EV_MIN_GAIN,
			.maximum = MT9P015_EV_MAX_GAIN,
			.step = MT9P015_EV_GAIN_STEP,
			.default_value = MT9P015_EV_DEF_GAIN,
		},
		.current_value = MT9P015_EV_DEF_GAIN,
	}
};

/**
 * find_vctrl - Finds the requested ID in the video control structure array
 * @id: ID of control to search the video control array
 *
 * Returns the index of the requested ID from the control structure array
 */
static int find_vctrl(int id)
{
	int i;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(mt9p015_video_control) - 1); i >= 0; i--)
		if (mt9p015_video_control[i].qc.id == id)
			break;
	if (i < 0)
		i = -EINVAL;
	return i;
}

/**
 * mt9p015_read_reg - Read a value from a register in an mt9p015 sensor device
 * @client: i2c driver client structure
 * @data_length: length of data to be read
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an mt9p015 sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9p015_read_reg(struct i2c_client *client, u16 data_length,
			    u16 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;
	if (data_length != MT9P015_8BIT && data_length != MT9P015_16BIT
					&& data_length != MT9P015_32BIT)
		return -EINVAL;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = (u8) (reg >> 8);;
	data[1] = (u8) (reg & 0xff);
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0) {
		msg->len = data_length;
		msg->flags = I2C_M_RD;
		err = i2c_transfer(client->adapter, msg, 1);
	}
	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == MT9P015_8BIT)
			*val = data[0];
		else if (data_length == MT9P015_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
				(data[1] << 16) + (data[0] << 24);
		return 0;
	}
	v4l_dbg(1, debug, client, "read from offset 0x%x error %d", reg, err);
	return err;
}
/**
 * mt9p015_write_reg - Write a value to a register in an mt9p015 sensor device
 * @client: i2c driver client structure
 * @data_length: length of data to be read
 * @reg: register address / offset
 * @val: value to be written to specified register
 *
 * Write a value to a register in an mt9p015 sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9p015_write_reg(struct i2c_client *client, u16 data_length,
			     u16 reg, u32 val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[6];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	if (data_length != MT9P015_8BIT && data_length != MT9P015_16BIT
					&& data_length != MT9P015_32BIT)
		return -EINVAL;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + data_length;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = (u8) (reg >> 8);;
	data[1] = (u8) (reg & 0xff);

	if (data_length == MT9P015_8BIT)
		data[2] = (u8) (val & 0xff);
	else if (data_length == MT9P015_16BIT) {
		data[2] = (u8) (val >> 8);
		data[3] = (u8) (val & 0xff);
	} else {
		data[2] = (u8) (val >> 24);
		data[3] = (u8) (val >> 16);
		data[4] = (u8) (val >> 8);
		data[5] = (u8) (val & 0xff);
	}

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;

	v4l_dbg(1, debug, client, "wrote 0x%x to offset 0x%x error %d", val,
							reg, err);
	if (retry <= MT9P015_I2C_RETRY_COUNT) {
		retry++;
		mdelay(20);
		goto again;
	}
	return err;
}

/**
 * mt9p015_write_regs - Initializes a list of MT9P015 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * Initializes a list of MT9P015 registers. The list of registers is
 * terminated by MT9P015_TOK_TERM.
 */
static int mt9p015_write_regs(struct i2c_client *client,
			      const struct mt9p015_reg reglist[])
{
	int err;
	const struct mt9p015_reg *next = reglist;

	for (; next->length != MT9P015_TOK_TERM; next++) {
		if (next->length == MT9P015_TOK_DELAY) {
			mdelay(next->val);
			continue;
		}

		err = mt9p015_write_reg(client, next->length,
						next->reg, next->val);
		if (err)
			return err;
	}
	return 0;
}

/**
 * mt9p015_set_exposure_time - sets exposure time per input value
 * @exp_time: exposure time to be set on device
 * @s: pointer to standard V4L2 device structure
 * @lvc: pointer to V4L2 exposure entry in video_controls array
 *
 * If the requested exposure time is within the allowed limits, the HW
 * is configured to use the new exposure time, and the video_controls
 * array is updated with the new current value.
 * The function returns 0 upon success.  Otherwise an error code is
 * returned.
 */
static int mt9p015_set_exposure_time(u32 exp_time, struct v4l2_int_device *s,
				    struct vcontrol *lvc)
{
	int err;
	struct mt9p015_sensor *sensor = s->priv;
	struct i2c_client *client = to_i2c_client(sensor->dev);
	u32 coarse_int_time = 0;

	if ((exp_time < min_exposure_time) ||
			(exp_time > max_exposure_time)) {
		dev_err(&client->dev, "Exposure time not within the "
			"legal range.\n");
		dev_err(&client->dev, "Min time %d us Max time %d us",
			min_exposure_time, max_exposure_time);
		return -EINVAL;
	}
	coarse_int_time = ((((exp_time / 10) * (pix_clk_freq / 1000)) / 1000) -
		(all_pll_settings[current_pll_video].fine_int_tm / 10)) /
		(all_pll_settings[current_pll_video].line_len / 10);

	dev_dbg(&client->dev, "coarse_int_time calculated = %d\n",
						coarse_int_time);

	set_exposure_time[1].val = coarse_int_time;
	err = mt9p015_write_regs(client, set_exposure_time);

	if (err)
		dev_err(&client->dev, "Error setting exposure time %d\n",
									err);
	else
		lvc->current_value = exp_time;

	return err;
}

/**
 * mt9p015_set_gain - sets sensor global gain per input value
 * @lineargain: global gain value to be set on device
 * @s: pointer to standard V4L2 device structure
 * @lvc: pointer to V4L2 global gain entry in video_controls array
 *
 * If the requested global gain is within the allowed limits, the HW
 * is configured to use the new gain value, and the video_controls
 * array is updated with the new current value.
 * The function returns 0 upon success.  Otherwise an error code is
 * returned.
 */
static int mt9p015_set_gain(u16 lineargain, struct v4l2_int_device *s,
			   struct vcontrol *lvc)
{
	int ret= 0, i;
	
	struct mt9p015_sensor *sensor = s->priv;
	struct i2c_client *client = to_i2c_client(sensor->dev);

	if (lineargain < MT9P015_EV_MIN_GAIN) {
		lineargain = MT9P015_EV_MIN_GAIN;
		v4l_err(client, "Gain lower than legal range.");
	}
	if (lineargain > MT9P015_EV_MAX_GAIN) {
		lineargain = MT9P015_EV_MAX_GAIN;
		v4l_err(client, "Gain out of legal range.");
	}
	for (i = 1; i < 6; i++)
		set_global_gain[i].val = MT9P015_EV_GAIN_TBL[lineargain];
	
	ret = mt9p015_write_regs(client, set_global_gain);
	if (ret) {
		dev_err(&client->dev, "Error setting gain.%d", ret);
		return ret;	
	} else {
		i = find_vctrl(V4L2_CID_GAIN);
		if (i >= 0) {
			lvc = &mt9p015_video_control[i];
			lvc->current_value = lineargain;
		}
	}

	return ret;
}

/**
 * mt9p015_calc_pll - Calculate PLL settings based on input image size
 * @isize: enum value corresponding to image size
 * @xclk: xclk value (calculate by mt9p015sensor_calc_xclk())
 * @sensor: pointer to sensor device information structure
 *
 * Calculates sensor PLL related settings (scaler, fps, pll_multiplier,
 * pix_clk_freq, min_exposure_time, max_exposure_time) based on input
 * image size.  
 */
static int mt9p015_calc_pll(enum mt9p015_image_size isize, unsigned long xclk,
			    struct mt9p015_sensor *sensor)
{
	int err = 0, row = 1, i = 0;
	unsigned int vt_pix_clk;
	unsigned int pll_multiplier;
	unsigned int exposure_factor, pix_clk_scaled;
	struct vcontrol *lvc;
	
	if (isize > MT9P015_1080p) {
		sensor->scaler = 0;
		sensor->fps = 13;
		current_pll_video = MT9P015_PLL_5MP;
		return 0;
	}
	else if (isize == MT9P015_1080p)  {
		sensor->scaler = 0;
		sensor->fps = 13;
		current_pll_video = MT9P015_PLL_1080p_30FPS;
	} 
	else if(isize == MT9P015_720p) {
		sensor->scaler = 0;
		if (sensor->fps > 30)
			current_pll_video = MT9P015_PLL_720p_60FPS;
		else
			current_pll_video = MT9P015_PLL_720p_30FPS;
	}
	else if(isize == MT9P015_VGA) {
		sensor->scaler = 0;
		if (sensor->fps > 30)
			current_pll_video = MT9P015_PLL_640_60FPS;
		else
			current_pll_video = MT9P015_PLL_640_30FPS;
	}
	/* Row adjustment */
	if (sensor->scaler)
		row = 2; /* Adjustment when using 4x binning and 12 MHz clk */
	/* Calculate the max and min exposure for a given pll */
	vt_pix_clk = sensor->fps *
		all_pll_settings[current_pll_video].frame_lines *
		all_pll_settings[current_pll_video].line_len;

	pll_multiplier =
		(((vt_pix_clk
		   * all_pll_settings[current_pll_video].vt_pix_clk_div
		   * all_pll_settings[current_pll_video].vt_sys_clk_div
		   * row) / xclk)
		   * all_pll_settings[current_pll_video].pre_pll_div) + 1;

	if (pll_multiplier < all_pll_settings[current_pll_video].min_pll)
		pll_multiplier = all_pll_settings[current_pll_video].min_pll;
	else if (pll_multiplier > all_pll_settings[current_pll_video].max_pll)
		pll_multiplier = all_pll_settings[current_pll_video].max_pll;

	pix_clk_freq = (xclk /
			(all_pll_settings[current_pll_video].pre_pll_div
			 * all_pll_settings[current_pll_video].vt_pix_clk_div
			 * all_pll_settings[current_pll_video].vt_sys_clk_div
			 * row)) * pll_multiplier;
	min_exposure_time = ((all_pll_settings[current_pll_video].fine_int_tm
			     * 1000000 / pix_clk_freq) + 1) * 100;
	exposure_factor = (all_pll_settings[current_pll_video].frame_lines - 1)
				* all_pll_settings[current_pll_video].line_len;
	exposure_factor += all_pll_settings[current_pll_video].fine_int_tm;
	exposure_factor *= 100;
	pix_clk_scaled = pix_clk_freq / 100;
	max_exposure_time = (exposure_factor / pix_clk_scaled) * 100;
	
	/* Update min/max for query control */
	i = find_vctrl(V4L2_CID_EXPOSURE);
	if (i >= 0) {
		lvc = &mt9p015_video_control[i];
		lvc->qc.minimum = min_exposure_time;
		lvc->qc.maximum = max_exposure_time;
	}
	
	return err;
}

/**
 * mt9p015_calc_size - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static enum mt9p015_image_size mt9p015_calc_size(unsigned int width,
						 unsigned int height)
{
	enum mt9p015_image_size isize;
	unsigned long pixels = width * height;

	for (isize = MT9P015_VGA; isize <= MT9P015_FIVE_MP; isize++) {
		if (mt9p015_sizes[isize].height *
					mt9p015_sizes[isize].width >= pixels) {
			return isize;
		}
	}

	return MT9P015_FIVE_MP;
}

/**
 * mt9p015_find_isize - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static enum mt9p015_image_size mt9p015_find_isize(unsigned int width)
{
	enum mt9p015_image_size isize;

	for (isize = MT9P015_VGA; isize <= MT9P015_FIVE_MP; isize++) {
		if (mt9p015_sizes[isize].width >= width)
			break;
	}

	return isize;
}
/**
 * mt9p015_find_fps_index - Find the best fps range match for a
 *  requested frame rate
 * @fps: desired frame rate
 * @isize: enum value corresponding to image size
 *
 * Find the best match for a requested frame rate.  The best match
 * is chosen between two fps ranges (16 - 30 and 10 - 15 fps) depending on
 * the image size. For image sizes larger than 720p, frame rate is fixed
 * at 15 fps.
 */
static unsigned int mt9p015_find_fps_index(unsigned int fps,
					   enum mt9p015_image_size isize)
{
	unsigned int index = MT9P015_FPS_LOW_RANGE;

	if (isize <= MT9P015_720p) {
		if (fps > 30)
			index = MT9P015_FPS_HIGH_RANGE;
	} else {
		if (fps > 13)
			index = MT9P015_FPS_HIGH_RANGE;
	}

	return index;
}

/**
 * mt9p015_calc_xclk - Calculate the required xclk frequency
 * @c: i2c client driver structure
 *
 * Given the image capture format in pix, the nominal frame period in
 * timeperframe, return the required xclk frequency, currently returning default frequency
 * Support for different xclk frequency could be implemented later
 */
static unsigned long mt9p015_calc_xclk(struct i2c_client *c)
{
	struct mt9p015_sensor *sensor = i2c_get_clientdata(c);
	struct v4l2_fract *timeperframe = &sensor->timeperframe;

	if (timeperframe->numerator == 0 ||
	    timeperframe->denominator == 0) {
		/* supply a default nominal_timeperframe */
		timeperframe->numerator = 1;
		timeperframe->denominator = MT9P015_DEF_FPS;
	}

	sensor->fps = timeperframe->denominator / timeperframe->numerator;
	if (sensor->fps < MT9P015_MIN_FPS)
		sensor->fps = MT9P015_MIN_FPS;
	else if (sensor->fps > MT9P015_MAX_FPS)
		sensor->fps = MT9P015_MAX_FPS;

	timeperframe->numerator = 1;
	timeperframe->denominator = sensor->fps;
	
	return MT9P015_XCLK_NOM;
}

/**
 * mt9p015_configure - Configure the mt9p015 for the specified image mode
 * @s: pointer to standard V4L2 device structure
 *
 * Configure the mt9p015 for a specified image size, pixel format, and frame
 * period.  xclk is the frequency (in Hz) of the xclk input to the mt9p015.
 * fps is the frame period (in seconds) expressed as a fraction.
 * Returns zero if successful, or non-zero otherwise.
 * The actual frame period is returned in fps.
 */
static int mt9p015_configure(struct v4l2_int_device *s)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix = &sensor->pix;
	struct i2c_client *client = to_i2c_client(sensor->dev);
	enum mt9p015_image_size isize;
	unsigned int fps_index;
	int err;

	isize = mt9p015_find_isize(pix->width);

	/* common register initialization */
	err = mt9p015_write_regs(client, mt9p015_common);
	if (err)
		return err;

	fps_index = mt9p015_find_fps_index(sensor->fps, isize);

	/* configure image size and pixel format */
	err = mt9p015_write_regs(client, mt9p015_reg_init[fps_index][isize]);
	if (err)
		return err;
	/* configure frame rate */
	err = mt9p015_calc_pll(isize, sensor->xclk_current, sensor);
	if (err)
		return err;

	/* configure streaming ON */
	err = mt9p015_write_regs(client, stream_on_list);

	return err;
}

/**
 * mt9p015_detect - Detect if an mt9p015 is present, and if so which revision
 * @client: pointer to the i2c client driver structure
 *
 * Detect if an mt9p015 is present, and if so which revision.
 * A device is considered to be detected if the manufacturer ID (MIDH and MIDL)
 * and the product ID (PID) registers match the expected values.
 * Any value of the version ID (VER) register is accepted.
 * Returns a negative error number if no device is detected, or the
 * non-negative value of the version ID register if a device is detected.
 */
static int mt9p015_detect(struct i2c_client *client)
{
	u32 model_id, mfr_id, rev;

	if (!client)
		return -ENODEV;
    if (mt9p015_read_reg(client, MT9P015_16BIT, REG_MODEL_ID, &model_id))
		return -ENODEV;
	if (mt9p015_read_reg(client, MT9P015_8BIT, REG_MANUFACTURER_ID,
				&mfr_id))
		return -ENODEV;
	if (mt9p015_read_reg(client, MT9P015_8BIT, REG_REVISION_NUMBER, &rev))
		return -ENODEV;

	dev_info(&client->dev, "model id detected 0x%x mfr 0x%x\n", model_id,
								mfr_id);
								
	if ((model_id != MT9P015_MOD_ID) || (mfr_id != MT9P015_MFR_ID)) {
		/* We didn't read the values we expected, so
		 * this must not be an MT9P015.
		 */
		dev_warn(&client->dev, "model id mismatch 0x%x mfr 0x%x\n",
			model_id, mfr_id);

		return -ENODEV;
	}
	return 0;

}

/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s, struct v4l2_queryctrl *qc)
{
	int i;

	i = find_vctrl(qc->id);
	if (i == -EINVAL)
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

	if (i < 0)
		return -EINVAL;

	*qc = mt9p015_video_control[i].qc;
	return 0;
}

/**
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct vcontrol *lvc;
	int i;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];

	switch (vc->id) {
	case  V4L2_CID_EXPOSURE:
		vc->value = lvc->current_value;
		break;
	case V4L2_CID_GAIN:
		vc->value = lvc->current_value;
		break;
	}

	return 0;
}

/**
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = -EINVAL;
	int i;
	struct vcontrol *lvc;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];

	switch (vc->id) {
	case V4L2_CID_EXPOSURE:
		retval = mt9p015_set_exposure_time(vc->value, s, lvc);
		break;
	case V4L2_CID_GAIN:
		retval = mt9p015_set_gain(vc->value, s, lvc);
		break;
	}

	return retval;
}

/**
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (index >= NUM_CAPTURE_FORMATS)
			return -EINVAL;
	break;
	default:
		return -EINVAL;
	}

	fmt->flags = mt9p015_formats[index].flags;
	strlcpy(fmt->description, mt9p015_formats[index].description,
					sizeof(fmt->description));
	fmt->pixelformat = mt9p015_formats[index].pixelformat;

	return 0;
}

/**
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int ioctl_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	enum mt9p015_image_size isize;
	int ifmt;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct mt9p015_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix2 = &sensor->pix;

	isize = mt9p015_calc_size(pix->width, pix->height);

	pix->width = mt9p015_sizes[isize].width;
	pix->height = mt9p015_sizes[isize].height;
	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (pix->pixelformat == mt9p015_formats[ifmt].pixelformat)
			break;
	}
	if (ifmt == NUM_CAPTURE_FORMATS)
		ifmt = 0;
	pix->pixelformat = mt9p015_formats[ifmt].pixelformat;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = pix->width * 2;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	*pix2 = *pix;
	return 0;
}

/**
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int ioctl_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int rval;

	rval = ioctl_try_fmt_cap(s, f);
	if (!rval)
		sensor->pix = *pix;

	return rval;
}

/**
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct mt9p015_sensor *sensor = s->priv;
	f->fmt.pix = sensor->pix;

	return 0;
}

/**
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = sensor->timeperframe;

	return 0;
}

/**
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct i2c_client *client = to_i2c_client(sensor->dev);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	sensor->timeperframe = *timeperframe;
	sensor->xclk_current = mt9p015_calc_xclk(client);
	*timeperframe = sensor->timeperframe;

	return 0;
}

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
	struct mt9p015_sensor *sensor = s->priv;

	return sensor->pdata->priv_data_set(p);
}

/**
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Nothing implemented as yet
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

/**
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the dev. at slave detach.  The complement of ioctl_dev_init.
 * Nothing implemented as yet
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	return 0;
}

/**
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.  Returns 0 if
 * mt9p015 device could be found, otherwise returns appropriate error.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct i2c_client *client = to_i2c_client(sensor->dev);
	int err;

	mdelay(10);	//time to stabilize power settings
	err = mt9p015_detect(client);
	if (err < 0) {
		dev_err(&client->dev, "Unable to detect sensor\n");
		sensor->detected = 0;
		return err;
	}
	sensor->detected = 1;
	sensor->ver = err;
	dev_dbg(&client->dev, "mt9p015 sensor with chip version 0x%02x detected\n", sensor->ver);

	return 0;
}
/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *frms)
{
	int ifmt;

	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (frms->pixel_format == mt9p015_formats[ifmt].pixelformat) {
			DPRINTK_DRIVER("found a matched pixelformat:0x%x at table entry %d\n",mt9p015_formats[ifmt].pixelformat,ifmt);
			break;
		}
	}
	
	/* Do we already reached all discrete framesizes? */
	if (frms->index >= ARRAY_SIZE(mt9p015_sizes)){
		DPRINTK_DRIVER("We've already reached all discrete framesizes %d!\n", ARRAY_SIZE(mt9p015_sizes));
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = mt9p015_sizes[frms->index -1].width;
		frms->discrete.height = mt9p015_sizes[frms->index-1].height;
		return -EINVAL;
	}

	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frms->discrete.width = mt9p015_sizes[frms->index].width;
	frms->discrete.height = mt9p015_sizes[frms->index].height;

	return 0;
}

const struct v4l2_fract mt9p015_frameintervals[] = {
	{  .numerator = 1, .denominator = 13 },
	{  .numerator = 1, .denominator = 30 },
};
/**
 * ioctl_enum_frameintervals - V4L2 sensor if handler for vidioc_int_enum_frameintervals
 * @s: pointer to standard V4L2 device structure
 * @frmi: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible frameintervals depending on choosen pixel format or returns error code if reached all sizes
 **/
static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
				     struct v4l2_frmivalenum *frmi)
{
	int ifmt;

	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (frmi->pixel_format == mt9p015_formats[ifmt].pixelformat)
			break;
	}
	
	if(frmi->index==ARRAY_SIZE(mt9p015_sizes)) return -EINVAL;
	
	/* Do we already reached all discrete framesizes? */
	if (((frmi->width == mt9p015_sizes[4].width) &&
				(frmi->height == mt9p015_sizes[4].height)) ||
				((frmi->width == mt9p015_sizes[3].width) &&
				(frmi->height == mt9p015_sizes[3].height)) ||
				((frmi->width == mt9p015_sizes[2].width) &&
				(frmi->height == mt9p015_sizes[2].height))) {
		if (frmi->index != 0)
			return -EINVAL;
	} else {
		if (frmi->index >= ARRAY_SIZE(mt9p015_frameintervals))
			return -EINVAL;
	}
	frmi->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frmi->discrete.numerator =
				mt9p015_frameintervals[frmi->index].numerator;
	frmi->discrete.denominator =
				mt9p015_frameintervals[frmi->index].denominator;

	return 0;
}

/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power new_power)
{
	struct mt9p015_sensor *sensor = s->priv;
	struct i2c_client *c = to_i2c_client(sensor->dev);
	int rval = 0;

	switch (new_power) {
	case V4L2_POWER_ON:
		rval = sensor->pdata->power_set(s, V4L2_POWER_ON);
		if (rval)
			break;
		if (sensor->detected)
			mt9p015_configure(s);
		else {
			rval = ioctl_dev_init(s);
			if (rval)
				goto err_on;
		}
		break;
	case V4L2_POWER_OFF:
err_on:
		rval |= sensor->pdata->power_set(s, V4L2_POWER_OFF);
		sensor->pdata->set_xclk(s, 0);
		break;
	case V4L2_POWER_STANDBY:
		if (sensor->detected)
			mt9p015_write_regs(c, stream_off_list);
		rval = sensor->pdata->power_set(s, V4L2_POWER_STANDBY);
		sensor->pdata->set_xclk(s, 0);
		break;
	default:
		return -EINVAL;
	}

	return rval;
}

static struct v4l2_int_ioctl_desc mt9p015_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_framesizes },
	{ .num = vidioc_int_enum_frameintervals_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_frameintervals },
	{ .num = vidioc_int_dev_init_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_dev_init },
	{ .num = vidioc_int_dev_exit_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_dev_exit },
	{ .num = vidioc_int_s_power_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_power },
	{ .num = vidioc_int_g_priv_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_priv },
	{ .num = vidioc_int_init_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_init },
	{ .num = vidioc_int_enum_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_fmt_cap },
	{ .num = vidioc_int_s_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_parm },
	{ .num = vidioc_int_s_parm_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_parm },
	{ .num = vidioc_int_queryctrl_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_queryctrl },
	{ .num = vidioc_int_g_ctrl_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_ctrl },
	{ .num = vidioc_int_s_ctrl_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_ctrl },
};

#ifdef MT9P015_DEBUG
/**
 * ---------------------------------------------------------------------------------
 * Sysfs
 * ---------------------------------------------------------------------------------
 */

/* Basic register read write support */
static u16 mt9p015_attr_basic_addr  = 0x0000;

static ssize_t
mt9p015_basic_reg_addr_show( struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\n", mt9p015_attr_basic_addr);
}

static ssize_t
mt9p015_basic_reg_addr_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    u32 val;
    sscanf(buf, "%x", &val);
    mt9p015_attr_basic_addr = (u16) val;
    return n;
}

static DEVICE_ATTR( basic_reg_addr, S_IRUGO|S_IWUSR, mt9p015_basic_reg_addr_show, mt9p015_basic_reg_addr_store);


static ssize_t
mt9p015_basic_reg_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
    u16 val;
    if (mt9p015_read_reg(sysSensor.client, MT9P015_8BIT, mt9p015_attr_basic_addr, (u32 *)&val)) {
        printk(KERN_ERR "mt9p015: Basic register read failed");
        return 0; // nothing processed
    } else {
        return sprintf(buf, "0x%x\n", val);
    }
}

static ssize_t
mt9p015_basic_reg_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    u32 val;
    sscanf(buf, "%x", &val);

    if (mt9p015_write_reg(sysSensor.client, MT9P015_8BIT, mt9p015_attr_basic_addr, (u16)val)) {
        printk(KERN_INFO "mt9p015: Basic regiser write failed");
        return n; // nothing processed
    } else {
        return n;
    }
}
static DEVICE_ATTR( basic_reg_val, S_IRUGO|S_IWUSR, mt9p015_basic_reg_val_show, mt9p015_basic_reg_val_store);

/* Advanced register read write support */
static u16 mt9p015_attr_adv_addr  = 0x0000;

static ssize_t
mt9p015_adv_reg_addr_show( struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\n", mt9p015_attr_adv_addr);
}

static ssize_t
mt9p015_adv_reg_addr_store(  struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    sscanf(buf, "%x", (unsigned int *)&mt9p015_attr_adv_addr);
    return n;
}


static DEVICE_ATTR( adv_reg_addr, S_IRUGO|S_IWUSR, mt9p015_adv_reg_addr_show, mt9p015_adv_reg_addr_store);


static ssize_t
mt9p015_adv_reg_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 val;
    if (mt9p015_read_reg(sysSensor.client, MT9P015_16BIT, mt9p015_attr_adv_addr, &val)) {
        printk(KERN_ERR "mt9p015: Adv register read failed");
        return 0; // nothing processed
    } else {
        return sprintf(buf, "0x%x\n", val);
    }
}

static ssize_t
mt9p015_adv_reg_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
    u32 val;
    sscanf(buf, "%x", &val);

    if (mt9p015_write_reg(sysSensor.client, MT9P015_16BIT, mt9p015_attr_adv_addr, (u16)val)) {
        printk(KERN_INFO "mt9p015: Adv regiser write failed");
        return n; // nothing processed
    } else {
        return n;
    }
}

static DEVICE_ATTR( adv_reg_val, S_IRUGO|S_IWUSR, mt9p015_adv_reg_val_show, mt9p015_adv_reg_val_store);

/* Exposure time access support */
static ssize_t
mt9p015_exposure_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 val;
	struct vcontrol *lvc;
	int i = find_vctrl(V4L2_CID_EXPOSURE);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];
	val = lvc->current_value;
	
	if(val < 0){        
		printk(KERN_INFO "mt9p015: Exposure value read failed");
		return 1; // nothing processed
	} else {
		return sprintf(buf, "%d\n", val);
	}
}


static ssize_t
mt9p015_exposure_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u32 val;
	struct v4l2_int_device *s;
	struct vcontrol *lvc;
	int i;
	
	i = find_vctrl(V4L2_CID_EXPOSURE);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];
	
	sscanf(buf, "%d", &val);
	s = sysSensor.v4l2_int_device;
		
	if (mt9p015_set_exposure_time((u32)val, s, lvc)) {
		printk(KERN_INFO "mt9p015: Exposure write failed");
		return n; // nothing processed
	} else {
		return n;
    }

}

static DEVICE_ATTR( exposure_val, S_IRUGO|S_IWUSR, mt9p015_exposure_val_show, mt9p015_exposure_val_store);


/* Global Gain access support */
static ssize_t
mt9p015_gain_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 val;
	struct vcontrol *lvc;
    int i;
    
	i = find_vctrl(V4L2_CID_GAIN);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];
	val = lvc->current_value;
      
	if(val < 0){        
		printk(KERN_INFO "mt9p015: Global Gain value read failed");
		return 1; // nothing processed
	} else {
		return sprintf(buf, "%d\n", val);
    }
}

static ssize_t
mt9p015_gain_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u16 val;
	struct v4l2_int_device *s;
	struct vcontrol *lvc;
	int i;
	
	sscanf(buf, "%hd", &val);
	s = sysSensor.v4l2_int_device;
		
	i = find_vctrl(V4L2_CID_GAIN);
	if (i < 0)
		return -EINVAL;
	lvc = &mt9p015_video_control[i];
		
	if (mt9p015_set_gain(val, s, lvc)) {
		printk(KERN_INFO "mt9p015: Global gain write failed");
		return n; // nothing processed
	} else {
		return n;
	}
}

static DEVICE_ATTR( gain_val, S_IRUGO|S_IWUSR, mt9p015_gain_val_show, mt9p015_gain_val_store);

static struct attribute *mt9p015_sysfs_attr[] = {
    &dev_attr_basic_reg_addr.attr,
    &dev_attr_basic_reg_val.attr,
    &dev_attr_adv_reg_addr.attr,
    &dev_attr_adv_reg_val.attr,
    &dev_attr_exposure_val.attr,
	&dev_attr_gain_val.attr,
};

static int mt9p015_sysfs_add(struct kobject *kobj)
{
    int i = ARRAY_SIZE(mt9p015_sysfs_attr);
    int rval = 0;

    do {
        rval = sysfs_create_file(kobj, mt9p015_sysfs_attr[--i]);
    } while((i > 0) && (rval == 0));
    return rval;
}

static int mt9p015_sysfs_rm(struct kobject *kobj)
{
    int i = ARRAY_SIZE(mt9p015_sysfs_attr);
    int rval = 0;

    do {
        sysfs_remove_file(kobj, mt9p015_sysfs_attr[--i]);
    } while(i > 0);
    return rval;
}

#endif
/**
 * ---------------------------------------------------------------------------------
 * Device Driver Init & Exit
 * ---------------------------------------------------------------------------------
 */

static struct v4l2_int_slave mt9p015_slave = {
	.ioctls = mt9p015_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(mt9p015_ioctl_desc),
};

static struct v4l2_int_device mt9p015_int_device = {
	.module = THIS_MODULE,
	.name = DRIVER_NAME,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &mt9p015_slave,
	},
};

/**
 * mt9p015_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int mt9p015_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mt9p015_sensor *sensor;
	struct mt9p015_platform_data *pdata;
	int err;

	if (i2c_get_clientdata(client))
		return -EBUSY;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "no platform data?\n");
		return -EINVAL;
	}
	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	/* Don't keep pointer to platform data, copy elements instead */
	sensor->pdata = kzalloc(sizeof(*sensor->pdata), GFP_KERNEL);
	if (!sensor->pdata) {
		err = -ENOMEM;
		goto on_err1;
	}

	sensor->pdata->power_set = pdata->power_set;
	sensor->pdata->set_xclk = pdata->set_xclk;
	sensor->pdata->priv_data_set = pdata->priv_data_set;
	sensor->pdata->flags = MT9P015_FLAG_PCLK_RISING_EDGE;
	/* Set sensor default values */
	sensor->timeperframe.numerator = 1;
	sensor->timeperframe.denominator = 30;
	sensor->xclk_current = MT9P015_XCLK_NOM;
	sensor->pix.width = MT9P015_VIDEO_WIDTH_VGA;
	sensor->pix.height = MT9P015_VIDEO_WIDTH_VGA;
	sensor->pix.pixelformat = mt9p015_formats[0].pixelformat;

	sensor->v4l2_int_device = &mt9p015_int_device;
	sensor->v4l2_int_device->priv = sensor;
	sensor->dev = &client->dev;
	sensor->client = client;

	i2c_set_clientdata(client, sensor);

	err = v4l2_int_device_register(sensor->v4l2_int_device);
	if (err) {
		goto on_err2;
	}

#ifdef MT9P015_DEBUG	
	sysSensor.client = sensor->client;
	sysSensor.dev = &client->dev;
	sysSensor.v4l2_int_device = &mt9p015_int_device;
	sysSensor.v4l2_int_device->priv = sensor;
#endif
#ifdef MT9P015_DEBUG
	mt9p015_sysfs_add(&client->dev.kobj);
#endif	//MT9P015_DEBUG	

	return 0;
on_err2:
	i2c_set_clientdata(client, NULL);
	kfree(sensor->pdata);
on_err1:
	kfree(sensor);
	return err;
}

/**
 * mt9p015_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of mt9p015_probe().
 */
static int mt9p015_remove(struct i2c_client *client)
{
	struct mt9p015_sensor *sensor = i2c_get_clientdata(client);

	v4l2_int_device_unregister(sensor->v4l2_int_device);
	i2c_set_clientdata(client, NULL);
#ifdef MT9P015_DEBUG	
	mt9p015_sysfs_rm(&client->dev.kobj);
#endif	//MT9P015_DEBUG	
	kfree(sensor->pdata);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id mt9p015_id[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, mt9p015_id);

static struct i2c_driver mt9p015_i2c_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mt9p015_probe,
	.remove = mt9p015_remove,
	.id_table = mt9p015_id,
};

/**
 * mt9p015sensor_init - sensor driver module_init handler
 *
 * Registers driver as an i2c client driver.  Returns 0 on success,
 * error code otherwise.
 */
static int __init mt9p015_init(void)
{
	return i2c_add_driver(&mt9p015_i2c_driver);
}
module_init(mt9p015_init);

/**
 * mt9p015sensor_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes driver as an i2c client driver.
 * Complement of mt9p015sensor_init.
 */
static void __exit mt9p015_cleanup(void)
{
	i2c_del_driver(&mt9p015_i2c_driver);
}
module_exit(mt9p015_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mt9p015 camera sensor driver");
