#ifndef __DEVS_H__
#define __DEVS_H__

#include <mach/board.h>

#define CFG_DEV_UART1
#if 0				/* for FPGA bring up */
#define CFG_DEV_UART2
#define CFG_DEV_UART3
#define CFG_DEV_UART4
#endif

/*
 * Define constants.
 */

#define MTK_UART_SIZE 0x100

/*
 * Define function prototype.
 */

extern int mt_board_init(void);

/* extern unsigned int *get_modem_size_list(void); */
/* extern unsigned int get_nr_modem(void); */

#endif				/* !__MT6575_DEVS_H__ */
