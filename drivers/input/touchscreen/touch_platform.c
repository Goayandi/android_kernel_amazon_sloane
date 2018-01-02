/*
 * touch_platform.c
 *
 * Copyright 2015 Amazon Technology, Inc. All Rights Reserved.
 *
 */

#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <uapi/linux/input.h>

/* cyttsp */
#include <uapi/linux/input.h>
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_i2c.h>
#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_mt.h>

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
#include <linux/input/synaptics_dsx.h>
#define TM_TYPE1        (1)     /* 2D only */
#define TM_TYPE2        (2)     /* 2D + 0D x 2 */
#define TM_TYPE3        (3)     /* 2D + 0D x 4 */
#endif

#define GPIO_CTP_RST_PIN	166
#define TOUCH_IRQ_GPIO		302
#define BOARD_ID_ASTON_PANEL    0xffff
#define BOARD_ID_ARIEL_PANEL	0xaaaa

#ifdef CONFIG_IDME
unsigned int idme_get_board_type(void);
#endif

#define USE_REGULATOR_FRAMEWORK

#if defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4) || defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
struct lab_i2c_board_info {
	struct i2c_board_info bi;
};

static int touch_power_enable(struct regulator *core_reg,
		struct regulator *io_reg, u16 rst_gpio, bool enable)
{
	int ret;

	gpio_direction_output(rst_gpio, 0);
	if (enable) {
#ifdef USE_REGULATOR_FRAMEWORK
		ret = regulator_enable(core_reg);
		if (ret != 0) {
			pr_err("%s: Failed to enable vgp6 voltage! ret = %d\n",
					__func__, ret);
			return ret;
		}

		ret = regulator_enable(io_reg);
		if (ret != 0) {
			pr_err("%s: Failed to enable vgp4 voltage! ret = %d\n",
					__func__, ret);
			return ret;
		}
#endif
		msleep(20);

		gpio_set_value(rst_gpio, 1);
	} else {
#ifdef USE_REGULATOR_FRAMEWORK
		ret = regulator_disable(core_reg);
		if (ret != 0) {
			pr_err("%s: Failed to disable vgp6 voltage!\n",
					__func__);
		}

		ret = regulator_disable(io_reg);
		if (ret != 0) {
			pr_err("%s: Failed to disable vgp4 voltage!\n",
					__func__);
		}
#endif
		gpio_direction_input(rst_gpio);
	}

	return 0;
}

static int touch_power_init(struct regulator **p_core_reg,
		struct regulator **p_io_reg, u16 rst_gpio, bool enable,
		struct device *dev)
{
	int ret = 0;
	gpio_request(rst_gpio, "touch-rst");
#ifdef USE_REGULATOR_FRAMEWORK
	*p_core_reg = devm_regulator_get(dev, "vgp6");
	*p_io_reg = devm_regulator_get(dev, "vgp4");

	ret = regulator_set_voltage(*p_core_reg, 3300000, 3300000);
	if (ret != 0) {
		pr_err("%s: Failed to set vgp6 voltage! ret = %d\n",
				__func__, ret);
		return ret;
	}

	ret = regulator_set_voltage(*p_io_reg, 1800000, 1800000);
	if (ret != 0) {
		pr_err("%s: Failed to set vgp4 voltage! ret = %d\n",
				__func__, ret);
		return ret;
	}

#endif

	return ret;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4
static int cyttsp4_xres_ariel(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
	touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->rst_gpio, false);
	msleep(100);
	touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->rst_gpio, true);

	return 0;
}

static int cyttsp4_power_ariel(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
#ifdef USE_REGULATOR_FRAMEWORK
	return touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->rst_gpio, on);
#else
	return 0;
#endif
}

/* driver-specific init (power on) */
static int cyttsp4_init_ariel(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
#ifdef USE_REGULATOR_FRAMEWORK
	struct regulator **p_core_reg = &pdata->core_reg;
	struct regulator **p_io_reg = &pdata->io_reg;
	int ret;

	ret = touch_power_init(p_core_reg, p_io_reg,
			pdata->rst_gpio, on, dev);
	if (ret != 0) {
		pr_err("%s: touch power init failed! ret = %d\n",
			__func__, ret);

		return ret;
	}

	return touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->rst_gpio, on);
#else
	return 0;
#endif
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
#include <linux/cyttsp4_img.h>
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp4_params.h"
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *) & cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *) & cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

static struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.ttconfig = &cyttsp4_ttconfig,
	.flags = CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION,
};

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	.irq_gpio = TOUCH_IRQ_GPIO,
	.rst_gpio = GPIO_CTP_RST_PIN,
	.xres = cyttsp4_xres_ariel,
	.init = cyttsp4_init_ariel,
	.power = cyttsp4_power_ariel,
	.detect = NULL,
	.flags = CY_CORE_FLAG_NONE,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
	.loader_pdata = &_cyttsp4_loader_platform_data,
};

static struct cyttsp4_core_info cyttsp4_core_info = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.platform_data = &_cyttsp4_core_platform_data,
};

#define CY_MAXX 800
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *) &cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = CY_MT_FLAG_NO_TOUCH_ON_LO,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

struct cyttsp4_device_info cyttsp4_mt_device_info = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_platform_data,
};
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4 */

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
static int synaptics_power_init(struct synaptics_rmi4_platform_data *pdata,
		bool enable, struct device *dev)
{
#ifdef USE_REGULATOR_FRAMEWORK
	struct regulator **p_core_reg = &pdata->core_reg;
	struct regulator **p_io_reg = &pdata->io_reg;
	int ret;

	ret = touch_power_init(p_core_reg, p_io_reg,
			pdata->reset_gpio, enable, dev);
	if (ret != 0) {
		pr_err("%s: touch power init failed! ret = %d\n",
			__func__, ret);

		return ret;
	}

	return touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->reset_gpio, enable);
#else
	return 0;
#endif
}

static int synaptics_power_control(const struct synaptics_rmi4_platform_data
					*pdata,	bool enable)
{
#ifdef USE_REGULATOR_FRAMEWORK
	return touch_power_enable(pdata->core_reg, pdata->io_reg,
			pdata->reset_gpio, enable);
#else
	return 0;
#endif
}

static unsigned char TM_TYPE1_f1a_button_codes[] = {};

static struct synaptics_rmi_f1a_button_map TM_TYPE1_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(TM_TYPE1_f1a_button_codes),
	.map = TM_TYPE1_f1a_button_codes,
};

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_type = IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.reset_gpio = GPIO_CTP_RST_PIN,
	.gpio = TOUCH_IRQ_GPIO,
	.f1a_button_map = &TM_TYPE1_f1a_button_map,
	.init_power = synaptics_power_init,
	.power = synaptics_power_control,
};
#endif /* CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4 */

#if defined (CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4) || defined (CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
static struct lab_i2c_board_info i2c_bus0[] = {
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
	{
		.bi = {
			I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
			.irq = TOUCH_IRQ_GPIO,
			.platform_data = &rmi4_platformdata,
		},
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4
	{
		.bi = {
			I2C_BOARD_INFO(CYTTSP4_I2C_NAME, 0x24),
			.irq = TOUCH_IRQ_GPIO,
			.platform_data = CYTTSP4_I2C_NAME,
		},
	},
#endif
};
#endif

/* I2C init method for cypress touch */
static int __init touch_i2c_init(void)
{
#if defined (CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4) || defined (CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
	int err;
	struct lab_i2c_board_info *info;
	int index = 0;

#if defined (CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4) && defined (CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
	unsigned int board_type = BOARD_ID_ASTON_PANEL;
#ifdef CONFIG_IDME
	board_type = idme_get_board_type();
#endif
	switch (board_type) {
	case BOARD_ID_ARIEL_PANEL:
		index = 1;
		break;
	case BOARD_ID_ASTON_PANEL:
		index = 0;
		break;
	default:
		index = 0;
	}
#endif

	info = &i2c_bus0[index];

	err = i2c_register_board_info(
			0, &info->bi, 1);

	if (err) {
		pr_err("%s: register i2c board info failed!\n", __func__);
		return 0;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4
	cyttsp4_register_core_device(&cyttsp4_core_info);
	cyttsp4_register_device(&cyttsp4_mt_device_info);
#endif
#endif /* defined either CYTTSP4 or SYNAPTICS  */
	return 0;
}

early_initcall(touch_i2c_init);

static void touch_platform_exit(void)
{
	pr_info("%s: module exit\n", __func__);
}

module_exit(touch_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen platform device");
