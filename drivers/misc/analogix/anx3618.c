#define DEBUG
/*
 * ANX3618 driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include "slimport_tx_drv.h"
#include "sp_tx_reg.h"
#include "slimport.h"
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/mt_pm_ldo.h>

#include <linux/platform_data/anx3618.h>
#include <misc/dongle_hdmi.h>
#include <linux/of_platform.h>
#include <mach/mt_boot_common.h>
#include <mach/mt_gpio_def.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "asm-generic/irq.h"


/*#include <mach/devs.h> */
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>

#include "slimport.h"
/* #include <mach/mt_cam.h> */
/* #include "mt_clk.h" */
static int anx3618_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int anx3618_remove(struct i2c_client *client);
static void anx3618_shutdown(struct i2c_client *client);


#define USB_IDDIG 34
#define SLIMPORT_IRQ_GPIO 11

static int global_log_enable;
typedef enum {
	ANX_NONE = 0,
	ANX_USB = 1,
	ANX_SLIMPORT = 2,
	ANX_INVALID = 3
} anx_mode;

struct anx3618 {
	struct mutex lock;
	struct mutex sr_lock;
	struct i2c_client *client;
	struct device *dev;
	struct anx3618_platform_data *pdata;
	unsigned long flags;
	bool cable_connected, cableproc;
	anx_mode mode;
	int irq;
	bool power;
	int forced_mode;
	int usb_plug;		/* track usb_plug signal value */

	unsigned char slave_address;

	struct wake_lock slimport_lock;
	/* debug */
	int regnum;
	int regval;
	int gpionum;
	int gpioval;
	int ldonum;
	int ldoval;
	int usb_det;
	unsigned char suspend_mode;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static struct anx3618_platform_data anx3618_data = {
	.gpio_hdmi = 114,
	.gpio_vmch = 115,
	.gpio_usb_plug = 162,
	.gpio_pd = 158,
	.gpio_rst = 161,
	.gpio_cable_det = SLIMPORT_IRQ_GPIO,
	.gpio_intp = 88,
	.gpio_usb_det = 120,
	.gpio_sel_uart = 87,
	.gpio_1_0_v_on = 156,
	.vreg_3_3v = MT65XX_POWER_LDO_VMCH,
	.vreg_gpio_1_0v = MT65XX_POWER_LDO_VMC,
	.vreg_gpio_1_0v_config = VOL_1800,
};

static const struct i2c_device_id anx3618_id[] = {
	{"anx3618", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id anx3618_of_ids[] = {
	{.compatible = "slimport,anx3618",},
	{},
};

MODULE_DEVICE_TABLE(of, anx3618_of_ids);

static struct i2c_driver anx3618_driver = {
	.probe = anx3618_probe,
	.remove = anx3618_remove,
	.shutdown = anx3618_shutdown,
	.id_table = anx3618_id,
	.driver = {
		   .name = "anx3618",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(anx3618_of_ids),
		   },
};

#else

struct lab_i2c_board_info {
	struct i2c_board_info bi;
	int (*init) (struct lab_i2c_board_info *);
};

static struct i2c_driver anx3618_driver = {
	.driver = {
		   .name = "anx3618",
		   .owner = THIS_MODULE,
#if (defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND))
		   .pm = &anx3618_pm_ops,
#endif
		   },
	.probe = anx3618_probe,
	.remove = anx3618_remove,
	.shutdown = anx3618_shutdown,
	.id_table = anx3618_id,
};

MODULE_DEVICE_TABLE(i2c, anx3618_id);

#endif

#define ANX_RESET_TIME_MS 10

static void setup_cable_detection_irq(struct anx3618 *anx);
static int slimport_system_init(void);
int g_anx_in_powerdown_mode = 0;
static struct anx3618 *g_anx;
/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en = 1;
int g_power_off_delay = 100;
static unsigned char is_wake_locked;
static const anx_mode anx_state_table[2][2] = {
	{ANX_USB, ANX_USB},
	{ANX_SLIMPORT, ANX_INVALID}
};

static void _gpio_cfg_out(int gpio, int val)
{
/* gpio_request(gpio, "anx3618"); */
/* gpio_direction_output(gpio, val); */

	mt_set_gpio_dir(gpio, GPIO_DIR_OUT);
	mt_set_gpio_out(gpio, val);

}

static void _gpio_cfg_in(int gpio)
{
/* gpio_request(gpio, "anx3618"); */
/* gpio_direction_input(gpio); */

	mt_set_gpio_dir(gpio, GPIO_DIR_IN);
}

static anx_mode anx3618_get_state(struct anx3618 *anx)
{
	struct anx3618_platform_data *pd = anx->pdata;
	int myDp_det, usb_det;
	anx_mode ret = ANX_NONE;

	myDp_det = anx->cable_connected;
/* usb_det  = gpio_get_value(pd->gpio_usb_det); */
	usb_det = mt_get_gpio_in(pd->gpio_usb_det);


	ret = anx_state_table[myDp_det][usb_det];

	if (global_log_enable)
		dev_err(anx->dev,
			"%s: cable_det=%d, usb_det=%d; new_state=%d, curr=%d\n",
			__func__, myDp_det, usb_det, ret, anx->mode);

	anx->usb_det = usb_det;
	return ret;
}

static void anx3618_reset(struct anx3618 *anx)
{
	struct anx3618_platform_data *pd = anx->pdata;
/* gpio_set_value(pd->gpio_1_0_v_on, 0); */
/* gpio_set_value(pd->gpio_usb_plug, 0); */
/* gpio_set_value(pd->gpio_pd, 1); */

	mt_set_gpio_out(pd->gpio_1_0_v_on, 0);
	mt_set_gpio_out(pd->gpio_usb_plug, 0);
	mt_set_gpio_out(pd->gpio_pd, 1);

	msleep(ANX_RESET_TIME_MS);

/* gpio_set_value(pd->gpio_rst, 0); */
	mt_set_gpio_out(pd->gpio_rst, 0);


	anx->mode = ANX_NONE;

	dev_dbg(anx->dev, "%s:\n", __func__);
}

static void anx3618_usb_switch(struct anx3618 *anx)
{
	dev_dbg(anx->dev, "%s:\n", __func__);

	hardware_power_ctl(0);
	anx->mode = ANX_USB;
}

static void anx3618_slimport_switch(struct anx3618 *anx)
{
	dev_dbg(anx->dev, "%s:\n", __func__);

	hardware_power_ctl(1);
	anx->mode = ANX_SLIMPORT;
}

static void anx3618_recover(struct anx3618 *anx)
{
	static int anx_recover_cnt;

	if (anx_recover_cnt == 10) {
		struct anx3618_platform_data *pd = anx->pdata;

/* gpio_set_value(pd->gpio_1_0_v_on, 0); */
/* gpio_set_value(pd->gpio_usb_plug, 0); */
/* gpio_set_value(pd->gpio_pd, 1); */
/* gpio_set_value(pd->gpio_rst, 0); */
/* msleep(ANX_RESET_TIME_MS); */
/* gpio_set_value(pd->gpio_rst, 1); */

		mt_set_gpio_out(pd->gpio_1_0_v_on, 0);
		mt_set_gpio_out(pd->gpio_usb_plug, 0);
		mt_set_gpio_out(pd->gpio_pd, 1);
		mt_set_gpio_out(pd->gpio_rst, 0);
		msleep(ANX_RESET_TIME_MS);
		mt_set_gpio_out(pd->gpio_rst, 1);

		anx_recover_cnt = 0;
	} else {
		anx_recover_cnt++;
	}

	anx->mode = ANX_NONE;
	dev_dbg(anx->dev, "%s:\n", __func__);
}

#if 0
/* trigger 3618 cable detection mode */
static void anx3618_cable_detection_trigger(struct anx3618 *anx)
{
	struct anx3618_platform_data *pd = anx->pdata;

	gpio_set_value(pd->gpio_usb_plug, anx->usb_plug ? 0 : 1);
	usleep_range(500, 1000);
	gpio_set_value(pd->gpio_usb_plug, anx->usb_plug);
}
#endif

#ifndef CONFIG_OF
static struct i2c_board_info i2c_anx3618 __initdata = {
	I2C_BOARD_INFO("anx3618", 0x39),
	.irq = SLIMPORT_IRQ_GPIO,
	.platform_data = &anx3618_data,
};
#endif
static int anx_init(void)
{
	struct anx3618_platform_data *pdata = &anx3618_data;
#ifndef CONFIG_OF
	i2c_register_board_info(6, &i2c_anx3618, 1);
#else
	if (i2c_add_driver(&anx3618_driver) != 0) {
		pr_err("Error unable to add anx3618_driver i2c driver.\n");
		return -1;
	}

#endif

#if 0
	ariel_gpio_init(pdata->gpio_1_0_v_on, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_rst, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_hdmi, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_vmch, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_usb_plug, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_pd, GPIOF_DIR_OUT, MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_sel_uart, GPIOF_DIR_IN, MT_PIN_PULL_ENABLE_DOWN);
	ariel_gpio_init(pdata->gpio_cable_det, GPIOF_DIR_IN, MT_PIN_PULL_ENABLE_UP);
	ariel_gpio_init(pdata->gpio_intp, GPIOF_DIR_IN, MT_PIN_PULL_ENABLE_UP);
	ariel_gpio_init(pdata->gpio_usb_det, GPIOF_DIR_IN, MT_PIN_PULL_DISABLE);
	ariel_gpio_to_irq(info);
#endif

	mt_set_gpio_dir(pdata->gpio_1_0_v_on, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(pdata->gpio_1_0_v_on, GPIO_PULL_DISABLE);

	mt_set_gpio_dir(pdata->gpio_rst, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(pdata->gpio_rst, GPIO_PULL_DISABLE);

	mt_set_gpio_dir(pdata->gpio_hdmi, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(pdata->gpio_hdmi, GPIO_PULL_DISABLE);

	mt_set_gpio_dir(pdata->gpio_vmch, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(pdata->gpio_vmch, GPIO_PULL_ENABLE);

	mt_set_gpio_dir(pdata->gpio_pd, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(pdata->gpio_pd, GPIO_PULL_DISABLE);
	/* mt_set_gpio_pull_select(pdata->gpio_pd, GPIO_PULL_UP); */


	mt_set_gpio_dir(pdata->gpio_sel_uart, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(pdata->gpio_sel_uart, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(pdata->gpio_sel_uart, GPIO_PULL_DOWN);

	mt_set_gpio_dir(pdata->gpio_cable_det, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(pdata->gpio_cable_det, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(pdata->gpio_cable_det, GPIO_PULL_UP);

	mt_set_gpio_dir(pdata->gpio_intp, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(pdata->gpio_intp, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(pdata->gpio_intp, GPIO_PULL_UP);

	mt_set_gpio_dir(pdata->gpio_usb_det, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(pdata->gpio_usb_det, GPIO_PULL_DISABLE);

#if 0
/* FIXME: Set GPIO87 to MODE5 for CLKMGR */
	if (pdata->gpio_extclk == 87) {
		mt_pin_set_mode_by_name(pdata->gpio_extclk, "CLKM");
		mt_pin_set_load(pdata->gpio_extclk, 12, true);
	}
	mt_pin_set_mode_eint(pdata->gpio_intp);
#endif
	return 0;
}


/* set mode */
static void anx3618_set_mode(struct anx3618 *anx, anx_mode mode)
{
	dev_dbg(anx->dev, "%s:\n", __func__);
	switch (mode) {
		/* extern void change_edid_read_mode(unsigned char use_mem); */
	case ANX_NONE:
		anx3618_reset(anx);
		/* change_edid_read_mode(1); */
		break;
	case ANX_USB:
		anx3618_usb_switch(anx);
		break;
	case ANX_SLIMPORT:
		anx3618_slimport_switch(anx);
		break;
	case ANX_INVALID:
		anx3618_recover(anx);
		break;
	default:
		/* change_edid_read_mode(0); */
		break;
	}
}


static int anx3618_power_init(struct i2c_client *client, struct anx3618_platform_data *pd)
{
/* gpio_set_value(pd->gpio_1_0_v_on, 0); */
	mt_set_gpio_out(pd->gpio_1_0_v_on, 0);
	hwPowerOn(pd->vreg_3_3v, VOL_3300, "anx_vdd_3.3");
	hwPowerOn(pd->vreg_gpio_1_0v, pd->vreg_gpio_1_0v_config, "anx_vdd_1.0");
	msleep(ANX_RESET_TIME_MS);
	return 0;
}

static int anx3618_gpio_init(struct i2c_client *client, struct anx3618_platform_data *pd)
{
	_gpio_cfg_out(pd->gpio_1_0_v_on, 0);
	_gpio_cfg_out(pd->gpio_rst, 0);
	_gpio_cfg_out(pd->gpio_hdmi, 0);
	/* turn on pmic_vmch to provide power from pmic
	   and turn off LDO */
	_gpio_cfg_out(pd->gpio_vmch, 1);
	_gpio_cfg_out(pd->gpio_usb_plug, 0);
	_gpio_cfg_out(pd->gpio_pd, 1);

	_gpio_cfg_in(pd->gpio_sel_uart);	/* ext and int PD enabled */
	_gpio_cfg_in(pd->gpio_cable_det);
	_gpio_cfg_in(pd->gpio_usb_det);
	return 0;
}

/* debug */
#define TX_P0 0x70
#define TX_P1 0x7A
#define TX_P2 0x72

#define RX_P0 0x7e
#define RX_P1 0x80

int __i2c_read_byte(u8 dev, u8 offset, unsigned char *buf)
{
	struct i2c_msg msg[2];
	u8 c = offset;
	int ret;

	msg[0].addr = dev >> 1;
	msg[0].buf = &c;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[1].addr = dev >> 1;
	msg[1].buf = buf;
	msg[1].len = 1;
	msg[1].flags = I2C_M_RD;

	ret = i2c_transfer(g_anx->client->adapter, msg, 2);
	if (ret != 2)
		pr_err("%s: failed to read i2c addr=%x\n", __func__, dev);

	return ret;
}

int sp_read_reg(unsigned char slave_addr, unsigned char offset, unsigned char *buf)
{
	uint8_t ret = 0;

	ret = __i2c_read_byte(slave_addr, offset, buf);
	return ret;
}


int _sp_read_reg(struct anx3618 *anx, uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;
	ret = sp_read_reg(slave_addr, offset, buf);
	return ret;
}

int sp_write_reg(unsigned char slave_addr, unsigned char offset, unsigned char value)
{
	uint8_t ret = 0;
	struct i2c_msg msg[1];
	uint8_t buf[2] = { offset, value };

	msg[0].addr = slave_addr >> 1;
	msg[0].buf = buf;
	msg[0].len = 2;
	msg[0].flags = 0;

	ret = i2c_transfer(g_anx->client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s: failed to write i2c addr=%x\n", __func__, slave_addr);
	return ret;
}


int _sp_write_reg(struct anx3618 *anx, uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	return sp_write_reg(slave_addr, offset, value);
}


static int anx3618_write_byte(struct anx3618 *anx, u8 reg, u8 data)
{
	if (anx->slave_address)
		return _sp_write_reg(anx, anx->slave_address, reg, data);
	else
		return _sp_write_reg(anx, TX_P2, reg, data);
}


static int anx3618_read_byte(struct anx3618 *anx, u8 reg, u8 *data)
{
	if (anx->slave_address)
		return _sp_read_reg(anx, anx->slave_address, reg, data);
	else
		return _sp_read_reg(anx, TX_P2, reg, data);
}

static ssize_t anx3618_moder(struct device *dev, struct device_attribute *attr, char *buf)
{
	static char *mode[] = {
		"ANX_NONE",
		"ANX_USB",
		"ANX_SLIMPORT",
		"ANX_INVALID"
	};

	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "mode %02x: %s\n forced: %d\n",
			 (int)anx->mode, anx->mode > 3 ? mode[3] : mode[anx->mode],
			 anx->forced_mode);
}

static ssize_t anx3618_modew(struct device *dev,
			     struct device_attribute *attr, const char *buf, size_t count)
{
	int mode = -1;

	struct anx3618 *anx = dev_get_drvdata(dev);

	sscanf(buf, "%d", &mode);
	if (mode > ANX_INVALID)
		anx->forced_mode = -1;
	else if (mode < ANX_NONE)
		anx->forced_mode = -1;
	else
		anx->forced_mode = mode;

	anx3618_set_mode(anx, anx->forced_mode);

	return count;
}

static ssize_t anx3618_r7730r(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%02x: %02x\n", anx->regnum, anx->regval);
}

static ssize_t anx3618_r7730w(struct device *dev,
			      struct device_attribute *attr, const char *buf, size_t count)
{
	char c = -1;
	int val = 0, read;
	u8 val8 = 0;
	struct anx3618 *anx = dev_get_drvdata(dev);
	unsigned int sink_sel, offset;

	read = sscanf(buf, "%c %x %x %x", &c, &sink_sel, &offset, &val);
	anx->slave_address = sink_sel;

	if (c == -1 || offset == -1)
		return -EINVAL;
	if (c == 'r') {
		i2c_master_read_reg(sink_sel, offset, &val8);
		dev_dbg(anx->dev, "%s: 7730 read 0x%x\n", __func__, val8);
		anx->regnum = offset;
		anx->regval = (int)val8;
	} else if (c == 'w') {
		dev_dbg(anx->dev, "ANX:write: %x %x %x\n", sink_sel, offset, val);
		i2c_master_write_reg(sink_sel, offset, val);
		anx->regnum = offset;
		anx->regval = val;
		i2c_master_read_reg(sink_sel, offset, &val8);
		dev_dbg(anx->dev, "ANX:7730 readback: 0x%x\n", val8);
	}
	return count;
}

static ssize_t anx3618_pokemer(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%02x: %02x\n", anx->regnum, anx->regval);
}

static ssize_t anx3618_pokemew(struct device *dev,
			       struct device_attribute *attr, const char *buf, size_t count)
{
	char c = -1;
	int reg = -1, val = 0, read;
	u8 val8 = 0;
	int slave_address;
	struct anx3618 *anx = dev_get_drvdata(dev);
	read = sscanf(buf, "%c %x %x %x", &c, &slave_address, &reg, &val);
	anx->slave_address = slave_address;

	if (c == -1 || reg == -1)
		return -EINVAL;
	if (c == 'r') {
		anx3618_read_byte(anx, reg, &val8);
		dev_dbg(anx->dev, "%s:read 0x%x\n", __func__, val8);
		anx->regnum = reg;
		anx->regval = (int)val8;
	} else if (c == 'w') {
		dev_dbg(anx->dev, "ANX:write: %c %x %x\n", c, reg, val);
		anx3618_write_byte(anx, reg, val);
		anx->regnum = reg;
		anx->regval = val;
		anx3618_read_byte(anx, reg, &val8);
		dev_dbg(anx->dev, "ANX:readback: 0x%x\n", val8);
	}
	return count;
}

static ssize_t anx3618_switchr(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE,
			 "usb_det:%d cable_connected:%d mode:%d\n",
			 anx->usb_det, anx->cable_connected, anx->mode);
}

static ssize_t anx3618_switchw(struct device *dev,
			       struct device_attribute *attr, const char *buf, size_t count)
{
	int log_enable;
	struct anx3618 *anx = dev_get_drvdata(dev);
	sscanf(buf, "%d", &log_enable);

	if (log_enable == 1)
		global_log_enable = 1;
	else if (log_enable == 0)
		global_log_enable = 0;

	anx3618_get_state(anx);
	return count;
}

static ssize_t anx3618_ldor(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d %d\n", anx->ldonum, anx->ldoval);
}

static ssize_t anx3618_ldow(struct device *dev,
			    struct device_attribute *attr, const char *buf, size_t count)
{
	int ldo = -1;
	int val = -1;

	struct anx3618 *anx = dev_get_drvdata(dev);
	struct anx3618_platform_data *pd = anx->pdata;

	sscanf(buf, "%d %d", &ldo, &val);
	if (ldo == -1)
		return -EINVAL;
	else if (val == -1)
		return -EINVAL;

	if (ldo)
		ldo = pd->vreg_gpio_1_0v;
	else
		ldo = pd->vreg_3_3v;

	if (val) {
		if (ldo == pd->vreg_gpio_1_0v)
			hwPowerOn(pd->vreg_gpio_1_0v, pd->vreg_gpio_1_0v_config, "anx_vdd_1.0");
		else
			hwPowerOn(pd->vreg_3_3v, VOL_3300, "anx_vdd_3.3");
	} else {
		if (ldo == pd->vreg_gpio_1_0v)
			hwPowerDown(pd->vreg_gpio_1_0v, "anx_vdd_1.0");
		else
			hwPowerDown(pd->vreg_3_3v, "anx_vdd_3.3");
	}

	anx->ldonum = ldo;
	anx->ldoval = val;

	return count;
}

static ssize_t anx3618_dpcdr(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%02x: %02x\n", anx->regnum, anx->regval);
}

static ssize_t anx3618_dpcdw(struct device *dev,
			     struct device_attribute *attr, const char *buf, size_t count)
{
	char c = -1;
	int reg = -1, read;
	unchar val8 = 0, val;
	unchar slave_address, addrh = 0, buf_count = 1, begin = 0;
	unchar r_buf[16];
	struct anx3618 *anx = dev_get_drvdata(dev);
	read =
	    sscanf(buf, "%c %x %x %x %x %c", &c, (int *)&addrh, (int *)&slave_address, (int *)&reg,
		   (int *)&buf_count, &val);
	anx->slave_address = slave_address;
	dev_dbg(anx->dev, "%s:%c addrh=%x,slave_address=%x,reg=%x, buf_count=%x\n",
		__func__, c, addrh, slave_address, reg, buf_count);
	if (c == -1 || reg == -1)
		return -EINVAL;
	if (c == 'r') {
		sp_tx_aux_dpcdread_bytes(addrh, slave_address, reg, buf_count, r_buf);
		for (begin = 0; begin < buf_count; begin++)
			dev_dbg(anx->dev, "%s:read 0x%x\n", __func__, (int)r_buf[begin]);
		anx->regnum = reg;
		anx->regval = (int)r_buf[0];
	} else if (c == 'w') {
		sp_tx_aux_dpcdwrite_bytes(addrh, slave_address, reg, buf_count, &val);
		dev_dbg(anx->dev, "ANX:write: %c %x %x\n", c, reg, (int)val);
		anx->regnum = reg;
		anx->regval = (int)val;
		sp_tx_aux_dpcdread_bytes(addrh, slave_address, reg, buf_count, &val8);
		dev_dbg(anx->dev, "ANX:readback: 0x%x\n", (int)val8);
	}
	return count;
}

/* dump firmware */
int read_eeprom_byte(struct anx3618 *anx, u8 addr_hi, u8 addr_lo, u8 *data)
{
	_sp_write_reg(anx, TX_P1, 0x8e, (addr_hi << 3));
	_sp_write_reg(anx, TX_P1, 0x8f, addr_lo);
	/*read data */
	_sp_write_reg(anx, TX_P1, 0x8d, 0x02);	/*read enable */
	return _sp_read_reg(anx, TX_P1, 0x90, data);	/*read */
}

ssize_t read_eeprom_bytes(struct anx3618 *anx, char *buf, int size)
{
	int i, offset = 0;
	uint8_t addr_h, addr_l, data;

	/* prepare */
	_sp_write_reg(anx, TX_P2, 0x05, 0x00);
	_sp_write_reg(anx, TX_P1, 0x96, 0x80);

	_sp_write_reg(anx, TX_P1, 0x8a, 0x28);	/*pattern 1 */
	_sp_write_reg(anx, TX_P1, 0x8b, 0x5C);	/*pattern 2 */
	_sp_write_reg(anx, TX_P1, 0x8c, 0x4E);	/*pattern 3 */

	/* dump 2K */
	for (i = 0; i < 2048; i++) {
		addr_h = (uint8_t) ((i >> 8));
		addr_l = (uint8_t) (i & 0xff);
		read_eeprom_byte(anx, addr_h, addr_l, &data);
		offset += scnprintf(buf + offset, size - offset, "%c", data);
	}
	return offset;
}

u8 write_eeprom_byte(struct anx3618 *anx, u8 addr_hi, u8 addr_lo, u8 wbyte)
{
	u8 rbyte = 0;
	_sp_write_reg(anx, TX_P1, 0x8e, (addr_hi << 3));
	_sp_write_reg(anx, TX_P1, 0x8f, addr_lo);
	_sp_write_reg(anx, TX_P1, 0x91, wbyte);	/* data */
	_sp_write_reg(anx, TX_P1, 0x8d, 0x04);	/* write enable */
	msleep(10);
	/* read data */
	_sp_write_reg(anx, TX_P1, 0x8d, 0x02);	/* read enable */
	_sp_read_reg(anx, TX_P1, 0x90, &rbyte);
	if (wbyte != rbyte) {
		dev_dbg(anx->dev, "error:%x %x, write %x, read %x  ",
			(uint) addr_hi, (uint) addr_lo, (uint) wbyte, (uint) rbyte);
		return 0;
	}
	return 1;
}

void write_eeprom(struct anx3618 *anx, const char *data, int count)
{
	int i = 0;
	u8 addr_hi, addr_lo;

	if (count > 2048)
		count = 2048;
	/* prepare */
	_sp_write_reg(anx, TX_P2, 0x05, 0x00);
	_sp_write_reg(anx, TX_P1, 0x96, 0x80);
	_sp_write_reg(anx, TX_P1, 0x8a, 0x28);	/* pattern 1 */
	_sp_write_reg(anx, TX_P1, 0x8b, 0x5C);	/* pattern 2 */
	_sp_write_reg(anx, TX_P1, 0x8c, 0x4E);	/* pattern 3 */

	/* write fw */
	for (i = 0; i < count; i++) {
		addr_hi = (unchar) (i >> 8);
		addr_lo = (unchar) (i & 0xff);
		if (write_eeprom_byte(anx, addr_hi, addr_lo, data[i]) == 0)
			break;
	}
	/* write padding 0 */
	for (; i < 2048; i++) {
		addr_hi = (unchar) (i >> 8);
		addr_lo = (unchar) (i & 0xff);
		if (write_eeprom_byte(anx, addr_hi, addr_lo, 0x0) == 0)
			break;
	}
	dev_dbg(anx->dev, "update finished\n");
}

static ssize_t anx3618_fwr(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	return read_eeprom_bytes(anx, buf, PAGE_SIZE);
}

/* program firmware */
static ssize_t anx3618_fww(struct device *dev,
			   struct device_attribute *attr, const char *buf, size_t count)
{
	struct anx3618 *anx = dev_get_drvdata(dev);

	/* clear */
	dev_dbg(anx->dev, "clear eeprom\n");
	write_eeprom(anx, buf, 0);
	/* write data */
	dev_dbg(anx->dev, "write eeprom\n");
	write_eeprom(anx, buf, count);

	return count;
}

static ssize_t anx3618_ctrl_fwr(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct anx3618 *anx = dev_get_drvdata(dev);
	/* fix warning */
	anx = anx;
	return scnprintf(buf, PAGE_SIZE, "n/a\n");
}

/* prepare firmware session */
static ssize_t anx3618_ctrl_fww(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int val = -1;
	struct anx3618 *anx = dev_get_drvdata(dev);
	struct anx3618_platform_data *pd = anx->pdata;

	sscanf(buf, "%d", &val);
	if (val == -1)
		return -EINVAL;

	val = !!val;
	if (val)
		anx->forced_mode = ANX_SLIMPORT;
	else
		anx->forced_mode = -1;

	/* prepare session */
	anx3618_set_mode(anx, anx->forced_mode);

	/* force chip reset */
	if (val) {
		/* gpio_set_value(pd->gpio_rst, 0); */
		/* msleep(10); */
		/* gpio_set_value(pd->gpio_pd, 1); */
		/* msleep(10); */
		/* gpio_set_value(pd->gpio_pd, 0); */
		/* msleep(50); */
		/* gpio_set_value(pd->gpio_rst, 1); */
		/* msleep(10); */

		mt_set_gpio_out(pd->gpio_rst, 0);
		msleep(10);
		mt_set_gpio_out(pd->gpio_pd, 1);
		msleep(10);
		mt_set_gpio_out(pd->gpio_pd, 0);
		msleep(50);
		mt_set_gpio_out(pd->gpio_rst, 1);
		msleep(10);
	}

	return count;
}

static ssize_t anx3618_notify_hdmi_w(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int c = 0;
	int read;

	read = sscanf(buf, "%d", &c);
	c = !!c;
	if (c == 1)
		notify_power_status_change(DONGLE_HDMI_POWER_ON);
	else
		notify_power_status_change(DONGLE_HDMI_POWER_OFF);
	return count;
}


static DEVICE_ATTR(pokeme, 0666, anx3618_pokemer, anx3618_pokemew);
static DEVICE_ATTR(r7730, 0666, anx3618_r7730r, anx3618_r7730w);
static DEVICE_ATTR(mode, 0666, anx3618_moder, anx3618_modew);
static DEVICE_ATTR(ct_ldo, 0666, anx3618_ldor, anx3618_ldow);
static DEVICE_ATTR(ct_fw, 0666, anx3618_ctrl_fwr, anx3618_ctrl_fww);
static DEVICE_ATTR(fw, 0666, anx3618_fwr, anx3618_fww);
static DEVICE_ATTR(ct_switch, 0666, anx3618_switchr, anx3618_switchw);
static DEVICE_ATTR(notify_hdmi, 0222, NULL, anx3618_notify_hdmi_w);
static DEVICE_ATTR(dpcd, 0666, anx3618_dpcdr, anx3618_dpcdw);

static struct attribute *anx3618_attributes[] = {
	&dev_attr_pokeme.attr,
	&dev_attr_r7730.attr,
	&dev_attr_mode.attr,
	&dev_attr_ct_ldo.attr,
	&dev_attr_ct_fw.attr,
	&dev_attr_fw.attr,
	&dev_attr_ct_switch.attr,
	&dev_attr_notify_hdmi.attr,
	&dev_attr_dpcd.attr,
	NULL,
};

static const struct attribute_group anx3618_attr_group = {
	.attrs = anx3618_attributes,
};

void Acquire_wakelock(void)
{
	struct anx3618 *anx = g_anx;
	dev_dbg(anx->dev, "mydp:%s", __func__);
	mutex_lock(&anx->sr_lock);
	if (is_wake_locked) {
		mutex_unlock(&anx->sr_lock);
		return;
	}
	wake_lock(&anx->slimport_lock);
	is_wake_locked = 1;
	mutex_unlock(&anx->sr_lock);
}
EXPORT_SYMBOL(Acquire_wakelock);

void Release_wakelock(void)
{
	struct anx3618 *anx = g_anx;
	dev_dbg(anx->dev, "mydp:%s", __func__);
	mutex_lock(&anx->sr_lock);
	if (is_wake_locked == 0) {
		mutex_unlock(&anx->sr_lock);
		return;
	}
	wake_unlock(&anx->slimport_lock);
	is_wake_locked = 0;
	mutex_unlock(&anx->sr_lock);
}
EXPORT_SYMBOL(Release_wakelock);

int get_slimport_hdcp_status(void)
{
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
/* return check_sink_mode(); */
	;
#else
	return 1;		/* hdcp_en always on */
#endif
	return 0;
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, edid_blocks, 128 * sizeof(char));
	} else if (block == 1) {
		memcpy(edid_buf, (edid_blocks + 128), 128 * sizeof(char));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

unchar sp_get_link_bw(void)
{
	unchar value;
	sp_tx_get_link_bw(&value);
	return value;
}
EXPORT_SYMBOL(sp_get_link_bw);

void sp_set_link_bw(unchar link_bw)
{
	sp_tx_set_link_bw(link_bw);
}
EXPORT_SYMBOL(sp_set_link_bw);
unsigned char sp_get_ds_cable_type(void)
{
	return sp_tx_rx_type;
}
EXPORT_SYMBOL(sp_get_ds_cable_type);

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
enum CHARGING_STATUS sp_get_ds_charge_type(void)
{
	/*
	   0x02: fast charge
	   0x01: slow charge
	   0x00: no charge
	 */
	return downstream_charging_status;
}
EXPORT_SYMBOL(sp_get_ds_charge_type);
#endif

bool slimport_goes_powerdown_mode(void)
{
	return g_anx_in_powerdown_mode;
}
EXPORT_SYMBOL(slimport_goes_powerdown_mode);

bool slimport_is_cable_connected(void)
{
	struct anx3618 *anx = g_anx;
	return anx->cable_connected;
}
EXPORT_SYMBOL(slimport_is_cable_connected);


bool get_cableproc_status(void)
{
	struct anx3618 *anx = g_anx;
	return anx->cableproc;
}


/*
 * a kthread that detects CONNECTION of the cable. It does a regular scan
 * every 500mSec or uppon triggering connection interrupt.
 * Is used only when the system is in the STATE_WAITTING_CABLE_PLUG
*/
static wait_queue_head_t cable_insert_wq;
static struct task_struct *slimport_main_task;
static atomic_t cable_insert_event = ATOMIC_INIT(0);

static int slimport_main_kthread(void *data)
{
	int workqueu_timer;
	int val_gpio_cab_det;
	struct anx3618 *anx = g_anx;
	while (!kthread_should_stop()) {
		if (sp_tx_system_state >= STATE_AUDIO_OUTPUT)
			/* Playing valid video - every half second scan the validitiy of the channel */
			workqueu_timer = 500;
		else if (sp_tx_system_state > STATE_WAITTING_CABLE_PLUG)
			/* The shortest sleep - when progressing between internal stages */
			workqueu_timer = 100;
		else
			/* just in case */
			workqueu_timer = 1000;

		wait_event_interruptible_timeout(cable_insert_wq,
						 atomic_read(&cable_insert_event),
						 msecs_to_jiffies(workqueu_timer));
		atomic_set(&cable_insert_event, 0);

/* val_gpio_cab_det = gpio_get_value(anx->pdata->gpio_cable_det); */
		val_gpio_cab_det = mt_get_gpio_in(anx->pdata->gpio_cable_det);
		if (g_anx_in_powerdown_mode && sp_tx_system_state <= STATE_SINK_CONNECTION)
			anx->cable_connected = 0;

		else if (val_gpio_cab_det) {
			if (sp_tx_system_state <= STATE_WAITTING_CABLE_PLUG
			    && g_anx_in_powerdown_mode == 0) {
				/* The system has to be powered */
				msleep(100);
				/* assure the cable got stuck firmly (and if there was any debounce,
				 * it has been gone already) */
				anx->cable_connected = 1;
			}
		} else {
			/* For the 'cable_connected' to be reset to 0 when the cable is actually unplugged we are */
			/* going to wait for EITHER: */
			/* 1. A valid connection to be established (STATE_PLAY_BACK) */
			/* or */
			/* 2. A connection failure (which in turn sets the state back to STATE_WAITTING_CABLE_PLUG) */
			/*if (sp_tx_system_state == STATE_PLAY_BACK  ||
			   sp_tx_system_state <= STATE_WAITTING_CABLE_PLUG) */
			anx->cable_connected = 0;
			if (sp_tx_system_state > STATE_WAITTING_CABLE_PLUG)
				sp_tx_set_sys_state(STATE_INIT);
		}

		dev_dbg(anx->dev,
			"%s: gpio_cab_det=%d, anx->cable_connected=%d, state=%d, pd=%d, is_wake_locked=%d\n",
			__func__, val_gpio_cab_det, anx->cable_connected, sp_tx_system_state,
			g_anx_in_powerdown_mode, is_wake_locked);

		slimport_main_process();
	}
	return 0;
}



static irqreturn_t cable_detect_isr(int irq, void *dev)
{
	/* struct anx3618 *anx = dev_id; */
	atomic_set(&cable_insert_event, 1);
	wake_up_interruptible(&cable_insert_wq);
	/* dev_dbg(anx->dev, "%s: IRQ entered\n", __func__); */


	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);
	return IRQ_HANDLED;
}

void setup_cable_detection_irq(struct anx3618 *anx)
{
	struct anx3618_platform_data *pdata = anx->pdata;
	struct device_node *node = NULL;
	int anx3618_irq;
	int err;

	dev_dbg(anx->dev, "Configure cable detect interrupt\n");
	pr_err("Configure cable detect interrupt\n");
#if 0
	err = anx->irq = gpio_to_irq(pdata->gpio_cable_det);
	if (err < 0) {
		dev_dbg(anx->dev, "Unable to get irq number for GPIO %d\n", pdata->gpio_cable_det);
		goto err1;
	}
#endif
	init_waitqueue_head(&cable_insert_wq);
	if (!slimport_main_task) {
		slimport_main_task =
		    kthread_create(slimport_main_kthread, NULL, "slimport_main_kthread");
		wake_up_process(slimport_main_task);
	}
#ifdef CONFIG_OF
	/*Get Anx3618 IRQ GPIO Pin Num */



	node = of_find_compatible_node(NULL, NULL, "slimport,anx3618");
	if (node) {
		err = of_property_read_u32(node, "int_irq", &anx3618_irq);
		if (!err) {
			pr_err("anx3618 irq = %d\n", anx3618_irq);
			anx->irq = anx3618_irq;
		}
#if 0
		if (anx->irq > 0) {
			mt_set_gpio_mode(anx3618_data.gpio_cable_det, GPIO_MODE_GPIO);
			mt_set_gpio_dir(anx3618_data.gpio_cable_det, GPIO_DIR_IN);
			mt_set_gpio_pull_select(anx3618_data.gpio_cable_det, GPIO_PULL_UP);
			mt_set_gpio_pull_enable(anx3618_data.gpio_cable_det, GPIO_PULL_ENABLE);
		}
#endif
		err = request_threaded_irq(anx->irq, NULL,
		cable_detect_isr, IRQF_ONESHOT | IRQF_TRIGGER_RISING, "anx-cable-detect", anx);
		if (err < 0) {
			dev_dbg(anx->dev, "failed to request irq; err=%d\n", err);
			goto err1;
		}
	}
#else
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM, EINTF_TRIGGER_RISING, cable_detect_isr, 0);
	mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);

#endif
#endif
#if 0
	err = request_threaded_irq(anx->irq, NULL, cable_detect_isr,
		IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				   "anx-cable-detect", anx);
	if (err < 0) {
		dev_dbg(anx->dev, "failed to request irq; err=%d\n", err);
		goto err1;
	}
#endif

	return;
err1:
	gpio_free(pdata->gpio_cable_det);
}

int slimport_system_init(void)
{
	msleep(50);
	/* if (!slimport_chip_detect()) { */
	hardware_power_ctl(0);
	pr_debug("mydp: Chip detect error\n");
	pr_err("mydp: neo_Chip detect error\n");
	/* return -ENODEV; */
	/*  */
	/* } */
	/* system_power_ctrl_0(); */
	return 0;
}

#ifdef CONFIG_PM
static int anx3618_suspend(struct device *dev)
{
	struct anx3618 *anx = dev_get_drvdata(dev);

	dev_dbg(anx->dev, "%s\n", __func__);
	/* On entering into power suspend mode - treat it as if cable released */
	g_anx_in_powerdown_mode = 1;

	return 0;
}

static int anx3618_resume(struct device *dev)
{
	struct anx3618 *anx = dev_get_drvdata(dev);

	dev_dbg(anx->dev, "%s\n", __func__);
	g_anx_in_powerdown_mode = 0;

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void anx3618_early_suspend(struct early_suspend *es)
{
	struct anx3618 *anx = container_of(es, struct anx3618, early_suspend);

	dev_dbg(anx->dev, "%s\n", __func__);

	anx3618_suspend(anx->dev);
}

static void anx3618_late_resume(struct early_suspend *es)
{
	struct anx3618 *anx = container_of(es, struct anx3618, early_suspend);

	dev_dbg(anx->dev, "%s\n", __func__);

	anx3618_resume(anx->dev);
}

#else
static const struct dev_pm_ops anx3618_pm_ops = {
	.suspend = anx3618_suspend,
	.resume = anx3618_resume,
};
#endif				/* CONFIG_HAS_EARLYSUSPEND */
#endif				/* CONFIG_PM */


extern BOOTMODE g_boot_mode;

static int anx3618_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct anx3618 *anx;
	/* struct anx3618_platform_data *pdata = client->dev.platform_data; */
	struct anx3618_platform_data *pdata = &anx3618_data;
	int error = 0;


	if (!pdata) {
		dev_err(&client->dev, "mydp: no platform data supplied\n");
		return -EINVAL;
	}

	error = anx3618_gpio_init(client, pdata);
	if (error) {
		dev_err(&client->dev, "mydp: Failed to request ANX3618 gpios"
			" (error %d)\n", error);
		return error;
	}

	error = anx3618_power_init(client, pdata);
	if (error) {
		dev_err(&client->dev, "mydp: Failed to power on ANX3618 controller "
			"(error %d)\n", error);
		return error;
	}

	anx = kzalloc(sizeof(struct anx3618), GFP_KERNEL);
	if (!anx) {
		dev_err(&client->dev, "mydp: Failed to allocate memory\n");
		return -ENOMEM;
	}

	mutex_init(&anx->lock);
	mutex_init(&anx->sr_lock);
	anx->client = client;
	/* kevin added to accommodate the slimport upstream code. */

	anx->dev = &client->dev;
	anx->pdata = pdata;
	anx->forced_mode = -1;

	g_anx = anx;

	kobject_uevent(&client->dev.kobj, KOBJ_ADD);
	i2c_set_clientdata(client, anx);

	error = sysfs_create_group(&anx->dev->kobj, &anx3618_attr_group);
	if (error) {
		dev_err(anx->dev, "mydp: failed to register sysfs. err: %d\n", error);
	}

	wake_lock_init(&anx->slimport_lock, WAKE_LOCK_SUSPEND, "slimport_wake_lock");

#ifdef CONFIG_HAS_EARLYSUSPEND
	anx->early_suspend.suspend = anx3618_early_suspend;
	anx->early_suspend.resume = anx3618_late_resume;
	register_early_suspend(&anx->early_suspend);
#endif

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	/* disable slimport driver if boot mode is power off charging or low power charging mode */
	if (g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    g_boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
		return 0;
#endif

	setup_cable_detection_irq(anx);

	slimport_system_init();

	return 0;
}

static int anx3618_remove(struct i2c_client *client)
{
	struct anx3618 *anx = i2c_get_clientdata(client);

	disable_irq(anx->irq);
	if (!slimport_main_task)
		kthread_stop(slimport_main_task);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&anx->early_suspend);
#endif
	free_irq(anx->irq, anx);

	return 0;
}

static void anx3618_shutdown(struct i2c_client *client)
{
	dev_info(&client->dev, "anx3618 shutdown\n");
	anx3618_remove(client);
}


#if 0
static void ariel_gpio_init(int gpio, u32 flags, int pull_mode)
{
	if (gpio > 0) {
		/* int err; */
		mt_set_gpio_mode(gpio, GPIO_MODE_00);
		mt_set_gpio_dir(gpio, flags);
		mt_set_gpio_pull_enable(gpio, pull_mode);
		/* mt_pin_set_mode_gpio(gpio); */
		/* mt_pin_set_pull(gpio, pull_mode); */
		/* err = gpio_request_one(gpio, flags, "ariel"); */
		/* pr_err("GPIO%d: board init done; err=%d\n", gpio, err); */
		/* gpio_free(gpio); */
	}
}
static int ariel_gpio_to_irq(struct lab_i2c_board_info *info)
{
	/* int err; */
	/* u16 gpio_irq = info->bi.irq; */
	int err = mt_pin_set_mode_eint(gpio_irq);
	if (!err)
		err = gpio_request(gpio_irq, info->bi.type);
	if (!err)
		err = gpio_direction_input(gpio_irq);
	if (!err)
		info->bi.irq = gpio_to_irq(gpio_irq);
	if (!err && info->bi.irq < 0)
		err = info->bi.irq;
	if (err)
		info->bi.irq = -1;
	return err;
	/* u16 gpio_irq = info->bi.irq; */
	/*err = mt_set_gpio_mode(GPIO_HDMI_EINT_PIN, GPIO_MODE_00);*/
	/* if (!err) */
		/* err = gpio_request(gpio_irq, info->bi.type); */
	if (!err)
		/* err = gpio_direction_input(gpio_irq); */
		err = mt_set_gpio_dir(GPIO_HDMI_EINT_PIN, GPIO_DIR_IN);
	/* if (!err) */
		/* info->bi.irq = gpio_to_irq(gpio_irq); */
	/* if (!err && info->bi.irq < 0) */
		/* err = info->bi.irq; */
	/* if (err) */
		/* info->bi.irq = -1; */
	return err;
	return 0;
}
#endif


/* Slimport IC power on/off sequence...*/
int hardware_power_ctl(int stat)
{
	struct anx3618 *anx = g_anx;
	struct anx3618_platform_data *pd = anx->pdata;
	stat = !!stat;
	if (stat != anx->power) {
		if (stat) {
			if (pd->anx3618_clk_enable) {
				pd->anx3618_clk_enable(true);
				slimport_set_xtal_freq(XTAL_24M);
				mdelay(20);
			} else
				slimport_set_xtal_freq(XTAL_24M);

/* gpio_set_value(pd->gpio_rst, 0); */
			mt_set_gpio_out(pd->gpio_rst, 0);
			mdelay(1);

			pr_debug("hardware_power_ctl\n");
/* gpio_set_value(pd->gpio_pd, 0); */
			mt_set_gpio_out(pd->gpio_pd, 0);
			mdelay(1);

/* gpio_set_value(pd->gpio_1_0_v_on, 1); */
			mt_set_gpio_out(pd->gpio_1_0_v_on, 1);
			mdelay(20);

/* gpio_set_value(pd->gpio_rst, 1); */
/* gpio_set_value(pd->gpio_hdmi, 1); */

			mt_set_gpio_out(pd->gpio_rst, 1);
			mt_set_gpio_out(pd->gpio_hdmi, 1);
			dev_dbg(anx->dev, "Chip is power on\n");
			pr_err("mydp: chip is power on\n");
			anx->power = true;
		} else {
/* gpio_set_value(pd->gpio_hdmi, 0); */
/* gpio_set_value(pd->gpio_rst, 0); */
/* gpio_set_value(pd->gpio_1_0_v_on, 0); */
/* gpio_set_value(pd->gpio_pd, 1); */

			mt_set_gpio_out(pd->gpio_hdmi, 0);
			mt_set_gpio_out(pd->gpio_rst, 0);
			mt_set_gpio_out(pd->gpio_1_0_v_on, 0);
			mt_set_gpio_out(pd->gpio_pd, 1);

			dev_dbg(anx->dev, "Chip is power down\n");
			pr_err("mydp: chip is power off\n");
			anx->power = false;
			if (pd->anx3618_clk_enable)
				pd->anx3618_clk_enable(false);

			msleep(100);
			/* wake_unlock(&anx->slimport_lock); */
			/* Release_wakelock(); */
		}
	}
	return 1;
}
module_init(anx_init);

/* module_i2c_driver(anx3618_driver); */

/* Module information */
MODULE_DESCRIPTION("ANX3618 driver");
MODULE_LICENSE("GPL");
