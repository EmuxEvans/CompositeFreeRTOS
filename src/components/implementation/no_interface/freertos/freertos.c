#include <string.h>
#include <cos_component.h>
#include <jw_freertos.h>

#include <bitmap.h>
#include <cos_debug.h>
#include <stdio.h>
#include <checkpoint.h>

#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"

typedef void (*crt_thd_fn_t)(void *);

static volatile int init_thd = -1;

#define ARG_STRLEN 512

int prints(char *str) {
	return freertos_print(str);
}

int __attribute__((format(printf,1,2)))
freertos_print(const char *fmt, ...)
{
        char s[ARG_STRLEN];
        va_list arg_ptr;
        int ret;

        va_start(arg_ptr, fmt);
        ret = vsnprintf(s, ARG_STRLEN-1, fmt, arg_ptr);
        va_end(arg_ptr);
        s[ARG_STRLEN-1] = '\0';
        cos_print(s, ret);

        return ret;
}

void freertos_lock() {
	//	freertos_print("taking lock with freertos_lock\n");
	if (cos_sched_lock_take()) {
		freertos_print("Couldn't take lock!\n\n");
		BUG();
	}
}

void freertos_unlock() {
	//	freertos_print("Releasing lock!\n");
	if (cos_sched_lock_release()) {
		BUG();
	}
}

extern int parent_sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd);
int freertos_create_thread(int a, int b, int c) {
	freertos_print("Creating thread in spd %d, current thd %d\n", (int) cos_spd_id(), cos_get_thd_id());
	//	return cos_create_thread(a, b, c);
	return parent_sched_child_thd_crt(cos_spd_id(), cos_spd_id());
}

int freertos_switch_thread(int a, int b) {
	//this is a hack. my apologies. should also lock here. 
	freertos_clear_pending_events();

	//	freertos_print("Switching threads from %d to %d\n", cos_get_thd_id(), a);
	return cos_switch_thread(a, b);
	//	freertos_print("Switched threads from %d to %d\n", cos_get_thd_id(), a);
}

int freertos_get_thread_id(void) {
	return cos_get_thd_id();
}

int freertos_brand_cntl(int a, int b, int c, int d) {
	return cos_brand_cntl(a, b, c, d);
}

int freertos_brand_wire(int a, int b, int c) {
	return cos_brand_wire(a, b, c);
}

long freertos_spd_id(void) {
	return cos_spd_id();
}

int freertos_checkpoint(void) {
	return checkpoint_checkpt(cos_spd_id());
}

void freertos_restore_checkpoint(void) {
	checkpoint_restore(cos_spd_id());
}

void freertos_clear_pending_events(void) {
	if (PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event) {
		PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event = 0;

	}
}

void freertos_sched_set_evt_urgency(u8_t evt_id, u16_t urgency)
{
	struct cos_sched_events *evt;
	u32_t old, new;
	u32_t *ptr;
	//struct cos_se_values *se;

	assert(evt_id < NUM_SCHED_EVTS);

	evt = &PERCPU_GET(cos_sched_notifications)->cos_events[evt_id];
	ptr = &COS_SCHED_EVT_VALS(evt);

	/* Need to do this atomically with cmpxchg as next and flags
	 * are in the same word as the urgency.
	 */
	while (1) {
		old = *ptr;
		new = old;
		/* 
		 * FIXME: Seems as though GCC cannot handle this with
		 * -O2; not picking up the alias for some odd reason:
		 *
		 * se = (struct cos_se_values*)&new;
		 * se->urgency = urgency;
		 */
		new &= 0xFFFF;
		new |= urgency<<16;
		
		if (cos_cmpxchg(ptr, (long)old, (long)new) == (long)new) break;
	}

	return;
}



int create_timer(int timer_init_fn)
{
	freertos_print("Creating timer\n");
	int bid, ret, timer_thread;


	bid = freertos_brand_cntl(COS_BRAND_CREATE_HW, 0, 0, freertos_spd_id());
	
	timer_thread = freertos_create_thread((int)timer_init_fn, (int)bid, 0);

	/* if (NULL == PERCPU_GET(sched_base_state)->timer) BUG(); */
	/* if (0 > sched_add_thd_to_brand(cos_spd_id(), bid, PERCPU_GET(sched_base_state)->timer->id)) BUG(); */

	if (freertos_brand_cntl(COS_BRAND_ADD_THD, bid, timer_thread, 0) < 0) {
		freertos_print("ERROR ADDING THREAD TO BRAND\n");
		while(1);
	} else {
		freertos_print("added thread to brand\n");
	}

	freertos_print("Timer thread has id %d with priority %s.\n", timer_thread, "t");
	freertos_brand_wire(bid, COS_HW_TIMER, 0);

	int i;
	for (i = 1; i < NUM_SCHED_EVTS; i++) {
		struct cos_sched_events *se;
		se = &PERCPU_GET(cos_sched_notifications)->cos_events[i];

		if (COS_SCHED_EVT_FLAGS(se) & COS_SCHED_EVT_FREE) {
			COS_SCHED_EVT_FLAGS(se) &= ~COS_SCHED_EVT_FREE;

			if (cos_sched_cntl(COS_SCHED_THD_EVT, timer_thread, i)) {
				freertos_print("Mapping thread to event failed.\n");
				COS_SCHED_EVT_FLAGS(se) |= COS_SCHED_EVT_FREE;
				while (1);
				return -1;
			} 
			freertos_print("Found a free event slot: %d\n", i);
			freertos_sched_set_evt_urgency(i, 1);
			break;
		}
	}

	if (i >= NUM_SCHED_EVTS) {
		freertos_print("Found no available event slots...\n");
	}

	return timer_thread;
}

void print(char *str) {
	cos_print(str, strlen(str));
}

static void freertos_ret_thd(void) { 
	return; 
}

extern void *prvWaitForStart( void *pvParams);
extern void timer_tick (void);
int sched_init(void);
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3) {
	switch (t) {
	case COS_UPCALL_CREATE:
		//freertos_ret_thd();
		freertos_print("Calling function to start thread....\n");
		((crt_thd_fn_t)arg1)(arg2);
		freertos_print("Thread terminated %d\n", (int) freertos_get_thread_id());
		break;
	case COS_UPCALL_DESTROY:
		while(1) freertos_switch_thread(init_thd, 0);
		BUG();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		print("Unhandled fault in freertos component.\n");
		break;
	case COS_UPCALL_BRAND_EXEC:
		print("Brand exec upcall in freertos component\n");
		timer_tick();
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		print("Brand complete upcall in freertos component\n");
		break;
	case COS_UPCALL_BOOTSTRAP:
		print("Bootstrap upcall in freertos component.\n");
		if (init_thd > 0) {
			prvWaitForStart(NULL);
			return;
		} else { 
			freertos_print("Init thd: %d\n", init_thd);
		}
		sched_init();
		break;
	default:
		print("Default cos_upcall... Que?\n");
		return;
	}
	return;
}

#include <sched_hier.h>

void cos_init(void);
extern int parent_sched_child_cntl_thd(spdid_t spdid);

PERCPU_ATTR(static volatile, int, initialized_core); /* record the cores that still depend on us */

void
sched_exit(void)
{
	return;
}

int sched_isroot(void) { return 0; }

int
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) { BUG(); return 0; }

int sched_init(void) {
	print("sched_init called from freertos component\n");
	init_thd = cos_get_thd_id();
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)PERCPU_GET(cos_sched_notifications))) BUG();
	
	int i;
	for (i = 0 ; i < NUM_SCHED_EVTS ; i++) {
		struct cos_sched_events *se;

		se = &PERCPU_GET(cos_sched_notifications)->cos_events[i];
		if (i == 0) {
			COS_SCHED_EVT_FLAGS(se) = 0;
		} else {
			COS_SCHED_EVT_FLAGS(se) = COS_SCHED_EVT_FREE;
		}
		COS_SCHED_EVT_NEXT(se) = 0;

	}

	(void) cos_init();
	return 0;
}


int
sched_child_cntl_thd(spdid_t spdid)
{
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) BUG();
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();

	return 0;
}

int
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }

extern int freeRTOS_entry( void );
void cos_init(void)
{
	freeRTOS_entry();
	
	while(1);
	
	return;
}
