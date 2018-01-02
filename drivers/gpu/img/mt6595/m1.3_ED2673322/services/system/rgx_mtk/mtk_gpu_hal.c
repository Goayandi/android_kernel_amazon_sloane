/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_gpu_hal.c
 *
 * Project:
 * --------
 *   MT02183
 *
 * Description:
 * ------------
 *   Implementation of mtk gpu hal
 *
 * Author:
 * -------
 *   Nick Huang
 *
 ****************************************************************************/
#include "rgxdevice.h"
#include "pvrsrv_error.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "mtk_gpu_hal.h"

static PVRSRV_DEVICE_NODE* g_psRGXDevNode = NULL;

static PVRSRV_DEVICE_NODE* MTKGetDevNode(PVRSRV_DEVICE_TYPE eDeviceType)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    if (psPVRSRVData)
    {
        IMG_UINT32 i;
    	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
    	{
    		PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];	
    		if (psDeviceNode->psDevConfig->eDeviceType == eDeviceType)
    		{
                return psDeviceNode;
    		}
    	}
    }
	return NULL;
}

void MTKGpuHalInit(IMG_VOID)
{
    g_psRGXDevNode = MTKGetDevNode(PVRSRV_DEVICE_TYPE_RGX);
}
