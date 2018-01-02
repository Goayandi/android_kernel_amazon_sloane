/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if defined(LINUX)

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
/* For MODULE_LICENSE */
#include "pvrmodule.h"

#if defined(SUPPORT_DRM)
#include <drm/drmP.h>
#include "pvr_drm.h"
#endif

#include "dc_osfuncs.h"
#include "dc_pdp.h"

#define DCPDP_WIDTH_MIN			(640)
#define DCPDP_WIDTH_MAX			(1280)
#define DCPDP_HEIGHT_MIN		(480)
#define DCPDP_HEIGHT_MAX		(1024)

#if defined (DCPDP_WIDTH) && !defined (DCPDP_HEIGHT)
#error ERROR: DCPDP_WIDTH defined but DCPDP_HEIGHT not defined
#elif !defined (DCPDP_WIDTH) && defined (DCPDP_HEIGHT)
#error ERROR: DCPDP_HEIGHT defined but DCPDP_WIDTH not defined
#elif !defined (DCPDP_WIDTH) && !defined (DCPDP_HEIGHT)
#define DCPDP_WIDTH			DCPDP_WIDTH_MAX
#define DCPDP_HEIGHT			DCPDP_HEIGHT_MAX
#elif (DCPDP_WIDTH > DCPDP_WIDTH_MAX)
#error ERROR: DCPDP_WIDTH too large (max: 1280)
#elif (DCPDP_WIDTH < DCPDP_WIDTH_MIN)
#error ERROR: DCPDP_WIDTH too small (min: 640)
#elif (DCPDP_HEIGHT > DCPDP_HEIGHT_MAX)
#error ERROR: DCPDP_HEIGHT too large (max: 1024)
#elif (DCPDP_HEIGHT < DCPDP_HEIGHT_MIN)
#error ERROR: DCPDP_HEIGHT too small (max: 480)
#endif

/* PDP module parameters */
DCPDP_MODULE_PARAMETERS  sModuleParams =
{
	.ui32PDPEnabled = 1,
	.ui32PDPWidth   = DCPDP_WIDTH,
	.ui32PDPHeight  = DCPDP_HEIGHT
};
module_param_named(mem_en, sModuleParams.ui32PDPEnabled, uint, S_IRUGO | S_IWUSR);
module_param_named(width,  sModuleParams.ui32PDPWidth,   uint, S_IRUGO | S_IWUSR);
module_param_named(height, sModuleParams.ui32PDPHeight,  uint, S_IRUGO | S_IWUSR);

const DCPDP_MODULE_PARAMETERS *DCPDPGetModuleParameters(IMG_VOID)
{
	return &sModuleParams;
}


#if defined(SUPPORT_DRM) && !defined(NO_HARDWARE)
int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(struct drm_device *psDev)
{
	struct pci_dev *psPCIDev = psDev->pdev;
	PVRSRV_ERROR eError;

	eError = DCPDPInit((IMG_VOID *)psPCIDev);
	if (eError != PVRSRV_OK)
	{
		printk(KERN_ERR DRVNAME " - %s: Failed to initialise device (%d)\n", __FUNCTION__, eError);
		return -ENODEV;
	}

	return 0;
}

void PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(struct drm_device *psDev)
{
	struct pci_dev *psPCIDev;

	DCPDPDeInit((IMG_VOID *)&psPCIDev);

	DC_ASSERT(psDev && psDev->pdev == psPCIDev);
}

#else /* defined(SUPPORT_DRM) && !defined(NO_HARDWARE) */
#if defined(SUPPORT_DRM)
int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(struct drm_device unref__ *psDev)
#else
static int __init dc_pdp_init(void)
#endif
{
	struct pci_dev *psPCIDev;
	PVRSRV_ERROR eError;
	int error;

	psPCIDev = pci_get_device(DCPDP_VENDOR_ID_POWERVR, DCPDP_DEVICE_ID_PCI_APOLLO_FPGA, NULL);
	if (psPCIDev == NULL)
	{
		psPCIDev = pci_get_device(DCPDP_VENDOR_ID_POWERVR, DCPDP_DEVICE_ID_PCIE_APOLLO_FPGA, NULL);
		if (psPCIDev == NULL)
		{
			printk(KERN_ERR DRVNAME " - %s : Failed to get PCI device\n", __FUNCTION__);
			return -ENODEV;
		}
	}

	error = pci_enable_device(psPCIDev);
	if (error != 0)
	{
		printk(KERN_ERR DRVNAME " - %s: Failed to enable PCI device (%d)\n", __FUNCTION__, error);
		return -ENODEV;
	}

	eError = DCPDPInit(psPCIDev);
	if (eError != PVRSRV_OK)
	{
		printk(KERN_ERR DRVNAME " - %s: Failed to initialise device (%d)\n", __FUNCTION__, eError);
		return -ENODEV;
	}

	/* To prevent possible problems with system suspend/resume, we don't
	   keep the device enabled, but rely on the fact that the Rogue driver
	   will have done a pci_enable_device. */
	pci_disable_device(psPCIDev);

	return 0;
}

#if defined(SUPPORT_DRM)
void PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(struct drm_device unref__ *dev)
#else
static void __exit dc_pdp_deinit(void)
#endif
{
	struct pci_dev *psPCIDev;

	DCPDPDeInit((IMG_VOID *)&psPCIDev);
}
#endif /* defined(SUPPORT_DRM) && !defined(NO_HARDWARE) */

#if !defined(SUPPORT_DRM)
module_init(dc_pdp_init);
module_exit(dc_pdp_deinit);
#endif

#endif /* defined(LINUX) */
