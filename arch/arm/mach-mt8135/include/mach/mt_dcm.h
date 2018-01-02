#ifndef _MT_DCM_H
#define _MT_DCM_H

#include "mach/mt_reg_base.h"


/* AXI bus dcm */
#define DCM_CFG                 (TOPRGU_BASE + 0x0104)

/* CA7 DCM */
#define TOP_DCMCTL              (INFRACFG_BASE + 0x0010)
#define TOP_DCMDBC              (INFRACFG_BASE + 0x0014)
#define TOP_CA7DCMFSEL          (INFRACFG_BASE + 0x0018)

/* infra dcm */
#define INFRA_DCMCTL            (INFRACFG_BASE + 0x0050)
#define INFRA_DCMDBC            (INFRACFG_BASE + 0x0054)
#define INFRA_DCMFSEL           (INFRACFG_BASE + 0x0058)

#define DRAMC_PD_CTRL           (DRAMC0_BASE + 0x01DC)

/* peri dcm */
#define PERI_GCON_DCMCTL        (PERICFG_BASE + 0x0050)
#define PERI_GCON_DCMDBC        (PERICFG_BASE + 0x0054)
#define PERI_GCON_DCMFSEL       (PERICFG_BASE + 0x0058)

/* m4u dcm */
#define M4U_DCM                 (SMI_MMU_TOP_BASE + 0x001C)

/* smi_common dcm */
#define SMI_COMMON_DCM          (SMI_COMMON_EXT_BASE + 0x0300)

/* smi_secure dcm */
#define SMI_SECURE_DCMCON       (SMI1_BASE + 0x0010)
#define SMI_SECURE_DCMSET       (SMI1_BASE + 0x0014)
#define SMI_SECURE_DCMCLR       (SMI1_BASE + 0x0018)

#define SMILARB0_BASE           SMI_LARB0
#define SMILARB1_BASE           SMI_LARB1
#define SMILARB3_BASE           SMI_LARB3
#define SMILARB4_BASE           SMI_LARB4


#define SMILARB0_DCM_STA        (SMILARB0_BASE + 0x00)
#define SMILARB0_DCM_CON        (SMILARB0_BASE + 0x10)
#define SMILARB0_DCM_SET        (SMILARB0_BASE + 0x14)
#define SMILARB0_DCM_CLR        (SMILARB0_BASE + 0x18)

#define SMILARB1_DCM_STA        (SMILARB1_BASE + 0x00)
#define SMILARB1_DCM_CON        (SMILARB1_BASE + 0x10)
#define SMILARB1_DCM_SET        (SMILARB1_BASE + 0x14)
#define SMILARB1_DCM_CLR        (SMILARB1_BASE + 0x18)

#define SMILARB2_DCM_STA        (SMILARB2_BASE + 0x00)
#define SMILARB2_DCM_CON        (SMILARB2_BASE + 0x10)
#define SMILARB2_DCM_SET        (SMILARB2_BASE + 0x14)
#define SMILARB2_DCM_CLR        (SMILARB2_BASE + 0x18)

#define SMILARB3_DCM_STA        (SMILARB3_BASE + 0x00)
#define SMILARB3_DCM_CON        (SMILARB3_BASE + 0x10)
#define SMILARB3_DCM_SET        (SMILARB3_BASE + 0x14)
#define SMILARB3_DCM_CLR        (SMILARB3_BASE + 0x18)

#define SMILARB4_DCM_STA        (SMILARB4_BASE + 0x00)
#define SMILARB4_DCM_CON        (SMILARB4_BASE + 0x10)
#define SMILARB4_DCM_SET        (SMILARB4_BASE + 0x14)
#define SMILARB4_DCM_CLR        (SMILARB4_BASE + 0x18)

/* MFG */
#define MFG_DCM_CON0            (MFG_CONFIG_BASE + 0x0010)
#define MFG_DCM_CON1            (MFG_CONFIG_BASE + 0x0014)

/* smi_isp_dcm */
#define CAM_BASE                (IMGSYS_CONFG_BASE + 0x4000)
#define CAM_CTL_RAW_DCM         (CAM_BASE + 0x190)
#define CAM_CTL_RGB_DCM         (CAM_BASE + 0x194)
#define CAM_CTL_YUV_DCM         (CAM_BASE + 0x198)
#define CAM_CTL_CDP_DCM         (CAM_BASE + 0x19C)

#define CAM_CTL_RAW_DCM_STA     (CAM_BASE + 0x1A0)
#define CAM_CTL_RGB_DCM_STA     (CAM_BASE + 0x1A4)
#define CAM_CTL_YUV_DCM_STA     (CAM_BASE + 0x1A8)
#define CAM_CTL_CDP_DCM_STA     (CAM_BASE + 0x1AC)

#define JPGDEC_DCM_CTRL         (IMGSYS_CONFG_BASE + 0x9300)
#define JPGENC_DCM_CTRL         (IMGSYS_CONFG_BASE + 0xA300)

#define SMI_ISP_COMMON_DCMCON   (SMI_ISPSYS + 0x0010)
#define SMI_ISP_COMMON_DCMSET   (SMI_ISPSYS + 0x0014)
#define SMI_ISP_COMMON_DCMCLR   (SMI_ISPSYS + 0x0018)

/* display sys */
#define DISP_HW_DCM_DIS0        (DISPSYS_BASE + 0x0120)
#define DISP_HW_DCM_DIS_SET0    (DISPSYS_BASE + 0x0124)
#define DISP_HW_DCM_DIS_CLR0    (DISPSYS_BASE + 0x0128)

#define DISP_HW_DCM_DIS1        (DISPSYS_BASE + 0x0130)
#define DISP_HW_DCM_DIS_SET1    (DISPSYS_BASE + 0x0134)
#define DISP_HW_DCM_DIS_CLR1    (DISPSYS_BASE + 0x0138)

/* venc sys */
#define VENC_CE                 (VENC_BASE + 0x00EC)
#define VENC_CLK_DCM_CTRL       (VENC_BASE + 0x00F4)
#define VENC_CLK_CG_CTRL        (VENC_BASE + 0x0094)
#define VENC_MP4_DCM_CTRL       (VENC_BASE + 0x06F0)

/* vdec */
#define VDEC_DCM_CON            (VDEC_GCON_BASE + 0x0018)


#define CPU_DCM                 (0x1 << 0)
#define IFR_DCM                 (0x1 << 1)
#define PER_DCM                 (0x1 << 2)
#define SMI_DCM                 (0x1 << 3)
#define DIS_DCM                 (0x1 << 4)
#define ISP_DCM                 (0x1 << 5)
#define VDE_DCM                 (0x1 << 6)
#define VEN_DCM                 (0x1 << 7)
#define ALL_DCM                 (0xFF)
#define NR_DCMS                 (8)


/* extern void dcm_get_status(unsigned int type); */
extern void dcm_enable(unsigned int type);
extern void dcm_disable(unsigned int type);

extern void bus_dcm_enable(void);
extern void bus_dcm_disable(void);

extern void disable_infra_dcm(void);
extern void restore_infra_dcm(void);

extern void disable_peri_dcm(void);
extern void restore_peri_dcm(void);

extern void mt_dcm_init(void);

#endif
