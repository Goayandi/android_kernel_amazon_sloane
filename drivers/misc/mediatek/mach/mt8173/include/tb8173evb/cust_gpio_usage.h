/*
 * Generated by MTK SP DrvGen Version 03.13.6 for MT8173. Copyright MediaTek Inc. (C) 2013.
 * Mon Nov 10 20:13:24 2014
 * Do Not Modify the File.
 */

#ifndef __CUST_GPIO_USAGE_H__
#define __CUST_GPIO_USAGE_H__


#define GPIO_WIFI_EINT_PIN         (GPIO1 | 0x80000000)
#define GPIO_WIFI_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_WIFI_EINT_PIN_M_EINT   GPIO_WIFI_EINT_PIN_M_GPIO

#define GPIO_IRQ_NFC_PIN         (GPIO5 | 0x80000000)
#define GPIO_IRQ_NFC_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_IRQ_NFC_PIN_M_CLK   GPIO_MODE_01
#define GPIO_IRQ_NFC_PIN_M_EINT   GPIO_IRQ_NFC_PIN_M_GPIO

#define GPIO_EXT_BUCK_EN_A_PIN         (GPIO6 | 0x80000000)
#define GPIO_EXT_BUCK_EN_A_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_CTP_EINT_PIN         (GPIO7 | 0x80000000)
#define GPIO_CTP_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_CTP_EINT_PIN_M_EINT   GPIO_CTP_EINT_PIN_M_GPIO

#define GPIO_CTP_RST_PIN         (GPIO8 | 0x80000000)
#define GPIO_CTP_RST_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_NFC_EINT_PIN         (GPIO10 | 0x80000000)
#define GPIO_NFC_EINT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_NFC_EINT_PIN_M_CLK   GPIO_MODE_01
#define GPIO_NFC_EINT_PIN_M_PWM   GPIO_MODE_03
#define GPIO_NFC_EINT_PIN_CLK     CLK_OUT0
#define GPIO_NFC_EINT_PIN_FREQ    GPIO_CLKSRC_NONE

#define GPIO_EXT_BUCK_IC_EN_PIN         (GPIO29 | 0x80000000)
#define GPIO_EXT_BUCK_IC_EN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_PCM_DAICLK_PIN         (GPIO33 | 0x80000000)
#define GPIO_PCM_DAICLK_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAICLK_PIN_M_CLK   GPIO_MODE_01
#define GPIO_PCM_DAICLK_PIN_M_PCM0_CLK   GPIO_MODE_02

#define GPIO_PCM_DAIPCMIN_PIN         (GPIO34 | 0x80000000)
#define GPIO_PCM_DAIPCMIN_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAIPCMIN_PIN_M_MRG_DI   GPIO_MODE_01
#define GPIO_PCM_DAIPCMIN_PIN_M_PCM0_DI   GPIO_MODE_02

#define GPIO_PCM_DAIPCMOUT_PIN         (GPIO35 | 0x80000000)
#define GPIO_PCM_DAIPCMOUT_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAIPCMOUT_PIN_M_MRG_DO   GPIO_MODE_01
#define GPIO_PCM_DAIPCMOUT_PIN_M_PCM0_DO   GPIO_MODE_02

#define GPIO_PCM_DAISYNC_PIN         (GPIO36 | 0x80000000)
#define GPIO_PCM_DAISYNC_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_PCM_DAISYNC_PIN_M_MRG_SYNC   GPIO_MODE_01
#define GPIO_PCM_DAISYNC_PIN_M_PCM0_SYNC   GPIO_MODE_02

#define GPIO_COMBO_RST_PIN         (GPIO38 | 0x80000000)
#define GPIO_COMBO_RST_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_COMBO_RST_PIN_M_CLK   GPIO_MODE_06
#define GPIO_COMBO_RST_PIN_M_USB_DRVVBUS_P   GPIO_MODE_01
#define GPIO_COMBO_RST_PIN_CLK     CLK_OUT1
#define GPIO_COMBO_RST_PIN_FREQ    GPIO_CLKSRC_NONE

#define GPIO_LCM_PWR2_EN         (GPIO42 | 0x80000000)
#define GPIO_LCM_PWR2_EN_M_GPIO   GPIO_MODE_00

#define GPIO_AUD_CLK_MOSI_PIN         (GPIO83 | 0x80000000)
#define GPIO_AUD_CLK_MOSI_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_AUD_CLK_MOSI_PIN_M_CLK   GPIO_MODE_01

#define GPIO_AUD_DAT_MISO_PIN         (GPIO84 | 0x80000000)
#define GPIO_AUD_DAT_MISO_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_AUD_DAT_MISO_PIN_M_AUD_DAT_MISO   GPIO_MODE_01

#define GPIO_AUD_DAT_MOSI_PIN         (GPIO85 | 0x80000000)
#define GPIO_AUD_DAT_MOSI_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_AUD_DAT_MOSI_PIN_M_AUD_DAT_MOSI   GPIO_MODE_01

#define GPIO_NFC_OSC_EN_PIN         (GPIO88 | 0x80000000)
#define GPIO_NFC_OSC_EN_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_NFC_OSC_EN_PIN_M_CLK   GPIO_MODE_01

#define GPIO_CAMERA_CMRST_PIN         (GPIO92 | 0x80000000)
#define GPIO_CAMERA_CMRST_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_CAMERA_CMRST_PIN_M_CLK   GPIO_MODE_01

#define GPIO_CAMERA_2_CMRST_PIN         (GPIO93 | 0x80000000)
#define GPIO_CAMERA_2_CMRST_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_CAMERA_CMPDN_PIN         (GPIO94 | 0x80000000)
#define GPIO_CAMERA_CMPDN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_CAMERA_2_CMPDN_PIN         (GPIO95 | 0x80000000)
#define GPIO_CAMERA_2_CMPDN_PIN_M_GPIO   GPIO_MODE_00

#define GPIO_LCM_STB         (GPIO102 | 0x80000000)
#define GPIO_LCM_STB_M_GPIO   GPIO_MODE_00
#define GPIO_LCM_STB_M_PWM   GPIO_MODE_05

#define GPIO_LCM_RST         (GPIO103 | 0x80000000)
#define GPIO_LCM_RST_M_GPIO   GPIO_MODE_00
#define GPIO_LCM_RST_M_PWM   GPIO_MODE_05

#define GPIO_COMBO_URXD_PIN         (GPIO117 | 0x80000000)
#define GPIO_COMBO_URXD_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_COMBO_URXD_PIN_M_URXD   GPIO_MODE_01
#define GPIO_COMBO_URXD_PIN_M_UTXD   GPIO_MODE_02

#define GPIO_COMBO_UTXD_PIN         (GPIO118 | 0x80000000)
#define GPIO_COMBO_UTXD_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_COMBO_UTXD_PIN_M_UTXD   GPIO_MODE_01
#define GPIO_COMBO_UTXD_PIN_M_URXD   GPIO_MODE_02

#define GPIO_KPD_KROW0_PIN         (GPIO119 | 0x80000000)
#define GPIO_KPD_KROW0_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KROW0_PIN_M_KROW   GPIO_MODE_01

#define GPIO_KPD_KROW1_PIN         (GPIO120 | 0x80000000)
#define GPIO_KPD_KROW1_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KROW1_PIN_M_KROW   GPIO_MODE_01
#define GPIO_KPD_KROW1_PIN_M_PWM   GPIO_MODE_03

#define GPIO_KPD_KROW2_PIN         (GPIO121 | 0x80000000)
#define GPIO_KPD_KROW2_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KROW2_PIN_M_KROW   GPIO_MODE_01
#define GPIO_KPD_KROW2_PIN_M_PWM   GPIO_MODE_04

#define GPIO_KPD_KCOL0_PIN         (GPIO122 | 0x80000000)
#define GPIO_KPD_KCOL0_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KCOL0_PIN_M_KCOL   GPIO_MODE_01

#define GPIO_KPD_KCOL1_PIN         (GPIO123 | 0x80000000)
#define GPIO_KPD_KCOL1_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KCOL1_PIN_M_KCOL   GPIO_MODE_01
#define GPIO_KPD_KCOL1_PIN_M_PWM   GPIO_MODE_03

#define GPIO_KPD_KCOL2_PIN         (GPIO124 | 0x80000000)
#define GPIO_KPD_KCOL2_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_KPD_KCOL2_PIN_M_KCOL   GPIO_MODE_01
#define GPIO_KPD_KCOL2_PIN_M_PWM   GPIO_MODE_04

#define GPIO_LCM_BRIDGE_EN         (GPIO127 | 0x80000000)
#define GPIO_LCM_BRIDGE_EN_M_GPIO   GPIO_MODE_00

#define GPIO_LCM_PWR_EN         (GPIO131 | 0x80000000)
#define GPIO_LCM_PWR_EN_M_GPIO   GPIO_MODE_00


/*Output for default variable names*/
/*@XXX_XX_PIN in gpio.cmp          */



#define GPIO_COMBO_PMU_EN_PIN         GPIOEXT13
#define GPIO_COMBO_PMU_EN_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_COMBO_PMU_EN_PIN_M_EINT   GPIO_MODE_02

#define GPIO_GPS_LNA_PIN         GPIOEXT14
#define GPIO_GPS_LNA_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_GPS_LNA_PIN_M_EINT   GPIO_MODE_02

#define GPIO_NFC_VENB_PIN         GPIOEXT21
#define GPIO_NFC_VENB_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_NFC_VENB_PIN_M_EINT   GPIO_MODE_02
#define GPIO_NFC_VENB_PIN_M_ROW   GPIO_MODE_01
#define GPIO_NFC_VENB_PIN_M_SDA0_3X   GPIO_MODE_03
#define GPIO_NFC_VENB_PIN_M_TEST_IN   GPIO_MODE_06
#define GPIO_NFC_VENB_PIN_M_TEST_OUT   GPIO_MODE_07

#define GPIO_NFC_RST_PIN         GPIOEXT25
#define GPIO_NFC_RST_PIN_M_GPIO   GPIO_MODE_00
#define GPIO_NFC_RST_PIN_M_EINT   GPIO_MODE_02
#define GPIO_NFC_RST_PIN_M_ROW   GPIO_MODE_01
#define GPIO_NFC_RST_PIN_M_TEST_IN   GPIO_MODE_06
#define GPIO_NFC_RST_PIN_M_TEST_OUT   GPIO_MODE_07

#define GPIO_HDMI_POWER_CONTROL         GPIOEXT15
#define GPIO_HDMI_POWER_CONTROL_M_GPIO  GPIO_MODE_00
#define GPIO_HDMI_POWER_CONTROL_M_EINT  GPIO_MODE_02
/*Output for default variable names*/
/*@XXX_XX_PIN in gpio.cmp          */



#endif				/* __CUST_GPIO_USAGE_H__ */
