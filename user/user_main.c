

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include "espmissingincludes.h"
#include <user_interface.h>
#include <mem.h>
#include "ets_sys.h"
#include "osapi.h"
#include "httpd.h"
#include "io.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "auth.h"
#include "driver/uart.h"
#include "stk500.h"
#include "server.h"
#include <gpio.h>

// AUTH_PASSWORD is used for turning on authorisation

#ifdef AUTH_PASSWORD
//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		os_strcpy(user, "admin");
		os_strcpy(pass, AUTH_PASSWORD);
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		os_strcpy(user, "user1");
//		os_strcpy(pass, "something");
//		return 1;
	}
	return 0;
}
#endif

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/index.tpl"},
	{"/flash.bin", cgiReadFlash, NULL},
	{"/led.tpl", cgiEspFsTemplate, tplLed},
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/led.cgi", cgiLed, NULL},
	{"/program.cgi", cgiProgram, NULL},
	{"/programming.tpl", cgiEspFsTemplate, tplProgramming},

	//Routines to make the /wifi URL and everything beneath it work.

//Enable the line below to protect an username/password combo.
#ifdef AUTH_PASSWORD
	{"*", authBasic, myPassFn},
#endif

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/setmode.cgi", cgiWifiSetMode, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

//Main routine. Initialize uart, the I/O and the webserver and we're done.
void user_init(void) {

// how to setup ip only
//or change mac

	gpio_init();

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	// install uart1 putc callback
	os_install_putc1((void *)uart1_write_char);

	os_printf("\n\n\n\n\n\n\n\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

//	ioInit();
	httpdInit(builtInUrls, 80);
	serverInit(23);

	init_stk500();
	os_printf("\nReady\n");
}
