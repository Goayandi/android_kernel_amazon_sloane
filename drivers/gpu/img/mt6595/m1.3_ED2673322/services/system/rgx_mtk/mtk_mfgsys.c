/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2005
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE. 
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

#include <linux/delay.h>
#include "mtk_gpu_hal.h"
#include "mtk_mfgsys.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_boot.h"
#include "osfunc.h"
#include "pvrsrv_error.h"
#include "pvrsrv.h"

#define MTK_RGX_DEVICE_INDEX_INVALID -1

#if GPU_DVFS_TIMER
static IMG_HANDLE g_hDVFSTimer = NULL;
#endif

static IMG_UINT32 g_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
static IMG_UINT32 gpu_freq_index = 1;
static IMG_UINT32 gpu_loading = 0;
static IMG_UINT32 gpu_last_loading = 0;

static IMG_INT32 MtkGetRGXDevIdx(IMG_VOID)
{
    IMG_UINT32 ui32DeviceIndex = MTK_RGX_DEVICE_INDEX_INVALID;
    IMG_UINT32 i;

    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

    for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
    {
        PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
        if (psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
        {
            ui32DeviceIndex = i;
        }
    }
    return ui32DeviceIndex;
}

static IMG_UINT32  MTKCalGpuLoading(IMG_VOID)
{
    if (MTK_RGX_DEVICE_INDEX_INVALID == g_ui32RGXDevIdx)
    {
        g_ui32RGXDevIdx = MtkGetRGXDevIdx();
    }

    if (MTK_RGX_DEVICE_INDEX_INVALID != g_ui32RGXDevIdx)
    {
        PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
        PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[g_ui32RGXDevIdx];
        if (psDeviceNode)
        {
    	    PVRSRV_RGXDEV_INFO* psDevInfo = psDeviceNode->pvDevice;
            if (psDevInfo->pfnGetGpuUtilStats)
            {
                RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};
                sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDeviceNode);
                if(sGpuUtilStats.bPoweredOn)
                {
#if 0
                    PVR_DPF((PVR_DBG_ERROR,"Loading: A(%d), I(%d), B(%d)", 
                        sGpuUtilStats.ui32GpuStatActive, sGpuUtilStats.ui32GpuStatIdle, sGpuUtilStats.ui32GpuStatBlocked));
#endif
                    return sGpuUtilStats.ui32GpuStatActive / 100;
                }
            }
        }
    }

    return 0;
}

static IMG_UINT32  MTKGetGpuLoading(IMG_VOID)
{
    return gpu_loading;
}

static IMG_VOID MtkWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
    RGX_DATA* psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
    psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000; /* kHz to Hz write to RGX as the same unit */
}

static IMG_VOID MTKMFGProcessDVFS(unsigned int ui32Loading)
{
    unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    unsigned int ui32CurFreqID = gpu_freq_index;//mt_gpufreq_get_cur_freq_index();
    unsigned int ui32NewFreqID = ui32MaxLevel - (ui32Loading * ui32MaxLevel / 100);

    if ((ui32NewFreqID != ui32CurFreqID) && (MTK_RGX_DEVICE_INDEX_INVALID != g_ui32RGXDevIdx))
    {
        if (PVRSRV_OK == PVRSRVDevicePreClockSpeedChange(g_ui32RGXDevIdx, FALSE, (IMG_VOID*)NULL))
        {
            unsigned int ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32NewFreqID);

            MtkWriteBackFreqToRGX(g_ui32RGXDevIdx, ui32GPUFreq);

            mt_gpufreq_target(ui32NewFreqID);

            PVRSRVDevicePostClockSpeedChange(g_ui32RGXDevIdx, FALSE, (IMG_VOID*)NULL);
        }
    }

    gpu_freq_index = ui32NewFreqID;
}

static IMG_VOID MTKDVFSTimerFuncCB(IMG_VOID *param)
{
    gpu_loading = MTKCalGpuLoading();

    MTKMFGProcessDVFS(gpu_loading);
}

extern unsigned int (*mtk_get_gpu_loading_fp)(void);

int MTKMFGSystemInit(void)
{
    mtk_get_gpu_loading_fp = MTKGetGpuLoading;

#if GPU_DVFS_TIMER
    g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL);

	if (!g_hDVFSTimer)
	{
        PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
	}
#endif

	return PVRSRV_OK;
}

int MTKMFGSystemDeInit(void)
{
#if GPU_DVFS_TIMER
	if(g_hDVFSTimer)
	{
		OSRemoveTimer(g_hDVFSTimer);

		g_hDVFSTimer = IMG_NULL;
    }
#endif
	return PVRSRV_OK;
}

static int MtkEnableMfgClock(void)
{
	enable_clock(MT_CG_MFG_AXI, "MFG");
	enable_clock(MT_CG_MFG_MEM, "MFG");
	enable_clock(MT_CG_MFG_G3D, "MFG");
	enable_clock(MT_CG_MFG_26M, "MFG");
	return PVRSRV_OK;
}

static int MtkDisableMfgClock(void)
{
	disable_clock(MT_CG_MFG_26M, "MFG");
    disable_clock(MT_CG_MFG_G3D, "MFG");
	disable_clock(MT_CG_MFG_MEM, "MFG");
	disable_clock(MT_CG_MFG_AXI, "MFG");
	return PVRSRV_OK;
}

static int MTKMFGClockPowerOn(void)
{
#if 0 //TBD
    #if MTK_PM_SUPPORT
	#else
	DRV_WriteReg32(MFG_PDN_RESET, 0x1);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
	DRV_WriteReg32(MFG_PDN_SET, 0xf);
	DRV_WriteReg32(MFG_PDN_RESET, 0x0);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
	#endif

	#if 0//RD_POWER_ISLAND
	//MtkEnableMfgClock();
	#endif
	
	mtk_mfg_debug("MFG_PDN_STATUS: 0x%X\n",DRV_Reg32(MFG_PDN_STATUS));
	
	//DRAM access path: need configuration for each power-up
	// 0x1 means G3D select Dram access through CCI-400
    DRV_WriteReg32(MFG_DRAM_ACCESS_PATH, 0x00000000);
#endif
	return PVRSRV_OK;

}

static int MTKEnableHWAPM(void)
{
	DRV_WriteReg32(0xf3fff024, 0x8007050f);
	DRV_WriteReg32(0xf3fff028, 0x0e0c0a09);
	return PVRSRV_OK;
}

static int MTKDisableHWAPM(void)
{
	DRV_WriteReg32(0xf3fff024, 0x0);
	DRV_WriteReg32(0xf3fff028, 0x0);
	return PVRSRV_OK;
}

bool MTKMFGIsE2andAboveVersion()
{
	CHIP_SW_VER ver = 0;
	ver = mt_get_chip_sw_ver();
	if(CHIP_SW_VER_02 <= ver)
	{
		return TRUE;
	}

    return FALSE;
}

IMG_UINT32 MTKMFGGetPowerIndex(IMG_VOID)
{
	DRV_WriteReg32(0xf3fff024, 0x0);
	DRV_WriteReg32(0xf3fff028, 0x0);    

}

PVRSRV_ERROR MTKDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState )
    {
#if GPU_DVFS_TIMER
        if (g_hDVFSTimer)
        {
            OSDisableTimer(g_hDVFSTimer);
        }
#endif
        gpu_last_loading = MTKCalGpuLoading();

        if(MTKMFGIsE2andAboveVersion())
        {
            MTKDisableHWAPM();
        }

        MtkDisableMfgClock();   

        gpu_loading = 0;
    }

	return PVRSRV_OK;	
}

PVRSRV_ERROR MTKDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eNewPowerState)
    {
        MtkEnableMfgClock();

        if(MTKMFGIsE2andAboveVersion())
        {
            MTKEnableHWAPM();
        }

        MTKMFGProcessDVFS(gpu_last_loading);

#if GPU_DVFS_TIMER
        if (g_hDVFSTimer)
        {
            OSEnableTimer(g_hDVFSTimer);
        }
#endif
    }

    return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
    {
#if 0
        if(MTKMFGIsE2andAboveVersion())
        {
            MTKDisableHWAPM();
        }
        MtkDisableMfgClock();
#endif
    }

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{
#if 0
        MtkEnableMfgClock();
        if(MTKMFGIsE2andAboveVersion())
        {
            MTKEnableHWAPM();
        }

        //MTKMFGProcessDVFS();
#endif
    }

	return PVRSRV_OK;
}

module_param(gpu_loading, ulong, 0644);
module_param(gpu_freq_index, ulong, 0644);

