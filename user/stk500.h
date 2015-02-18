extern int stk_stage;
extern int stk_error;
extern int stk_tick;
extern const char *stk_error_descr;

void init_stk500();
void program(int size, char *buf);
