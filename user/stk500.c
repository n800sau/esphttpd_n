#include "stk500.h"
#include <osapi.h>
#include "user_interface.h"
#include "espmissingincludes.h"
#include "driver/uart.h"
#include "mem.h"


static ETSTimer delayTimer;
static int buflen = 0;
static char *bufptr = NULL;

static void ICACHE_FLASH_ATTR runProgrammer(void *arg)
{
	int i;
	static int tick=0;
	os_printf("Tick %d\n", tick);
	os_printf("syncing\n");
	for(i=0; i<5; i++) {
		uart_tx_one_char(0x30);
		uart_tx_one_char(0x20);
		os_delay_us(50000);
	}
	os_printf("receiving sync ack\n");
	
	if(++tick > 10) {
		os_timer_disarm(&delayTimer);
	}
}

void program(int size, char *buf)
{
	os_timer_disarm(&delayTimer);
	if(bufptr != NULL) {
		// clean old buffer
		os_free(bufptr);
	}
	bufptr = buf;
	buflen = size;
	os_timer_setfn(&delayTimer, runProgrammer, NULL);
	os_timer_arm(&delayTimer, 2000, 1);
}
