/**
 * @file mt_cpufreq.h
 * @brief CPU DVFS driver interface
 */

#ifndef __MT_CPUFREQ_H__
#define __MT_CPUFREQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MT_CPUFREQ_C__
#define CPUFREQ_EXTERN
#else
#define CPUFREQ_EXTERN extern
#endif

/*=============================================================*/
/* Include files */
/*=============================================================*/

/* system includes */

/* project includes */
#ifndef __KERNEL__
#include "freqhop_sw.h"
#else				/* __KERNEL__ */
#include "mach/mt_reg_base.h"
#include "mach/mt_clkmgr.h"
#endif				/* ! __KERNEL__ */

/* local includes */

/* forward references */


/*=============================================================*/
/* Macro definition */
/*=============================================================*/


/*=============================================================*/
/* Type definition */
/*=============================================================*/

	enum mt_cpu_dvfs_id {

#ifdef CONFIG_MTK_FORCE_CLUSTER1
		MT_CPU_DVFS_BIG,
		MT_CPU_DVFS_LITTLE,
#else
		MT_CPU_DVFS_LITTLE,
		MT_CPU_DVFS_BIG,
#endif

		NR_MT_CPU_DVFS,
	};

	enum top_ckmuxsel {
		TOP_CKMUXSEL_CLKSQ = 0,	/* i.e. reg setting */
		TOP_CKMUXSEL_ARMPLL = 1,
		TOP_CKMUXSEL_UNIVPLL = 2,
		TOP_CKMUXSEL_MAINPLL = 3,

		NR_TOP_CKMUXSEL,
	};

/*
 * PMIC_WRAP
 */

/* Phase */
	enum pmic_wrap_phase_id {
		PMIC_WRAP_PHASE_NORMAL,
		PMIC_WRAP_PHASE_SUSPEND,
		PMIC_WRAP_PHASE_DEEPIDLE,
		PMIC_WRAP_PHASE_SODI,

		NR_PMIC_WRAP_PHASE,
	};

/* IDX mapping */
	enum {
		IDX_NM_VSRAM_CA15L,	/* 0 *//* PMIC_WRAP_PHASE_NORMAL */
		IDX_NM_VPROC_CA7,	/* 1 */
		IDX_NM_VGPU,	/* 2 */
		IDX_NM_VCORE_AO,	/* 3 */
		IDX_NM_VCORE_PDN,	/* 4 */

		NR_IDX_NM,
	};
	enum {
		IDX_SP_VSRAM_CA15L_PWR_ON,	/* 0 *//* PMIC_WRAP_PHASE_SUSPEND */
		IDX_SP_VSRAM_CA15L_SHUTDOWN,	/* 1 */
		IDX_SP_VPROC_CA7_PWR_ON,	/* 2 */
		IDX_SP_VPROC_CA7_SHUTDOWN,	/* 3 */
		IDX_SP_VSRAM_CA7_PWR_ON,	/* 4 */
		IDX_SP_VSRAM_CA7_SHUTDOWN,	/* 5 */
		IDX_SP_VCORE_PDN_EN_HW_MODE,	/* 6 */
		IDX_SP_VCORE_PDN_EN_SW_MODE,	/* 7 */

		NR_IDX_SP,
	};
	enum {
		IDX_DI_VSRAM_CA15L_NORMAL,	/* 0 *//* PMIC_WRAP_PHASE_DEEPIDLE */
		IDX_DI_VSRAM_CA15L_SLEEP,	/* 1 */
		IDX_DI_VPROC_CA7_NORMAL,	/* 2 */
		IDX_DI_VPROC_CA7_SLEEP,	/* 3 */
		IDX_DI_VCORE_AO_NORMAL,	/* 4 */
		IDX_DI_VCORE_AO_SLEEP,	/* 5 */
		IDX_DI_VCORE_PDN_NORMAL,	/* 6 */
		IDX_DI_VCORE_PDN_SLEEP,	/* 7 */

		NR_IDX_DI,
	};

	typedef void (*cpuVoltsampler_func) (enum mt_cpu_dvfs_id, unsigned int mv);
/*=============================================================*/
/* Global variable definition */
/*=============================================================*/


/*=============================================================*/
/* Global function definition */
/*=============================================================*/

/* PMIC WRAP */
	CPUFREQ_EXTERN void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase);
	CPUFREQ_EXTERN void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx,
						    unsigned int cmd_wdata);
	CPUFREQ_EXTERN void mt_cpufreq_apply_pmic_cmd(int idx);

/* PTP-OD */
	CPUFREQ_EXTERN unsigned int mt_cpufreq_max_frequency_by_DVS(enum mt_cpu_dvfs_id id,
								    int idx);
	CPUFREQ_EXTERN int mt_cpufreq_voltage_set_by_ptpod(enum mt_cpu_dvfs_id id,
							   unsigned int *volt_tbl, int nr_volt_tbl);
	CPUFREQ_EXTERN void mt_cpufreq_return_default_DVS_by_ptpod(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN unsigned int mt_cpufreq_cur_vproc(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id);

/* Thermal */
	CPUFREQ_EXTERN void mt_cpufreq_thermal_protect(unsigned int limited_power);

/* SDIO */
	CPUFREQ_EXTERN void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled);
	CPUFREQ_EXTERN void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt);
	CPUFREQ_EXTERN unsigned int mt_vcore_dvfs_volt_get_by_sdio(void);

/* Generic */
	CPUFREQ_EXTERN int mt_cpufreq_state_set(int enabled);
	CPUFREQ_EXTERN int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel);
	CPUFREQ_EXTERN enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);


#ifndef __KERNEL__
	CPUFREQ_EXTERN int mt_cpufreq_pdrv_probe(void);
	CPUFREQ_EXTERN int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx);
	CPUFREQ_EXTERN int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx);
	CPUFREQ_EXTERN unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq);
	CPUFREQ_EXTERN unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id);
	CPUFREQ_EXTERN void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int volt);
	CPUFREQ_EXTERN void dvfs_set_gpu_volt(int pmic_val);
	CPUFREQ_EXTERN void dvfs_set_vcore_ao_volt(int pmic_val);
	CPUFREQ_EXTERN void dvfs_set_vcore_pdn_volt(int pmic_val);
	CPUFREQ_EXTERN void dvfs_disable_by_ptpod(void);
	CPUFREQ_EXTERN void dvfs_enable_by_ptpod(void);
#endif				/* ! __KERNEL__ */

#undef CPUFREQ_EXTERN

#ifdef __cplusplus
}
#endif
#endif				/* __MT_CPUFREQ_H__ */