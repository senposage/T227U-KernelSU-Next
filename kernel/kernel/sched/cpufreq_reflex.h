/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Measured demand and speed floors from Reflex CPUFreq governor v0.4.0.
 * Copyright (C) Masahito Suzuki
 *
 * Backport for the in-tree schedutil and MediaTek DVFS request paths.
 */
#ifndef _SCHED_CPUFREQ_REFLEX_H
#define _SCHED_CPUFREQ_REFLEX_H

#include <linux/energy_model.h>

#define REFLEX_FLOOR_COUNT	65
#define REFLEX_WEIGHT_DEFAULT	1000
#define REFLEX_WEIGHT_MAX	100000

struct reflex_sample {
	u64 last_sample_ns;
	u64 last_wall_us;
	u64 last_idle_us;
	u64 running_us;
	unsigned long demand;
	bool censored;
	bool initialized;
};

static void reflex_sample_cpu(struct reflex_sample *sample, int cpu, u64 time,
			      u64 interval_ns)
{
	u64 idle, wall, elapsed, stopped, busy;
	unsigned long scale, capacity, measured;

	/* The local clock must close this CPU's idle accounting interval. */
	if (cpu != smp_processor_id() ||
	    (sample->initialized &&
	     (s64)(time - sample->last_sample_ns) < (s64)interval_ns))
		return;

	idle = get_cpu_idle_time(cpu, &wall, 0);
	if (!sample->initialized || unlikely(wall < sample->last_wall_us)) {
		sample->initialized = true;
		goto record;
	}
	elapsed = wall - sample->last_wall_us;
	if (!elapsed)
		return;
	stopped = idle > sample->last_idle_us ?
		  min(idle - sample->last_idle_us, elapsed) : 0;
	busy = elapsed - stopped;
	scale = arch_scale_freq_invariant() ?
		arch_scale_freq_capacity(cpu) : SCHED_CAPACITY_SCALE;
	capacity = arch_scale_cpu_capacity(NULL, cpu);
	measured = div64_u64((busy << SCHED_CAPACITY_SHIFT), elapsed);
	measured = cap_scale(cap_scale(measured, scale), capacity);
	sample->demand = max_t(unsigned long, measured,
		cpufreq_pelt_decay(sample->demand, elapsed * NSEC_PER_USEC));
	sample->running_us = cpufreq_pelt_decay(sample->running_us,
						elapsed * NSEC_PER_USEC) +
			     cpufreq_pelt_decay(busy, stopped * NSEC_PER_USEC);
	sample->censored = !stopped;
record:
	sample->last_sample_ns = time;
	sample->last_wall_us = wall;
	sample->last_idle_us = idle;
}

#ifdef CONFIG_ENERGY_MODEL
static bool reflex_energy_floors(struct cpufreq_policy *policy,
				 unsigned int weight, unsigned int *floor)
{
	const struct em_perf_domain *pd = em_cpu_get(policy->cpu);
	const struct em_cap_state *table, *top;
	u64 lmax = 0;
	int i, k, nr;

	if (!pd || pd->nr_cap_states < 2)
		return false;
	table = pd->table;
	nr = pd->nr_cap_states;
	top = &table[nr - 1];
	/* A stock Energy Model cannot estimate the power of an added OC OPP. */
	if (top->frequency != policy->cpuinfo.max_freq)
		return false;

	for (i = 0; i < nr - 1; i++) {
		u64 a = (u64)top->power * table[i].frequency;
		u64 b = (u64)table[i].power * top->frequency;

		if (table[i].frequency < top->frequency && a > b)
			lmax = max(lmax, DIV64_U64_ROUND_UP(a - b,
					top->frequency - table[i].frequency));
	}

	for (k = 0; k < REFLEX_FLOOR_COUNT; k++) {
		u64 latency = lmax * weight * k, best = U64_MAX, best_rem = 0;
		unsigned int freq = top->frequency;

		for (i = 0; i < nr; i++) {
			u64 score, rem;

			if (!table[i].frequency)
				continue;
			score = div64_u64_rem((u64)table[i].power * 1000 *
					      (REFLEX_FLOOR_COUNT - 1) + latency,
					      table[i].frequency, &rem);
			/* Preserve precision without cross-multiplying the numerator. */
			if (score < best || (score == best &&
			    rem * freq <= best_rem * table[i].frequency)) {
				best = score;
				best_rem = rem;
				freq = table[i].frequency;
			}
		}
		WRITE_ONCE(floor[k], freq);
	}
	return true;
}
#else
static inline bool reflex_energy_floors(struct cpufreq_policy *policy,
					unsigned int weight, unsigned int *floor)
{
	return false;
}
#endif

static u64 reflex_icbrt(u64 value)
{
	u64 lo = 0, hi = 1ULL << 16;

	while (lo < hi) {
		u64 mid = (lo + hi + 1) >> 1;

		if (mid * mid * mid <= value)
			lo = mid;
		else
			hi = mid - 1;
	}
	return lo;
}

static void reflex_speed_floors(struct cpufreq_policy *policy, unsigned int weight,
				unsigned int *floor)
{
	unsigned int fmax = policy->cpuinfo.max_freq;
	u64 den = 1000 * (REFLEX_FLOOR_COUNT - 1);
	int k;

	if (reflex_energy_floors(policy, weight, floor))
		return;

	/* Cube-law power model when the available OPPs have no complete model. */
	for (k = 0; k < REFLEX_FLOOR_COUNT; k++) {
		u64 num = (u64)weight * k;
		unsigned int freq = fmax;

		if (num < den)
			freq = ((u64)fmax *
				reflex_icbrt(div64_u64(num << 48, den))) >> 16;
		WRITE_ONCE(floor[k], freq);
	}
}

static unsigned int reflex_floor_index(const struct reflex_sample *sample,
				       u64 interval_ns)
{
	u64 alpha = div_u64(interval_ns, NSEC_PER_USEC);
	u64 den = sample->running_us + alpha;

	if (sample->censored)
		return REFLEX_FLOOR_COUNT - 1;

	return div64_u64((REFLEX_FLOOR_COUNT - 1) * alpha + den / 2, den);
}

#endif /* _SCHED_CPUFREQ_REFLEX_H */
