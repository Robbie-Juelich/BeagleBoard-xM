/*
 * Aptina AP0100 sensor driver
 *
 * Copyright (C) 2012 Aptina Imaging
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
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/sysfs.h>

#include <media/ap0100.h>
#include <media/v4l2-int-device.h>
#include <media/v4l2-chip-ident.h>

#undef AP0100_DEBUG_REG_ACCESS
#undef AP0100_DEBUG

/************************************************************************
			macro
************************************************************************/
#define AP0100_MAX_WIDTH	1280
#define AP0100_MAX_HEIGHT	720
#define AP0100_DEFAULT_WIDTH	1280
#define AP0100_DEFAULT_HEIGHT	720

#define AP0100_XCLK_NOM_1	12000000
#define AP0100_XCLK_NOM_2	24000000

/************************************************************************
			Register Address
************************************************************************/
#define AP0100_CHIP_VERSION_REG         0x0000
#define AP0100_CHIP_ID			0x0062
#define AP0100_RESET_REG		0x001A
#define AP0100_RESET			0x0E05
#define AP0100_NORMAL			0x0E04
#define AP0100_SEQ_PORT                 0x3086
#define AP0100_SEQ_PORT_CTRL            0x3088
#define AP0100_TEST_REG                 0x3070
#define AP0100_READ_MODE                0xC846
#define AP0100_MODE_SELECT              0xC88C
#define AP0100_PATTERN_SELECT           0xC88F
#define AP0100_SENSOR                   0x0
#define AP0100_TEST_GENERATOR           0x2
#define AP0100_SOLID_COLOR              0x1
#define AP0100_COLOR_BAR                0x4

#define AP0100_COMMAND_REGISTER         0x0040
#define AP0100_CMD_PARAM_0              0xFC00
#define AP0100_CMD_PARAM_1              0xFC02
#define AP0100_CMD_PARAM_2              0xFC04
#define AP0100_CMD_PARAM_3              0xFC06
#define AP0100_CMD_PARAM_4              0xFC08
#define AP0100_CMD_PARAM_5              0xFC0A
#define AP0100_CMD_PARAM_6              0xFC0C
#define AP0100_CMD_PARAM_7              0xFC0E
#define AP0100_CHANGE_CONFIG            0x2800
#define AP0100_SUSPEND                  0x4000
#define AP0100_SOFT_STANDBY             0x5000
#define AP0100_SET_STATE                0x8100
#define AP0100_GET_STATE                0x8101

struct ap0100_frame_size {
	u16 width;
	u16 height;
};

struct ap0100_priv {
	struct ap0100_platform_data  *pdata;
	struct v4l2_int_device  *v4l2_int_device;
	struct i2c_client  *client;
	struct v4l2_pix_format  pix;
	struct v4l2_fract timeperframe;
	unsigned long xclk_current;
	int fps;
	int scaler;
	int ver;
	int  model;
	u32  flags;
/* for flags */
#define INIT_DONE  (1<<0)
};
struct ap0100_priv sysPriv;

static const struct v4l2_fmtdesc ap0100_formats[] = {
	{
		.description = "standard UYVY 4:2:2",
		.pixelformat = V4L2_PIX_FMT_UYVY,
	},
};

static const unsigned int ap0100_num_formats = ARRAY_SIZE(ap0100_formats);

/**************************supported sizes******************************/
const static struct ap0100_frame_size   ap0100_supported_framesizes[]={
	{ 1280, 720 },
};

enum ap0100_image_size {
	HDV_720P_10FPS,
};

enum ap0100_image_size ap0100_current_size_index;

const struct v4l2_fract ap0100_frameintervals[] = {
	{ .numerator = 20, .denominator = 2 },
};

/**
 * ap0100_reg_read - read resgiter value
 * @client: pointer to i2c client
 * @command: register address
 */
static int ap0100_reg_read(const struct i2c_client *client, u16 addr)
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
 * ap0100_reg_write - read resgiter value
 * @client: pointer to i2c client
 * @command: register address
 * @data: value to be written 
 */ 
static int ap0100_reg_write(const struct i2c_client *client, u16 addr,
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
        if (ret >= 0)
                return 0;

        v4l_err(client, "Write failed at 0x%x error %d\n", addr, ret);
        return ret;
}

/* ap0100_init_camera - initialize camera settings in context A
 * @client: pointer to i2c client
 * Initialize camera settings */ 
static int 
ap0100_init_camera(const struct i2c_client *client)
{
	int count, ret;
        unsigned int data;

        ret = ap0100_reg_write(client, AP0100_CMD_PARAM_0, AP0100_CHANGE_CONFIG);
        if (ret < 0)
                return ret;
        ret = ap0100_reg_write(client, AP0100_COMMAND_REGISTER, AP0100_SET_STATE);
        if (ret < 0)
                return ret;
        data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
        count = 0;
        while(data){
                data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
                if(count++ > 5){
			ret = ap0100_reg_write(client, AP0100_CMD_PARAM_0, AP0100_CHANGE_CONFIG);
			ret = ap0100_reg_write(client, AP0100_COMMAND_REGISTER, AP0100_SET_STATE);
                	data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
			msleep(1);
                        if(!data)
                                break;
#ifdef AP0100_DEBUG	
                        else
				printk(KERN_INFO "Failed to set CHANGE CONFIG: ERROR = 0x%x\n",data);
#endif
                }
                msleep(10);
        }

	return ret;
}

/* ap0100_suspend_camera - suspend camera 
 * @client: pointer to i2c client
 * Suspend camera */ 
void
ap0100_suspend_camera(const struct i2c_client *client)
{
	int count;
        unsigned int data;
	
	ap0100_reg_write(client, AP0100_CMD_PARAM_0, AP0100_SUSPEND);
	ap0100_reg_write(client, AP0100_COMMAND_REGISTER, AP0100_SET_STATE);
	data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
	count = 0;
	while(data){
		data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
		if(count++ > 5){
			ap0100_reg_write(client, AP0100_CMD_PARAM_0, AP0100_SUSPEND);
			ap0100_reg_write(client, AP0100_COMMAND_REGISTER, AP0100_SET_STATE);
			data = ap0100_reg_read(client, AP0100_COMMAND_REGISTER);
			msleep(1);
                        if(!data)
                                break;
#ifdef AP0100_DEBUG	
                        else
				printk(KERN_INFO "Failed to set SUSPEND state: ERROR = 0x%x\n",data);
#endif
		}	
		msleep(10);
	}	
}

/**
 * ap0100_detect - Detect if an ap0100 is present, and if so which revision
 * @client: pointer to the i2c client driver structure
 *
 * Returns a negative error number if no device is detected
 */
static int 
ap0100_detect(struct i2c_client *client)
{
	const char *devname;
	u16 chipid;
	
	if (!client)
		return -ENODEV;
	
	chipid = ap0100_reg_read(client, AP0100_CHIP_VERSION_REG);
	if(chipid == AP0100_CHIP_ID) {
		devname = "ap0100";
		dev_info(&client->dev, "Aptina %s is detected (chipID = 0x%04x)\n", devname, chipid);
		return 0;
	}
			
	dev_err(&client->dev, "Product ID error %04x\n", chipid);
	return -ENODEV;
}

/************************************************************************
			v4l2_ioctls
************************************************************************/
/**
 * ap0100_v4l2_int_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int 
ap0100_v4l2_int_s_power(struct v4l2_int_device *s, enum v4l2_power power)
{
	struct ap0100_priv *priv = s->priv;
	struct i2c_client *client = priv->client;
	static int sensor_is_ready = 0;
	
	int ret;

	switch (power) {
	case V4L2_POWER_STANDBY:
		break;

	case V4L2_POWER_OFF:
		if(--sensor_is_ready > 0){
#ifdef AP0100_DEBUG	
			printk("ap0100 power: V4L2_POWER_OFF is not granted! power = %d\n", sensor_is_ready);
#endif
			break;	
		}
		
		ap0100_suspend_camera(client);
		ret = priv->pdata->power_set(s, power);
		if (ret < 0) {
			dev_err(&client->dev, "Unable to set target board power" " state (OFF/STANDBY)\n");
			printk("ap0100 power: V4L2_POWER_OFF failed, power = %d\n", sensor_is_ready);
			return ret;
		} else {
#ifdef AP0100_DEBUG	
			printk("ap0100 power: V4L2_POWER_OFF is successful, power = %d\n", sensor_is_ready);
#endif
		}
		break;
	case V4L2_POWER_ON:
		if(sensor_is_ready) {
			sensor_is_ready = 1;
#ifdef AP0100_DEBUG	
			printk("ap0100 power: V4L2_POWER_ON is not needed since it is already on\n");
#endif
			break;
		}

		ret = priv->pdata->power_set(s, power);
		if (ret < 0) {
			dev_err(&client->dev, "Unable to set target board power " "state (ON)\n");
			return ret;
		}
#ifdef AP0100_DEBUG	
		printk("ap0100 power: V4L2_POWER_ON is successful, power = %d\n", sensor_is_ready);
#endif

		if (!(priv->flags & INIT_DONE)) {
			ret = ap0100_detect(client);
			if (ret < 0) {
				dev_err(&client->dev, "Unable to detect sensor\n");
				return ret;
			}
			priv->flags |= INIT_DONE;
		}

		ret = ap0100_init_camera(client);
		if (ret < 0) {
			dev_err(&client->dev, "Unable to initialize sensor\n");
			return ret;
		} else {
			dev_info(&client->dev, "Aptina ap0100 is initialized\n");
			sensor_is_ready=1; 
		} 
		break;
	default:
		dev_err(&client->dev, " ap0100 power: unknow 'power' value: %d\n", power);
		break;
	}
	
	return 0;
}

/**
 * ap0100_v4l2_int_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int 
ap0100_v4l2_int_enum_fmt_cap(struct v4l2_int_device *s,struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if(index >= ARRAY_SIZE(ap0100_formats)) 
			return -EINVAL;

        break;
	default:
		printk(KERN_ERR KBUILD_MODNAME "ap0100_v4l2_int_enum_fmt_cap() failed, return -EINVAL\n");
		return -EINVAL;
	}

	strlcpy(fmt->description, ap0100_formats[index].description, sizeof(fmt->description));
	fmt->pixelformat = ap0100_formats[index].pixelformat;

	return 0;
}

/**
 * ap0100_v4l2_int_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int 
ap0100_v4l2_int_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;

#ifdef AP0100_DEBUG
	printk("Request format from input: format = 0x%x, width = %d, height = %d\n",
		pix->pixelformat, pix->width, pix->height);
#endif

	pix->pixelformat = ap0100_formats[0].pixelformat;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = pix->width * 2;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	pix->width = AP0100_DEFAULT_WIDTH;
	pix->height = AP0100_DEFAULT_HEIGHT;

	return 0;
}

/**
 * ap0100_v4l2_int_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int 
ap0100_v4l2_int_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	int ret;

#ifdef AP0100_DEBUG	
	printk("S_FMT: format = 0x%x, width = %d, height = %d\n", pix->pixelformat, pix->width, pix->height);
#endif
	ret = ap0100_v4l2_int_try_fmt_cap(s, f);
		
	return ret;
}

/**
 * ap0100_v4l2_int_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int 
ap0100_v4l2_int_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct ap0100_priv *priv = s->priv;
	
	f->fmt.pix.width	= priv->pix.width;
	f->fmt.pix.height	= priv->pix.height;
	f->fmt.pix.pixelformat	= V4L2_COLORSPACE_SRGB;
	f->fmt.pix.pixelformat	= priv->pix.pixelformat;
	f->fmt.pix.field	= V4L2_FIELD_NONE;

	return 0;
}

/**
 * ap0100_v4l2_int_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 * ----->Note, this function is not active in this release.<------
 */
static int ap0100_v4l2_int_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	//Not yet available.  Will be implemented later
	return 0;
}

/**
 * ap0100_v4l2_int_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int 
ap0100_v4l2_int_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct ap0100_priv *priv = s->priv;
	struct v4l2_captureparm *cparm	= &a->parm.capture;

	if(a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = priv->timeperframe;
	cparm->timeperframe.numerator  = ap0100_frameintervals[0].numerator;
	cparm->timeperframe.denominator = ap0100_frameintervals[0].denominator;

	return 0;
}

/**
 * ap0100_v4l2_int_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int 
ap0100_v4l2_int_g_priv(struct v4l2_int_device *s, void *p)
{
	struct ap0100_priv *priv = s->priv;

	return priv->pdata->priv_data_set(p);
}

/**
 * ap0100_v4l2_int_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's ifparm
 *
 * Returns device's (sensor's) ifparm in p parameter
 */
static int ap0100_v4l2_int_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	struct ap0100_priv *priv = s->priv;
	int rval;

	if (p == NULL)
		return -EINVAL;

	if (!priv->pdata->ifparm)
		return -EINVAL;

	rval = priv->pdata->ifparm(p);
	if (rval) {
		v4l_err(priv->client, "g_ifparm.Err[%d]\n", rval);
		return rval;
	}

	return 0;
}

/**
 * ap0100_v4l2_int_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 */
static int 
ap0100_v4l2_int_enum_framesizes(struct v4l2_int_device *s, struct v4l2_frmsizeenum *frms)
{
	int ifmt;

	for (ifmt = 0; ifmt < ARRAY_SIZE(ap0100_formats); ifmt++){
		if (ap0100_formats[ifmt].pixelformat == frms->pixel_format){
#ifdef AP0100_DEBUG	
			printk("Found a matched pixelformat:0x%x at table entry %d\n",
				ap0100_formats[ifmt].pixelformat, ifmt);
#endif
			break;
		}
	}
	if (ifmt == ARRAY_SIZE(ap0100_formats)){
		printk(KERN_ERR KBUILD_MODNAME "Couldn't find any match for the given frame format:0x%x\n",frms->pixel_format);
	}

	if (frms->index > ARRAY_SIZE(ap0100_supported_framesizes)){
#ifdef AP0100_DEBUG	
		printk("We've already reached all discrete framesizes %d!\n", ARRAY_SIZE(ap0100_supported_framesizes));
#endif
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = ap0100_supported_framesizes[frms->index - 1].width;
		frms->discrete.height = ap0100_supported_framesizes[frms->index - 1].height;
		return -EINVAL;
	}

	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frms->discrete.width = ap0100_supported_framesizes[frms->index].width;
	frms->discrete.height = ap0100_supported_framesizes[frms->index].height;

	return 0;
}

static int 
ap0100_v4l2_int_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *frmi)
{
	int ifmt;
	int max_size;

	for (ifmt = 0; ifmt < ARRAY_SIZE(ap0100_formats); ifmt++)
		if (ap0100_formats[ifmt].pixelformat == frmi->pixel_format)
			break;

	max_size = ARRAY_SIZE(ap0100_supported_framesizes);
	if(frmi->index == max_size)
		return -EINVAL;
	
	for(ifmt = 0; ifmt < max_size; ifmt++) {
		if(frmi->width <= ap0100_supported_framesizes[ifmt].width) {
			frmi->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			frmi->discrete.numerator   = ap0100_frameintervals[frmi->index].numerator;
			frmi->discrete.denominator = ap0100_frameintervals[frmi->index].denominator;
#ifdef AP0100_DEBUG	
			printk("ifmt = %d, frmi->index = %d, frmi->width = %d\n", ifmt, frmi->index, frmi->width);
#endif
			if(frmi->discrete.denominator <= ap0100_frameintervals[max_size - ifmt - 1].denominator) 
				return 0;
			else
				return -EINVAL;

			return 0;
		}
	}
	return 0;
}

static struct v4l2_int_ioctl_desc ap0100_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_enum_framesizes },
	{ .num = vidioc_int_enum_frameintervals_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_enum_frameintervals },
	{ .num = vidioc_int_s_power_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_s_power },
	{ .num = vidioc_int_g_priv_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_g_priv },
	{ .num = vidioc_int_g_ifparm_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_g_ifparm },
	{ .num = vidioc_int_enum_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_g_fmt_cap }, 
	{ .num = vidioc_int_s_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_g_parm },
	{ .num = vidioc_int_s_parm_num,
	  .func = (v4l2_int_ioctl_func *)ap0100_v4l2_int_s_parm },
};

#ifdef AP0100_DEBUG_REG_ACCESS
/***********************************************************************************
 *                               Sysfs                                             *                               
************************************************************************************/
/* Basic register read write support */
static u16 ap0100_attr_basic_addr  = 0x0000;

static ssize_t
ap0100_basic_reg_addr_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", ap0100_attr_basic_addr);
}

static ssize_t
ap0100_basic_reg_addr_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u16 val;
	sscanf(buf, "%hx", &val);
	ap0100_attr_basic_addr = (u16) val;
	return n;
}

static DEVICE_ATTR( basic_reg_addr, S_IRUGO|S_IWUSR, ap0100_basic_reg_addr_show, ap0100_basic_reg_addr_store);


static ssize_t
ap0100_basic_reg_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 val;
	int ret;

	ret = ap0100_reg_read(sysPriv.client, ap0100_attr_basic_addr, &val);
	if(ret < 0){        
		printk(KERN_INFO "ap0100: Basic register read failed");
		return 1; /* nothing processed */
	} else {
		return sprintf(buf, "0x%x\n", val);
	}
}

static ssize_t
ap0100_basic_reg_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u32 val;
	sscanf(buf, "%x", &val);

	if (ap0100_reg_write(sysPriv.client, ap0100_attr_basic_addr, (u16)val)) {
		printk(KERN_INFO "ap0100_basic_reg_val_store(): val=0x%x is written failed, return n=%d\n",val,n);
		return n; /* nothing processed */
	} else {
		return n;
	}
}
static DEVICE_ATTR( basic_reg_val, S_IRUGO|S_IWUSR, ap0100_basic_reg_val_show, ap0100_basic_reg_val_store);


/* Exposure time access support */
static ssize_t
ap0100_exposure_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 val;
	struct vcontrol *lvc;
	int i = find_vctrl(V4L2_CID_EXPOSURE);
	if (i < 0)
		return -EINVAL;
	lvc = &ap0100_video_control[i];
	val = lvc->current_value;
	
	if(val < 0){        
		printk(KERN_INFO "ap0100: Exposure value read failed");
		return 1; /* nothing processed */
	} else {
		return sprintf(buf, "%d\n", val);
	}
}

static ssize_t
ap0100_exposure_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u32 val;
	struct i2c_client *client;
	struct vcontrol *lvc;
	
	sscanf(buf, "%d", &val);
	client = sysPriv.client;
		
	lvc = &ap0100_video_control[V4L2_CID_EXPOSURE];	

	if (ap0100_set_exposure_time((u32)val, client, lvc)) {
		printk(KERN_INFO "ap0100: Exposure write failed");
		return n; /* nothing processed */
	} else {
		return n;
    }
}

static DEVICE_ATTR( exposure_val, S_IRUGO|S_IWUSR, ap0100_exposure_val_show, ap0100_exposure_val_store);


/* Global Gain access support */
static ssize_t
ap0100_gain_val_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 val;
	struct vcontrol *lvc;
    
	int i = find_vctrl(V4L2_CID_GAIN);
	if (i < 0)
		return -EINVAL;
	lvc = &ap0100_video_control[i];
	val = lvc->current_value;
      
	if(val < 0){        
		printk(KERN_INFO "ap0100: Global Gain value read failed");
		return 1; /* nothing processed */
	} else {
		return sprintf(buf, "%d\n", val);
    }
}

static ssize_t
ap0100_gain_val_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	u16 val;
	struct i2c_client *client;
	struct vcontrol *lvc;
	
	sscanf(buf, "%hd", &val);
	client = sysPriv.client;
		
	lvc = &ap0100_video_control[V4L2_CID_GAIN];	
		
	if (ap0100_set_gain(val, client, lvc)) {
		printk(KERN_INFO "ap0100: Global gain write failed");
		return n; /* nothing processed */
	} else {
		return n;
	}
}

static DEVICE_ATTR( gain_val, S_IRUGO|S_IWUSR, ap0100_gain_val_show, ap0100_gain_val_store);

static struct attribute *ap0100_sysfs_attr[] = {
	&dev_attr_basic_reg_addr.attr,
	&dev_attr_basic_reg_val.attr,
	&dev_attr_exposure_val.attr,
	&dev_attr_gain_val.attr,
};

static int 
ap0100_sysfs_add(struct kobject *kobj)
{
	int i = ARRAY_SIZE(ap0100_sysfs_attr);
	int rval = 0;
	
	do {
		rval = sysfs_create_file(kobj, ap0100_sysfs_attr[--i]);
	} while((i > 0) && (rval == 0));
	return rval;
}

static int 
ap0100_sysfs_rm(struct kobject *kobj)
{
	int i = ARRAY_SIZE(ap0100_sysfs_attr);
	int rval = 0;

	do {
		sysfs_remove_file(kobj, ap0100_sysfs_attr[--i]);
	} while(i > 0);
	return rval;
}
#endif	/* AP0100_DEBUG_REG_ACCESS */

static struct v4l2_int_slave ap0100_slave = {
	.ioctls = ap0100_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(ap0100_ioctl_desc),
};

/**
 * ap0100_probe - probing the ap0100 soc sensor
 * @client: i2c client driver structure
 * @did:    device id of i2c_device_id structure
 *
 * Upon the given i2c client, the sensor's module id is to be retrieved.
 */
static int 
ap0100_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
	struct ap0100_priv *priv;
	struct v4l2_int_device	*v4l2_int_device;
	int ret;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "no platform data?\n");
		return -ENODEV;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	v4l2_int_device = kzalloc(sizeof(*v4l2_int_device), GFP_KERNEL);
	if (!v4l2_int_device) {
		kfree(priv);
		return -ENOMEM;
	}

	v4l2_int_device->module = THIS_MODULE;
	strncpy(v4l2_int_device->name, "ap0100", sizeof(v4l2_int_device->name));
	
	v4l2_int_device->type = v4l2_int_type_slave;
	v4l2_int_device->u.slave = &ap0100_slave;

	v4l2_int_device->priv = priv;
	priv->v4l2_int_device = v4l2_int_device;
	priv->client = client;
	priv->pdata = client->dev.platform_data;
	
	/* Setting Pixel Values */
	priv->pix.width       = AP0100_DEFAULT_WIDTH;
	priv->pix.height      = AP0100_DEFAULT_HEIGHT;
	priv->pix.pixelformat = ap0100_formats[0].pixelformat;
	i2c_set_clientdata(client, priv);
	sysPriv.client = priv->client;

	ret = v4l2_int_device_register(priv->v4l2_int_device);
	if (ret) {
		i2c_set_clientdata(client, NULL);
		kfree(v4l2_int_device);
		kfree(priv);
	}
	
#ifdef AP0100_DEBUG_REG_ACCESS
	ap0100_sysfs_add(&client->dev.kobj);
#endif
	return ret;
}

/**
 * ap0100_remove - remove the ap0100 soc sensor driver module
 * @client: i2c client driver structure
 *
 * Upon the given i2c client, the sensor driver module is removed.
 */
static int 
ap0100_remove(struct i2c_client *client)
{
	struct ap0100_priv *priv = i2c_get_clientdata(client);

	v4l2_int_device_unregister(priv->v4l2_int_device);
	i2c_set_clientdata(client, NULL);

#ifdef AP0100_DEBUG_REG_ACCESS
	ap0100_sysfs_rm(&client->dev.kobj);
#endif	
	
	kfree(priv->v4l2_int_device);
	kfree(priv);
	return 0;
}

static const struct i2c_device_id ap0100_id[] = {
	{ "ap0100", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ap0100_id);

static struct i2c_driver ap0100_i2c_driver = {
	.driver = {
		.name = "ap0100",
	},
	.probe    = ap0100_probe,
	.remove   = ap0100_remove,
	.id_table = ap0100_id,
};

/************************************************************************
			module function
************************************************************************/
static int __init ap0100_module_init(void)
{
	return i2c_add_driver(&ap0100_i2c_driver);
}

static void __exit ap0100_module_exit(void)
{
	i2c_del_driver(&ap0100_i2c_driver);
}

module_init(ap0100_module_init);
module_exit(ap0100_module_exit);

MODULE_DESCRIPTION("ap0100 driver");
MODULE_AUTHOR("Aptina");
MODULE_LICENSE("GPL v2");
