#include "cmucam4.h"
#include "driver/uart.h"
#include <osapi.h>

static void ICACHE_FLASH_ATTR cmucam4_stop_and_flush()
{
	uart0_tx_buffer("\r", 1);
	uart0_clean_chars();
}

int ICACHE_FLASH_ATTR cmucam4_send(const char *cmd)
{
	int is_prim = is_uart0_primary();
	uart0_secondary();
	cmucam4_stop_and_flush();
	uart0_tx_buffer((uint8_t*)cmd, strlen(cmd));
	uart0_tx_one_char('\r');
	char c = 0;
	os_printf("%s sent\n", cmd);
	do {
		if(uart0_count_chars() > 0) {
			c = uart0_get_char();
			os_printf("c=%c\n", c);
		}
		wdt_feed();
		os_delay_us(5000);
	} while(c != '\r');
	if(is_prim) {
		uart0_primary();
	}
	return -1;
}

int ICACHE_FLASH_ATTR cmucam4_color(uint8_t buf[], int bufsize)
{
	int rs = -1;
	int is_prim = is_uart0_primary();
	uart0_secondary();

	if(is_prim) {
		uart0_primary();
	}
	return rs;
}

int ICACHE_FLASH_ATTR cmucam4_bw(uint8_t buf[], int bufsize)
{
	int rs = -1;
	int is_prim = is_uart0_primary();
	uart0_secondary();

	if(is_prim) {
		uart0_primary();
	}
	return rs;
}

int ICACHE_FLASH_ATTR cmucam4_tc(uint8_t buf[], int bufsize)
{
	int rs = -1;
	int is_prim = is_uart0_primary();
	uart0_secondary();

	if(is_prim) {
		uart0_primary();
	}
	return rs;
}

int ICACHE_FLASH_ATTR cmucam4_tw(uint8_t buf[], int bufsize)
{
	int rs = -1;
	int is_prim = is_uart0_primary();
	uart0_secondary();

	if(is_prim) {
		uart0_primary();
	}
	return rs;
}


