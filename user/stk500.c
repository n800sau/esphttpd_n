#include "stk500.h"
#include <osapi.h>
//#include <stdio.h>
#include <stdlib.h>
#include "user_interface.h"
#include "espmissingincludes.h"
#include "driver/uart.h"
#include <mem.h>

#define UART_BUF_SIZE 100
static char uart_buf[UART_BUF_SIZE];
static int buf_pos = 0;

static void add_char(char c)
{
	uart_buf[buf_pos] = c;
	buf_pos++;
	if(buf_pos >= sizeof(uart_buf)) {
		buf_pos = sizeof(uart_buf) - 1;
		// forget the oldest
		memmove(uart_buf, uart_buf+1, sizeof(uart_buf) - 1);
	}
}

static char get_char()
{
	char rs = (buf_pos > 0) ? uart_buf[0] : -1;
	memmove(uart_buf, uart_buf+1, sizeof(uart_buf) - 1);
	return rs;
}

static int count_chars()
{
	return buf_pos;
}

os_event_t recvCharTaskQueue[recvCharTaskQueueLen];

static void ICACHE_FLASH_ATTR recvCharTaskCb(os_event_t *events)
{
	uint8_t temp;

	//add transparent determine
	while(READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
	{
		WRITE_PERI_REG(0X60000914, 0x73); //WTD

		temp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
		add_char(temp);
		os_printf("char=%2X\n", temp);

	}
	if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
	} else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
	}
	ETS_UART_INTR_ENABLE();
}


static ETSTimer delayTimer;
static int buflen = 0;
static char *bufptr = NULL;

static int progState = 0;
static int error = 0;

static int in_sync(char fb, char lb)
{
	int rs = fb == 0x14 && lb == 0x10;
	if(rs) {
		os_printf("in sync\n");
	} else {
		os_printf("OUT of sync\n");
	}
	return rs;
}

static void ICACHE_FLASH_ATTR runProgrammer(void *arg)
{
	int i, j, vals[17*8], size;
	static int address = 0;
	char ok, insync, laddress, haddress, *bp;
	static char major, minor, signature[3];
	os_printf("tick\n");
	switch(progState) {
		case 0:
			os_printf("syncing\n");
			for(i=0; i<5; i++) {
				uart_tx_one_char(0x30);
				uart_tx_one_char(0x20);
			}
			progState = 1;
		case 1:
			if(count_chars() >= 2) {
				insync = get_char();
				ok = get_char();
				if(in_sync(insync, ok)) {
					uart_tx_one_char(0x41);
					uart_tx_one_char(0x81);
					uart_tx_one_char(0x20);
					os_printf("receiving MAJOR version\n");
					progState = 2;
				} else {
					error = 1;
				}
			}
			break;
		case 2:
			if(count_chars() >= 3) {
				insync = get_char(); // STK_INSYNC
				major = get_char(); // STK_SW_MJAOR
				ok = get_char(); // STK_OK
				if(in_sync(insync, ok)) {
					uart_tx_one_char(0x41);
					uart_tx_one_char(0x82);
					uart_tx_one_char(0x20);
					os_printf("receiving MINOR version\n");
					progState = 3;
				} else {
					error = 1;
				}
			}
			break;
		case 3:
			if(count_chars() >= 3) {
				insync = get_char(); // STK_INSYNC
				minor = get_char(); // STK_SW_MJAOR
				ok = get_char(); // STK_OK
				if(in_sync(insync, ok)) {
					os_printf("bootloader version %d.%d", major, minor);
					os_printf("entering prog mode\n");
					uart_tx_one_char(0x50);
					uart_tx_one_char(0x20);
					os_printf("receiving sync ack\n");
					progState = 4;
				} else {
					error = 1;
				}
			}
			break;
		case 4:
			if(count_chars() >= 2) {
				insync = get_char();
				ok = get_char();
				if(in_sync(insync, ok)) {
					uart_tx_one_char(0x75);
					uart_tx_one_char(0x20);
					os_printf("receiving signature\n");
					progState = 5;
				} else {
					error = 1;
				}
			}
			break;
		case 5:
			if(count_chars() >= 3) {
				insync = get_char(); // STK_INSYNC
				signature[0] = get_char();
				signature[1] = get_char();
				signature[2] = get_char();
				ok = get_char(); // STK_OK
				if(in_sync(insync, ok)) {
					os_printf("signature %d-%d-%d", signature[0], signature[1], signature[2]);
					address = 0;
					progState = 6;
				} else {
					error = 1;
				}
			}
			break;
		case 6:
			laddress = address & 0xFF;
			haddress = address >> 8 & 0xff;
			address += 64;
			os_printf("set page addr\n");
			uart_tx_one_char(0x55); //STK_LOAD_ADDRESS
			uart_tx_one_char(laddress);
			uart_tx_one_char(haddress);
			uart_tx_one_char(0x20); // SYNC_CRC_EOP
			progState = 7;
			break;
		case 7:
			if(count_chars() >= 2) {
				insync = get_char();
				ok = get_char();
				if(in_sync(insync, ok)) {
					os_printf("sending program page");
					memset(vals, 0, sizeof(vals));
					size = 0;
					for(i=0; i<8; i++) {
						// skip till ':'
						while(address < buflen) {
							if(bufptr[++address] == ':') {
								break;
							}
						}
						address += 8;
						for(i=0; i<sizeof(vals); i++) {
							address += i*2;
							if(address >= buflen) {
								break;
							}
							bp = bufptr + address;
							if(*bp == '\x0a' || *bp == '\x0d') {
								break;
							}
							vals[i] = (int)strtol(bp, NULL, 16);
//							sscanf(bp, "%2X", vals+i);
						}
						for(j=0; j<i-1; j++) {
							size += i-1;
						}
					}
					uart_tx_one_char(0x64); // STK_PROGRAM_PAGE
					uart_tx_one_char(0); // page size
					uart_tx_one_char(size); // page size
					uart_tx_one_char(0x46); // flash memory, 'F'
					for(j=0; j<size; j++) {
						uart_tx_one_char(vals[j]);
					}
					uart_tx_one_char(0x20); // SYNC_CRC_EOP
					if(address >= buflen) {
						progState = 8;
					}
				} else {
					error = 1;
				}
			}
			break;
		case 8:
			if(count_chars() >= 2) {
				insync = get_char();
				ok = get_char();
				if(in_sync(insync, ok)) {
					os_printf("leaving prog mode\n");
					uart_tx_one_char(0x51);
					uart_tx_one_char(0x20);
					progState = 9;
				} else {
					error = 1;
				}
			}
			break;
		case 9:
			if(count_chars() >= 2) {
				insync = get_char();
				ok = get_char();
				if(in_sync(insync, ok)) {
					os_printf("end\n");
					os_timer_disarm(&delayTimer);
					os_free(bufptr);
					bufptr = NULL;
					buflen = 0;
				} else {
					error = 1;
				}
			}
			break;
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
	progState = 0;
	error = 0;
	os_timer_setfn(&delayTimer, runProgrammer, NULL);
	os_timer_arm(&delayTimer, 2000, 1);
}

void init_stk500()
{
	system_os_task(recvCharTaskCb, recvCharTaskPrio, recvCharTaskQueue, recvCharTaskQueueLen);
}