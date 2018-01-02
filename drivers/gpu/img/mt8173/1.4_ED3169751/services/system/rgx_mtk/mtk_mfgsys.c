
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgsys.c
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Implementation interface between RGX DDK and kernel GPU DVFS module
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ April 27th 2013
 *2. Add  Enable/Disable MFG sytem API, Enable/Disable MFG clock API @May 20th 2013
 *3. Add MTKDevPrePowerState/MTKDevPostPowerState/MTKSystemPrePowerState/MTKSystemPostPowerState @ May 29th 2013
 *4. Move some interface to mtk_mfgdvfs.c @ July 10th 2013
 *5. E2 above IC version support HW APM feature @ Sep 17th 2013
 *============================================================================
 ****************************************************************************/

#include <linux/delay.h>
#include "mtk_mfgsys.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_boot.h"
#include "osfunc.h"
#include "pvrsrv_error.h"
#include "pvrsrv.h"

#include <dt-bindings/clock/mt8173-clk.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>


#if GPU_DVFS_TIMER
static IMG_HANDLE g_hDVFSTimer;
#endif

static IMG_UINT32 g_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
static IMG_UINT32 gpu_loading;
static IMG_UINT32 gpu_block;
static IMG_UINT32 gpu_idle;
static IMG_UINT32 gpu_pre_loading;

static struct clk *g_mfgclk_power;
static struct clk *g_mfgclk_axi;
static struct clk *g_mfgclk_mem;
static struct clk *g_mfgclk_g3d;
static struct clk *g_mfgclk_26m;


static IMG_INT32 MtkGetRGXDevIdx(IMG_VOID)
{
	IMG_UINT32 ui32DeviceIndex = MTK_RGX_DEVICE_INDEX_INVALID;
	IMG_UINT32 i;

	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
		PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
		if (psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
			ui32DeviceIndex = i;
	}

	return ui32DeviceIndex;
}


static IMG_UINT32  MTKCalGpuLoading(IMG_VOID)
{
	if (g_ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID)
		g_ui32RGXDevIdx = MtkGetRGXDevIdx();

	if (g_ui32RGXDevIdx != MTK_RGX_DEVICE_INDEX_INVALID) {
		PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
		PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[g_ui32RGXDevIdx];

		if (psDeviceNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
			if (psDevInfo->pfnGetGpuUtilStats) {
				RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

				sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDeviceNode);
				if (sGpuUtilStats.bValid) {
#if 0
					PVR_DPF((PVR_DBG_ERROR, "Loading: A(%d), I(%d), B(%d)",
						sGpuUtilStats.ui32GpuStatActiveHigh,
						sGpuUtilStats.ui32GpuStatIdle,
						sGpuUtilStats.ui32GpuStatBlocked));
#endif
					gpu_block = sGpuUtilStats.ui32GpuStatBlocked/100;
					gpu_idle  = sGpuUtilStats.ui32GpuStatIdle/100;
					return sGpuUtilStats.ui32GpuStatActiveHigh/100;
				}
			}
		}
	}

	return 0;
}


static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int *pui32NewFreqID)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
	int i32NewFreqID = i32CurFreqID;

	if (ui32GPULoading >= 99)
		i32NewFreqID = 0;
	else if (ui32GPULoading <= 1)
		i32NewFreqID = i32MaxLevel;
	else if (ui32GPULoading >= 85)
		i32NewFreqID -= 2;
	else if (ui32GPULoading <= 30)
		i32NewFreqID += 2;
	else if (ui32GPULoading >= 70)
		i32NewFreqID -= 1;
	else if (ui32GPULoading <= 50)
		i32NewFreqID += 1;

	if (i32NewFreqID < i32CurFreqID) {
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
			i32NewFreqID -= 1;
	} else if (i32NewFreqID > i32CurFreqID) {
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
			i32NewFreqID += 1;
	}

	if (i32NewFreqID > i32MaxLevel)
		i32NewFreqID = i32MaxLevel;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	if (i32NewFreqID != i32CurFreqID) {
		*pui32NewFreqID = (unsigned int)i32NewFreqID;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}


static IMG_VOID MtkWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
	RGX_DATA *psRGXData = (RGX_DATA *)psDeviceNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit*/
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq*1000;
}


static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID)
{
	if (g_ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID)
		return IMG_FALSE;

	if (PVRSRV_OK == PVRSRVDevicePreClockSpeedChange(g_ui32RGXDevIdx, FALSE, (IMG_VOID *)NULL)) {
		unsigned int ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32NewFreqID);

		MtkWriteBackFreqToRGX(g_ui32RGXDevIdx, ui32GPUFreq);

		mt_gpufreq_target(ui32NewFreqID);

		PVRSRVDevicePostClockSpeedChange(g_ui32RGXDevIdx, FALSE, (IMG_VOID *)NULL);
	}

	return IMG_TRUE;
}


static IMG_VOID MTKDVFSTimerFuncCB(IMG_VOID *param)
{
	IMG_UINT32 ui32NewFreqID;

	if (mt_gpufreq_dvfs_ready() == false)
		return;

	gpu_loading = MTKCalGpuLoading();

	/* do gpu dvfs */
	if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
		MTKDoGpuDVFS(ui32NewFreqID);

	gpu_pre_loading = gpu_loading;
}


static IMG_UINT32  MTKGetGpuLoading(IMG_VOID)
{
	return gpu_loading;
}

static IMG_UINT32 MTKGetGpuBlock(IMG_VOID)
{
	return gpu_block;
}

static IMG_UINT32 MTKGetGpuIdle(IMG_VOID)
{
	return gpu_idle;
}

/* kernel/drivers/gpu/img/mt6595/mtk_gpu_utility.c
   kernel/drivers/misc/mediatek/gpu/mtk_gpu_utility.c
   extern unsigned int (*mtk_get_gpu_loading_fp)(void);*/
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
int MTKMFGSystemInit(void)
{
	/*mtk_get_gpu_loading_fp = MTKGetGpuLoading;*/

#if GPU_DVFS_TIMER
	g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL);
	if (!g_hDVFSTimer)
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));

	OSEnableTimer(g_hDVFSTimer);
#endif
	mtk_get_gpu_loading_fp	 = MTKGetGpuLoading;
	mtk_get_gpu_block_fp   = MTKGetGpuBlock;
	mtk_get_gpu_idle_fp    = MTKGetGpuIdle;

	return PVRSRV_OK;
}


int MTKMFGSystemDeInit(void)
{
#if GPU_DVFS_TIMER
	if (g_hDVFSTimer) {
		OSDisableTimer(g_hDVFSTimer);
		OSRemoveTimer(g_hDVFSTimer);

		g_hDVFSTimer = IMG_NULL;
	}
#endif

	return PVRSRV_OK;
}

static int MtkEnableMfgClock(void)
{
	clk_prepare(g_mfgclk_power);
	clk_enable(g_mfgclk_power);

	clk_prepare(g_mfgclk_axi);
	clk_enable(g_mfgclk_axi);

	clk_prepare(g_mfgclk_mem);
	clk_enable(g_mfgclk_mem);

	clk_prepare(g_mfgclk_g3d);
	clk_enable(g_mfgclk_g3d);

	clk_prepare(g_mfgclk_26m);
	clk_enable(g_mfgclk_26m);

	return PVRSRV_OK;
}


static int MtkDisableMfgClock(void)
{
	clk_disable(g_mfgclk_26m);
	clk_unprepare(g_mfgclk_26m);

	clk_disable(g_mfgclk_g3d);
	clk_unprepare(g_mfgclk_g3d);

	clk_disable(g_mfgclk_mem);

	clk_unprepare(g_mfgclk_mem);

	clk_disable(g_mfgclk_axi);
	clk_unprepare(g_mfgclk_axi);

	clk_disable(g_mfgclk_power);
	clk_unprepare(g_mfgclk_power);

	return PVRSRV_OK;
}


int MTKMFGGetClocks(struct platform_device *pdev)
{
	int ret;

	if (&pdev->dev == NULL)
		PVR_TRACE(("&pdev->dev is NULL"));

	g_mfgclk_power = devm_clk_get(&pdev->dev, "MT_CG_MFG_POWER");
	g_mfgclk_axi = devm_clk_get(&pdev->dev, "MT_CG_MFG_AXI");
	g_mfgclk_mem = devm_clk_get(&pdev->dev, "MT_CG_MFG_MEM");
	g_mfgclk_g3d = devm_clk_get(&pdev->dev, "MT_CG_MFG_G3D");
	g_mfgclk_26m = devm_clk_get(&pdev->dev, "MT_CG_MFG_26M");

/*
	PVR_TRACE(("MTKMFGGetClocks: g_mfgclk_axi (0x%x) g_mfgclk_g3d (0x%x) g_mfgclk_mem (0x%x) g_mfgclk_26m (0x%x)",
		g_mfgclk_axi,g_mfgclk_g3d,g_mfgclk_mem,g_mfgclk_26m));
*/

	if (IS_ERR(g_mfgclk_power)) {
		ret = PTR_ERR(g_mfgclk_power);
		dev_err(&pdev->dev, "Failed to request g_mfgclk_power: %d\n", ret);
		return ret;
	}

	if (IS_ERR(g_mfgclk_axi)) {
		ret = PTR_ERR(g_mfgclk_axi);
		dev_err(&pdev->dev, "Failed to request g_mfgclk_axi: %d\n", ret);
		return ret;
	}

	if (IS_ERR(g_mfgclk_mem)) {
		ret = PTR_ERR(g_mfgclk_mem);
		dev_err(&pdev->dev, "Failed to request g_mfgclk_mem: %d\n", ret);
		return ret;
	}

	if (IS_ERR(g_mfgclk_g3d)) {
		ret = PTR_ERR(g_mfgclk_g3d);
		dev_err(&pdev->dev, "Failed to request g_mfgclk_g3d: %d\n", ret);
		return ret;
	}

	if (IS_ERR(g_mfgclk_26m)) {
		ret = PTR_ERR(g_mfgclk_26m);
		dev_err(&pdev->dev, "Failed to request g_mfgclk_26m: %d\n", ret);
		return ret;
	}

	//MtkEnableMfgClock();
	return 0;
}


#if 0
static int MTKMFGClockPowerOn(void)
{
#if 0
#if MTK_PM_SUPPORT
#else
	DRV_WriteReg32(MFG_PDN_RESET, 0x1);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
	DRV_WriteReg32(MFG_PDN_SET, 0xf);
	DRV_WriteReg32(MFG_PDN_RESET, 0x0);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
#endif

#if 0/*RD_POWER_ISLAND*/
	/*MtkEnableMfgClock();*/
#endif

	mtk_mfg_debug("MFG_PDN_STATUS: 0x%X\n", DRV_Reg32(MFG_PDN_STATUS));

	/*DRAM access path: need configuration for each power-up
	  0x1 means G3D select Dram access through CCI-400*/
	DRV_WriteReg32(MFG_DRAM_ACCESS_PATH, 0x00000000);
#endif
	return PVRSRV_OK;

}
#endif


extern void __iomem *gbRegBase;
static int MTKEnableHWAPM(void)
{
	DRV_WriteReg32(((char *)gbRegBase + 0x24), 0x8007050f);
	DRV_WriteReg32(((char *)gbRegBase + 0x28), 0x0e0c0a09);
	DRV_WriteReg32(((char *)gbRegBase + 0xa0), 0x08000000);

	return PVRSRV_OK;
}


static int MTKDisableHWAPM(void)
{
	DRV_WriteReg32(((char *)gbRegBase + 0x24), 0x0);
	DRV_WriteReg32(((char *)gbRegBase + 0x28), 0x0);
	DRV_WriteReg32(((char *)gbRegBase + 0xa0), 0x0);

	return PVRSRV_OK;
}


#if 0
IMG_UINT32 MTKMFGGetPowerIndex(IMG_VOID)
{
	DRV_WriteReg32(0xf3fff024, 0x0);
	DRV_WriteReg32(0xf3fff028, 0x0);
}
#endif


PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if (PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState &&
	    PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState) {
#if GPU_DVFS_TIMER
		if (g_hDVFSTimer)
			OSDisableTimer(g_hDVFSTimer);
#endif

		/*gpu_last_loading = MTKCalGpuLoading();*/

		MTKDisableHWAPM();
		MtkDisableMfgClock();

		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if (PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState &&
	    PVRSRV_DEV_POWER_STATE_ON == eNewPowerState) {

		MtkEnableMfgClock();

		MTKEnableHWAPM();

		/*MTKMFGProcessDVFS(gpu_last_loading);*/

#if GPU_DVFS_TIMER
		if (g_hDVFSTimer)
			OSEnableTimer(g_hDVFSTimer);
#endif
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState) {
#if 0
		if (MTKMFGIsE2andAboveVersion())
			MTKDisableHWAPM();
		MtkDisableMfgClock();
#endif
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (PVRSRV_SYS_POWER_STATE_ON == eNewPowerState) {
#if 0
		MtkEnableMfgClock();
		if (MTKMFGIsE2andAboveVersion())
			MTKEnableHWAPM();

		/*MTKMFGProcessDVFS();*/
#endif
	}

	return PVRSRV_OK;
}


module_param(gpu_loading, uint, 0644);
