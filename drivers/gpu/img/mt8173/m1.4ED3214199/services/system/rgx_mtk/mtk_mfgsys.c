
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
#include <linux/mtk_gpu_utility.h>


static IMG_HANDLE g_hDVFSTimer;
static POS_LOCK   ghDVFSLock;

static IMG_BOOL   g_bExit = IMG_TRUE;
static IMG_INT32  g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 gpu_thermal_freq;

static IMG_UINT32 gpu_power;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 gpu_powerlimit_enable;
static IMG_UINT32 gpu_touchboost_enable;
static IMG_UINT32 gpu_smartboost_enable;
static IMG_UINT32 gpu_debug_enable;
static IMG_UINT32 gpu_dvfs_force_idle;

static IMG_UINT32 gpu_pre_loading;
static IMG_UINT32 gpu_loading;
static IMG_UINT32 gpu_block;
static IMG_UINT32 gpu_idle;

#define MTK_GPIO_REG_OFFSET             0x30
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000

static struct clk *g_mfgclk_power;
static struct clk *g_mfgclk_axi;
static struct clk *g_mfgclk_mem;
static struct clk *g_mfgclk_g3d;
static struct clk *g_mfgclk_26m;

#if 1 //defined(MTK_USE_HW_APM)
static IMG_PVOID g_pvRegsKM;
#endif
static IMG_PVOID g_pvRegsBaseKM;


static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(IMG_VOID)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;
	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
		if (psDeviceNode && psDeviceNode->psDevConfig &&
			psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
		{
			return psDeviceNode;
		}
	}
	return NULL;
}

static IMG_INT32 MTKGetRGXDevIdx(IMG_VOID)
{
	IMG_UINT32 ui32DeviceIndex = MTK_RGX_DEVICE_INDEX_INVALID;
	IMG_UINT32 i;

	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
		if (psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
		{
			ui32DeviceIndex = i;
		}
	}

	return ui32DeviceIndex;
}

static IMG_VOID MtkWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
	RGX_DATA *psRGXData = (RGX_DATA *)psDeviceNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit*/
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq*1000;
}


static IMG_VOID MTKEnableMfgClock(void)
{
	mt_gpufreq_voltage_enable_set(1);

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
}


static IMG_VOID MTKDisableMfgClock(void)
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

	mt_gpufreq_voltage_enable_set(0);
}


int MTKMFGGetClocks(struct platform_device *pdev)
{
	int ret = 0;

	g_mfgclk_power = devm_clk_get(&pdev->dev, "MT_CG_MFG_POWER");
	g_mfgclk_axi = devm_clk_get(&pdev->dev, "MT_CG_MFG_AXI");
	g_mfgclk_mem = devm_clk_get(&pdev->dev, "MT_CG_MFG_MEM");
	g_mfgclk_g3d = devm_clk_get(&pdev->dev, "MT_CG_MFG_G3D");
	g_mfgclk_26m = devm_clk_get(&pdev->dev, "MT_CG_MFG_26M");

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
	return 0;
}


#if  defined(MTK_USE_HW_APM)
static int MTKInitHWAPM(void)
{
#if 0 //disable HW APM
	if (!g_pvRegsKM)
	{
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
		if (psDevNode)
		{
			IMG_CPU_PHYADDR sRegsPBase;
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
			PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
			if (psDevInfo)
			{
				PVR_DPF((PVR_DBG_ERROR, "psDevInfo->pvRegsBaseKM: %p", psDevInfo->pvRegsBaseKM));
			}
			if (psDevConfig)
			{
				sRegsPBase = psDevConfig->sRegsCpuPBase;
				sRegsPBase.uiAddr += 0xfff000;
				PVR_DPF((PVR_DBG_ERROR, "sRegsCpuPBase.uiAddr: 0x%08lx", (unsigned long)psDevConfig->sRegsCpuPBase.uiAddr));
				PVR_DPF((PVR_DBG_ERROR, "sRegsPBase.uiAddr: 0x%08lx", (unsigned long)sRegsPBase.uiAddr));
				g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
			}
		}
	}
#endif
	if (g_pvRegsKM)
	{
		DRV_WriteReg32(g_pvRegsKM + 0x24, 0x003c3d4d);
		DRV_WriteReg32(g_pvRegsKM + 0x28, 0x4d45440b);
		DRV_WriteReg32(g_pvRegsKM + 0xe0, 0x7a710184);
		DRV_WriteReg32(g_pvRegsKM + 0xe4, 0x835f6856);
		DRV_WriteReg32(g_pvRegsKM + 0xe8, 0x002b0234);
		DRV_WriteReg32(g_pvRegsKM + 0xec, 0x80000000);
		DRV_WriteReg32(g_pvRegsKM + 0xa0, 0x08000000);
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s g_pvRegsKM is NULL", __FUNCTION__));
	}
	return PVRSRV_OK;
}


static int MTKDeInitHWAPM(void)
{
	return PVRSRV_OK;
}
#endif


static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32RGXDevIdx;

	if (mt_gpufreq_dvfs_ready() == false)
	{
		return IMG_FALSE;
	}

	// smart boost bottom bound
	if (gpu_smartboost_enable)
	{
		if (ui32NewFreqID > g_bottom_freq_id)
		{
			ui32NewFreqID = g_bottom_freq_id;
		}
	}

	// thermal power limit
	if (gpu_powerlimit_enable)
	{
		if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
			ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();
	}

	// no change
	if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
	{
		return IMG_FALSE;
	}

	ui32RGXDevIdx = MTKGetRGXDevIdx();
	if (ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID)
	{
		return IMG_FALSE;
	}

	eResult = PVRSRVDevicePreClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID *)NULL);
	if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY))
	{
		unsigned int ui32GPUFreq;
		PVRSRV_DEV_POWER_STATE ePowerState;

		PVRSRVGetDevicePowerState(ui32RGXDevIdx, &ePowerState);
		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
		{
			MTKEnableMfgClock();
		}

		mt_gpufreq_target(ui32NewFreqID);

		ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32NewFreqID);
		MtkWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);

		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
		{
			MTKDisableMfgClock();
		}

		if (eResult == PVRSRV_OK)
		{
			PVRSRVDevicePostClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID *)NULL);
		}
	}

	return IMG_TRUE;
}


static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
	if (g_iSkipCount > 0)
	{
		return;
	}

	if (gpu_touchboost_enable == 0)
	{
		return;
	}

	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKFreqInputBoostCB: freq = %d", ui32BoostFreqID));
	}

	ui32BoostFreqID = mt_gpufreq_get_dvfs_table_num() - 2;

	OSLockAcquire(ghDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index())
	{
		if (MTKDoGpuDVFS(ui32BoostFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
		{
			g_sys_dvfs_time_ms = OSClockms();
		}
	}

	OSLockRelease(ghDVFSLock);
}


static void MTKFreqPowerLimitCB(unsigned int ui32LimitFreqID)
{
	if (g_iSkipCount > 0)
	{
		return;
	}

	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKFreqPowerLimitCB: freq = %d", ui32LimitFreqID));
	}

	OSLockAcquire(ghDVFSLock);

	gpu_thermal_freq = mt_gpufreq_get_frequency_by_level(ui32LimitFreqID);

	if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index())
	{
		if (MTKDoGpuDVFS(ui32LimitFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
		{
			g_sys_dvfs_time_ms = OSClockms();
		}
	}

	OSLockRelease(ghDVFSLock);
}


#if 0
static IMG_VOID MTKStartPowerIndex(IMG_VOID)
{
	if (!g_pvRegsBaseKM)
	{
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
		if (psDevNode)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
			if (psDevInfo)
			{
				g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
			}
		}
	}
	if (g_pvRegsBaseKM)
	{
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}
}
#endif


static IMG_VOID MTKReStartPowerIndex(IMG_VOID)
{
	if (g_pvRegsBaseKM)
	{
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
	}
}

#if 0
static IMG_VOID MTKStopPowerIndex(IMG_VOID)
{
	if (g_pvRegsBaseKM)
	{
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
	}
}
#endif

static IMG_UINT32 MTKCalPowerIndex(IMG_VOID)
{
	IMG_UINT32 ui32State, ui32Result;
	PVRSRV_DEV_POWER_STATE  ePowerState;
	IMG_BOOL bTimeout;
	IMG_UINT32 u32Deadline;
	IMG_PVOID pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
	IMG_PVOID pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)6328;

	if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
	{
		return 0;
	}

	if (PVRSRVPowerLock() != PVRSRV_OK)
	{
		return 0;
	}

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
	if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
	{
		PVRSRVPowerUnlock();
		return 0;
	}

	//writes 1 to GPIO_INPUT_REQ, bit[0]
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

	//wait for 1 in GPIO_OUTPUT_REQ, bit[16]
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
	while (OSClockus() < u32Deadline)
	{
		if (0x10000 & DRV_Reg32(pvGPIO_REG))
		{
			bTimeout = IMG_FALSE;
			break;
		}
	}

	//writes 0 to GPIO_INPUT_REQ, bit[0]
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
	if (bTimeout)
	{
		PVRSRVPowerUnlock();
		return 0;
	}

	//read GPIO_OUTPUT_DATA, bit[24]
	ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

	//read POWER_ESTIMATE_RESULT
	ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

	//writes 1 to GPIO_OUTPUT_ACK, bit[17]
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

	//wait for 0 in GPIO_OUTPUT_REG, bit[16]
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
	while (OSClockus() < u32Deadline)
	{
		if (!(0x10000 & DRV_Reg32(pvGPIO_REG)))
		{
			bTimeout = IMG_FALSE;
			break;
		}
	}

	//writes 0 to GPIO_OUTPUT_ACK, bit[17]
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
	if (bTimeout)
	{
		PVRSRVPowerUnlock();
		return 0;
	}

	MTKReStartPowerIndex();

	PVRSRVPowerUnlock();

	return (1 == ui32State) ? ui32Result : 0;
}


static IMG_VOID MTKCalGpuLoading(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	psDevNode = MTKGetRGXDevNode();
	if (!psDevNode)
	{
		return;
	}

	psDevInfo = (PVRSRV_RGXDEV_INFO *)psDevNode->pvDevice;
	if (psDevInfo && psDevInfo->pfnGetGpuUtilStats)
	{
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};
		sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode);
		if (sGpuUtilStats.bValid)
		{
#if 0
			PVR_DPF((PVR_DBG_ERROR, "Loading: A(%d), I(%d), B(%d)",
				sGpuUtilStats.ui32GpuStatActive,
				sGpuUtilStats.ui32GpuStatIdle,
				sGpuUtilStats.ui32GpuStatBlocked));
#endif

			*pui32Loading = sGpuUtilStats.ui32GpuStatActiveHigh/100;
			*pui32Block   = sGpuUtilStats.ui32GpuStatBlocked/100;
			*pui32Idle    = sGpuUtilStats.ui32GpuStatIdle/100;
		}
	}
}


static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int *pui32NewFreqID)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
	int i32NewFreqID = i32CurFreqID;

	if (ui32GPULoading >= 99)
	{
		i32NewFreqID = 0;
	}
	else if (ui32GPULoading <= 1)
	{
		i32NewFreqID = i32MaxLevel;
	}
	else if (ui32GPULoading >= 85)
	{
		i32NewFreqID -= 2;
	}
	else if (ui32GPULoading <= 30)
	{
		i32NewFreqID += 2;
	}
	else if (ui32GPULoading >= 70)
	{
		i32NewFreqID -= 1;
	}
	else if (ui32GPULoading <= 50)
	{
		i32NewFreqID += 1;
	}

	if (i32NewFreqID < i32CurFreqID)
	{
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
		{
			i32NewFreqID -= 1;
		}
	}
	else if (i32NewFreqID > i32CurFreqID)
	{
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
		{
			i32NewFreqID += 1;
		}
	}

	if (i32NewFreqID > i32MaxLevel)
	{
		i32NewFreqID = i32MaxLevel;
	}
	else if (i32NewFreqID < 0)
	{
		i32NewFreqID = 0;
	}

	if (i32NewFreqID != i32CurFreqID)
	{
		*pui32NewFreqID = (unsigned int)i32NewFreqID;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}


static IMG_VOID MTKDVFSTimerFuncCB(IMG_VOID *param)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();

	if (0 == gpu_dvfs_enable)
	{
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		return;
	}

	if (g_iSkipCount > 0)
	{
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;
		return;
	}

	if ((!g_bExit) || (i32CurFreqID < i32MaxLevel))
	{

		IMG_UINT32 ui32NewFreqID;

		gpu_power = MTKCalPowerIndex();
		MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);

		OSLockAcquire(ghDVFSLock);

		// check system boost duration
		if ((g_sys_dvfs_time_ms > 0) && (OSClockms() - g_sys_dvfs_time_ms < MTK_SYS_BOOST_DURATION_MS))
		{
			OSLockRelease(ghDVFSLock);
			return;
		}
		g_sys_dvfs_time_ms = 0;

		// do gpu dvfs
		if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
		{
			MTKDoGpuDVFS(ui32NewFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
		}

		gpu_pre_loading = gpu_loading;

		OSLockRelease(ghDVFSLock);
	}
}


static void MTKMFGEnableDVFSTimer(bool bEnable)
{
	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: %s", bEnable ? "yes" : "no"));
	}
	if (bEnable)
	{
		OSEnableTimer(g_hDVFSTimer);
	}
	else
	{
		OSDisableTimer(g_hDVFSTimer);
	}
}


PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if (PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState &&
	    PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)
	{
		if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
		{
			g_bExit = IMG_TRUE;
		}

#if  defined(MTK_USE_HW_APM)
		MTKDeInitHWAPM();
#endif
		MTKDisableMfgClock();

		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		gpu_power = 0;
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if (PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState &&
	    PVRSRV_DEV_POWER_STATE_ON == eNewPowerState)
	{
		MTKEnableMfgClock();

#if defined(MTK_USE_HW_APM)
		MTKInitHWAPM();
#endif

		if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
		{
			g_bExit = IMG_FALSE;
		}
	}
	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
	{
#if 0
		MTKDeInitHWAPM();
		MTKDisableMfgClock();
#endif
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{
#if 0
		MTKEnableMfgClock();
		MTKInitHWAPM();
#endif
	}

	return PVRSRV_OK;
}


static void MTKBoostGpuFreq(void)
{
	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKBoostGpuFreq"));
	}
	MTKFreqInputBoostCB(0);
}


static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	if (g_iSkipCount > 0)
	{
		return;
	}

	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKSetBottomGPUFreq: freq = %d", ui32FreqLevel));
	}

	if (ui32FreqLevel > ui32MaxLevel)
	{
		ui32FreqLevel = ui32MaxLevel;
	}

	OSLockAcquire(ghDVFSLock);

	// 0 => The highest frequency
	// table_num - 1 => The lowest frequency
	g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	if (g_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
	{
		MTKDoGpuDVFS(g_bottom_freq_id, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
	}

	OSLockRelease(ghDVFSLock);

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

static IMG_UINT32 MTKGetPowerIndex(IMG_VOID)
{
	return gpu_power;
}


int MTKMFGSystemInit(void)
{
	PVRSRV_ERROR error;
	PVRSRV_DEVICE_NODE *psDevNode;

	error = OSLockCreate(&ghDVFSLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
		return error;
	}

	psDevNode = MTKGetRGXDevNode();
	if (psDevNode)
	{
		IMG_CPU_PHYADDR sRegsPBase;
		PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
		PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;

		if (psDevInfo)
		{
			g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
		}

		if (psDevConfig)
		{
			sRegsPBase = psDevConfig->sRegsCpuPBase;
			sRegsPBase.uiAddr += (uintptr_t)0xFFF000;
			g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
			PVR_DPF((PVR_DBG_ERROR, "sRegsPBase.uiAddr = 0x%x", (unsigned int)sRegsPBase.uiAddr));
		}
	}

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

#if GPU_DVFS_TIMER
	g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);
	if (!g_hDVFSTimer)
	{
		OSLockDestroy(ghDVFSLock);
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSEnableTimer(g_hDVFSTimer);
#endif

	gpu_dvfs_enable = 1;
	gpu_powerlimit_enable = 1;
	gpu_touchboost_enable = 1;
	gpu_smartboost_enable = 1;

	g_sys_dvfs_time_ms = 0;

	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	mt_gpufreq_input_boost_notify_registerCB(MTKFreqInputBoostCB);
	mt_gpufreq_power_limit_notify_registerCB(MTKFreqPowerLimitCB);

	mtk_boost_gpu_freq_fp = MTKBoostGpuFreq;
	mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;
	mtk_enable_gpu_dvfs_timer_fp = MTKMFGEnableDVFSTimer;

	mtk_get_gpu_loading_fp = MTKGetGpuLoading;
	mtk_get_gpu_block_fp   = MTKGetGpuBlock;
	mtk_get_gpu_idle_fp    = MTKGetGpuIdle;
	mtk_get_gpu_power_loading_fp = MTKGetPowerIndex;

	return PVRSRV_OK;
}


int MTKMFGSystemDeInit(void)
{
	g_bExit = IMG_TRUE;

#if GPU_DVFS_TIMER
	if (g_hDVFSTimer)
	{
		OSDisableTimer(g_hDVFSTimer);
		OSRemoveTimer(g_hDVFSTimer);
		g_hDVFSTimer = IMG_NULL;
	}
#endif

	if (ghDVFSLock)
	{
		OSLockDestroy(ghDVFSLock);
		ghDVFSLock = NULL;
	}

	g_pvRegsBaseKM = NULL;

	if (g_pvRegsKM)
	{
		OSUnMapPhysToLin(g_pvRegsKM, 0xFF, 0);
		g_pvRegsKM = NULL;
	}

	return PVRSRV_OK;
}


module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_power, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(gpu_powerlimit_enable, uint, 0644);
module_param(gpu_touchboost_enable, uint, 0644);
module_param(gpu_smartboost_enable, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);
module_param(gpu_dvfs_force_idle, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_thermal_freq, uint, 0644);
