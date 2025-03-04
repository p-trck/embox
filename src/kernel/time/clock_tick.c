/**
 * @file
 * @brief Link layer between timers and clock sources.
 *
 * @date 09.07.2012
 * @author Alexander Kalmuk
 */

#include <kernel/irq_lock.h>
#include <kernel/time/timer.h>
#include <kernel/time/clock_source.h>

#include <kernel/sched/schedee_priority.h>
#include <kernel/lthread/lthread.h>

#include <hal/clock.h>
#include <hal/cpu.h>

#include <framework/mod/options.h>

#define CLOCK_HND_PRIORITY OPTION_GET(NUMBER, hnd_priority)

static struct lthread clock_handler_lt;
/* from jiffies.c */
extern struct clock_source *cs_jiffies;

void clock_tick_handler(void *dev_id) {
	if (dev_id == cs_jiffies) {
		jiffies_update(1);
	} else {
		clock_handle_ticks(dev_id, 1);
	}
}

void jiffies_update(int ticks) {
	clock_t next_event;
#if defined(SMP) && (OPTION_MODULE_GET(embox__arch__smp, BOOLEAN, independent_clock) == true)
	static clock_t ticks_for_multi_clock;
	if(ticks_for_multi_clock++ % NCPU)
#endif
		cs_jiffies->event_device->jiffies += ticks;

	if ((timer_strat_get_next_event(&next_event) == 0) &&
			(cs_jiffies->event_device->jiffies >= next_event)) {
		lthread_launch(&clock_handler_lt);
	}
}

void clock_handle_ticks(void *dev_id, unsigned ticks) {
	clock_t next_event;
	struct clock_source *cs = (struct clock_source *) dev_id;

	assert(cs);

	cs->event_device->jiffies += ticks;

	if ((timer_strat_get_next_event(&next_event) == 0) &&
			(cs_jiffies->event_device->jiffies >= next_event)) {
		lthread_launch(&clock_handler_lt);
	}
}

static int clock_handler(struct lthread *self) {
	timer_strat_sched(cs_jiffies->event_device->jiffies);
	return 0;
}

int clock_tick_init(void) {
	lthread_init(&clock_handler_lt, &clock_handler);
	schedee_priority_set(&clock_handler_lt.schedee, CLOCK_HND_PRIORITY);

	return 0;
}
