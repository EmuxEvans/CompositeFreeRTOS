#include <string.h>
#include <cos_component.h>
#include <jw_freertos.h>
#include <bitmap.h>
#include <cos_debug.h>
#include <stdio.h>

#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"

typedef void (*crt_thd_fn_t)(void *);

volatile int init_thd;

#define ARG_STRLEN 512

int prints(char *str) {
	return jw_print(str);
}

int __attribute__((format(printf,1,2)))
jw_print(char *fmt, ...)
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

void jw_lock() {
	jw_print("taking lock with jw_lock\n");
	if (cos_sched_lock_take()) {
		jw_print("Couldn't take lock!\n\n");
		BUG();
	}
}

void jw_unlock() {
	jw_print("Releasing lock!\n");
	if (cos_sched_lock_release()) {
		BUG();
	}
}

int jw_create_thread(int a, int b, int c) {
	jw_print("Creating thread, current thd %d\n", cos_get_thd_id());
	return cos_create_thread(a, b, c);
}

int jw_switch_thread(int a, int b) {
	jw_print("Switching threads from %d to %d\n", cos_get_thd_id(), a);
	return cos_switch_thread(a, b);
}

int jw_get_thread_id(void) {
	return cos_get_thd_id();
}

void print(char *str) {
	cos_print(str, strlen(str));
}

static void freertos_ret_thd() { 
	//	jw_print("freertos_ret_thd\n");
	return; 
}

int sched_init(void);
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3) {
	switch (t) {
	case COS_UPCALL_CREATE:
		//freertos_ret_thd();
		jw_print("Calling function to start thread....\n");
		((crt_thd_fn_t)arg1)(arg2);
		jw_print("Thread terminated %d\n", jw_get_thread_id);
		break;
	case COS_UPCALL_DESTROY:
		while(1) jw_switch_thread(init_thd, 0);
		BUG();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		print("Unhandled fault in freertos component.\n");
	case COS_UPCALL_BRAND_EXEC:
		print("Brand exec upcall in freertos component\n");
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		print("Brand complete upcall in freertos component\n");
		break;
	case COS_UPCALL_BOOTSTRAP:
		print("Bootstrap upcall in freertos component.\n");
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
	vaddr_t *memory, test;
	int i, j, k;
	test = "Hello, World from component!\n";
	print(test);
	
	j = 2;
	k = 3;

	jw_print("j: %d, k: %d, k_addr: %x\n", j, k, (unsigned int) &k);
	
	freeRTOS_entry();
	
	while(1);
	
	/* memory = NULL; */
	/* for (i = 0; i < FREERTOS_MAX_MEMORY; i++) { */
	/* 	print("Allocating page... \n"); */
	/* 	vaddr_t page = cos_get_vas_page(); */
	/* 	if (!memory) memory = page; */

	/* 	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), page, i)) { */
	/* 		print("Error allocating memory\n"); */
	/* 		return; */
	/* 	} */
	/* } */
	return;
}
