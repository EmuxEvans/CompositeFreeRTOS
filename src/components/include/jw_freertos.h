void jw_lock();
void jw_unlock();

int jw_create_thread(int a, int b, int c);
int jw_switch_thread(int a, int b);
int jw_get_thread_id(void);
int __attribute__((format(printf,1,2))) jw_print(char *fmt, ...);

