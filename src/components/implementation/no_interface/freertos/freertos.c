#include <string.h>
#include <cos_component.h>
#include <jw_freertos.h>
#include <bitmap.h>


#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"

int
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
}


//#define FREERTOS_MAX_MEMORY 900

typedef void (*crt_thd_fn_t)(void *);

extern int freeRTOS_entry( void );

void jw_lock() {
	if (cos_sched_lock_take()) {
		BUG();
	}
}

void jw_unlock() {
	if (cos_sched_lock_release()) {
		BUG();
	}
}

int jw_create_thread(int a, int b, int c) {
	return cos_create_thread(a, b, c);
}

int jw_switch_thread(int a, int b) {
	return cos_switch_thread(a, b);
}

void print(char *str) {
	cos_print(str, strlen(str));
}

static void freertos_ret_thd() { return; }

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3) {
	switch (t) {
	case COS_UPCALL_CREATE:
		freertos_ret_thd();
		print("Calling function to start thread....\n");
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_DESTROY:
		print("Destroy upcall in freertos cos_upcall_fn\n");
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

void cos_init(void);
int sched_init(void) {
	print("sched_init called from freertos component\n");
	(void) cos_init();
	return 0;
}


void cos_init(void)
{
	vaddr_t *memory, test;
	int i;
	test = "Hello, World from component!\n";
	print(test);
	
	freeRTOS_entry();
	
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
