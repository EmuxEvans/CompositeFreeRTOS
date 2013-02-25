#include <cos_types.h>

void jw_lock();
void jw_unlock();

int jw_create_thread(int a, int b, int c);
int jw_switch_thread(int a, int b);
int jw_get_thread_id(void);
int __attribute__((format(printf,1,2))) jw_print(char *fmt, ...);
int jw_brand_cntl(int a, int b, int c, int d);
int jw_brand_wire(int a, int b, int c);
long jw_spd_id(void);

