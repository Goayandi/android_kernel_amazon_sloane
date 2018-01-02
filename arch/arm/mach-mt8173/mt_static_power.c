#include <linux/kernel.h>
#include <linux/init.h>
#if defined(__KERNEL__)		/* || !defined (__CTP__) */
#include <linux/export.h>
#include <linux/module.h>
#endif				/* #if !defined (__CTP__) */

#include <asm/system.h>
#include "mt_spower_data.h"
#include "mach/mt_static_power.h"

/*********************************
 * macro for log
 **********************************/
#define SPOWER_LOG_NONE                                0
#define SPOWER_LOG_WITH_XLOG                           1
#define SPOWER_LOG_WITH_PRINTK                         2

#define SPOWER_LOG_PRINT SPOWER_LOG_WITH_PRINTK

#if (SPOWER_LOG_PRINT == SPOWER_LOG_NONE)
#define SPOWER_INFO(fmt, args...)
#elif (SPOWER_LOG_PRINT == SPOWER_LOG_WITH_XLOG)
#define SPOWER_INFO(fmt, args...)		xlog_printk(ANDROID_LOG_INFO, "Power/spower", fmt, ##args)
#elif (SPOWER_LOG_PRINT == SPOWER_LOG_WITH_PRINTK)
#define SPOWER_INFO(fmt, args...)		printk("[Power/spower] "fmt, ##args)
#endif



static sptbl_t sptab[MT_SPOWER_MAX];

/****************************************************************
 * this table is generated by scramble function.                *
 * (plz refer to DE team.)                                      *
 ****************************************************************/
char devinfo_table[] = {
	71, 148, 16, 34, 1349, 2818, 309, 646,
	69, 145, 16, 33, 1318, 2754, 302, 631,
	74, 155, 17, 35, 1413, 2951, 324, 676,
	72, 151, 17, 35, 1380, 2884, 316, 661,
	65, 135, 15, 31, 1230, 2570, 282, 589,
	63, 132, 14, 30, 1202, 2512, 275, 575,
	68, 141, 15, 32, 1288, 2692, 295, 617,
	66, 138, 15, 32, 1259, 2630, 288, 603,
	85, 178, 19, 41, 1622, 3388, 372, 776,
	83, 174, 19, 40, 1585, 3311, 363, 759,
	89, 186, 20, 43, 1698, 3548, 389, 813,
	87, 182, 20, 42, 1660, 3467, 380, 794,
	78, 162, 18, 37, 1479, 3090, 339, 708,
	76, 158, 17, 36, 1445, 3020, 331, 692,
	81, 170, 19, 39, 1549, 3236, 355, 741,
	79, 166, 18, 38, 1514, 3162, 347, 724,
	49, 102, 11, 23, 933, 1950, 214, 447,
	48, 100, 11, 23, 912, 1905, 209, 437,
	51, 107, 12, 25, 977, 2042, 224, 468,
	50, 105, 11, 24, 955, 1995, 219, 457,
	45, 93, 10, 21, 851, 1778, 195, 407,
	44, 91, 10, 21, 832, 1738, 191, 398,
	47, 98, 11, 22, 891, 1862, 204, 427,
	46, 95, 10, 22, 871, 1820, 200, 417,
	59, 123, 13, 28, 1122, 2344, 257, 537,
	58, 120, 13, 28, 1096, 2291, 251, 525,
	62, 129, 14, 30, 1175, 2455, 269, 562,
	60, 126, 14, 29, 1148, 2399, 263, 550,
	54, 112, 12, 26, 1023, 2138, 234, 490,
	52, 110, 12, 25, 1000, 2089, 229, 479,
	56, 117, 13, 27, 1072, 2239, 245, 513,
	55, 115, 13, 26, 1047, 2188, 240, 501,
};


int interpolate(int x1, int x2, int x3, int y1, int y2)
{
	BUG_ON(x1 == x2);

	return (x3 - x1) * (y2 - y1) / (x2 - x1) + y1;
}

int interpolate_2d(sptbl_t *tab, int v1, int v2, int t1, int t2, int voltage, int degree)
{
	int c1, c2, p1, p2, p;

	if (v1 == v2 && t1 == t2) {
		p = mA(tab, v1, t1);
		return p;
	} else if (v1 == v2) {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v1, t2);
		p = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);
		return p;
	} else if (t1 == t2) {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v2, t1);
		p = interpolate(mV(tab, v1), mV(tab, v2), voltage, c1, c2);
		return p;
	} else {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v1, t2);
		p1 = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);

		c1 = mA(tab, v2, t1);
		c2 = mA(tab, v2, t2);
		p2 = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);

		p = interpolate(mV(tab, v1), mV(tab, v2), voltage, p1, p2);
		return p;
	}
}

void interpolate_table(sptbl_t *spt, int c1, int c2, int c3, sptbl_t *tab1, sptbl_t *tab2)
{
	int v, t;

	/* avoid divid error, if we have bad raw data table */
	if (unlikely(c1 == c2)) {
		*spt = *tab1;
		SPOWER_INFO("sptab equal to tab1:%d/%d\n", c1, c3);
		return;
	}

	SPOWER_INFO("make sptab %d, %d, %d\n", c1, c2, c3);
	for (t = 0; t < tsize(spt); t++) {
		for (v = 0; v < vsize(spt); v++) {
			int *p = &mA(spt, v, t);
			p[0] = interpolate(c1, c2, c3, mA(tab1, v, t), mA(tab2, v, t));

			printk("%d ", p[0]);
		}
		printk("\n");
	}
	SPOWER_INFO("make sptab done!\n");

	return;
}


int sptab_lookup(sptbl_t *tab, int voltage, int degree)
{
	int x1, x2, y1, y2, i;
	int mamper;

	/** lookup voltage **/
	for (i = 0; i < vsize(tab); i++) {
		if (voltage <= mV(tab, i))
			break;
	}

	if (unlikely(voltage == mV(tab, i))) {
		x1 = x2 = i;
	} else if (unlikely(i == vsize(tab))) {
		x1 = vsize(tab) - 2;
		x2 = vsize(tab) - 1;
	} else if (i == 0) {
		x1 = 0;
		x2 = 1;
	} else {
		x1 = i - 1;
		x2 = i;
	}


	/** lookup degree **/
	for (i = 0; i < tsize(tab); i++) {
		if (degree <= deg(tab, i))
			break;
	}

	if (unlikely(degree == deg(tab, i))) {
		y1 = y2 = i;
	} else if (unlikely(i == tsize(tab))) {
		y1 = tsize(tab) - 2;
		y2 = tsize(tab) - 1;
	} else if (i == 0) {
		y1 = 0;
		y2 = 1;
	} else {
		y1 = i - 1;
		y2 = i;
	}

	mamper = interpolate_2d(tab, x1, x2, y1, y2, voltage, degree);

	return mamper;
}


int mt_spower_make_table(sptbl_t *spt, spower_raw_t *spower_raw, int wat, int voltage, int degree)
{
	int i;
	int c1, c2, c = -1;
	sptbl_t tab[MAX_TABLE_SIZE], *tab1, *tab2, *tspt;

	/** FIXME, test only; please read efuse to assign. **/
	/* wat = 80; */
	/* voltage = 1150; */
	/* degree = 25; */

	BUG_ON(spower_raw->table_size < 3);

	/** structurize the raw data **/
	spower_tab_construct(&tab, spower_raw);

	/** lookup tables which the chip type locates to **/
	for (i = 0; i < spower_raw->table_size; i++) {
		c = sptab_lookup(&tab[i], voltage, degree);
		/** table order: ff, tt, ss **/
		if (wat >= c)
			break;
	}

	/** FIXME,
	 * There are only 2 tables are used to interpolate to form SPTAB.
	 * Thus, sptab takes use of the container which raw data is not used anymore.
	 **/
	if (wat == c) {
		/** just match **/
		tab1 = tab2 = &tab[i];
		/** pointer duplicate  **/
		tspt = tab1;
	} else if (i == spower_raw->table_size) {
		/** above all **/
#if defined(EXTER_POLATION)
		tab1 = &tab[spower_raw->table_size - 2];
		tab2 = &tab[spower_raw->table_size - 1];

		/** occupy the free container**/
		tspt = &tab[spower_raw->table_size - 3];
#else				/* #if defined (EXTER_POLATION) */
		tspt = tab1 = tab2 = &tab[spower_raw->table_size - 1];
#endif				/* #if defined (EXTER_POLATION) */
	} else if (i == 0) {
#if defined(EXTER_POLATION)
		/** below all **/
		tab1 = &tab[0];
		tab2 = &tab[1];

		/** occupy the free container**/
		tspt = &tab[2];
#else				/* #if defined (EXTER_POLATION) */
		tspt = tab1 = tab2 = &tab[0];
#endif				/* #if defined (EXTER_POLATION) */
	} else {
		/** anyone **/
		tab1 = &tab[i - 1];
		tab2 = &tab[i];

		/** occupy the free container**/
		tspt = &tab[(i + 1) % spower_raw->table_size];
	}


	/** sptab needs to interpolate 2 tables. **/
	if (tab1 != tab2) {
		c1 = sptab_lookup(tab1, voltage, degree);
		c2 = sptab_lookup(tab2, voltage, degree);

		interpolate_table(tspt, c1, c2, wat, tab1, tab2);
	}

	/** update to global data **/
	*spt = *tspt;

	return 0;
}




#define MT_SPOWER_UT 1

#if defined(MT_SPOWER_UT)
void mt_spower_ut(void)
{
	int v, t, p, i;

	for (i = 0; i < MT_SPOWER_MAX; i++) {
		sptbl_t *spt = &sptab[i];

		v = 750;
		t = 22;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 750;
		t = 25;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 750;
		t = 28;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 750;
		t = 82;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 750;
		t = 120;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 820;
		t = 22;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 820;
		t = 25;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 820;
		t = 28;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 820;
		t = 82;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);


		v = 820;
		t = 120;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 1200;
		t = 22;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 1200;
		t = 25;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 1200;
		t = 28;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 1200;
		t = 82;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);


		v = 1200;
		t = 120;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);


		v = 950;
		t = 80;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

		v = 1000;
		t = 85;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);


		v = 1150;
		t = 105;
		p = sptab_lookup(spt, v, t);
		SPOWER_INFO("v/t/p: %d/%d/%d\n", v, t, p);

	}
}


#endif				/* #if defined (MT_SPOWER_UT) */

extern u32 get_devinfo_with_index(u32 index);


int mt_spower_init(void)
{
#define DEVINFO_IDX (17)
#define DEVINFO_CA17_BIT (0)
#define DEVINFO_CA7_BIT (8)
#define DEVINFO_GPU_BIT (16)
#define DEVINFO_SOC_BIT (24)

	int devinfo = (int)get_devinfo_with_index(DEVINFO_IDX);
	int ca17_leak = (devinfo >> DEVINFO_CA17_BIT) & 0x0ff;
	int ca7_leak = (devinfo >> DEVINFO_CA7_BIT) & 0x0ff;
	int gpu_leak = (devinfo >> DEVINFO_GPU_BIT) & 0x0ff;
	int soc_leak = (devinfo >> DEVINFO_SOC_BIT) & 0x0ff;

	ca17_leak = (int)devinfo_table[ca17_leak];
	ca7_leak = (int)devinfo_table[ca7_leak];
	gpu_leak = (int)devinfo_table[gpu_leak];
	soc_leak = (int)devinfo_table[soc_leak];

	mt_spower_make_table(&sptab[0], &ca7_spower_raw, ca17_leak, 1150, 25);
	mt_spower_make_table(&sptab[1], &ca15l_spower_raw, ca7_leak, 1150, 25);
	mt_spower_make_table(&sptab[2], &gpu_spower_raw, gpu_leak, 1150, 25);

#if defined(MT_SPOWER_UT)
	mt_spower_ut();
#endif				/* #if defined (MT_SPOWER_UT) */

	return 0;
}
late_initcall(mt_spower_init);


/** return -1, means sptab is not yet ready. **/
int mt_spower_get_leakage(int dev, int vol, int deg)
{
	BUG_ON(!(dev < MT_SPOWER_MAX));

	if (!tab_validate(&sptab[dev]))
		return -1;

	return sptab_lookup(&sptab[dev], vol, deg);
}
EXPORT_SYMBOL(mt_spower_get_leakage);