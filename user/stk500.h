extern int stk_stage;
extern int stk_error;
extern int stk_tick;
extern const char *stk_error_descr;
extern char stk_major, stk_minor, stk_signature[3];

void init_stk500();
void program(int size, int pos_start);
