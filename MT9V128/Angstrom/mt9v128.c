/*
 * drivers/media/video/mt9v128.c
 *
 * Aptina MT9V128/SoC356 SoC sensor driver
 *
 * Copyright (C) 2011 Aptina Imaging
 * 
 * Leverage mt9v128.c
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>

#include <media/mt9v128.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "mt9v128_regs.h"

#define MT9V128_RST_REG			0x301A
#define MT9V128_SW_RST			0x001A
#define MT9V128_RST_ENABLE		0x0001
#define MT9V128_RST_DISABLE		0x0000
#define MT9V128_CHIP_VERSION		0x00
#define MT9V128_CHIP_VERSION_VALUE	0x2281
#define MT9V128_I2C_CONTROL		0x0006
#define MT9V128_PLL_DIVIDERS		0x0010
#define MT9V128_PLL_P_DIVIDERS		0x0012
#define MT9V128_PLL_CONTROL		0x0014
#define MT9V128_PLL_CONTROL_PWROFF	0x5844
#define MT9V128_PLL_CONTROL_PWRON	0x5846

#define MT9V128_CLOCKS_CONTROL			0x0016	
#define MT9V128_STANDBY_CONTROL_AND_STATUS	0x0018
#define MT9V128_RESET_AND_MISC_CONTROL		0x001A
#define MT9V128_MCU_BOOT_MODE			0x001C	
#define MT9V128_INITIALIZE_SENSOR		0x0026
#define MT9V128_PAD_SLEW			0x0030
#define MT9V128_PAD_CONTROL			0x0032
#define MT9V128_PAD_GPI_STATUS			0x0034
#define MT9V128_COMMAND_REGISTER		0x40
/*
#define MT9V128_K22B_RTL_VER			0x0050
#define MT9V128_BIST_CONTROL
#define MT9V128_BIST_TEST
#define MT9V128_BIST_TEST_RESUME
#define MT9V128_BIST_DEBUG
#define MT9V128_BIST_HOLD
#define MT9V128_BIST_TEST_DONE
#define MT9V128_BIST_FAIL
#define MT9V128_BIST_START_RETENTION
*/

#define MT9V128_LOGICAL_ADDRESS		0x098E
#define MT9V128_LOGICAL_DATA		0x0990

#define MT9V128_WINDOW_WIDTH_DEF	640
#define MT9V128_WINDOW_HEIGHT_DEF	480
#define MT9V128_COLUMN_START_DEF	0
#define MT9V128_ROW_START_DEF		0
#define MT9V128_MAX_WIDTH		722
#define MT9V128_MAX_HEIGHT		486

/* Zoom Capbilities */
#define MT9V128_MIN_ABSZOOM		0
#define MT9V128_MAX_ABSZOOM		16
#define MT9V128_DEF_ABSZOOM		0
#define MT9V128_ABSZOOM_STEP		2

#undef MT9V128_TEST_EN
#define MT9V128_TEST_MODE_REG	0x3070
#define MT9V128_TEST_PATTERN	0x0003
#define MT9V128_TEST_RED	0x3072
#define MT9V128_TEST_GREEN_R	0x3074
#define MT9V128_TEST_BLUE	0x3076
#define MT9V128_TEST_GREEN_B	0x3078

//#define MT9V128_HEADBOARD
#undef MT9V128_HEADBOARD

/*
 * Logical address
 */
#define _VAR(id, offset, base)	(base | (id & 0x1F) << 10 | (offset & 0x3FF))
#define VAR_L(id, offset)  _VAR(id, offset, 0x0000)
#define VAR(id, offset) _VAR(id, offset, 0x8000)

struct mt9v128_pll_divs {
	u32 ext_freq;
       	u32 target_freq;
       	u8 m;
       	u8 n;
       	u8 p1;
};

struct mt9v128_frame_size {
	u16 width;
	u16 height;
};

struct mt9v128 {
       	struct v4l2_subdev subdev;
       	struct media_pad pad;
       	struct v4l2_rect rect;  /* Sensor window */
	struct v4l2_rect curr_rect;
       	struct v4l2_mbus_framefmt format;
       	struct v4l2_ctrl_handler ctrls;
       	struct mt9v128_platform_data *pdata;
       	struct mutex power_lock; /* lock to protect power_count */
       	int power_count;
       	u16 xskip;
       	u16 yskip;
       	u32 ext_freq;
       	/* pll dividers */
	
       	u8 m;
       	u8 n;
       	u8 p1;
       	/* cache register values */
 	u16 output_control;

};

/**************************supported sizes******************************/
const static struct mt9v128_frame_size   mt9v128_supported_framesizes[]={
	{  80,  60 },
	{ 160, 120 },
	{ 176, 144 },
	{ 320, 240 },
	{ 352, 288 },
	{ 400, 300 },
	{ 640, 480 },
	{ 722, 486 },
};

#ifdef MT9V128_HEADBOARD
/**
 * mt9v128_config_PCA9543A - configure on-board I2c level-shifter PCA9543A of MT9V128 Headboards from Aptina
 * @client: pointer to i2c client
 * Configures the level shifter to enable channel 0 
 */
static int 
mt9v128_config_PCA9543A(const struct i2c_client *client)
{
	struct	i2c_msg msg;
	int	ret;
	u8	buf= 0x21;
	
	msg.addr  = (0xE6 >> 1);	//slave address of PCA9543A
	msg.flags = 0;
	msg.len   = 1;
	msg.buf   = &buf;
	
	ret = i2c_transfer(client->adapter, &msg, 1);

	return 0;
		
}
#endif /* MT9V128_HEADBOARD */

/**
 * to_mt9v128 - A helper function which returns pointer to the private data structure
 * @client: pointer to i2c client
 * 
 */
static struct mt9v128 *to_mt9v128(const struct i2c_client *client)
{
       	return container_of(i2c_get_clientdata(client), struct mt9v128, subdev);
}

/**
 * reg_read - reads the data from the given register
 * @client: pointer to i2c client
 * @command: address of the register which is to be read
 *
 */
static int reg_read(struct i2c_client *client, const u16 command)
{
	struct i2c_msg msg[2];
	u16 val;
	u8 buf[2];
	int ret;

	/* 16 bit addressable register */
	buf[0] = command >> 8;
	buf[1] = command & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;
	ret = i2c_transfer(client->adapter, &msg[0], 1);

	if (ret >= 0) {
		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD; //1
		msg[1].len = 2;
		msg[1].buf = buf;
		ret = i2c_transfer(client->adapter, &msg[1], 1);
	}
	/*
	 * if return value of this function is < 0,
	 * it mean error.
	 * else, under 16bit is valid data.
	 */
	if (ret >= 0) {
		val = 0;
		val = buf[1] + (buf[0] << 8);
		return val;
	}

	v4l_err(client, "read from offset 0x%x error %d", command, ret);
	return ret;
}

/**
 * reg_write - writes the data into the given register
 * @client: pointer to i2c client
 * @command: address of the register in which to write
 * @data: data to be written into the register
 *
 */
static int reg_write(struct i2c_client *client, const u16 command,
                       u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	/* 16-bit addressable register */
	buf[0] = command >> 8;
	buf[1] = command & 0xff;

	data = swab16(data);
	memcpy(buf + 2, &data, 2);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = buf;

	/* i2c_transfer return message length, but this function should return 0 if correct case */
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;
	else
		v4l_err(client, "Write failed at 0x%x error %d", command, ret);
	
	return ret;
}

/**
 * mt9v128_calc_size - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static int mt9v128_calc_size(unsigned int request_width,
		unsigned int request_height) {
	int i = 0;
	unsigned long requested_pixels = request_width * request_height;
	for (i = 0; i < ARRAY_SIZE(mt9v128_supported_framesizes); i++) {
		if (mt9v128_supported_framesizes[i].height
				* mt9v128_supported_framesizes[i].width >= requested_pixels)
			return i;
	}
	/* couldn't find a match, return the max size as a default */
	return (ARRAY_SIZE(mt9v128_supported_framesizes) - 1);
}

/**
 * mt9v128_v4l2_try_fmt_cap - Find the best match for a requested image capture size
 * @requestedsize: a pointer to the structure which specifies requested image size
 * 
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static int mt9v128_v4l2_try_fmt_cap(struct mt9v128_frame_size *requstedsize)
{

	int isize;
		
	isize = mt9v128_calc_size(requstedsize->width,requstedsize->height);
	requstedsize->width = mt9v128_supported_framesizes[isize].width;
	requstedsize->height = mt9v128_supported_framesizes[isize].height;
	
	return 0;
}

/**
 * mt9v128_reset - Soft resets the sensor
 * @client: pointer to i2c client
 * 
 */
static int mt9v128_reset(struct i2c_client *client)
{
       	int ret = 0;
	u16 data;

	data = reg_read(client, MT9V128_SW_RST);
	data |= 0x0001;
       	ret = reg_write(client, MT9V128_SW_RST, data);
       	if (ret <  0)
               	return ret;
	
	mdelay(20);

	data &= 0xFFFE;
       	ret |= reg_write(client, MT9V128_SW_RST, data);

	mdelay(20);

	return ret;
}

/**
 * mt9v128_power_on - power on the sensor
 * @mt9v128: pointer to private data structure
 * 
 */
static int mt9v128_power_on(struct mt9v128 *mt9v128)
{
       	struct i2c_client *client = v4l2_get_subdevdata(&mt9v128->subdev);
       	int ret;

	/* Ensure RESET_BAR is low */
       	if (mt9v128->pdata->reset) {
	       	mt9v128->pdata->reset(&mt9v128->subdev, 1);
	       	msleep(1);
       	}
       	/* Enable clock */
       	if (mt9v128->pdata->set_xclk)
			mt9v128->pdata->set_xclk(&mt9v128->subdev, mt9v128->pdata->ext_freq);
       	/* Now RESET_BAR must be high */
       	if (mt9v128->pdata->reset) {
	       	mt9v128->pdata->reset(&mt9v128->subdev, 0);
	       	msleep(1);
       	}
#ifdef MT9V128_HEADBOARD
	ret = mt9v128_config_PCA9543A(client);
#endif
      	/* soft reset */
       	ret = mt9v128_reset(client);
       	if (ret<  0) {
	      	dev_err(&client->dev, "Failed to reset the camera\n");
	       	return ret;
       	}

 	return ret;
}

/**
 * mt9v128_video_probe - detects the sensor chip
 * @client: pointer to i2c client
 * 
 */
static int mt9v128_video_probe(struct i2c_client *client)
{
	u16 data;

	/* Read out the chip version register */
	data = reg_read(client, MT9V128_CHIP_VERSION);
	if (data != MT9V128_CHIP_VERSION_VALUE) {
		dev_err(&client->dev,
		"No MT9V128 chip detected, register read %x\n", data);
		return -ENODEV;
	}
	dev_info(&client->dev, "Detected MT9V128, chip ID = %x\n", data);
	return 0;
}

/**
 * mt9v128_power_off - power off the sensor
 * @mt9v128: pointer to private data structure
 * 
 */
static void mt9v128_power_off(struct mt9v128 *mt9v128)
{	  
	if (mt9v128->pdata->reset) {
               	mt9v128->pdata->reset(&mt9v128->subdev, 1);
               	msleep(1);
       	}
       	if (mt9v128->pdata->set_xclk)
               	mt9v128->pdata->set_xclk(&mt9v128->subdev, 0);
}

static int mt9v128_enum_mbus_code(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_mbus_code_enum *code)
{	  
      	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);
	if (code->pad || code->index)
               	return -EINVAL;

       	code->code = mt9v128->format.code;
       	return 0;
}

static struct v4l2_mbus_framefmt *mt9v128_get_pad_format(
       struct mt9v128 *mt9v128,
       struct v4l2_subdev_fh *fh,
       unsigned int pad, u32 which)
{	
	switch (which) {
		case V4L2_SUBDEV_FORMAT_TRY:
			return v4l2_subdev_get_try_format(fh, pad);
		case V4L2_SUBDEV_FORMAT_ACTIVE:
			return &mt9v128->format;
		default:
			return NULL;
	}
}

static int mt9v128_set_power(struct v4l2_subdev *sd, int on)
{	   

	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);
	int ret = 0;

	mutex_lock(&mt9v128->power_lock);
	/*
	* If the power count is modified from 0 to != 0 or from != 0 to 0,
	* update the power state.
	*/
	if (mt9v128->power_count == !on) {
		if (on) {
			ret = mt9v128_power_on(mt9v128);
			if (ret) {
			dev_err(mt9v128->subdev.v4l2_dev->dev,
			"Failed to power on: %d\n", ret);
			goto out;
			}
		} else {
			mt9v128_power_off(mt9v128);
		}
	}	
	/* Update the power count. */
	mt9v128->power_count += on ? 1 : -1;
	WARN_ON(mt9v128->power_count<  0);
out:
	mutex_unlock(&mt9v128->power_lock);
	return ret;
}

static int mt9v128_registered(struct v4l2_subdev *sd)
{	
       	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);
       	struct i2c_client *client = v4l2_get_subdevdata(&mt9v128->subdev);
       	int ret;
	
       	ret = mt9v128_set_power(&mt9v128->subdev, 1);
       	if (ret) {
               	dev_err(&client->dev,
                       	"Failed to power on device: %d\n", ret);
               	return ret;
       	}
       	ret = mt9v128_video_probe(client);
	mt9v128_set_power(&mt9v128->subdev, 0);
       	return ret;
}

static int mt9v128_setup_sensor_output(struct mt9v128 *mt9v128, u16 width, u16 height)
{
	int ret = 0, count;
	int i,j;
	u16 data;
	struct i2c_client *client = v4l2_get_subdevdata(&mt9v128->subdev);

#ifdef MT9V128_TEST_EN
	ret |= reg_write(client, MT9V128_TEST_MODE_REG, MT9V128_TEST_PATTERN); // test patterns
#endif
	/* Set Parallel Mode - CPIPE 8 bit with FVLV */ 
	ret |= reg_write(client, 0x098E, 0x7C00);
	ret |= reg_write(client, 0xFC00, 0x3000);
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8801);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR "Failed to write command 0x8801: ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}

	/* Write Config - ntsc_640x480_dewarp */
	ret |= reg_write(client, 0x098E, 0x7C00);
	for(j = 0; j < 34; j++){
		for (i = 0; i < 8; i++){
			ret |= reg_write(client, (0x0990 + i*2), ntsc_640x480_dewarp[j][i]);
		}
        	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8304);
        	data = reg_read(client, MT9V128_COMMAND_REGISTER);
		count = 0;
		while(data){
			data = reg_read(client, MT9V128_COMMAND_REGISTER);
			if(count++ > 5){
				printk(KERN_ERR "Failed to write command 0x8304: ERROR = 0x%x\n",data);
				break;
			}
			mdelay(1);
		}
	}

	for (i = 0; i < 3; i++){
		ret |= reg_write(client, (0x0990 + i*2), ntsc_640x480_dewarp[34][i]);
	}
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8304);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8304: ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}

	/* DW - Apply Config */
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8305);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8305: ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8301);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8301 (0x8305): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	/* Enable NTSC */
	ret |= reg_write(client, 0xFC00, 0x0100);
    	ret |= reg_write(client, 0xFC02, 0x0000);
    	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8300);
    	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8300: ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	mdelay(100); /* wait until complete */

	/* Get NTSC status */
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8301);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8301 (0x8300): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}

	/* CCM_AWB */
	for(i = 0; i < 22; i++){
		ret |= reg_write(client, (0xC8CE + i*2), CCM_AWB_1[i]);
	}
	for(i = 0; i < 12; i++){
		ret |= reg_write(client, (0xC910 + i*2), CCM_AWB_2[i]);
	}
    	for(i = 0; i < 6; i++){
		ret |= reg_write(client, (0xC962 + i*2), CCM_AWB_3[i]);
	}
	for(i = 0; i < 4; i++){
		ret |= reg_write(client, (0xAC40 + i*2), CCM_AWB_4[i]);
	}

	/* Sensor Setup */
	ret |= reg_write(client, MT9V128_RST_REG, 0x10D0); //Reset Bit-field 0x0004
        
	mdelay(100);
	/* Sensor setup - action */
	ret |= reg_write(client, 0x3ed8, 0x0999); /* VLN boosted, Anti-eclipse lowered */
	ret |= reg_write(client, 0x3e14, 0x6886);
	ret |= reg_write(client, 0x3e1a, 0x8507);
	ret |= reg_write(client, 0x3e1c, 0x8705);
	ret |= reg_write(client, 0x3e24, 0x9a10);
	ret |= reg_write(client, 0x3e26, 0x8f09);
	ret |= reg_write(client, 0x3e2a, 0x8060);
	ret |= reg_write(client, 0x3e2c, 0x6169);
	ret |= reg_write(client, 0x3ed0, 0x8f7f);
	ret |= reg_write(client, 0x3EDA, 0x68F6);
	ret |= reg_write(client, 0xC8A0, 0x05BD);
  
	mdelay(100);
    
	ret |= reg_write(client, MT9V128_RST_REG, 0x10D4); //Set Bit-field 0x0004

	mdelay(10);
        
	/* Tuning*/
	//AE_TRACK_MODE, 0x00D7
	ret |= reg_write(client,VAR(10, 0x0002), 0x00D7);
	//CAM1_AET_AE_VIRT_GAIN_TH_CG, 0x0100
	ret |= reg_write(client,VAR(18, 0x00AC), 0x0100);
	//CAM1_AET_AE_VIRT_GAIN_TH_DCG, 0x00A0
	ret |= reg_write(client,VAR(18, 0x00AE), 0x00A0);
	//AE_TRACK_TARGET, 0x0032
	ret |= reg_write(client,VAR(10, 0x0012), 0x0032);
	//AE_TRACK_GATE, 0x0004
	ret |= reg_write(client,VAR(10, 0x0014), 0x0004);
	//AE_TRACK_JUMP_DIVISOR, 0x0002
	ret |= reg_write(client,VAR(10, 0x001A), 0x0002);
	//CAM1_AET_SKIP_FRAMES, 0x0001
	ret |= reg_write(client,VAR(18, 0x009A), 0x0001);
	//YUV_YCBCR_CONTROL, 0x0007 	
	ret |= reg_write(client,0x337C, 0x0007);
	//CAM1_LL_START_BRIGHTNESS, 0x0190
	ret |= reg_write(client,VAR(18, 0x0130), 0x0190);
	//CAM1_LL_STOP_BRIGHTNESS, 0x0640
	ret |= reg_write(client,VAR(18, 0x0132), 0x0640);
	//CAM1_LL_START_SATURATION, 0x0080
	ret |= reg_write(client,VAR(18, 0x0134), 0x0080);
	//CAM1_LL_END_SATURATION, 0x0000
	ret |= reg_write(client,VAR(18, 0x0136), 0x0000);
	//CAM1_LL_START_GAMMA_BM, 0x0190
	ret |= reg_write(client,VAR(18, 0x0158), 0x0190);
	//CAM1_LL_STOP_GAMMA_BM, 0x0640
	ret |= reg_write(client,VAR(18, 0x015A), 0x0640);
	//CAM1_SENSOR_0_FINE_CORRECTION, 0x0031
	ret |= reg_write(client,VAR(18, 0x0016), 0x0031);
	//CAM1_LL_LL_START_1, 0x0007
	ret |= reg_write(client,VAR(18, 0x013E), 0x0007);
	//CAM1_LL_LL_START_2, 0x0002
	ret |= reg_write(client,VAR(18, 0x0140), 0x0002);
	//CAM1_LL_LL_STOP_0, 0x0008
	ret |= reg_write(client,VAR(18, 0x0142), 0x0008);
	//CAM1_LL_LL_STOP_1, 0x0002
	ret |= reg_write(client,VAR(18, 0x0144), 0x0002);
	//CAM1_LL_LL_STOP_2, 0x0020
	ret |= reg_write(client,VAR(18, 0x0146), 0x0020);
	//CAM1_LL_NR_STOP_0, 0x0040
	ret |= reg_write(client,VAR(18, 0x0150), 0x0040);
	//CAM1_LL_NR_STOP_1, 0x0040
	ret |= reg_write(client,VAR(18, 0x0152), 0x0040);
	//CAM1_LL_NR_STOP_2, 0x0040
	ret |= reg_write(client,VAR(18, 0x0154), 0x0040);
	//CAM1_LL_NR_STOP_3, 0x0040
	ret |= reg_write(client,VAR(18, 0x0156), 0x0040);
	//CAM1_AET_AE_MAX_VIRT_AGAIN, 0x1FFF
	ret |= reg_write(client,VAR(18, 0x00A8), 0x1FFF);
	//CAM1_AET_AE_MAX_VIRT_DGAIN, 0x0100
	ret |= reg_write(client,VAR(18, 0x00A4), 0x0100);
	//CAM1_MAX_ANALOG_GAIN, 0x0100
	ret |= reg_write(client,VAR(18, 0x007C), 0x0100);
	//SYS_REFRESH_MASK, 0x0003
	ret |= reg_write(client,VAR(23, 0x0028), 0x0003);
	//LL_GAMMA_NRCURVE_0, 0x0000
	ret |= reg_write(client,VAR(15, 0x0032), 0x0000);
	//LL_GAMMA_NRCURVE_1, 0x0018
	ret |= reg_write(client,VAR(15, 0x0034), 0x0018);
	//LL_GAMMA_NRCURVE_2, 0x0025
	ret |= reg_write(client,VAR(15, 0x0036), 0x0025);
	//LL_GAMMA_NRCURVE_3, 0x003A
	ret |= reg_write(client,VAR(15, 0x0038), 0x003A);
	//LL_GAMMA_NRCURVE_4, 0x0059
	ret |= reg_write(client,VAR(15, 0x003A), 0x0059);
	//LL_GAMMA_NRCURVE_5, 0x0070
	ret |= reg_write(client,VAR(15, 0x003C), 0x0070);
	//LL_GAMMA_NRCURVE_6, 0x0081
	ret |= reg_write(client,VAR(15, 0x003E), 0x0081);
	//LL_GAMMA_NRCURVE_7, 0x0090
	ret |= reg_write(client,VAR(15, 0x0040), 0x0090);
	//LL_GAMMA_NRCURVE_8, 0x009E
	ret |= reg_write(client,VAR(15, 0x0042), 0x009E);
	//LL_GAMMA_NRCURVE_9, 0x00AB
	ret |= reg_write(client,VAR(15, 0x0044), 0x00AB);
	//LL_GAMMA_NRCURVE_10, 0x00B6
	ret |= reg_write(client,VAR(15, 0x0046), 0x00B6);
	//LL_GAMMA_NRCURVE_11, 0x00C1
	ret |= reg_write(client,VAR(15, 0x0048), 0x00C1);
	//LL_GAMMA_NRCURVE_12, 0x00CB
	ret |= reg_write(client,VAR(15, 0x004A), 0x00CB);
	//LL_GAMMA_NRCURVE_13, 0x00D5
	ret |= reg_write(client,VAR(15, 0x004C), 0x00D5);
	//LL_GAMMA_NRCURVE_14, 0x00DE
	ret |= reg_write(client,VAR(15, 0x004E), 0x00DE);
	//LL_GAMMA_NRCURVE_15, 0x00E7
	ret |= reg_write(client,VAR(15, 0x0050), 0x00E7);
	//LL_GAMMA_NRCURVE_16, 0x00EF
	ret |= reg_write(client,VAR(15, 0x0052), 0x00EF);
	//LL_GAMMA_NRCURVE_17, 0x00F7
	ret |= reg_write(client,VAR(15, 0x0054), 0x00F7);
	//LL_GAMMA_NRCURVE_18, 0x00FF
	ret |= reg_write(client,VAR(15, 0x0056), 0x00FF);

	/* Apply Patches */
	
	/*Load Patch 0211 */
	ret |= reg_write(client,0x0982, 0x0001); //ACCESS_CTL_STAT, 0x0001
	
	/* PHYSICAL_ADDRESS_ACCESS, 0x69AC to 0x6C5C */
	for(j = 0; j < 43; j++){
		ret |= reg_write(client,0x098A, (0x69AC + (j*0x10)));
		for (i = 0; i < 8; i++){
			ret |= reg_write(client, (0x0990 + i*2), patch_0211[j][i]);
		}
	}
	ret |= reg_write(client,0x098A, 0x6C5C);
	for (i = 0; i < 4; i++){
		ret |= reg_write(client, (0x0990 + i*2), patch_0211[43][i]);
	}

	/* Apply Patch 0211 */
	ret |= reg_write(client,0x098E, 0x7C57);
	ret |= reg_write(client,0x098E, 0x7C00);
	ret |= reg_write(client,0x0990, 0x0BD8);
	ret |= reg_write(client,0x0992, 0x0211);
	ret |= reg_write(client,0x0994, 0x0103);
	ret |= reg_write(client,0x0996, 0x0611);
	ret |= reg_write(client,0x0998, 0x02B8);
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8702);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8702 (patch_0211): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8701);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8701 (patch_0211): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8703);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8703 (patch_0211): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	data = reg_read(client, 0x0990);
//	printk(KERN_ERR"number of patches applied = 0x%x\n",data >> 8);

	/*Load Patch 0611 */
	ret |= reg_write(client,0x0982, 0x0001); //ACCESS_CTL_STAT, 0x0001
	
	/* PHYSICAL_ADDRESS_ACCESS, 0x7678 to 0x76D8 */
	for(j = 0; j < 6; j++){
		ret |= reg_write(client,0x098A, (0x7678 +(j*0x10)));
		for (i = 0; i < 8; i++){
			ret |= reg_write(client, (0x0990 + i*2), patch_0611[j][i]);
		}
	}
	ret |= reg_write(client,0x098A, 0x76D8);
	for (i = 0; i < 2; i++){
		ret |= reg_write(client, (0x0990 + i*2), patch_0611[6][i]);
	}

	/* Apply Patch 0611 */
	ret |= reg_write(client,0x098E, 0x7C57);
	ret |= reg_write(client,0x098E, 0x7C00);
	ret |= reg_write(client,0x0990, 0x16a8);
	ret |= reg_write(client,0x0992, 0x0611);
	ret |= reg_write(client,0x0994, 0x0103);
	ret |= reg_write(client,0x0996, 0x0611);
	ret |= reg_write(client,0x0998, 0x0064);
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8702);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8702 (patch_0611): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8701);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8701 (patch_0611): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8703);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8703 (patch_0611): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	data = reg_read(client, 0x0990);
	//printk(KERN_ERR"number of patches applied = 0x%x\n",data >> 8);

	/*Load Patch 0711 */
	ret |= reg_write(client,0x0982, 0x0001); //ACCESS_CTL_STAT, 0x0001
	
	/* PHYSICAL_ADDRESS_ACCESS, 0x76DC to 0x773C */
	for(j = 0; j < 7; j++){
		ret |= reg_write(client,0x098A, (0x76DC +(j*0x10)));
		for (i = 0; i < 8; i++){
			ret |= reg_write(client, (0x0990 + i*2), patch_0711[j][i]);
		}
	}

	/* Apply Patch 0711 */
	ret |= reg_write(client,0x098E, 0x7C57);
	ret |= reg_write(client,0x098E, 0x7C00);
	ret |= reg_write(client,0x0990, 0x1728);
	ret |= reg_write(client,0x0992, 0x0711);
	ret |= reg_write(client,0x0994, 0x0103);
	ret |= reg_write(client,0x0996, 0x0611);
	ret |= reg_write(client,0x0998, 0x0070);
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8702);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8702 (patch_0711): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
		
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8701);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8701 (patch_0711): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8703);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8703 (patch_0711): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	data = reg_read(client, 0x0990);
	//printk(KERN_ERR"number of patches applied = 0x%x\n",data >> 8);

	/*Load Patch 0911 */
	ret |= reg_write(client,0x0982, 0x0001); //ACCESS_CTL_STAT, 0x0001
	
	/* PHYSICAL_ADDRESS_ACCESS, 0x77a8 to 0x7828 */
	for(j = 0; j < 8; j++){
		ret |= reg_write(client,0x098A, (0x77a8 +(j*0x10)));
		for (i = 0; i < 8; i++){
			ret |= reg_write(client, (0x0990 + i*2), patch_0911[j][i]);
		}
	}
	ret |= reg_write(client,0x098A, 0x7828);
	for (i = 0; i < 2; i++){
		ret |= reg_write(client, (0x0990 + i*2), patch_0911[8][i]);
	}

	/* Apply Patch 0911 */
	ret |= reg_write(client,0x098E, 0x7C00);
	ret |= reg_write(client,0x098E, 0x7C00);
	ret |= reg_write(client,0x0990, 0x17e4);
	ret |= reg_write(client,0x0992, 0x0911);
	ret |= reg_write(client,0x0994, 0x0103);
	ret |= reg_write(client,0x0996, 0x0611);
	ret |= reg_write(client,0x0998, 0x0084);
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8702);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8702 (patch_0911): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8701);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8701 (patch_0911): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8703);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8703 (patch_0911): ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	data = reg_read(client, 0x0990);
	//printk(KERN_ERR"number of patches applied = 0x%x\n",data >> 8);

	ret |= reg_write(client, 0x0016, 0x585F); //CLOCKS_CONTROL, 0x587F
	ret |= reg_write(client, 0x0030, 0x0200); //PAD_SLEW, 0x0207
	ret |= reg_write(client, 0x0030, 0x0207); //PAD_SLEW, 0x0207
	ret |= reg_write(client, 0x337C, 0x000F); //YUV_YCBCR_CONTROL, 0x000F
	ret |= reg_write(client, 0x337E, 0x1000); //Y_RGB_OFFSET, 0x1000
	ret |= reg_write(client, 0xC8BE, 0x0002); //CAM1_AET_EXT_GAIN_SETUP_0, 0x0002
	ret |= reg_write(client, 0xFC00, 0x0000); //CMD_HANDLER_PARAMS_POOL_0, 0x0000
	ret |= reg_write(client, 0xFC02, 0x0000); //CMD_HANDLER_PARAMS_POOL_1, 0x0000
	ret |= reg_write(client, 0xFC04, 0x0103); //CMD_HANDLER_PARAMS_POOL_2, 0x0103
	ret |= reg_write(client, MT9V128_COMMAND_REGISTER, 0x8800);
	data = reg_read(client, MT9V128_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = reg_read(client, MT9V128_COMMAND_REGISTER);
		if(count++ > 5){
			printk(KERN_ERR"Failed to write command 0x8800: ERROR = 0x%x\n",data);
			break;
		}
		mdelay(1);
	}
	mdelay(10);
	return ret;
}

static int mt9v128_s_stream(struct v4l2_subdev *sd, int enable)
{	   
	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);
	struct v4l2_rect *rect = &mt9v128->rect;
	struct i2c_client *client = v4l2_get_subdevdata(&mt9v128->subdev);
	int ret = 0;

	if (!enable) {
		return 0;
	} 

	return mt9v128_setup_sensor_output(mt9v128, rect->width, rect->height);
}

static int mt9v128_get_format(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_format *fmt)
{
	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);

	fmt->format = *mt9v128_get_pad_format(mt9v128, fh, fmt->pad, fmt->which);
       	return 0;
}

static int mt9v128_set_format(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_format *format)
{	   
        struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);
	struct mt9v128_frame_size size;

	size.height = format->format.height;
	size.width  = format->format.width;	
	mt9v128_v4l2_try_fmt_cap(&size);
	mt9v128->rect.width        = size.width;
	mt9v128->rect.height       = size.height;
	mt9v128->curr_rect.width   = size.width;
	mt9v128->curr_rect.height  = size.height;
	mt9v128->format.width      = size.width;
	mt9v128->format.height     = size.height;
	mt9v128->format.code	   = V4L2_MBUS_FMT_UYVY8_1X16;
	return 0;
}

static int mt9v128_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
       	struct mt9v128 *mt9v128;
       	mt9v128 = container_of(sd, struct mt9v128, subdev);
	
	mt9v128->rect.width     = MT9V128_WINDOW_WIDTH_DEF;
	mt9v128->rect.height    = MT9V128_WINDOW_HEIGHT_DEF;
	mt9v128->rect.left      = MT9V128_COLUMN_START_DEF;
	mt9v128->rect.top       = MT9V128_ROW_START_DEF;
	
       	if(mt9v128->pdata->version == MT9V128_MONOCHROME_VERSION)
			mt9v128->format.code = V4L2_MBUS_FMT_Y12_1X12;
	else {
		mt9v128->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
	}	

	mt9v128->format.width  	   = MT9V128_WINDOW_WIDTH_DEF;
	mt9v128->format.height     = MT9V128_WINDOW_HEIGHT_DEF;
	mt9v128->format.field      = V4L2_FIELD_NONE;
	mt9v128->format.colorspace = V4L2_COLORSPACE_SRGB;
       	return mt9v128_set_power(sd, 1);
}

static int mt9v128_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{	  
       	return mt9v128_set_power(sd, 0);
}

static struct v4l2_subdev_core_ops mt9v128_subdev_core_ops = {
	.s_power = mt9v128_set_power,
};

static struct v4l2_subdev_video_ops mt9v128_subdev_video_ops = {
	.s_stream = mt9v128_s_stream,
};

static struct v4l2_subdev_pad_ops mt9v128_subdev_pad_ops = {
       	.enum_mbus_code = mt9v128_enum_mbus_code,
       	.get_fmt = mt9v128_get_format,
       	.set_fmt = mt9v128_set_format,
      	// .get_crop = mt9v128_get_crop,
      	// .set_crop = mt9v128_set_crop,
};

static struct v4l2_subdev_ops mt9v128_subdev_ops = {
	.core  = &mt9v128_subdev_core_ops,
	.video = &mt9v128_subdev_video_ops,
	.pad   = &mt9v128_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops mt9v128_subdev_internal_ops = {
	.registered = mt9v128_registered,
	.open = mt9v128_open,
	.close = mt9v128_close,
};

static int mt9v128_probe(struct i2c_client *client,
                               const struct i2c_device_id *did)
{	 
	int ret;
	struct mt9v128 *mt9v128;
	struct mt9v128_platform_data *pdata = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
		"I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
	return -EIO;
	}

       	mt9v128 = kzalloc(sizeof(struct mt9v128), GFP_KERNEL);
       	if (!mt9v128)
               	return -ENOMEM;

	mt9v128->pdata = pdata;

       	v4l2_ctrl_handler_init(&mt9v128->ctrls, 4);

       	mt9v128->subdev.ctrl_handler = &mt9v128->ctrls;
       	if (mt9v128->ctrls.error)
		printk(KERN_INFO "%s: control initialization error %d\n",
			__func__, mt9v128->ctrls.error);

	mutex_init(&mt9v128->power_lock);
	v4l2_i2c_subdev_init(&mt9v128->subdev, client, &mt9v128_subdev_ops);
	mt9v128->subdev.internal_ops = &mt9v128_subdev_internal_ops;

       	mt9v128->pad.flags = MEDIA_PAD_FL_SOURCE;
       	ret = media_entity_init(&mt9v128->subdev.entity, 1, &mt9v128->pad, 0);
       	if (ret)
               	return ret;

       	mt9v128->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	if (mt9v128->pdata->version == MT9V128_MONOCHROME_VERSION)
		mt9v128->format.code = V4L2_MBUS_FMT_Y12_1X12;
	else {
	//	mt9v128->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		mt9v128->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
	}	

       	return 0;
}

static int mt9v128_remove(struct i2c_client *client)
{	 
       	struct v4l2_subdev *sd = i2c_get_clientdata(client);
       	struct mt9v128 *mt9v128 = container_of(sd, struct mt9v128, subdev);

	v4l2_device_unregister_subdev(sd);
       	media_entity_cleanup(&sd->entity);
       	kfree(mt9v128);
       	return 0;
}

static const struct i2c_device_id mt9v128_id[] = {
	{ "mt9v128", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9v128_id);

static struct i2c_driver mt9v128_i2c_driver = {
	.driver = {
		.name = "mt9v128",
	},
	.probe    = mt9v128_probe,
	.remove   = mt9v128_remove,
	.id_table = mt9v128_id,
};

/************************************************************************
			module function
************************************************************************/
static int __init mt9v128_module_init(void)
{
	return i2c_add_driver(&mt9v128_i2c_driver);
}

static void __exit mt9v128_module_exit(void)
{
	i2c_del_driver(&mt9v128_i2c_driver);
}

module_init(mt9v128_module_init);
module_exit(mt9v128_module_exit);

MODULE_DESCRIPTION("mt9v128 SoC sensor(VGA) driver");
MODULE_AUTHOR("Aptina");
MODULE_LICENSE("GPL v2");
