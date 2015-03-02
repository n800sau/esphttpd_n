#include "stk500.h"
#include <osapi.h>
#include <stdlib.h>
#include "user_interface.h"
#include "espmissingincludes.h"
#include "driver/uart.h"
#include <mem.h>
#include <gpio.h>
#include "mmem.h"

#define TICK_TIME 100
#define TICK_TIMEOUT 10000
#define TICK_MAX (TICK_TIMEOUT / TICK_TIME)
#define SYNC_PAUSE 500
#define SYNC_STEP (SYNC_PAUSE / TICK_TIME)
#define STK500_LOCK 1677

// gpio 4, 5, 12, 13, 14, 15


static ETSTimer delayTimer;
static int fsize = -1;
static int fpos_start = -1;
static int fcur_pos = -1;

int stk_tick = 0;
int stk_stage = 0;
int stk_error = 0;
const char *stk_error_descr = NULL;
char stk_major, stk_minor, stk_signature[3];

static int in_sync(char fb, char lb)
{
	int rs = fb == 0x14 && lb == 0x10;
	if(rs) {
		os_printf(stk_error_descr="in sync\n");
	} else {
		os_printf(stk_error_descr="OUT of sync: %d %d\n", fb, lb);
	}
	return rs;
}

static void stop_ticking()
{
	os_timer_disarm(&delayTimer);
	fpos_start = fcur_pos = fsize = -1;
	uart0_lock = 0;
}

#define PAGE_SIZE (8*8)
#define VALS_COUNT (PAGE_SIZE * 2 + 8)

static void ICACHE_FLASH_ATTR runProgrammer(void *arg)
{
	int i, j, vals[VALS_COUNT], size, fpos = -1, valsize, lineaddr, rtype ,crc, val;
	static int sync_cnt;
	static int address = 0;
	char ok, insync, laddress, haddress, snum[5], *line, *p;
	os_printf("state=%d, tick %d\n", stk_stage, stk_tick);
	stk_tick++;
	if(stk_tick > TICK_MAX) {
		os_printf(stk_error_descr = "stk timeout...\n");
		stk_error = 1;
	} else {
		if(fcur_pos >= 0) {
			fpos = ff_tell();
			ff_seek(fcur_pos);
		}
		switch(stk_stage) {
			case 0:
				uart0_clean_chars();
				sync_cnt = 0;
				os_printf("syncing\n");
				uart0_tx_one_char(0x30);
				uart0_tx_one_char(0x20);
				stk_stage = 1;
				stk_tick = 0;
				break;
			case 1:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						os_printf("synced\n");
						uart0_tx_one_char(0x41);
						uart0_tx_one_char(0x81);
						uart0_tx_one_char(0x20);
						os_printf("receiving MAJOR version\n");
						stk_stage = 2;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				} else {
					if(stk_tick % SYNC_STEP == 0) {
						if(sync_cnt < 5) {
							uart0_clean_chars();
							sync_cnt++;
							os_printf("syncing\n");
							uart0_tx_one_char(0x30);
							uart0_tx_one_char(0x20);
						} else {
							os_printf(stk_error_descr = "not connected\n");
							stk_error = 1;
						}
					}
				}
				break;
			case 2:
				if(uart0_count_chars() >= 3) {
					insync = uart0_get_char(); // STK_INSYNC
					stk_major = uart0_get_char(); // STK_SW_MJAOR
					ok = uart0_get_char(); // STK_OK
					if(in_sync(insync, ok)) {
						uart0_tx_one_char(0x41);
						uart0_tx_one_char(0x82);
						uart0_tx_one_char(0x20);
						os_printf("receiving MINOR version\n");
						stk_stage = 3;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 3:
				if(uart0_count_chars() >= 3) {
					insync = uart0_get_char(); // STK_INSYNC
					stk_minor = uart0_get_char(); // STK_SW_MJAOR
					ok = uart0_get_char(); // STK_OK
					if(in_sync(insync, ok)) {
						os_printf("bootloader version %d.%d\n", stk_major, stk_minor);
						os_printf("entering prog mode\n");
						fcur_pos = fpos_start;
						uart0_tx_one_char(0x50);
						uart0_tx_one_char(0x20);
						os_printf("receiving sync ack\n");
						stk_stage = 4;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 4:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						uart0_tx_one_char(0x75);
						uart0_tx_one_char(0x20);
						os_printf("receiving signature\n");
						stk_stage = 5;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 5:
				if(uart0_count_chars() >= 3) {
					insync = uart0_get_char(); // STK_INSYNC
					stk_signature[0] = uart0_get_char();
					stk_signature[1] = uart0_get_char();
					stk_signature[2] = uart0_get_char();
					ok = uart0_get_char(); // STK_OK
					if(in_sync(insync, ok)) {
						os_printf("signature %d-%d-%d\n", stk_signature[0], stk_signature[1], stk_signature[2]);
						address = 0;
						stk_stage = 6;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 6:
				laddress = address & 0xFF;
				haddress = address >> 8 & 0xff;
				os_printf("set page addr: %4X\n", address);
				uart0_tx_one_char(0x55); //STK_LOAD_ADDRESS
				uart0_tx_one_char(laddress);
				uart0_tx_one_char(haddress);
				uart0_tx_one_char(0x20); // SYNC_CRC_EOP
				stk_stage = 7;
				stk_tick = 0;
				break;
			case 7:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						os_printf("sending program page <=%d bytes\n", PAGE_SIZE);
						memset(vals, 0, sizeof(vals));
						size = 0;
						rtype = 0;
						for(j = 0; j < 8 && rtype!= 1 && ff_tell() - fpos_start < fsize && !stk_error; j++) {
							// process single line
							p = line = ff_mread_str();
							if(line && line[0]) {
								crc = 0;
//								os_printf("line='%s'\n", line);
								snum[0] = p[1];
								snum[1] = p[2];
								snum[2] = 0;
								valsize = (int)strtol(snum, NULL, 16);
								crc += valsize;
								snum[0] = p[3];
								snum[1] = p[4];
								snum[2] = p[5];
								snum[3] = p[6];
								snum[4] = 0;
								lineaddr = (int)strtol(snum, NULL, 16);
								crc += lineaddr & 0xff;
								crc += (lineaddr >> 8) & 0xff;
								os_printf("lineaddr=0x%4.4X\n", lineaddr);
								snum[0] = p[7];
								snum[1] = p[8];
								snum[2] = 0;
								rtype = (int)strtol(snum, NULL, 16);
								crc += rtype;
								if(rtype == 0) { // record type - (0-data, 1-end)
									// skip 9 byte of : and address etc
									p += 9;
									for(i=0; i<valsize; i++) {
										snum[0] = p[0];
										snum[1] = p[1];
										snum[2] = 0;
										val = (int)strtol(snum, NULL, 16);
										crc += val;
										os_printf("j=%d, i=%d, size=%d, b=%2X (%s), ftell=%d, fsize=%d\n", j, i, size, val, snum, ff_tell()-fpos_start, fsize);
										vals[size++] = val;
										p += 2;
									}
									// check crc
									snum[0] = p[0];
									snum[1] = p[1];
									snum[2] = 0;
									val = (int)strtol(snum, NULL, 16);
									crc = ((crc ^ 0xff) + 1) & 0xff;
									if(crc != val) {
										os_printf("CRC error: crc=%X vs val=%X\n", crc, val);
										stk_error = 1;
									}
								}
								mfree(&line);
							} else {
								break;
							}
						}
						if(!stk_error) {
							uart0_tx_one_char(0x64); // STK_PROGRAM_PAGE
							uart0_tx_one_char(0); // page size
							uart0_tx_one_char(size); // page size
							uart0_tx_one_char(0x46); // flash memory, 'F'
							os_printf("size=%d\n", size);
							for(j=0; j<size; j++) {
								uart0_tx_one_char(vals[j]);
							}
							uart0_tx_one_char(0x20); // SYNC_CRC_EOP
							address += size;
							if(ff_tell() - fpos_start >= fsize || rtype == 1) {
								// end of file - stop/go to the next stage
								stk_stage = 9;
							} else {
								// wait for sync then next block
								stk_stage = 8;
							}
							stk_tick = 0;
						}
					} else {
						stk_error = 1;
					}
				}
				break;
			case 8:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						// to send address again
						stk_stage = 6;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 9:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						os_printf("leaving prog mode\n");
						uart0_tx_one_char(0x51);
						uart0_tx_one_char(0x20);
						stk_stage = 10;
						stk_tick = 0;
					} else {
						stk_error = 1;
					}
				}
				break;
			case 10:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						stk_stage = 11;
						os_printf("end\n");
						stop_ticking();
					} else {
						stk_error = 1;
					}
				}
				break;
		}
		if(fpos >= 0) {
			fcur_pos = ff_tell();
			ff_seek(fpos);
		}
	}
	if(stk_error) {
		os_printf("Error occured\n");
		stop_ticking();
	}
}

void ICACHE_FLASH_ATTR reset_arduino()
{
	// reset on gpio5
	GPIO_OUTPUT_SET(5, 0);
	os_delay_us(300);
	GPIO_OUTPUT_SET(5, 1);
}

void program(int size, int pos_start)
{
	os_timer_disarm(&delayTimer);
	fpos_start = pos_start;
	fsize = size;
	stk_stage = 0;
	stk_tick = 0;
	stk_error = 0;
	stk_error_descr = NULL;
	stk_major = stk_minor = 0;
	stk_signature[0] = stk_signature[1] = stk_signature[2] = 0;
	uart0_lock = STK500_LOCK;
	reset_arduino();
	os_timer_setfn(&delayTimer, runProgrammer, NULL);
	os_timer_arm(&delayTimer, 100, 1);
}

void ICACHE_FLASH_ATTR init_reset_pin()
{
	//set gpio5 as gpio pin
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	//disable pulldown
	PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO5_U);

	//enable pull up R
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);

	GPIO_OUTPUT_SET(5, 1);
}

void init_stk500()
{
	init_reset_pin();
}