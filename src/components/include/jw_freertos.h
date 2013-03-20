#include <cos_types.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

extern void jw_lock();
extern void jw_unlock();

extern int jw_create_thread(int a, int b, int c);
extern int jw_switch_thread(int a, int b);
extern int jw_get_thread_id(void);
extern int __attribute__((format(printf,1,2))) jw_print(char *fmt, ...);
extern int jw_brand_cntl(int a, int b, int c, int d);
extern int jw_brand_wire(int a, int b, int c);
extern long jw_spd_id(void);
extern int create_timer(int timer_init);
extern void freertos_clear_pending_events(void);
