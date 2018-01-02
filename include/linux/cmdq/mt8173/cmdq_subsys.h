
/* msb	 id   group        reg-base-name */
DECLARE_CMDQ_SUBSYS(0x1300, 0, MFG, HAN_BASE)
	DECLARE_CMDQ_SUBSYS(0x1400, 1, MMSYS, MMSYS_CONFIG_BASE)
	DECLARE_CMDQ_SUBSYS(0x1401, 2, DISP, DISP_RDMA2_BASE)
	DECLARE_CMDQ_SUBSYS(0x1402, 3, DISP, MM_MUTEX_BASE)
	DECLARE_CMDQ_SUBSYS(0x1500, 4, CAM, IMGSYS_BASE)
	DECLARE_CMDQ_SUBSYS(0x1600, 5, VDEC, VDEC_GCON_BASE)
	DECLARE_CMDQ_SUBSYS(0x1700, 6, MJC, MJC_CONFIG_BASE)
	DECLARE_CMDQ_SUBSYS(0x1800, 7, VENC, VENC_GCON_BASE)
	DECLARE_CMDQ_SUBSYS(0x1000, 8, INFRA_AO, CKSYS_BASE)
	DECLARE_CMDQ_SUBSYS(0x1001, 9, INFRA_AO, KP_BASE)
	DECLARE_CMDQ_SUBSYS(0x1002, 10, MD32, MD32_BASE)
	DECLARE_CMDQ_SUBSYS(0x1003, 11, MD32, MD32_BASE)
	DECLARE_CMDQ_SUBSYS(0x1004, 12, MD32, MD32_BASE)
	DECLARE_CMDQ_SUBSYS(0x1005, 13, MD32, MD32_BASE)
	DECLARE_CMDQ_SUBSYS(0x1020, 14, INFRASYS, MCUCFG_BASE)
	DECLARE_CMDQ_SUBSYS(0x1021, 15, INFRASYS, GCPU_BASE)
	DECLARE_CMDQ_SUBSYS(0x1120, 16, PERISYS, USB0_BASE)
	DECLARE_CMDQ_SUBSYS(0x1121, 17, PERISYS, USB_SIF_BASE)
	DECLARE_CMDQ_SUBSYS(0x1122, 18, PERISYS, AUDIO_BASE)
	DECLARE_CMDQ_SUBSYS(0x1123, 19, PERISYS, MSDC0_BASE)
	DECLARE_CMDQ_SUBSYS(0x1124, 20, PERISYS, MSDC1_BASE)
	DECLARE_CMDQ_SUBSYS(0x1125, 21, PERISYS, MSDC2_BASE)
	DECLARE_CMDQ_SUBSYS(0x1126, 22, PERISYS, MSDC3_BASE)
