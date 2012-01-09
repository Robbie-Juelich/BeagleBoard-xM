/*
 * drivers/media/video/mt9d131.c
 *
 * Aptina MT9D131/A2010 soc sensor driver
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

#include<media/mt9d131.h>
#include<media/v4l2-chip-ident.h>
#include<media/v4l2-ctrls.h>
#include<media/v4l2-device.h>
#include<media/v4l2-subdev.h>

#define MT9D131_RST	0x0D
#define MT9D131_RST_ENABLE	0x0021
#define MT9D131_RST_DISABLE	0x0000
#define MT9D131_CHIP_VERSION	0x00
#define MT9D131_CHIP_VERSION_VALUE	0x1519
#define MT9D131_PLL_CONTROL	0x65
#define MT9D131_PLL_CONFIG_1	0x66
#define MT9D131_PLL_CONFIG_2	0x67
#define MT9D131_PLL_CONTROL_PWRON	0xA000
#define MT9D131_PLL_CONTROL_USEPLL	0x2000
#define MT9D131_PLL_CONTROL_PWROFF	0xC000

#define MT9D131_WINDOW_WIDTH_DEF	800
#define MT9D131_WINDOW_HEIGHT_DEF	600
#define MT9D131_COLUMN_START_DEF	0
#define MT9D131_ROW_START_DEF		0
#define MT9D131_MAX_WIDTH		1600
#define MT9D131_MAX_HEIGHT		1200

/* Zoom Capbilities */
#define MT9D131_MIN_ABSZOOM		     0
#define MT9D131_MAX_ABSZOOM		     16
#define MT9D131_DEF_ABSZOOM		     0
#define MT9D131_ABSZOOM_STEP		     2

//#define MT9D131_DEBUG
#undef MT9D131_DEBUG
//#define MT9D131_HEADBOARD
#undef MT9D131_HEADBOARD
#ifdef MT9D131_DEBUG
#define DPRINTK_DRIVER(format, ...)				\
	printk(KERN_INFO "_MT9D131_DRIVER: \n" format, ## __VA_ARGS__)
#else
#define DPRINTK_DRIVER(format, ...)
#endif

struct mt9d131_pll_divs {
	u32 ext_freq;
       	u32 target_freq;
       	u8 m;
       	u8 n;
       	u8 p1;
};

struct mt9d131_frame_size {
	u16 width;
	u16 height;
};

struct mt9d131 {
       	struct v4l2_subdev subdev;
       	struct media_pad pad;
       	struct v4l2_rect rect;  /* Sensor window */
	struct v4l2_rect curr_rect;
       	struct v4l2_mbus_framefmt format;
       	struct v4l2_ctrl_handler ctrls;
       	struct mt9d131_platform_data *pdata;
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
const static struct mt9d131_frame_size   mt9d131_supported_framesizes[]={
	{  80,  60 },
	{  160, 120 },
	{  176, 144 },
	{  320, 240 },
	{  352, 288 },
	{  400, 300 },
	{  640, 480 },
	{  800, 600 },
	{ 1280, 720 },
	{ 1280, 960 },
	{ 1280, 1024},
	{ 1600, 1200 },
};

#ifdef MT9D131_HEADBOARD
/**
 * mt9d131_config_PCA9543A - configure on-board I2c level-shifter PCA9543A of MT9D131 Headboards from Aptina
 * @client: pointer to i2c client
 * Configures the level shifter to enable channel 0 
 */
static int 
mt9d131_config_PCA9543A(const struct i2c_client *client)
{
	struct	i2c_msg msg;
	int		ret;
	u8		buf= 0x21;
	
	msg.addr  = (0xE6 >> 1);	//slave address of PCA9543A
	msg.flags = 0;
	msg.len   = 1;
	msg.buf   = &buf;
	
	ret = i2c_transfer(client->adapter, &msg, 1);

	return 0;
		
}
#endif //MT9D131_HEADBOARD
/**
 * to_mt9d131 - A helper function which returns pointer to the private data structure
 * @client: pointer to i2c client
 * 
 */
static struct mt9d131 *to_mt9d131(const struct i2c_client *client)
{
       	return container_of(i2c_get_clientdata(client), struct mt9d131, subdev);
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
 * mt9d131_calc_size - Find the best match for a requested image capture size
 * @width: requested image width in pixels
 * @height: requested image height in pixels
 *
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static int mt9d131_calc_size(unsigned int request_width,
		unsigned int request_height) {
	int i = 0;
	unsigned long requested_pixels = request_width * request_height;
	for (i = 0; i < ARRAY_SIZE(mt9d131_supported_framesizes); i++) {
		if (mt9d131_supported_framesizes[i].height
				* mt9d131_supported_framesizes[i].width >= requested_pixels)
			return i;
	}
	//couldn't find a match, return the max size as a default
	return (ARRAY_SIZE(mt9d131_supported_framesizes) - 1);
}
/**
 * mt9d131_v4l2_try_fmt_cap - Find the best match for a requested image capture size
 * @mt9d131_frame_size: a ointer to the structure which specifies requested image size
 * 
 * Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static int mt9d131_v4l2_try_fmt_cap(struct mt9d131_frame_size *requstedsize) {
	int isize,ret = 0;
		
	isize = mt9d131_calc_size(requstedsize->width,requstedsize->height);
	
	requstedsize->width = mt9d131_supported_framesizes[isize].width;
	requstedsize->height = mt9d131_supported_framesizes[isize].height;
	
	return 0;
}
/**
 * mt9d131_reset - Soft resets the sensor
 * @client: pointer to i2c client
 * 
 */
static int mt9d131_reset(struct i2c_client *client)
{
       	struct mt9d131 *mt9d131 = to_mt9d131(client);
       	int ret;

       	/* Disable chip output, synchronous option update */
       	ret = reg_write(client, MT9D131_RST, MT9D131_RST_ENABLE);
       	if (ret<  0)
               	return ret;
       	ret = reg_write(client, MT9D131_RST, MT9D131_RST_DISABLE);
       	if (ret<  0)
	       	return ret;
	return 0;
}
/**
 * mt9d131_power_on - power on the sensor
 * @mt9d131: pointer to private data structure
 * 
 */
static int mt9d131_power_on(struct mt9d131 *mt9d131)
{
       	struct i2c_client *client = v4l2_get_subdevdata(&mt9d131->subdev);
       	int ret;
	   	DPRINTK_DRIVER("power on HERE \n");
      	 /* Ensure RESET_BAR is low */
       	if (mt9d131->pdata->reset) {
	       	mt9d131->pdata->reset(&mt9d131->subdev, 1);
	       	msleep(1);
       	}
       	/* Emable clock */
       	if (mt9d131->pdata->set_xclk)
	       	mt9d131->pdata->set_xclk(&mt9d131->subdev,
	                        	mt9d131->pdata->ext_freq);
       	/* Now RESET_BAR must be high */
       	if (mt9d131->pdata->reset) {
	       	mt9d131->pdata->reset(&mt9d131->subdev, 0);
	       	msleep(1);
       	}
#ifdef MT9D131_HEADBOARD
	ret = mt9d131_config_PCA9543A(client);
#endif
      	/* soft reset */
       	ret = mt9d131_reset(client);
       	if (ret<  0) {
	      	dev_err(&client->dev, "Failed to reset the camera\n");
	       	return ret;
       	}

 	return ret;
}
/**
 * mt9d131_video_probe - detects the sensor chip
 * @client: pointer to i2c client
 * 
 */
static int mt9d131_video_probe(struct i2c_client *client)
{	   
		
       	s32 data;
     	/* Read out the chip version register */
       	data = reg_read(client, MT9D131_CHIP_VERSION);
       	if (data != MT9D131_CHIP_VERSION_VALUE) {
               	dev_err(&client->dev,
                       	"No MT9D131 chip detected, register read %x\n", data);
      	 	return -ENODEV;
 	}
       	dev_info(&client->dev, "Detected a MT9D131 chip ID %x\n", data);
       	return 0;
}
/**
 * mt9d131_power_on - power off the sensor
 * @mt9d131: pointer to private data structure
 * 
 */
static void mt9d131_power_off(struct mt9d131 *mt9d131)
{	  
	if (mt9d131->pdata->reset) {
               	mt9d131->pdata->reset(&mt9d131->subdev, 1);
               	msleep(1);
       	}
       	if (mt9d131->pdata->set_xclk)
               	mt9d131->pdata->set_xclk(&mt9d131->subdev, 0);
}

static int mt9d131_enum_mbus_code(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_mbus_code_enum *code)
{	  
      	struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
	if (code->pad || code->index)
               	return -EINVAL;

       	code->code = mt9d131->format.code;
       	return 0;
}

static struct v4l2_mbus_framefmt *mt9d131_get_pad_format(
       struct mt9d131 *mt9d131,
       struct v4l2_subdev_fh *fh,
       unsigned int pad, u32 which)
{	
       	switch (which) {
       	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
       	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return&mt9d131->format;
       	default:
		return NULL;
       }
}

static int mt9d131_set_power(struct v4l2_subdev *sd, int on)
{	   
		
       	struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
       	int ret = 0;
	mutex_lock(&mt9d131->power_lock);
       /*
        * If the power count is modified from 0 to != 0 or from != 0 to 0,
        * update the power state.
        */
       	if (mt9d131->power_count == !on) {
               	if (on) {
                       ret = mt9d131_power_on(mt9d131);
                       if (ret) {
                               dev_err(mt9d131->subdev.v4l2_dev->dev,
                                       "Failed to power on: %d\n", ret);
                               goto out;
                       }
               } else {
                       mt9d131_power_off(mt9d131);
               }
       }
       /* Update the power count. */
       mt9d131->power_count += on ? 1 : -1;
       WARN_ON(mt9d131->power_count<  0);

out:
       mutex_unlock(&mt9d131->power_lock);
       return ret;
}

static int mt9d131_registered(struct v4l2_subdev *sd)
{	
       	struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
       	struct i2c_client *client = v4l2_get_subdevdata(&mt9d131->subdev);
       	int ret;
	
       	ret = mt9d131_set_power(&mt9d131->subdev, 1);
       	if (ret) {
               	dev_err(&client->dev,
                       	"Failed to power on device: %d\n", ret);
               	return ret;
       	}
       	ret = mt9d131_video_probe(client);
	mt9d131_set_power(&mt9d131->subdev, 0);
       	return ret;
}

static inline int mt9d131_pll_disable(struct i2c_client *client)
{	
       return reg_write(client, MT9D131_PLL_CONTROL,
                        MT9D131_PLL_CONTROL_PWROFF);
}

static int mt9d131_setup_sensor_output(struct i2c_client *client, u16 width, u16 height)
{
	int	ret=0;
	u16	reg_val;

	ret  = reg_write(client, 0x00F0, 0x0001); ///change to page 1
	ret |= reg_write(client, 0x00C6, 0x2703);
	ret |= reg_write(client, 0x00C8, width);
	ret |= reg_write(client, 0x00C6, 0x2705);
	ret |= reg_write(client, 0x00C8, height);
	if(ret) printk("mt9d131_setup_sensor_output() failed at width and height setting: ret=%d\n",ret);

	if((width > MT9D131_WINDOW_WIDTH_DEF) && (height > MT9D131_WINDOW_HEIGHT_DEF)){ //disable binning
		DPRINTK_DRIVER("disable binning for %dx%d>%dx%d\n",
												width, height,MT9D131_WINDOW_WIDTH_DEF,MT9D131_WINDOW_HEIGHT_DEF);
		ret |= reg_write(client, 0x00F0, 0x0000); ///change to page 0
		ret |= reg_write(client, 0x0021, 0x0400); //disable binning

		ret |= reg_write(client, 0x00F0, 0x0001); ///change to page 1

		ret |= reg_write(client, 0x00C6, 0x2729); //change the cropping to full size,i.e., 1600
		ret |= reg_write(client, 0x00C8, width);

		ret |= reg_write(client, 0x00C6, 0x272D); ///change the cropping to full size,i.e., 1200
		ret |= reg_write(client, 0x00C8, height);
		if(ret) printk("mt9d131_setup_sensor_output() failed at disabling binning setting: ret=%d\n",ret);
	}else{ //enable binning
		DPRINTK_DRIVER("enable binning for %dx%d<= %dx%d\n",
												width, height,MT9D131_WINDOW_WIDTH_DEF,MT9D131_WINDOW_HEIGHT_DEF);
		ret |= reg_write(client, 0x00F0, 0x0000); ///change to page 0
		ret |= reg_write(client, 0x0021, 0x8400); //enable binning

		ret |= reg_write(client, 0x00F0, 0x0001); ///change to page 1
		ret |= reg_write(client, 0x00C6, 0x2729); //change the cropping to default size,i.e., 800
		ret |= reg_write(client, 0x00C8, width);

		ret |= reg_write(client, 0x00C6, 0x272D); ///change the cropping to default size,i.e., 600
		ret |= reg_write(client, 0x00C8, height);
		if(ret) printk("mt9d131_setup_sensor_output() failed at enabling binning setting: ret=%d\n",ret);
	}
	mdelay(200); //addd some delay to let it setttle down
	ret |= reg_write(client, 0x00F0, 0x0001); ///change to page 1
	ret |= reg_write(client, 0x00C6, 0xA103); //refresh
	ret |= reg_write(client, 0x00C8, 0x0005);

	DPRINTK_DRIVER("finished mt9d131_setup_sensor_output():ret=%d\n",ret);

	return (ret>= 0? 0:-EIO);
}

static int mt9d131_set_params(struct i2c_client *client,
                               struct v4l2_rect *rect)
{	  
       	struct mt9d131 *mt9d131 = to_mt9d131(client);
       	int ret;
       	u16 xbin, ybin;
       	__s32 left;
	mt9d131_setup_sensor_output(client, rect->width, rect->height);
	return ret;
}

static int mt9d131_s_stream(struct v4l2_subdev *sd, int enable)
{	   
       struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
       struct i2c_client *client = v4l2_get_subdevdata(&mt9d131->subdev);
       struct v4l2_rect rect = mt9d131->rect;
      
	u16 poll=0;
       	int ret;
	//We enable the pll in Stream on only..
	// While going to stream off we will disable pll..
	if (enable) {
       		ret  = reg_write(client, 0x00F0, 0x00);
		ret |= reg_write(client, 0x66, 0x500B);
		ret |= reg_write(client, 0x67, 0x200);
		ret |= reg_write(client, 0x65, 0xA000);
		ret |= reg_write(client, 0x65, 0x2000);
		mdelay(20);		

		ret |= reg_write(client, 0x00F0, 0x1);
		ret |=  reg_write(client, 0xC6, 0xA77D);
		ret |=  reg_write(client, 0xC8, 0x0000);
		//ret |=  reg_write(client, 0x03, 0x0001);
		//ret |=  reg_write(client, 0x09, 0x0000);
		ret |= reg_write(client, 0x00C6, 0xA103);
	   	ret |= reg_write(client, 0x00C8, 0x0005);
		
		ret = mt9d131_set_params(client,&rect);
		if (ret<  0)
                       return ret;
            
       } else {
                 ret = mt9d131_pll_disable(client);
       }
       return ret;
}

static int mt9d131_get_format(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_format *fmt)
{	   
		
       	struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
	fmt->format =
               	*mt9d131_get_pad_format(mt9d131, fh, fmt->pad, fmt->which);
       	return 0;
}

static int mt9d131_set_format(struct v4l2_subdev *sd,
                               struct v4l2_subdev_fh *fh,
                               struct v4l2_subdev_format *format)
{	   
	int ret=0;
        struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
        struct i2c_client *client = v4l2_get_subdevdata(&mt9d131->subdev);
	struct mt9d131_frame_size size;
	size.height = format->format.height;
	size.width = format->format.width;	
	mt9d131_v4l2_try_fmt_cap(&size);
	mt9d131->rect.width     = size.width;
	mt9d131->rect.height    = size.height;
	mt9d131->curr_rect.width = size.width;
	mt9d131->curr_rect.height  = size.height;
	mt9d131->format.width      = size.width;
	mt9d131->format.height    = size.height;
	mt9d131->format.code = V4L2_MBUS_FMT_UYVY8_1X16;	
	return 0;
}

static int mt9d131_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{	   
		
       	struct mt9d131 *mt9d131;
       	mt9d131 = container_of(sd, struct mt9d131, subdev);
	
	mt9d131->rect.width     = MT9D131_WINDOW_WIDTH_DEF;
	mt9d131->rect.height    = MT9D131_WINDOW_HEIGHT_DEF;
	mt9d131->rect.left      = MT9D131_COLUMN_START_DEF;
        mt9d131->rect.top       = MT9D131_ROW_START_DEF;
	
       	if (mt9d131->pdata->version == MT9D131_MONOCHROME_VERSION)
               mt9d131->format.code = V4L2_MBUS_FMT_Y12_1X12;
	else
		mt9d131->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
		//mt9d131->format.code = V4L2_MBUS_FMT_SRGGB10_1X10;
     		//mt9d131->format.code = V4L2_MBUS_FMT_SGRBG12_1X12;
	mt9d131->format.width = MT9D131_WINDOW_WIDTH_DEF;
	mt9d131->format.height = MT9D131_WINDOW_HEIGHT_DEF;
	mt9d131->format.field = V4L2_FIELD_NONE;
       	mt9d131->format.colorspace = V4L2_COLORSPACE_SRGB;
       	return mt9d131_set_power(sd, 1);
}

static int mt9d131_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{	  
       	return mt9d131_set_power(sd, 0);
}

/**
 * mt9d131_set_zoom - Zooms the image
 * @client: pointer to the i2c client driver structure
 * @zoomfactor: zooming value
 * @lvc: pointer to vcontrol structure
 * Returns a negative error number if no device is detected
 */
static int mt9d131_set_zoom(u32 zoomfactor, struct i2c_client  *client) {
	int ret, width, height, i;
	struct mt9d131 *mt9d131 = to_mt9d131(client);
	struct v4l2_crop crop;
	struct v4l2_rect rect = mt9d131->curr_rect;
	
	crop.c.top = 0;
	crop.c.left = 0;
	crop.c.height = rect.height;
	crop.c.width = rect.width;
	
	switch (zoomfactor) {
	case 0:
		width = crop.c.width;
		height = crop.c.height;
		break;
	case 2:
		width = crop.c.width * 2;
		height = crop.c.height * 2;
		break;
	case 4:
		width = crop.c.width * 4;
		height = crop.c.height * 4;
		break;
	case 6:
		width = crop.c.width * 6;
		height = crop.c.height * 6;
		break;
	default:
		dev_err(&client->dev, "Absolute Zoom value out of range, Supported values are 0,2,4,6");
		return -EINVAL;
	}
	if(height > 1200 || width > 1600)
		{
			height = 1200;
			width = 1600;
		}
	ret = mt9d131_setup_sensor_output(client, width, height);
	return ret;

}

static int mt9d131_s_ctrl(struct v4l2_ctrl *ctrl)
{	   
       	struct mt9d131 *mt9d131 = container_of(ctrl->handler, struct mt9d131, ctrls);
       	struct i2c_client *client = v4l2_get_subdevdata(&mt9d131->subdev);
       	u16 data = 0;
       	int ret;
	switch (ctrl->id) {
       	case V4L2_CID_VFLIP:
		if (ctrl->val) {
			ret  = reg_write(client, 0x00F0, 0x0);
                       	data = reg_read(client, 0x20);
                        ret |= reg_write(client, 0x20, data | 0x0001);
		 	data = reg_read(client, 0x21);
                        ret |= reg_write(client, 0x21, data | 0x0001);
			ret |= reg_write(client, 0x00F0, 0x1);
			ret |= reg_write(client, 0x00C6, 0xA103);
	        	return reg_write(client, 0x00C8, 0x0005);
		} else {
                        ret  = reg_write(client, 0x00F0, 0x0);
                       	data = reg_read(client, 0x20);
                        ret |= reg_write(client, 0x20, data & 0xFFFE);
		 	data = reg_read(client, 0x21);
			ret |=reg_write(client, 0x21, data & 0xFFFE);
                        ret |= reg_write(client, 0x00F0, 0x1);
			ret |= reg_write(client, 0x00C6, 0xA103);
	        	return reg_write(client, 0x00C8, 0x0005);
		}
		
	case V4L2_CID_HFLIP:
		if (ctrl->val) {
			ret  = reg_write(client, 0x00F0, 0x00);
                       	data = reg_read(client, 0x20);
                        ret |= reg_write(client, 0x20, data | 0x0002);
		 	data = reg_read(client, 0x21);
                        ret |= reg_write(client, 0x21, data | 0x0002);
			ret |= reg_write(client, 0x00F0, 0x1);
			ret |= reg_write(client, 0x00C6, 0xA103);
	        	return reg_write(client, 0x00C8, 0x0005);
		} else {
                        ret  = reg_write(client, 0x00F0, 0x00);
                       	data = reg_read(client, 0x20);
                        ret |= reg_write(client, 0x20, data & 0xFFFD);
		 	data = reg_read(client, 0x21);
                        ret |= reg_write(client, 0x21, data & 0xFFFD);
			ret |= reg_write(client, 0x00F0, 0x1);
			ret |= reg_write(client, 0x00C6, 0xA103);
	        	return reg_write(client, 0x00C8, 0x0005);
		}
		
       case  V4L2_CID_ZOOM_ABSOLUTE:
		 return mt9d131_set_zoom(ctrl->val,client);
		
            	
       case V4L2_CID_PAN_ABSOLUTE:
		break;

       }

       return 0;
}

static struct v4l2_ctrl_ops mt9d131_ctrl_ops = {
       	.s_ctrl = mt9d131_s_ctrl,
};

static struct v4l2_subdev_core_ops mt9d131_subdev_core_ops = {
       	.s_power        = mt9d131_set_power,
};

static struct v4l2_subdev_video_ops mt9d131_subdev_video_ops = {
       	.s_stream       = mt9d131_s_stream,
};

static struct v4l2_subdev_pad_ops mt9d131_subdev_pad_ops = {
       	.enum_mbus_code = mt9d131_enum_mbus_code,
       	.get_fmt = mt9d131_get_format,
       	.set_fmt = mt9d131_set_format,
      	// .get_crop = mt9d131_get_crop,
      	// .set_crop = mt9d131_set_crop,
};

static struct v4l2_subdev_ops mt9d131_subdev_ops = {
       	.core   =&mt9d131_subdev_core_ops,
       	.video  =&mt9d131_subdev_video_ops,
       	.pad    =&mt9d131_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops mt9d131_subdev_internal_ops = {
       	.registered = mt9d131_registered,
       	.open = mt9d131_open,
       	.close = mt9d131_close,
};



static int mt9d131_probe(struct i2c_client *client,
                               const struct i2c_device_id *did)
{	 
       	int ret;
       	unsigned int i;
       	struct mt9d131 *mt9d131;
       	struct mt9d131_platform_data *pdata = client->dev.platform_data;
       	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	
       	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
               	dev_warn(&adapter->dev,
                       	"I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
               	return -EIO;
       	}

       	mt9d131 = kzalloc(sizeof(struct mt9d131), GFP_KERNEL);
       	if (!mt9d131)
               	return -ENOMEM;

	mt9d131->pdata = pdata;

       	v4l2_ctrl_handler_init(&mt9d131->ctrls, 4);

	v4l2_ctrl_new_std(&mt9d131->ctrls,&mt9d131_ctrl_ops,
                 	V4L2_CID_VFLIP, 0, 1, 1, 0);
       	v4l2_ctrl_new_std(&mt9d131->ctrls,&mt9d131_ctrl_ops,
                         V4L2_CID_HFLIP, 0, 1, 1, 0);

       	v4l2_ctrl_new_std(&mt9d131->ctrls,&mt9d131_ctrl_ops,
                         V4L2_CID_ZOOM_ABSOLUTE, MT9D131_MIN_ABSZOOM,
                         MT9D131_MAX_ABSZOOM, MT9D131_ABSZOOM_STEP,
                         MT9D131_DEF_ABSZOOM);

	v4l2_ctrl_new_std(&mt9d131->ctrls,&mt9d131_ctrl_ops,
                         V4L2_CID_PAN_ABSOLUTE, 0,
                         400, 10,
                         0);
     
       	mt9d131->subdev.ctrl_handler =&mt9d131->ctrls;
       	if (mt9d131->ctrls.error)
               	printk(KERN_INFO "%s: control initialization error %d\n",
                      	__func__, mt9d131->ctrls.error);

       	mutex_init(&mt9d131->power_lock);
       	v4l2_i2c_subdev_init(&mt9d131->subdev, client,&mt9d131_subdev_ops);
       	mt9d131->subdev.internal_ops =&mt9d131_subdev_internal_ops;

       	mt9d131->pad.flags = MEDIA_PAD_FL_SOURCE;
       	ret = media_entity_init(&mt9d131->subdev.entity, 1,&mt9d131->pad, 0);
       	if (ret)
               	return ret;

       	mt9d131->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	if (mt9d131->pdata->version == MT9D131_MONOCHROME_VERSION)
		mt9d131->format.code = V4L2_MBUS_FMT_Y12_1X12;
	else
		mt9d131->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
       	return 0;
	
}

static int mt9d131_remove(struct i2c_client *client)
{	 
       	struct v4l2_subdev *sd = i2c_get_clientdata(client);
       	struct mt9d131 *mt9d131 = container_of(sd, struct mt9d131, subdev);
	v4l2_device_unregister_subdev(sd);
       	media_entity_cleanup(&sd->entity);
       	kfree(mt9d131);
       	return 0;
}

static const struct i2c_device_id mt9d131_id[] = {
	{ "mt9d131", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9d131_id);

static struct i2c_driver mt9d131_i2c_driver = {
	.driver = {
		.name = "mt9d131",
	},
	.probe    = mt9d131_probe,
	.remove   = mt9d131_remove,
	.id_table = mt9d131_id,
};

/************************************************************************
			module function
************************************************************************/
static int __init mt9d131_module_init(void)
{
	return i2c_add_driver(&mt9d131_i2c_driver);
}

static void __exit mt9d131_module_exit(void)
{
	i2c_del_driver(&mt9d131_i2c_driver);
}

module_init(mt9d131_module_init);
module_exit(mt9d131_module_exit);

MODULE_DESCRIPTION("mt9d131 soc sensor(2 meg pixel) driver");
MODULE_AUTHOR("Aptina");
MODULE_LICENSE("GPL v2");
