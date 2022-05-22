int init_timer(void); // returns 0 if all good.
void close_timer(void);
void start_fast_timer(void);
double stop_fast_timer(void);
void set_keyboard_repeat_rate(unsigned long seconds);
int init_keyboard(void);
void start_normal_timer(void);
long stop_normal_timer(void);
