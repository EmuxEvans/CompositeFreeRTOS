#include <cos_types.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

extern void freertos_lock();
extern void freertos_unlock();

extern int freertos_create_thread(int a, int b, int c);
extern int freertos_switch_thread(int a, int b);
extern int freertos_get_thread_id(void);
extern int __attribute__((format(printf,1,2))) freertos_print(char *fmt, ...);
extern int freertos_brand_cntl(int a, int b, int c, int d);
extern int freertos_brand_wire(int a, int b, int c);
extern long freertos_spd_id(void);
extern int create_timer(int timer_init);
extern void freertos_clear_pending_events(void);
extern int freertos_checkpoint(void);
extern void freertos_restore_checkpoint(void);

extern void *prvWaitForStart( void *pvParams);
