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
int stk_percent = 0;
int stk_error = 0;
const char *stk_error_descr = NULL;
char stk_major, stk_minor, stk_signature[3];

static ICACHE_FLASH_ATTR int in_sync(char fb, char lb)
{
	int rs = fb == 0x14 && lb == 0x10;
	if(rs) {
//		os_printf("in sync\n");
	} else {
		stk_error_descr="OUT of sync";
		os_printf("%s: %d %d\n", stk_error_descr, fb, lb);
	}
	return rs;
}

static ICACHE_FLASH_ATTR void stop_ticking()
{
	os_timer_disarm(&delayTimer);
	fpos_start = fcur_pos = fsize = -1;
	uart0_lock = 0;
}

static void ICACHE_FLASH_ATTR stopReset(void *arg)
{
//	gpio_output_set(BIT12, 0, BIT12, 0);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
	os_printf("Reset off\n");
}

void reset_arduino()
{
	static ETSTimer resetTimer;
	// reset on gpio5
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0);

//	gpio_output_set(0, BIT12, BIT12, 0);
	os_printf("Reset on\n");
//	os_delay_us(500000);
//	gpio_output_set(BIT12, 0, BIT12, 0);
//	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
	os_timer_setfn(&resetTimer, stopReset, NULL);
	os_timer_arm(&resetTimer, 500, 0);
}

#define PAGE_SIZE (8*16)
#define VALS_COUNT PAGE_SIZE

typedef struct _CUR_LINE {
	int addr;
	int type;
	int n;
	int vals[VALS_COUNT];
	int idx;
} CUR_LINE;

static CUR_LINE cline = { .n=0, .idx=0 };

// return -1 if error or end of vals
static ICACHE_FLASH_ATTR int read_cur_byte()
{
	char *line, *p, snum[5];
	int rs = -1, crc, val, i, n;
	if( cline.idx >= cline.n ) {
		stk_percent =  100 * (ff_tell() - fpos_start) / fsize;
//		os_printf("tell=%d, off=%d, fsize=%d, percent=%d\n", ff_tell(), fpos_start, fsize, stk_percent);
		if(ff_tell() - fpos_start < fsize) {
			// read next line;
			p = line = ff_mread_str();
//			os_printf("Read line %s\n", line);
			if(line && line[0]) {
				crc = 0;
//				os_printf("line='%s'\n", line);
				snum[0] = p[7];
				snum[1] = p[8];
				snum[2] = 0;
				cline.type = (int)strtol(snum, NULL, 16);
				crc += cline.type;
				if(cline.type == 0) { // record type - (0-data, 1-end)
					snum[0] = p[1];
					snum[1] = p[2];
					snum[2] = 0;
					n = (int)strtol(snum, NULL, 16);
					crc += n;
					if(n > VALS_COUNT) {
						os_printf("Error. Hex line is too long (%d > %d).\n", n, VALS_COUNT);
					} else {
						snum[0] = p[3];
						snum[1] = p[4];
						snum[2] = p[5];
						snum[3] = p[6];
						snum[4] = 0;
						cline.addr = (int)strtol(snum, NULL, 16);
						crc += cline.addr & 0xff;
						crc += (cline.addr >> 8) & 0xff;
//						os_printf("lineaddr=0x%4.4X\n", cline.addr);
						// skip 9 byte of : and address etc
						p += 9;
						for(i=0; i<n; i++) {
							snum[0] = p[0];
							snum[1] = p[1];
							snum[2] = 0;
							val = (int)strtol(snum, NULL, 16);
							crc += val;
//							os_printf("b=%2X (%s), ftell=%d, fsize=%d\n", val, snum, ff_tell()-fpos_start, fsize);
							cline.vals[i] = val;
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
						}
						cline.n = n;
						cline.idx = 0;
					}
				}
				mfree(&line);
			}
		}
	}
	if( cline.idx < cline.n) {
		rs = cline.vals[cline.idx];
	}
	return rs;
}

// return -1 if error or end of vals
static ICACHE_FLASH_ATTR int read_next_byte()
{
	int rs = read_cur_byte();
	if(rs >= 0) {
		cline.idx++;
	}
	return rs;
}

static void ICACHE_FLASH_ATTR runProgrammer(void *arg)
{
	int i, j, fpos, b;
	static int sync_cnt;
	static int address = -1;
	char ok, insync, laddress, haddress;
	os_printf("state=%d, tick %d\n", stk_stage, stk_tick);
	stk_tick++;
	if(stk_tick > TICK_MAX) {
		os_printf(stk_error_descr = "stk timeout...\n");
		stk_error = 1;
	} else {
		if(fcur_pos >= 0) {
			fpos = ff_tell();
			ff_seek(fcur_pos);
//			os_printf("fpos=%d, fcur_pos=%d\n", fpos, fcur_pos);
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
						ff_seek(fpos_start);
						address = -1;
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
				if( address < 0) {
					b = read_cur_byte();
					if(b >= 0) {
						address = cline.addr;
					} else {
						stk_error = 1;
						stop_ticking();
					}
				}
				if(!stk_error) {
					laddress = address & 0xFF;
					haddress = address >> 8 & 0xff;
					os_printf("set page addr: %4X\n", address);
					uart0_tx_one_char(0x55); //STK_LOAD_ADDRESS
//					os_printf("send laddr: %2X\n", laddress);
					uart0_tx_one_char(laddress);
//					os_printf("send haddr: %2X\n", haddress);
					uart0_tx_one_char(haddress);
					address += PAGE_SIZE >> 1;
					uart0_tx_one_char(0x20); // SYNC_CRC_EOP
					stk_stage = 7;
					stk_tick = 0;
				}
				break;
			case 7:
				if(uart0_count_chars() >= 2) {
					insync = uart0_get_char();
					ok = uart0_get_char();
					if(in_sync(insync, ok)) {
						uart0_tx_one_char(0x64); // STK_PROGRAM_PAGE
						uart0_tx_one_char(0); // page size
						uart0_tx_one_char(PAGE_SIZE); // page size
						uart0_tx_one_char(0x46); // flash memory, 'F'
						for(i=0; i<PAGE_SIZE; i++) {
							b = read_next_byte();
							uart0_tx_one_char(b);
						}
						uart0_tx_one_char(0x20); // SYNC_CRC_EOP
						if(b < 0) {
							// end of file - stop/go to the next stage
							stk_stage = 9;
						} else {
							// wait for sync then next block
							stk_stage = 8;
						}
						stk_tick = 0;
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
						reset_arduino();
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

void program(int size, int pos_start)
{
	os_timer_disarm(&delayTimer);
	fpos_start = pos_start;
	fsize = size;
	stk_stage = 0;
	stk_tick = 0;
	stk_percent = 0;
	stk_error = 0;
	stk_error_descr = NULL;
	stk_major = stk_minor = 0;
	stk_signature[0] = stk_signature[1] = stk_signature[2] = 0;
	reset_arduino();
	uart0_lock = STK500_LOCK;
	os_timer_setfn(&delayTimer, runProgrammer, NULL);
	os_timer_arm(&delayTimer, 100, 1);
}

void ICACHE_FLASH_ATTR init_reset_pin()
{
	//set gpio5 as gpio pin
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);

	//disable pulldown
	PIN_PULLDWN_DIS(PERIPHS_IO_MUX_MTDI_U);

	//enable pull up R
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDI_U);

	//1
//	gpio_output_set(BIT12, 0, BIT12, 0);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
}

void init_stk500()
{
	init_reset_pin();
}