#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#else
#include <string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include "ddp_dsi.h"
#include "ddp_hal.h"
#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include "sn65dsi83_i2c.h"
#include "ddp_dsi.h"
#include "ddp_hal.h"
#endif
#include "lcm_drv.h"
#include "cpt_clap070wp03xg_sn65dsi83.h"

#define LVDS_PANEL_8BITS_SUPPORT
#define SN65DSI83_PATTERN

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

/* GPIO131        VDD33_LCD */
#ifdef GPIO_LCM_PWR_EN
#define GPIO_LCD_PWR_EN      GPIO_LCM_PWR_EN
#else
#define GPIO_LCD_PWR_EN      GPIO131	/* 0xFFFFFFFF */
#endif

/* GPIO42       VDD18_LVDS for SN65DSI83 power */
#ifdef GPIO_LCM_PWR2_EN
#define GPIO_LCD_PWR2_EN      GPIO_LCM_PWR2_EN
#else
#define GPIO_LCD_PWR2_EN      GPIO42	/* 0xFFFFFFFF */
#endif

/* VGP6_PMU       AVDDVGH/VGL/VCOM for panel power */

/* GPIO103       LCM_RST for panel */
#ifdef GPIO_LCM_RST
#define GPIO_LCD_RST_EN		GPIO_LCM_RST
#else
#define GPIO_LCD_RST_EN		GPIO103	/* 0xFFFFFFFF */
#endif

/* GPIO127       LVDS_EN_R for SN65DSI83 */
#ifdef GPIO_LCM_BRIDGE_EN
#define GPIO_LCD_BRIDGE_EN		GPIO_LCM_BRIDGE_EN
#else
#define GPIO_LCD_BRIDGE_EN		GPIO127	/* 0xFFFFFFFF */
#endif

/* GPIO87       DISP_PWM0 for backlight panel */
#ifdef GPIO_LCM_BL_EN
#define GPIO_LCD_BL_EN		GPIO_LCM_BL_EN
#else
#define GPIO_LCD_BL_EN		0xFFFFFFFF
#endif

/* GPIO102       LCM_STBY_2V8 for panel */
#ifdef GPIO_LCM_STB
#define GPIO_LCD_STB_EN		GPIO_LCM_STB
#else
#define GPIO_LCD_STB_EN		GPIO102	/* 0xFFFFFFFF */
#endif

/* the following don't use by lcm driver */
#ifdef GPIO_LCM_LVL_SHIFT_EN
#define GPIO_SHIFT_EN			GPIO_LCM_LVL_SHIFT_EN
#else
#define GPIO_SHIFT_EN			0xFFFFFFFF
#endif

#define SN65DSI_DEBUG		/* for check system(bb dsi and ti chip) status */
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};


#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))
#define UDELAY(n)					(lcm_util.udelay(n))
#define MDELAY(n)					(lcm_util.mdelay(n))
#define REGFLAG_DELAY			0xAB

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
extern void DSI_clk_HS_mode(DISP_MODULE_ENUM module, void *cmdq, bool enter);

#ifdef BUILD_LK

#define sn65dsi83_SLAVE_ADDR_WRITE		0x58
#define SN65DSI83_I2C_ID							I2C4

static struct mt_i2c_t sn65dsi83_i2c;

/**********************************************************
  *
  *   [I2C Function For Read/Write fan5405]
  *
  *********************************************************/
kal_uint32 sn65dsi83_write_byte(kal_uint8 addr, kal_uint8 value)
{
	kal_uint32 ret_code = I2C_OK;
	kal_uint8 write_data[2];
	kal_uint16 len;

	write_data[0] = addr;
	write_data[1] = value;

	sn65dsi83_i2c.id = SN65DSI83_I2C_ID;
	/* Since i2c will left shift 1 bit, we need to set SN65DSI83 I2C address to >>1 */
	sn65dsi83_i2c.addr = (sn65dsi83_SLAVE_ADDR_WRITE >> 1);
	sn65dsi83_i2c.mode = ST_MODE;
	sn65dsi83_i2c.speed = 100;
	len = 2;

	ret_code = i2c_write(&sn65dsi83_i2c, write_data, len);

	return ret_code;
}

kal_uint32 sn65dsi83_read_byte(kal_uint8 addr, kal_uint8 *dataBuffer)
{
	kal_uint32 ret_code = I2C_OK;
	kal_uint16 len;

	*dataBuffer = addr;

	sn65dsi83_i2c.id = SN65DSI83_I2C_ID;
	/* Since i2c will left shift 1 bit, we need to set SN65DSI83 I2C address to >>1 */
	sn65dsi83_i2c.addr = (sn65dsi83_SLAVE_ADDR_WRITE >> 1);
	sn65dsi83_i2c.mode = ST_MODE;
	sn65dsi83_i2c.speed = 100;
	len = 1;

	ret_code = i2c_write_read(&sn65dsi83_i2c, dataBuffer, len, len);

	return ret_code;
}

 /******************************************************************************
 *IIC drvier,:protocol type 2 add by chenguangjian end
 ******************************************************************************/
#else
extern int sn65dsi83_read_byte(kal_uint8 cmd, kal_uint8 *returnData);
extern int sn65dsi83_write_byte(kal_uint8 cmd, kal_uint8 Data);
#endif
/* sn65dis83 chip init table */
static sn65dsi8x_setting_table sn65dis83_init_table[] = {
#if defined(LVDS_PANEL_8BITS_SUPPORT)
#if 0
	{0x09, 0x00},
	{0x0A, 0x05},
	{0x0B, 0x10},
	{0x0D, 0x00},
	{0x10, 0x26},
	{0x11, 0x00},
	{0x12, 0x2c},
	{0x13, 0x00},
	{0x18, 0x78},
	{0x19, 0x00},
	{0x1A, 0x03},
	{0x1B, 0x00},
	{0x20, 0x20},
	{0x21, 0x03},
	{0x22, 0x00},
	{0x23, 0x00},
	{0x24, 0x00},
	{0x25, 0x05},		/* 0x00 */
	{0x26, 0x00},
	{0x27, 0x00},
	{0x28, 0x21},
	{0x29, 0x00},
	{0x2A, 0x00},
	{0x2B, 0x00},
	{0x2C, 0x0a},
	{0x2D, 0x00},
	{0x2E, 0x00},
	{0x2F, 0x00},
	{0x30, 0x02},
	{0x31, 0x00},
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x18},
	{0x35, 0x00},
	{0x36, 0x02},		/* 0x00 */
	{0x37, 0x00},
	{0x38, 0x1e},		/* 0x00 */
	{0x39, 0x00},
	{0x3A, 0x04},		/* 0x00 */
	{0x3B, 0x00},
	{0x3C, 0x10},		/* 0x00 */
	{0x3D, 0x00},
	{0x3E, 0x00},
	{0x0D, 0x01},
	{REGFLAG_DELAY, 0x0a},
	{0x09, 0x01},
	{0xFF, 0x00},
#else
	{0x09, 0x00},
	{0x0A, 0x05},
	{0x0B, 0x10},
	{0x0D, 0x00},
	{0x10, 0x26},
	{0x11, 0x00},
	{0x12, 0x2c},
	{0x13, 0x00},
	{0x18, 0x78},
	{0x19, 0x00},
	{0x1A, 0x03},
	{0x1B, 0x00},
	{0x20, 0x20},
	{0x21, 0x03},
	{0x22, 0x00},
	{0x23, 0x00},
	{0x24, 0x00},
	{0x25, 0x00},		/* 0x00 */
	{0x26, 0x00},
	{0x27, 0x00},
	{0x28, 0x21},
	{0x29, 0x00},
	{0x2A, 0x00},
	{0x2B, 0x00},
	{0x2C, 0x0a},
	{0x2D, 0x00},
	{0x2E, 0x00},
	{0x2F, 0x00},
	{0x30, 0x02},
	{0x31, 0x00},
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x18},
	{0x35, 0x00},
	{0x36, 0x00},		/* 0x00 */
	{0x37, 0x00},
	{0x38, 0x00},		/* 0x00 */
	{0x39, 0x00},
	{0x3A, 0x00},		/* 0x00 */
	{0x3B, 0x00},
	{0x3C, 0x00},		/* 0x00 */
	{0x3D, 0x00},
	{0x3E, 0x00},
	{0x0D, 0x01},
	{REGFLAG_DELAY, 0x0a},
	{0x09, 0x01},
	{0xFF, 0x00},
#endif
#endif
};

static void push_table(sn65dsi8x_setting_table *table, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].data);
			break;

		case 0xFF:
			break;

		default:
#ifdef BUILD_LK
			sn65dsi83_write_byte(cmd, table[i].data);
#else
			sn65dsi83_write_byte(cmd, table[i].data);
#endif
		}
	}
}

#ifdef SN65DSI_DEBUG
static void dump_reg_table(sn65dsi8x_setting_table *table, unsigned int count)
{
	unsigned int i;
	unsigned char data;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].data);
			break;

		case 0xFF:
			break;

		default:
#ifdef BUILD_LK
			sn65dsi83_read_byte(cmd, &data);
			printf("dump cmd=0x%x  data=0x%x\n", cmd, data);
#else
			sn65dsi83_read_byte(cmd, &data);
			printk("dump cmd=0x%x  data=0x%x\n", cmd, data);
#endif
		}
	}
}
#endif

void init_sn65dsi8x(void)
{
#ifdef SN65DSI_DEBUG
	unsigned char data;
#endif

	push_table(sn65dis83_init_table,
		   sizeof(sn65dis83_init_table) / sizeof(sn65dsi8x_setting_table));

#ifdef SN65DSI_DEBUG
	sn65dsi83_write_byte(0xe0, 1);
	sn65dsi83_write_byte(0xe1, 0xff);
	MDELAY(5);

	sn65dsi83_read_byte(0xe5, &data);
#ifdef BUILD_LK
	printf("dump cmd=0xe5  data=0x%x\n", data);
#else
	printk("dump cmd=0xe5  data=0x%x\n", data);
#endif
	dump_reg_table(sn65dis83_init_table,
		       sizeof(sn65dis83_init_table) / sizeof(sn65dsi8x_setting_table));
#endif
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode = SYNC_EVENT_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;

	/* The following defined the fomat for data coming from LCD engine. */
#if defined(LVDS_PANEL_8BITS_SUPPORT)
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
#else
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB666;
#endif

	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 2;
	params->dsi.vertical_frontporch = 4;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 24;
	params->dsi.horizontal_frontporch = 30;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#if defined(LVDS_PANEL_8BITS_SUPPORT)
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
#else
	params->dsi.PS = LCM_PACKED_PS_18BIT_RGB666;
#endif

	params->dsi.pll_select = 0;	/* 0: MIPI_PLL; 1: LVDS_PLL */
	params->dsi.PLL_CLOCK = 224;
	params->dsi.cont_clock = 1;
}

#ifndef BUILD_LK
static struct regulator *lcm_vgp;

/* get(vgp6) LDO supply */
static int lcm_get_vgp_supply(struct platform_device *pdev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	printk("[Kernel/LCM]lcm_get_vgp_supply is going\n");

	lcm_vgp_ldo = devm_regulator_get(&pdev->dev, "reg-lcm");

	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		dev_err(&pdev->dev, "failed to get reg-lcm LDO\n");
		return ret;
	}

	pr_debug("[Kernel/LCM]lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	dev_err(&pdev->dev, "lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = kzalloc(sizeof(uint8_t *) * sizeof(struct regulator *), GFP_KERNEL);

	if (!lcm_vgp) {
		printk("[Kernel/LCM]lcm_get_vgp_supply kmalloc fail.\n");

		ret = -ENOMEM;
		return ret;
	}

	memset(lcm_vgp, 0, sizeof(uint8_t *) * sizeof(struct regulator *));
	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

static int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	/* set(vgp6) voltage to 3.3V */
	ret = regulator_set_voltage(lcm_vgp, 3300000, 3300000);
	if (ret != 0) {
		printk("lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get(vgp6) voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 3300000)
		printk("set regulator voltage lcm_vgp pass!\n");

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		printk("Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
}

static int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	printk("lcm query regulator enable status[0x%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			printk("lcm failed to disable lcm_vgp: %d\n", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			printk("lcm regulator disable pass\n");
	}

	return ret;
}

static void lcm_request_gpio_control(void)
{
	gpio_request(GPIO_LCD_PWR_EN, "GPIO_LCD_PWR_EN");
	pr_debug("[KE/LCM] GPIO_LCD_PWR_EN = 0x%x\n", GPIO_LCD_PWR_EN);

	gpio_request(GPIO_LCD_PWR2_EN, "GPIO_LCD_PWR2_EN");
	pr_debug("[KE/LCM] GPIO_LCD_PWR2_EN = 0x%x\n", GPIO_LCD_PWR2_EN);

	gpio_request(GPIO_LCD_RST_EN, "GPIO_LCD_RST_EN");
	pr_debug("[KE/LCM] GPIO_LCD_RST_EN = 0x%x\n", GPIO_LCD_RST_EN);

	gpio_request(GPIO_LCD_STB_EN, "GPIO_LCD_STB_EN");
	pr_debug("[KE/LCM] GPIO_LCD_STB_EN =  0x%x\n", GPIO_LCD_STB_EN);

	gpio_request(GPIO_LCD_BRIDGE_EN, "GPIO_LCD_BRIDGE_EN");
	pr_debug("[KE/LCM] GPIO_LCD_BRIDGE_EN = 0x%x\n", GPIO_LCD_BRIDGE_EN);
}
#endif

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if (GPIO == 0xFFFFFFFF) {
#ifdef BUILD_LK
		printf("[LK/LCM] GPIO_LCD_PWR_EN =   0x%x\n", GPIO_LCD_PWR_EN);
		printf("[LK/LCM] GPIO_LCD_PWR2_EN =  0x%x\n", GPIO_LCD_PWR2_EN);
		printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x\n", GPIO_LCD_RST_EN);
		printf("[LK/LCM] GPIO_LCD_STB_EN =   0x%x\n", GPIO_LCD_STB_EN);
		printf("[LK/LCM] GPIO_SHIFT_EN =   0x%x\n", GPIO_SHIFT_EN);
		printf("[LK/LCM] GPIO_LCD_BL_EN =   0x%x\n", GPIO_LCD_BL_EN);
		printf("[LK/LCM] GPIO_LCD_BRIDGE_EN =  0x%x\n", GPIO_LCD_BRIDGE_EN);
#elif (defined BUILD_UBOOT)	/* do nothing in uboot */
#else
#endif

		return;
	}

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#else
	gpio_set_value(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#endif
}

static void lcm_init(struct platform_device *pdev)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_init\n");

	/* step1: sn65dsi8x enbable and init */
	/* GPIO42       VDD18_LVDS for SN65DSI83 power */
	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(20);

	/* GPIO127       LVDS_EN_R for SN65DSI83 */
	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(50);

	DSI_clk_HS_mode(0, NULL, 1);
	MDELAY(5);

	init_sn65dsi8x();
	MDELAY(10);

	/* step 2 :lvds lcd init */
	/* GPIO131        VDD33_LCD */
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(50);

	/* VGP6_PMU       AVDDVGH/VGL/VCOM for panel power */
	upmu_set_rg_vgp6_vosel(0x7);
	upmu_set_rg_vgp6_sw_en(0x1);

	/* GPIO103       LCM_RST for panel */
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(50);

	/* GPIO102       LCM_STBY_2V8 for panel */
	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
	MDELAY(20);

	/* GPIO87       DISP_PWM0 for backlight panel */
	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);

#elif (defined BUILD_UBOOT)
#else
	printk("[Kernel/LCM]lcm_init\n");

	DSI_clk_HS_mode(0, NULL, 1);

	lcm_get_vgp_supply(pdev);
	MDELAY(20);

	lcm_request_gpio_control();
#endif
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_suspend enter\n");

#else
	unsigned char temp;

	printk("[Kernel/LCM]lcm_suspend enter\n");

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ZERO);
	MDELAY(30);		/* avoid LCD resume transint */

	/* step 1 power down lvds lcd */
	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(10);

	//hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");            /* LCMBIASON :VGH VHL */
	lcm_vgp_supply_enable();
	MDELAY(20);

	lcm_vgp_supply_disable();
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);	/* LCM VCC :enable LCD VCC */
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);	/* LCM_STBY */
	MDELAY(50);

	/* step 2 suspend sn65dsi8x */
	sn65dsi83_read_byte(0x0a, &temp);	/* for test wether ti lock the pll clok */
	printk("lcm_suspend  0x0a  value=0x%x\n", temp);

	sn65dsi83_read_byte(0x0d, &temp);
	printk("lcm_suspend  0x0d  value=0x%x\n", temp);

	sn65dsi83_write_byte(0x0d, (temp & 0xfe));	/* set bit0: 0 */

	/* step 3 set dsi LP mode */
	DSI_clk_HS_mode(0, NULL, 0);
#endif
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_resume enter\n");

#else
#ifdef SN65DSI_DEBUG
	unsigned char temp;
#endif

	printk("[Kernel/LCM]lcm_resume enter\n");

	DSI_clk_HS_mode(0, NULL, 1);
	MDELAY(50);

	/* step 1 resume sn65dsi8x */
	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(10);

	init_sn65dsi8x();
	MDELAY(10);

#ifdef SN65DSI_DEBUG
	sn65dsi83_read_byte(0x0a, &temp);
	printk("lcm_resume cmd-- 0x0a=0x%x\n", temp);

	sn65dsi83_read_byte(0x0d, &temp);
	printk("lcm_resume cmd-- 0x0d=0x%x\n", temp);

	sn65dsi83_read_byte(0x09, &temp);
	printk("lcm_resume cmd-- 0x09=0x%x\n", temp);
#endif

	/* step 2 resume lvds */
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(50);

	/* hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM"); */
	lcm_vgp_supply_enable();
	MDELAY(30);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
#endif
}

static unsigned int lcm_compare_id(void)
{
#if defined(BUILD_LK)
	printf("Sn65dsi83 lcm_compare_id enter\n");
#endif

	return 1;
}

LCM_DRIVER cpt_clap070wp03xg_sn65dsi83_lcm_drv = {
	.name = "cpt_clap070wp03xg_sn65dsi83",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
};
