/*
 * A V4L2 driver for imx274 Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("lwj");
MODULE_DESCRIPTION("A low-level driver for imx274 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (72*1000*1000)
#define V4L2_IDENT_SENSOR 0x0274

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The IMX274 i2c address
 */
#define I2C_ADDR 0x30

#define SENSOR_NAME "imx274_slvds"
#define DOL_RHS1	35
#define DOL_RATIO	16

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_10lane_1080p_dol[] = {
	{0x0000, 0x1a},

	{0x003E, 0x00},

	{0x0120, 0x80},
	{0x0121, 0x00},
	{0x0122, 0x03},
	{0x0129, 0x68},
	{0x012A, 0x03},
	{0x012D, 0x02},
	{0x010B, 0x00},

	{0x00EE, 0x01},

	{0x032C, 0x51},
	{0x032D, 0x04},
	{0x034A, 0x51},
	{0x034B, 0x04},
	{0x05B6, 0x51},
	{0x05B7, 0x04},
	{0x05B8, 0x51},
	{0x05B9, 0x04},

	{0x004C, 0x00},
	{0x004D, 0x03},

	{0x031C, 0x1A},
	{0x031D, 0x00},
	{0x0502, 0x02},
	{0x0529, 0x0E},
	{0x052A, 0x0E},
	{0x052B, 0x0E},
	{0x0538, 0x0E},
	{0x0539, 0x0E},
	{0x0553, 0x00},
	{0x057D, 0x05},

	{0x057F, 0x05},

	{0x0581, 0x04},

	{0x0583, 0x76},
	{0x0587, 0x01},
	{0x05BB, 0x0E},
	{0x05BC, 0x0E},
	{0x05BD, 0x0E},
	{0x05BE, 0x0E},
	{0x05BF, 0x0E},
	{0x066E, 0x00},

	{0x066F, 0x00},

	{0x0670, 0x00},

	{0x0671, 0x00},

	{REG_DLY, 0x10},

	{0x00E6, 0x01},
	{0x00E8, 0x01},
	{0x0000, 0x18},

	{REG_DLY, 0x10},

	{0x0001, 0x10},

	{0x0009, 0x01},

	{0x0003, 0x22},/*6 lane*/

	{0x00E2, 0x02},/*mode sel*/

	{0x0004, 0x07},
	{0x0005, 0x21},
	{0x0006, 0x00},
	{0x0007, 0x11},

	{0x0342, 0x0A},
	{0x0343, 0x00},
	{0x0344, 0x1A},
	{0x0345, 0x00},
	{0x0528, 0x0E},
	{0x0554, 0x00},
	{0x0555, 0x01},
	{0x0556, 0x01},
	{0x0557, 0x01},
	{0x0558, 0x01},
	{0x0559, 0x00},
	{0x055A, 0x00},
	{0x05BA, 0x0E},
	{0x066A, 0x1B},
	{0x066B, 0x1A},
	{0x066C, 0x19},
	{0x066D, 0x17},
	{0x03A6, 0x01},
	{0x006B, 0x05},

	{0x000C, 0x0C},
	{0x000D, 0x00},
	{0x000E, 0x00},
	{0x000F, 0x00},

	{0x0019, 0x31},/*DOL mode sel,LI output enable*/
	/*{0x0019, 0x11},DOL mode sel,LI not output enable*/

	{0x002E, 0x06},
	{0x002F, 0x00},
	{0x0030, 0x10},
	{0x0031, 0x00},
	{0x0032, DOL_RHS1 & 0xff},
	{0x0033, (DOL_RHS1 >> 8) & 0xff},
	{0x0041, 0x31},
	{0x0042, 0x04},
	{0x0043, 0x01},
	{0x00E9, 0x01},

	{0x00F4, 0x00},
};

static struct regval_list sensor_1080p30_regs[] = {
	/*Mode 2 raw12*/
	{0x0000, 0x1A},
	{0x003E, 0x00},

	{0x0120, 0x80},/*PLRD1*/
	{0x0121, 0x00},/*PLRD1*/
	{0x0122, 0x03},/*PLRD2*/
	{0x0129, 0x68},/*PLRD3*/
	{0x012A, 0x03},/*PLRD4*/
	{0x012D, 0x02},/*PLRD5*/
	{0x010B, 0x00},
	{0x004C, 0x00},
	{0x004D, 0x03},
	{0x031C, 0x1A},
	{0x031D, 0x00},
	{0x0502, 0x02},
	{0x0529, 0x0E},
	{0x052A, 0x0E},
	{0x052B, 0x0E},
	{0x0538, 0x0E},
	{0x0539, 0x0E},
	{0x0553, 0x00},
	{0x057D, 0x05},
	{0x057F, 0x05},
	{0x0581, 0x04},
	{0x0583, 0x76},
	{0x0587, 0x01},
	{0x05BB, 0x0E},
	{0x05BC, 0x0E},
	{0x05BD, 0x0E},
	{0x05BE, 0x0E},
	{0x05BF, 0x0E},
	{0x066E, 0x00},
	{0x066F, 0x00},
	{0x0670, 0x00},
	{0x0671, 0x00},
	{0x00EE, 0x01},

	{0x032C, 0x8E},
	{0x032D, 0x12},
	{0x034A, 0x8E},
	{0x034B, 0x12},
	{0x05B6, 0x8E},
	{0x05B7, 0x12},
	{0x05B8, 0x8E},
	{0x05B9, 0x12},
	{0x00E2, 0x02},

	{0x0003, 0x33},
	{0x0004, 0x02},
	{0x0005, 0x27},
	{0x0006, 0x00},
	{0x0007, 0x11},
	{0x0342, 0xFF},
	{0x0343, 0x01},
	{0x0344, 0xFF},
	{0x0345, 0x01},
	{0x0528, 0x0F},
	{0x0554, 0x00},
	{0x0555, 0x00},
	{0x0556, 0x00},
	{0x0557, 0x00},
	{0x0558, 0x00},
	{0x0559, 0x1F},
	{0x055A, 0x1F},
	{0x05BA, 0x0F},
	{0x066A, 0x00},
	{0x066B, 0x00},
	{0x066C, 0x00},
	{0x066D, 0x00},
	{0x03A6, 0x01},
	{0x006B, 0x07},
	{0x000E, 0x00},

	{0x000a, 0x00},/*ana gain*/
	{0x000b, 0x04},
	{0x0012, 0x04},/*dig gain*/


	{0xffff, 0x10},
	{0x00E6, 0x01},
	{0x00E8, 0x01},
	{0x0000, 0x18},
	{0xffff, 0x10},

	{0x0001, 0x10},
	{0x0009, 0x01},
	{0x00F4, 0x00},
	{0x0019, 0x10},
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_spi_read_byte(struct spi_device *spi, loff_t from,
			size_t len, u_char *buf)
{
	struct spi_transfer t[2];
	struct spi_message m;
	u8 command[3] = {0};

	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	command[0] = 0x80;
	command[1] = (from >> 8) & 0xff;
	command[2] = from & 0xff;
	t[0].tx_buf = command;
	t[0].len = 3;
	spi_message_add_tail(&t[0], &m);

	t[1].rx_buf = buf;
	t[1].len = len;
	spi_message_add_tail(&t[1], &m);

	spi_sync(spi, &m);

	return 0;
}

static int sensor_spi_write_byte(struct spi_device *spi, loff_t to,
			size_t len, const u_char buf)
{
	struct spi_transfer t;
	struct spi_message m;
	u8 command[4] = {0};

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));
	command[0] = 0x81;
	command[1] = (to >> 8) & 0xff;
	command[2] = to & 0xff;
	command[3] = buf;
	t.tx_buf = command;
	t.len = 4;
	spi_message_add_tail(&t, &m);

	spi_sync(spi, &m);

	return 0;
}

static int sensor_spi_read(struct v4l2_subdev *sd, u16 reg, u8 *val)
{
	struct spi_device *spi = v4l2_get_subdevdata(sd);
	loff_t from = reg;

	return sensor_spi_read_byte(spi, from, 1, val);
}

static int sensor_spi_write(struct v4l2_subdev *sd, u16 reg, u8 val)
{
	struct spi_device *spi = v4l2_get_subdevdata(sd);
	loff_t to = reg;

	return sensor_spi_write_byte(spi, to, 1, val);
}

static int sensor_spi_write_array(struct v4l2_subdev *sd,
		struct regval_list *regs, int array_size)
{
	int i = 0;

	if (!regs)
		return -EINVAL;

	while (i < array_size) {
		if (regs->addr == REG_DLY)
			msleep(regs->data);
		else
			sensor_spi_write(sd, regs->addr, (u8)regs->data);
		i++;
		regs++;
	}
	return 0;
}

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int imx274_sensor_vts;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);
	u8 explow, exphigh;
	int shutter, exp_val_m;

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		/*LEF*/
		shutter = imx274_sensor_vts - (exp_val>>4) - 1;
		if (shutter < DOL_RHS1 + 6) {
			shutter = DOL_RHS1 + 6;
			exp_val = (imx274_sensor_vts - shutter - 1) << 4;
		}
		sensor_dbg("long exp_val: %d, shutter: %d\n", exp_val, shutter);

		exphigh = (unsigned char) ((0xff00&shutter)>>8);
		explow	= (unsigned char) ((0x00ff&shutter));

		sensor_spi_write(sd, 0x0030, explow);
		sensor_spi_write(sd, 0x0031, exphigh);

		/*SEF*/
		exp_val_m = exp_val/DOL_RATIO;
		if (exp_val_m < 2 * 16)
			exp_val_m = 2 * 16;
		shutter = DOL_RHS1 - (exp_val_m>>4) - 1;
		if (shutter < 2)
			shutter = 2;

		sensor_dbg("short exp_val: %d, shutter: %d\n", exp_val_m, shutter);

		exphigh = (unsigned char) ((0xff00&shutter)>>8);
		explow	= (unsigned char) ((0x00ff&shutter));

		sensor_spi_write(sd, 0x002E, explow);
		sensor_spi_write(sd, 0x002F, exphigh);

	} else {
		shutter = imx274_sensor_vts-((exp_val + 8) >> 4);

		if (shutter > imx274_sensor_vts - 4)
			shutter = imx274_sensor_vts - 4;
		if (shutter < 12)
			shutter = 12;

		exphigh = (u8)((0xff00 & shutter) >> 8);
		explow = (u8)((0x00ff & shutter));

		sensor_spi_write(sd, 0x000c, explow);
		sensor_spi_write(sd, 0x000d, exphigh);

	}

	sensor_dbg("sensor_set_exp = %d line Done!\n", exp_val);

	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	u8 gainlow = 0;
	u8 gainhigh = 0;
	u8 gaindig = 0;
	int gainana = gain_val;

	if (gainana < 22*16) {
		gaindig = 0;
	} else if (gainana < 22*32) {
		gainana /= 2;
		gaindig = 1;
	} else if (gainana < 22*64) {
		gainana /= 4;
		gaindig = 2;
	} else if (gainana < 22*128) {
		gainana /= 8;
		gaindig = 3;
	} else if (gainana < 22*256) {
		gainana /= 16;
		gaindig = 4;
	} else if (gainana < 22*512) {
		gainana /= 32;
		gaindig = 5;
	} else {
		gainana /= 64;
		gaindig = 6;
	}

	gainana = 2048 - 32768 / gain_val;
	gainlow = (u8)(gainana & 0xff);
	gainhigh = (u8)((gainana >> 8) & 0x07);

	sensor_spi_write(sd, 0x000a, gainlow);
	sensor_spi_write(sd, 0x000b, gainhigh);
	sensor_spi_write(sd, 0x0012, gaindig);

	sensor_dbg("sensor_set_gain = %d, Done!\n", gain_val);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int exp_val, gain_val;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static void sensor_g_combo_sync_code(struct v4l2_subdev *sd,
				struct combo_sync_code *sync)
{
	int i;

	for (i = 0; i < 12; i++) {
		sync->lane_sof[i].low_bit = 0x0000ab00;
		sync->lane_sof[i].high_bit = 0xFFFF0000;
		sync->lane_sol[i].low_bit = 0x00008000;
		sync->lane_sol[i].high_bit = 0xFFFF0000;
		sync->lane_eol[i].low_bit = 0x00009d00;
		sync->lane_eol[i].high_bit = 0xFFFF0000;
		sync->lane_eof[i].low_bit = 0x0000b600;
		sync->lane_eof[i].high_bit = 0xFFFF0000;
	}
}

static void sensor_g_combo_lane_map(struct v4l2_subdev *sd,
				struct combo_lane_map *map)
{
	struct sensor_info *info = to_state(sd);

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		map->lvds_lane0 = LVDS_MAPPING_A_D0_TO_LANE0;
		map->lvds_lane1 = LVDS_MAPPING_A_D1_TO_LANE1;
		map->lvds_lane2 = LVDS_MAPPING_B_D2_TO_LANE2;
		map->lvds_lane3 = LVDS_MAPPING_B_D0_TO_LANE3;
		map->lvds_lane4 = LVDS_MAPPING_B_D3_TO_LANE4;
		map->lvds_lane5 = LVDS_MAPPING_C_D2_TO_LANE5;
		map->lvds_lane6 = LVDS_LANE6_NO_USE;
		map->lvds_lane7 = LVDS_LANE7_NO_USE;
		map->lvds_lane8 = LVDS_LANE8_NO_USE;
		map->lvds_lane9 = LVDS_LANE9_NO_USE;
		map->lvds_lane10 = LVDS_LANE10_NO_USE;
		map->lvds_lane11 = LVDS_LANE11_NO_USE;
	} else {
		map->lvds_lane0 = LVDS_MAPPING_A_D1_TO_LANE0;
		map->lvds_lane1 = LVDS_MAPPING_B_D2_TO_LANE1;
		map->lvds_lane2 = LVDS_MAPPING_B_D0_TO_LANE2;
		map->lvds_lane3 = LVDS_MAPPING_B_D3_TO_LANE3;
		map->lvds_lane4 = LVDS_LANE4_NO_USE;
		map->lvds_lane5 = LVDS_LANE5_NO_USE;
		map->lvds_lane6 = LVDS_LANE6_NO_USE;
		map->lvds_lane7 = LVDS_LANE7_NO_USE;
		map->lvds_lane8 = LVDS_LANE8_NO_USE;
		map->lvds_lane9 = LVDS_LANE9_NO_USE;
		map->lvds_lane10 = LVDS_LANE10_NO_USE;
		map->lvds_lane11 = LVDS_LANE11_NO_USE;

	}
}

static void sensor_g_combo_wdr_cfg(struct v4l2_subdev *sd,
				struct combo_wdr_cfg *wdr)
{
	wdr->line_code_mode = 1;

	wdr->line_cnt = 2;

	wdr->code_mask = 0x40ff;
	wdr->wdr_fid_mode_sel = 0;
	wdr->wdr_fid_map_en = 0x3;
	wdr->wdr_fid0_map_sel = 0x7;
	wdr->wdr_fid1_map_sel = 0x6;
	wdr->wdr_fid2_map_sel = 0;
	wdr->wdr_fid3_map_sel = 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	u8 rdval;

	ret = sensor_spi_read(sd, 0x0000, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_spi_write(sd, 0x0000, rdval | 0x01);
	else
		ret = sensor_spi_write(sd, 0x0000, rdval & 0xfe);
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(7000, 8000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(10000, 12000);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	u8 read_data = 0;

	sensor_spi_read(sd, 0x000e, &read_data);
	sensor_print("%s read value is 0x%x\n", __func__, read_data);

	sensor_spi_write(sd, 0x000e, 0x55);
	sensor_spi_read(sd, 0x000e, &read_data);
	sensor_print("%s read value is 0x%x\n", __func__, read_data);

	sensor_spi_write(sd, 0x000e, 0x66);
	sensor_spi_read(sd, 0x000e, &read_data);
	sensor_print("%s read value is 0x%x\n", __func__, read_data);
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = HD1080_WIDTH;
	info->height = HD1080_HEIGHT;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
				sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case GET_COMBO_SYNC_CODE:
		sensor_g_combo_sync_code(sd, (struct combo_sync_code *)arg);
		break;
	case GET_COMBO_LANE_MAP:
		sensor_g_combo_lane_map(sd, (struct combo_lane_map *)arg);
		break;
	case GET_COMBO_WDR_CFG:
		sensor_g_combo_wdr_cfg(sd, (struct combo_wdr_cfg *)arg);
		break;
	case SET_FPS:
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
	{
	 .width = 1920,
	 .height = 1080,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 2288,
	 .vts = 1155,
	 .pclk = 158 * 1000 * 1000,
	 .mipi_bps = 576 * 1000 * 1000,
	 .fps_fixed = 60,
	 .if_mode = LVDS_5CODE_WDR_MODE,
	 .wdr_mode = ISP_DOL_WDR_MODE,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (1155 - 4) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 1440 << 4,
	 .regs = sensor_10lane_1080p_dol,
	 .regs_size = ARRAY_SIZE(sensor_10lane_1080p_dol),
	 .set_size = NULL,
	 },

	{
	 .width = 1920,
	 .height = 1080,
	 .hoffset = 0,
	 .voffset = 16,
	 .hts = 2288,
	 .vts = 593,
	 .pclk = 82 * 1000 * 1000,
	 .mipi_bps = 576 * 1000 * 1000,
	 .fps_fixed = 60,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (593 - 4) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 1440 << 4,
	 .regs = sensor_1080p30_regs,
	 .regs_size = ARRAY_SIZE(sensor_1080p30_regs),
	 .set_size = NULL,
	 },
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_get_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_SUBLVDS;
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_SUBLVDS_6_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_SUBLVDS_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	struct sensor_exp_gain exp_gain;

	ret = sensor_spi_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	sensor_spi_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_spi_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	imx274_sensor_vts = wsize->vts;

	exp_gain.exp_val = 400*16;
	exp_gain.gain_val = 512;
	sensor_s_exp_gain(sd, &exp_gain);

	sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
		     wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable) {
		vin_gpio_set_status(sd, SM_VS, 0);
		vin_gpio_set_status(sd, SM_HS, 0);
		vin_set_sync_mclk(sd, 0, MCLK, OFF);
		return 0;
	} else {
		vin_set_sync_mclk(sd, 0, MCLK, ON);
		vin_gpio_set_status(sd, SM_VS, 3);
		vin_gpio_set_status(sd, SM_HS, 3);
	}
	return sensor_reg_init(info);
}

int imx274_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
{
	struct sensor_info *info = to_state(sd);
	int ret = 0;

	ret = sensor_set_fmt(sd, cfg, fmt);

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return ret;
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = imx274_set_fmt,
	.get_mbus_config = sensor_get_mbus_config,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static int sensor_registered(struct v4l2_subdev *sd)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	mutex_lock(&info->lock);

	sensor_power(sd, PWR_ON);
	ret = sensor_init(sd, 0);
	sensor_power(sd, PWR_OFF);

	mutex_unlock(&info->lock);

	return ret;
}

static const struct v4l2_subdev_internal_ops sensor_internal_ops = {
	.registered = sensor_registered,
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
			      65536 * 16, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_probe(struct spi_device *spi)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	v4l2_spi_subdev_init(sd, spi, &sensor_ops);

	snprintf(sd->name, sizeof(sd->name), "%s", SENSOR_NAME);
	sd->grp_id = VIN_GRP_ID_SENSOR;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &sensor_internal_ops;

	BUG_ON(info->magic_num && (info->magic_num != SENSOR_MAGIC_NUMBER));
	info->sensor_pads[SENSOR_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	media_entity_pads_init(&sd->entity, SENSOR_PAD_NUM, info->sensor_pads);

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);
#ifdef CONFIG_SAME_I2C
	info->sensor_i2c_addr = I2C_ADDR >> 1;
#endif
	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->combo_mode = CMB_TERMINAL_RES | LVDS_NORMAL_MODE;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

	return 0;
}
static int sensor_remove(struct spi_device *spi)
{
	struct v4l2_subdev *sd;

	sd = spi_get_drvdata(spi);
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct spi_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(spi, sensor_id);

static struct spi_driver sensor_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = SENSOR_NAME,
		   },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};
static __init int init_sensor(void)
{
	return spi_register_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	spi_unregister_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);
