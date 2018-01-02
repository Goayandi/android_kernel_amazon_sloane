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

#if! defined(__DC_PDP_H__)
#define __DC_PDP_H__

#include "img_types.h"
#include "pvrsrv_error.h"

#define DRVNAME	"dc_pdp"

#if !defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
#error PDP requires that SUPPORT_SYSTEM_INTERRUPT_HANDLING be enabled in the build
#endif

/*************************************************************************/ /*!
 PCI Device Information
*/ /**************************************************************************/

#define DCPDP_VENDOR_ID_POWERVR			(0x1010)

#define DCPDP_DEVICE_ID_PCI_APOLLO_FPGA		(0x1CF1)
#define DCPDP_DEVICE_ID_PCIE_APOLLO_FPGA	(0x1CF2)


/*************************************************************************/ /*!
 PCI Device Base Address Information
*/ /**************************************************************************/

/* PLL and PDP registers on base address register 0 */
#define DCPDP_REG_PCI_BASENUM			(0)

#define DCPDP_PCI_PLL_REG_OFFSET		(0x1000)
#define DCPDP_PCI_PLL_REG_SIZE			(0x0400)

#define DCPDP_PCI_PDP_REG_OFFSET		(0xC000)
#define DCPDP_PCI_PDP_REG_SIZE			(0x2000)


/*************************************************************************/ /*!
 Misc register information
*/ /**************************************************************************/

/* This information isn't captured in tcf_rgbpdp_regs.h so define it here */
#define DCPDP_STR1SURF_FORMAT_ARGB8888		(0xE)
#define DCPDP_STR1ADDRCTRL_BASE_ADDR_SHIFT	(4)
#define DCPDP_STR1POSN_STRIDE_SHIFT		(4)


/*************************************************************************/ /*!
 dc_pdp OS functions
*/ /**************************************************************************/

typedef struct DCPDP_MODULE_PARAMETERS_TAG
{
	IMG_UINT32  ui32PDPEnabled;
	IMG_UINT32  ui32PDPWidth;
	IMG_UINT32  ui32PDPHeight;
} DCPDP_MODULE_PARAMETERS;

const DCPDP_MODULE_PARAMETERS *DCPDPGetModuleParameters(IMG_VOID);


/*******************************************************************************
 * dc_pdp functions
 ******************************************************************************/

PVRSRV_ERROR DCPDPInit(IMG_VOID *pvDevice);
IMG_VOID DCPDPDeInit(IMG_VOID **ppvDevice);

#endif /* !defined(__DC_PDP_H__) */
