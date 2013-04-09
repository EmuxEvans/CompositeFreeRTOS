/* For printing: */

#include <stdio.h>
#include <string.h>

static int 
prints(char *s)
{
	int len = strlen(s);
	cos_print(s, len);
	return len;
}

static int __attribute__((format(printf,1,2))) 
printc(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_print(s, ret);

	return ret;
}

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);} } while(0)
#endif

#ifdef BOOT_DEPS_H
#error "boot_deps.h should not be included more than once, or in anything other than boot."
#endif
#define BOOT_DEPS_H

#include <cos_component.h>
#include <res_spec.h>

struct llbooter_per_core {
	/* 
	 * alpha:        the initial thread context for the system
	 * init_thd:     the thread used for the initialization of the rest 
	 *               of the system
	 * recovery_thd: the thread to perform initialization/recovery
	 * prev_thd:     the thread requesting the initialization
	 * recover_spd:  the spd that will require rebooting
	 */
	int     alpha, init_thd, recovery_thd;
	int     sched_offset;      
	volatile int prev_thd, recover_spd;
} __attribute__((aligned(4096))); /* Avoid the copy-on-write issue for us. */

PERCPU(struct llbooter_per_core, llbooter);

/* enum { /\* hard-coded initialization schedule *\/ */
/* 	LLBOOT_SCHED = 2, */
/* 	LLBOOT_MM    = 3, */
/* }; */
enum { /* hard-coded initialization schedule */
	LLBOOT_FREERTOS = 2
};

struct comp_boot_info {
	int symbols_initialized, initialized, memory_granted;
};
//#define NCOMPS 6 	/* comp0, us, and the four other components */
#define NCOMPS 3 	/* comp0, us, and the four other components */
static struct comp_boot_info comp_boot_nfo[NCOMPS];

//static spdid_t init_schedule[]   = {LLBOOT_MM, LLBOOT_SCHED, 0};
static spdid_t init_schedule[]   = {LLBOOT_FREERTOS, 0};
//static int     init_mem_access[] = {1, 0, 0};
static int     init_mem_access[] = {1, 0};
static int     nmmgrs            = 0;
static int     frame_frontier    = 0; /* which physical frames have we used? */

typedef void (*crt_thd_fn_t)(void *);

/* Thread regs for checkpointing */
#include "fault_regs.h"
struct cos_regs thread_regs[256];

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"
/* synchronization... */
#define LOCK()   if (cos_sched_lock_take())    BUG();
#define UNLOCK() if (cos_sched_lock_release()) BUG();

/* scheduling/thread operations... */

/* We'll do all of our own initialization... */
static int
sched_create_thread_default(spdid_t spdid, u32_t v1, u32_t v2, u32_t v3)
{ return 0; }

static void
llboot_ret_thd(void) { return; }

/* 
 * When a created thread finishes, here we decide what to do with it.
 * If the system's initialization thread finishes, we know to reboot.
 * Otherwise, we know that recovery is complete, or should be done.
 */
static void
llboot_thd_done(void)
{
	int tid = cos_get_thd_id();

	assert(PERCPU_GET(llbooter)->alpha);
	/* 
	 * When the initial thread is done, then all we have to do is
	 * switch back to alpha who should reboot the system.
	 */
	if (tid == PERCPU_GET(llbooter)->init_thd) {
		int offset = PERCPU_GET(llbooter)->sched_offset;
		spdid_t s = init_schedule[offset];

		/* Is it done, or do we need to initialize another component? */
		if (s) {
			/* If we have a memory manger, give it a
			 * proportional amount of memory WRT to the
			 * other memory managers. */
			if (init_mem_access[offset] && cos_cpuid() == INIT_CORE) {
				int max_pfn, proportion;

				max_pfn = cos_pfn_cntl(COS_PFN_MAX_MEM, 0, 0, 0);
				proportion = (max_pfn - frame_frontier)/nmmgrs;
				cos_pfn_cntl(COS_PFN_GRANT, s, frame_frontier, proportion);
				comp_boot_nfo[s].memory_granted = 1;
			}
			PERCPU_GET(llbooter)->sched_offset++;
			comp_boot_nfo[s].initialized = 1;
			
			/* printc("core %ld: booter init_thd upcalling into spdid %d.\n", cos_cpuid(), (unsigned int)s); */
			cos_upcall(s); /* initialize the component! */
			BUG();
		}
		/* Done initializing; reboot!  If we are here, then
		 * all of the threads have terminated, thus there is
		 * no more execution left to do.  Technically, the
		 * other components should have called
		 * sched_exit... */
		printc("core %ld: booter init_thd switching back to alpha %d.\n", cos_cpuid(), PERCPU_GET(llbooter)->alpha);

		while (1) cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);
		BUG();
	}
	
	while (1) {
		int     pthd = PERCPU_GET(llbooter)->prev_thd;
		spdid_t rspd = PERCPU_GET(llbooter)->recover_spd;
				
		assert(tid == PERCPU_GET(llbooter)->recovery_thd);
		if (rspd) {             /* need to recover a component */
			assert(pthd);
			PERCPU_GET(llbooter)->recover_spd = 0;
			cos_upcall(rspd); /* This will escape from the loop */
			assert(0);
		} else {		/* ...done reinitializing...resume */
			assert(pthd && pthd != tid);
			PERCPU_GET(llbooter)->prev_thd = 0;   /* FIXME: atomic action required... */
			cos_switch_thread(pthd, 0);
		}
	}
}

void 
failure_notif_fail(spdid_t caller, spdid_t failed);

#include <checkpoint.h>

int have_checkpoint[NCOMPS];
int checkpoint_restore_fault_regs(spdid_t caller);

int 
fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	printc("PAGE FAULT CAUGHT at: %x\n", (unsigned int) ip);
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();

	//If we have a checkpoint for this component, use that.

	//check that the invfrm_ip is 0.
	if (have_checkpoint[spdid] == 1) {

		/* 
		 * THIS was the thing that fixed the infinite page
		 * fault bug.  Obviously, the subsequent code is a
		 * hack, but we just need to set the IP in the faulted
		 * component to zero, thanks to a bug in pop() 
		 * in a call to ipc_walk_static_cap.
		 */
		/* r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0); */
		/* printc("Invocation frame ip: %x\n", (unsigned int) r_ip); */
		/* if (r_ip != 0) { */
		/* 	assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, 0)); */
		/* } */
		
		int ret = checkpoint_restore_fault_regs(spdid);
		return 0;
	}

	/* failure_notif_fail(cos_spd_id(), spdid); */
	
	/* no reason to save register contents... */
	/* if(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0)) { */
	/* 	/\* Manipulate the return address of the component that called */
	/* 	 * the faulting component... *\/ */
	/* 	assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0)); */
	/* 	/\* ...and set it to its value -8, which is the fault handler */
	/* 	 * of the stub. *\/ */
	/* 	assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8)); */

	/* 	/\* switch to the recovery thread... *\/ */
	/* 	PERCPU_GET(llbooter)->recover_spd = spdid; */
	/* 	PERCPU_GET(llbooter)->prev_thd = cos_get_thd_id(); */
	/* 	cos_switch_thread(PERCPU_GET(llbooter)->recovery_thd, 0); */
	/* 	/\* after the recovery thread is done, it should switch back to us. *\/ */
	/* 	return 0; */
	/* } */
	/* /\* */
	/*  * The thread was created in the failed component...just use */
	/*  * it to restart the component!  This might even be the */
	/*  * initial thread. */
	/*  *\/ */
	/* cos_upcall(spdid); 	/\* FIXME: give back stack... *\/ */
	/* BUG(); */

	return 0;
}

/* memory operations... */

static vaddr_t init_hp = 0; 		/* initial heap pointer */
/* 
 * Assumptions about the memory management functions: 
 * - we only get single-page-increasing virtual addresses to map into.
 * - we never deallocate memory.
 * - we allocate memory contiguously
 * Many of these assumptions are ensured by the following code.
 * cos_get_vas_page should allocate vas contiguously, and incrementing
 * by a page, and the free function is made empty.
 */

/* 
 * Virtual address to frame calculation...assume the first address
 * passed in is the start of the heap, and they only increase by a
 * page from there.
 */
static inline int
__vpage2frame(vaddr_t addr) { return (addr - init_hp) / PAGE_SIZE; }

static vaddr_t
__mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), addr, frame_frontier++)) BUG();
	if (!init_hp) init_hp = addr;
	return addr;
}

static vaddr_t
__mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	int fp;

	assert(init_hp);
	fp = __vpage2frame(s_addr);
	assert(fp >= 0);
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, d_spd, d_addr, fp)) BUG();
	return d_addr;
}

static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static void
comp_info_record(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci) 
{ 
	if (!comp_boot_nfo[spdid].symbols_initialized) {
		comp_boot_nfo[spdid].symbols_initialized = 1;
		boot_spd_set_symbs(h, spdid, ci);
	}
}

static inline void boot_create_init_thds(void)
{
	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)PERCPU_GET(cos_sched_notifications))) BUG();

	PERCPU_GET(llbooter)->alpha        = cos_get_thd_id();
	PERCPU_GET(llbooter)->recovery_thd = cos_create_thread((int)llboot_ret_thd, (int)0, 0);
	assert(PERCPU_GET(llbooter)->recovery_thd >= 0);
	PERCPU_GET(llbooter)->init_thd     = cos_create_thread((int)llboot_ret_thd, 0, 0);
	printc("Core %ld, Low-level booter created threads:\n\t"
	       "%d: alpha\n\t%d: recov\n\t%d: init\n",
	       cos_cpuid(), PERCPU_GET(llbooter)->alpha, 
	       PERCPU_GET(llbooter)->recovery_thd, PERCPU_GET(llbooter)->init_thd);
	assert(PERCPU_GET(llbooter)->init_thd >= 0);
}

static void
boot_deps_init(void)
{
	int i;
	boot_create_init_thds();

	/* How many memory managers are there? */
	for (i = 0 ; init_schedule[i] ; i++) nmmgrs += init_mem_access[i];
	assert(nmmgrs > 0);
}

static void
boot_deps_run(void)
{
	assert(cos_cpuid() == INIT_CORE);
	assert(PERCPU_GET(llbooter)->init_thd);
	return; /* We return to comp0 and release other cores first. */
	//cos_switch_thread(per_core_llbooter[cos_cpuid()]->init_thd, 0);
}

static void
boot_deps_run_all(void)
{
	assert(PERCPU_GET(llbooter)->init_thd);
	cos_switch_thread(PERCPU_GET(llbooter)->init_thd, 0);
	return ;
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	printc("core %ld: <<cos_upcall_fn as %d (type %d, CREATE=%d, DESTROY=%d, FAULT=%d)>>\n",
	       cos_cpuid(), cos_get_thd_id(), t, COS_UPCALL_CREATE, COS_UPCALL_DESTROY, COS_UPCALL_UNHANDLED_FAULT);
	switch (t) {
	case COS_UPCALL_CREATE:
		llboot_ret_thd();
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_DESTROY:
		llboot_thd_done();
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		printc("Core %ld: Fault detected by the llboot component in thread %d: "
		       "Major system error.\n", cos_cpuid(), cos_get_thd_id());
	default:
		while (1) ;
		return;
	}

	return;
}

#include <sched_hier.h>

void cos_init(void);
int  sched_init(void)   
{ 
	if (cos_cpuid() == INIT_CORE) {
		/* The init core will call this function twice: first do
		 * the cos_init, then return to cos_loader and boot
		 * other cores, last call here again to run the init
		 * core. */
		if (!PERCPU_GET(llbooter)->init_thd) cos_init();
		else boot_deps_run_all();
	} else {
		LOCK();
		boot_create_init_thds();
		UNLOCK();
		boot_deps_run_all();
		/* printc("core %ld, alpha: exiting system.\n", cos_cpuid()); */
	}

	return 0; 
}

int  sched_isroot(void) { return 1; }
void 
sched_exit(void)
{
	printc("LLBooter: Core %ld called sched_exit. Switching back to alpha.\n", cos_cpuid());
	while (1) cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);	
}

int 
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) 
{ BUG(); return 0; }

int 
sched_child_cntl_thd(spdid_t spdid) 
{ 
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) {
		BUG(); 
		while(1);
	}
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();
	return 0;
}

/* 
 * fault_page_fault_handler() -> notification of the page fault; need
 * to restore here via memcpy(page_chkpt, page_start,
 * page_end-page_start) (these are in local_md)
 * this is already in boot_deps.h under llboot.
 */


void
checkpoint_thd_crt(void *d)
{
	int cur_spdid = (int)d;
	//	cur_spdid = 2;
	printc("Attempting to start thread in spd %d...\n", cur_spdid);
	if (cos_upcall(cur_spdid)) prints("crboot: error making upcall into spd.\n");
	BUG();
}

int 
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) 
{ 
	/*
	 * - Create thread.
	 * - enter thread in data structure to be checkpointed
	 * - return control of thread to freeRTOS.
	 * - have a feeling the answer is in cos_sched_base.c under same function.
	 * - cos_list.h provides a linked-list implementation that looks like it should work
	 *    -- ADD_LIST and REM_LIST are both implemented, wonderful.
	 * - going to need a thread list for every component.
	 * - when a component checkpoints, store those checkpoints somewhere.
	 *
	 *
	 * Question: how should freeRTOS inform us of that thread's termination?
	 * Question: do freeRTOS threads ever NEED to terminate?
	 * Question: if they do, we should probably restore them, no?
	 * Question: timer thread. oh shit.
	 * Question: how do we deal with checkpoints taken after threads have terminated?
	 *   - ideally there's a serialized data structure that can be saved and restored.
	 *   - how to do this generally?
	 */
	int tid;

	LOCK();
	printc("Got a call to create a child thread, cur_spd: %d, dest_spd: %d\n", spdid, dest_spd);
	tid = cos_create_thread((int) checkpoint_thd_crt, (int)spdid, 0);

	//cos_regs_save(tid, spdid, NULL, &thread_regs[tid]);
	thread_regs[tid].regs.ip = 0;
	thread_regs[tid].regs.sp = 0;
	thread_regs[tid].regs.bp = 0;
	thread_regs[tid].regs.ax = 0;
	thread_regs[tid].regs.bx = 0;
	thread_regs[tid].regs.cx = 0;
	thread_regs[tid].regs.dx = 0;
	thread_regs[tid].regs.di = 0;
	thread_regs[tid].regs.si = 0;
	thread_regs[tid].spdid = spdid;
	thread_regs[tid].tid = tid;

	assert(tid >= 0);
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, tid, spdid)) BUG();
	
	// add it to some sort of DS here.
	UNLOCK();
	
	return tid;
}

int
checkpoint_checkpt(spdid_t caller) 
{
	int i, cur_thd;
	struct spd_local_md *md;
	//	printc("checkpoint called, copying memory!\n");
	LOCK();
	md = &local_md[caller];
	assert(md);
	//	printc("about to memcpy 0x%x bytes from 0x%x to 0x%x\n", md->page_end - md->page_start, md->page_start, md->checkpt_region_start);
	memcpy(md->checkpt_region_start, md->page_start, (md->page_end - md->page_start));

	cur_thd = cos_get_thd_id();
	//	printc("Current thread: %d\n", cur_thd);

	/* 
	 * Checkpoint the threads that belong to this spdid. 
	 * Right now we assume that threads don't die, 
	 * and that there will only ever be 256 of them.
	 * These are Bad Assumptions.
	 * Each spd should only access threads that really belong to it,
	 * so there's a security hole here as well. Hooray.
	 */
	for (i = 0; i < 256; i++) {
		if (thread_regs[i].spdid == caller) {
			//			printc("Saving thread %d\n", i);
			if (i == cur_thd) {
				//				printc("Thread %d is the current thread\n", i);
				thread_regs[i].regs.ip = cos_thd_cntl(COS_THD_INVFRM_IP, i, 1, 0);
				thread_regs[i].regs.sp = cos_thd_cntl(COS_THD_INVFRM_SP, i, 1, 0);
				thread_regs[i].regs.ax = (long) 1;
				thread_regs[i].regs.cx = (long) 1;
				thread_regs[i].regs.dx = (long) 1;
			} else { 
				cos_regs_save(i, caller, NULL, &thread_regs[i]);
			}
			//			cos_regs_print(&thread_regs[i]);
		}
	}

	/* /\* doing this because I seem to miss the init thd in the */
	/*  * checkpoint, because it's created by the llbooter. seeing if this gets */
	/*  * around my current problem  */
	/*  *\/ */
	/* i = cur_thd; */
	/* printc("Saving thread %d\n", i); */
	/* printc("Thread %d is the current thread\n", i); */
	/* thread_regs[i].regs.ip = cos_thd_cntl(COS_THD_INVFRM_IP, i, 1, 0); */
	/* thread_regs[i].regs.sp = cos_thd_cntl(COS_THD_INVFRM_SP, i, 1, 0); */
	/* thread_regs[i].regs.ax = (long) 1; */
	/* thread_regs[i].regs.cx = (long) 1; */
	/* thread_regs[i].regs.dx = (long) 1; */
	
	
	have_checkpoint[caller] = 1;
	UNLOCK();
	return 0;
}

static inline int 
checkpoint_restore_helper(spdid_t caller, int use_fault_regs) 
{
	//	printc("restore checkpoint called... restoring... from thread %d\n", cos_get_thd_id());
	struct spd_local_md *md;
	int i, thd_id;

	LOCK();
	md = &local_md[caller];
	assert(md);
	memcpy(md->page_start, md->checkpt_region_start, md->page_end - md->page_start);

	for (i = 0; i < 256; i++) {
		if (thread_regs[i].spdid == caller) {
			//			printc("Restoring thread %d\n", i);
			cos_regs_restore(&thread_regs[i]);
			//			cos_regs_print(&thread_regs[i]);
		}
	}

	// properly restore the faulting thread'
	if (use_fault_regs) {
		thd_id = cos_get_thd_id();
		//		printc("Resetting fault regs of thread %d\n", thd_id);
		cos_regs_fault_restore(&thread_regs[thd_id]);
		//		cos_regs_print(&thread_regs[thd_id]);
		//		printc("\n");
	}
	
	//	printc("Done restoring...\n");
	UNLOCK();
	return 1;
	
}

static int faulted = 0;
int
checkpoint_test_should_fault_again() {
	if (faulted) return 0;
	faulted = 1;
	return 1;
}

static u64_t total_restore_time = 0;
static u64_t restore_samples = 0;

int
checkpoint_restore(spdid_t caller) 
{
	u64_t start, end;
	int ret;
	start = end = 0;

	rdtscll(start);
	ret = checkpoint_restore_helper(caller, 0);
	rdtscll(end);
	
	if (end > start) {
		restore_samples++;
		total_restore_time += (end - start);
		printc("Restore time (cycles): %llu\n", (end - start));
		printc("Average Restore time (cycles): %llu\n", (total_restore_time / restore_samples));
	}

	return ret;
}

int
checkpoint_restore_fault_regs(spdid_t caller) 
{
	//	printc("Restoring to FAULT regs\n");
	return checkpoint_restore_helper(caller, 1);
}
