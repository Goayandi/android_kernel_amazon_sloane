
#include "servicesext.h"
#include "rgxdevice.h"


#ifndef MTK_MFGSYS_H
#define MTK_MFGSYS_H

/* control APM is enabled or not  */
#define MTK_PM_SUPPORT 1

/*  unit ms, timeout interval for DVFS detection */
#define MTK_DVFS_SWITCH_INTERVAL  300

#define GPU_DVFS_TIMER 0

//extern to be used by PVRCore_Init in RGX DDK module.c 
int MTKMFGSystemInit(void);

int MTKMFGSystemDeInit(void);

bool MTKMFGIsE2andAboveVersion(void);


/* below register interface in RGX sysconfig.c */
PVRSRV_ERROR MTKDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced);

PVRSRV_ERROR MTKDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced);

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

#endif // MTK_MFGSYS_H

