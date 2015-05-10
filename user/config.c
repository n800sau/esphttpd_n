// this is the normal build target ESP include set
#include "espmissingincludes.h"
#include <c_types.h>
#include <strings.h>
#include "user_interface.h"
#include "espconn.h"
#include <mem.h>
#include "mmem.h"
#include "osapi.h"
#include "driver/uart.h"
#include <gpio.h>
#include "stk500.h"

#include "config.h"

#define CR "\r"
#define NL CR "\n"
#define MSG_OK "OK" NL
#define MSG_ERROR "ERROR" NL
#define MSG_INVALID_CMD "UNKNOWN COMMAND" NL

#define MAX_ARGS 12
#define MSG_BUF_LEN 128

int debug_mode = 0;

typedef struct config_commands {
	char *command;
	void (*function)(p_espconn conn, uint8_t argc, char *argv[]);
} config_commands_t;

const config_commands_t config_commands[];


char *my_strdup(char *str) {
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if (!(copy = os_malloc((u_int)len)))
		return (NULL);
	os_memcpy(copy, str, len);
	return (copy);
}

char **config_parse_args(char *buf, uint8_t *argc) {
	const char delim[] = " \t";
	char *save, *tok;
	char **argv = (char **)os_malloc(sizeof(char *) * MAX_ARGS);	// note fixed length
	os_memset(argv, 0, sizeof(char *) * MAX_ARGS);

	*argc = 0;
	for (; *buf == ' ' || *buf == '\t'; ++buf); // absorb leading spaces
	for (tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
		argv[*argc] = my_strdup(tok);
		(*argc)++;
		if (*argc == MAX_ARGS) {
			break;
		}
	}
	return argv;
}

static void config_parse_args_free(uint8_t argc, char *argv[]) {
	uint8_t i;
	for (i = 0; i <= argc; ++i) {
		if (argv[i])
			os_free(argv[i]);
	}
	os_free(argv);
}

static void config_cmd_reset(struct espconn *conn, uint8_t argc, char *argv[]) {
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	system_restart();
}

static void config_cmd_baud(struct espconn *conn, uint8_t argc, char *argv[]) {
	if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		uint32_t baud = atoi(argv[1]);
		if ((baud > (UART_CLK_FREQ / 16)) || baud == 0) {
			espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
		} else {
			// pump and dump fifo
			while(TRUE) {
				uint32_t fifo_cnt = READ_PERI_REG(UART_STATUS(0)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
				if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) == 0) {
					break;
				}
			}
			os_printf("Set baud to %d\n", baud);
			os_delay_us(1000);
			uart_div_modify(0, UART_CLK_FREQ / baud);
			espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		}
	}
}
static void config_cmd_mode(struct espconn *conn, uint8_t argc, char *argv[]) {
	uint8_t mode;

	if (argc == 0) {
		char *buf = os_malloc(MSG_BUF_LEN);
		os_sprintf(buf, "MODE=%d\n", wifi_get_opmode());
		mconcat(&buf, MSG_OK);
		espconn_sent(conn, buf, strlen(buf));
		os_free(buf);
	} else if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		mode = atoi(argv[1]);
		if (mode >= 0 && mode <= 3) {
			if (wifi_get_opmode() != mode) {
				ETS_UART_INTR_DISABLE();
				wifi_set_opmode(mode);
				ETS_UART_INTR_ENABLE();
				espconn_sent(conn, (uint8_t*)MSG_OK, strlen(MSG_OK));
				os_delay_us(10000);
				system_restart();
			} else {
				espconn_sent(conn, (uint8_t*)MSG_OK, strlen(MSG_OK));
			}
		} else {
			espconn_sent(conn, (uint8_t*)MSG_ERROR, strlen(MSG_ERROR));
		}
	}
}

// spaces are not supported in the ssid or password
static void config_cmd_sta(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct station_config sta_conf;

	os_bzero(&sta_conf, sizeof(struct station_config));
	wifi_station_get_config(&sta_conf);

	os_printf("SSID=%s PASSWORD=%s\n", sta_conf.ssid, sta_conf.password);
	if (argc == 0) {
		char *buf = os_malloc(MSG_BUF_LEN);
		os_sprintf(buf, "SSID=%s PASSWORD=%s\n", sta_conf.ssid, sta_conf.password);
		mconcat(&buf, MSG_OK);
		espconn_sent(conn, buf, strlen(buf));
		os_free(buf);
	} else if (argc != 2) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		os_strncpy(sta_conf.ssid, ssid, sizeof(sta_conf.ssid));
		os_strncpy(sta_conf.password, password, sizeof(sta_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		wifi_station_disconnect();
		ETS_UART_INTR_DISABLE(); 
		wifi_station_set_config(&sta_conf);		
		ETS_UART_INTR_ENABLE(); 
		wifi_station_connect();
	}
}

// spaces are not supported in the ssid or password
static void config_cmd_ap(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct softap_config ap_conf;

	os_bzero(&ap_conf, sizeof(struct softap_config));
	wifi_softap_get_config(&ap_conf);

	if (argc == 0) {
		char *buf = os_malloc(MSG_BUF_LEN);
		os_sprintf(buf, "SSID=%s PASSWORD=%s AUTHMODE=%d CHANNEL=%d\n", ap_conf.ssid, ap_conf.password, ap_conf.authmode, ap_conf.channel);
		mconcat(&buf, MSG_OK);
		espconn_sent(conn, buf, strlen(buf));
		os_free(buf);
	} else if (argc == 1) {
		os_strncpy(ap_conf.ssid, ssid, sizeof(ap_conf.ssid));
		os_bzero(ap_conf.password, sizeof(ap_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		ap_conf.authmode = AUTH_OPEN;
		ap_conf.channel = 6;
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&ap_conf);
		ETS_UART_INTR_ENABLE();
	} else if (argc == 2) {
		os_strncpy(ap_conf.ssid, ssid, sizeof(ap_conf.ssid));
		os_strncpy(ap_conf.password, password, sizeof(ap_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		ap_conf.authmode = AUTH_WPA_PSK;
		ap_conf.channel = 6;
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&ap_conf);
		ETS_UART_INTR_ENABLE();
	} else {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	}
}

// spaces are not supported in the ssid or password
static void io_reset(struct espconn *conn, uint8_t argc, char *argv[]) {
	reset_arduino();
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

void _ledon()
{
	os_printf("Led on\n");
	GPIO_OUTPUT_SET(GPIO_ID_PIN(5), 1);
}

void _ledoff()
{
	os_printf("Led off\n");
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(5), 0);
}

// led on via gpio5
static void io_ledon(struct espconn *conn, uint8_t argc, char *argv[]) {
	_ledon();
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

// led off via gpio5
static void io_ledoff(struct espconn *conn, uint8_t argc, char *argv[]) {
	_ledoff();
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

// led flash via gpio5
static void io_ledflash(struct espconn *conn, uint8_t argc, char *argv[]) {
	static ETSTimer resetTimer;
	_ledon();
	os_timer_setfn(&resetTimer, _ledoff, NULL);
	os_timer_arm(&resetTimer, 500, 0);
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

static void config_debug_mode(struct espconn *conn, uint8_t argc, char *argv[]) {
	uint8_t mode;

	if (argc == 0) {
		char *buf = os_malloc(MSG_BUF_LEN);
		os_sprintf(buf, "DEBUG MODE=%d\n", debug_mode);
		mconcat(&buf, MSG_OK);
		espconn_sent(conn, buf, strlen(buf));
		os_free(buf);
	} else if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		debug_mode = atoi(argv[1]);
		espconn_sent(conn, (uint8_t*)MSG_OK, strlen(MSG_OK));
	}
}

static void do_ifconfig(struct espconn *conn, uint8_t argc, char* argv[])
{
	struct ip_info sta_info, ap_info;
	int mode = wifi_get_opmode();
	char *buf = NULL;
	wifi_get_ip_info(STATION_IF, &sta_info);
	mconcat(&buf, "sta ip ");
	madd_ip(&buf, sta_info.ip.addr);
	mconcat(&buf, " mask ");
	madd_ip(&buf, sta_info.netmask.addr);
	mconcat(&buf, " gw ");
	madd_ip(&buf, sta_info.gw.addr);
	mconcat(&buf, NL);
	wifi_get_ip_info(SOFTAP_IF, &ap_info);
	mconcat(&buf, "ap ip ");
	madd_ip(&buf, ap_info.ip.addr);
	mconcat(&buf, " mask ");
	madd_ip(&buf, ap_info.netmask.addr);
	mconcat(&buf, " gw ");
	madd_ip(&buf, ap_info.gw.addr);
	mconcat(&buf, NL);
	mconcat(&buf, MSG_OK);
	espconn_sent(conn, buf, strlen(buf));
	os_free(buf);
}

static void do_help(struct espconn *conn, uint8_t argc, char* argv[])
{
	const config_commands_t *p = config_commands;
	char *buf = NULL;
	while(p->command) {
		buf = mconcat(&buf, p->command);
		buf = mconcat(&buf, NL);
//		os_printf("print %s\n", p->command);
		p++;
	}
	buf = mconcat(&buf, MSG_OK);
	espconn_sent(conn, buf, strlen(buf));
	mfree(&buf);
}

static void config_upri(struct espconn *conn, uint8_t argc, char *argv[]) {
	uart0_primary();
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

static void config_usec(struct espconn *conn, uint8_t argc, char *argv[]) {
	uart0_secondary();
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

static void io_volts(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *buf = os_malloc(MSG_BUF_LEN);
	os_sprintf(buf, "%d\n", (uint16)phy_get_vdd33());
	mconcat(&buf, MSG_OK);
	espconn_sent(conn, buf, strlen(buf));
	os_free(buf);
}

static void show_version(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *buf = os_malloc(MSG_BUF_LEN);
	os_sprintf(buf, "%d\n", system_get_boot_version());
	mconcat(&buf, MSG_OK);
	espconn_sent(conn, buf, strlen(buf));
	os_free(buf);
}

const config_commands_t config_commands[] = { 
		{ "HELP", &do_help },
		{ "VER", &show_version },
		{ "BAUD", &config_cmd_baud },
		{ "USEC", &config_usec },
		{ "UPRI", &config_upri },
		{ "IFCONFIG", &do_ifconfig },
		{ "RESET", &config_cmd_reset }, 
		{ "MODE", &config_cmd_mode },
		{ "STA", &config_cmd_sta },
		{ "AP", &config_cmd_ap },
		{ "IORST", &io_reset },
		{ "LON", &io_ledon },
		{ "LOFF", &io_ledoff },
		{ "LFLASH", &io_ledflash },
		{ "VOLTS", &io_volts },
		{ "DEBUG", &config_debug_mode },
		{ NULL, NULL }
	};

void config_parse(struct espconn *conn, char *buf, int len) {
	char *lbuf = (char *)os_malloc(len + 1), **argv;
	uint8_t i, argc;
	// we need a '\0' end of the string
	os_memcpy(lbuf, buf, len);
	lbuf[len] = '\0';
	
	// command echo
	//espconn_sent(conn, lbuf, len);

	// remove any CR / LF
	for (i = 0; i < len; ++i)
		if (lbuf[i] == '\n' || lbuf[i] == '\r')
			lbuf[i] = '\0';

	// verify the command prefix
	if (os_strncmp(lbuf, "+++AT", 5) != 0) {
		return;
	}
	// parse out buffer into arguments
	argv = config_parse_args(&lbuf[5], &argc);
#if 0
// debugging
	{
		uint8_t i;
		size_t len;
		char buf[MSG_BUF_LEN];
		for (i = 0; i < argc; ++i) {
			//len = os_snprintf(buf, MSG_BUF_LEN, "argument %d: '%s'\r\n", i, argv[i]);
			len = os_sprintf(buf, "argument %d: '%s'\r\n", i, argv[i]);
			espconn_sent(conn, buf, len);
		}
	}
// end debugging
#endif
	if (argc == 0) {
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else {
		argc--;	// to mimic C main() argc argv
		for (i = 0; config_commands[i].command; ++i) {
			if (os_strncmp(argv[0], config_commands[i].command, strlen(argv[0])) == 0) {
				config_commands[i].function(conn, argc, argv);
				break;
			}
		}
		if (!config_commands[i].command)
			espconn_sent(conn, MSG_INVALID_CMD, strlen(MSG_INVALID_CMD));
	}
	config_parse_args_free(argc, argv);
	os_free(lbuf);
}

