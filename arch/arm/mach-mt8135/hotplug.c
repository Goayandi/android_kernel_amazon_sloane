#include <linux/errno.h>	/* EPERM */
#include <asm/cacheflush.h>	/* flush_cache_all */
#include <linux/cpu.h>
#include <mach/hotplug.h>
#include <mach/mt_spm.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <trustzone/kree/tz_pm.h>

#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
#include <mach/mt_spm_mtcmos.h>
#endif

#include <mach/wd_api.h>
#ifdef CONFIG_MTK_SCHED_TRACERS
#include <trace/events/mtk_events.h>
#include "kernel/trace/trace.h"
DEFINE_PER_CPU(unsigned long long, last_event_ts);
#endif

/*
 * global variable
 */
atomic_t hotplug_cpu_count = ATOMIC_INIT(1);


#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
/*
 * static function
 */
static inline void cpu_enter_lowpower(unsigned int cpu)
{
	HOTPLUG_INFO("cpu_enter_lowpower\n");

	/* Cluster off */
	if ((cpu == 3 && cpu_online(2) == 0) || (cpu == 2 && cpu_online(3) == 0)) {

		/* Clear the SCTLR C bit to prevent further data cache allocation */
		__disable_dcache();
		isb();
		dsb();

		/* Clean and invalidate all data from the L1, L2 data cache */
		inner_dcache_flush_all();
		/* flush_cache_all(); */

		/* Switch the processor from SMP mode to AMP mode by clearing the ACTLR SMP bit */
		__switch_to_amp();

		isb();
		dsb();

		/* disable CA15 CCI */
		spm_write(CA15_CCI400_DVM_EN, spm_read(CA15_CCI400_DVM_EN) & ~0x3);
		/* wait cci change pending */
		while (spm_read(CCI400_STATUS) & 0x1)
			;
		/* Ensure the ACP master does not send further requests to the individual processor.
		   Assert AINACTS to idle the ACP slave interface after all responses are received. */
		/* mt65xx_reg_sync_writel( *CA15_MISC_DBG | 0x11, CA15_MISC_DBG); */
		spm_write(CA15_MISC_DBG, spm_read(CA15_MISC_DBG) | 0x11);

	} else {
		/* Clear the SCTLR C bit to prevent further data cache allocation */
		__disable_dcache();
		isb();
		dsb();
		/* Clean and invalidate all data from the L1 data cache */
		inner_dcache_flush_L1();
		/* Just flush the cache. */
		/* flush_cache_all(); */
		/* Execute a CLREX instruction */
		__asm__ __volatile__("clrex");
		/* Switch the processor from SMP mode to AMP mode by clearing the ACTLR SMP bit */
		__switch_to_amp();
	}

}

static inline void cpu_leave_lowpower(unsigned int cpu)
{
	/* HOTPLUG_INFO("cpu_leave_lowpower\n"); */

	if ((cpu == 3 && cpu_online(2) == 0) || (cpu == 2 && cpu_online(3) == 0)) {
		spm_write(CA15_MISC_DBG, spm_read(CA15_MISC_DBG) & ~0x11);
		spm_write(CA15_CCI400_DVM_EN, spm_read(CA15_CCI400_DVM_EN) | 0x3);
		/* wait cci change pending */
		while (spm_read(CCI400_STATUS) & 0x1)
			;
	}

	/* Set the ACTLR.SMP bit to 1 for SMP mode */
	__switch_to_smp();

	/* Enable dcache */
	__enable_dcache();
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	/* Just enter wfi for now. TODO: Properly shut off the cpu. */
	for (;;) {

		/* Execute an ISB instruction to ensure that all of the CP15 register changes from
		the previous steps have been committed */
		isb();

		/* Execute a DSB instruction to ensure that all cache, TLB and branch predictor maintenance operations
		issued by any processor in the multiprocessor device before the SMP bit was cleared have completed */
		dsb();

		/*
		 * here's the WFI
		 */
		__asm__ __volatile__("wfi");

		if (pen_release == cpu_logical_map(cpu)) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}
#else
static inline void cpu_enter_lowpower(unsigned int cpu)
{
}

static inline void cpu_leave_lowpower(unsigned int cpu)
{
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	/* Use TEE PM to power off this cpu */
	kree_pm_cpu_lowpower(&pen_release, cpu_logical_map(cpu));

	/*
	 * Getting here, means that we have come out of WFI without
	 * having been woken up - this shouldn't happen
	 *
	 * Just note it happening - when we're woken, we can report
	 * its occurrence.
	 */
	(*spurious)++;
}
#endif				/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */





/*
 * platform_cpu_kill:
 * @cpu:
 * Return TBD.
 */
int mt_cpu_kill(unsigned int cpu)
{
	HOTPLUG_INFO("platform_cpu_kill, cpu: %d\n", cpu);

#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
	switch (cpu) {
	case 1:
		spm_mtcmos_ctrl_cpu1(STA_POWER_DOWN);
		break;
	case 2:
		spm_mtcmos_ctrl_cpu2(STA_POWER_DOWN);
		break;
	case 3:
		spm_mtcmos_ctrl_cpu3(STA_POWER_DOWN);
		break;
	default:
		break;
	}
#endif

	atomic_dec(&hotplug_cpu_count);

	return 1;
}

/*
 * platform_cpu_die: shutdown a CPU
 * @cpu:
 */
void __ref mt_cpu_die(unsigned int cpu)
{
	int spurious = 0;

	struct wd_api *wd_api = NULL;
	get_wd_api(&wd_api);
	HOTPLUG_INFO("platform_cpu_die, cpu: %d\n", cpu);

#ifdef CONFIG_MTK_WD_KICKER

	wd_api->wd_cpu_hot_plug_off_notify(cpu);

#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
	trace_cpu_hotplug(cpu, 0, per_cpu(last_event_ts, cpu));
	per_cpu(last_event_ts, cpu) = ns2usecs(ftrace_now(cpu));
#endif

	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower(cpu);
	platform_do_lowpower(cpu, &spurious);

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	cpu_leave_lowpower(cpu);

	if (spurious)
		HOTPLUG_INFO("platform_do_lowpower, spurious wakeup call, cpu: %d, spurious: %d\n",
			     cpu, spurious);
}

/*
 * platform_cpu_disable:
 * @cpu:
 * Return error code.
 */
int mt_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	HOTPLUG_INFO("platform_cpu_disable, cpu: %d\n", cpu);
	return cpu == 0 ? -EPERM : 0;
}