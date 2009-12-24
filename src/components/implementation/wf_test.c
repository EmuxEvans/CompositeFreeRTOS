#include <cos_component.h>
#include <cos_debug.h>
#include <cos_synchronization.h>
#include <print.h>

/* extern int lock_component_take(spdid_t spdid); */
/* extern int lock_component_release(spdid_t spdid); */
/* extern unsigned long lock_component_alloc(spdid_t spdid); */
/* extern void lock_component_free(spdid_t spdid, unsigned long lock_id); */

#define ITER 1000000

extern int sched_create_thread(spdid_t);
extern int timed_event_block(spdid_t spdinv, unsigned int amnt);

#define LOCK_OWNER_TIMEOUT 20000
static int timeouts[] = {1, 50000, 10000, 20000, 1, 40000, 30000, 10000};
#define TIMEOUT_MASK (8-1);
static int timeout_ptr = 0;

cos_lock_t lock;
volatile int blah, val = 0;

void begin_work(int incr)
{
	while (1) {
		int i, time = timeout_ptr, ret;

		timeout_ptr = (timeout_ptr+1) & TIMEOUT_MASK;
		ret = lock_take_timed(&lock, timeouts[time]);
		
		assert(ret != -1);
		if (ret != TIMER_EXPIRED) {
			assert(0); /* express timeout in ticks */
			timed_event_block(cos_spd_id(), LOCK_OWNER_TIMEOUT);
			lock_release(&lock);
			continue;
		} 

		for (i = 0 ; i < ITER ; i++) {
			blah += incr;
		}
	}
}

void begin_owner(void)
{
	static volatile int first = 1;

	if (first == 1) {
		lock_static_init(&lock);
		first = 0;
	}
	assert(lock.lock_id != 0);
	
	begin_work(1);
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		begin_owner();
		break;
	default:
		print("wf_text: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}

	return;
}

void bin(void) {
	assert(0);
	/* this doesn't work... */
	sched_create_thread(cos_spd_id());
}