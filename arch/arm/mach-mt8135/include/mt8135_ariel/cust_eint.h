/*
 * Generated by MTK SP DrvGen Version 03.13.0 for MT8135_NP. Copyright MediaTek Inc. (C) 2013.
 * Fri Oct 11 14:10:18 2013
 * Do Not Modify the File.
 */

#ifndef __CUST_EINTH
#define __CUST_EINTH
#ifdef __cplusplus
extern "C" {
#endif
#define CUST_EINTF_TRIGGER_RISING			1	/* High Polarity and Edge Sensitive */
#define CUST_EINTF_TRIGGER_FALLING			2	/* Low Polarity and Edge Sensitive */
#define CUST_EINTF_TRIGGER_HIGH					4	/* High Polarity and Level Sensitive */
#define CUST_EINTF_TRIGGER_LOW					8	/* Low Polarity and Level Sensitive */
#define CUST_EINT_DEBOUNCE_DISABLE          0
#define CUST_EINT_DEBOUNCE_ENABLE           1
/* //////////////////////////////////////////////////////////////////////////// */


#define CUST_EINT_MT6397_PMIC_NUM              0
#define CUST_EINT_MT6397_PMIC_DEBOUNCE_CN      1
#define CUST_EINT_MT6397_PMIC_TYPE							CUST_EINTF_TRIGGER_HIGH
#define CUST_EINT_MT6397_PMIC_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_ENABLE

#define CUST_EINT_GYRO_NUM              2
#define CUST_EINT_GYRO_DEBOUNCE_CN      0
#define CUST_EINT_GYRO_TYPE							CUST_EINTF_TRIGGER_HIGH
#define CUST_EINT_GYRO_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE

#define CUST_EINT_TOUCH_PANEL_NUM              5
#define CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN      0
#define CUST_EINT_TOUCH_PANEL_TYPE							CUST_EINTF_TRIGGER_FALLING
#define CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE

#define CUST_EINT_COMBO_BGF_NUM              8
#define CUST_EINT_COMBO_BGF_DEBOUNCE_CN      0
#define CUST_EINT_COMBO_BGF_TYPE							CUST_EINTF_TRIGGER_LOW
#define CUST_EINT_COMBO_BGF_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE

#define CUST_EINT_WIFI_NUM              9
#define CUST_EINT_WIFI_DEBOUNCE_CN      0
#define CUST_EINT_WIFI_TYPE							CUST_EINTF_TRIGGER_LOW
#define CUST_EINT_WIFI_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE

#define CUST_EINT_ACCDET_NUM              10
#define CUST_EINT_ACCDET_DEBOUNCE_CN      256
#define CUST_EINT_ACCDET_TYPE							CUST_EINTF_TRIGGER_LOW
#define CUST_EINT_ACCDET_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_ENABLE



/* //////////////////////////////////////////////////////////////////////////// */
#ifdef __cplusplus
}
#endif
#endif				/* _CUST_EINT_H */
