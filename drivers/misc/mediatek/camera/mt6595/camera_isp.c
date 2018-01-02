/******************************************************************************
 * mt6593_isp.c - MT6593 Linux ISP Device Driver
 *
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers ISP relative functions
 *
 ******************************************************************************/

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
#include <linux/proc_fs.h>	/* proc file use */
/*  */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/xlog.h>		/* For xlog_printk(). */
/*  */
#include <mach/hardware.h>
/* #include <mach/mt6593_pll.h> */
#include <camera_isp.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>	/* For clock mgr APIS. enable_clock()/disable_clock(). */
#include <mach/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
#include <mach/mt_spm_idle.h>	/* For spm_enable_sodi()/spm_disable_sodi(). */


#include <cmdq_core.h>

/*  */
#include "smi_common.h"
#include "camera_isp_m4u_extern.h"

#define CAMSV_DBG
#ifdef CAMSV_DBG
#define CAM_TAG "CAM:"
#define CAMSV_TAG "SV1:"
#define CAMSV2_TAG "SV2:"
#else
#define CAMSV_TAG ""
#define CAMSV2_TAG ""
#define CAM_TAG ""
#endif
typedef unsigned char MUINT8;
typedef unsigned int MUINT32;
/*  */
typedef signed char MINT8;
typedef signed int MINT32;
/*  */
typedef bool MBOOL;
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif
/* ---------------------------------------------------------------------------- */
/* #define LOG_MSG(fmt, arg...)    printk(KERN_ERR "[ISP][%s]" fmt,__FUNCTION__, ##arg) */
/* #define LOG_DBG(fmt, arg...)    printk(KERN_ERR  "[ISP][%s]" fmt,__FUNCTION__, ##arg) */
/* #define LOG_WRN(fmt, arg...)    printk(KERN_ERR "[ISP][%s]Warning" fmt,__FUNCTION__, ##arg) */
/* #define LOG_ERR(fmt, arg...)    printk(KERN_ERR   "[ISP][%s]Err(%5d):" fmt, __FUNCTION__,__LINE__, ##arg) */


#define LOG_VRB(format, args...)    printk("[ISP][%s] " format "\n", __func__, ##args)
#define LOG_DBG(format, args...)    printk("[ISP][%s] " format "\n", __func__, ##args)
/* Both ANDROID_LOG_DEBUG and ANDROID_LOG_VERBOSE can be logged only to mobile log, */
/* but ANDROID_LOG_INFO would be logged to both mobile log and uart, so we use ANDROID_LOG_DEBUG to replace ANDROID_LOG_INFO. */
#define LOG_INF(format, args...)    printk("[ISP][%s] " format "\n", __func__, ##args)
#define LOG_WRN(format, args...)    printk("[ISP][%s] WARNING: " format "\n", __func__, ##args)
#define LOG_ERR(format, args...)    printk("[ISP][%s, line%04d] ERROR: " format "\n", __func__, __LINE__, ##args)
#define LOG_AST(format, args...)    printk("[ISP][%s, line%04d] ASSERT: " format "\n", __func__, __LINE__, ##args)

/*******************************************************************************
*
********************************************************************************/
/* #define ISP_WR32(addr, data)    iowrite32(data, addr) // For other projects. */
#define ISP_WR32(addr, data)    mt65xx_reg_sync_writel(data, (void *)(addr))	/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define ISP_RD32(addr)          ioread32((void *)(addr))
#define ISP_SET_BIT(reg, bit)   ((*(volatile MUINT32*)(reg)) |= (MUINT32)(1 << (bit)))
#define ISP_CLR_BIT(reg, bit)   ((*(volatile MUINT32*)(reg)) &= ~((MUINT32)(1 << (bit))))
/*******************************************************************************
*
********************************************************************************/
#define ISP_DEV_NAME                "camera-isp"

/* ///////////////////////////////////////////////////////////////// */
/* for restricting range in mmap function */
/* isp driver */
#define ISP_RTBUF_REG_RANGE  0x10000
#define IMGSYS_BASE_ADDR     0x15000000
#define ISP_REG_RANGE        (0x10000)	/* 0x10000,the same with the value in isp_reg.h and page-aligned */
/* seninf driver */
#define SENINF_BASE_ADDR     0x15008000	/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define SENINF_REG_RANGE    (0x4000)	/* 0x4000,the same with the value in seninf_reg.h and page-aligned */
#define PLL_BASE_ADDR        0x10000000	/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define PLL_RANGE            (0x1000)	/* 0x200,the same with the value in seninf_drv.cpp and page-aligned */
#define MIPIRX_CONFIG_ADDR   0x1500C000	/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define MIPIRX_CONFIG_RANGE (0x1000)	/* 0x100,the same with the value in seninf_drv.cpp and page-aligned */
#define MIPIRX_ANALOG_ADDR   0x10217000	/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define MIPIRX_ANALOG_RANGE (0x3000)
#define GPIO_BASE_ADDR       0x10005000	/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define GPIO_RANGE          (0x1000)
/* security concern */
#define ISP_RANGE         (0x10000)
/* ///////////////////////////////////////////////////////////////// */

/*******************************************************************************
*
********************************************************************************/
#define ISP_DBG_INT                 (0x00000001)
#define ISP_DBG_HOLD_REG            (0x00000002)
#define ISP_DBG_READ_REG            (0x00000004)
#define ISP_DBG_WRITE_REG           (0x00000008)
#define ISP_DBG_CLK                 (0x00000010)
#define ISP_DBG_TASKLET             (0x00000020)
#define ISP_DBG_SCHEDULE_WORK       (0x00000040)
#define ISP_DBG_BUF_WRITE           (0x00000080)
#define ISP_DBG_BUF_CTRL            (0x00000100)
#define ISP_DBG_REF_CNT_CTRL        (0x00000200)
#define ISP_DBG_INT_2               (0x00000400)
#define ISP_DBG_INT_3               (0x00000800)


/*******************************************************************************
*
********************************************************************************/
#define ISP_ADDR                        (IMGSYS_BASE + 0x4000)
#define ISP_ADDR_CAMINF                 IMGSYS_BASE
#define ISP_REG_ADDR_EN1                (ISP_ADDR + 0x4)
#define ISP_REG_ADDR_INT_P1_ST          (ISP_ADDR + 0x4C)
#define ISP_REG_ADDR_INT_P1_ST2         (ISP_ADDR + 0x54)
#define ISP_REG_ADDR_INT_P1_ST_D        (ISP_ADDR + 0x5C)
#define ISP_REG_ADDR_INT_P1_ST2_D       (ISP_ADDR + 0x64)
#define ISP_REG_ADDR_INT_P2_ST          (ISP_ADDR + 0x6C)
#define ISP_REG_ADDR_INT_STATUSX        (ISP_ADDR + 0x70)
#define ISP_REG_ADDR_INT_STATUS2X       (ISP_ADDR + 0x74)
#define ISP_REG_ADDR_INT_STATUS3X       (ISP_ADDR + 0x78)
#define ISP_REG_ADDR_CAM_SW_CTL         (ISP_ADDR + 0x8C)
#define ISP_REG_ADDR_IMGO_FBC           (ISP_ADDR + 0xF0)
#define ISP_REG_ADDR_RRZO_FBC           (ISP_ADDR + 0xF4)
#define ISP_REG_ADDR_IMGO_D_FBC         (ISP_ADDR + 0xF8)
#define ISP_REG_ADDR_RRZO_D_FBC         (ISP_ADDR + 0xFC)
#define ISP_REG_ADDR_TG_VF_CON          (ISP_ADDR + 0x414)
#define ISP_REG_ADDR_TG_INTER_ST        (ISP_ADDR + 0x44C)
#define ISP_REG_ADDR_TG2_VF_CON         (ISP_ADDR + 0x2414)
#define ISP_REG_ADDR_TG2_INTER_ST       (ISP_ADDR + 0x244C)
#define ISP_REG_ADDR_IMGO_BASE_ADDR     (ISP_ADDR + 0x3300)
#define ISP_REG_ADDR_RRZO_BASE_ADDR     (ISP_ADDR + 0x3320)
#define ISP_REG_ADDR_IMGO_D_BASE_ADDR   (ISP_ADDR + 0x34D4)
#define ISP_REG_ADDR_RRZO_D_BASE_ADDR   (ISP_ADDR + 0x34F4)
#define ISP_REG_ADDR_SENINF1_INT        (ISP_ADDR + 0x4128)
#define ISP_REG_ADDR_SENINF2_INT        (ISP_ADDR + 0x4528)
#define ISP_REG_ADDR_SENINF3_INT        (ISP_ADDR + 0x4928)
#define ISP_REG_ADDR_SENINF4_INT        (ISP_ADDR + 0x4D28)
#define ISP_REG_ADDR_CAMSV_FMT_SEL      (ISP_ADDR + 0x5004)
#define ISP_REG_ADDR_CAMSV_INT          (ISP_ADDR + 0x500C)
#define ISP_REG_ADDR_CAMSV_SW_CTL       (ISP_ADDR + 0x5010)
#define ISP_REG_ADDR_CAMSV_TG_INTER_ST  (ISP_ADDR + 0x544C)
#define ISP_REG_ADDR_CAMSV2_FMT_SEL     (ISP_ADDR + 0x5804)
#define ISP_REG_ADDR_CAMSV2_INT         (ISP_ADDR + 0x580C)
#define ISP_REG_ADDR_CAMSV2_SW_CTL      (ISP_ADDR + 0x5810)
#define ISP_REG_ADDR_CAMSV_TG2_INTER_ST (ISP_ADDR + 0x5C4C)
#define ISP_REG_ADDR_CAMSV_IMGO_FBC     (ISP_ADDR + 0x501C)
#define ISP_REG_ADDR_CAMSV2_IMGO_FBC    (ISP_ADDR + 0x581C)
#define ISP_REG_ADDR_IMGO_SV_BASE_ADDR  (ISP_ADDR + 0x5208)
#define ISP_REG_ADDR_IMGO_SV_XSIZE      (ISP_ADDR + 0x5210)
#define ISP_REG_ADDR_IMGO_SV_YSIZE      (ISP_ADDR + 0x5214)
#define ISP_REG_ADDR_IMGO_SV_STRIDE     (ISP_ADDR + 0x5218)
#define ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR    (ISP_ADDR + 0x5228)
#define ISP_REG_ADDR_IMGO_SV_D_XSIZE    (ISP_ADDR + 0x5230)
#define ISP_REG_ADDR_IMGO_SV_D_YSIZE    (ISP_ADDR + 0x5234)
#define ISP_REG_ADDR_IMGO_SV_D_STRIDE   (ISP_ADDR + 0x5238)
#define TG_REG_ADDR_GRAB_W              (ISP_ADDR + 0x418)
#define TG2_REG_ADDR_GRAB_W             (ISP_ADDR + 0x2418)
#define ISP_REG_ADDR_CAMSV_TG_VF_CON    (ISP_ADDR + 0x5414)
#define ISP_REG_ADDR_CAMSV_TG2_VF_CON   (ISP_ADDR + 0x5C14)
/* spare register */
/* #define ISP_REG_ADDR_TG_MAGIC_0         (ISP_ADDR + 0x94) */
/* #define ISP_REG_ADDR_TG_MAGIC_1         (ISP_ADDR + 0x9C) */
/* New define by 20131114 */
#define ISP_REG_ADDR_TG_MAGIC_0         (IMGSYS_BASE + 0x75DC)	/* 0088 */

#define ISP_REG_ADDR_TG2_MAGIC_0        (IMGSYS_BASE + 0x75E4)	/* 0090 */


/*
CAM_REG_CTL_SPARE1              CAM_CTL_SPARE1;                 //4094
CAM_REG_CTL_SPARE2              CAM_CTL_SPARE2;                 //409C
CAM_REG_CTL_SPARE3              CAM_CTL_SPARE3;                 //4100
CAM_REG_AE_SPARE                 CAM_AE_SPARE;                   //4694
CAM_REG_DM_O_SPARE             CAM_DM_O_SPARE;                 //48F0
CAM_REG_MIX1_SPARE              CAM_MIX1_SPARE;                 //4C98
CAM_REG_MIX2_SPARE              CAM_MIX2_SPARE;                 //4CA8
CAM_REG_MIX3_SPARE              CAM_MIX3_SPARE;                 //4CB8
CAM_REG_NR3D_SPARE0            CAM_NR3D_SPARE0;                //4D04
CAM_REG_AWB_D_SPARE           CAM_AWB_D_SPARE;                //663C
CAM_REG_AE_D_SPARE              CAM_AE_D_SPARE;                 //6694
CAMSV_REG_CAMSV_SPARE0      CAMSV_CAMSV_SPARE0;             //9014
CAMSV_REG_CAMSV_SPARE1      CAMSV_CAMSV_SPARE1;             //9018
CAMSV_REG_CAMSV2_SPARE0    CAMSV_CAMSV2_SPARE0;            //9814
CAMSV_REG_CAMSV2_SPARE1    CAMSV_CAMSV2_SPARE1;            //9818
*/

/* inner register */
/* 1500_d000 ==> 1500_4000 */
/* 1500_e000 ==> 1500_6000 */
/* 1500_f000 ==> 1500_7000 */
#define ISP_INNER_REG_ADDR_FMT_SEL_P1       (ISP_ADDR + 0x0028)
#define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR + 0x002C)
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1       (ISP_ADDR_CAMINF + 0xD028) */
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR_CAMINF + 0xD02C) */
#define ISP_INNER_REG_ADDR_IMGO_XSIZE       (ISP_ADDR_CAMINF + 0xF308)
#define ISP_INNER_REG_ADDR_IMGO_YSIZE       (ISP_ADDR_CAMINF + 0xF30C)
#define ISP_INNER_REG_ADDR_IMGO_STRIDE      (ISP_ADDR_CAMINF + 0xF310)
#define ISP_INNER_REG_ADDR_RRZO_XSIZE       (ISP_ADDR_CAMINF + 0xF328)
#define ISP_INNER_REG_ADDR_RRZO_YSIZE       (ISP_ADDR_CAMINF + 0xF32C)
#define ISP_INNER_REG_ADDR_RRZO_STRIDE      (ISP_ADDR_CAMINF + 0xF330)
#define ISP_INNER_REG_ADDR_IMGO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4DC)
#define ISP_INNER_REG_ADDR_IMGO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF4E0)
#define ISP_INNER_REG_ADDR_IMGO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF4E4)
#define ISP_INNER_REG_ADDR_RRZO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4FC)
#define ISP_INNER_REG_ADDR_RRZO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF500)
#define ISP_INNER_REG_ADDR_RRZO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF504)

#define ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xD7B4)
#define ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xD7BC)
#define ISP_INNER_REG_ADDR_RRZ_IN_IMG        (ISP_ADDR_CAMINF + 0xD7A4)
#define ISP_INNER_REG_ADDR_RRZ_OUT_IMG       (ISP_ADDR_CAMINF + 0xD7A8)

#define ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xE7B4)
#define ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xE7BC)
#define ISP_INNER_REG_ADDR_RRZ_D_IN_IMG        (ISP_ADDR_CAMINF + 0xE7A4)
#define ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG       (ISP_ADDR_CAMINF + 0xE7A8)


/* camsv hw no inner address to read */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_XSIZE    (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_YSIZE  (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_STRIDE (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_XSIZE    (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_YSIZE    (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_STRIDE   (0) */
/* #define ISP_INNER_REG_ADDR_CAMSV_FMT_SEL  (0) */
/* #define ISP_INNER_REG_ADDR_CAMSV2_FMT_SEL (0) */

#define ISP_TPIPE_ADDR                  (0x15004000)

/* CAM_CTL_SW_CTL */
#define ISP_REG_SW_CTL_SW_RST_P1_MASK   (0x00000007)
#define ISP_REG_SW_CTL_SW_RST_TRIG      (0x00000001)
#define ISP_REG_SW_CTL_SW_RST_STATUS    (0x00000002)
#define ISP_REG_SW_CTL_HW_RST           (0x00000004)
#define ISP_REG_SW_CTL_SW_RST_P2_MASK   (0x00000070)
#define ISP_REG_SW_CTL_SW_RST_P2_TRIG   (0x00000010)
#define ISP_REG_SW_CTL_SW_RST_P2_STATUS (0x00000020)
#define ISP_REG_SW_CTL_HW_RST_P2        (0x00000040)
#define ISP_REG_SW_CTL_RST_CAM_P1       (1)
#define ISP_REG_SW_CTL_RST_CAM_P2       (2)
#define ISP_REG_SW_CTL_RST_CAMSV        (3)
#define ISP_REG_SW_CTL_RST_CAMSV2       (4)

/* CAM_CTL_INT_P1_STATUS */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST          (ISP_IRQ_P1_STATUS_VS1_INT_ST |\
					    ISP_IRQ_P1_STATUS_TG1_INT1_ST |\
					    ISP_IRQ_P1_STATUS_TG1_INT2_ST |\
					    ISP_IRQ_P1_STATUS_EXPDON1_ST |\
					    ISP_IRQ_P1_STATUS_PASS1_DON_ST |\
					    ISP_IRQ_P1_STATUS_SOF1_INT_ST |\
					    ISP_IRQ_P1_STATUS_AF_DON_ST |\
					    ISP_IRQ_P1_STATUS_FLK_DON_ST |\
					    ISP_IRQ_P1_STATUS_FBC_RRZO_DON_ST |\
					    ISP_IRQ_P1_STATUS_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST_ERR     (ISP_IRQ_P1_STATUS_TG1_ERR_ST |\
					    ISP_IRQ_P1_STATUS_TG1_GBERR |\
					    ISP_IRQ_P1_STATUS_CQ0_ERR |\
					    ISP_IRQ_P1_STATUS_CQ0_VS_ERR_ST |\
					    ISP_IRQ_P1_STATUS_IMGO_DROP_FRAME_ST |\
					    ISP_IRQ_P1_STATUS_RRZO_DROP_FRAME_ST |\
					    ISP_IRQ_P1_STATUS_IMGO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_AAO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_LCSO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_RRZO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_ESFKO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_FLK_ERR_ST |\
					    ISP_IRQ_P1_STATUS_LSC_ERR_ST |\
					    ISP_IRQ_P1_STATUS_DMA_ERR_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_P1_ST_WAITQ    (ISP_IRQ_P1_STATUS_VS1_INT_ST |\
					    ISP_IRQ_P1_STATUS_PASS1_DON_ST |\
					    ISP_IRQ_P1_STATUS_SOF1_INT_ST|\
					    ISP_IRQ_P1_STATUS_AF_DON_ST)

/* CAM_CTL_INT_P1_STATUS2 */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST2         (ISP_IRQ_P1_STATUS2_IMGO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_UFEO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_RRZO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_ESFKO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_LCSO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_AAO_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_BPCI_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_LSCI_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_AF_TAR_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_AF_FLO1_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_AF_FLO2_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_AF_FLO3_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST2_ERR     (0x0)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_P1_ST2_WAITQ   (0x0)


/* CAM_CTL_INT_P1_STATUS_D */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST_D        (ISP_IRQ_P1_STATUS_D_VS1_INT_ST |\
					    ISP_IRQ_P1_STATUS_D_TG1_INT1_ST |\
					    ISP_IRQ_P1_STATUS_D_TG1_INT2_ST |\
					    ISP_IRQ_P1_STATUS_D_EXPDON1_ST |\
					    ISP_IRQ_P1_STATUS_D_PASS1_DON_ST |\
					    ISP_IRQ_P1_STATUS_D_SOF1_INT_ST |\
					    ISP_IRQ_P1_STATUS_D_AF_DON_ST |\
					    ISP_IRQ_P1_STATUS_D_FBC_RRZO_DON_ST |\
					    ISP_IRQ_P1_STATUS_D_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST_D_ERR     (ISP_IRQ_P1_STATUS_D_TG1_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_TG1_GBERR |\
					    ISP_IRQ_P1_STATUS_D_CQ0_ERR |\
					    ISP_IRQ_P1_STATUS_D_CQ0_VS_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_IMGO_DROP_FRAME_ST |\
					    ISP_IRQ_P1_STATUS_D_RRZO_DROP_FRAME_ST |\
					    ISP_IRQ_P1_STATUS_D_IMGO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_AAO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_LCSO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_RRZO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_AFO_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_LSC_ERR_ST |\
					    ISP_IRQ_P1_STATUS_D_DMA_ERR_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_P1_ST_D_WAITQ    (ISP_IRQ_P1_STATUS_D_VS1_INT_ST |\
					    ISP_IRQ_P1_STATUS_D_PASS1_DON_ST |\
					    ISP_IRQ_P1_STATUS_D_SOF1_INT_ST|\
					    ISP_IRQ_P1_STATUS_D_AF_DON_ST)

/* CAM_CTL_INT_P1_STATUS2_D */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST2_D       (ISP_IRQ_P1_STATUS2_D_IMGO_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_RRZO_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AFO_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_LCSO_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AAO_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_BPCI_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_LSCI_D_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AF_TAR_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AF_FLO1_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AF_FLO2_DONE_ST |\
					    ISP_IRQ_P1_STATUS2_D_AF_FLO3_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST2_D_ERR   (0x0)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_P1_ST2_D_WAITQ  (0x0)

/* CAM_CTL_INT_P2_STATUS */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P2_ST          (ISP_IRQ_P2_STATUS_PASS2_DON_ST |\
					    ISP_IRQ_P2_STATUS_TILE_DON_ST |\
					    ISP_IRQ_P2_STATUS_CQ_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2A_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2B_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2C_DON_ST |\
					    ISP_IRQ_P2_STATUS_CQ1_DONE_ST   |\
					    ISP_IRQ_P2_STATUS_CQ2_DONE_ST   |\
					    ISP_IRQ_P2_STATUS_CQ3_DONE_ST   |\
					    ISP_IRQ_P2_STATUS_IMGI_DONE_ST |\
					    ISP_IRQ_P2_STATUS_UFDI_DONE_ST |\
					    ISP_IRQ_P2_STATUS_VIPI_DONE_ST |\
					    ISP_IRQ_P2_STATUS_VIP2I_DONE_ST |\
					    ISP_IRQ_P2_STATUS_VIP3I_DONE_ST |\
					    ISP_IRQ_P2_STATUS_LCEI_DONE_ST |\
					    ISP_IRQ_P2_STATUS_MFBO_DONE_ST |\
					    ISP_IRQ_P2_STATUS_IMG2O_DONE_ST |\
					    ISP_IRQ_P2_STATUS_IMG3O_DONE_ST |\
					    ISP_IRQ_P2_STATUS_IMG3BO_DONE_ST |\
					    ISP_IRQ_P2_STATUS_IMG3CO_DONE_ST |\
					    ISP_IRQ_P2_STATUS_FEO_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P2_ST_ERR      (ISP_IRQ_P2_STATUS_CQ_ERR_ST |\
					    ISP_IRQ_P2_STATUS_TDR_ERR_ST |\
					    ISP_IRQ_P2_STATUS_PASS2A_ERR_TRIG_ST |\
					    ISP_IRQ_P2_STATUS_PASS2B_ERR_TRIG_ST |\
					    ISP_IRQ_P2_STATUS_PASS2C_ERR_TRIG_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_P2_ST_WAITQ    (ISP_IRQ_P2_STATUS_PASS2_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2A_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2B_DON_ST |\
					    ISP_IRQ_P2_STATUS_PASS2C_DON_ST)
/* CAM_CTL_INT_STATUSX */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUSX        (ISP_IRQ_STATUSX_VS1_INT_ST |\
					    ISP_IRQ_STATUSX_TG1_INT1_ST |\
					    ISP_IRQ_STATUSX_TG1_INT2_ST |\
					    ISP_IRQ_STATUSX_EXPDON1_ST |\
					    ISP_IRQ_STATUSX_PASS1_DON_ST |\
					    ISP_IRQ_STATUSX_SOF1_INT_ST |\
					    ISP_IRQ_STATUSX_PASS2_DON_ST |\
					    ISP_IRQ_STATUSX_TILE_DON_ST |\
					    ISP_IRQ_STATUSX_AF_DON_ST |\
					    ISP_IRQ_STATUSX_FLK_DON_ST |\
					    ISP_IRQ_STATUSX_CQ_DON_ST |\
					    ISP_IRQ_STATUSX_FBC_RRZO_DON_ST |\
					    ISP_IRQ_STATUSX_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUSX_ERR    (ISP_IRQ_STATUSX_TG1_ERR_ST |\
					    ISP_IRQ_STATUSX_TG1_GBERR |\
					    ISP_IRQ_STATUSX_CQ0_ERR |\
					    ISP_IRQ_STATUSX_CQ0_VS_ERR_ST |\
					    ISP_IRQ_STATUSX_IMGO_DROP_FRAME_ST |\
					    ISP_IRQ_STATUSX_RRZO_DROP_FRAME_ST |\
					    ISP_IRQ_STATUSX_CQ_ERR_ST |\
					    ISP_IRQ_STATUSX_IMGO_ERR_ST |\
					    ISP_IRQ_STATUSX_AAO_ERR_ST |\
					    ISP_IRQ_STATUSX_LCSO_ERR_ST |\
					    ISP_IRQ_STATUSX_RRZO_ERR_ST |\
					    ISP_IRQ_STATUSX_ESFKO_ERR_ST |\
					    ISP_IRQ_STATUSX_FLK_ERR_ST |\
					    ISP_IRQ_STATUSX_LSC_ERR_ST |\
					    ISP_IRQ_STATUSX_DMA_ERR_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_STATUSX_WAITQ  (0x0)

/* CAM_CTL_INT_STATUS2X */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUS2X       (ISP_IRQ_STATUS2X_VS1_INT_ST |\
					    ISP_IRQ_STATUS2X_TG1_INT1_ST |\
					    ISP_IRQ_STATUS2X_TG1_INT2_ST |\
					    ISP_IRQ_STATUS2X_EXPDON1_ST |\
					    ISP_IRQ_STATUS2X_PASS1_DON_ST |\
					    ISP_IRQ_STATUS2X_SOF1_INT_ST |\
					    ISP_IRQ_STATUS2X_AF_DON_ST |\
					    ISP_IRQ_STATUS2X_FBC_RRZO_DON_ST |\
					    ISP_IRQ_STATUS2X_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUS2X_ERR   (ISP_IRQ_STATUS2X_TG1_ERR_ST |\
					    ISP_IRQ_STATUS2X_TG1_GBERR |\
					    ISP_IRQ_STATUS2X_CQ0_ERR |\
					    ISP_IRQ_STATUS2X_CQ0_VS_ERR_ST |\
					    ISP_IRQ_STATUS2X_IMGO_DROP_FRAME_ST |\
					    ISP_IRQ_STATUS2X_RRZO_DROP_FRAME_ST |\
					    ISP_IRQ_STATUS2X_IMGO_ERR_ST |\
					    ISP_IRQ_STATUS2X_AAO_ERR_ST |\
					    ISP_IRQ_STATUS2X_LCSO_ERR_ST |\
					    ISP_IRQ_STATUS2X_RRZO_ERR_ST |\
					    ISP_IRQ_STATUS2X_AFO_ERR_ST |\
					    ISP_IRQ_STATUS2X_LSC_ERR_ST |\
					    ISP_IRQ_STATUS2X_DMA_ERR_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_STATUS2X_WAITQ  (0x0)

/* CAM_CTL_INT_STATUS3X */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUS3X       (ISP_IRQ_STATUS3X_IMGO_DONE_ST |\
					    ISP_IRQ_STATUS3X_UFEO_DONE_ST |\
					    ISP_IRQ_STATUS3X_RRZO_DONE_ST |\
					    ISP_IRQ_STATUS3X_ESFKO_DONE_ST |\
					    ISP_IRQ_STATUS3X_LCSO_DONE_ST |\
					    ISP_IRQ_STATUS3X_AAO_DONE_ST |\
					    ISP_IRQ_STATUS3X_BPCI_DONE_ST |\
					    ISP_IRQ_STATUS3X_LSCI_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMGO_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_RRZO_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_AFO_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_LCSO_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_AAO_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_BPCI_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_LCSI_D_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMGI_DONE_ST |\
					    ISP_IRQ_STATUS3X_UFDI_DONE_ST |\
					    ISP_IRQ_STATUS3X_VIPI_DONE_ST |\
					    ISP_IRQ_STATUS3X_VIP2I_DONE_ST |\
					    ISP_IRQ_STATUS3X_VIP3I_DONE_ST |\
					    ISP_IRQ_STATUS3X_LCEI_DONE_ST |\
					    ISP_IRQ_STATUS3X_MFBO_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMG2O_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMG3O_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMG3BO_DONE_ST |\
					    ISP_IRQ_STATUS3X_IMG3CO_DONE_ST |\
					    ISP_IRQ_STATUS3X_FEO_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUS3X_ERR   (0X0)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_STATUS3X_WAITQ  (0x0)


/* SENINF1_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF1       (0X0)
#define ISP_REG_MASK_INT_SENINF1_ERR   (SENINF1_IRQ_OVERRUN_IRQ_STA |\
					   SENINF1_IRQ_CRCERR_IRQ_STA |\
					   SENINF1_IRQ_FSMERR_IRQ_STA |\
					   SENINF1_IRQ_VSIZEERR_IRQ_STA |\
					   SENINF1_IRQ_HSIZEERR_IRQ_STA |\
					   SENINF1_IRQ_SENSOR_VSIZEERR_IRQ_STA |\
					   SENINF1_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_SENINF1_WAITQ  (0x0)

/* SENINF2_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF2       (0X0)
#define ISP_REG_MASK_INT_SENINF2_ERR   (SENINF2_IRQ_OVERRUN_IRQ_STA |\
					   SENINF1_IRQ_CRCERR_IRQ_STA |\
					   SENINF2_IRQ_FSMERR_IRQ_STA |\
					   SENINF2_IRQ_VSIZEERR_IRQ_STA |\
					   SENINF2_IRQ_HSIZEERR_IRQ_STA |\
					   SENINF2_IRQ_SENSOR_VSIZEERR_IRQ_STA |\
					   SENINF2_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_SENINF2_WAITQ  (0x0)

/* SENINF3_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF3       (0X0)
#define ISP_REG_MASK_INT_SENINF3_ERR   (SENINF3_IRQ_OVERRUN_IRQ_STA |\
					   SENINF3_IRQ_CRCERR_IRQ_STA |\
					   SENINF3_IRQ_FSMERR_IRQ_STA |\
					   SENINF3_IRQ_VSIZEERR_IRQ_STA |\
					   SENINF3_IRQ_HSIZEERR_IRQ_STA |\
					   SENINF3_IRQ_SENSOR_VSIZEERR_IRQ_STA |\
					   SENINF3_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_SENINF3_WAITQ  (0x0)

/* SENINF4_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF4       (0X0)
#define ISP_REG_MASK_INT_SENINF4_ERR   (SENINF4_IRQ_OVERRUN_IRQ_STA |\
					   SENINF4_IRQ_CRCERR_IRQ_STA |\
					   SENINF4_IRQ_FSMERR_IRQ_STA |\
					   SENINF4_IRQ_VSIZEERR_IRQ_STA |\
					   SENINF4_IRQ_HSIZEERR_IRQ_STA |\
					   SENINF4_IRQ_SENSOR_VSIZEERR_IRQ_STA |\
					   SENINF4_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue or not */
#define ISP_REG_MASK_INT_SENINF4_WAITQ  (0x0)

#define ISP_REG_MASK_CAMSV_ST          (ISP_IRQ_CAMSV_STATUS_VS1_ST |\
					    ISP_IRQ_CAMSV_STATUS_TG_ST1 |\
					    ISP_IRQ_CAMSV_STATUS_TG_ST2 |\
					    ISP_IRQ_CAMSV_STATUS_EXPDON1_ST |\
					    ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST |\
					    ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_CAMSV_ST_ERR     (ISP_IRQ_CAMSV_STATUS_TG_ERR_ST |\
					    ISP_IRQ_CAMSV_STATUS_TG_GBERR_ST |\
					    ISP_IRQ_CAMSV_STATUS_TG_DROP_ST |\
					    ISP_IRQ_CAMSV_STATUS_IMGO_ERR_ST |\
					    ISP_IRQ_CAMSV_STATUS_IMGO_OVERR_ST |\
					    ISP_IRQ_CAMSV_STATUS_IMGO_DROP_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_CAMSV_ST_WAITQ    (ISP_IRQ_CAMSV_STATUS_VS1_ST |\
					    ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST |\
					    ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST)

#define ISP_REG_MASK_CAMSV2_ST          (ISP_IRQ_CAMSV2_STATUS_VS1_ST |\
					    ISP_IRQ_CAMSV2_STATUS_TG_ST1 |\
					    ISP_IRQ_CAMSV2_STATUS_TG_ST2 |\
					    ISP_IRQ_CAMSV2_STATUS_EXPDON1_ST |\
					    ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST |\
					    ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_CAMSV2_ST_ERR     (ISP_IRQ_CAMSV2_STATUS_TG_ERR_ST |\
					    ISP_IRQ_CAMSV2_STATUS_TG_GBERR_ST |\
					    ISP_IRQ_CAMSV2_STATUS_TG_DROP_ST |\
					    ISP_IRQ_CAMSV2_STATUS_IMGO_ERR_ST |\
					    ISP_IRQ_CAMSV2_STATUS_IMGO_OVERR_ST |\
					    ISP_IRQ_CAMSV2_STATUS_IMGO_DROP_ST)
/* if we service wait queue or not */
#define ISP_REG_MASK_CAMSV2_ST_WAITQ    (ISP_IRQ_CAMSV2_STATUS_VS1_ST |\
					    ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST |\
					    ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST)

static volatile MINT32 EDBufQueRemainNodeCnt;	/* record remain node count(success/fail) excludes head when enque/deque control */

static volatile wait_queue_head_t WaitQueueHead_EDBuf_WaitDeque;
static volatile wait_queue_head_t WaitQueueHead_EDBuf_WaitFrame;
static spinlock_t SpinLockEDBufQueList;
#define _MAX_SUPPORT_P2_FRAME_NUM_ 512
#define _MAX_SUPPORT_P2_BURSTQ_NUM_ 4
static volatile MINT32 P2_Support_BurstQNum = 1;
#define _MAX_SUPPORT_P2_PACKAGE_NUM_ (_MAX_SUPPORT_P2_FRAME_NUM_/_MAX_SUPPORT_P2_BURSTQ_NUM_)
#define P2_EDBUF_MLIST_TAG 1
#define P2_EDBUF_RLIST_TAG 2
typedef struct {
	volatile MUINT32 processID;	/* caller process ID */
	volatile MUINT32 callerID;	/* caller thread ID */
	volatile MINT32 p2dupCQIdx;	/* p2 duplicate CQ index(for recognize belong to which package) */
	volatile ISP_ED_BUF_STATE_ENUM bufSts;	/* buffer status */
} ISP_EDBUF_STRUCT;
static volatile MINT32 P2_EDBUF_RList_FirstBufIdx;
static volatile MINT32 P2_EDBUF_RList_CurBufIdx;
static volatile MINT32 P2_EDBUF_RList_LastBufIdx;
static volatile ISP_EDBUF_STRUCT P2_EDBUF_RingList[_MAX_SUPPORT_P2_FRAME_NUM_];

typedef struct {
	volatile MUINT32 processID;	/* caller process ID */
	volatile MUINT32 callerID;	/* caller thread ID */
	volatile MINT32 p2dupCQIdx;	/* p2 duplicate CQ index(for recognize belong to which package) */
	volatile MINT32 dequedNum;	/* number of dequed buffer no matter deque success or fail */
} ISP_EDBUF_MGR_STRUCT;
static volatile MINT32 P2_EDBUF_MList_FirstBufIdx;
/* static volatile MINT32 P2_EDBUF_MList_CurBufIdx=0; */
static volatile MINT32 P2_EDBUF_MList_LastBufIdx;
static volatile ISP_EDBUF_MGR_STRUCT P2_EDBUF_MgrList[_MAX_SUPPORT_P2_PACKAGE_NUM_];

static volatile MUINT32 g_regScen = 0xa5a5a5a5;
static spinlock_t SpinLockRegScen;



/*******************************************************************************
*
********************************************************************************/
/* internal data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf;
/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc;
/*  */
static volatile ISP_RT_BUF_STRUCT *pstRTBuf;
static volatile MUINT32 m_CurrentPPB;

/* static ISP_DEQUE_BUF_INFO_STRUCT g_deque_buf = {0,{}};    // Marked to remove build warning. */

unsigned long g_Flash_SpinLock;


static volatile unsigned int G_u4EnableClockCount;

/*******************************************************************************
*
********************************************************************************/
typedef struct {
	pid_t Pid;
	pid_t Tid;
} ISP_USER_INFO_STRUCT;

/*******************************************************************************
*
********************************************************************************/
#define ISP_BUF_SIZE            (4096)
#define ISP_BUF_SIZE_WRITE      1024
#define ISP_BUF_WRITE_AMOUNT    6

typedef enum {
	ISP_BUF_STATUS_EMPTY,
	ISP_BUF_STATUS_HOLD,
	ISP_BUF_STATUS_READY
} ISP_BUF_STATUS_ENUM;

typedef struct {
	volatile ISP_BUF_STATUS_ENUM Status;
	volatile MUINT32 Size;
	MUINT8 *pData;
} ISP_BUF_STRUCT;

typedef struct {
	ISP_BUF_STRUCT Read;
	ISP_BUF_STRUCT Write[ISP_BUF_WRITE_AMOUNT];
} ISP_BUF_INFO_STRUCT;


/*******************************************************************************
*
********************************************************************************/
typedef struct {
	atomic_t HoldEnable;
	atomic_t WriteEnable;
	ISP_HOLD_TIME_ENUM Time;
} ISP_HOLD_INFO_STRUCT;

typedef struct {
	volatile MUINT32 Status[ISP_IRQ_USER_MAX][ISP_IRQ_TYPE_AMOUNT];
	MUINT32 Mask[ISP_IRQ_TYPE_AMOUNT];
	MUINT32 ErrMask[ISP_IRQ_TYPE_AMOUNT];
	MUINT32 WaitQMask[ISP_IRQ_TYPE_AMOUNT];
	MINT8 WaitQIdx[ISP_IRQ_TYPE_AMOUNT * 32];	/* index in waitQ list of irqs */
} ISP_IRQ_INFO_STRUCT;

typedef struct {
	MUINT32 Vd;
	MUINT32 Expdone;
	MUINT32 WorkQueueVd;
	MUINT32 WorkQueueExpdone;
	MUINT32 TaskletVd;
	MUINT32 TaskletExpdone;
} ISP_TIME_LOG_STRUCT;

typedef enum _eChannel {
	_PASS1 = 0,
	_PASS1_D = 1,
	_CAMSV = 2,
	_CAMSV_D = 3,
	_PASS2 = 4,
	_ChannelMax = 5,
} eChannel;

#define DMA_TRANS(dma, Out) \
do { \
    if (dma == _imgo_ || dma == _rrzo_) {\
	Out = _PASS1;\
    } \
    else if (dma == _imgo_d_ || dma == _rrzo_d_) { \
	Out = _PASS1_D;\
    } \
    else if (dma == _camsv_imgo_) {\
	Out = _CAMSV;\
    } \
    else if (dma == _camsv2_imgo_) {\
	Out = _CAMSV_D;\
    } \
    else { \
	} \
} while (0)

/* basically , should separate into p1/p1_d/p2/camsv/camsv_d, */
/* currently, only use camsv/camsv_d/others */
typedef enum _eISPIrq {
	_IRQ = 0,
	_IRQ_D = 1,
	_CAMSV_IRQ = 2,
	_CAMSV_D_IRQ = 3,
	_IRQ_MAX = 4,
} eISPIrq;

typedef enum _eLOG_TYPE {
	_LOG_DBG = 0,		/* currently, only used at ipl_buf_ctrl. to protect critical section */
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
} eLOG_TYPE;

typedef enum _eLOG_OP {
	_LOG_INIT = 0,
	_LOG_RST = 1,
	_LOG_ADD = 2,
	_LOG_PRT = 3,
	_LOG_GETCNT = 4,
	_LOG_OP_MAX = 5
} eLOG_OP;

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/*#define SV_LOG_STR_LEN NORMAL_STR_LEN*/

#define LOG_PPNUM 2
typedef struct _SV_LOG_STR {
	MUINT32 _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
} SV_LOG_STR, *PSV_LOG_STR;

static void *pLog_kmalloc;
static SV_LOG_STR gSvLog[_IRQ_MAX];
/* static SV_LOG_STR gSvLog_IRQ = {0}; */
/* static SV_LOG_STR gSvLog_CAMSV_IRQ= {0}; */
/* static SV_LOG_STR gSvLog_CAMSV_D_IRQ= {0}; */
static volatile MBOOL g_bDmaERR_p1 = MFALSE;
static volatile MBOOL g_bDmaERR_p1_d = MFALSE;
static volatile MBOOL g_bDmaERR_p2 = MFALSE;
static volatile MBOOL g_bDmaERR_deepDump = MFALSE;
static volatile UINT32 g_ISPIntErr[_IRQ_MAX] = { 0 };

#define nDMA_ERR_P1     (11)
#define nDMA_ERR_P1_D   (7)
#define nDMA_ERR    (nDMA_ERR_P1 + nDMA_ERR_P1_D)
static MUINT32 g_DmaErr_p1[nDMA_ERR] = { 0 };

/**
    for irq used,keep log until IRQ_LOG_PRINTER being involked,
    limited:
	each log must shorter than 512 bytes
	total log length in each irq/logtype can't over 1024 bytes
*/
#define IRQ_LOG_KEEPER_T(sec, usec) {\
	ktime_t time;           \
	time = ktime_get();     \
	sec = time.tv64;        \
	do_div(sec, 1000);    \
	usec = do_div(sec, 1000000);\
}
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
    char *ptr; \
    char *pDes;\
    MUINT32 *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
    unsigned int str_leng;\
    if (_LOG_ERR == logT) {\
	str_leng = NORMAL_STR_LEN*ERR_PAGE; \
    } else if (_LOG_DBG == logT) {\
	str_leng = NORMAL_STR_LEN*DBG_PAGE; \
    } else if (_LOG_INF == logT) {\
	str_leng = NORMAL_STR_LEN*INF_PAGE;\
    } else {\
	str_leng = 0;\
    } \
    ptr = pDes = (char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);    \
    sprintf((char *)(pDes), fmt, ##__VA_ARGS__);   \
    if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		LOG_ERR("log str over flow(%d)", irq);\
    } \
    while (*ptr++ != '\0') {        \
	(*ptr2)++;\
    }     \
} while (0);
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...)  printk("KEEPER", "[%s] " fmt, __func__, ##__VA_ARGS__)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
    SV_LOG_STR *pSrc = &gSvLog[irq];\
    char *ptr;\
    MUINT32 i;\
    MINT32 ppb = 0;\
    MINT32 logT = 0;\
    if (ppb_in > 1) {\
	ppb = 1;\
    } else{\
	ppb = ppb_in;\
    } \
    if (logT_in > _LOG_ERR) {\
	logT = _LOG_ERR;\
    } else{\
	logT = logT_in;\
    } \
    ptr = pSrc->_str[ppb][logT];\
    if (0 != pSrc->_cnt[ppb][logT]) {\
	if (_LOG_DBG == logT) {\
		for (i = 0; i < DBG_PAGE; i++) {\
		if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
		    ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
			LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
		} else{\
			LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
		    break;\
		} \
	    } \
	} \
	else if (_LOG_INF == logT) {\
		for (i = 0; i < INF_PAGE; i++) {\
		if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
		    ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
			LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
		} else{\
			LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
		    break;\
		} \
	    } \
	} \
	else if (_LOG_ERR == logT) {\
		for (i = 0; i < ERR_PAGE; i++) {\
		if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
		    ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
			LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
		} else{\
			LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
		    break;\
		} \
	    } \
	} \
	else {\
		LOG_ERR("N.S.%d", logT);\
	} \
	ptr[0] = '\0';\
	pSrc->_cnt[ppb][logT] = 0;\
    } \
} while (0);


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

#define SUPPORT_MAX_IRQ 32
typedef struct {
	spinlock_t SpinLockIspRef;
	spinlock_t SpinLockIsp;
	spinlock_t SpinLockIrq[_IRQ_MAX];	/* currently, IRQ and IRQ_D share the same ISR , so share the same key,IRQ. */
	spinlock_t SpinLockHold;
	spinlock_t SpinLockRTBC;
	spinlock_t SpinLockClock;
	wait_queue_head_t WaitQueueHead;
	/* wait_queue_head_t*              WaitQHeadList; */
	volatile wait_queue_head_t WaitQHeadList[SUPPORT_MAX_IRQ];
	struct work_struct ScheduleWorkVD;
	struct work_struct ScheduleWorkEXPDONE;
	MUINT32 UserCount;
	MUINT32 DebugMask;
	MINT32 IrqNum;
	ISP_IRQ_INFO_STRUCT IrqInfo;
	ISP_HOLD_INFO_STRUCT HoldInfo;
	ISP_BUF_INFO_STRUCT BufInfo;
	ISP_TIME_LOG_STRUCT TimeLog;
	ISP_CALLBACK_STRUCT Callback[ISP_CALLBACK_AMOUNT];
} ISP_INFO_STRUCT;

static ISP_INFO_STRUCT IspInfo;

volatile MUINT32 PrvAddr[_ChannelMax] = { 0 };

/**********************************************
************************************************/
#ifdef T_STAMP_2_0
#define SlowMotion  100
typedef struct {
	volatile unsigned long long T_ns;	/* 1st frame start time, accurency in us,unit in ns */
	unsigned long interval_us;	/* unit in us */
	unsigned long compensation_us;
	MUINT32 fps;
	MUINT32 fcnt;
} T_STAMP;

static T_STAMP m_T_STAMP = { 0 };
#endif

/*******************************************************************************
*
********************************************************************************/
/* test flag */
#define ISP_KERNEL_MOTIFY_SINGAL_TEST
#ifdef ISP_KERNEL_MOTIFY_SINGAL_TEST
/*** Linux signal test ***/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/siginfo.h>	/* siginfo */
#include <linux/rcupdate.h>	/* rcu_read_lock */
#include <linux/sched.h>	/* find_task_by_pid_type */
#include <linux/debugfs.h>
#include <linux/uaccess.h>

/* js_test */
#define __tcmfunc


#define SIG_TEST 44		/* we choose 44 as our signal number (real-time signals are in the range of 33 to 64) */

struct siginfo info;
struct task_struct *t;


int getTaskInfo(pid_t pid)
{
	/* send the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE;	/* this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space, */
	/* and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data */
	/* is not delivered to the user space signal handler function. */
	info.si_int = 1234;	/* real time signals may have 32 bits of data. */

	rcu_read_lock();

	t = find_task_by_vpid(pid);
	/* t = find_task_by_pid_type(PIDTYPE_PID, g_pid);  //find the task_struct associated with this pid */
	if (t == NULL) {
		LOG_DBG("no such pid");
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();

	return 0;
}

int sendSignal(void)
{
	int ret = 0;
	ret = send_sig_info(SIG_TEST, &info, t);	/* send the signal */
	if (ret < 0) {
		LOG_DBG("error sending signal");
		return ret;
	}

	return ret;
}

/*** Linux signal test ***/

#endif				/* ISP_KERNEL_MOTIFY_SINGAL_TEST */

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_MsToJiffies(MUINT32 Ms)
{
	return (Ms * HZ + 512) >> 10;
}

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_UsToJiffies(MUINT32 Us)
{
	return ((Us / 1000) * HZ + 512) >> 10;
}

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_GetIRQState(eISPIrq eIrq, MUINT32 type, MUINT32 userNumber, MUINT32 stus)
{
	MUINT32 ret;
	unsigned long flags;
	/*  */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	ret = (IspInfo.IrqInfo.Status[userNumber][type] & stus);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*  */
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_GetEDBufQueWaitDequeState(MINT32 idx)
{
	MUINT32 ret = MFALSE;
	/*  */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_RingList[idx].bufSts == ISP_ED_BUF_STATE_RUNNING) {
		ret = MTRUE;
	}
	spin_unlock(&(SpinLockEDBufQueList));
	/*  */
	return ret;
}

static inline MUINT32 ISP_GetEDBufQueWaitFrameState(MINT32 idx)
{
	MUINT32 ret = MFALSE;
	/*  */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_MgrList[idx].dequedNum == P2_Support_BurstQNum) {
		ret = MTRUE;
	}
	spin_unlock(&(SpinLockEDBufQueList));
	/*  */
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_JiffiesToMs(MUINT32 Jiffies)
{
	return (Jiffies * 1000) / HZ;
}


/*******************************************************************************
*
********************************************************************************/
static MUINT32 ISP_DumpDmaDeepDbg(void)
{
	if (g_bDmaERR_p1) {
		g_DmaErr_p1[0] = (MUINT32) ISP_RD32(ISP_ADDR + 0x356c);
		g_DmaErr_p1[1] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3570);
		g_DmaErr_p1[2] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3574);
		g_DmaErr_p1[3] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3578);
		g_DmaErr_p1[4] = (MUINT32) ISP_RD32(ISP_ADDR + 0x357C);
		g_DmaErr_p1[5] = (MUINT32) ISP_RD32(ISP_ADDR + 0x358c);
		g_DmaErr_p1[6] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3590);
		g_DmaErr_p1[7] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3594);
		g_DmaErr_p1[8] = (MUINT32) ISP_RD32(ISP_ADDR + 0x3598);
		g_DmaErr_p1[9] = (MUINT32) ISP_RD32(ISP_ADDR + 0x359c);
		g_DmaErr_p1[10] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35a0);
		LOG_ERR
		    ("IMGI:0x%x,BPCI:0x%x,LSCI=0x%x,UFDI=0x%x,LCEI=0x%x,imgo=0x%x,rrzo:0x%x,lcso:0x%x,esfko:0x%x,aao:0x%x,ufeo:0x%x",
		     g_DmaErr_p1[0], g_DmaErr_p1[1], g_DmaErr_p1[2], g_DmaErr_p1[3], g_DmaErr_p1[4],
		     g_DmaErr_p1[5], g_DmaErr_p1[6], g_DmaErr_p1[7], g_DmaErr_p1[8], g_DmaErr_p1[9],
		     g_DmaErr_p1[10]);
		g_bDmaERR_p1 = MFALSE;
	}
	if (g_bDmaERR_p1_d) {
		g_DmaErr_p1[11] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35bc);
		g_DmaErr_p1[12] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35c0);
		g_DmaErr_p1[13] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35c4);
		g_DmaErr_p1[14] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35c8);
		g_DmaErr_p1[15] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35cc);
		g_DmaErr_p1[16] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35d0);
		g_DmaErr_p1[17] = (MUINT32) ISP_RD32(ISP_ADDR + 0x35d4);
		LOG_ERR
		    ("BPCI_D:0x%x,LSCI_D:0x%x,IMGO_D=0x%x,RRZO_D=0x%x,LSCO_D=0x%x,AFO_D=0x%x,AAO_D:0x%x",
		     g_DmaErr_p1[11], g_DmaErr_p1[12], g_DmaErr_p1[13], g_DmaErr_p1[14],
		     g_DmaErr_p1[15], g_DmaErr_p1[16], g_DmaErr_p1[17]);
		g_bDmaERR_p1_d = MFALSE;
	}
#if 0
	if (g_bDmaERR_p2) {
		LOG_ERR
		    ("vipi:0x%x,VIPI:0x%x,VIP2I=0x%x,VIP3I=0x%x,MFBO=0x%x,IMG3BO=0x%x,IMG3CO:0x%x,IMG2O:0x%x,IMG3O:0x%x,FEO:0x%x",
		     ISP_RD32(ISP_ADDR + 0x3574), ISP_RD32(ISP_ADDR + 0x3580),
		     ISP_RD32(ISP_ADDR + 0x3584), ISP_RD32(ISP_ADDR + 0x3588),
		     ISP_RD32(ISP_ADDR + 0x35a4), ISP_RD32(ISP_ADDR + 0x35a8),
		     ISP_RD32(ISP_ADDR + 0x35ac), ISP_RD32(ISP_ADDR + 0x35b0),
		     ISP_RD32(ISP_ADDR + 0x35b4), ISP_RD32(ISP_ADDR + 0x35b8));
		g_bDmaERR_p2 = MFALSE;
	}

	if (g_bDmaERR_deepDump) {
		ISP_WR32((ISP_ADDR + 0x160), 0x0);
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1E);
		LOG_ERR("imgi_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x11E);
		LOG_ERR("imgi_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x21E);
		LOG_ERR("imgi_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x31E);
		LOG_ERR("imgi_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* vipi */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x41E);
		LOG_ERR("vipi_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x51E);
		LOG_ERR("vipi_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x61E);
		LOG_ERR("vipi_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x71E);
		LOG_ERR("vipi_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x81E);
		LOG_ERR("imgo_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x91E);
		LOG_ERR("imgo_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xa1E);
		LOG_ERR("imgo_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xb1E);
		LOG_ERR("imgo_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0xc1E);
		LOG_ERR("imgo_d_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xd1E);
		LOG_ERR("imgo_d_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xe1E);
		LOG_ERR("imgo_d_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xf1E);
		LOG_ERR("imgo_d_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x101E);
		LOG_ERR("rrzo_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x111E);
		LOG_ERR("rrzo_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x121E);
		LOG_ERR("rrzo_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x131E);
		LOG_ERR("rrzo_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x151E);
		LOG_ERR("rrzo_d_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x161E);
		LOG_ERR("rrzo_d_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x171E);
		LOG_ERR("rrzo_d_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_ERR("rrzo_d_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img3o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x191E);
		LOG_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1a1E);
		LOG_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1b1E);
		LOG_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img2o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1c1E);
		LOG_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1d1E);
		LOG_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1e1E);
		LOG_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1f1E);
		LOG_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		g_bDmaERR_deepDump = MFALSE;
	}
#endif

	return 0;
}

#define RegDump(start, end) {\
    MUINT32 i;\
    for (i = start; i <= end; i += 0x10) {\
	LOG_DBG("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + i), (unsigned int)ISP_RD32(ISP_ADDR + i),\
		    (unsigned int)(ISP_TPIPE_ADDR + i+0x4), (unsigned int)ISP_RD32(ISP_ADDR + i+0x4),\
		    (unsigned int)(ISP_TPIPE_ADDR + i+0x8), (unsigned int)ISP_RD32(ISP_ADDR + i+0x8),\
		    (unsigned int)(ISP_TPIPE_ADDR + i+0xc), (unsigned int)ISP_RD32(ISP_ADDR + i+0xc));\
    } \
}


static MINT32 ISP_DumpReg(void)
{
	MINT32 Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	/* spin_lock_irqsave(&(IspInfo.SpinLock), flags); */

	/* tile tool parse range */
	/* Joseph Hung (xa)#define ISP_ADDR_START  0x15004000 */
	/* #define ISP_ADDR_END    0x15006000 */
	/*  */
	/* N3D control */
	ISP_WR32((ISP_ADDR + 0x40c0), 0x746);	/* (dbg_sel is cnt1 inner) */
	LOG_DBG("[0x%08X %08X] [0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x40c0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40c0), (unsigned int)(ISP_TPIPE_ADDR + 0x40d8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));
	ISP_WR32((ISP_ADDR + 0x40c0), 0x946);	/* (dbg_sel is cnt2 inner) */
	LOG_DBG("[0x%08X %08X] [0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x40c0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40c0), (unsigned int)(ISP_TPIPE_ADDR + 0x40d8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));

	/* isp top */
	RegDump(0x0, 0x200);
	/* dump p1 dma reg */
	RegDump(0x3200, 0x3570);
	/* dump all isp dma reg */
	RegDump(0x3300, 0x3400);
	/* dump all isp dma err reg */
	RegDump(0x3560, 0x35e0);
#if 0
	g_bDmaERR_p1 = g_bDmaERR_p1_d = g_bDmaERR_p2 = g_bDmaERR_deepDump = MTRUE;
	ISP_DumpDmaDeepDbg();
#endif
	/* TG1 */
	RegDump(0x410, 0x4a0);
	/* TG2 */
	RegDump(0x2410, 0x2450);
	/* hbin */
	LOG_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4f0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x534), (unsigned int)(ISP_TPIPE_ADDR + 0x4f4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x538));
	/* LSC */
	RegDump(0x530, 0x550);
	/* awb win */
	RegDump(0x5b0, 0x5d0);
	/* ae win */
	RegDump(0x650, 0x690);
	/* af win */
	RegDump(0x6b0, 0x700);
	/* flk */
	RegDump(0x770, 0x780);
	/* rrz */
	RegDump(0x7a0, 0x7d0);
	/* eis */
	RegDump(0xdc0, 0xdf0);
	/* dmx/rmx/bmx */
	RegDump(0xe00, 0xe30);
	/* Mipi source */
	LOG_DBG("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217000),
		(unsigned int)ISP_RD32(0xf0217000), (unsigned int)(0x10217004),
		(unsigned int)ISP_RD32(0xf0217004));
	LOG_DBG("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217008),
		(unsigned int)ISP_RD32(0xf0217008), (unsigned int)(0x1021700c),
		(unsigned int)ISP_RD32(0xf021700c));
	LOG_DBG("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217010),
		(unsigned int)ISP_RD32(0xf0217010), (unsigned int)(0x10217030),
		(unsigned int)ISP_RD32(0xf0217030));

	/* seninf1 */
	LOG_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008), (unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4120, 0x4160);
	RegDump(0x4360, 0x43f0);
	/* seninf2 */
	LOG_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008), (unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4520, 0x4560);
	RegDump(0x4600, 0x4610);
	RegDump(0x4760, 0x47f0);
	/* LSC_D */
	RegDump(0x2530, 0x2550);
	/* awb_d */
	RegDump(0x25b0, 0x25d0);
	/* ae_d */
	RegDump(0x2650, 0x2690);
	/* af_d */
	RegDump(0x26b0, 0x2700);
	/* rrz_d */
	RegDump(0x27a0, 0x27d0);
	/* rmx_d/bmx_d/dmx_d */
	RegDump(0x2e00, 0x2e30);

	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x800),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x800));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x880),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x880));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x884),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x884));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x888),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x888));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x8A0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x8A0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x920),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x920));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x924),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x924));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x928),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x928));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x92C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x92C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x930),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x930));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x934),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x934));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x938),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x938));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x93C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x93C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x960),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x960));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9C4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9C4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9E4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9E8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9EC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9EC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA00));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA04),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA04));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA08),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA08));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA0C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA0C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA10));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA14),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA14));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA20));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xAA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xAA0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xACC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xACC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB00));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB04),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB04));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB08),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB08));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB0C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB0C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB10));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB14),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB14));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB18),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB18));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB1C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB1C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB20));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB44),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB44));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB48),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB48));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB4C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB4C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB50),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB50));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB54),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB54));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB58),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB58));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB5C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB5C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB60),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB60));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBAC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBBC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBC0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBC0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xC20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xC20));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCC0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCC0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCE4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCE8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCEC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCEC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCFC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCFC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD24),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD24));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD28),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD28));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD2C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD2c));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD40),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD40));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD64),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD64));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD68),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD68));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD6C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD6c));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD70),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD70));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD74),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD74));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD78),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD78));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD7C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD7C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDA4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDA8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDAC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2410),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2410));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2414),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2414));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2418),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2418));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x241C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x241C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2420),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2420));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x243C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x243C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2440),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2440));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2444),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2444));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2448),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2448));

	/* seninf3 */
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4900),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4900));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4920),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4920));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4924),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4924));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4928),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4928));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x492C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x492C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4930),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4930));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4934),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4934));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4938),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4938));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BAC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB4));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x10);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x11);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x12);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	/* seninf4 */
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D00));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D20));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D24),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D24));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D28),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D28));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D2C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D2C));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D30),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D30));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D34),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D34));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D38),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D38));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA4));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FAC));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB0));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB4));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x10);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x11);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x12);
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));

	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x35FC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x35FC));
	LOG_DBG("end MT6593");

	/*  */
	LOG_DBG("0x%08X %08X ", (unsigned int)ISP_ADDR_CAMINF,
		(unsigned int)ISP_RD32(ISP_ADDR_CAMINF));
	LOG_DBG("0x%08X %08X ", (unsigned int)(ISP_TPIPE_ADDR + 0x150),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x150));
	/*  */
	/* debug msg for direct link */


	/* mdp crop */
	LOG_DBG("MDPCROP Related");
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_ADDR + 0xd10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xd10));
	LOG_DBG("0x%08X %08X", (unsigned int)(ISP_ADDR + 0xd20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xd20));
	/* cq */
	LOG_DBG("CQ Related");
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x6000);
	LOG_DBG("0x%08X %08X (0x15004160=6000)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x7000);
	LOG_DBG("0x%08X %08X (0x15004160=7000)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x8000);
	LOG_DBG("0x%08X %08X (0x15004160=8000)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	/* imgi_debug */
	LOG_DBG("IMGI_DEBUG Related");
	ISP_WR32(IMGSYS_BASE + 0x75f4, 0x001e);
	LOG_DBG("0x%08X %08X (0x150075f4=001e)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x75f4, 0x011e);
	LOG_DBG("0x%08X %08X (0x150075f4=011e)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x75f4, 0x021e);
	LOG_DBG("0x%08X %08X (0x150075f4=021e)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x75f4, 0x031e);
	LOG_DBG("0x%08X %08X (0x150075f4=031e)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	/* yuv */
	LOG_DBG("yuv-mdp crop Related");
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x3014);
	LOG_DBG("0x%08X %08X (0x15004160=3014)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	LOG_DBG("yuv-c24b out Related");
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x301e);
	LOG_DBG("0x%08X %08X (0x15004160=301e)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x301f);
	LOG_DBG("0x%08X %08X (0x15004160=301f)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x3020);
	LOG_DBG("0x%08X %08X (0x15004160=3020)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));
	ISP_WR32(IMGSYS_BASE + 0x4160, 0x3021);
	LOG_DBG("0x%08X %08X (0x15004160=3021)", (unsigned int)(IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(IMGSYS_BASE + 0x4164));


#if 0				/* _mt6593fpga_dvt_use_ */
	{
		int tpipePA = ISP_RD32(ISP_ADDR + 0x204);
		int ctlStart = ISP_RD32(ISP_ADDR + 0x000);
		int ctlTcm = ISP_RD32(ISP_ADDR + 0x054);
		int map_va = 0, map_size;
		int i;
		int *pMapVa;
#define TPIPE_DUMP_SIZE    200

		if ((ctlStart & 0x01) && (tpipePA) && (ctlTcm & 0x80000000)) {	/* for pass2 */
			map_va = 0;
			m4u_mva_map_kernel(tpipePA, TPIPE_DUMP_SIZE, 0, &map_va, &map_size);
			pMapVa = map_va;
			LOG_DBG("pMapVa(0x%x),map_size(0x%x)", pMapVa, map_size);
			LOG_DBG("ctlStart(0x%x),tpipePA(0x%x),ctlTcm(0x%x)", ctlStart, tpipePA,
				ctlTcm);
			if (pMapVa) {
				for (i = 0; i < TPIPE_DUMP_SIZE; i += 10) {
					LOG_DBG
					    ("[idx(%d)]%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X",
					     i, pMapVa[i], pMapVa[i + 1], pMapVa[i + 2],
					     pMapVa[i + 3], pMapVa[i + 4], pMapVa[i + 5],
					     pMapVa[i + 6], pMapVa[i + 7], pMapVa[i + 8],
					     pMapVa[i + 9]);
				}
			}
			m4u_mva_unmap_kernel(tpipePA, map_size, map_va);
		}
	}
#endif

	/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
	/*  */
	LOG_DBG("- X.");
	/*  */
	return Ret;
}


/*******************************************************************************
*
********************************************************************************/
static void ISP_EnableClock(MBOOL En)
{

	if (G_u4EnableClockCount == 1) {
		LOG_DBG("- E. En: %d. G_u4EnableClockCount: %d.", En, G_u4EnableClockCount);
	}
	if (En) {		/* Enable clock. */

		/* from SY yang,,*IMG_CG_CLR = 0xffffffff; *MMSYS_CG_CLR0 = 0x00000003; *CLK_CFG_7 = *CLK_CFG_7 | 0x02000000; *CAM_CTL_CLK_EN = 0x00000009; */
		/* address map, MMSYS_CG_CLR0:0x14000108,CLK_CFG_7:0x100000b0 */
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock enbled. G_u4EnableClockCount: %d.", G_u4EnableClockCount); */
		switch (G_u4EnableClockCount) {
		case 0:
			enable_clock(MT_CG_DISP0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_CAM_SV,  "CAMERA"); */
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			break;
		default:
			break;
		}
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
	} else {		/* Disable clock. */

		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock disabled. G_u4EnableClockCount: %d.", G_u4EnableClockCount); */
		G_u4EnableClockCount--;
		switch (G_u4EnableClockCount) {
		case 0:
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			/* disable_clock(MT_CG_IMAGE_CAM_SV,  "CAMERA"); */
			/* disable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DISP0_SMI_COMMON, "CAMERA");
			break;
		default:
			break;
		}
		spin_unlock(&(IspInfo.SpinLockClock));
	}
}

/*******************************************************************************
*
********************************************************************************/
static inline void ISP_Reset(MINT32 rst_path)
{
	/* ensure the view finder is disabe. 0: take_picture */
	/* ISP_CLR_BIT(ISP_REG_ADDR_EN1, 0); */
	MUINT32 Reg;
	MUINT32 setReg;
	/* MUINT32 i, flags; */
	/*  */
	LOG_DBG("- E.");

	LOG_DBG("isp gate clk(0x%x),rst_path(%d)", ISP_RD32(ISP_ADDR_CAMINF), rst_path);


	if (rst_path == ISP_REG_SW_CTL_RST_CAM_P1) {
		/* ISP Soft SW reset process */
#if 1
		Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		setReg =
		    (Reg & (~ISP_REG_SW_CTL_SW_RST_P1_MASK)) | (ISP_REG_SW_CTL_SW_RST_TRIG &
								ISP_REG_SW_CTL_SW_RST_P1_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
		/* ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0); */
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST);

		setReg =
		    (Reg & (~ISP_REG_SW_CTL_SW_RST_P1_MASK)) | (0x00 &
								ISP_REG_SW_CTL_SW_RST_P1_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
#else
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_SW_RST_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
#endif
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAM_P2) {
#if 1
		Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		setReg =
		    (Reg & (~ISP_REG_SW_CTL_SW_RST_P2_MASK)) | (ISP_REG_SW_CTL_SW_RST_P2_TRIG &
								ISP_REG_SW_CTL_SW_RST_P2_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
		/* ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0); */
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_P2_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST_P2);

		setReg =
		    (Reg & (~ISP_REG_SW_CTL_SW_RST_P2_MASK)) | (0x00 &
								ISP_REG_SW_CTL_SW_RST_P2_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);

#else
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_SW_RST_P2_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_P2_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST_P2);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
#endif
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAMSV) {
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, ISP_REG_SW_CTL_SW_RST_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAMSV_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, ISP_REG_SW_CTL_HW_RST);
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, 0);
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAMSV2) {
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, ISP_REG_SW_CTL_SW_RST_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAMSV2_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, ISP_REG_SW_CTL_HW_RST);
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, 0);
	}
#if 0
	/* need modify here */
	for (i = 0; i < _IRQ_MAX; i++) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[i]), flags);
	}
#endif
#if 0
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ_D]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
	for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
		IspInfo.IrqInfo.Status[i] = 0;
	}
	for (i = 0; i < _ChannelMax; i++) {
		PrvAddr[i] = 0;
	}
#if 0
	for (i = 0; i < _IRQ_MAX; i++) {
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[i]), flags);
	}
#endif
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ_D]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ]), flags);
/*  */
#endif


	/*  */
	LOG_DBG("- X.");
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_ReadReg(ISP_REG_IO_STRUCT * pRegIo)
{
	MUINT32 i;
	MINT32 Ret = 0;
	/*  */
	ISP_REG_STRUCT reg;
	MUINT32 *pData = (MUINT32 *) pRegIo->Data;
	for (i = 0; i < pRegIo->Count; i++) {
		if (0 != get_user(reg.Addr, pData)) {
			LOG_ERR("get_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		pData++;
		if ((ISP_ADDR_CAMINF + reg.Addr >= ISP_ADDR)
		    && (ISP_ADDR_CAMINF + reg.Addr < (ISP_ADDR_CAMINF + ISP_RANGE))) {
			reg.Val = ISP_RD32(ISP_ADDR_CAMINF + reg.Addr);
		} else {
			LOG_ERR("Wrong address(0x%x)", (unsigned int)(ISP_ADDR_CAMINF + reg.Addr));
			reg.Val = 0;
		}
		/* printk("[KernelRDReg]addr(0x%x),value()0x%x\n",ISP_ADDR_CAMINF + reg.Addr,reg.Val); */

		if (0 != put_user(reg.Val, pData)) {
			LOG_ERR("put_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		pData++;
		/*  */
	}
	/*  */
 EXIT:
	return Ret;
}


/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_WriteRegToHw(ISP_REG_STRUCT *pReg, MUINT32 Count)
{
	MINT32 Ret = 0;
	MUINT32 i;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG) {
		LOG_DBG("- E.");
	}
	/*  */
	spin_lock(&(IspInfo.SpinLockIsp));
	for (i = 0; i < Count; i++) {
		if (IspInfo.DebugMask & ISP_DBG_WRITE_REG) {
			LOG_DBG("Addr(0x%08X), Val(0x%08X)",
				(MUINT32) (ISP_ADDR_CAMINF + pReg[i].Addr),
				(MUINT32) (pReg[i].Val));
		}
		if (((ISP_ADDR_CAMINF + pReg[i].Addr) >= ISP_ADDR)
		    && ((ISP_ADDR_CAMINF + pReg[i].Addr) < (ISP_ADDR_CAMINF + ISP_RANGE))) {
			ISP_WR32(ISP_ADDR_CAMINF + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%x)",
				(unsigned int)(ISP_ADDR_CAMINF + pReg[i].Addr));
		}
	}
	spin_unlock(&(IspInfo.SpinLockIsp));
	/*  */
	return Ret;
}

/*******************************************************************************
*
********************************************************************************
static void ISP_BufWrite_Init(void)    //Vent@20121106: Marked to remove build warning: 'ISP_BufWrite_Init' defined but not used [-Wunused-function]
{
    MUINT32 i;
    //
    if(IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
    {
	LOG_DBG("- E.");
    }
    //
    for(i=0; i<ISP_BUF_WRITE_AMOUNT; i++)
    {
	IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
	IspInfo.BufInfo.Write[i].Size = 0;
	IspInfo.BufInfo.Write[i].pData = NULL;
    }
}

*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Dump(void)
{
	MUINT32 i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		LOG_DBG("i=%d, Status=%d, Size=%d", i, IspInfo.BufInfo.Write[i].Status,
			IspInfo.BufInfo.Write[i].Size);
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		IspInfo.BufInfo.Write[i].pData = NULL;
	}
}


/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Free(void)
{
	MUINT32 i;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
		LOG_DBG("- E.");
	}
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		if (IspInfo.BufInfo.Write[i].pData != NULL) {
			kfree(IspInfo.BufInfo.Write[i].pData);
			IspInfo.BufInfo.Write[i].pData = NULL;
		}
	}
}

/*******************************************************************************
*
********************************************************************************/
static MBOOL ISP_BufWrite_Alloc(void)
{
	MUINT32 i;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
		LOG_DBG("- E.");
	}
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		IspInfo.BufInfo.Write[i].pData = (MUINT8 *) kmalloc(ISP_BUF_SIZE_WRITE, GFP_ATOMIC);
		if (IspInfo.BufInfo.Write[i].pData == NULL) {
			LOG_DBG("ERROR: i = %d, pData is NULL", i);
			ISP_BufWrite_Free();
			return false;
		}
	}
	return true;
}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Reset(void)
{
	MUINT32 i;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
		LOG_DBG("- E.");
	}
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
	}
}

/*******************************************************************************
*
********************************************************************************/
static inline MUINT32 ISP_BufWrite_GetAmount(void)
{
	MUINT32 i, Count = 0;
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_READY) {
			Count++;
		}
	}
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
		LOG_DBG("Count = %d", Count);
	}
	return Count;
}

/*******************************************************************************
*
********************************************************************************/
static MBOOL ISP_BufWrite_Add(MUINT32 Size, MUINT8 *pData)
{
	MUINT32 i;
	/*  */
	/* LOG_DBG("- E."); */
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_HOLD) {
			if ((IspInfo.BufInfo.Write[i].Size + Size) > ISP_BUF_SIZE_WRITE) {
				LOG_ERR("i = %d, BufWriteSize(%d)+Size(%d) > %d", i,
					IspInfo.BufInfo.Write[i].Size, Size, ISP_BUF_SIZE_WRITE);
				return false;
			}
			/*  */
			if (copy_from_user
			    ((MUINT8 *) (IspInfo.BufInfo.Write[i].pData +
					 IspInfo.BufInfo.Write[i].Size), (MUINT8 *) pData,
			     Size) != 0) {
				LOG_ERR("copy_from_user failed");
				return false;
			}
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
				LOG_DBG("i = %d, BufSize = %d, Size = %d", i,
					IspInfo.BufInfo.Write[i].Size, Size);
			}
			/*  */
			IspInfo.BufInfo.Write[i].Size += Size;
			return true;
		}
	}
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_EMPTY) {
			if (Size > ISP_BUF_SIZE_WRITE) {
				LOG_ERR("i = %d, Size(%d) > %d", i, Size, ISP_BUF_SIZE_WRITE);
				return false;
			}
			/*  */
			if (copy_from_user
			    ((MUINT8 *) (IspInfo.BufInfo.Write[i].pData), (MUINT8 *) pData,
			     Size) != 0) {
				LOG_ERR("copy_from_user failed");
				return false;
			}
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
				LOG_DBG("i = %d, Size = %d", i, Size);
			}
			/*  */
			IspInfo.BufInfo.Write[i].Size = Size;
			/*  */
			IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_HOLD;
			return true;
		}
	}

	/*  */
	LOG_ERR("All write buffer are full of data!");
	return false;

}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_SetReady(void)
{
	MUINT32 i;
	/*  */
	/* LOG_DBG("- E."); */
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_HOLD) {
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
				LOG_DBG("i = %d, Size = %d", i, IspInfo.BufInfo.Write[i].Size);
			}
			IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_READY;
		}
	}
}

/*******************************************************************************
*
********************************************************************************/
static MBOOL ISP_BufWrite_Get(MUINT32 * pIndex, MUINT32 *pSize, MUINT8 ** ppData)
{
	MUINT32 i;
	/*  */
	/* LOG_DBG("- E."); */
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_READY) {
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
				LOG_DBG("i = %d, Size = %d", i, IspInfo.BufInfo.Write[i].Size);
			}
			*pIndex = i;
			*pSize = IspInfo.BufInfo.Write[i].Size;
			*ppData = IspInfo.BufInfo.Write[i].pData;
			return true;
		}
	}
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
		LOG_DBG("No buf is ready!");
	}
	return false;
}

/*******************************************************************************
*
********************************************************************************/
static MBOOL ISP_BufWrite_Clear(MUINT32 Index)
{
	/*  */
	/* LOG_DBG("- E."); */
	/*  */
	if (IspInfo.BufInfo.Write[Index].Status == ISP_BUF_STATUS_READY) {
		if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
			LOG_DBG("Index = %d, Size = %d", Index, IspInfo.BufInfo.Write[Index].Size);
		}
		IspInfo.BufInfo.Write[Index].Size = 0;
		IspInfo.BufInfo.Write[Index].Status = ISP_BUF_STATUS_EMPTY;
		return true;
	} else {
		LOG_DBG("WARNING: Index(%d) is not ready! Status = %d", Index,
			IspInfo.BufInfo.Write[Index].Status);
		return false;
	}
}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_WriteToHw(void)
{
	MUINT8 *pBuf;
	MUINT32 Index, BufSize;
	/*  */
	spin_lock(&(IspInfo.SpinLockHold));
	/*  */
	LOG_DBG("- E.");
	/*  */
	while (ISP_BufWrite_Get(&Index, &BufSize, &pBuf)) {
		if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
			LOG_DBG("Index = %d, BufSize = %d ", Index, BufSize);
		}
		ISP_WriteRegToHw((ISP_REG_STRUCT *) pBuf, BufSize / sizeof(ISP_REG_STRUCT));
		ISP_BufWrite_Clear(Index);
	}
	/* LOG_DBG("No more buf."); */
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);
	wake_up_interruptible(&(IspInfo.WaitQueueHead));
	/*  */
	spin_unlock(&(IspInfo.SpinLockHold));
}


/*******************************************************************************
*
********************************************************************************/
void ISP_ScheduleWork_VD(struct work_struct *data)
{
	if (IspInfo.DebugMask & ISP_DBG_SCHEDULE_WORK) {
		LOG_DBG("- E.");
	}
	/*  */
	IspInfo.TimeLog.WorkQueueVd = ISP_JiffiesToMs(jiffies);
	/*  */
	if (IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_VD].Func != NULL) {
		IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_VD].Func();
	}
}

/*******************************************************************************
*
********************************************************************************/
void ISP_ScheduleWork_EXPDONE(struct work_struct *data)
{
	if (IspInfo.DebugMask & ISP_DBG_SCHEDULE_WORK) {
		LOG_DBG("- E.");
	}
	/*  */
	IspInfo.TimeLog.WorkQueueExpdone = ISP_JiffiesToMs(jiffies);
	/*  */
	if (IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_EXPDONE].Func != NULL) {
		IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_EXPDONE].Func();
	}
}

/*******************************************************************************
*
********************************************************************************/
void ISP_Tasklet_VD(unsigned long Param)
{
	if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
		LOG_DBG("- E.");
	}
	/*  */
	IspInfo.TimeLog.TaskletVd = ISP_JiffiesToMs(jiffies);
	/*  */
	if (IspInfo.Callback[ISP_CALLBACK_TASKLET_VD].Func != NULL) {
		IspInfo.Callback[ISP_CALLBACK_TASKLET_VD].Func();
	}
	/*  */
	if (IspInfo.HoldInfo.Time == ISP_HOLD_TIME_VD) {
		ISP_BufWrite_WriteToHw();
	}
}

DECLARE_TASKLET(IspTaskletVD, ISP_Tasklet_VD, 0);

/*******************************************************************************
*
********************************************************************************/
void ISP_Tasklet_EXPDONE(unsigned long Param)
{
	if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
		LOG_DBG("- E.");
	}
	/*  */
	IspInfo.TimeLog.TaskletExpdone = ISP_JiffiesToMs(jiffies);
	/*  */
	if (IspInfo.Callback[ISP_CALLBACK_TASKLET_EXPDONE].Func != NULL) {
		IspInfo.Callback[ISP_CALLBACK_TASKLET_EXPDONE].Func();
	}
	/*  */
	if (IspInfo.HoldInfo.Time == ISP_HOLD_TIME_EXPDONE) {
		ISP_BufWrite_WriteToHw();
	}
}

DECLARE_TASKLET(IspTaskletEXPDONE, ISP_Tasklet_EXPDONE, 0);


/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_WriteReg(ISP_REG_IO_STRUCT * pRegIo)
{
	MINT32 Ret = 0;
	MINT32 TimeVd = 0;
	MINT32 TimeExpdone = 0;
	MINT32 TimeTasklet = 0;
	MUINT8 *pData = NULL;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG) {
		LOG_DBG("Data(0x%08X), Count(%d)", (MUINT32) (pRegIo->Data),
			(MUINT32) (pRegIo->Count));
	}
	/*  */
	if (atomic_read(&(IspInfo.HoldInfo.HoldEnable))) {
		if (ISP_BufWrite_Add
		    ((pRegIo->Count) * sizeof(ISP_REG_STRUCT), (MUINT8 *) (pRegIo->Data))) {
			/* LOG_DBG("Add write buffer OK"); */
		} else {
			LOG_ERR("Add write buffer fail");
			TimeVd = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.Vd;
			TimeExpdone = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.Expdone;
			TimeTasklet = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.TaskletExpdone;
			LOG_ERR("HoldTime(%d), VD(%d ms), Expdone(%d ms), Tasklet(%d ms)",
				IspInfo.HoldInfo.Time, TimeVd, TimeExpdone, TimeTasklet);
			ISP_BufWrite_Dump();
			ISP_DumpReg();
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		pData = (MUINT8 *) kmalloc((pRegIo->Count) * sizeof(ISP_REG_STRUCT), GFP_ATOMIC);
		if (pData == NULL) {
			LOG_DBG("ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
				current->comm, current->pid, current->tgid);
			Ret = -ENOMEM;
		}
		if (copy_from_user
		    (pData, (MUINT8 *) (pRegIo->Data),
		     pRegIo->Count * sizeof(ISP_REG_STRUCT)) != 0) {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/*  */
		Ret = ISP_WriteRegToHw((ISP_REG_STRUCT *) pData, pRegIo->Count);
	}
	/*  */
 EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_SetHoldTime(ISP_HOLD_TIME_ENUM HoldTime)
{
	LOG_DBG("HoldTime(%d)", HoldTime);
	IspInfo.HoldInfo.Time = HoldTime;
	/*  */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_ResetBuf(void)
{
	LOG_DBG("- E. hold_reg(%d), BufAmount(%d)", atomic_read(&(IspInfo.HoldInfo.HoldEnable)),
		ISP_BufWrite_GetAmount());
	/*  */
	ISP_BufWrite_Reset();
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), 0);
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);
	LOG_DBG("- X.");
	return 0;
}


/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_EnableHoldReg(MBOOL En)
{
	MINT32 Ret = 0;
	MUINT32 BufAmount = 0;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_HOLD_REG) {
		LOG_DBG("En(%d), HoldEnable(%d)", En, atomic_read(&(IspInfo.HoldInfo.HoldEnable)));
	}
	/*  */
	if (!spin_trylock_bh(&(IspInfo.SpinLockHold))) {
		/* Should wait until tasklet done. */
		MINT32 Timeout;
		MINT32 IsLock = 0;
		/*  */
		if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
			LOG_DBG("Start wait ... ");
		}
		/*  */
		Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
							   (IsLock =
							    spin_trylock_bh(&
									    (IspInfo.SpinLockHold))),
							   ISP_MsToJiffies(500));
		/*  */
		if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
			LOG_DBG("End wait ");
		}
		/*  */
		if (IsLock == 0) {
			LOG_ERR("Should not happen, Timeout & IsLock is 0");
			Ret = -EFAULT;
			goto EXIT;
		}
	}
	/* Here we get the lock. */
	if (En == MFALSE) {
		ISP_BufWrite_SetReady();
		BufAmount = ISP_BufWrite_GetAmount();
		/*  */
		if (BufAmount) {
			atomic_set(&(IspInfo.HoldInfo.WriteEnable), 1);
		}
	}
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_HOLD_REG) {
		LOG_DBG("En(%d), HoldEnable(%d), BufAmount(%d)", En,
			atomic_read(&(IspInfo.HoldInfo.HoldEnable)), BufAmount);
	}
	/*  */
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), En);
	/*  */
	spin_unlock_bh(&(IspInfo.SpinLockHold));
	/*  */
 EXIT:
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static atomic_t g_imem_ref_cnt[ISP_REF_CNT_ID_MAX];
/*  */
static long ISP_REF_CNT_CTRL_FUNC(MUINT32 Param)
{
	MINT32 Ret = 0;
	ISP_REF_CNT_CTRL_STRUCT ref_cnt_ctrl;
	MINT32 imem_ref_cnt = 0;


	/* //////////////////---add lock here */
/* spin_lock_irq(&(IspInfo.SpinLock)); */
	/* ////////////////// */
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL) {
		LOG_DBG("[rc]+");
	}
	/*  */
	if (NULL == (void *)Param) {
		LOG_ERR("[rc]NULL Param");
		/* //////////////////---add unlock here */
/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
		/* ////////////////// */
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&ref_cnt_ctrl, (void *)Param, sizeof(ISP_REF_CNT_CTRL_STRUCT)) == 0) {


		if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL) {
			LOG_DBG("[rc]ctrl(%d),id(%d)", ref_cnt_ctrl.ctrl, ref_cnt_ctrl.id);
		}
		/*  */
		if (ISP_REF_CNT_ID_MAX > ref_cnt_ctrl.id) {
			/* //////////////////---add lock here */
			spin_lock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */
			/*  */
			switch (ref_cnt_ctrl.ctrl) {
			case ISP_REF_CNT_GET:
				break;
			case ISP_REF_CNT_INC:
				atomic_inc(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt++; */
				break;
			case ISP_REF_CNT_DEC:
			case ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE:
				atomic_dec(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt--; */
				break;
			default:
			case ISP_REF_CNT_MAX:	/* Add this to remove build warning. */
				/* Do nothing. */
				break;
			}
			/*  */
			imem_ref_cnt = (MINT32) atomic_read(&g_imem_ref_cnt[ref_cnt_ctrl.id]);

			if (imem_ref_cnt == 0) {
				/* No user left and ctrl is RESET_IF_LAST_ONE, do ISP reset. */
				if (ref_cnt_ctrl.ctrl == ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE
				    || ref_cnt_ctrl.ctrl ==
				    ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE) {
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P1);
					LOG_DBG("Reset P1\n");
				}

				if (ref_cnt_ctrl.ctrl == ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE
				    || ref_cnt_ctrl.ctrl ==
				    ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE) {
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);
				}
			}
			/* //////////////////---add unlock here */
			spin_unlock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */

			/*  */
			if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL) {
				LOG_DBG("[rc]ref_cnt(%d)", imem_ref_cnt);
			}
			/*  */
			if (copy_to_user
			    ((void *)ref_cnt_ctrl.data_ptr, &imem_ref_cnt, sizeof(MINT32)) != 0) {
				LOG_ERR("[rc][GET]:copy_to_user failed");
				Ret = -EFAULT;
			}
		} else {
			LOG_ERR("[rc]:id(%d) exceed", ref_cnt_ctrl.id);
			Ret = -EFAULT;
		}


	} else {
		LOG_ERR("[rc]copy_from_user failed");
		Ret = -EFAULT;
	}

	/*  */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL) {
		LOG_DBG("[rc]-");
	}
	/*  */
	/* //////////////////---add unlock here */
/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
	/* ////////////////// */
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
/* js_test */
/*  */
#ifndef _rtbc_use_cq0c_
static MUINT32 bEnqBuf;
static MUINT32 bDeqBuf;
static MINT32 rtbc_enq_dma = _rt_dma_max_;
static MINT32 rtbc_deq_dma = _rt_dma_max_;
#endif

static MUINT32 prv_tstamp_s[_rt_dma_max_] = { 0 };
static MUINT32 prv_tstamp_us[_rt_dma_max_] = { 0 };

static volatile MUINT32 sof_count[_ChannelMax] = { 0, 0, 0, 0 };
static MUINT32 start_time[_ChannelMax] = { 0, 0, 0, 0 };
static MUINT32 avg_frame_time[_ChannelMax] = { 0, 0, 0, 0 };


static int sof_pass1done[2] = { 0, 0 };

static volatile MUINT32 gSof_camsvdone[2] = { 0, 0 };
static volatile MBOOL g1stSof[4] = { MTRUE, MTRUE };
static volatile MUINT8 g1stErrhandle[_rt_dma_max_] = { 0 };	/* err handle at 1st frame if SOF&DON dome at the same time */

#ifdef _rtbc_buf_que_2_0_
typedef struct _FW_RCNT_CTRL {
	MUINT32 INC[_IRQ_MAX][ISP_RT_BUF_SIZE];	/* rcnt_in */
	MUINT32 DMA_IDX[_rt_dma_max_];	/* enque cnt */
	MUINT32 rdIdx[_IRQ_MAX];	/* enque read cnt */
	MUINT32 curIdx[_IRQ_MAX];	/* record avail rcnt pair */
	MUINT32 bLoadBaseAddr[_IRQ_MAX];
} FW_RCNT_CTRL;
static volatile FW_RCNT_CTRL mFwRcnt = { {{0} }, {0}, {0}, {0}, {0} };
static MUINT8 dma_en_recorder[_rt_dma_max_][ISP_RT_BUF_SIZE] = { {0} };
#endif
/*  */
static MINT32 ISP_RTBC_ENQUE(MINT32 dma, ISP_RT_BUF_INFO_STRUCT * prt_buf_info)
{
	MINT32 Ret = 0;
	MINT32 rt_dma = dma;
	MUINT32 buffer_exist = 0;
	MUINT32 i = 0;
	MUINT32 index = 0;

	/* check max */
	if (ISP_RT_BUF_SIZE == pstRTBuf->ring_buf[rt_dma].total_count) {
		LOG_ERR("[rtbc][ENQUE]:real time buffer number FULL:rt_dma(%d)", rt_dma);
		Ret = -EFAULT;
		/* break; */
	}
	/*  */
	/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/* check if buffer exist */
	for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
		if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == prt_buf_info->base_pAddr) {
			buffer_exist = 1;
			break;
		}
		/*  */
		if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == 0) {
			break;
		}
	}
	/*  */
	if (buffer_exist) {
		/*  */
		if (ISP_RTBC_BUF_EMPTY != pstRTBuf->ring_buf[rt_dma].data[i].bFilled) {
			pstRTBuf->ring_buf[rt_dma].data[i].bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf->ring_buf[rt_dma].empty_count++;
			index = i;
		}
		/*  */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG
			    ("[rtbc][ENQUE]::buffer_exist(%d)/i(%d)/PA(0x%x)/bFilled(%d)/empty(%d)",
			     buffer_exist, i, prt_buf_info->base_pAddr,
			     pstRTBuf->ring_buf[rt_dma].data[i].bFilled,
			     pstRTBuf->ring_buf[rt_dma].empty_count);
		}

	} else {
		/* overwrite oldest element if buffer is full */
		if (pstRTBuf->ring_buf[rt_dma].total_count == ISP_RT_BUF_SIZE) {
			LOG_ERR("[ENQUE]:[rtbc]:buffer full(%d)",
				pstRTBuf->ring_buf[rt_dma].total_count);
		} else {
			/* first time add */
			index = pstRTBuf->ring_buf[rt_dma].total_count % ISP_RT_BUF_SIZE;
			/*  */
			pstRTBuf->ring_buf[rt_dma].data[index].memID = prt_buf_info->memID;
			pstRTBuf->ring_buf[rt_dma].data[index].size = prt_buf_info->size;
			pstRTBuf->ring_buf[rt_dma].data[index].base_vAddr =
			    prt_buf_info->base_vAddr;
			pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr =
			    prt_buf_info->base_pAddr;
			pstRTBuf->ring_buf[rt_dma].data[index].bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf->ring_buf[rt_dma].data[index].bufIdx = prt_buf_info->bufIdx;
			/*  */
			pstRTBuf->ring_buf[rt_dma].total_count++;
			pstRTBuf->ring_buf[rt_dma].empty_count++;
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG
				    ("[rtbc][ENQUE]:dma(%d),index(%d),bufIdx(0x%x),PA(0x%x)/empty(%d)/total(%d)",
				     rt_dma, index, prt_buf_info->bufIdx,
				     pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr,
				     pstRTBuf->ring_buf[rt_dma].empty_count,
				     pstRTBuf->ring_buf[rt_dma].total_count);
			}
		}
	}
	/*  */

	/* count ==1 means DMA stalled already or NOT start yet */
	if (1 == pstRTBuf->ring_buf[rt_dma].empty_count) {
		if (_imgo_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_rrzo_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_RRZO_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_imgo_d_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMGO_D_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_rrzo_d_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_RRZO_D_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_camsv_imgo_ == rt_dma) {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_camsv2_imgo_ == rt_dma) {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		}

		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG
			    ("[rtbc][ENQUE]:dma(%d),base_pAddr(0x%x)/imgo(0x%x)/rrzo(0x%x)/imgo_d(0x%x)/rrzo_d(0x%x)/empty_count(%d) ",
			     rt_dma, pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr,
			     ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
			     ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
			     ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR),
			     ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR),
			     pstRTBuf->ring_buf[rt_dma].empty_count);

		}
#if defined(_rtbc_use_cq0c_)
		/* Do nothing */
#else
		MUINT32 reg_val = 0;

		/* disable FBC control to go on download */
		if (_imgo_ == rt_dma) {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_FBC, reg_val);
		} else {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC, reg_val);
		}
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("[rtbc][ENQUE]:dma(%d),disable fbc:IMGO(0x%x),IMG2O(0x%x)", rt_dma,
				ISP_RD32(ISP_REG_ADDR_IMGO_FBC), ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC));
		}
#endif
		pstRTBuf->ring_buf[rt_dma].pre_empty_count = pstRTBuf->ring_buf[rt_dma].empty_count;

	}
	/*  */
	/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
		LOG_DBG
		    ("[rtbc][ENQUE]:dma:(%d),start(%d),index(%d),empty_count(%d),base_pAddr(0x%x)",
		     rt_dma, pstRTBuf->ring_buf[rt_dma].start, index,
		     pstRTBuf->ring_buf[rt_dma].empty_count,
		     pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
	}
	/*  */
	return Ret;
}

static void ISP_FBC_DUMP(MUINT32 dma_id, MUINT32 VF_1, MUINT32 VF_2, MUINT32 VF_3, MUINT32 VF_4)
{
	MUINT32 z;
	char str[128];
	char str2[_rt_dma_max_];
	MUINT32 dma;
	LOG_INF("================================\n");
	LOG_INF("pass1 timeout log(timeout port:%d)", dma_id);
	LOG_INF("================================\n");
	str[0] = '\0';
	LOG_INF("current activated dmaport");
	for (z = 0; z < _rt_dma_max_; z++) {
		sprintf(str2, "%d_", pstRTBuf->ring_buf[z].active);
		strcat(str, str2);
	}
	LOG_INF("%s", str);
	LOG_INF("================================\n");
	if (VF_1) {
		LOG_INF("imgo:");
		dma = _imgo_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_IRQ][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_IRQ]);
			LOG_INF("================================\n");
		}
		LOG_INF("rrzo:");
		dma = _rrzo_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_IRQ][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_IRQ]);
			LOG_INF("================================\n");
		}
	}
	if (VF_2) {
		LOG_INF("imgo_d:");
		dma = _imgo_d_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_IRQ_D][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ_D]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_IRQ_D]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_IRQ_D]);
			LOG_INF("================================\n");
		}
		LOG_INF("rrzo_d:");
		dma = _rrzo_d_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_IRQ_D][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ_D]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_IRQ_D]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_IRQ_D]);
			LOG_INF("================================\n");
		}
	}
	if (VF_3) {
		LOG_INF("camsv_imgo:");
		dma = _camsv_imgo_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_CAMSV_IRQ][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_CAMSV_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_CAMSV_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_CAMSV_IRQ]);
			LOG_INF("================================\n");
		}
	}
	if (VF_4) {
		LOG_INF("camsv2_imgo:");
		dma = _camsv2_imgo_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_CAMSV_D_IRQ][z]);
				strcat(str, str2);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_CAMSV_D_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n", mFwRcnt.curIdx[_CAMSV_D_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n", mFwRcnt.bLoadBaseAddr[_CAMSV_D_IRQ]);
			LOG_INF("================================\n");
		}
	}
}

static MINT32 ISP_RTBC_DEQUE(MINT32 dma, ISP_DEQUE_BUF_INFO_STRUCT * pdeque_buf)
{
	MINT32 Ret = 0;
	MINT32 rt_dma = dma;
	MUINT32 i = 0;
	MUINT32 index = 0;

	/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
		LOG_DBG("[rtbc][DEQUE]+");
	}
	/*  */
	pdeque_buf->count = 0;
	/* in SOF, "start" is next buffer index */
	for (i = 0; i < pstRTBuf->ring_buf[rt_dma].total_count; i++) {
		/*  */
		index =
		    (pstRTBuf->ring_buf[rt_dma].start + i) % pstRTBuf->ring_buf[rt_dma].total_count;
		/*  */
		if (ISP_RTBC_BUF_FILLED == pstRTBuf->ring_buf[rt_dma].data[index].bFilled) {
			pstRTBuf->ring_buf[rt_dma].data[index].bFilled = ISP_RTBC_BUF_LOCKED;
			pdeque_buf->count = P1_DEQUE_CNT;
			break;
		}
	}
	/*  */
	if (0 == pdeque_buf->count) {
		/* queue buffer status */
		LOG_DBG
		    ("[rtbc][DEQUE]:dma(%d),start(%d),total(%d),empty(%d), pdeque_buf->count(%d) ",
		     rt_dma, pstRTBuf->ring_buf[rt_dma].start,
		     pstRTBuf->ring_buf[rt_dma].total_count, pstRTBuf->ring_buf[rt_dma].empty_count,
		     pdeque_buf->count);
		/*  */
		for (i = 0; i <= pstRTBuf->ring_buf[rt_dma].total_count - 1; i++) {
			LOG_DBG("[rtbc][DEQUE]Buf List:%d/%d/0x%x/0x%x/0x%x/%d/  ",
				i,
				pstRTBuf->ring_buf[rt_dma].data[i].memID,
				pstRTBuf->ring_buf[rt_dma].data[i].size,
				pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr,
				pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr,
				pstRTBuf->ring_buf[rt_dma].data[i].bFilled);
		}
	}
	/*  */
	if (pdeque_buf->count) {
		/* Fill buffer head */
		/* "start" is current working index */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("[rtbc][DEQUE]:rt_dma(%d)/index(%d)/empty(%d)/total(%d)",
				rt_dma,
				index,
				pstRTBuf->ring_buf[rt_dma].empty_count,
				pstRTBuf->ring_buf[rt_dma].total_count);
		}
		/*  */
		for (i = 0; i < pdeque_buf->count; i++) {
			pdeque_buf->data[i].memID =
			    pstRTBuf->ring_buf[rt_dma].data[index + i].memID;
			pdeque_buf->data[i].size = pstRTBuf->ring_buf[rt_dma].data[index + i].size;
			pdeque_buf->data[i].base_vAddr =
			    pstRTBuf->ring_buf[rt_dma].data[index + i].base_vAddr;
			pdeque_buf->data[i].base_pAddr =
			    pstRTBuf->ring_buf[rt_dma].data[index + i].base_pAddr;
			pdeque_buf->data[i].timeStampS =
			    pstRTBuf->ring_buf[rt_dma].data[index + i].timeStampS;
			pdeque_buf->data[i].timeStampUs =
			    pstRTBuf->ring_buf[rt_dma].data[index + i].timeStampUs;
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG
				    ("[rtbc][DEQUE]:index(%d)/PA(0x%x)/memID(%d)/size(0x%x)/VA(0x%x)",
				     index + i, pdeque_buf->data[i].base_pAddr,
				     pdeque_buf->data[i].memID, pdeque_buf->data[i].size,
				     pdeque_buf->data[i].base_vAddr);
			}

		}
		/*  */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("[rtbc][DEQUE]-");
		}
		/*  */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
		/*  */
	} else {
		/*  */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
		LOG_ERR("[rtbc][DEQUE]:no filled buffer");
		Ret = -EFAULT;
	}

	return Ret;
}

#ifdef _MAGIC_NUM_ERR_HANDLING_
#define _INVALID_FRM_CNT_ 0xFFFF
#define _MAX_FRM_CNT_ 0xFF

#define _UNCERTAIN_MAGIC_NUM_FLAG_ 0x40000000
static MUINT32 m_LastMNum[_rt_dma_max_] = { 0 };	/* imgo/rrzo */

#endif
static long ISP_Buf_CTRL_FUNC(MUINT32 Param)
{
	MINT32 Ret = 0;
	MINT32 rt_dma;
	MUINT32 reg_val = 0;
	MUINT32 reg_val2 = 0;
	MUINT32 camsv_reg_cal[2] = { 0, 0 };
	MUINT32 i = 0;
	MUINT32 iBuf = 0;
	MUINT32 size = 0;
	MUINT32 bWaitBufRdy = 0;
	ISP_BUFFER_CTRL_STRUCT rt_buf_ctrl;
	MBOOL _bFlag = MTRUE;
	/* MUINT32 buffer_exist = 0; */
	CQ_RTBC_FBC p1_fbc[_rt_dma_max_];
	MUINT32 p1_fbc_reg[_rt_dma_max_];
	MUINT32 p1_dma_addr_reg[_rt_dma_max_];
	unsigned long flags;
	ISP_RT_BUF_INFO_STRUCT rt_buf_info;
	ISP_DEQUE_BUF_INFO_STRUCT deque_buf;
	eISPIrq irqT = _IRQ_MAX;
	eISPIrq irqT_Lock = _IRQ_MAX;
	MBOOL CurVF_En = MFALSE;
	/*  */
	if (NULL == pstRTBuf) {
		LOG_ERR("[rtbc]NULL pstRTBuf");
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&rt_buf_ctrl, (void *)Param, sizeof(ISP_BUFFER_CTRL_STRUCT)) == 0) {
		rt_dma = rt_buf_ctrl.buf_id;
		/*  */
		/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL) { */
		/* LOG_DBG("[rtbc]ctrl(0x%x)/buf_id(0x%x)/data_ptr(0x%x)/ex_data_ptr(0x%x)\n", \ */
		/* rt_buf_ctrl.ctrl, \ */
		/* rt_buf_ctrl.buf_id, \ */
		/* rt_buf_ctrl.data_ptr, \ */
		/* rt_buf_ctrl.ex_data_ptr); */
		/* } */
		/*  */
		if (_imgo_ == rt_dma ||
		    _rrzo_ == rt_dma ||
		    _imgo_d_ == rt_dma ||
		    _rrzo_d_ == rt_dma || _camsv_imgo_ == rt_dma || _camsv2_imgo_ == rt_dma) {

#if defined(_rtbc_use_cq0c_)
			/* do nothing */
#else				/* for camsv */
			if ((_camsv_imgo_ == rt_dma) || (_camsv2_imgo_ == rt_dma))
				_bFlag = MTRUE;
			else
				_bFlag = MFALSE;
#endif
			/*  */
			if (MTRUE == _bFlag) {
				if ((ISP_RT_BUF_CTRL_ENQUE == rt_buf_ctrl.ctrl) ||
				    (ISP_RT_BUF_CTRL_DEQUE == rt_buf_ctrl.ctrl) ||
				    (ISP_RT_BUF_CTRL_IS_RDY == rt_buf_ctrl.ctrl)) {
					/*  */
					p1_fbc[_imgo_].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
					p1_fbc[_rrzo_].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
					p1_fbc[_imgo_d_].Reg_val =
					    ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
					p1_fbc[_rrzo_d_].Reg_val =
					    ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);

					p1_fbc_reg[_imgo_] = ISP_REG_ADDR_IMGO_FBC;
					p1_fbc_reg[_rrzo_] = ISP_REG_ADDR_RRZO_FBC;
					p1_fbc_reg[_imgo_d_] = ISP_REG_ADDR_IMGO_D_FBC;
					p1_fbc_reg[_rrzo_d_] = ISP_REG_ADDR_RRZO_D_FBC;

					p1_dma_addr_reg[_imgo_] = ISP_REG_ADDR_IMGO_BASE_ADDR;
					p1_dma_addr_reg[_rrzo_] = ISP_REG_ADDR_RRZO_BASE_ADDR;
					p1_dma_addr_reg[_imgo_d_] = ISP_REG_ADDR_IMGO_D_BASE_ADDR;
					p1_dma_addr_reg[_rrzo_d_] = ISP_REG_ADDR_RRZO_D_BASE_ADDR;


					p1_fbc[_camsv_imgo_].Reg_val =
					    ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
					p1_fbc[_camsv2_imgo_].Reg_val =
					    ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);

					p1_fbc_reg[_camsv_imgo_] = ISP_REG_ADDR_CAMSV_IMGO_FBC;
					p1_fbc_reg[_camsv2_imgo_] = ISP_REG_ADDR_CAMSV2_IMGO_FBC;

					p1_dma_addr_reg[_camsv_imgo_] =
					    ISP_REG_ADDR_IMGO_SV_BASE_ADDR;
					p1_dma_addr_reg[_camsv_imgo_] =
					    ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR;
					/*  */
#if 0
					if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
						LOG_DBG
						    ("[rtbc]:ctrl(%d),o(0x%x),zo(0x%x),camsv(0x%x/0x%x)",
						     rt_buf_ctrl.ctrl, p1_fbc[_imgo_].Reg_val,
						     p1_fbc[_rrzo_].Reg_val,
						     p1_fbc[_camsv_imgo_].Reg_val,
						     p1_fbc[_camsv2_imgo_].Reg_val);
					}
#endif
				}
			}
		} else {
#ifdef _rtbc_buf_que_2_0_
			if (rt_buf_ctrl.ctrl != ISP_RT_BUF_CTRL_DMA_EN)
#endif
			{
				LOG_ERR("[rtbc]invalid dma channel(%d)", rt_dma);
				return -EFAULT;
			}
		}
		/*  */
		switch (rt_buf_ctrl.ctrl) {
		case ISP_RT_BUF_CTRL_ENQUE:
/* case ISP_RT_BUF_CTRL_EXCHANGE_ENQUE: */
			/*  */
			if (copy_from_user
			    (&rt_buf_info, (void *)rt_buf_ctrl.data_ptr,
			     sizeof(ISP_RT_BUF_INFO_STRUCT)) == 0) {
				reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
				reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
				camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON);
				camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON);
				/* VF start already */
				/* MBOOL CurVF_En = MFALSE; */
				if ((_imgo_ == rt_dma) || (_rrzo_ == rt_dma)) {
					if (reg_val & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if ((_imgo_d_ == rt_dma) || (_rrzo_d_ == rt_dma)) {
					if (reg_val2 & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if (_camsv_imgo_ == rt_dma) {
					if (camsv_reg_cal[0] & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if (_camsv2_imgo_ == rt_dma) {
					if (camsv_reg_cal[1] & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				}

				if (CurVF_En) {
					if (_bFlag == MTRUE) {
						MUINT32 ch_imgo = 0, ch_rrzo = 0;
						/*  */
						switch (rt_dma) {
						case _imgo_:
						case _rrzo_:
							irqT = _IRQ;
							ch_imgo = _imgo_;
							ch_rrzo = _rrzo_;
							irqT_Lock = _IRQ;
							break;
						case _imgo_d_:
						case _rrzo_d_:
							irqT = _IRQ_D;
							ch_imgo = _imgo_d_;
							ch_rrzo = _rrzo_d_;
							irqT_Lock = _IRQ;
							break;
						case _camsv_imgo_:
							irqT_Lock = _CAMSV_IRQ;
							irqT = _CAMSV_IRQ;
							break;
						case _camsv2_imgo_:
							irqT_Lock = _CAMSV_D_IRQ;
							irqT = _CAMSV_D_IRQ;
							break;
						default:
							irqT_Lock = _IRQ;
							irqT = _IRQ;
							LOG_ERR("[rtbc]N.S.(%d)\n", rt_dma);
							break;
						}
#if 0
						static MUINT32 RTBC_DBG_test;
						if (RTBC_DBG_test++ > 3) {
							RTBC_DBG_test -= 3;
							ISP_FBC_DUMP(rt_dma, 1, 0, 0, 0);
						}
#endif
						spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]),
								  flags);
						if (0 != rt_buf_ctrl.ex_data_ptr) {
							/* borrow deque_buf.data memory , in order to shirnk memory required,avoid compile err */
							if (copy_from_user
							    (&deque_buf.data[0],
							     (void *)rt_buf_ctrl.ex_data_ptr,
							     sizeof(ISP_RT_BUF_INFO_STRUCT)) == 0) {
								/*  */
								i = 0;
								if (deque_buf.data[0].bufIdx !=
								    0xFFFF) {
									/* replace the specific buffer with the same bufIdx */
									/* LOG_ERR("[rtbc][replace2]Search By Idx"); */
									for (i = 0;
									     i < ISP_RT_BUF_SIZE;
									     i++) {
										if (pstRTBuf->ring_buf[rt_dma].data[i].bufIdx == deque_buf.data[0].bufIdx) {
											break;
										}
									}
								} else {
									/*  */
									/* LOG_ERR("[rtbc][replace2]Search By Addr+"); */
									for (i = 0;
									     i < ISP_RT_BUF_SIZE;
									     i++) {
										if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == rt_buf_info.base_pAddr) {
											/* LOG_ERR("[rtbc][replace2]Search By Addr i[%d]", i); */
											break;
										}
									}
								}

								if (i == ISP_RT_BUF_SIZE) {
									/* error: can't search the buffer... */
									LOG_ERR
									    ("[rtbc][replace2]error Can't get the idx-(0x%x)/Addr(0x%x) buf\n",
									     deque_buf.
									     data[0].bufIdx,
									     rt_buf_info.base_pAddr);
									spin_unlock_irqrestore(&
											       (IspInfo.SpinLockIrq
												[irqT_Lock]),
											       flags);
									IRQ_LOG_PRINTER(irqT, 0,
											_LOG_DBG);

									for (i = 0;
									     i < ISP_RT_BUF_SIZE;
									     i += 4) {
										LOG_ERR
										    ("[rtbc][replace2]error idx-(0x%x/0x%x/0x%x/0x%x)\n",
										     pstRTBuf->ring_buf
										     [rt_dma].data[i
												   +
												   0].
										     bufIdx,
										     pstRTBuf->ring_buf
										     [rt_dma].data[i
												   +
												   1].
										     bufIdx,
										     pstRTBuf->ring_buf
										     [rt_dma].data[i
												   +
												   2].
										     bufIdx,
										     pstRTBuf->ring_buf
										     [rt_dma].data[i
												   +
												   3].bufIdx);
									}
									return -EFAULT;
								}
								{
									/*  */
									{
										/* LOG_DBG("[rtbc]dma(%d),old(%d) PA(0x%x) VA(0x%x)",rt_dma,i,pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr,pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr); */
										IRQ_LOG_KEEPER(irqT,
											       0,
											       _LOG_DBG,
											       "[rtbc][replace2]dma(%d),idx(%d) PA(0x%x_0x%x)\n",
											       rt_dma,
											       i,
											       pstRTBuf->ring_buf
											       [rt_dma].data
											       [i].base_pAddr,
											       deque_buf.data
											       [0].base_pAddr);
										/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags); */
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].memID =
										    deque_buf.data
										    [0].memID;
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].size =
										    deque_buf.data
										    [0].size;
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].base_pAddr =
										    deque_buf.data
										    [0].base_pAddr;
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].base_vAddr =
										    deque_buf.data
										    [0].base_vAddr;
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].bFilled =
										    ISP_RTBC_BUF_EMPTY;
										pstRTBuf->ring_buf
										    [rt_dma].data
										    [i].
										    image.frm_cnt =
										    _INVALID_FRM_CNT_;

#ifdef _rtbc_buf_que_2_0_
										if (pstRTBuf->ring_buf[rt_dma].empty_count < pstRTBuf->ring_buf[rt_dma].total_count)
											pstRTBuf->ring_buf
											    [rt_dma].empty_count++;
										else {
											spin_unlock_irqrestore
											    (&
											     (IspInfo.SpinLockIrq
											      [irqT_Lock]),
											     flags);
											IRQ_LOG_PRINTER
											    (irqT,
											     0,
											     _LOG_DBG);
											LOG_ERR
											    ("[rtbc]dma(%d),PA(0x%x),over enque",
											     rt_dma,
											     pstRTBuf->ring_buf
											     [rt_dma].data
											     [i].base_pAddr);
											return
											    -EFAULT;
										}
										/* LOG_INF("RTBC_DBG7 e_dma_%d:%d %d %d\n",rt_dma,pstRTBuf->ring_buf[rt_dma].data[0].bFilled,pstRTBuf->ring_buf[rt_dma].data[1].bFilled,pstRTBuf->ring_buf[rt_dma].data[2].bFilled); */
#else
										pstRTBuf->ring_buf
										    [rt_dma].empty_count++;
#endif
										/* spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags); */
									}
								}
							}
						} else {	/* this case for camsv & pass1 fw rtbc */
							for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
								/*  */
								if (pstRTBuf->
								    ring_buf[rt_dma].data[i].
								    base_pAddr ==
								    rt_buf_info.base_pAddr) {
									/* LOG_DBG("[rtbc]dma(%d),old(%d) PA(0x%x) VA(0x%x)",rt_dma,i,pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr,pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr); */
									/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags); */
									pstRTBuf->
									    ring_buf[rt_dma].data
									    [i].bFilled =
									    ISP_RTBC_BUF_EMPTY;
									pstRTBuf->
									    ring_buf[rt_dma].data
									    [i].image.frm_cnt =
									    _INVALID_FRM_CNT_;
#ifdef _rtbc_buf_que_2_0_
									if (pstRTBuf->ring_buf
									    [rt_dma].empty_count <
									    pstRTBuf->ring_buf
									    [rt_dma].total_count)
										pstRTBuf->ring_buf
										    [rt_dma].empty_count++;
									else {
										spin_unlock_irqrestore
										    (&
										     (IspInfo.SpinLockIrq
										      [irqT_Lock]),
										     flags);
										IRQ_LOG_PRINTER
										    (irqT, 0,
										     _LOG_DBG);
										LOG_ERR
										    ("[rtbc]error:dma(%d),PA(0x%x),over enque",
										     rt_dma,
										     pstRTBuf->ring_buf
										     [rt_dma].data
										     [i].base_pAddr);
										return -EFAULT;
									}

									/* double check */
									if (1) {
										if (rt_buf_info.bufIdx != pstRTBuf->ring_buf[rt_dma].data[i].bufIdx) {
											LOG_ERR
											    ("[rtbc][replace2]error: BufIdx MisMatch. 0x%x/0x%x",
											     rt_buf_info.bufIdx,
											     pstRTBuf->ring_buf
											     [rt_dma].data
											     [i].bufIdx);
										}
									}
#else
									pstRTBuf->
									    ring_buf
									    [rt_dma].empty_count++;
#endif
									/* spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags); */
									/* LOG_DBG("[rtbc]dma(%d),new(%d) PA(0x%x) VA(0x%x)",rt_dma,i,pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr,pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr); */
									break;
								}
							}
						}
						/* set RCN_INC = 1; */
						/* RCNT++ */
						/* FBC_CNT-- */


						/* RCNT_INC++ */
						{
#ifdef _rtbc_buf_que_2_0_
							/* make sure rct_inc will be pulled at the same vd. */
							MBOOL bImmediate = MFALSE;
							MUINT32 z;

							if ((_IRQ == irqT) || (_IRQ_D == irqT)) {
								if ((MTRUE ==
								     pstRTBuf->
								     ring_buf[ch_imgo].active)
								    && (MTRUE ==
									pstRTBuf->
									ring_buf[ch_rrzo].active)) {
									if (0 !=
									    rt_buf_ctrl.ex_data_ptr)
									{
										if ((p1_fbc
										     [rt_dma].Bits.
										     FB_NUM ==
										     p1_fbc
										     [rt_dma].Bits.
										     FBC_CNT)
										    ||
										    ((p1_fbc
										      [rt_dma].
										      Bits.FB_NUM -
										      1) ==
										     p1_fbc
										     [rt_dma].Bits.FBC_CNT))
										{
											mFwRcnt.bLoadBaseAddr
											    [irqT] =
											    MTRUE;
										}
									}
									dma_en_recorder[rt_dma]
									    [mFwRcnt.DMA_IDX
									     [rt_dma]] = MTRUE;
									mFwRcnt.DMA_IDX[rt_dma] =
									    (++mFwRcnt.DMA_IDX
									     [rt_dma] >=
									     ISP_RT_BUF_SIZE)
									    ? (mFwRcnt.DMA_IDX
									       [rt_dma] -
									       ISP_RT_BUF_SIZE)
									    : (mFwRcnt.DMA_IDX
									       [rt_dma]);
									/* LOG_INF("RTBC_DBG1:%d %d %d\n",rt_dma,dma_en_recorder[rt_dma][mFwRcnt.DMA_IDX[rt_dma]],mFwRcnt.DMA_IDX[rt_dma]); */
									for (z = 0;
									     z < ISP_RT_BUF_SIZE;
									     z++) {
										if (dma_en_recorder
										    [ch_imgo]
										    [mFwRcnt.rdIdx
										     [irqT]]
										    &&
										    dma_en_recorder
										    [ch_rrzo]
										    [mFwRcnt.rdIdx
										     [irqT]]) {
											mFwRcnt.INC
											    [irqT]
											    [mFwRcnt.curIdx[irqT]++]
											    = 1;
											dma_en_recorder
											    [ch_imgo]
											    [mFwRcnt.rdIdx[irqT]]
											    =
											    dma_en_recorder
											    [ch_rrzo]
											    [mFwRcnt.rdIdx[irqT]]
											    =
											    MFALSE;
											mFwRcnt.rdIdx
											    [irqT] =
											    (++mFwRcnt.rdIdx
											     [irqT]
											     >=
											     ISP_RT_BUF_SIZE)
											    ?
											    (mFwRcnt.rdIdx
											     [irqT]
											     -
											     ISP_RT_BUF_SIZE)
											    :
											    (mFwRcnt.rdIdx
											     [irqT]);
											/* LOG_INF("RTBC_DBG2:%d %d\n",mFwRcnt.rdIdx[irqT],mFwRcnt.curIdx[irqT]); */
										} else {
											break;
										}
									}
								} else {
									/* rcnt_sync only work when multi-dma ch enabled. but in order to support multi-enque, these mech. also to be */
									/* worked under 1 dma ch enabled */
#if 0
									bImmediate = MTRUE;
#else
									if (MTRUE ==
									    pstRTBuf->ring_buf
									    [rt_dma].active) {
										if (0 !=
										    rt_buf_ctrl.ex_data_ptr)
										{
											if ((p1_fbc
											     [rt_dma].Bits.FB_NUM
											     ==
											     p1_fbc
											     [rt_dma].Bits.FBC_CNT)
											    ||
											    ((p1_fbc
											      [rt_dma].Bits.FB_NUM
											      -
											      1) ==
											     p1_fbc
											     [rt_dma].Bits.FBC_CNT))
											{

												mFwRcnt.bLoadBaseAddr
												    [irqT]
												    =
												    MTRUE;
											}
										}
										dma_en_recorder
										    [rt_dma]
										    [mFwRcnt.DMA_IDX
										     [rt_dma]] =
										    MTRUE;
										mFwRcnt.DMA_IDX
										    [rt_dma]
										    =
										    (++mFwRcnt.DMA_IDX
										     [rt_dma]
										     >=
										     ISP_RT_BUF_SIZE)
										    ?
										    (mFwRcnt.DMA_IDX
										     [rt_dma] -
										     ISP_RT_BUF_SIZE)
										    :
										    (mFwRcnt.DMA_IDX
										     [rt_dma]);

										for (z = 0;
										     z <
										     ISP_RT_BUF_SIZE;
										     z++) {
											if (dma_en_recorder[rt_dma][mFwRcnt.rdIdx[irqT]]) {
												mFwRcnt.INC
												    [irqT]
												    [mFwRcnt.curIdx[irqT]++]
												    =
												    1;
												dma_en_recorder
												    [rt_dma]
												    [mFwRcnt.rdIdx[irqT]]
												    =
												    MFALSE;
												mFwRcnt.rdIdx
												    [irqT]
												    =
												    (++mFwRcnt.rdIdx
												     [irqT]
												     >=
												     ISP_RT_BUF_SIZE)
												    ?
												    (mFwRcnt.rdIdx
												     [irqT]
												     -
												     ISP_RT_BUF_SIZE)
												    :
												    (mFwRcnt.rdIdx
												     [irqT]);
											} else {
												break;
											}
										}
									} else {
										bImmediate = MTRUE;
									}
#endif
								}
							} else {	/* camsv case */
								if (MTRUE ==
								    pstRTBuf->
								    ring_buf[rt_dma].active) {
									if (0 !=
									    rt_buf_ctrl.ex_data_ptr)
									{
										if ((p1_fbc
										     [rt_dma].Bits.
										     FB_NUM ==
										     p1_fbc
										     [rt_dma].Bits.
										     FBC_CNT)
										    ||
										    ((p1_fbc
										      [rt_dma].
										      Bits.FB_NUM -
										      1) ==
										     p1_fbc
										     [rt_dma].Bits.FBC_CNT))
										{
											mFwRcnt.bLoadBaseAddr
											    [irqT] =
											    MTRUE;
										}
									}
									dma_en_recorder[rt_dma]
									    [mFwRcnt.DMA_IDX
									     [rt_dma]] = MTRUE;
									mFwRcnt.DMA_IDX[rt_dma] =
									    (++mFwRcnt.DMA_IDX
									     [rt_dma] >=
									     ISP_RT_BUF_SIZE)
									    ? (mFwRcnt.DMA_IDX
									       [rt_dma] -
									       ISP_RT_BUF_SIZE)
									    : (mFwRcnt.DMA_IDX
									       [rt_dma]);

									for (z = 0;
									     z < ISP_RT_BUF_SIZE;
									     z++) {
										if (dma_en_recorder
										    [rt_dma]
										    [mFwRcnt.rdIdx
										     [irqT]]) {
											mFwRcnt.INC
											    [irqT]
											    [mFwRcnt.curIdx[irqT]++]
											    = 1;
											dma_en_recorder
											    [rt_dma]
											    [mFwRcnt.rdIdx[irqT]]
											    =
											    MFALSE;
											mFwRcnt.rdIdx
											    [irqT] =
											    (++mFwRcnt.rdIdx
											     [irqT]
											     >=
											     ISP_RT_BUF_SIZE)
											    ?
											    (mFwRcnt.rdIdx
											     [irqT]
											     -
											     ISP_RT_BUF_SIZE)
											    :
											    (mFwRcnt.rdIdx
											     [irqT]);
										} else {
											break;
										}
									}
								} else {
									bImmediate = MTRUE;
								}
							}

							if (MTRUE == bImmediate) {
								/* if ( FB_NUM==FBC_CNT ||  (FB_NUM-1)==FBC_CNT ) */
								if ((p1_fbc[rt_dma].Bits.FB_NUM ==
								     p1_fbc[rt_dma].Bits.FBC_CNT)
								    ||
								    ((p1_fbc[rt_dma].Bits.FB_NUM -
								      1) ==
								     p1_fbc[rt_dma].Bits.FBC_CNT)) {
									/* write to phy register */
									/* LOG_INF("[rtbc_%d][ENQUE] write2Phy directly(%d,%d)",rt_dma,p1_fbc[rt_dma].Bits.FB_NUM,p1_fbc[rt_dma].Bits.FBC_CNT); */
									IRQ_LOG_KEEPER(irqT, 0,
										       _LOG_DBG,
										       "[rtbc_%d][ENQUE] write2Phy directly(%d,%d)\n",
										       rt_dma,
										       p1_fbc
										       [rt_dma].Bits.
										       FB_NUM,
										       p1_fbc
										       [rt_dma].Bits.FBC_CNT);
									ISP_WR32(p1_dma_addr_reg
										 [rt_dma],
										 pstRTBuf->ring_buf
										 [rt_dma].data
										 [i].base_pAddr);
								}
								if ((_camsv_imgo_ == rt_dma)
								    || (_camsv2_imgo_ == rt_dma)) {
									p1_fbc[rt_dma].
									    Bits.RCNT_INC = 1;
									ISP_WR32(p1_fbc_reg[rt_dma],
										 p1_fbc
										 [rt_dma].Reg_val);
									p1_fbc[rt_dma].
									    Bits.RCNT_INC = 0;
									ISP_WR32(p1_fbc_reg[rt_dma],
										 p1_fbc
										 [rt_dma].Reg_val);
								} else {
									p1_fbc[rt_dma].
									    Bits.RCNT_INC = 1;
									ISP_WR32(p1_fbc_reg[rt_dma],
										 p1_fbc
										 [rt_dma].Reg_val);
								}
							}
#else
/* if ( FB_NUM==FBC_CNT ||  (FB_NUM-1)==FBC_CNT ) */
							if ((p1_fbc[rt_dma].Bits.FB_NUM ==
							     p1_fbc[rt_dma].Bits.FBC_CNT)
							    || ((p1_fbc[rt_dma].Bits.FB_NUM - 1) ==
								p1_fbc[rt_dma].Bits.FBC_CNT)) {
								/* write to phy register */
								/* LOG_INF("[rtbc_%d][ENQUE] write2Phy directly(%d,%d)",rt_dma,p1_fbc[rt_dma].Bits.FB_NUM,p1_fbc[rt_dma].Bits.FBC_CNT); */
								IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
									       "[rtbc_%d][ENQUE] write2Phy directly(%d,%d)\n",
									       rt_dma,
									       p1_fbc[rt_dma].
									       Bits.FB_NUM,
									       p1_fbc[rt_dma].
									       Bits.FBC_CNT);
								ISP_WR32(p1_dma_addr_reg[rt_dma],
									 pstRTBuf->
									 ring_buf[rt_dma].data[i].
									 base_pAddr);
							}
/* patch camsv hw bug */
							if ((_camsv_imgo_ == rt_dma)
							    || (_camsv2_imgo_ == rt_dma)) {
								p1_fbc[rt_dma].Bits.RCNT_INC = 1;
								ISP_WR32(p1_fbc_reg[rt_dma],
									 p1_fbc[rt_dma].Reg_val);
								p1_fbc[rt_dma].Bits.RCNT_INC = 0;
								ISP_WR32(p1_fbc_reg[rt_dma],
									 p1_fbc[rt_dma].Reg_val);
							} else {
								p1_fbc[rt_dma].Bits.RCNT_INC = 1;
								ISP_WR32(p1_fbc_reg[rt_dma],
									 p1_fbc[rt_dma].Reg_val);
							}
#endif
						}
						spin_unlock_irqrestore(&
								       (IspInfo.SpinLockIrq
									[irqT_Lock]), flags);
						IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
						/*  */
						if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
							/* LOG_DBG("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),ZO(0x%x),O_D(0x%x),ZO_D(0x%x),camsv(0x%x/0x%x)fps(%d/%d/%d/%d)us", */
							LOG_DBG
							    ("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),ZO(0x%x),O_D(0x%x),ZO_D(0x%x),camsv(0x%x/0x%x)fps(%d/%d/%d/%d)us\n",
							     rt_dma, rt_buf_info.base_pAddr,
							     ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
							     ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_D_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_RRZO_D_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
							     avg_frame_time[_PASS1],
							     avg_frame_time[_PASS1_D],
							     avg_frame_time[_CAMSV],
							     avg_frame_time[_CAMSV_D]);
						}

					}
				} else {
					ISP_RTBC_ENQUE(rt_dma, &rt_buf_info);
				}

			} else {
				LOG_ERR("[rtbc][ENQUE]:copy_from_user fail");
				return -EFAULT;
			}
			break;

		case ISP_RT_BUF_CTRL_DEQUE:
			switch (rt_dma) {
			case _camsv_imgo_:
				irqT_Lock = _CAMSV_IRQ;
				irqT = _CAMSV_IRQ;
				break;
			case _camsv2_imgo_:
				irqT_Lock = _CAMSV_D_IRQ;
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			}
			/*  */
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
			reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
			camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON);
			camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON);
			/* VF start already */
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			if ((reg_val & 0x01) || (reg_val2 & 0x01) || (camsv_reg_cal[0] & 0x01)
			    || (camsv_reg_cal[1] & 0x01)) {
				if (MTRUE == _bFlag) {
					deque_buf.count = P1_DEQUE_CNT;
#ifdef _rtbc_buf_que_2_0_
					iBuf = pstRTBuf->ring_buf[rt_dma].read_idx;	/* p1_fbc[rt_dma].Bits.WCNT - 1;    //WCNT = [1,2,..] */
					pstRTBuf->ring_buf[rt_dma].read_idx =
					    (pstRTBuf->ring_buf[rt_dma].read_idx +
					     1) % pstRTBuf->ring_buf[rt_dma].total_count;
					if (deque_buf.count != P1_DEQUE_CNT) {
						LOG_ERR("support only deque 1 buf at 1 time\n");
						deque_buf.count = P1_DEQUE_CNT;
					}
#else
					iBuf = p1_fbc[rt_dma].Bits.RCNT - 1;	/* RCNT = [1,2,3,...] */
#endif
					for (i = 0; i < deque_buf.count; i++) {
						MUINT32 out;

						deque_buf.data[i].memID =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf + i].memID;
						deque_buf.data[i].size =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf + i].size;
						deque_buf.data[i].base_vAddr =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].base_vAddr;
						deque_buf.data[i].base_pAddr =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].base_pAddr;
						deque_buf.data[i].timeStampS =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].timeStampS;
						deque_buf.data[i].timeStampUs =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].timeStampUs;
						deque_buf.data[i].image.w =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.w;
						deque_buf.data[i].image.h =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.h;
						deque_buf.data[i].image.xsize =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.xsize;
						deque_buf.data[i].image.stride =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.stride;
						deque_buf.data[i].image.fmt =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.fmt;
						deque_buf.data[i].image.pxl_id =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.pxl_id;
						deque_buf.data[i].image.wbn =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.wbn;
						deque_buf.data[i].image.ob =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.ob;
						deque_buf.data[i].image.lsc =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.lsc;
						deque_buf.data[i].image.rpg =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].image.rpg;
						deque_buf.data[i].image.m_num_0 =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].
						    image.m_num_0;
						deque_buf.data[i].image.frm_cnt =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].
						    image.frm_cnt;
						deque_buf.data[i].rrzInfo.srcX =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.srcX;
						deque_buf.data[i].rrzInfo.srcY =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.srcY;
						deque_buf.data[i].rrzInfo.srcW =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.srcW;
						deque_buf.data[i].rrzInfo.srcH =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.srcH;
						deque_buf.data[i].rrzInfo.dstW =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.dstW;
						deque_buf.data[i].rrzInfo.dstH =
						    pstRTBuf->ring_buf[rt_dma].data[iBuf +
										    i].rrzInfo.dstH;

#ifdef _MAGIC_NUM_ERR_HANDLING_

						/*LOG_ERR("[rtbc][deque][m_num]:d(%d),fc(0x%x),lfc0x%x,m0(0x%x),lm#(0x%x)\n", \
						   rt_dma, \
						   deque_buf.data[i].image.frm_cnt, \
						   m_LastFrmCnt[rt_dma] \
						   ,deque_buf.data[i].image.m_num_0, \
						   m_LastMNum[rt_dma]);
						 */

						if ((_INVALID_FRM_CNT_ ==
						     deque_buf.data[i].image.frm_cnt)
						    || (m_LastMNum[rt_dma] >
							deque_buf.data[i].image.m_num_0)) {
							/*  */
							deque_buf.data[i].image.m_num_0 |=
							    _UNCERTAIN_MAGIC_NUM_FLAG_;
							/*  */
							IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
								       "m# uncertain:dma(%d),m0(0x%x),fcnt(0x%x),Lm#(0x%x)",
								       rt_dma,
								       deque_buf.data[i].
								       image.m_num_0,
								       deque_buf.data[i].
								       image.frm_cnt,
								       m_LastMNum[rt_dma]);
#ifdef T_STAMP_2_0
							if (m_T_STAMP.fps > SlowMotion) {	/*patch here is because of that uncertain should happen only in missing SOF. And because of FBC, image still can be deque. That's why  timestamp still need to be increased here. */
								m_T_STAMP.T_ns +=
								    ((unsigned long long)m_T_STAMP.
								     interval_us * 1000);
								if (++m_T_STAMP.fcnt ==
								    m_T_STAMP.fps) {
									m_T_STAMP.fcnt = 0;
									m_T_STAMP.T_ns +=
									    ((unsigned long long)
									     m_T_STAMP.
									     compensation_us *
									     1000);
								}
							}
#endif
						} else {
							m_LastMNum[rt_dma] =
							    deque_buf.data[i].image.m_num_0;
						}

#endif

						DMA_TRANS(rt_dma, out);
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].bFilled =
						    ISP_RTBC_BUF_LOCKED;
						deque_buf.sof_cnt = sof_count[out];
						deque_buf.img_cnt =
						    pstRTBuf->ring_buf[rt_dma].img_cnt;
						spin_unlock_irqrestore(&
								       (IspInfo.SpinLockIrq
									[irqT_Lock]), flags);
						IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
						/* LOG_INF("RTBC_DBG7 d_dma_%d:%d %d %d\n",rt_dma,pstRTBuf->ring_buf[rt_dma].data[0].bFilled,pstRTBuf->ring_buf[rt_dma].data[1].bFilled,pstRTBuf->ring_buf[rt_dma].data[2].bFilled); */
						if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
							LOG_DBG
							    ("[rtbc][DEQUE](%d):d(%d)/id(0x%x)/bs(0x%x)/va(0x%x)/pa(0x%x)/t(%d.%d)/img(%d,%d,%d,%d,%d,%d,%d,%d)/m(0x%x)/fc(%d)/rrz(%d,%d,%d,%d,%d,%d),lm#(0x%x)",
							     iBuf + i, rt_dma,
							     deque_buf.data[i].memID,
							     deque_buf.data[i].size,
							     deque_buf.data[i].base_vAddr,
							     deque_buf.data[i].base_pAddr,
							     deque_buf.data[i].timeStampS,
							     deque_buf.data[i].timeStampUs,
							     deque_buf.data[i].image.w,
							     deque_buf.data[i].image.h,
							     deque_buf.data[i].image.stride,
							     deque_buf.data[i].image.fmt,
							     deque_buf.data[i].image.wbn,
							     deque_buf.data[i].image.ob,
							     deque_buf.data[i].image.lsc,
							     deque_buf.data[i].image.rpg,
							     deque_buf.data[i].image.m_num_0,
							     deque_buf.data[i].image.frm_cnt,
							     deque_buf.data[i].rrzInfo.srcX,
							     deque_buf.data[i].rrzInfo.srcY,
							     deque_buf.data[i].rrzInfo.srcW,
							     deque_buf.data[i].rrzInfo.srcH,
							     deque_buf.data[i].rrzInfo.dstW,
							     deque_buf.data[i].rrzInfo.dstH,
							     m_LastMNum[rt_dma]);


							/*  */
#if 0
							LOG_DBG
							    ("[rtbc][DEQUE]:D(%d),TStamp\"%d.%06d\",o(0x%08x),zo(0x%08x),i(%d),VA(0x%x),PA(0x%x),O(0x%x),2O(0x%x)",
							     rt_dma, deque_buf.data[i].timeStampS,
							     deque_buf.data[i].timeStampUs,
							     p1_fbc[_imgo_], p1_fbc[_rrzo_],
							     iBuf + i, deque_buf.data[i].base_vAddr,
							     deque_buf.data[i].base_pAddr,
							     ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
							     ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR));

							LOG_DBG
							    ("[rtbc][DEQUE]:o_d(0x%08x),zo_d(0x%08x),O_D(0x%x),2O_D(0x%x)",
							     p1_fbc[_imgo_d_], p1_fbc[_rrzo_d_],
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_D_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_RRZO_D_BASE_ADDR));
							LOG_DBG
							    ("[rtbc][DEQUE]:camsv(0x%x/0x%x),camsv_imgo(0x%x/0x%x)",
							     p1_fbc[_camsv_imgo_],
							     p1_fbc[_camsv2_imgo_],
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
							     ISP_RD32
							     (ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR));
#endif
						}
						/*  */
						/* tstamp = deque_buf.data[i].timeStampS*1000000+deque_buf.data[i].timeStampUs; */
						/* if ( (0 != prv_tstamp) && (prv_tstamp >= tstamp) ) { */
						if (0 != prv_tstamp_s[rt_dma]) {
							if ((prv_tstamp_s[rt_dma] >
							     deque_buf.data[i].timeStampS)
							    ||
							    ((prv_tstamp_s[rt_dma] ==
							      deque_buf.data[i].timeStampS)
							     && (prv_tstamp_us[rt_dma] >=
								 deque_buf.data[i].timeStampUs))) {
								LOG_ERR
								    ("[rtbc]TS rollback,D(%d),prv\"%d.%06d\",cur\"%d.%06d\"",
								     rt_dma, (int)prv_tstamp_s,
								     (int)prv_tstamp_us,
								     deque_buf.data[i].timeStampS,
								     deque_buf.data[i].timeStampUs);
							}
						}
						prv_tstamp_s[rt_dma] = deque_buf.data[i].timeStampS;
						prv_tstamp_us[rt_dma] =
						    deque_buf.data[i].timeStampUs;
					}

#if 0
					LOG_DBG
					    ("+LARB in DEQUE,BWL(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X),220(0x%08X)/(0x%08X),0x14(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X)",
					     ISP_RD32(0xF0202000 + 0x204),
					     ISP_RD32(0xF0202000 + 0x20c),
					     ISP_RD32(0xF0202000 + 0x210),
					     ISP_RD32(0xF0202000 + 0x214),
					     ISP_RD32(0xF0202000 + 0x220),
					     ISP_RD32(0xF0202000 + 0x230),
					     ISP_RD32(SMI_LARB0 + 0x10), ISP_RD32(SMI_LARB1 + 0x10),
					     ISP_RD32(SMI_LARB2 + 0x10), ISP_RD32(SMI_LARB3 + 0x10),
					     ISP_RD32(SMI_LARB4 + 0x10));
#endif
				}
			} else {
				ISP_RTBC_DEQUE(rt_dma, &deque_buf);
			}

			if (deque_buf.count) {
				/*  */
				if (copy_to_user
				    ((void *)rt_buf_ctrl.data_ptr, &deque_buf,
				     sizeof(ISP_DEQUE_BUF_INFO_STRUCT)) != 0) {
					LOG_ERR("[rtbc][DEQUE]:copy_to_user failed");
					Ret = -EFAULT;
				}

			} else {
				/*  */
				/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
				LOG_ERR("[rtbc][DEQUE]:no filled buffer");
				Ret = -EFAULT;
			}

			break;
		case ISP_RT_BUF_CTRL_CUR_STATUS:
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON) & 0x1;
			reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON) & 0x1;
			camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON) & 0x1;
			camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON) & 0x1;
			ISP_FBC_DUMP(rt_buf_ctrl.buf_id, reg_val, reg_val2, camsv_reg_cal[0],
				     camsv_reg_cal[1]);
			break;
		case ISP_RT_BUF_CTRL_IS_RDY:
			/*  */
			/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
			/*  */
			bWaitBufRdy = 1;
#ifdef _rtbc_buf_que_2_0_
			switch (rt_dma) {
			case _imgo_:
			case _rrzo_:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			case _imgo_d_:
			case _rrzo_d_:
				irqT = _IRQ_D;
				irqT_Lock = _IRQ;
				break;
			case _camsv_imgo_:
				irqT_Lock = _CAMSV_IRQ;
				irqT = _CAMSV_IRQ;
				break;
			case _camsv2_imgo_:
				irqT_Lock = _CAMSV_D_IRQ;
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				LOG_ERR("[rtbc]N.S.(%d)\n", rt_dma);
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			}

			spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);

			if (ISP_RTBC_BUF_FILLED ==
			    pstRTBuf->ring_buf[rt_dma].data[pstRTBuf->ring_buf[rt_dma].
							    read_idx].bFilled) {
				bWaitBufRdy = 0;
			}
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				MUINT32 z;
				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
					       "cur dma:%d,read idx = %d,total cnt = %d,bWaitBufRdy= %d\n",
					       rt_dma, pstRTBuf->ring_buf[rt_dma].read_idx,
					       pstRTBuf->ring_buf[rt_dma].total_count, bWaitBufRdy);
				for (z = 0; z < pstRTBuf->ring_buf[rt_dma].total_count; z++) {
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "%d_",
						       pstRTBuf->ring_buf[rt_dma].data[z].bFilled);
				}
				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "\n");
			}
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
#else
#if defined(_rtbc_use_cq0c_)
			bWaitBufRdy = p1_fbc[rt_dma].Bits.FBC_CNT ? 0 : 1;
#else
			bWaitBufRdy = MTRUE;
#endif
#endif

			/*  */
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
			/*  */
			if (copy_to_user
			    ((void *)rt_buf_ctrl.data_ptr, &bWaitBufRdy, sizeof(MUINT32)) != 0) {
				LOG_ERR("[rtbc][IS_RDY]:copy_to_user failed");
				Ret = -EFAULT;
			}
			/*  */
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC), flags); */
			/*  */
			break;
		case ISP_RT_BUF_CTRL_GET_SIZE:
			/*  */
			size = pstRTBuf->ring_buf[rt_dma].total_count;
			/*  */
			/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL) { */
			/* LOG_DBG("[rtbc][GET_SIZE]:rt_dma(%d)/size(%d)",rt_dma,size); */
			/* } */
			if (copy_to_user((void *)rt_buf_ctrl.data_ptr, &size, sizeof(MUINT32)) != 0) {
				LOG_ERR("[rtbc][GET_SIZE]:copy_to_user failed");
				Ret = -EFAULT;
			}
			break;
		case ISP_RT_BUF_CTRL_CLEAR:
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_INF("[rtbc][CLEAR]:rt_dma(%d)", rt_dma);
			}
			/*  */
			switch (rt_dma) {
			case _imgo_:
			case _rrzo_:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			case _imgo_d_:
			case _rrzo_d_:
				irqT = _IRQ_D;
				irqT_Lock = _IRQ;
				break;
			case _camsv_imgo_:
				irqT_Lock = _CAMSV_IRQ;
				irqT = _CAMSV_IRQ;
				break;
			case _camsv2_imgo_:
				irqT_Lock = _CAMSV_D_IRQ;
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				LOG_ERR("[rtbc][CLEAR]N.S.(%d)\n", rt_dma);
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				return -EFAULT;
			}
			/* remove, cause clear will be involked only when current module r totally stopped */
			/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */

#if 0
			pstRTBuf->ring_buf[rt_dma].total_count = 0;
			pstRTBuf->ring_buf[rt_dma].start = 0;
			pstRTBuf->ring_buf[rt_dma].empty_count = 0;
			pstRTBuf->ring_buf[rt_dma].active = 0;

			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr ==
				    rt_buf_info.base_pAddr) {
					buffer_exist = 1;
					break;
				}
				/*  */
				if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == 0) {
					break;
				}
			}
#else
			/* if ((_imgo_ == rt_dma)||(_rrzo_ == rt_dma)||(_imgo_d_ == rt_dma)||(_rrzo_d_ == rt_dma)) */
			/* active */
			pstRTBuf->ring_buf[rt_dma].active = MFALSE;
			memset((char *)&pstRTBuf->ring_buf[rt_dma], 0x00,
			       sizeof(ISP_RT_RING_BUF_INFO_STRUCT));
			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				pstRTBuf->ring_buf[rt_dma].data[i].image.frm_cnt =
				    _INVALID_FRM_CNT_;
			}
			memset((char *)&prv_tstamp_s[rt_dma], 0x0, sizeof(MUINT32));
			memset((char *)&prv_tstamp_us[rt_dma], 0x0, sizeof(MUINT32));
#ifdef _rtbc_buf_que_2_0_
			memset((void *)dma_en_recorder[rt_dma], 0,
			       sizeof(MUINT8) * ISP_RT_BUF_SIZE);
			mFwRcnt.DMA_IDX[rt_dma] = 0;
#endif

			{
				unsigned int ii = 0;
				MUINT32 out[4] = { _IRQ_MAX, _IRQ_MAX, _IRQ_MAX, _IRQ_MAX };
				if ((pstRTBuf->ring_buf[_imgo_].active == MFALSE)
				    && (pstRTBuf->ring_buf[_rrzo_].active == MFALSE))
					out[0] = _IRQ;
				if ((pstRTBuf->ring_buf[_imgo_d_].active == MFALSE)
				    && (pstRTBuf->ring_buf[_rrzo_d_].active == MFALSE))
					out[1] = _IRQ_D;
				if (pstRTBuf->ring_buf[_camsv_imgo_].active == MFALSE)
					out[2] = _CAMSV_IRQ;
				if (pstRTBuf->ring_buf[_camsv2_imgo_].active == MFALSE)
					out[3] = _CAMSV_D_IRQ;

				for (ii = 0; ii < 4; ii++) {
					if (out[ii] != _IRQ_MAX) {
						sof_count[out[ii]] = 0;
						start_time[out[ii]] = 0;
						avg_frame_time[out[ii]] = 0;
						g1stSof[out[ii]] = MTRUE;
						PrvAddr[out[ii]] = 0;
						g_ISPIntErr[out[ii]] = 0;
#ifdef _rtbc_buf_que_2_0_
						mFwRcnt.bLoadBaseAddr[out[ii]] = 0;
						mFwRcnt.curIdx[out[ii]] = 0;
						memset((void *)mFwRcnt.INC[out[ii]], 0,
						       sizeof(MUINT32) * ISP_RT_BUF_SIZE);
						mFwRcnt.rdIdx[out[ii]] = 0;
#endif
#ifdef T_STAMP_2_0
						if (out[ii] == _IRQ) {
							memset((char *)&m_T_STAMP, 0x0,
							       sizeof(T_STAMP));
						}
#endif
					}
				}
				for (ii = 0; ii < _rt_dma_max_; ii++) {
					if (pstRTBuf->ring_buf[ii].active) {
						break;
					}
				}

				if (ii == _rt_dma_max_) {
					pstRTBuf->dropCnt = 0;
					pstRTBuf->state = 0;
				}
			}

#ifdef _MAGIC_NUM_ERR_HANDLING_
			m_LastMNum[rt_dma] = 0;
			LOG_INF("[js_test][rtbc][CLEAR]:rt_dma(%d)", rt_dma);
#endif

#endif
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */

			break;
#ifdef _rtbc_buf_que_2_0_
		case ISP_RT_BUF_CTRL_DMA_EN:
			{
				MUINT8 array[_rt_dma_max_];
				if (copy_from_user
				    (array, (void *)rt_buf_ctrl.data_ptr,
				     sizeof(UINT8) * _rt_dma_max_) == 0) {
					MUINT32 z;
					for (z = 0; z < _rt_dma_max_; z++) {
						pstRTBuf->ring_buf[z].active = array[z];
						if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
							LOG_INF("[rtbc][DMA_EN]:dma_%d:%d", z,
								array[z]);
						}
					}
				} else {
					LOG_ERR("[rtbc][DMA_EN]:copy_from_user failed");
					Ret = -EFAULT;
				}
			}
			break;
#endif
		case ISP_RT_BUF_CTRL_MAX:	/* Add this to remove build warning. */
			/* Do nothing. */
			break;
		default:
			break;
		}
		/*  */
	} else {
		LOG_ERR("[rtbc]copy_from_user failed");
		Ret = -EFAULT;
	}

	return Ret;
}



#if 0
/**
    rrzo/imgo/rrzo_d/imgo_d have hw cq, if lost p1 done, need to add start index inorder to match HW CQ
    camsv have no hw cq, it will refer to WCNT at SOF. WCNT have no change when no p1_done, so start index no need to change.
*/
static MINT32 ISP_LostP1Done_ErrHandle(MUINT32 dma)
{
	switch (dma) {
	case _imgo_:
	case _rrzo_:
	case _imgo_d_:
	case _rrzo_d_:
		pstRTBuf->ring_buf[dma].start++;
		pstRTBuf->ring_buf[dma].start =
		    pstRTBuf->ring_buf[dma].start % pstRTBuf->ring_buf[dma].total_count;
		break;
	default:
		break;
	}
}
#endif
static MINT32 ISP_SOF_Buf_Get(eISPIrq irqT, unsigned long long sec, unsigned long usec, MBOOL bDrop)
{
#if defined(_rtbc_use_cq0c_)

	CQ_RTBC_FBC imgo_fbc;
	CQ_RTBC_FBC rrzo_fbc;
	MUINT32 imgo_idx = 0;	/* (imgo_fbc.Bits.WCNT+imgo_fbc.Bits.FB_NUM-1)%imgo_fbc.Bits.FB_NUM; //[0,1,2,...] */
	MUINT32 rrzo_idx = 0;	/* (img2o_fbc.Bits.WCNT+img2o_fbc.Bits.FB_NUM-1)%img2o_fbc.Bits.FB_NUM; //[0,1,2,...] */
	MUINT32 curr_pa = 0;
	MUINT32 ch_imgo, ch_rrzo;
	MUINT32 i = 0;
	MUINT32 _dma_cur_fw_idx = 0;
	MUINT32 _dma_cur_hw_idx = 0;
	MUINT32 _working_dma = 0;
	MUINT32 out = 0;

	if (_IRQ == irqT) {
		imgo_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
		rrzo_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
		ch_imgo = _imgo_;
		ch_rrzo = _rrzo_;
		curr_pa = ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR);
		i = _PASS1;
	} else {		/* _IRQ_D */
		imgo_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
		rrzo_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);
		ch_imgo = _imgo_d_;
		ch_rrzo = _rrzo_d_;
		curr_pa = ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR);
		i = _PASS1_D;
	}

	if (MTRUE == g1stSof[irqT]) {	/* 1st frame of streaming */
		pstRTBuf->ring_buf[ch_imgo].start = imgo_fbc.Bits.WCNT - 1;
		pstRTBuf->ring_buf[ch_rrzo].start = rrzo_fbc.Bits.WCNT - 1;
		/* move to below because of 1st sof&done errhandle */
		/* g1stSof[irqT] = MFALSE; */
	}
	/*  */
#if 0				/* this can't be trusted , because rcnt_in is pull high at sof */
	/* No drop */
	if (imgo_fbc.Bits.FB_NUM != imgo_fbc.Bits.FBC_CNT) {
		pstRTBuf->dropCnt = 0;
	}
	/* dropped */
	else {
		pstRTBuf->dropCnt = 1;
	}
#else
	pstRTBuf->dropCnt = bDrop;
#endif
	/*  */
	/* if(IspInfo.DebugMask & ISP_DBG_INT_2) { */
	/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"[rtbc]dropCnt(%d)\n",pstRTBuf->dropCnt); */
	/* } */
	/* No drop */
	if (0 == pstRTBuf->dropCnt) {

		/* verify write buffer */

		/* if(PrvAddr[i] == curr_pa) */
		/* { */
		/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"PrvAddr:Last(0x%x) == Cur(0x%x)\n",PrvAddr[i],curr_pa); */
		/* ISP_DumpReg(); */
		/* } */
		PrvAddr[i] = curr_pa;
#ifdef _rtbc_buf_que_2_0_
		imgo_idx = pstRTBuf->ring_buf[ch_imgo].start;
		rrzo_idx = pstRTBuf->ring_buf[ch_rrzo].start;
		/* dynamic dma port ctrl */
		if (pstRTBuf->ring_buf[ch_imgo].active) {
			_dma_cur_fw_idx = imgo_idx;
			_dma_cur_hw_idx = imgo_fbc.Bits.WCNT - 1;
			_working_dma = ch_imgo;
		} else if (pstRTBuf->ring_buf[ch_rrzo].active) {
			_dma_cur_fw_idx = rrzo_idx;
			_dma_cur_hw_idx = rrzo_fbc.Bits.WCNT - 1;
			_working_dma = ch_rrzo;
		}
		if (_dma_cur_fw_idx != _dma_cur_hw_idx) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "dma sof after done %d_%d\n",
				       _dma_cur_fw_idx, _dma_cur_hw_idx);
		}
#else
		/* last update buffer index */
		rrzo_fbc = rrzo_fbc.Bits.WCNT - 1;	/* [0,1,2,...] */
		/* curr_img2o = img2o_fbc.Bits.WCNT - 1; //[0,1,2,...] */
		imgo_idx = rrzo_idx;
#endif
		/* verify write buffer,once pass1_done lost, WCNT is untrustful. */
		if (ISP_RT_CQ0C_BUF_SIZE < pstRTBuf->ring_buf[_working_dma].total_count) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR, "buf cnt(%d)\n",
				       pstRTBuf->ring_buf[_working_dma].total_count);
			pstRTBuf->ring_buf[_working_dma].total_count = ISP_RT_CQ0C_BUF_SIZE;
		}
		/*  */
		if (curr_pa != pstRTBuf->ring_buf[_working_dma].data[_dma_cur_fw_idx].base_pAddr) {
			/*  */
			/* LOG_INF("RTBC_DBG6:0x%x_0x%x\n",curr_pa,pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].base_pAddr); */
			for (i = 0; i < pstRTBuf->ring_buf[_working_dma].total_count; i++) {
				/*  */
				if (curr_pa == pstRTBuf->ring_buf[_working_dma].data[i].base_pAddr) {
					/*  */
					if (IspInfo.DebugMask & ISP_DBG_INT_2) {
						IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
							       "[rtbc]curr:old/new(%d/%d)\n",
							       _dma_cur_fw_idx, i);
					}
					/*  */
					imgo_idx = i;
					rrzo_idx = i;
					break;
				}
			}
		}
		/* LOG_INF("RTBC_DBG3:%d_%d\n",imgo_idx,rrzo_idx); */
		/* LOG_INF("RTBC_DBG7 imgo:%d %d %d\n",pstRTBuf->ring_buf[_imgo_].data[0].bFilled,pstRTBuf->ring_buf[_imgo_].data[1].bFilled,pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
		/* LOG_INF("RTBC_DBG7 rrzo:%d %d %d\n",pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
		/*  */
		pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampUs = usec;
		pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].timeStampUs = usec;

		if (_IRQ == irqT) {
			MUINT32 _tmp = ISP_RD32(TG_REG_ADDR_GRAB_W);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.xsize =
			    ISP_RD32(ISP_INNER_REG_ADDR_IMGO_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w =
			    ((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h =
			    (ISP_RD32(ISP_INNER_REG_ADDR_IMGO_YSIZE) & 0x1FFF) + 1;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.stride =
			    ISP_RD32(ISP_INNER_REG_ADDR_IMGO_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.fmt =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0xF000) >> 12;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0x03);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0 =
			    ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt =
			    (ISP_RD32(ISP_REG_ADDR_TG_INTER_ST) & 0x00FF0000) >> 16;
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.wbn; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.ob; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.lsc; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.rpg; */
			/*  */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.xsize =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZO_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[rrzo_idx].image.w =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.h =
			    (ISP_RD32(ISP_INNER_REG_ADDR_RRZO_YSIZE) & 0x1FFF) + 1;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.stride =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZO_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.fmt =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0x30) >> 4;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.pxl_id =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0 =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_IN_IMG);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =
			    ((_tmp & 0x1FFF) -
			     pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX * 2) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =
			    (((_tmp >> 16) & 0x1FFF) -
			     pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY * 2) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_OUT_IMG);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW = _tmp & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstH =
			    (_tmp >> 16) & 0x1FFF;
			/* LOG_ERR("SOF_Buf_Get: RRZ_IN_IMG= 0x%8x", ISP_RD32(ISP_INNER_REG_ADDR_RRZ_IN_IMG), pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW, pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH); */
			/*  */

			/*  */



#if 0
#ifdef _MAGIC_NUM_ERR_HANDLING_
			LOG_ERR("[rtbc][sof0][m_num]:fc(0x%x),m0(0x%x),rrz_src(%d,%d)",
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt,
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH);
#endif

			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc]TStamp(%d.%06d),curr(%d),pa(0x%x/0x%x),cq0c(0x%x)\n",
					       pstRTBuf->ring_buf[ch_imgo].
					       data[imgo_idx].timeStampS,
					       pstRTBuf->ring_buf[ch_imgo].
					       data[imgo_idx].timeStampUs, imgo_idx,
					       pstRTBuf->ring_buf[ch_imgo].
					       data[imgo_idx].base_pAddr,
					       pstRTBuf->ring_buf[ch_rrzo].
					       data[rrzo_idx].base_pAddr,
					       ISP_RD32(ISP_ADDR + 0xB4));
			}
#endif
		} else {
			MUINT32 _tmp = ISP_RD32(TG2_REG_ADDR_GRAB_W);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.xsize =
			    ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w =
			    ((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h =
			    (ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_YSIZE) & 0x1FFF) + 1;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.stride =
			    ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.fmt =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0xF000) >> 12;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0x03);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0 =
			    ISP_RD32(ISP_REG_ADDR_TG2_MAGIC_0);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt =
			    (ISP_RD32(ISP_REG_ADDR_TG2_INTER_ST) & 0x00FF0000) >> 16;

			/*  */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.xsize =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.w =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.h =
			    (ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_YSIZE) & 0x1FFF) + 1;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.stride =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.fmt =
			    (ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0x30) >> 4;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.pxl_id =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0 =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt =
			    pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY =
			    ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_IN_IMG);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =
			    ((_tmp & 0x1FFF) -
			     (pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX) *
			     2) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =
			    ((((_tmp >> 16) & 0x1FFF)) -
			     pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY * 2) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW = _tmp & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstH =
			    (_tmp >> 16) & 0x1FFF;
			/*  */


#if 0
#ifdef _MAGIC_NUM_ERR_HANDLING_
			LOG_ERR("[rtbc][sof0][m_num]:fc(0x%x),m0(0x%x),rrz_src(%d,%d)",
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH);
#endif

			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc]TStamp(%d.%06d),curr(%d),pa(0x%x/0x%x),cq0c(0x%x)\n",
					       pstRTBuf->ring_buf[ch_imgo].
					       data[imgo_idx].timeStampS,
					       pstRTBuf->ring_buf[ch_imgo].
					       data[imgo_idx].timeStampUs, imgo_idx,
					       pstRTBuf->ring_buf[ch_imgo].
					       data[rrzo_idx].base_pAddr,
					       pstRTBuf->ring_buf[ch_rrzo].
					       data[rrzo_idx].base_pAddr,
					       ISP_RD32(ISP_ADDR + 0xB4));
			}
#endif
		}
		/*  */
	}
	/* err handle when 1st sof&done sequence err */
	if (g1stSof[irqT]) {
		if (g1stErrhandle[ch_imgo] == MTRUE) {
			g1stErrhandle[ch_imgo] = MFALSE;
			pstRTBuf->ring_buf[ch_imgo].start =
			    (pstRTBuf->ring_buf[ch_imgo].start +
			     1) % pstRTBuf->ring_buf[ch_imgo].total_count;
		}
		if (g1stErrhandle[ch_rrzo] == MTRUE) {
			g1stErrhandle[ch_rrzo] = MFALSE;
			pstRTBuf->ring_buf[ch_rrzo].start =
			    (pstRTBuf->ring_buf[ch_rrzo].start +
			     1) % pstRTBuf->ring_buf[ch_rrzo].total_count;
		}
		g1stSof[irqT] = MFALSE;
	}
	/* frame time profile */
	DMA_TRANS(ch_imgo, out);
	if (0 == start_time[out]) {
		start_time[out] = sec * 1000000 + usec;	/* us */
	} else {		/* calc once per senond */
		if (avg_frame_time[out]) {
			avg_frame_time[out] += (sec * 1000000 + usec) - avg_frame_time[out];
			avg_frame_time[out] = avg_frame_time[out] >> 1;
		} else {
			avg_frame_time[out] = (sec * 1000000 + usec) - start_time[out];
		}
	}

	sof_count[out]++;
	if (sof_count[out] > 255) {	/* for match vsync cnt */
		sof_count[out] -= 256;
	}
	pstRTBuf->state = ISP_RTBC_STATE_SOF;
#else
#ifdef _rtbc_buf_que_2_0_
#error "isp kernel define condition is conflicted"
#endif
#endif

	return 0;
}				/*  */

static MINT32 ISP_CAMSV_SOF_Buf_Get(unsigned int dma, unsigned long long sec, unsigned long usec,
				    MBOOL bDrop)
{
	CQ_RTBC_FBC camsv_fbc;
	MUINT32 camsv_imgo_idx = 0;
	MUINT32 curr_pa = 0;
	eISPIrq irqT;
	MUINT32 out;
	DMA_TRANS(dma, out);

	if (_camsv_imgo_ == dma) {
		camsv_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
		curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR);
		irqT = _CAMSV_IRQ;
	} else {
		camsv_fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);
		curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR);
		irqT = _CAMSV_D_IRQ;
	}

	if (MTRUE == g1stSof[irqT]) {	/* 1st frame of streaming */
		pstRTBuf->ring_buf[dma].start = camsv_fbc.Bits.WCNT - 1;
		/* move to below because of errhanlde of 1st sof&done sequence error */
		/* g1stSof[irqT] = MFALSE; */
	}
#if 0				/* this can't be trusted , because rcnt_in is pull high at sof */
	if (camsv_fbc.Bits.FB_NUM != camsv_fbc.Bits.FBC_CNT) {
		pstRTBuf->dropCnt = 0;
	} else {
		pstRTBuf->dropCnt = 1;
	}
#else
	pstRTBuf->dropCnt = bDrop;
#endif
	if (IspInfo.DebugMask & ISP_DBG_INT_2) {
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d dropCnt(%d)\n", dma,
			       (int)pstRTBuf->dropCnt);
	}

	/* No drop */
	if (0 == pstRTBuf->dropCnt) {
		if (PrvAddr[out] == curr_pa) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR,
				       "sv%d overlap prv(0x%x) = Cur(0x%x)\n", dma, PrvAddr[out],
				       curr_pa);
			/* ISP_DumpReg(); */
		}
		PrvAddr[out] = curr_pa;

		/* last update buffer index */
		camsv_imgo_idx = (camsv_fbc.Bits.WCNT % camsv_fbc.Bits.FB_NUM);	/* nest frame */


		if (_camsv_imgo_ == dma) {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
				 pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].base_pAddr);
		} else {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
				 pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].base_pAddr);
		}

		/*  */
		camsv_imgo_idx = (camsv_imgo_idx > 0) ? (camsv_imgo_idx - 1) : (camsv_fbc.Bits.FB_NUM - 1);	/* for current */
		if (camsv_imgo_idx != pstRTBuf->ring_buf[dma].start) {	/* theoretically, it shout not be happened( wcnt is inc. at p1_done) */
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR, "sv%d WCNT%d != start%d\n",
				       dma, camsv_fbc.Bits.WCNT, pstRTBuf->ring_buf[dma].start);
		}
		pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampUs = usec;
		/* camsv support no inner address, these informations r truely untrustful, but */
		/* because of no resize in camsv, so these r also ok. */
		if (dma == _camsv_imgo_) {
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.w =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.h =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE) & 0x1FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.stride =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_STRIDE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.fmt =
			    (ISP_RD32(ISP_REG_ADDR_CAMSV_FMT_SEL) & 0x30000);
		} else {
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.w =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.h =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE) & 0x1FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.stride =
			    (ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_STRIDE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.fmt =
			    (ISP_RD32(ISP_REG_ADDR_CAMSV2_FMT_SEL) & 0x30000);
		}

		/*  */
		if (IspInfo.DebugMask & ISP_DBG_INT_2) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				       "sv%d T(%d.%06d),cur(%d),addr(0x%x),prv(0x%x),fbc(0x%08x)\n",
				       dma, pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampS,
				       pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampUs,
				       camsv_imgo_idx,
				       pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].base_pAddr,
				       PrvAddr[out], (unsigned int)camsv_fbc.Reg_val);
		}
		/*  */
	}
	/* err handle when 1st sof&done sequence err */
	if (g1stSof[irqT]) {
		if (g1stErrhandle[dma] == MTRUE) {
			g1stErrhandle[dma] = MFALSE;
			pstRTBuf->ring_buf[dma].start =
			    (pstRTBuf->ring_buf[dma].start +
			     1) % pstRTBuf->ring_buf[dma].total_count;
		}
		g1stSof[irqT] = MFALSE;
	}

	if (0 == start_time[out]) {
		start_time[out] = sec * 1000000 + usec;	/* us */
	} else {		/* calc once per senond */
		if (avg_frame_time[out]) {
			avg_frame_time[out] += (sec * 1000000 + usec) - avg_frame_time[out];
			avg_frame_time[out] = avg_frame_time[out] >> 1;
		} else {
			avg_frame_time[out] = (sec * 1000000 + usec) - start_time[out];
		}
	}

	sof_count[out]++;

	pstRTBuf->state = ISP_RTBC_STATE_SOF;
	return 0;
}

static MINT32 ISP_CAMSV_DONE_Buf_Time(unsigned int dma, unsigned long long sec, unsigned long usec)
{
	unsigned int curr;
	unsigned int reg_fbc;
	eISPIrq irqT;
	/* MUINT32 loopCount = 0; */
	CQ_RTBC_FBC fbc;
	MUINT32 _tmp;
	MUINT32 out = 0;

	/*  */
	if (_camsv_imgo_ == dma) {
		reg_fbc = ISP_REG_ADDR_CAMSV_IMGO_FBC;
		irqT = _CAMSV_IRQ;
	} else {
		reg_fbc = ISP_REG_ADDR_CAMSV2_IMGO_FBC;
		irqT = _CAMSV_D_IRQ;
	}

	/*  */
	if (0 == pstRTBuf->ring_buf[dma].empty_count) {
		/*  */
		if (IspInfo.DebugMask & ISP_DBG_INT_2) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d RTB empty,start(%d)\n",
				       dma, pstRTBuf->ring_buf[dma].start);
		}
		/* TODO: err handle */
		return -1;
	}

	curr = pstRTBuf->ring_buf[dma].start;
	fbc.Reg_val = ISP_RD32(reg_fbc);

	{			/* wcnt start at idx1, and +1 at p1_done  by hw */
		_tmp = fbc.Bits.WCNT - 1;
		_tmp = (_tmp > 0) ? (_tmp - 1) : (fbc.Bits.FB_NUM - 1);
	}
	if (curr != _tmp) {
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR, "sv%d:RTBC_%d != FBC cnt_%d\n", dma,
			       curr, _tmp);
	}

	DMA_TRANS(dma, out);
	while (1) {		/* search next start buf, basically loop 1 time only */

		if (IspInfo.DebugMask & ISP_DBG_INT_2) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d,cur(%d),bFilled(%d)\n",
				       dma, curr, pstRTBuf->ring_buf[dma].data[curr].bFilled);
		}
		/* this buf shoud be empty.If it's non-empty , maybe err in start index(timing shift) */
		if (pstRTBuf->ring_buf[dma].data[curr].bFilled == ISP_RTBC_BUF_EMPTY) {
			pstRTBuf->ring_buf[dma].data[curr].bFilled = ISP_RTBC_BUF_FILLED;
			/* start + 1 */
			pstRTBuf->ring_buf[dma].start =
			    (curr + 1) % pstRTBuf->ring_buf[dma].total_count;
			pstRTBuf->ring_buf[dma].empty_count--;
			pstRTBuf->ring_buf[dma].img_cnt = sof_count[out];
			if (g1stSof[irqT] == MTRUE) {
				LOG_ERR("Done&&Sof recieve at the same time in 1st f\n");
				g1stErrhandle[dma] = MTRUE;
			}
			break;
		} else {
			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR,
					       "sv%d:curr(%d),bFilled(%d) != EMPTY\n", dma, curr,
					       pstRTBuf->ring_buf[dma].data[curr].bFilled);
			}
			/* start + 1 */
			/* curr = (curr+1)%pstRTBuf->ring_buf[dma].total_count; */
			break;
		}
#if 0
		if (++loopCount > pstRTBuf->ring_buf[dma].total_count) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_ERR,
				       "sv%d:find no empty buf in total_count(%d)\n", dma,
				       pstRTBuf->ring_buf[dma].total_count);
			break;
		} else {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				       "sv%d:buf is not empty for current p1_done\n", dma);
		}
#endif
	}

	/*  */
	if (IspInfo.DebugMask & ISP_DBG_INT_2) {
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d:start(%d),empty(%d)\n",
			       dma,
			       pstRTBuf->ring_buf[dma].start, pstRTBuf->ring_buf[dma].empty_count);
	}
	/*  */
	/* if(IspInfo.DebugMask & ISP_DBG_INT_2) { */
	/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"sv%d:curr(%d),sec(%lld),usec(%ld)\n", dma, curr, sec, usec); */
	/* } */


	/*  */
	pstRTBuf->state = ISP_RTBC_STATE_DONE;

	return 0;
}


static MINT32 ISP_DONE_Buf_Time(eISPIrq irqT, unsigned long long sec, unsigned long usec)
{
	int i, k;
	int i_dma;
	unsigned int curr;
	/* unsigned int reg_fbc; */
	/* MUINT32 reg_val = 0; */
	MUINT32 ch_imgo, ch_rrzo, ch_imgo_fbc, ch_rrzo_fbc;
	CQ_RTBC_FBC imgo_fbc;
	CQ_RTBC_FBC rrzo_fbc;
	CQ_RTBC_FBC _dma_cur_fbc;
	MUINT32 _working_dma = 0;
#ifdef _rtbc_buf_que_2_0_
	/* for isr cb timing shift err hanlde */
	MUINT32 shiftT = 0;
	MUINT32 out;
#endif
	if (_IRQ == irqT) {
		ch_imgo = _imgo_;
		ch_rrzo = _rrzo_;
		ch_imgo_fbc = ISP_REG_ADDR_IMGO_FBC;
		ch_rrzo_fbc = ISP_REG_ADDR_RRZO_FBC;
		memset(&g_DmaErr_p1[0], 0, sizeof(MUINT32) * nDMA_ERR_P1);
	} else {
		ch_imgo = _imgo_d_;
		ch_rrzo = _rrzo_d_;
		ch_imgo_fbc = ISP_REG_ADDR_IMGO_D_FBC;
		ch_rrzo_fbc = ISP_REG_ADDR_RRZO_D_FBC;
		memset(&g_DmaErr_p1[nDMA_ERR_P1], 0, sizeof(MUINT32) * (nDMA_ERR - nDMA_ERR_P1));
	}
	imgo_fbc.Reg_val = ISP_RD32(ch_imgo_fbc);
	rrzo_fbc.Reg_val = ISP_RD32(ch_rrzo_fbc);


#ifdef _rtbc_buf_que_2_0_

	/* dynamic dma port ctrl */
	if (pstRTBuf->ring_buf[ch_imgo].active) {
		_dma_cur_fbc = imgo_fbc;
		_working_dma = ch_imgo;
	} else if (pstRTBuf->ring_buf[ch_rrzo].active) {
		_dma_cur_fbc = rrzo_fbc;
		_working_dma = ch_rrzo;
	} else {
		LOG_ERR("non-supported dma port %d %d\n", pstRTBuf->ring_buf[ch_imgo].active,
			pstRTBuf->ring_buf[ch_rrzo].active);
		return 0;
	}
	/* isr cb timing shift err handle */
	if (_dma_cur_fbc.Bits.WCNT > 0) {
		if (_dma_cur_fbc.Bits.WCNT > (pstRTBuf->ring_buf[_working_dma].start + 2)) {
			shiftT =
			    _dma_cur_fbc.Bits.WCNT - pstRTBuf->ring_buf[_working_dma].start - 2;
			if (shiftT > 0)
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc%d]:alert(%d,%d)\n", irqT,
					       pstRTBuf->ring_buf[_working_dma].start,
					       _dma_cur_fbc.Bits.WCNT);
		} else if (_dma_cur_fbc.Bits.WCNT < (pstRTBuf->ring_buf[_working_dma].start + 2)) {
			shiftT =
			    _dma_cur_fbc.Bits.WCNT + _dma_cur_fbc.Bits.FB_NUM -
			    (pstRTBuf->ring_buf[_working_dma].start + 2);
			if (shiftT >= _dma_cur_fbc.Bits.FB_NUM) {
				LOG_ERR("err shiftT = (%d,%d ,%d)\n", _dma_cur_fbc.Bits.WCNT,
					_dma_cur_fbc.Bits.FB_NUM,
					pstRTBuf->ring_buf[_working_dma].start);
				shiftT =
				    (_dma_cur_fbc.Bits.FB_NUM ? (_dma_cur_fbc.Bits.FB_NUM -
								 1) : (_dma_cur_fbc.Bits.FB_NUM));
			}
		} else {
		}		/* _dma_cur_fbc.Bits.WCNT == (pstRTBuf->ring_buf[_working_dma].start + 2) */
	}
#endif


#ifdef _rtbc_buf_que_2_0_
	for (k = 0; k < shiftT + 1; k++)	/* patch missed isr_done */
#endif
	{
		for (i = 0; i <= 1; i++) {
			/*  */
			if (0 == i) {
				i_dma = ch_imgo;
				/* reg_fbc = ch_imgo_fbc; */
			} else {
				i_dma = ch_rrzo;
				/* reg_fbc = ch_rrzo_fbc; */
			}
			/*  */
			if (0 == pstRTBuf->ring_buf[i_dma].empty_count) {
				/*  */
				if (IspInfo.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "[rtbc][DONE]:dma(%d)buf num empty,start(%d)\n",
						       i_dma, pstRTBuf->ring_buf[i_dma].start);
				}
				/*  */
				continue;
			}
#if 0
			/* once if buffer put into queue between SOF and ISP_DONE. */
			if (MFALSE == pstRTBuf->ring_buf[i_dma].active) {
				/*  */
				if (IspInfo.DebugMask & ISP_DBG_INT_2) {
					LOG_DBG("[rtbc][DONE] ERROR: missing SOF ");
				}
				/*  */
				continue;
			}
#endif
			curr = pstRTBuf->ring_buf[i_dma].start;
			/* MUINT32 loopCount = 0; */
			while (1) {
				if (IspInfo.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "i_dma(%d),curr(%d),bFilled(%d)\n", i_dma,
						       curr,
						       pstRTBuf->ring_buf[i_dma].
						       data[curr].bFilled);
				}
				/*  */
				if (pstRTBuf->ring_buf[i_dma].data[curr].bFilled ==
				    ISP_RTBC_BUF_EMPTY) {
					if (IspInfo.DebugMask & ISP_DBG_INT_2)
						IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
							       "[rtbc][DONE]:dma_%d,fill buffer,cur_%d\n",
							       i_dma, curr);
					pstRTBuf->ring_buf[i_dma].data[curr].bFilled =
					    ISP_RTBC_BUF_FILLED;
					/* start + 1 */
					pstRTBuf->ring_buf[i_dma].start =
					    (curr + 1) % pstRTBuf->ring_buf[i_dma].total_count;
					pstRTBuf->ring_buf[i_dma].empty_count--;
					/*  */
					if (g1stSof[irqT] == MTRUE) {
						LOG_ERR
						    ("Done&&Sof recieve at the same time in 1st f(%d)\n",
						     i_dma);
						g1stErrhandle[i_dma] = MTRUE;
					}
					break;
				} else {
					if (1) {	/* (IspInfo.DebugMask & ISP_DBG_INT_2) { */
						LOG_ERR
						    ("dma_%d,cur_%d,bFilled_%d != EMPTY(%d %d %d)\n",
						     i_dma, curr,
						     pstRTBuf->ring_buf[i_dma].data[curr].bFilled,
						     pstRTBuf->ring_buf[i_dma].data[0].bFilled,
						     pstRTBuf->ring_buf[i_dma].data[1].bFilled,
						     pstRTBuf->ring_buf[i_dma].data[2].bFilled);

					}
					/* start + 1 */
					/* pstRTBuf->ring_buf[i_dma].start = (curr+1)%pstRTBuf->ring_buf[i_dma].total_count; */
					break;
				}
#if 0
				loopCount++;
				if (loopCount > pstRTBuf->ring_buf[i_dma].total_count) {
					LOG_ERR("Can't find empty dma(%d) buf in total_count(%d)",
						i_dma, pstRTBuf->ring_buf[i_dma].total_count);
					break;
				}
#endif
			}
#if 0
			/* enable fbc to stall DMA */
			if (0 == pstRTBuf->ring_buf[i_dma].empty_count) {
				if (_imgo_ == i_dma) {
					reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
					reg_val |= 0x4000;
					/* ISP_WR32(ISP_REG_ADDR_IMGO_FBC,reg_val); */
				} else {
					reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
					reg_val |= 0x4000;
					/* ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC,reg_val); */
				}
				/*  */
				if (IspInfo.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "[rtbc][DONE]:dma(%d),en fbc(0x%x) stalled DMA out",
						       i_dma, ISP_RD32(reg_fbc));
				}
			}
#endif
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc][DONE]:dma(%d),start(%d),empty(%d)\n", i_dma,
					       pstRTBuf->ring_buf[i_dma].start,
					       pstRTBuf->ring_buf[i_dma].empty_count);
			}
#if 0				/* time stamp move to sof */
			/*  */
			pstRTBuf->ring_buf[i_dma].data[curr].timeStampS = sec;
			pstRTBuf->ring_buf[i_dma].data[curr].timeStampUs = usec;
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				LOG_DBG("[rtbc][DONE]:dma(%d),curr(%d),sec(%lld),usec(%ld) ", i_dma,
					curr, sec, usec);
			}
#endif
			/*  */
			DMA_TRANS(i_dma, out);
			pstRTBuf->ring_buf[i_dma].img_cnt = sof_count[out];
		}
	}

	if (pstRTBuf->ring_buf[ch_imgo].active && pstRTBuf->ring_buf[ch_rrzo].active) {
		if (pstRTBuf->ring_buf[ch_imgo].start != pstRTBuf->ring_buf[ch_rrzo].start) {
			LOG_ERR("start idx mismatch %d_%d(%d %d %d,%d %d %d)",
				pstRTBuf->ring_buf[ch_imgo].start,
				pstRTBuf->ring_buf[ch_rrzo].start,
				pstRTBuf->ring_buf[ch_imgo].data[0].bFilled,
				pstRTBuf->ring_buf[ch_imgo].data[1].bFilled,
				pstRTBuf->ring_buf[ch_imgo].data[2].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[0].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[1].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[2].bFilled);
		}
	}
	/* LOG_INF("RTBC_DBG7 imgo(buf cnt): %d %d %d\n",pstRTBuf->ring_buf[_imgo_].data[0].bFilled,pstRTBuf->ring_buf[_imgo_].data[1].bFilled,pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
	/* LOG_INF("RTBC_DBG7 rrzo(buf cnt): %d %d %d\n",pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
#if 0
	if (IspInfo.DebugMask & ISP_DBG_INT_2) {
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "-:[rtbc]");
	}
#endif
	/*  */
	pstRTBuf->state = ISP_RTBC_STATE_DONE;
	/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_ED_BufQue_Update_GPtr(int listTag)
{
	MINT32 ret = 0;
	MINT32 tmpIdx = 0;
	MINT32 cnt = 0;
	bool stop = false;
	int i = 0;
	ISP_ED_BUF_STATE_ENUM gPtrSts = ISP_ED_BUF_STATE_NONE;
	switch (listTag) {
	case P2_EDBUF_RLIST_TAG:
		/* [1] check global pointer current sts */
		gPtrSts = P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts;

		/* ////////////////////////////////////////////////////////////////////// */
		/* Assume we have the buffer list in the following situation */
		/* ++++++         ++++++         ++++++ */
		/* +  vss +         +  prv +         +  prv + */
		/* ++++++         ++++++         ++++++ */
		/* not deque         erased           enqued */
		/* done */
		/*  */
		/* if the vss deque is done, we should update the CurBufIdx to the next "enqued" buffer node instead of just moving to the next buffer node */
		/* ////////////////////////////////////////////////////////////////////// */
		/* [2]calculate traverse count needed */
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			cnt = P2_EDBUF_RList_LastBufIdx - P2_EDBUF_RList_FirstBufIdx;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_ - P2_EDBUF_RList_FirstBufIdx;
			cnt += P2_EDBUF_RList_LastBufIdx;
		}

		/* [3] update */
		tmpIdx = P2_EDBUF_RList_CurBufIdx;
		switch (gPtrSts) {
		case ISP_ED_BUF_STATE_ENQUE:
			P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts =
			    ISP_ED_BUF_STATE_RUNNING;
			break;
		case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
		case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
		case ISP_ED_BUF_STATE_DEQUE_FAIL:
			do {	/* to find the newest cur index */

				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
					P2_EDBUF_RingList[tmpIdx].bufSts = ISP_ED_BUF_STATE_RUNNING;
					P2_EDBUF_RList_CurBufIdx = tmpIdx;
					stop = true;
					break;
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* ////////////////////////////////////////////////////////////////////// */
			/* Assume we have the buffer list in the following situation */
			/* ++++++         ++++++         ++++++ */
			/* +  vss +         +  prv +         +  prv + */
			/* ++++++         ++++++         ++++++ */
			/* not deque         erased           erased */
			/* done */
			/*  */
			/* all the buffer node are deque done in the current moment, should update current index to the last node */
			/* if the vss deque is done, we should update the CurBufIdx to the last buffer node */
			/* ////////////////////////////////////////////////////////////////////// */
			if ((!stop) && (i == (cnt))) {
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;
			}
			break;
		case ISP_ED_BUF_STATE_NONE:
		case ISP_ED_BUF_STATE_RUNNING:
		default:
			break;
		}
		break;
	case P2_EDBUF_MLIST_TAG:
	default:
		LOG_ERR("Wrong List tag(%d)\n", listTag);
		break;
	}
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
#if 0				/* disable it to avoid build warning */
static MINT32 ISP_ED_BufQue_Set_FailNode(ISP_ED_BUF_STATE_ENUM failType, MINT32 idx)
{
	MINT32 ret = 0;
	spin_lock(&(SpinLockEDBufQueList));
	/* [1]set fail type */
	P2_EDBUF_RingList[idx].bufSts = failType;

	/* [2]update global pointer */
	ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
	spin_unlock(&(SpinLockEDBufQueList));
	return ret;
}
#endif

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_ED_BufQue_Erase(MINT32 idx, int listTag)
{
	MINT32 ret = -1;
	bool stop = false;
	int i = 0;
	MINT32 cnt = 0;
	int tmpIdx = 0;

	switch (listTag) {
	case P2_EDBUF_MLIST_TAG:
		tmpIdx = P2_EDBUF_MList_FirstBufIdx;
		/* [1] clear buffer status */
		P2_EDBUF_MgrList[idx].processID = 0x0;
		P2_EDBUF_MgrList[idx].callerID = 0x0;
		P2_EDBUF_MgrList[idx].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[idx].dequedNum = 0;
		/* [2] update first index */
		if (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx == -1) {
			/* traverse count needed, cuz user may erase the element but not the one at first idx(pip or vss scenario) */
			if (P2_EDBUF_MList_FirstBufIdx <= P2_EDBUF_MList_LastBufIdx) {
				cnt = P2_EDBUF_MList_LastBufIdx - P2_EDBUF_MList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_PACKAGE_NUM_ - P2_EDBUF_MList_FirstBufIdx;
				cnt += P2_EDBUF_MList_LastBufIdx;
			}
			do {	/* to find the newest first lindex */

				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				switch (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx) {
				case (-1):
					break;
				default:
					stop = true;
					P2_EDBUF_MList_FirstBufIdx = tmpIdx;
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one firstBufindex point at */
			/* and all the buffer node are deque done in the current moment, should update first index to the last node */
			if ((!stop) && (i == cnt)) {
				P2_EDBUF_MList_FirstBufIdx = P2_EDBUF_MList_LastBufIdx;
			}
		}
		break;
	case P2_EDBUF_RLIST_TAG:
		tmpIdx = P2_EDBUF_RList_FirstBufIdx;
		/* [1] clear buffer status */
		P2_EDBUF_RingList[idx].processID = 0x0;
		P2_EDBUF_RingList[idx].callerID = 0x0;
		P2_EDBUF_RingList[idx].p2dupCQIdx = -1;
		P2_EDBUF_RingList[idx].bufSts = ISP_ED_BUF_STATE_NONE;
		EDBufQueRemainNodeCnt--;
		/* [2]update first index */
		if (P2_EDBUF_RingList[tmpIdx].bufSts == ISP_ED_BUF_STATE_NONE) {
			/* traverse count needed, cuz user may erase the element but not the one at first idx */
			if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
				cnt = P2_EDBUF_RList_LastBufIdx - P2_EDBUF_RList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_FRAME_NUM_ - P2_EDBUF_RList_FirstBufIdx;
				cnt += P2_EDBUF_RList_LastBufIdx;
			}
			/* to find the newest first lindex */
			do {
				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
					stop = true;
					P2_EDBUF_RList_FirstBufIdx = tmpIdx;
					break;
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one firstBufindex point at */
			/* and all the buffer node are deque done in the current moment, should update first index to the last node */
			if ((!stop) && (i == (cnt))) {
				P2_EDBUF_RList_FirstBufIdx = P2_EDBUF_RList_LastBufIdx;
			}
		}
		break;
	default:
		break;
	}
	return ret;
}

/*******************************************************************************
* get first matched buffer
********************************************************************************/
static MINT32 ISP_ED_BufQue_Get_FirstMatBuf(ISP_ED_BUFQUE_STRUCT param, int ListTag, int type)
{
	MINT32 idx = -1;
	MINT32 i = 0;
	switch (ListTag) {
	case P2_EDBUF_MLIST_TAG:
		if (type == 0) {	/* for user wait frame, do not care p2 dupCq index, first enqued p2 dupCQ first out */
			if (P2_EDBUF_MList_FirstBufIdx <= P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx; i <= P2_EDBUF_MList_LastBufIdx;
				     i++) {
					if ((P2_EDBUF_MgrList[i].processID == param.processID)
					    && (P2_EDBUF_MgrList[i].callerID == param.callerID)) {
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
					if ((P2_EDBUF_MgrList[i].processID == param.processID)
					    && (P2_EDBUF_MgrList[i].callerID == param.callerID)) {
						idx = i;
						break;
					}
				}
				if (idx != -1) {	/*get in the first for loop */
				} else {
					for (i = 0; i <= P2_EDBUF_MList_LastBufIdx; i++) {
						if ((P2_EDBUF_MgrList[i].processID ==
						     param.processID)
						    && (P2_EDBUF_MgrList[i].callerID ==
							param.callerID)) {
							idx = i;
							break;
						}
					}
				}
			}
		} else {	/* for buffer node deque done notify */
			if (P2_EDBUF_MList_FirstBufIdx <= P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx; i <= P2_EDBUF_MList_LastBufIdx;
				     i++) {
					if ((P2_EDBUF_MgrList[i].processID == param.processID) && (P2_EDBUF_MgrList[i].callerID == param.callerID) && (P2_EDBUF_MgrList[i].p2dupCQIdx == param.p2dupCQIdx) && (P2_EDBUF_MgrList[i].dequedNum < P2_Support_BurstQNum)) {	/* avoid race that dupCQ_1 of buffer2 enqued while dupCQ_1 of buffer1 have beend deque done but not been erased yet */
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
					if ((P2_EDBUF_MgrList[i].processID == param.processID)
					    && (P2_EDBUF_MgrList[i].callerID == param.callerID)
					    && (P2_EDBUF_MgrList[i].p2dupCQIdx == param.p2dupCQIdx)
					    && (P2_EDBUF_MgrList[i].dequedNum <
						P2_Support_BurstQNum)) {
						idx = i;
						break;
					}
				}
				if (idx != -1) {	/*get in the first for loop */
				} else {
					for (i = 0; i <= P2_EDBUF_MList_LastBufIdx; i++) {
						if ((P2_EDBUF_MgrList[i].processID ==
						     param.processID)
						    && (P2_EDBUF_MgrList[i].callerID ==
							param.callerID)
						    && (P2_EDBUF_MgrList[i].p2dupCQIdx ==
							param.p2dupCQIdx)
						    && (P2_EDBUF_MgrList[i].dequedNum <
							P2_Support_BurstQNum)) {
							idx = i;
							break;
						}
					}
				}
			}
		}
		break;
	case P2_EDBUF_RLIST_TAG:
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx; i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && (P2_EDBUF_RingList[i].callerID == param.callerID)) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && (P2_EDBUF_RingList[i].callerID == param.callerID)) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {	/*get in the first for loop */
			} else {
				for (i = 0; i <= P2_EDBUF_RList_LastBufIdx; i++) {
					if ((P2_EDBUF_RingList[i].processID == param.processID)
					    && (P2_EDBUF_RingList[i].callerID == param.callerID)) {
						idx = i;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}
	if (idx == -1) {
		LOG_ERR("Could not find match buffer tag(%d) pid/cid/p2dupCQidx(%d/0x%x/%d)",
			ListTag, param.processID, param.callerID, param.p2dupCQIdx);
	}
	return idx;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_ED_BufQue_CTRL_FUNC(ISP_ED_BUFQUE_STRUCT param)
{
	MINT32 ret = 0;
	int i = 0;
	int idx = -1, idx2 = -1;
	MINT32 restTime = 0;
	switch (param.ctrl) {
	case ISP_ED_BUFQUE_CTRL_ENQUE_FRAME:	/* signal that a specific buffer is enqueued */
		/* [1] check the ring buffer list is full or not */
		spin_lock(&(SpinLockEDBufQueList));
		if (((P2_EDBUF_MList_LastBufIdx + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_) ==
		    P2_EDBUF_MList_FirstBufIdx && (P2_EDBUF_MList_LastBufIdx != -1)) {
			LOG_ERR("F/L(%d,%d),(%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d)",
				P2_EDBUF_MList_FirstBufIdx, P2_EDBUF_MList_LastBufIdx,
				P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx,
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx,
				P2_EDBUF_RList_FirstBufIdx, P2_EDBUF_RList_CurBufIdx,
				P2_EDBUF_RList_LastBufIdx,
				P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts,
				P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts,
				P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts);
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("p2 ring buffer list is full, enque Fail.");
			ret = -EFAULT;
			return ret;
		} else {
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, 0, _LOG_DBG,
				       "pD(%d_0x%x) MF/L(%d,%d),(%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d),dCq(%d)/Bq(%d)\n",
				       param.processID, param.callerID, P2_EDBUF_MList_FirstBufIdx,
				       P2_EDBUF_MList_LastBufIdx,
				       P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx,
				       P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx,
				       P2_EDBUF_RList_FirstBufIdx, P2_EDBUF_RList_CurBufIdx,
				       P2_EDBUF_RList_LastBufIdx,
				       P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts,
				       P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts,
				       P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts,
				       param.p2dupCQIdx, param.p2burstQIdx);
			/* [2] add new element to the last of the list */
			if (P2_EDBUF_RList_FirstBufIdx == P2_EDBUF_RList_LastBufIdx && P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts == ISP_ED_BUF_STATE_NONE) {	/* all buffer node is empty */
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_FirstBufIdx = P2_EDBUF_RList_LastBufIdx;
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;
			} else if (P2_EDBUF_RList_CurBufIdx == P2_EDBUF_RList_LastBufIdx && P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts == ISP_ED_BUF_STATE_NONE) {	/* first node is not empty, but current/last is empty */
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;
			} else {
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
			}
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].processID = param.processID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].callerID = param.callerID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].p2dupCQIdx = param.p2dupCQIdx;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts =
			    ISP_ED_BUF_STATE_ENQUE;
			EDBufQueRemainNodeCnt++;

			/* [3] add new buffer package in manager list */
			if (param.p2burstQIdx == 0) {
				if (P2_EDBUF_MList_FirstBufIdx == P2_EDBUF_MList_LastBufIdx && P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx == -1) {	/* all managed buffer node is empty */
					P2_EDBUF_MList_LastBufIdx =
					    (P2_EDBUF_MList_LastBufIdx +
					     1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
					P2_EDBUF_MList_FirstBufIdx = P2_EDBUF_MList_LastBufIdx;
				} else {
					P2_EDBUF_MList_LastBufIdx =
					    (P2_EDBUF_MList_LastBufIdx +
					     1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				}
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].processID =
				    param.processID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].callerID =
				    param.callerID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx =
				    param.p2dupCQIdx;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].dequedNum = 0;
			}
		}
		/* [4]update global index */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
		IRQ_LOG_PRINTER(_CAMSV_D_IRQ, 0, _LOG_DBG);
		/* [5] wake up thread that wait for deque */
		wake_up_interruptible((wait_queue_head_t *) (&WaitQueueHead_EDBuf_WaitDeque));
		break;
	case ISP_ED_BUFQUE_CTRL_WAIT_DEQUE:	/* a dequeue thread is waiting to do dequeue */
		/* [1]traverse for finding the buffer which had not beed dequeued of the process */
		spin_lock(&(SpinLockEDBufQueList));
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx; i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					|| (P2_EDBUF_RingList[i].bufSts ==
					    ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					|| (P2_EDBUF_RingList[i].bufSts ==
					    ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {	/*get in the first for loop */
			} else {
				for (i = 0; i <= P2_EDBUF_RList_LastBufIdx; i++) {
					if ((P2_EDBUF_RingList[i].processID == param.processID)
					    &&
					    ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					     || (P2_EDBUF_RingList[i].bufSts ==
						 ISP_ED_BUF_STATE_RUNNING))) {
						idx = i;
						break;
					}
				}
			}
		}
		spin_unlock(&(SpinLockEDBufQueList));
		if (idx == -1) {
			LOG_ERR("Do not find match buffer (pid/cid %d/0x%x) to deque!",
				param.processID, param.callerID);
			ret = -EFAULT;
			return ret;
		} else {
			restTime = wait_event_interruptible_timeout(*((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitDeque), ISP_GetEDBufQueWaitDequeState(idx), ISP_UsToJiffies(5000000));	/* 5s */
			if (restTime == 0) {
				LOG_ERR("Wait Deque fail, idx(%d) pID(%d),cID(0x%x)", idx,
					param.processID, param.callerID);
				ret = -EFAULT;
			} else {
				/* LOG_INF("wakeup and goto deque,rTime(%d), pID(%d)",restTime,param.processID); */
			}
		}
		break;
	case ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS:	/* signal that a buffer is dequeued(success) */
	case ISP_ED_BUFQUE_CTRL_DEQUE_FAIL:	/* signal that a buffer is dequeued(fail) */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("dq cm(%d),pID(%d),cID(0x%x)\n", param.ctrl, param.processID,
				param.callerID);
		}
		spin_lock(&(SpinLockEDBufQueList));
		/* [1]update buffer status for the current buffer */
		/* ////////////////////////////////////////////////////////////////////// */
		/* Assume we have the buffer list in the following situation */
		/* ++++++    ++++++ */
		/* +  vss +    +  prv + */
		/* ++++++    ++++++ */
		/*  */
		/* if the vss deque is not done(not blocking deque), dequeThread in userspace would change to deque prv buffer(block deque) immediately to decrease ioctrl count. */
		/* -> vss buffer would be deque at next turn, so curBuf is still at vss buffer node */
		/* -> we should use param to find the current buffer index in Rlikst to update the buffer status cuz deque success/fail may not be the first buffer in Rlist */
		/* ////////////////////////////////////////////////////////////////////// */
		idx2 = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_RLIST_TAG, 1);
		if (param.ctrl == ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS) {
			P2_EDBUF_RingList[idx2].bufSts = ISP_ED_BUF_STATE_DEQUE_SUCCESS;
		} else {
			P2_EDBUF_RingList[idx2].bufSts = ISP_ED_BUF_STATE_DEQUE_FAIL;
		}
		/* [2]update dequeued num in managed buffer list */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_MLIST_TAG, 1);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("ERRRRRRRRRRR findmatch index fail");
			ret = -EFAULT;
			return ret;
		}
		P2_EDBUF_MgrList[idx].dequedNum++;
		/* [3]update global pointer */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		/* [4]erase node in ring buffer list */
		if (idx2 == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("ERRRRRRRRRRR findmatch index fail");
			ret = -EFAULT;
			return ret;
		}
		ISP_ED_BufQue_Erase(idx2, P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
		/* [5]wake up thread user that wait for a specific buffer and the thread that wait for deque */
		wake_up_interruptible((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitFrame);
		wake_up_interruptible((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitDeque);
		break;
	case ISP_ED_BUFQUE_CTRL_WAIT_FRAME:	/* wait for a specific buffer */
		spin_lock(&(SpinLockEDBufQueList));
		/* [1]find first match buffer */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_MLIST_TAG, 0);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("could not find match buffer pID/cID (%d/0x%x)", param.processID,
				param.callerID);
			ret = -EFAULT;
			return ret;
		}
		/* [2]check the buffer is dequeued or not */
		if (P2_EDBUF_MgrList[idx].dequedNum == P2_Support_BurstQNum) {
			ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);	/* erase the buffer no matter user wait successfully or not */
			spin_unlock(&(SpinLockEDBufQueList));
			ret = 0;
			LOG_DBG("Frame is alreay dequeued, return user, pd(%d/0x%x),idx(%d)",
				param.processID, param.callerID, idx);
			return ret;
		} else {
			spin_unlock(&(SpinLockEDBufQueList));
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG("=pd(%d/0x%x_%d)wait(%d us)=\n", param.processID,
					param.callerID, idx, param.timeoutUs);
			}
			/* [3]if not, goto wait event and wait for a signal to check */
			restTime =
			    wait_event_interruptible_timeout(*
							     ((wait_queue_head_t *) &
							      WaitQueueHead_EDBuf_WaitFrame),
							     ISP_GetEDBufQueWaitFrameState(idx),
							     ISP_UsToJiffies(param.timeoutUs));
			if (restTime == 0) {
				LOG_ERR
				    ("Dequeue Buffer fail, rT(%d),idx(%d) pID(%d),cID(0x%x),p2SupportBNum(%d)\n",
				     restTime, idx, param.processID, param.callerID,
				     P2_Support_BurstQNum);
				ret = -EFAULT;
				break;
			} else {
				spin_lock(&(SpinLockEDBufQueList));
				ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);	/* erase the buffer if wait successfully */
				spin_unlock(&(SpinLockEDBufQueList));
			}
		}
		break;
	case ISP_ED_BUFQUE_CTRL_WAKE_WAITFRAME:	/* wake all sleeped users to check buffer is dequeued or not */
		wake_up_interruptible((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitFrame);
		break;
	case ISP_ED_BUFQUE_CTRL_CLAER_ALL:	/* free all recored dequeued buffer */
		spin_lock(&(SpinLockEDBufQueList));
		for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			P2_EDBUF_RingList[i].processID = 0x0;
			P2_EDBUF_RingList[i].callerID = 0x0;
			P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
		}
		P2_EDBUF_RList_FirstBufIdx = 0;
		P2_EDBUF_RList_CurBufIdx = 0;
		P2_EDBUF_RList_LastBufIdx = -1;
		/*  */
		for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			P2_EDBUF_MgrList[i].processID = 0x0;
			P2_EDBUF_MgrList[i].callerID = 0x0;
			P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
			P2_EDBUF_MgrList[i].dequedNum = 0;
		}
		P2_EDBUF_MList_FirstBufIdx = 0;
		P2_EDBUF_MList_LastBufIdx = -1;
		spin_unlock(&(SpinLockEDBufQueList));
		break;
	default:
		LOG_ERR("do not support this ctrl cmd(%d)", param.ctrl);
		break;
	}
	return ret;
}

#if 0
static MINT32 ISP_RegisterIrq(ISP_WAIT_IRQ_STRUCT RegisterIrq)
{
	LOG_INF("+\n");
	MINT32 ret = 0;
	MUINT32 flags;
	MUINT32 i = 0;
	MINT32 idx =
	    IspInfo.IrqInfo.WaitQIdx[(RegisterIrq.Type * 32) + my_get_pow_idx(RegisterIrq.Status)];
	if (idx < 0) {
		LOG_ERR("WE DON NOT SUPPORT THIS IRQ \N");
		goto EXIT;
	}
	if (RegisterIrq.UserName == NULL) {
		LOG_ERR("plz pass userName!");
		/* goto EXIT; */
	}
	struct my_list_head *ptr;
/* LOG_INF("user(%s) irqType(%d) irqSts(0x%8x)\n",RegisterIrq.UserName,RegisterIrq.Type,RegisterIrq.Status); */
	unsigned long long sec;
	unsigned long usec;
	ktime_t time;
	time = ktime_get();	/* ns */
	sec = time.tv64;
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */

	REG_IRQ_NODE *usernewnode = NULL;
	usernewnode = kmalloc(sizeof(*usernewnode), GFP_ATOMIC);
	if (usernewnode == NULL) {
		LOG_ERR("IRQ USER_NODE ALLOCATION FAILED!");
		ret = -ENOMEM;
		goto EXIT;
	}
	strcpy(usernewnode->userName, RegisterIrq.UserName);
	usernewnode->reg_T = usec;
	usernewnode->regIrq_OccurT = 0;
	usernewnode->regIrq_Cate = RegisterIrq.Type;
	usernewnode->regIrq_Sts = RegisterIrq.Status;
	MY_LIST_HEAD_INIT(&usernewnode->list);
	LOG_INF("name(%s) reg_T(%d),time(%d)\n", usernewnode->userName, usernewnode->reg_T, usec);

	spin_lock_irqsave(&(SpinLockIrqUserList), flags);
	ptr = ((struct my_list_head *)(&SupIrqUserListHead.list))->nextirq;
	for (i = 0; i < idx; i++) {	/* find father node of this irq */
		ptr = ptr->nextirq;
	}
	int cnt = 0;
	REG_IRQ_NODE *accessNode;
	/* add new user to user list */
	while (ptr->nextuser != NULL) {
		ptr = ptr->nextuser;
	}
	my_list_add_irqUserChild(&usernewnode->list, ptr);
	spin_unlock_irqrestore(&(SpinLockIrqUserList), flags);
 EXIT:
	LOG_INF("-\n");
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_UnRegisterIrq(ISP_WAIT_IRQ_STRUCT RegisterIrq)
{
	LOG_INF("+\n");
	MINT32 ret = 0;
	MUINT32 flags;
	MUINT32 i = 0;
	if (RegisterIrq.UserName == NULL) {
		LOG_ERR("plz pass userName!");
		/* return ret; */
	}
	MINT32 idx =
	    IspInfo.IrqInfo.WaitQIdx[(RegisterIrq.Type * 32) + my_get_pow_idx(RegisterIrq.Status)];
	/* LOG_INF("idx(%d) user(%s) irqType(%d) irqSts(0x%8x)\n",idx,RegisterIrq.UserName,RegisterIrq.Type,RegisterIrq.Status); */
	struct my_list_head *ptr;
	struct my_list_head *prv;
	REG_IRQ_NODE *accessNode;

	spin_lock_irqsave(&(SpinLockIrqUserList), flags);
	ptr = ((struct my_list_head *)(&SupIrqUserListHead.list))->nextirq;
	for (i = 0; i < idx; i++) {	/* find father node of this irq */
		ptr = ptr->nextirq;
	}
	prv = ptr;		/* father node of the irq */
	if (ptr->nextuser != NULL) {
		ptr = ptr->nextuser;	/* fisrt user child */
		do {		/* traverse user child nodes */
			typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (ptr);
			accessNode =
			    ((REG_IRQ_NODE *) ((char *)__mptr - offsetof(REG_IRQ_NODE, list)));
			/* accessNode=list_entry(ptr, REG_IRQ_NODE, list); */
			if ((strcmp(RegisterIrq.UserName, accessNode->userName) == 0)) {
				prv->nextuser = ptr->nextuser;
				ptr->nextuser = NULL;
				LOG_INF("free one,user(%s),reg_T(%d),Name(%s)\n",
					accessNode->userName, accessNode->reg_T,
					accessNode->userName);
				kfree(accessNode);
				break;
			} else {
				prv = ptr;
				ptr = ptr->nextuser;
			}
		} while (ptr != NULL);
	}
	spin_unlock_irqrestore(&(SpinLockIrqUserList), flags);
#if 0
	/* test print rest child nodes */
	int cnt = 0;
	ptr = ((struct my_list_head *)(&SupIrqUserListHead.list))->nextirq;
	for (i = 0; i < idx; i++) {	/* find father node of this irq */
		ptr = ptr->nextirq;
	}
	while (ptr->nextuser != NULL) {
		ptr = ptr->nextuser;
		accessNode = list_entry(ptr, REG_IRQ_NODE, list);
		LOG_INF("cnt(%d) name(%s) reg_T(%d)\n", ++cnt, accessNode->userName,
			accessNode->reg_T);
	}
#endif
	LOG_INF("-\n");

	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_UnRegister_AllregIrq()
{
	LOG_INF("+\n");
	MINT32 ret = 0;
	MUINT32 i = 0;
	MUINT32 flags;
	struct my_list_head *father;
	struct my_list_head *userchild;
	spin_lock_irqsave(&(SpinLockIrqUserList), flags);
	father = ((struct my_list_head *)(&SupIrqUserListHead.list));	/* first support irq */
	do {
		father = father->nextirq;
		/* delete all child user in this fathre node */
		while (1) {
			userchild = father;
			if (userchild->nextuser != NULL) {	/* traverse and delete user child nodes */
				userchild = userchild->nextuser;
				REG_IRQ_NODE *accessNode;
				typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (userchild);
				accessNode =
				    ((REG_IRQ_NODE *) ((char *)__mptr -
						       offsetof(REG_IRQ_NODE, list)));

				father->nextuser = userchild->nextuser;
				userchild->nextuser = NULL;
				LOG_INF("free one,user(%s),reg_T(%d)\n", accessNode->userName,
					accessNode->reg_T);
				kfree(accessNode);
			} else {
				LOG_INF("break\n");
				break;
			}
		}
	} while (father->nextirq != father);
	spin_unlock_irqrestore(&(SpinLockIrqUserList), flags);
#if 0
	/* test print rest child nodes */
	struct my_list_head *ptr;
	struct my_list_head *ptr2;
	int cnt = 0;
	ptr2 = ((struct my_list_head *)(&SupIrqUserListHead.list))->nextirq;
	for (i = 0; i < 5; i++) {	/* find father node of this irq */
		ptr = ptr2;
		while (ptr->nextuser != NULL) {
			ptr = ptr->nextuser;
			REG_IRQ_NODE *accessNode;
			accessNode = list_entry(ptr, REG_IRQ_NODE, list);
			LOG_INF("cnt(%d) name(%s) reg_T(%d)\n", ++cnt, accessNode->userName,
				accessNode->reg_T);
		}
		ptr2 = ptr2->nextirq;
	}
#endif

	LOG_INF("-\n");
	return ret;
}
#endif

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_WaitIrq(ISP_WAIT_IRQ_STRUCT * WaitIrq)
{
	MINT32 Ret = 0, Timeout = WaitIrq->Timeout;
	MUINT32 i;
	unsigned long flags;
	eISPIrq eIrq = _IRQ;
	int cnt = 0;
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		LOG_DBG("+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d)",
			WaitIrq->Clear, WaitIrq->Type, WaitIrq->Status, WaitIrq->Timeout);
	}
	/*  */

	switch (WaitIrq->Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}
	if (WaitIrq->Clear == ISP_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		if (IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] & WaitIrq->Status) {
			/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status); */
			IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &=
			    (~WaitIrq->Status);
		}
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type]); */
		IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] = 0;
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type]); */
		IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
		return Ret;
	}
	/*  */
	Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
						   ISP_GetIRQState(eIrq, WaitIrq->Type,
								   WaitIrq->UserNumber,
								   WaitIrq->Status),
						   ISP_MsToJiffies(WaitIrq->Timeout));
	/* check if user is interrupted by system signal */

	while ((Timeout != 0)
	       && (!ISP_GetIRQState(eIrq, WaitIrq->Type, WaitIrq->UserNumber, WaitIrq->Status))
	       && (cnt < 3)) {
		/*  */
		LOG_ERR("[ERR] (cnt %d)interrupted by system signal,T(%d),go sleep again", cnt,
			Timeout);
		Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
							   ISP_GetIRQState(eIrq, WaitIrq->Type,
									   WaitIrq->UserNumber,
									   WaitIrq->Status),
							   ISP_MsToJiffies(WaitIrq->Timeout));
		LOG_ERR("[ERR] (cnt %d)leave second sleep stage", cnt);
		cnt++;
	}
	/* timeout */
	if (Timeout == 0) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		LOG_ERR
		    ("WaitIrq Timeout Clear(%d), Type(%d), IrqStatus(0x%08X), WaitStatus(0x%08X), Timeout(%d)",
		     WaitIrq->Clear, WaitIrq->Type,
		     IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type], WaitIrq->Status,
		     WaitIrq->Timeout);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
		if (WaitIrq->bDumpReg) {
			ISP_DumpReg();
		}
		Ret = -EFAULT;
		goto EXIT;
	}

	/*  */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			/* LOG_DBG("Type(%d), IrqStatus(0x%08X)",i,IspInfo.IrqInfo.Status[i]); */
		}
	}
	/*  */
	IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &= (~WaitIrq->Status);	/* clear the status if someone get the irq */
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*  */
	/* check CQ status, when pass2, pass2b, pass2c done */
	if (WaitIrq->Type == ISP_IRQ_TYPE_INT_P2_ST) {
		MUINT32 CQ_status;
		ISP_WR32(IMGSYS_BASE + 0x4160, 0x6000);
		CQ_status = ISP_RD32(IMGSYS_BASE + 0x4164);
		switch (WaitIrq->Status) {
		case ISP_IRQ_P2_STATUS_PASS2A_DON_ST:
			if ((CQ_status & 0x0000000F) != 0x001) {
				LOG_ERR("CQ1 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2B_DON_ST:
			if ((CQ_status & 0x000000F0) != 0x010) {
				LOG_ERR("CQ2 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2C_DON_ST:
			if ((CQ_status & 0x00000F00) != 0x100) {
				LOG_ERR("CQ3 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		default:
			break;
		}
	}

 EXIT:
	return Ret;
}

/* #define _debug_dma_err_ */
#if defined(_debug_dma_err_)
#define bit(x) (0x1<<(x))

MUINT32 DMA_ERR[3 * 12] = {
	bit(1), 0xF50043A8, 0x00000011,	/* IMGI */
	bit(2), 0xF50043AC, 0x00000021,	/* IMGCI */
	bit(4), 0xF50043B0, 0x00000031,	/* LSCI */
	bit(5), 0xF50043B4, 0x00000051,	/* FLKI */
	bit(6), 0xF50043B8, 0x00000061,	/* LCEI */
	bit(7), 0xF50043BC, 0x00000071,	/* VIPI */
	bit(8), 0xF50043C0, 0x00000081,	/* VIP2I */
	bit(9), 0xF50043C4, 0x00000194,	/* IMGO */
	bit(10), 0xF50043C8, 0x000001a4,	/* IMG2O */
	bit(11), 0xF50043CC, 0x000001b4,	/* LCSO */
	bit(12), 0xF50043D0, 0x000001c4,	/* ESFKO */
	bit(13), 0xF50043D4, 0x000001d4,	/* AAO */
};

static MINT32 DMAErrHandler()
{
	MUINT32 err_ctrl = ISP_RD32(0xF50043A4);
	LOG_DBG("err_ctrl(0x%08x)", err_ctrl);

	MUINT32 i = 0;

	MUINT32 *pErr = DMA_ERR;
	for (i = 0; i < 12; i++) {
		MUINT32 addr = 0;
#if 1
		if (err_ctrl & (*pErr)) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, ISP_RD32(addr), ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#else
		addr = pErr[1];
		MUINT32 status = ISP_RD32(addr);

		if (status & 0x0000FFFF) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, status, ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#endif
		pErr = pErr + 3;
	}

}
#endif

/* ///////////////////////////////////////////////////////////////////////////// */
/* for CAMSV */
static __tcmfunc irqreturn_t ISP_Irq_CAMSV(MINT32 Irq, void *DeviceId)
{
	/* MUINT32 result=0x0; */
	MUINT32 j = 0;
	/* MINT32  idx=0; */
	MUINT32 IrqStatus_CAMSV;
	CQ_RTBC_FBC fbc;
	fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
	spin_lock(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]));
	IrqStatus_CAMSV =
	    (ISP_RD32(ISP_REG_ADDR_CAMSV_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV]));
	for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
		IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_CAMSV] |=
		    (IrqStatus_CAMSV & IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV]);
	}
	if (IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV] & IrqStatus_CAMSV) {
		IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_ERR,
			       CAMSV_TAG "Err IRQ, Type(%d), Status(0x%x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV,
			       IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV] & IrqStatus_CAMSV);
	}
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
			       CAMSV_TAG "Type(%d), IrqStatus(0x%x | 0x%x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV,
			       (unsigned int)IspInfo.IrqInfo.Status[ISP_IRQ_TYPE_INT_CAMSV],
			       IrqStatus_CAMSV);
	}


	if (IrqStatus_CAMSV & ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST) {
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "DONE_%d_%d(0x%x,0x%x,0x%x,0x%x)\n",
				       (sof_count[_CAMSV]) ? (sof_count[_CAMSV] -
							      1) : (sof_count[_CAMSV]),
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST) & 0x00FF0000) >>
					16), (unsigned int)fbc.Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE));
		}
#if 0				/* time stamp move to sof */
		unsigned long long sec = 0;
		unsigned long usec = 0;
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			sec = cpu_clock(0);	/* ns */
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */
		}
		ISP_CAMSV_DONE_Buf_Time(_camsv_imgo_, sec, usec);
#else
		ISP_CAMSV_DONE_Buf_Time(_camsv_imgo_, 0, 0);
#endif
	}
	if (IrqStatus_CAMSV & ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST) {
		/* chk this frame have EOF or not */
		if ((fbc.Bits.FB_NUM == fbc.Bits.FBC_CNT)
		    && (g1stErrhandle[_camsv_imgo_] == MFALSE)) {
			gSof_camsvdone[0] = 1;
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "Lost done_%d\n", sof_count[_CAMSV]);

		} else {
			gSof_camsvdone[0] = 0;
		}
#ifdef _rtbc_buf_que_2_0_
		{
			MUINT32 z;
			if (1 == mFwRcnt.bLoadBaseAddr[_CAMSV_IRQ]) {
				if (pstRTBuf->ring_buf[_camsv_imgo_].active) {
					IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
						       CAMSV_TAG "wr2Phy,");
					ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
						 pstRTBuf->ring_buf[_camsv_imgo_].
						 data[pstRTBuf->ring_buf
						      [_camsv_imgo_].start].base_pAddr);
				}
				mFwRcnt.bLoadBaseAddr[_CAMSV_IRQ] = 0;
			}
			/* equal case is for clear curidx */
			for (z = 0; z <= mFwRcnt.curIdx[_CAMSV_IRQ]; z++) {
				if (1 == mFwRcnt.INC[_CAMSV_IRQ][z]) {
					mFwRcnt.INC[_CAMSV_IRQ][z] = 0;
					/* patch hw bug */
					fbc.Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_CAMSV_IMGO_FBC, fbc.Reg_val);
					fbc.Bits.RCNT_INC = 0;
					ISP_WR32(ISP_REG_ADDR_CAMSV_IMGO_FBC, fbc.Reg_val);
					IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
						       CAMSV_TAG "RCNT_INC\n");
				} else {
					mFwRcnt.curIdx[_CAMSV_IRQ] = 0;
					break;
				}
			}
		}
#endif
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "SOF_%d_%d(0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_CAMSV],
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST) & 0x00FF0000) >>
					16), ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE));
		}
		{
			unsigned long long sec;
			unsigned long usec;
			ktime_t time;

			time = ktime_get();	/* ns */
			sec = time.tv64;
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */

			ISP_CAMSV_SOF_Buf_Get(_camsv_imgo_, sec, usec, gSof_camsvdone[0]);
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]));
#ifdef ISR_LOG_ON
	IRQ_LOG_PRINTER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(_CAMSV_IRQ, m_CurrentPPB, _LOG_ERR);
#endif
	wake_up_interruptible(&IspInfo.WaitQueueHead);

	return IRQ_HANDLED;
}

static __tcmfunc irqreturn_t ISP_Irq_CAMSV2(MINT32 Irq, void *DeviceId)
{
	/* MUINT32 result=0x0; */
	MUINT32 j = 0;
	/* MINT32  idx=0; */
	MUINT32 IrqStatus_CAMSV2;
	CQ_RTBC_FBC fbc;
	fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);
	spin_lock(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]));
	IrqStatus_CAMSV2 =
	    (ISP_RD32(ISP_REG_ADDR_CAMSV2_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2]));
	for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
		IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_CAMSV2] |=
		    (IrqStatus_CAMSV2 & IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2]);
	}
	if (IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2] & IrqStatus_CAMSV2) {
		IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_ERR,
			       CAMSV2_TAG "Error IRQ, Type(%d), Status(0x%08x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV2,
			       IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2] & IrqStatus_CAMSV2);
	}
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
			       CAMSV2_TAG "Type(%d), IrqStatus(0x%08X | 0x%08x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV2,
			       (unsigned int)IspInfo.IrqInfo.Status[ISP_IRQ_TYPE_INT_CAMSV2],
			       IrqStatus_CAMSV2);
	}
	if (IrqStatus_CAMSV2 & ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST) {
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF, CAMSV2_TAG "fbc(0x%x)",
				       (unsigned int)fbc.Reg_val);

			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG
				       "DONE_%d_%d(0x%x,0x%x,0x%x,0x%x,camsv support no inner addr)\n",
				       (sof_count[_CAMSV_D]) ? (sof_count[_CAMSV_D] -
								1) : (sof_count[_CAMSV_D]),
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST) & 0x00FF0000) >>
					16), (unsigned int)fbc.Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE));
		}
#if 0				/* time stamp move to sof */
		unsigned long long sec;
		unsigned long usec;
		sec = cpu_clock(0);	/* ns */
		do_div(sec, 1000);	/* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */

		ISP_CAMSV_DONE_Buf_Time(_camsv2_imgo_, sec, usec);
#else
		ISP_CAMSV_DONE_Buf_Time(_camsv2_imgo_, 0, 0);
#endif
	}
	if (IrqStatus_CAMSV2 & ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST) {
		/* chk this frame have EOF or not */
		if ((fbc.Bits.FB_NUM == fbc.Bits.FBC_CNT)
		    && (g1stErrhandle[_camsv_imgo_] == MFALSE)) {
			gSof_camsvdone[1] = 1;
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG "Lost done %d", sof_count[_CAMSV_D]);

		} else {
			gSof_camsvdone[1] = 0;
		}
#ifdef _rtbc_buf_que_2_0_
		{
			MUINT32 z;
			if (1 == mFwRcnt.bLoadBaseAddr[_CAMSV_D_IRQ]) {
				if (pstRTBuf->ring_buf[_camsv2_imgo_].active) {
					IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
						       CAMSV2_TAG "wr2Phy,");
					ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
						 pstRTBuf->ring_buf[_camsv2_imgo_].
						 data[pstRTBuf->ring_buf
						      [_camsv2_imgo_].start].base_pAddr);
				}
				mFwRcnt.bLoadBaseAddr[_CAMSV_D_IRQ] = 0;
			}
			/* equal case is for clear curidx */
			for (z = 0; z <= mFwRcnt.curIdx[_CAMSV_D_IRQ]; z++) {
				if (1 == mFwRcnt.INC[_CAMSV_D_IRQ][z]) {
					mFwRcnt.INC[_CAMSV_D_IRQ][z] = 0;
					/* path hw bug */
					fbc.Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_CAMSV2_IMGO_FBC, fbc.Reg_val);
					fbc.Bits.RCNT_INC = 0;
					ISP_WR32(ISP_REG_ADDR_CAMSV2_IMGO_FBC, fbc.Reg_val);
					IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
						       CAMSV2_TAG "RCNT_INC\n");
				} else {
					mFwRcnt.curIdx[_CAMSV_D_IRQ] = 0;
					break;
				}
			}
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG
				       "SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,camsv support no inner addr)\n",
				       sof_count[_CAMSV_D],
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST) & 0x00FF0000) >>
					16), ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE));
		}
		{
			unsigned long long sec;
			unsigned long usec;
			ktime_t time;

			time = ktime_get();	/* ns */
			sec = time.tv64;
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */

			ISP_CAMSV_SOF_Buf_Get(_camsv2_imgo_, sec, usec, gSof_camsvdone[1]);
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]));
	/* dump log during spin lock */
#ifdef ISR_LOG_ON
	IRQ_LOG_PRINTER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_ERR);
#endif
	wake_up_interruptible(&IspInfo.WaitQueueHead);
	return IRQ_HANDLED;
}

/* ///////////////////////////////////////////////////////////////////////////// */

/*******************************************************************************
*
********************************************************************************/
static __tcmfunc irqreturn_t ISP_Irq_CAM(MINT32 Irq, void *DeviceId)
{
	/* printk("+ ===== ISP_Irq =====\n"); */

/* LOG_DBG("- E."); */
	MUINT32 i;
	/* MUINT32 result=0x0; */
	MUINT32 j = 0;
	/* MINT32  idx=0; */
	MUINT32 IrqStatus[ISP_IRQ_TYPE_AMOUNT];
	/* MUINT32 IrqStatus_fbc_int; */
	CQ_RTBC_FBC p1_fbc[4];
	MUINT32 cur_v_cnt = 0;
	MUINT32 d_cur_v_cnt = 0;
#if 0
	if ((ISP_RD32(ISP_REG_ADDR_TG_VF_CON) & 0x1) == 0x0) {
		LOG_INF("before vf:0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
			ISP_RD32(ISP_REG_ADDR_INT_P1_ST), ISP_RD32(ISP_REG_ADDR_INT_P1_ST2),
			ISP_RD32(ISP_REG_ADDR_INT_P1_ST_D), ISP_RD32(ISP_REG_ADDR_INT_P1_ST2_D),
			ISP_RD32(ISP_REG_ADDR_INT_P2_ST), ISP_RD32(ISP_REG_ADDR_INT_STATUSX),
			ISP_RD32(ISP_REG_ADDR_INT_STATUS2X), ISP_RD32(ISP_REG_ADDR_INT_STATUS3X));
	}
#endif
	/* Read irq status */
	spin_lock(&(IspInfo.SpinLockIrq[_IRQ]));
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] =
	    (ISP_RD32(ISP_REG_ADDR_INT_P1_ST) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST]));
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2] = (ISP_RD32(ISP_REG_ADDR_INT_P1_ST2));	/* & (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2]|IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2])); */
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] =
	    (ISP_RD32(ISP_REG_ADDR_INT_P1_ST_D) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST_D] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST_D]));
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2_D] =
	    (ISP_RD32(ISP_REG_ADDR_INT_P1_ST2_D) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2_D] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2_D]));
	IrqStatus[ISP_IRQ_TYPE_INT_P2_ST] =
	    (ISP_RD32(ISP_REG_ADDR_INT_P2_ST) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P2_ST] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P2_ST]));

	/* below may need to read elsewhere */
	IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] =
	    (ISP_RD32(ISP_REG_ADDR_INT_STATUSX) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUSX] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX]));
	IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] =
	    (ISP_RD32(ISP_REG_ADDR_INT_STATUS2X) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS2X] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS2X]));
	IrqStatus[ISP_IRQ_TYPE_INT_STATUS3X] =
	    (ISP_RD32(ISP_REG_ADDR_INT_STATUS3X) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS3X] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS3X]));
	spin_unlock(&(IspInfo.SpinLockIrq[_IRQ]));
	/* IrqStatus_fbc_int = ISP_RD32(ISP_ADDR + 0xFC); */
#ifdef _rtbc_buf_que_2_0_	/* todo: need to separate p1 & p1_d */
	p1_fbc[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
	p1_fbc[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);

	p1_fbc[2].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
	p1_fbc[3].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);
#else
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		p1_fbc[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
		p1_fbc[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);

		p1_fbc[2].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
		p1_fbc[3].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);
	}
#endif
#if 1				/* err status mechanism */
#define STATUSX_WARNING (ISP_IRQ_STATUSX_ESFKO_ERR_ST|ISP_IRQ_STATUSX_RRZO_ERR_ST|ISP_IRQ_STATUSX_LCSO_ERR_ST|ISP_IRQ_STATUSX_AAO_ERR_ST|ISP_IRQ_STATUSX_IMGO_ERR_ST|ISP_IRQ_STATUSX_RRZO_ERR_ST)
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX] =
	    IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX] &
	    (~(ISP_IRQ_STATUSX_IMGO_DROP_FRAME_ST | ISP_IRQ_STATUSX_RRZO_DROP_FRAME_ST));
	/* in rome , p1 && p1_d share the same interrupt status */
	if (IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX]) {
		if ((IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & ISP_IRQ_STATUSX_DMA_ERR_ST)
		    || (IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] & ISP_IRQ_STATUSX_DMA_ERR_ST)) {
			g_bDmaERR_p1_d = MTRUE;
			g_bDmaERR_p1 = MTRUE;
			g_bDmaERR_deepDump = MFALSE;
			ISP_DumpDmaDeepDbg();
		}
		/* if(IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & ISP_IRQ_STATUSX_AAO_ERR_ST){ */
		/* ISP_DumpReg(); */
		/* } */
		/* mark, can ignor fifo may overrun if dma_err isn't pulled. */
		/* if(IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & STATUSX_WARNING){ */
		/* LOG_INF("warning: fifo may overrun"); */
		/* } */
		if (IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & (~STATUSX_WARNING)) {
			LOG_ERR("ISP INT ERR_P1 0x%x\n", IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]);
			g_ISPIntErr[_IRQ] |= IrqStatus[ISP_IRQ_TYPE_INT_STATUSX];
		}
		if (IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] & (~STATUSX_WARNING)) {
			LOG_ERR("ISP INT ERR_P1_D 0x%x\n", IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X]);
			g_ISPIntErr[_IRQ_D] |= IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X];
		}
	}
	/* LOG_INF("isp irq status:0x%x_0x%x",IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]); */
	/* LOG_INF("imgo fill:%d %d %d\n",pstRTBuf->ring_buf[_imgo_].data[0].bFilled,pstRTBuf->ring_buf[_imgo_].data[1].bFilled,pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
	/* LOG_INF("rrzo fill:%d %d %d\n",pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST)
	    || (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST))
		cur_v_cnt = ((ISP_RD32(ISP_REG_ADDR_TG_INTER_ST) & 0x00FF0000) >> 16);
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)
	    || (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_D_SOF1_INT_ST))
		d_cur_v_cnt = ((ISP_RD32(ISP_REG_ADDR_TG2_INTER_ST) & 0x00FF0000) >> 16);
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST)
	    && (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST)) {
		if (cur_v_cnt != sof_count[_PASS1])
			LOG_ERR("isp sof_don block, %d_%d\n", cur_v_cnt, sof_count[_PASS1]);
	}
#endif

#if 0
	/sensor interface would use another isr id
	    /* sensor interface related irq */
	    IrqStatus[ISP_IRQ_TYPE_INT_SENINF1] =
	    (ISP_RD32(ISP_REG_ADDR_SENINF1_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF1] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF1]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF2] =
	    (ISP_RD32(ISP_REG_ADDR_SENINF2_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF2] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF2]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF3] =
	    (ISP_RD32(ISP_REG_ADDR_SENINF3_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF3] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF3]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF4] =
	    (ISP_RD32(ISP_REG_ADDR_SENINF4_INT) &
	     (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF4] | IspInfo.
	      IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF4]));
#endif
	for (i = 0; i < ISP_IRQ_TYPE_ISP_AMOUNT; i++) {
#if 0
		if (IspInfo.IrqInfo.ErrMask[i] & IrqStatus[i]) {
			/* ISP_IRQ_INTX_STATUS_IMGO_ERR_ST: on-the-fly imgo error, not really overrun */
			if ((i != ISP_IRQ_TYPE_INT_STATUSX)
			    &&
			    ((IspInfo.
			      IrqInfo.ErrMask[i] & IrqStatus[i] & (~ISP_IRQ_STATUSX_IMGO_ERR_ST))))
			{
				/* IRQ_LOG_KEEPER(_IRQ,m_CurrentPPB,_LOG_ERR,"IRQ, Type(%d), Status(0x%x),p1(0x%x_0x%x_0x%x)\n",i,IspInfo.IrqInfo.ErrMask[i] & IrqStatus[i],IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],p1_fbc[0].Reg_val,p1_fbc[1].Reg_val); */
				/* TODO: Add error handler... */
			}
		}
#endif
		/*  */
#if 0
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			/* IRQ_LOG_KEEPER(_IRQ,m_CurrentPPB,_LOG_INF,"Type(%d), IrqStatus(0x%x | 0x%x)\n",i,IspInfo.IrqInfo.Status[i], IrqStatus[i]); */
		}
#endif
		for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
			IspInfo.IrqInfo.Status[j][i] |= (IrqStatus[i] & IspInfo.IrqInfo.Mask[i]);
		}
	}


	/* service pass1_done first once if SOF/PASS1_DONE are coming together. */
	/* get time stamp */
	/* push hw filled buffer to sw list */
	/* LOG_INF("RTBC_DBG %x_%x\n",IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]); */
	spin_lock(&(IspInfo.SpinLockIrq[_IRQ]));
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST) {
#ifdef _rtbc_buf_que_2_0_
		ISP_DONE_Buf_Time(_IRQ, 0, 0);	/* time stamp move to sof */
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "P1_DON_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1]) ? (sof_count[_PASS1] -
							      1) : (sof_count[_PASS1]),
				       (unsigned int)p1_fbc[0].Reg_val,
				       (unsigned int)p1_fbc[1].Reg_val);
		}
#else
#if defined(_rtbc_use_cq0c_)
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "P1_DON_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1]) ? (sof_count[_PASS1] -
							      1) : (sof_count[_PASS1]),
				       p1_fbc[0].Reg_val, p1_fbc[1].Reg_val);
		}
#else
		/* LOG_DBG("[k_js_test]Pass1_done(0x%x)",IrqStatus[ISP_IRQ_TYPE_INT]); */
		unsigned long long sec;
		unsigned long usec;
		sec = cpu_clock(0);	/* ns */
		do_div(sec, 1000);	/* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */

		ISP_DONE_Buf_Time(sec, usec);
		/*Check Timesamp reverse */
		/* what's this? */
		/*  */
#endif
#endif
	}
	/* switch pass1 WDMA buffer */
	/* fill time stamp for cq0c */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST) {
		MUINT32 _dmaport = 0;
		if (pstRTBuf->ring_buf[_imgo_].active)
			_dmaport = 0;
		else if (pstRTBuf->ring_buf[_rrzo_].active)
			_dmaport = 1;
		else
			LOG_ERR("no main dma port opened at SOF\n");
		/* chk this frame have EOF or not, dynimic dma port chk */
		if ((p1_fbc[_dmaport].Bits.FB_NUM == p1_fbc[_dmaport].Bits.FBC_CNT)
		    && (g1stErrhandle[_imgo_] == MFALSE) && (g1stErrhandle[_rrzo_] == MFALSE)) {
			sof_pass1done[0] = 1;
#ifdef _rtbc_buf_que_2_0_
			/* ISP_LostP1Done_ErrHandle(_imgo_); */
			/* ISP_LostP1Done_ErrHandle(_rrzo_); */
			/* IRQ_LOG_KEEPER(_IRQ,m_CurrentPPB,_LOG_INF,"lost p1Done ErrHandle\n"); */
#endif

			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "Lost p1 done_%d (0x%x)\n",
				       sof_count[_PASS1], cur_v_cnt);
		} else {
			sof_pass1done[0] = 0;
		}
#ifdef _rtbc_buf_que_2_0_
		{
			MUINT32 z;
			if (1 == mFwRcnt.bLoadBaseAddr[_IRQ]) {
				if (pstRTBuf->ring_buf[_imgo_].active) {
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "p1_%d:wr2Phy_0x%x", _imgo_,
						       pstRTBuf->ring_buf[_imgo_].
						       data[pstRTBuf->ring_buf
							    [_imgo_].start].base_pAddr);
					ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
						 pstRTBuf->ring_buf[_imgo_].
						 data[pstRTBuf->ring_buf[_imgo_].start].base_pAddr);
				}
				if (pstRTBuf->ring_buf[_rrzo_].active) {
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "p1_%d:wr2Phy_0x%x", _rrzo_,
						       pstRTBuf->ring_buf[_rrzo_].
						       data[pstRTBuf->ring_buf
							    [_rrzo_].start].base_pAddr);
					ISP_WR32(ISP_REG_ADDR_RRZO_BASE_ADDR,
						 pstRTBuf->ring_buf[_rrzo_].
						 data[pstRTBuf->ring_buf[_rrzo_].start].base_pAddr);
				}
				mFwRcnt.bLoadBaseAddr[_IRQ] = 0;
			}
			/* equal case is for clear curidx */
			for (z = 0; z <= mFwRcnt.curIdx[_IRQ]; z++) {
				/* LOG_INF("curidx:%d\n",mFwRcnt.curIdx[_IRQ]); */
				if (1 == mFwRcnt.INC[_IRQ][z]) {
					mFwRcnt.INC[_IRQ][z] = 0;
					p1_fbc[0].Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_IMGO_FBC, p1_fbc[0].Reg_val);
					p1_fbc[1].Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_RRZO_FBC, p1_fbc[1].Reg_val);
					if (IspInfo.DebugMask & ISP_DBG_INT)
						IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
							       "p1:RCNT_INC\n");
				} else {
					/* LOG_INF("RTBC_DBG:%d %d %d %d %d %d %d %d %d %d",mFwRcnt.INC[_IRQ][0],mFwRcnt.INC[_IRQ][1],mFwRcnt.INC[_IRQ][2],mFwRcnt.INC[_IRQ][3],mFwRcnt.INC[_IRQ][4],\ */
					/* mFwRcnt.INC[_IRQ][5],mFwRcnt.INC[_IRQ][6],mFwRcnt.INC[_IRQ][7],mFwRcnt.INC[_IRQ][8],mFwRcnt.INC[_IRQ][9]); */
					mFwRcnt.curIdx[_IRQ] = 0;
					break;
				}
			}
		}
#endif
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			p1_fbc[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);	/* in order to log newest fbc condition */
			p1_fbc[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
				       "P1_SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_PASS1], cur_v_cnt,
				       (unsigned int)p1_fbc[0].Reg_val,
				       (unsigned int)p1_fbc[1].Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
				       ISP_RD32(ISP_INNER_REG_ADDR_IMGO_YSIZE),
				       ISP_RD32(ISP_INNER_REG_ADDR_RRZO_YSIZE),
				       ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0));
		}
		{
			unsigned long long sec;
			unsigned long usec;
			ktime_t time;

			time = ktime_get();	/* ns */
			sec = time.tv64;
#ifdef T_STAMP_2_0
			if (g1stSof[_IRQ] == MTRUE) {
				m_T_STAMP.T_ns = sec;
			}
			if (m_T_STAMP.fps > SlowMotion) {
				m_T_STAMP.fcnt++;
				if (g1stSof[_IRQ] == MFALSE) {
					m_T_STAMP.T_ns +=
					    ((unsigned long long)m_T_STAMP.interval_us * 1000);
					if (m_T_STAMP.fcnt == m_T_STAMP.fps) {
						m_T_STAMP.fcnt = 0;
						m_T_STAMP.T_ns +=
						    ((unsigned long long)m_T_STAMP.compensation_us *
						     1000);
					}
				}
				sec = m_T_STAMP.T_ns;
			}
#endif
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */

			ISP_SOF_Buf_Get(_IRQ, sec, usec, sof_pass1done[0]);
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_IRQ]));
	spin_lock(&(IspInfo.SpinLockIrq[_IRQ]));
	/* TG_D Done */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_PASS1_DON_ST) {
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
				       "P1_D_DON_%d_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1_D]) ? (sof_count[_PASS1_D] -
								1) : (sof_count[_PASS1_D]),
				       d_cur_v_cnt, (unsigned int)p1_fbc[2].Reg_val,
				       (unsigned int)p1_fbc[3].Reg_val);
		}
#ifdef _rtbc_buf_que_2_0_
		ISP_DONE_Buf_Time(_IRQ_D, 0, 0);	/* time stamp move to sof */
#endif
	}
	/* TG_D SOF */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_SOF1_INT_ST) {
		MUINT32 _dmaport = 0;
#ifdef _rtbc_buf_que_2_0_
		MUINT32 z;
#endif
		if (pstRTBuf->ring_buf[_imgo_d_].active)
			_dmaport = 2;
		else if (pstRTBuf->ring_buf[_rrzo_d_].active)
			_dmaport = 3;
		else
			LOG_ERR("no main dma port opened at SOF_D\n");
		/* chk this frame have EOF or not,dynamic dma port chk */
		if ((p1_fbc[_dmaport].Bits.FB_NUM == p1_fbc[_dmaport].Bits.FBC_CNT)
		    && (g1stErrhandle[_imgo_d_] == MFALSE) && (g1stErrhandle[_rrzo_d_] == MFALSE)) {
			sof_pass1done[1] = 1;
#ifdef _rtbc_buf_que_2_0_
			/* ISP_LostP1Done_ErrHandle(_imgo_d_); */
			/* ISP_LostP1Done_ErrHandle(_rrzo_d_); */
			/* IRQ_LOG_KEEPER(_IRQ_D,m_CurrentPPB,_LOG_INF,"lost p1d_Done ErrHandle\n"); */
#endif

			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "Lost p1_d done_%d (0x%x)\n",
				       sof_count[_PASS1_D], d_cur_v_cnt);
		} else {
			sof_pass1done[1] = 0;
		}
#ifdef _rtbc_buf_que_2_0_
		if (1 == mFwRcnt.bLoadBaseAddr[_IRQ_D]) {
			if (pstRTBuf->ring_buf[_imgo_d_].active) {
				IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "p1_d_%d:wr2Phy\n",
					       _imgo_d_);
				ISP_WR32(ISP_REG_ADDR_IMGO_D_BASE_ADDR,
					 pstRTBuf->ring_buf[_imgo_d_].
					 data[pstRTBuf->ring_buf[_imgo_d_].start].base_pAddr);
			}
			if (pstRTBuf->ring_buf[_rrzo_d_].active) {
				IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "p1_d_%d:wr2Phy\n",
					       _rrzo_d_);
				ISP_WR32(ISP_REG_ADDR_RRZO_D_BASE_ADDR,
					 pstRTBuf->ring_buf[_rrzo_d_].
					 data[pstRTBuf->ring_buf[_rrzo_d_].start].base_pAddr);
			}
			mFwRcnt.bLoadBaseAddr[_IRQ_D] = 0;
		}
		/* equal case is for clear curidx */
		for (z = 0; z <= mFwRcnt.curIdx[_IRQ_D]; z++) {
			if (1 == mFwRcnt.INC[_IRQ_D][z]) {
				mFwRcnt.INC[_IRQ_D][z] = 0;
				p1_fbc[2].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC, p1_fbc[2].Reg_val);
				p1_fbc[3].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_RRZO_D_FBC, p1_fbc[3].Reg_val);
				if (IspInfo.DebugMask & ISP_DBG_INT)
					IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
						       "p1_d:RCNT_INC\n");
			} else {
				mFwRcnt.curIdx[_IRQ_D] = 0;
				break;
			}
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			p1_fbc[2].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
			p1_fbc[3].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);
			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
				       "P1_D_SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_PASS1_D], d_cur_v_cnt,
				       (unsigned int)p1_fbc[2].Reg_val,
				       (unsigned int)p1_fbc[3].Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR),
				       ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_YSIZE),
				       ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_YSIZE),
				       ISP_RD32(ISP_REG_ADDR_TG2_MAGIC_0));
		}
		{
			unsigned long long sec;
			unsigned long usec;
			ktime_t time;
			/*  */
			time = ktime_get();	/* ns */
			sec = time.tv64;
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */

			/*  */
			ISP_SOF_Buf_Get(_IRQ_D, sec, usec, sof_pass1done[1]);
		}
	}
	/* */
	/* make sure isr sequence r all done after this status switch */
	for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
		for (i = 0; i < ISP_IRQ_TYPE_ISP_AMOUNT; i++) {
			IspInfo.IrqInfo.Status[j][i] |= (IrqStatus[i] & IspInfo.IrqInfo.Mask[i]);
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_IRQ]));

	/* dump log during spin lock */
#ifdef ISR_LOG_ON
	IRQ_LOG_PRINTER(_IRQ, m_CurrentPPB, _LOG_INF);
	/* IRQ_LOG_PRINTER(_IRQ,m_CurrentPPB,_LOG_ERR); */

	IRQ_LOG_PRINTER(_IRQ_D, m_CurrentPPB, _LOG_INF);
	/* IRQ_LOG_PRINTER(_IRQ_D,m_CurrentPPB,_LOG_ERR); */
#endif
	/*  */
	wake_up_interruptible(&IspInfo.WaitQueueHead);

	/* Work queue. It is interruptible, so there can be "Sleep" in work queue function. */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_VS1_INT_ST)) &
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_VS1_INT_ST))) {
		IspInfo.TimeLog.Vd = ISP_JiffiesToMs(jiffies);
		schedule_work(&IspInfo.ScheduleWorkVD);
		tasklet_schedule(&IspTaskletVD);
	}
	/* Tasklet. It is uninterrupted, so there can NOT be "Sleep" in tasklet function. */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_EXPDON1_ST)) &
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_EXPDON1_ST))) {
		IspInfo.TimeLog.Expdone = ISP_JiffiesToMs(jiffies);
		schedule_work(&IspInfo.ScheduleWorkEXPDONE);
		tasklet_schedule(&IspTaskletEXPDONE);
	}
/* LOG_DBG("- X."); */

	/*  */
	/* printk("- ===== ISP_Irq =====\n"); */
	return IRQ_HANDLED;
}

/*******************************************************************************
*
********************************************************************************/
static long ISP_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	MINT32 Ret = 0;
	/*  */
	MBOOL HoldEnable = MFALSE;
	MUINT32 DebugFlag[2] = { 0 }, pid = 0;
	ISP_REG_IO_STRUCT RegIo;
	ISP_HOLD_TIME_ENUM HoldTime;
	ISP_WAIT_IRQ_STRUCT WaitIrq;
	ISP_READ_IRQ_STRUCT ReadIrq;
	ISP_CLEAR_IRQ_STRUCT ClearIrq;
	ISP_USER_INFO_STRUCT *pUserInfo;
	ISP_ED_BUFQUE_STRUCT edQueBuf;
	MUINT32 regScenInfo_value = 0xa5a5a5a5;
	MINT32 burstQNum;
	unsigned long flags;
	/*  */
	if (pFile->private_data == NULL) {
		LOG_WRN("private_data is NULL,(process, pid, tgid)=(%s, %d, %d)", current->comm,
			current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (ISP_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case ISP_GET_INT_ERR:
		if (copy_to_user((void *)Param, (void *)g_ISPIntErr, sizeof(MUINT32) * _IRQ_MAX) !=
		    0) {
			LOG_ERR("get int err fail\n");
		}
		break;
	case ISP_GET_DMA_ERR:
		if (copy_to_user((void *)Param, &g_DmaErr_p1[0], sizeof(MUINT32) * nDMA_ERR) != 0) {
			LOG_ERR("get dma_err fail\n");
		}
		break;
	case ISP_GET_CUR_SOF:
		if (copy_to_user((void *)Param, (void *)&sof_count[_IRQ], sizeof(MUINT32)) != 0) {
			LOG_ERR("get cur sof fail\n");
		}
		break;
	case ISP_RESET_CAM_P1:
		{
			spin_lock(&(IspInfo.SpinLockIsp));
			ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P1);
			spin_unlock(&(IspInfo.SpinLockIsp));
			break;
		}
	case ISP_RESET_CAM_P2:
		{
			spin_lock(&(IspInfo.SpinLockIsp));
			ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);
			spin_unlock(&(IspInfo.SpinLockIsp));
			break;
		}
	case ISP_RESET_CAMSV:
		{
			spin_lock(&(IspInfo.SpinLockIsp));
			ISP_Reset(ISP_REG_SW_CTL_RST_CAMSV);
			spin_unlock(&(IspInfo.SpinLockIsp));
			break;
		}
	case ISP_RESET_CAMSV2:
		{
			spin_lock(&(IspInfo.SpinLockIsp));
			ISP_Reset(ISP_REG_SW_CTL_RST_CAMSV2);
			spin_unlock(&(IspInfo.SpinLockIsp));
			break;
		}
	case ISP_RESET_BUF:
		{
			spin_lock_bh(&(IspInfo.SpinLockHold));
			ISP_ResetBuf();
			spin_unlock_bh(&(IspInfo.SpinLockHold));
			break;
		}
	case ISP_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param, sizeof(ISP_REG_IO_STRUCT)) == 0) {
				Ret = ISP_ReadReg(&RegIo);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param, sizeof(ISP_REG_IO_STRUCT)) == 0) {
				Ret = ISP_WriteReg(&RegIo);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_HOLD_REG_TIME:
		{
			if (copy_from_user(&HoldTime, (void *)Param, sizeof(ISP_HOLD_TIME_ENUM)) ==
			    0) {
				spin_lock(&(IspInfo.SpinLockIsp));
				Ret = ISP_SetHoldTime(HoldTime);
				spin_unlock(&(IspInfo.SpinLockIsp));
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_HOLD_REG:
		{
			if (copy_from_user(&HoldEnable, (void *)Param, sizeof(MBOOL)) == 0) {
				Ret = ISP_EnableHoldReg(HoldEnable);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_WAIT_IRQ:
		{
			if (copy_from_user(&WaitIrq, (void *)Param, sizeof(ISP_WAIT_IRQ_STRUCT)) ==
			    0) {
				/*  */
				if (WaitIrq.UserNumber >= ISP_IRQ_USER_MAX) {
					LOG_ERR("errUserEnum(%d)", WaitIrq.UserNumber);
					Ret = -EFAULT;
				}
				/*  */
				Ret = ISP_WaitIrq(&WaitIrq);
				if (copy_to_user
				    ((void *)Param, &WaitIrq, sizeof(ISP_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed");
					Ret = -EFAULT;
				}
				/* call unregister here */
				/* ISP_UnRegisterIrq(WaitIrq); */
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_READ_IRQ:
		{
			if (copy_from_user(&ReadIrq, (void *)Param, sizeof(ISP_READ_IRQ_STRUCT)) ==
			    0) {
				eISPIrq irqT = _IRQ;
				/*  */
				if (ReadIrq.UserNumber >= ISP_IRQ_USER_MAX) {
					LOG_ERR("errUserEnum(%d)", ReadIrq.UserNumber);
					Ret = -EFAULT;
				}
				/*  */
				switch (ReadIrq.Type) {
				case ISP_IRQ_TYPE_INT_CAMSV:
					irqT = _CAMSV_IRQ;
					break;
				case ISP_IRQ_TYPE_INT_CAMSV2:
					irqT = _CAMSV_D_IRQ;
					break;
				default:
					irqT = _IRQ;
					break;
				}
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags);
				ReadIrq.Status =
				    IspInfo.IrqInfo.Status[ReadIrq.UserNumber][ReadIrq.Type];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags);
				/*  */
				if (copy_to_user
				    ((void *)Param, &ReadIrq, sizeof(ISP_READ_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param, sizeof(ISP_CLEAR_IRQ_STRUCT))
			    == 0) {
				eISPIrq irqT;
				/*  */
				if (ClearIrq.UserNumber >= ISP_IRQ_USER_MAX) {
					LOG_ERR("errUserEnum(%d)", ClearIrq.UserNumber);
					Ret = -EFAULT;
				}
				/*  */
				switch (ClearIrq.Type) {
				case ISP_IRQ_TYPE_INT_CAMSV:
					irqT = _CAMSV_IRQ;
					break;
				case ISP_IRQ_TYPE_INT_CAMSV2:
					irqT = _CAMSV_D_IRQ;
					break;
				default:
					irqT = _IRQ;
					break;
				}
				LOG_DBG("ISP_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)",
					ClearIrq.Type, ClearIrq.Status,
					IspInfo.IrqInfo.Status[ClearIrq.UserNumber][ClearIrq.Type]);
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags);
				IspInfo.IrqInfo.Status[ClearIrq.UserNumber][ClearIrq.Type] &=
				    (~ClearIrq.Status);
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case ISP_REGISTER_IRQ:
#if 0
		if (copy_from_user(&WaitIrq, (void *)Param, sizeof(ISP_WAIT_IRQ_STRUCT)) == 0) {
			Ret = ISP_RegisterIrq(WaitIrq);
		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
#endif
		break;
	case ISP_UNREGISTER_IRQ:
#if 0
		if (copy_from_user(&WaitIrq, (void *)Param, sizeof(ISP_WAIT_IRQ_STRUCT)) == 0) {
			Ret = ISP_UnRegisterIrq(WaitIrq);
		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
#endif
		break;
		/*  */
	case ISP_ED_QUEBUF_CTRL:
		if (copy_from_user(&edQueBuf, (void *)Param, sizeof(ISP_ED_BUFQUE_STRUCT)) == 0) {
			edQueBuf.processID = pUserInfo->Pid;
			Ret = ISP_ED_BufQue_CTRL_FUNC(edQueBuf);
		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
		/*  */
	case ISP_UPDATE_REGSCEN:
		if (copy_from_user(&regScenInfo_value, (void *)Param, sizeof(MUINT32)) == 0) {
			spin_lock(&SpinLockRegScen);
			g_regScen = regScenInfo_value;
			spin_unlock(&SpinLockRegScen);
		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_QUERY_REGSCEN:
		spin_lock(&SpinLockRegScen);
		regScenInfo_value = g_regScen;
		spin_unlock(&SpinLockRegScen);
		/*  */
		if (copy_to_user((void *)Param, &regScenInfo_value, sizeof(MUINT32)) != 0) {
			LOG_ERR("copy_to_user failed");
			Ret = -EFAULT;
		}
		break;
		/*  */
	case ISP_UPDATE_BURSTQNUM:
		if (copy_from_user(&burstQNum, (void *)Param, sizeof(MINT32)) == 0) {
			spin_lock(&SpinLockRegScen);
			P2_Support_BurstQNum = burstQNum;
			spin_unlock(&SpinLockRegScen);
			LOG_DBG("new BurstQNum(%d)", P2_Support_BurstQNum);
		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_QUERY_BURSTQNUM:
		spin_lock(&SpinLockRegScen);
		burstQNum = P2_Support_BurstQNum;
		spin_unlock(&SpinLockRegScen);
		 /**/ if (copy_to_user((void *)Param, &burstQNum, sizeof(MUINT32)) != 0) {
			LOG_ERR("copy_to_user failed");
			Ret = -EFAULT;
		}
		break;
	 /**/ case ISP_DUMP_REG:
		{
			Ret = ISP_DumpReg();
			break;
		}
	case ISP_DEBUG_FLAG:
		{
			if (copy_from_user(DebugFlag, (void *)Param, sizeof(MUINT32) * 2) == 0) {
				MUINT32 lock_key = _IRQ_MAX;
				if (DebugFlag[1] == _IRQ_D)
					lock_key = _IRQ;
				else
					lock_key = DebugFlag[1];
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[lock_key]), flags);
				IspInfo.DebugMask = DebugFlag[0];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[lock_key]), flags);
				LOG_DBG("FBC kernel debug level = %x\n", IspInfo.DebugMask);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
#ifdef ISP_KERNEL_MOTIFY_SINGAL_TEST
	case ISP_SET_USER_PID:
		{
			if (copy_from_user(&pid, (void *)Param, sizeof(MUINT32)) == 0) {
				spin_lock(&(IspInfo.SpinLockIsp));
				getTaskInfo((pid_t) pid);

				sendSignal();

				LOG_DBG("[ISP_KERNEL_MOTIFY_SINGAL_TEST]:0x08%x ", pid);
				spin_unlock(&(IspInfo.SpinLockIsp));
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}

			break;
		}
#endif
	case ISP_BUFFER_CTRL:
		Ret = ISP_Buf_CTRL_FUNC(Param);
		break;
	case ISP_REF_CNT_CTRL:
		Ret = ISP_REF_CNT_CTRL_FUNC(Param);
		break;
	case ISP_DUMP_ISR_LOG:
		if (copy_from_user(DebugFlag, (void *)Param, sizeof(MUINT32)) == 0) {
			MUINT32 currentPPB = m_CurrentPPB;
			MUINT32 lock_key = _IRQ_MAX;
			if (DebugFlag[0] == _IRQ_D)
				lock_key = _IRQ;
			else
				lock_key = DebugFlag[0];
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[lock_key]), flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[lock_key]), flags);

			IRQ_LOG_PRINTER(DebugFlag[0], currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(DebugFlag[0], currentPPB, _LOG_ERR);

		} else {
			LOG_ERR("copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
#ifdef T_STAMP_2_0
	case ISP_SET_FPS:
		if (copy_from_user(DebugFlag, (void *)Param, sizeof(MUINT32)) == 0) {
			if (m_T_STAMP.fps == 0) {
				m_T_STAMP.fps = DebugFlag[0];
				m_T_STAMP.interval_us = 1000000 / m_T_STAMP.fps;
				m_T_STAMP.compensation_us =
				    1000000 - (m_T_STAMP.interval_us * m_T_STAMP.fps);
			} else {
				LOG_ERR("copy_from_user failed");
				Ret = -EFAULT;
			}
		}
		break;
#endif

	default:
		{
			LOG_ERR("Unknown Cmd(%d)", Cmd);
			Ret = -EPERM;
			break;
		}
	}
	/*  */
	if (Ret != 0) {
		LOG_ERR("Fail, Cmd(%d), Pid(%d), (process, pid, tgid)=(%s, %d, %d)", Cmd,
			pUserInfo->Pid, current->comm, current->pid, current->tgid);
	}
	/*  */
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_open(struct inode *pInode, struct file *pFile)
{
	MINT32 Ret = 0;
	MUINT32 i;
	int q = 0;
	ISP_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.", IspInfo.UserCount);
	/*  */
	spin_lock(&(IspInfo.SpinLockIspRef));
	/*  */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*  */
	pFile->private_data = NULL;
	pFile->private_data = kmalloc(sizeof(ISP_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG("ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)", current->comm,
			current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (ISP_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (IspInfo.UserCount > 0) {
		IspInfo.UserCount++;
		LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			IspInfo.UserCount, current->comm, current->pid, current->tgid);
		goto EXIT;
	}
	/* do wait queue head init when re-enter in camera */
	EDBufQueRemainNodeCnt = 0;
	P2_Support_BurstQNum = 1;
	/*  */
	for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
		P2_EDBUF_RingList[i].processID = 0x0;
		P2_EDBUF_RingList[i].callerID = 0x0;
		P2_EDBUF_RingList[i].p2dupCQIdx = -1;
		P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
	}
	P2_EDBUF_RList_FirstBufIdx = 0;
	P2_EDBUF_RList_CurBufIdx = 0;
	P2_EDBUF_RList_LastBufIdx = -1;
	/*  */
	for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
		P2_EDBUF_MgrList[i].processID = 0x0;
		P2_EDBUF_MgrList[i].callerID = 0x0;
		P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[i].dequedNum = 0;
	}
	P2_EDBUF_MList_FirstBufIdx = 0;
	P2_EDBUF_MList_LastBufIdx = -1;
	/*  */
	g_regScen = 0xa5a5a5a5;
	/*  */
	IspInfo.BufInfo.Read.pData = (MUINT8 *) kmalloc(ISP_BUF_SIZE, GFP_ATOMIC);
	IspInfo.BufInfo.Read.Size = ISP_BUF_SIZE;
	IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	if (IspInfo.BufInfo.Read.pData == NULL) {
		LOG_DBG("ERROR: BufRead kmalloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	if (!ISP_BufWrite_Alloc()) {
		LOG_DBG("ERROR: BufWrite kmalloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), 0);
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);
	for (i = 0; i < ISP_REF_CNT_ID_MAX; i++) {
		atomic_set(&g_imem_ref_cnt[i], 0);
	}
	/* Enable clock */
	ISP_EnableClock(MTRUE);
	/*  */

	for (q = 0; q < ISP_IRQ_USER_MAX; q++) {
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			IspInfo.IrqInfo.Status[q][i] = 0;
		}
	}
	for (i = 0; i < ISP_CALLBACK_AMOUNT; i++) {
		IspInfo.Callback[i].Func = NULL;
	}
	/*  */
	IspInfo.UserCount++;
	LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
		IspInfo.UserCount, current->comm, current->pid, current->tgid);
	/*  */


#ifdef KERNEL_LOG
	/*IspInfo.DebugMask = (ISP_DBG_INT | ISP_DBG_BUF_CTRL); */
#endif
	/*  */
 EXIT:
	if (Ret < 0) {
		if (IspInfo.BufInfo.Read.pData != NULL) {
			kfree(IspInfo.BufInfo.Read.pData);
			IspInfo.BufInfo.Read.pData = NULL;
		}
		/*  */
		ISP_BufWrite_Free();
	}
	/*  */
	spin_unlock(&(IspInfo.SpinLockIspRef));
	/*  */

	/* LOG_DBG("Before spm_disable_sodi()."); */
	/* Disable sodi (Multi-Core Deep Idle). */


#if 0				/* _mt6593fpga_dvt_use_ */
	spm_disable_sodi();
#endif

	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, IspInfo.UserCount);
	return Ret;

}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_release(struct inode *pInode, struct file *pFile)
{
	ISP_USER_INFO_STRUCT *pUserInfo;
	MUINT32 Reg;
	LOG_DBG("- E. UserCount: %d.", IspInfo.UserCount);
	/*  */

	/*  */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo = (ISP_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.UserCount--;
	if (IspInfo.UserCount > 0) {
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			IspInfo.UserCount, current->comm, current->pid, current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(IspInfo.SpinLockIspRef));
	/*  */
	LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		IspInfo.UserCount, current->comm, current->pid, current->tgid);

	/* reason of close vf is to make sure camera can serve regular after previous abnormal exit */
	Reg = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
	Reg &= 0xfffffffE;	/* close Vfinder */
	ISP_WR32(ISP_REG_ADDR_TG_VF_CON, Reg);

	Reg = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
	Reg &= 0xfffffffE;	/* close Vfinder */
	ISP_WR32(ISP_REG_ADDR_TG2_VF_CON, Reg);

	/* Disable clock. */
	ISP_EnableClock(MFALSE);

	if (IspInfo.BufInfo.Read.pData != NULL) {
		kfree(IspInfo.BufInfo.Read.pData);
		IspInfo.BufInfo.Read.pData = NULL;
		IspInfo.BufInfo.Read.Size = 0;
		IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	}
	/*  */
	ISP_BufWrite_Free();
	/*  */
 EXIT:

	/*  */
	/* LOG_DBG("Before spm_enable_sodi()."); */
	/* Enable sodi (Multi-Core Deep Idle). */

#if 0				/* _mt6593fpga_dvt_use_ */
	spm_enable_sodi();
#endif

	LOG_DBG("- X. UserCount: %d.", IspInfo.UserCount);
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
/* helper function, mmap's the kmalloc'd area which is physically contiguous */
static MINT32 mmap_kmem(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	unsigned long length = 0;
	length = vma->vm_end - vma->vm_start;

	/* check length - do not allow larger mappings than the number of
	   pages allocated */
	if (length > RT_BUF_TBL_NPAGES * PAGE_SIZE)
		return -EIO;

	/* map the whole physically contiguous area in one piece */
	LOG_DBG("Vma->vm_pgoff(0x%lx),Vma->vm_start(0x%lx),Vma->vm_end(0x%lx),length(0x%lx)",
		vma->vm_pgoff, vma->vm_start, vma->vm_end, length);
	if (length > ISP_RTBUF_REG_RANGE) {
		LOG_ERR("mmap range error! : length(0x%x),ISP_RTBUF_REG_RANGE(0x%x)!",
			(unsigned int)length, ISP_RTBUF_REG_RANGE);
		return -EAGAIN;
	}
	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      virt_to_phys((void *)pTbl_RTBuf) >> PAGE_SHIFT,
			      length, vma->vm_page_prot);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	MUINT32 pfn = 0x0;
	LOG_DBG("- E.");
	length = pVma->vm_end - pVma->vm_start;
	/* at offset RT_BUF_TBL_NPAGES we map the kmalloc'd area */
	if (pVma->vm_pgoff == RT_BUF_TBL_NPAGES) {
		return mmap_kmem(pFile, pVma);
	} else {
		/*  */
		pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
		LOG_DBG
		    ("pVma->vm_pgoff(0x%lx),phy(0x%lx),pVmapVma->vm_start(0x%lx),pVma->vm_end(0x%lx),length(0x%lx)",
		     pVma->vm_pgoff, pVma->vm_pgoff << PAGE_SHIFT, pVma->vm_start, pVma->vm_end,
		     length);
		pfn = pVma->vm_pgoff << PAGE_SHIFT;	/* page from number, physical address of kernel memory */
		switch (pfn) {
		case IMGSYS_BASE_ADDR:	/* imgsys */
			if (length > ISP_REG_RANGE) {
				LOG_ERR("mmap range error : length(0x%lx),ISP_REG_RANGE(0x%x)!",
					length, ISP_REG_RANGE);
				return -EAGAIN;
			}
			break;
		case SENINF_BASE_ADDR:
			if (length > SENINF_REG_RANGE) {
				LOG_ERR("mmap range error : length(0x%lx),SENINF_REG_RANGE(0x%x)!",
					length, SENINF_REG_RANGE);
				return -EAGAIN;
			}
			break;
		case PLL_BASE_ADDR:
			if (length > PLL_RANGE) {
				LOG_ERR("mmap range error : length(0x%lx),PLL_RANGE(0x%x)!", length,
					PLL_RANGE);
				return -EAGAIN;
			}
			break;
		case MIPIRX_CONFIG_ADDR:
			if (length > MIPIRX_CONFIG_RANGE) {
				LOG_ERR
				    ("mmap range error : length(0x%lx),MIPIRX_CONFIG_RANGE(0x%x)!",
				     length, MIPIRX_CONFIG_RANGE);
				return -EAGAIN;
			}
			break;
		case MIPIRX_ANALOG_ADDR:
			if (length > MIPIRX_ANALOG_RANGE) {
				LOG_ERR
				    ("mmap range error : length(0x%lx),MIPIRX_ANALOG_RANGE(0x%x)!",
				     length, MIPIRX_ANALOG_RANGE);
				return -EAGAIN;
			}
			break;
		case GPIO_BASE_ADDR:
			if (length > GPIO_RANGE) {
				LOG_ERR("mmap range error : length(0x%lx),GPIO_RANGE(0x%x)!",
					length, GPIO_RANGE);
				return -EAGAIN;
			}
			break;
		default:
			LOG_ERR("Illegal starting HW addr for mmap!");
			return -EAGAIN;
			break;
		}
		if (remap_pfn_range
		    (pVma, pVma->vm_start, pVma->vm_pgoff, pVma->vm_end - pVma->vm_start,
		     pVma->vm_page_prot)) {
			return -EAGAIN;
		}
	}
	/*  */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static dev_t IspDevNo;
static struct cdev *pIspCharDrv;
static struct class *pIspClass;

static const struct file_operations IspFileOper = {
	.owner = THIS_MODULE,
	.open = ISP_open,
	.release = ISP_release,
	/* .flush   = mt_isp_flush, */
	.mmap = ISP_mmap,
	.unlocked_ioctl = ISP_ioctl
};

/*******************************************************************************
*
********************************************************************************/
static void ISP_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pIspCharDrv != NULL) {
		cdev_del(pIspCharDrv);
		pIspCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(IspDevNo, 1);
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_RegCharDev(void)
{
	MINT32 Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&IspDevNo, 0, 1, ISP_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pIspCharDrv = cdev_alloc();
	if (pIspCharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pIspCharDrv, &IspFileOper);
	/*  */
	pIspCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pIspCharDrv, IspDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
 EXIT:
	if (Ret < 0) {
		ISP_UnregCharDev();
	}
	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_probe(struct platform_device *pDev)
{
	MINT32 Ret = 0;
	struct resource *pRes = NULL;
	MINT32 i = 0;
	MUINT8 n;
	/* MINT32 j=0; */
	/*  */
	printk("kk:+ ISP_probe\n");

	LOG_DBG("- E.");
	/* Check platform_device parameters */
	if (pDev == NULL) {
		dev_err(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}
	/* Register char driver */
	Ret = ISP_RegCharDev();
	if (Ret) {
		dev_err(&pDev->dev, "register char failed");
		return Ret;
	}
	/* Mapping CAM_REGISTERS */
	for (i = 0; i < 1; i++) {	/* NEED_TUNING_BY_CHIP. 1: Only one IORESOURCE_MEM type resource in kernel\mt_devs.c\mt_resource_isp[]. */
		LOG_DBG("Mapping CAM_REGISTERS. i: %d.", i);
		pRes = platform_get_resource(pDev, IORESOURCE_MEM, i);
		if (pRes == NULL) {
			dev_err(&pDev->dev, "platform_get_resource failed");
			Ret = -ENOMEM;
			goto EXIT;
		}
		pRes = request_mem_region(pRes->start, pRes->end - pRes->start + 1, pDev->name);
		if (pRes == NULL) {
			dev_err(&pDev->dev, "request_mem_region failed");
			Ret = -ENOMEM;
			goto EXIT;
		}
	}
	/* Create class register */
	pIspClass = class_create(THIS_MODULE, "ispdrv");
	if (IS_ERR(pIspClass)) {
		Ret = PTR_ERR(pIspClass);
		LOG_ERR("Unable to create class, err = %d", Ret);
		return Ret;
	}
	/* FIXME: error handling */
	device_create(pIspClass, NULL, IspDevNo, NULL, ISP_DEV_NAME);
	/*  */
	init_waitqueue_head(&IspInfo.WaitQueueHead);
	/*  */
	INIT_WORK(&IspInfo.ScheduleWorkVD, ISP_ScheduleWork_VD);
	INIT_WORK(&IspInfo.ScheduleWorkEXPDONE, ISP_ScheduleWork_EXPDONE);
	/*  */
	spin_lock_init(&(IspInfo.SpinLockIspRef));
	spin_lock_init(&(IspInfo.SpinLockIsp));
	for (n = 0; n < _IRQ_MAX; n++)
		spin_lock_init(&(IspInfo.SpinLockIrq[n]));
	spin_lock_init(&(IspInfo.SpinLockHold));
	spin_lock_init(&(IspInfo.SpinLockRTBC));
	spin_lock_init(&(IspInfo.SpinLockClock));
	/*  */
	IspInfo.UserCount = 0;
	IspInfo.HoldInfo.Time = ISP_HOLD_TIME_EXPDONE;
	/*  */
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST] = ISP_REG_MASK_INT_P1_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2] = ISP_REG_MASK_INT_P1_ST2;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST_D] = ISP_REG_MASK_INT_P1_ST_D;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2_D] = ISP_REG_MASK_INT_P1_ST2_D;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P2_ST] = ISP_REG_MASK_INT_P2_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUSX] = ISP_REG_MASK_INT_STATUSX;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS2X] = ISP_REG_MASK_INT_STATUS2X;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS3X] = ISP_REG_MASK_INT_STATUS3X;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF1] = ISP_REG_MASK_INT_SENINF1;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF2] = ISP_REG_MASK_INT_SENINF2;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF3] = ISP_REG_MASK_INT_SENINF3;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF4] = ISP_REG_MASK_INT_SENINF4;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV] = ISP_REG_MASK_CAMSV_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2] = ISP_REG_MASK_CAMSV2_ST;
	/*  */
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST] = ISP_REG_MASK_INT_P1_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2] = ISP_REG_MASK_INT_P1_ST2_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST_D] = ISP_REG_MASK_INT_P1_ST_D_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2_D] = ISP_REG_MASK_INT_P1_ST2_D_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P2_ST] = ISP_REG_MASK_INT_P2_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX] = ISP_REG_MASK_INT_STATUSX_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS2X] = ISP_REG_MASK_INT_STATUS2X_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS3X] = ISP_REG_MASK_INT_STATUS3X_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF1] = ISP_REG_MASK_INT_SENINF1_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF2] = ISP_REG_MASK_INT_SENINF2_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF3] = ISP_REG_MASK_INT_SENINF3_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF4] = ISP_REG_MASK_INT_SENINF4_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV] = ISP_REG_MASK_CAMSV_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2] = ISP_REG_MASK_CAMSV2_ST_ERR;
	/*  */
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_P1_ST] = ISP_REG_MASK_INT_P1_ST_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_P1_ST2] = ISP_REG_MASK_INT_P1_ST2_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_P1_ST_D] = ISP_REG_MASK_INT_P1_ST_D_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_P1_ST2_D] = ISP_REG_MASK_INT_P1_ST2_D_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_P2_ST] = ISP_REG_MASK_INT_P2_ST_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_STATUSX] = ISP_REG_MASK_INT_STATUSX_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_STATUS2X] = ISP_REG_MASK_INT_STATUS2X_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_STATUS3X] = ISP_REG_MASK_INT_STATUS3X_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_SENINF1] = ISP_REG_MASK_INT_SENINF1_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_SENINF2] = ISP_REG_MASK_INT_SENINF2_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_SENINF3] = ISP_REG_MASK_INT_SENINF3_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_SENINF4] = ISP_REG_MASK_INT_SENINF4_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_CAMSV] = ISP_REG_MASK_CAMSV_ST_WAITQ;
	IspInfo.IrqInfo.WaitQMask[ISP_IRQ_TYPE_INT_CAMSV2] = ISP_REG_MASK_CAMSV2_ST_WAITQ;

	/* enqueue/dequeue control in ihalpipe wrapper */
	init_waitqueue_head((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitDeque);
	init_waitqueue_head((wait_queue_head_t *) &WaitQueueHead_EDBuf_WaitFrame);
	spin_lock_init(&(SpinLockEDBufQueList));
	spin_lock_init((&SpinLockRegScen));

	/* Request CAM_ISP IRQ */
#if 1				/* FIXME */
	if (request_irq
	    (CAM0_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAM, IRQF_TRIGGER_LOW, "ISP", NULL))
/* if (request_irq(CAMERA_ISP_IRQ0_ID, (irq_handler_t)ISP_Irq, IRQF_TRIGGER_HIGH, "isp", NULL)) */
	{
		LOG_ERR("MT6593_CAM_IRQ_LINE IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
	/* mt_irq_unmask(CAMERA_ISP_IRQ0_ID); */
	/* request CAM_SV IRQ */
	if (request_irq
	    (CAM_SV0_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAMSV, IRQF_TRIGGER_LOW, "ISP", NULL)) {
		LOG_ERR("MT6593_CAMSV1_IRQ_LINE IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
	/* request CAM_SV2 IRQ */
	if (request_irq
	    (CAM_SV1_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAMSV2, IRQF_TRIGGER_LOW, "ISP", NULL)) {
		LOG_ERR("MT6593_CAMSV2_IRQ_LINE IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
#endif

 EXIT:
	if (Ret < 0) {
		ISP_UnregCharDev();
	}
	/*  */
	LOG_DBG("- X.");
	/*  */
	printk("kk:- ISP_probe\n");
	/*  */
	return Ret;
}

/*******************************************************************************
* Called when the device is being detached from the driver
********************************************************************************/
static MINT32 ISP_remove(struct platform_device *pDev)
{
	struct resource *pRes;
	MINT32 i;
	MINT32 IrqNum;
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	ISP_UnregCharDev();
	/* unmaping ISP CAM_REGISTER registers */
	for (i = 0; i < 2; i++) {
		pRes = platform_get_resource(pDev, IORESOURCE_MEM, 0);
		release_mem_region(pRes->start, (pRes->end - pRes->start + 1));
	}
	/* Release IRQ */
	disable_irq(IspInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);
#if 0
	/* free all registered irq(child nodes) */
	ISP_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;
	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;
			typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (father);
			accessNode =
			    ((REG_IRQ_NODE *) ((char *)__mptr - offsetof(REG_IRQ_NODE, list)));
			LOG_INF("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {	/* last father node */
				head->nextirq = head;
				LOG_INF("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pIspClass, IspDevNo);
	/*  */
	class_destroy(pIspClass);
	pIspClass = NULL;
	/*  */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 bPass1_On_In_Resume_TG1;
static MINT32 bPass1_On_In_Resume_TG2;
static MINT32 ISP_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/* TG_VF_CON[0] (0x15004414[0]): VFDATA_EN. TG1 Take Picture Request. */
	MUINT32 regTG1Val = ISP_RD32(ISP_ADDR + 0x414);
	/* TG2_VF_CON[0] (0x150044B4[0]): VFDATA_EN. TG2 Take Picture Request. */
	MUINT32 regTG2Val = ISP_RD32(ISP_ADDR + 0x4B4);

	LOG_DBG
	    ("bPass1_On_In_Resume_TG1(%d). bPass1_On_In_Resume_TG2(%d). regTG1Val(0x%08x). regTG2Val(0x%08x)\n",
	     bPass1_On_In_Resume_TG1, bPass1_On_In_Resume_TG2, regTG1Val, regTG2Val);

	bPass1_On_In_Resume_TG1 = 0;
	if (regTG1Val & 0x01) {	/* For TG1 Main sensor. */
		bPass1_On_In_Resume_TG1 = 1;
		ISP_WR32(ISP_ADDR + 0x414, (regTG1Val & (~0x01)));
	}

	bPass1_On_In_Resume_TG2 = 0;
	if (regTG2Val & 0x01) {	/* For TG2 Sub sensor. */
		bPass1_On_In_Resume_TG2 = 1;
		ISP_WR32(ISP_ADDR + 0x4B4, (regTG2Val & (~0x01)));
	}

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_resume(struct platform_device *pDev)
{
	/* TG_VF_CON[0] (0x15004414[0]): VFDATA_EN. TG1 Take Picture Request. */
	MUINT32 regTG1Val = ISP_RD32(ISP_ADDR + 0x414);
	/* TG2_VF_CON[0] (0x150044B4[0]): VFDATA_EN. TG2 Take Picture Request. */
	MUINT32 regTG2Val = ISP_RD32(ISP_ADDR + 0x4B4);

	LOG_DBG
	    ("bPass1_On_In_Resume_TG1(%d). bPass1_On_In_Resume_TG2(%d). regTG1Val(0x%x) regTG2Val(0x%x)\n",
	     bPass1_On_In_Resume_TG1, bPass1_On_In_Resume_TG2, regTG1Val, regTG2Val);

	if (bPass1_On_In_Resume_TG1) {
		bPass1_On_In_Resume_TG1 = 0;
		ISP_WR32(ISP_ADDR + 0x414, (regTG1Val | 0x01));	/* For TG1 Main sensor. */
	}

	if (bPass1_On_In_Resume_TG2) {
		bPass1_On_In_Resume_TG2 = 0;
		ISP_WR32(ISP_ADDR + 0x4B4, (regTG2Val | 0x01));	/* For TG2 Sub sensor. */
	}

	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int ISP_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	BUG_ON(pdev == NULL);

	pr_debug("calling %s()\n", __func__);

	return ISP_suspend(pdev, PMSG_SUSPEND);
}

int ISP_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	BUG_ON(pdev == NULL);

	pr_debug("calling %s()\n", __func__);

	return ISP_resume(pdev);
}


int ISP_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);

	mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);

	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define ISP_pm_suspend NULL
#define ISP_pm_resume  NULL
#define ISP_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
const struct dev_pm_ops ISP_pm_ops = {
	.suspend = ISP_pm_suspend,
	.resume = ISP_pm_resume,
	.freeze = ISP_pm_suspend,
	.thaw = ISP_pm_resume,
	.poweroff = ISP_pm_suspend,
	.restore = ISP_pm_resume,
	.restore_noirq = ISP_pm_restore_noirq,
};


/*******************************************************************************
*
********************************************************************************/
static struct platform_driver IspDriver = {
	.probe = ISP_probe,
	.remove = ISP_remove,
	.suspend = ISP_suspend,
	.resume = ISP_resume,
	.driver = {
		   .name = ISP_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &ISP_pm_ops,
#endif
		   }
};

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_DumpRegToProc(struct file *pFile,
				char __user *pBuffer, size_t Count, loff_t *off)
{
	char *p = (char *)pFile;
	char **ppStart = NULL;
	MINT32 Length = 0;
	MUINT32 i = 0;
	MINT32 ret = 0;
	/*  */
	LOG_DBG("- E. pPage: 0x%08x. off: %d. Count: %d.", (unsigned int)((char *)pFile),
		(unsigned int)(*off), Count);
	/*  */
	p += sprintf(p, " MT6593 ISP Register\n");
	p += sprintf(p, "====== top ====\n");
	for (i = 0x0; i <= 0x1AC; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	p += sprintf(p, "====== dma ====\n");
	for (i = 0x200; i <= 0x3D8; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n\r", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	p += sprintf(p, "====== tg ====\n");
	for (i = 0x400; i <= 0x4EC; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	p += sprintf(p, "====== cdp (including EIS) ====\n");
	for (i = 0xB00; i <= 0xDE0; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	p += sprintf(p, "====== seninf ====\n");
	for (i = 0x4000; i <= 0x40C0; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4100; i <= 0x41BC; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4200; i <= 0x4208; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4300; i <= 0x4310; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x43A0; i <= 0x43B0; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4400; i <= 0x4424; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4500; i <= 0x4520; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4600; i <= 0x4608; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	for (i = 0x4A00; i <= 0x4A08; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	p += sprintf(p, "====== 3DNR ====\n");
	for (i = 0x4F00; i <= 0x4F38; i += 4) {
		p += sprintf(p, "+0x%08x 0x%08x\n", ISP_ADDR + i, ISP_RD32(ISP_ADDR + i));
	}
	/*  */
	*ppStart = (char *)pFile + (*off);
	/*  */
	Length = p - (char *)pFile;
	if (Length > (*off)) {
		Length -= (*off);
	} else {
		Length = 0;
	}
	/*  */

	ret = Length < Count ? Length : Count;

	LOG_DBG("- X. ret: %d.", ret);

	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 ISP_RegDebug(struct file *pFile,
			   const char __user *pBuffer, size_t Count, loff_t *pData)
{
	char RegBuf[64];
	MUINT32 CopyBufSize = (Count < (sizeof(RegBuf) - 1)) ? (Count) : (sizeof(RegBuf) - 1);
	MUINT32 Addr = 0;
	MUINT32 Data = 0;

	LOG_DBG("- E. pFile: 0x%08x. pBuffer: 0x%08x. Count: %d.", (unsigned int)pFile,
		(unsigned int)pBuffer, (int)Count);
	/*  */
	if (copy_from_user(RegBuf, pBuffer, CopyBufSize)) {
		LOG_ERR("copy_from_user() fail.");
		return -EFAULT;
	}
	/*  */
	if (sscanf(RegBuf, "%x %x", &Addr, &Data) == 2) {
		ISP_WR32(ISP_ADDR_CAMINF + Addr, Data);
		LOG_DBG("Write => Addr: 0x%08X, Write Data: 0x%08X. Read Data: 0x%08X.",
			ISP_ADDR_CAMINF + Addr, Data, ioread32((void *)(ISP_ADDR_CAMINF + Addr)));
	} else if (sscanf(RegBuf, "%x", &Addr) == 1) {
		LOG_DBG("Read => Addr: 0x%08X, Read Data: 0x%08X.", ISP_ADDR_CAMINF + Addr,
			ioread32((void *)(ISP_ADDR_CAMINF + Addr)));
	}
	/*  */
	LOG_DBG("- X. Count: %d.", (int)Count);
	return Count;
}

static MUINT32 proc_regOfst;
static MINT32 CAMIO_DumpRegToProc(struct file *pFile,
				  char __user *pBuffer, size_t Count, loff_t *off)
{
	char *p = (char *)pFile;
	char **ppStart = NULL;
	MINT32 Length = 0;
	MINT32 ret = 0;
	/*  */
	LOG_DBG("- E. pPage: 0x%08x. off: %d. Count: %d.", (unsigned int)((char *)pFile),
		(int)(*off), Count);
	/*  */
	p += sprintf(p, "reg_0x%08X = 0x%X\n", ISP_ADDR_CAMINF + proc_regOfst,
		     ioread32((void *)(ISP_ADDR_CAMINF + proc_regOfst)));

	*ppStart = (char *)pFile + (*off);
	/*  */
	Length = p - (char *)pFile;
	if (Length > (*off)) {
		Length -= (*off);
	} else {
		Length = 0;
	}
	/*  */

	ret = Length < Count ? Length : Count;

	LOG_DBG("- X. ret: %d.", ret);

	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static MINT32 CAMIO_RegDebug(struct file *pFile,
			     const char __user *pBuffer, size_t Count, loff_t *pData)
{
	char RegBuf[64];
	MUINT32 CopyBufSize = (Count < (sizeof(RegBuf) - 1)) ? (Count) : (sizeof(RegBuf) - 1);
	MUINT32 Addr = 0;
	MUINT32 Data = 0;
	LOG_DBG("- E. pFile: 0x%08x. pBuffer: 0x%08x. Count: %d.", (unsigned int)pFile,
		(unsigned int)pBuffer, (int)Count);

	/*  */
	if (copy_from_user(RegBuf, pBuffer, CopyBufSize)) {
		LOG_ERR("copy_from_user() fail.");
		return -EFAULT;
	}
	/*  */
	if (sscanf(RegBuf, "%x %x", &Addr, &Data) == 2) {
		proc_regOfst = Addr;
		ISP_WR32(GPIO_BASE + Addr, Data);
		LOG_DBG("Write => Addr: 0x%08X, Write Data: 0x%08X. Read Data: 0x%08X.",
			GPIO_BASE + Addr, Data, ioread32((void *)(GPIO_BASE + Addr)));
	} else if (sscanf(RegBuf, "%x", &Addr) == 1) {
		proc_regOfst = Addr;
		LOG_DBG("Read => Addr: 0x%08X, Read Data: 0x%08X.", GPIO_BASE + Addr,
			ioread32((void *)(GPIO_BASE + Addr)));
	}
	/*  */
	LOG_DBG("- X. Count: %d.", (int)Count);
	return Count;
}

/*******************************************************************************
*
********************************************************************************/
static const struct file_operations fcameraisp_proc_fops = {
	.read = ISP_DumpRegToProc,
	.write = ISP_RegDebug,
};

static const struct file_operations fcameraio_proc_fops = {
	.read = CAMIO_DumpRegToProc,
	.write = CAMIO_RegDebug,
};

/*******************************************************************************
*
********************************************************************************/

static MINT32 __init ISP_Init(void)
{
	MINT32 Ret = 0, j;
	void *tmp;
#if 0
	struct proc_dir_entry *pEntry;
#endif
	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&IspDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}
	/*  */
/* FIX-ME: linux-3.10 procfs API changed */
#if 1
	proc_create("driver/isp_reg", 0, NULL, &fcameraisp_proc_fops);
	proc_create("driver/camio_reg", 0, NULL, &fcameraio_proc_fops);
#else
	pEntry = create_proc_entry("driver/isp_reg", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = ISP_DumpRegToProc;
		pEntry->write_proc = ISP_RegDebug;
	} else {
		LOG_ERR("add /proc/driver/isp_reg entry fail");
	}

	pEntry = create_proc_entry("driver/camio_reg", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = CAMIO_DumpRegToProc;
		pEntry->write_proc = CAMIO_RegDebug;
	} else {
		LOG_ERR("add /proc/driver/camio_reg entry fail");
	}
#endif
	/*  */
	/* allocate a memory area with kmalloc. Will be rounded up to a page boundary */
	/* RT_BUF_TBL_NPAGES*4096(1page) = 64k Bytes */

	if (sizeof(ISP_RT_BUF_STRUCT) > ((RT_BUF_TBL_NPAGES) * PAGE_SIZE)) {
		i = 0;
		while (i < sizeof(ISP_RT_BUF_STRUCT)) {
			i += PAGE_SIZE;
		}
		pBuf_kmalloc = kmalloc(i + 2 * PAGE_SIZE, GFP_KERNEL);
		if (pBuf_kmalloc == NULL) {
			LOG_ERR("mem not enough\n");
			return -ENOMEM;
		}
	} else {
		pBuf_kmalloc = kmalloc((RT_BUF_TBL_NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
		if (pBuf_kmalloc == NULL) {
			LOG_ERR("mem not enough\n");
			return -ENOMEM;
		}
	}
	memset(pBuf_kmalloc, 0x00, RT_BUF_TBL_NPAGES * PAGE_SIZE);
	/* round it up to the page bondary */
	pTbl_RTBuf = (int *)((((unsigned long)pBuf_kmalloc) + PAGE_SIZE - 1) & PAGE_MASK);
	pstRTBuf = (ISP_RT_BUF_STRUCT *) pTbl_RTBuf;
	pstRTBuf->state = ISP_RTBC_STATE_INIT;
	/* isr log */
	if (PAGE_SIZE <
	    ((_IRQ_MAX * NORMAL_STR_LEN * ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((_IRQ_MAX * NORMAL_STR_LEN * ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
			LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		LOG_ERR("mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < _IRQ_MAX; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			tmp = (void *)((unsigned int)tmp + (NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			tmp = (void *)((unsigned int)tmp + (NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			tmp = (void *)((unsigned int)tmp + (NORMAL_STR_LEN * ERR_PAGE));
		}
		tmp = (void *)((unsigned int)tmp + NORMAL_STR_LEN);	/* log buffer ,in case of overflow */
	}
	/* mark the pages reserved , FOR MMAP */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));
	}
	/*  */
	/* Register ISP callback */
	LOG_DBG("register isp callback for MDP");
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP,
			   ISP_MDPClockOnCallback,
			   ISP_MDPDumpCallback, ISP_MDPResetCallback, ISP_MDPClockOffCallback);
	/* Register GCE callback for dumping ISP register */
	LOG_DBG("register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(ISP_BeginGCECallback, ISP_EndGCECallback);

	/* Register M4U callback dump */
	LOG_DBG("register M4U callback dump");
	m4u_register_fault_callback(M4U_PORT_IMGI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMGO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_RRZO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_AAO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_LCSO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_ESFKO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMGO_S, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_LSCI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_LSCI_D, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_BPCI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_BPCI_D, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_UFDI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMG2O, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMG3O, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIPI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIP2I, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIP3I, ISP_M4U_TranslationFault_callback, NULL);


#ifdef _MAGIC_NUM_ERR_HANDLING_
	LOG_DBG("init m_LastMNum");
	for (i = 0; i < _rt_dma_max_; i++) {
		m_LastMNum[i] = 0;
	}
#endif


	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static void __exit ISP_Exit(void)
{
	int i;
	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&IspDriver);
	/*  */
	/* Unregister ISP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP, NULL, NULL, NULL, NULL);
	/* Un-Register GCE callback */
	LOG_DBG("Un-register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(NULL, NULL);
	/*  */
	/* Un-Register M4U callback dump */
	LOG_DBG("Un-Register M4U callback dump");
	m4u_unregister_fault_callback(M4U_PORT_IMGI);


	/* unreserve the pages */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));
	}
	/* free the memory areas */
	kfree(pBuf_kmalloc);
	kfree(pLog_kmalloc);

	/*  */
}

/*******************************************************************************
*
********************************************************************************/
MBOOL ISP_RegCallback(ISP_CALLBACK_STRUCT * pCallback)
{
	/*  */
	if (pCallback == NULL) {
		LOG_ERR("pCallback is null");
		return MFALSE;
	}
	/*  */
	if (pCallback->Func == NULL) {
		LOG_ERR("Func is null");
		return MFALSE;
	}
	/*  */
	LOG_DBG("Type(%d)", pCallback->Type);
	IspInfo.Callback[pCallback->Type].Func = pCallback->Func;
	/*  */
	return MTRUE;
}

/*******************************************************************************
*
********************************************************************************/
MBOOL ISP_UnregCallback(ISP_CALLBACK_ENUM Type)
{
	if (Type > ISP_CALLBACK_AMOUNT) {
		LOG_ERR("Type(%d) must smaller than %d", Type, ISP_CALLBACK_AMOUNT);
		return MFALSE;
	}
	/*  */
	LOG_DBG("Type(%d)", Type);
	IspInfo.Callback[Type].Func = NULL;
	/*  */
	return MTRUE;
}

void ISP_MCLK1_EN(BOOL En)
{
	MUINT32 temp = 0;
	temp = ISP_RD32(ISP_ADDR + 0x4200);
	if (En) {
		temp |= 0x20000000;
		ISP_WR32(ISP_ADDR + 0x4200, temp);
	} else {
		temp &= 0xDFFFFFFF;
		ISP_WR32(ISP_ADDR + 0x4200, temp);
	}

}

void ISP_MCLK2_EN(BOOL En)
{
	MUINT32 temp = 0;
	temp = ISP_RD32(ISP_ADDR + 0x4600);
	if (En) {
		temp |= 0x20000000;
		ISP_WR32(ISP_ADDR + 0x4600, temp);
	} else {
		temp &= 0xDFFFFFFF;
		ISP_WR32(ISP_ADDR + 0x4600, temp);
	}

}

void ISP_MCLK3_EN(BOOL En)
{
	MUINT32 temp = 0;
	temp = ISP_RD32(ISP_ADDR + 0x4A00);
	if (En) {
		temp |= 0x20000000;
		ISP_WR32(ISP_ADDR + 0x4A00, temp);
	} else {
		temp &= 0xDFFFFFFF;
		ISP_WR32(ISP_ADDR + 0x4A00, temp);
	}

}

int32_t ISP_MDPClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("ISP_MDPClockOnCallback"); */

	ISP_EnableClock(MTRUE);

	return 0;
}

int32_t ISP_MDPDumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("ISP_MDPDumpCallback");

	ISP_DumpReg();

	return 0;
}

int32_t ISP_MDPResetCallback(uint64_t engineFlag)
{
	LOG_DBG("ISP_MDPResetCallback");

	ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);

	return 0;
}

int32_t ISP_MDPClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("ISP_MDPClockOffCallback"); */

	ISP_EnableClock(MFALSE);

	return 0;
}


static uint32_t *addressToDump[] = {
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4018),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x401C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4024),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4030),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x403C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4040),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4080),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4084),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4088),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x40A0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x40A4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x40A8),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48A0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48A4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48A8),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48AC),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48B0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48B4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48B8),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48BC),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48C0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48C4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48C8),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48CC),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48D0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48D4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48D8),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48DC),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48E0),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48E4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x48E8),
	/*  */
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4948),
	/*  */
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B00),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B04),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B08),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B0C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B10),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B14),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B18),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B1C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B20),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B24),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B28),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B2C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B30),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B34),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x4B38),
	/*  */
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7204),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7208),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x720C),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7230),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7240),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7288),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x72a4),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7300),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7320),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7440),
	(uint32_t *) IO_VIRT_TO_PHYS(IMGSYS_BASE + 0x7460)
};

int32_t ISP_BeginGCECallback(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
	LOG_DBG("+,taskID(%d)", taskID);

	*regCount = sizeof(addressToDump) / sizeof(uint32_t);
	*regAddress = (uint32_t *) addressToDump;

	LOG_DBG("-,*regCount(%d)", *regCount);

	return 0;
}

int32_t ISP_EndGCECallback(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
#define PER_LINE_LOG_SIZE   10
	int32_t i, j, pos;
	uint32_t add[PER_LINE_LOG_SIZE];
	uint32_t val[PER_LINE_LOG_SIZE];

	LOG_DBG("End taskID(%d),regCount(%d)", taskID, regCount);

	for (i = 0; i < regCount; i += PER_LINE_LOG_SIZE) {
		for (j = 0; j < PER_LINE_LOG_SIZE; j++) {
			pos = i + j;
			if (pos < regCount) {
				add[j] = (uint32_t) addressToDump[pos] & 0xffff;
				val[j] = regValues[pos];
			}
		}

		LOG_DBG
		    ("[0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x]\n",
		     add[0], val[0], add[1], val[1], add[2], val[2], add[3], val[3], add[4], val[4],
		     add[5], val[5], add[6], val[6], add[7], val[7], add[8], val[8], add[9],
		     val[9]);
	}

	return 0;
}

m4u_callback_ret_t ISP_M4U_TranslationFault_callback(int port, unsigned int mva, void *data)
{
	LOG_DBG("[ISP_M4U]fault call port=%d, mva=0x%x", port, mva);

	switch (port) {
	case M4U_PORT_IMGO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3300),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3300));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3304),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3304));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3308),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3308));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x330c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x330c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3310),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3310));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3314),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3314));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3318),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3318));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x331c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x331c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3320),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3320));
		break;
	case M4U_PORT_RRZO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3320),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3320));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3324),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3324));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3328),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3328));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x332c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x332c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3330),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3330));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3334),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3334));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3338),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3338));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x333c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x333c));
		break;
	case M4U_PORT_AAO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3364),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3364));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3368),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3368));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3388),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3388));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x338c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x338c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3390),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3390));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3394),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3394));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3398),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3398));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x339c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x339c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a0));
		break;
	case M4U_PORT_LCSO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3340),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3340));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3344),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3344));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3348),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3348));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x334c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x334c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3350),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3350));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3354),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3354));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3358),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3358));
		break;
	case M4U_PORT_ESFKO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x335c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x335c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3360),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3360));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x336c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x336c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3370),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3370));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3374),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3374));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3378),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3378));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x337c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x337c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3380),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3380));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3384),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3384));
		break;
	case M4U_PORT_IMGO_S:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34dc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34dc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34ec),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34ec));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34fc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34fc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3500),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3500));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3504),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3504));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3508),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3508));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x350c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x350c));
		break;
	case M4U_PORT_LSCI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x326c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x326c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3270),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3270));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3274),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3274));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3278),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3278));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x327c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x327c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3280),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3280));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3284),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3284));
		break;
	case M4U_PORT_LSCI_D:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d0));
		break;
	case M4U_PORT_BPCI:
	case M4U_PORT_BPCI_D:
		break;
	case M4U_PORT_IMGI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3234),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3234));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3238),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3238));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x323c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x323c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3240),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3240));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3248),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3248));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x324c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x324c));
		break;
	case M4U_PORT_LCEI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32bc));
		break;
	case M4U_PORT_UFDI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3288),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3288));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x328c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x328c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3290),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3290));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3294),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3294));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3298),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3298));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x329c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x329c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a0));
		break;
	case M4U_PORT_IMG2O:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3440),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3440));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3444),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3444));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3448),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3448));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x344c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x344c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3450),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3450));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3454),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3454));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3458),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3458));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x345c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x345c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3480),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3480));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3484),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3484));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3488),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3488));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x348c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x348c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3490),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3490));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3494),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3494));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3498),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3498));
		break;
	case M4U_PORT_IMG3O:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3460),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3460));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3464),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3464));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3468),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3468));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x346c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x346c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3470),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3470));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3474),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3474));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3478),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3478));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x347c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x347c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3400),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3400));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3404),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3404));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3408),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3408));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x340c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x340c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3410),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3410));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3414),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3414));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3418),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3418));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x341c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x341c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3420),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3420));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3424),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3424));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3428),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3428));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x342c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x342c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3430),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3430));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3434),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3434));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3438),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3438));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x343c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x343c));
		break;
	case M4U_PORT_VIPI:
	case M4U_PORT_VIP2I:
	case M4U_PORT_VIP3I:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32dc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32dc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32ec),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32ec));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32fc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32fc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33c0));
		break;
	default:
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		break;
	}

	return M4U_CALLBACK_HANDLED;
}



/*******************************************************************************
*
********************************************************************************/
module_init(ISP_Init);
module_exit(ISP_Exit);
MODULE_DESCRIPTION("Camera ISP driver");
MODULE_AUTHOR("ME3");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL(ISP_RegCallback);
EXPORT_SYMBOL(ISP_UnregCallback);
EXPORT_SYMBOL(ISP_MCLK1_EN);
EXPORT_SYMBOL(ISP_MCLK2_EN);
EXPORT_SYMBOL(ISP_MCLK3_EN);
