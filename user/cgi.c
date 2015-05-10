/*
Some random cgi routines. Used in the LED example and the page that returns the entire
flash as a binary. Also handles the hit counter on the main page.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include "stk500.h"
#include <string.h>
#include <osapi.h>
#include "user_interface.h"
#include <driver/uart.h>
#include <mem.h>
#include "httpd.h"
#include "cgi.h"
#include "config.h"
#include "base64.h"
#include <ip_addr.h>
#include "espmissingincludes.h"
#include "mmem.h"
#include <ets_sys.h>
#include "cmucam4.h"

//cause I can't be bothered to write an ioGetLed()
static char currLedState=0;

//Cgi that turns the LED on or off according to the 'led' param in the POST data
int ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024], *line;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	ff_reset();
	line = ff_mread_str();
	len = httpdFindArg(line, "led", buff, sizeof(buff));
	mfree(&line);
	if (len!=0) {
		currLedState=atoi(buff);
		if(currLedState) _ledon(); else _ledoff();
	}

	httpdRedirect(connData, "led.tpl");
	return HTTPD_CGI_DONE;
}



//Template code for the led page.
void ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "ledstate")==0) {
		if (currLedState) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
}

static long hitCounter=0;

//Template code for the counter on the index page.
void ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
}


//Cgi that reads the SPI flash. Assumes 512KByte flash.
int ICACHE_FLASH_ATTR cgiReadFlash(HttpdConnData *connData) {
	int *pos=&connData->pos;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start flash download.\n");
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/bin");
		httpdEndHeaders(connData);
		*pos=0x40200000;
		return HTTPD_CGI_MORE;
	}
	//Send 1K of flash per call. We will get called again if we haven't sent 512K yet.
	espconn_sent(connData->conn, (uint8 *)(*pos), 1024);
	*pos+=1024;
	if (*pos>=0x40200000+(512*1024)) return HTTPD_CGI_DONE; else return HTTPD_CGI_MORE;
}

int ICACHE_FLASH_ATTR cgiProgram(HttpdConnData *connData)
{
	char *p;
	int sz, baud, pos_start, pos_end;

	ets_wdt_disable();

	if(connData->postLen <= 0) {
		os_printf("Error post len=%d\n", connData->postLen);
		return HTTPD_CGI_ERROR;
	}
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		os_printf("Aborted\n");
		return HTTPD_CGI_DONE;
	}

//	os_printf("boundary=%s\n", connData->boundary);
	sz = httpdFindMultipartArg(connData->boundary, "baud", &pos_start, &pos_end);
	if( sz < 0 ) {
		os_printf("\"baud\" not found\n");
	} else {
		ff_seek(pos_start);
		p = ff_mread_alloc(sz+1);
		p[sz] = 0;
		baud = atoi(p);
		mfree(&p);
		os_printf("baud=%d\n", baud);
		switch(baud) {
			case 19200:
				baud = BIT_RATE_19200;
				break;
			case 38400:
				baud = BIT_RATE_38400;
				break;
			case 57600:
				baud = BIT_RATE_57600;
				break;
			case 115200:
				baud = BIT_RATE_115200;
				break;
			default:
				baud = 0;
				break;
		}
		if(baud) {
			uart0_change_rate(baud);
		}
	}
	sz = httpdFindMultipartArg(connData->boundary, "datafile", &pos_start, &pos_end);
	wdt_feed();
	ets_wdt_enable();
	if( sz < 0 ) {
		os_printf("\"datafile\" not found\n");
	} else {
		os_printf("file size=%d\n", sz);
		program(sz, pos_start);
		httpdRedirect(connData, "/programming.tpl");
	}
	return HTTPD_CGI_DONE;
}

void tplProgramming(HttpdConnData *connData, char *token, void **arg)
{
	char buff[128];
	if (token==NULL) return;
	os_strcpy(buff, "");
	if(stk_stage >= 11) {
		if (os_strcmp(token, "prog_status")==0) {
			os_strcpy(buff, "finished");
		}
		if (os_strcmp(token, "is_return")==0) {
			os_strcpy(buff, "true");
		}
	} else {
		if (os_strcmp(token, "is_return")==0) {
			os_strcpy(buff, "false");
		}
		if (os_strcmp(token, "prog_status")==0) {
			os_sprintf(buff, "%s %d%%", (stk_error) ? "error occured at": "done", stk_percent);
		}
		if (os_strcmp(token, "status_msg")==0) {
			os_sprintf(buff, "%s", (stk_error_descr) ? stk_error_descr: "");
		}
	}
	if (os_strcmp(token, "is_error")==0) {
		os_strcpy(buff, (stk_error) ? "true" : "false");
	}
	if (os_strcmp(token, "bl_version")==0) {
		os_sprintf(buff, "%d.%d", stk_major, stk_minor);
	}
	if (os_strcmp(token, "signature")==0) {
		os_sprintf(buff, "%d.%d.%d", stk_signature[0], stk_signature[1], stk_signature[2]);
	}
	httpdSend(connData, buff, -1);
}

// Switch serial to the secondary pins
// Read and return colour bmp from CMUcam4
int ICACHE_FLASH_ATTR cgiCmuCam4color(HttpdConnData *connData) {
	int *pos=&connData->pos;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start color bmp download.\n");
		char buf[9600];
		int n = cmucam4_color(buf, sizeof(buf));
		return HTTPD_CGI_MORE;
	}
}

// Switch serial to the secondary pins
// Read and return mask bmp from CMUcam4
int ICACHE_FLASH_ATTR cgiCmuCam4bw(HttpdConnData *connData) {
	int *pos=&connData->pos;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start mask bmp download.\n");
		char buf[600];
		int n=cmucam4_bw(buf, sizeof(buf));
		if(n < 0) {
			return HTTPD_CGI_ERROR;
		} else if(n == 0) {
			httpdStartResponse(connData, 200);
			httpdHeader(connData, "Content-Type", "application/octet-stream");
			httpdEndHeaders(connData);
			return HTTPD_CGI_MORE;
		} else {
			espconn_sent(connData->conn, buf, n);
			return HTTPD_CGI_DONE;
		}
	}
}

// Switch serial to the secondary pins
// Read and return tc json from CMUcam4
int ICACHE_FLASH_ATTR cgiCmuCam4tc(HttpdConnData *connData) {
	int *pos=&connData->pos;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start TC json download.\n");
		char buf[600];
		int n=cmucam4_tc(buf, sizeof(buf));
		if(n < 0) {
			return HTTPD_CGI_ERROR;
		} else if(n == 0) {
			httpdStartResponse(connData, 200);
			httpdHeader(connData, "Content-Type", "application/json");
			httpdEndHeaders(connData);
			return HTTPD_CGI_MORE;
		} else {
			espconn_sent(connData->conn, buf, n);
			return HTTPD_CGI_DONE;
		}
	}
}

// Switch serial to the secondary pins
// Read and return tw json from CMUcam4
int ICACHE_FLASH_ATTR cgiCmuCam4tw(HttpdConnData *connData) {
	char buf[200];
	int *pos=&connData->pos;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start TW json download.\n");
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/json");
		httpdEndHeaders(connData);
		if(cmucam4_send("TW") < 0) {
			return HTTPD_CGI_ERROR;
		} else {
			return HTTPD_CGI_MORE;
		}
	} else {
		int n=cmucam4_tw(buf, sizeof(buf));
		if(n < 0) {
			return HTTPD_CGI_ERROR;
		} else if(n == 0) {
		} else {
			espconn_sent(connData->conn, buf, n);
			return HTTPD_CGI_DONE;
		}
	}
}

