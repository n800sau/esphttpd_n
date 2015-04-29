/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: uart.c
 *
 * Description: Two UART mode configration and interrupt handler.
 *				Check your hardware connection while use this mode.
 *
 * Modification history:
 *	   2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "user_interface.h"
#include "eagle_soc_ext.h"

#define UART0	0
#define UART1	1

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

LOCAL void uart0_rx_intr_handler(void *para);

#define recvCharTaskPrio		0
#define recvCharTaskQueueLen	64

#define UART0_BUF_SIZE 512
static char uart0_buf[UART0_BUF_SIZE];
static int uart0_buf_pos = 0;

int uart0_lock = 0;

static int _uart0_primary = 1;

void ICACHE_FLASH_ATTR uart0_add_char(char c)
{
	uart0_buf[uart0_buf_pos] = c;
	uart0_buf_pos++;
	if(uart0_buf_pos >= sizeof(uart0_buf)) {
		uart0_buf_pos = sizeof(uart0_buf) - 1;
		// forget the oldest
		memmove(uart0_buf, uart0_buf+1, sizeof(uart0_buf) - 1);
	}
}

char ICACHE_FLASH_ATTR uart0_get_char()
{
	char rs = -1;
	if(uart0_buf_pos > 0) {
		rs =  uart0_buf[0];
		memmove(uart0_buf, uart0_buf+1, sizeof(uart0_buf) - 1);
		uart0_buf_pos--;
	}
//	os_printf("get char : %2X\n", rs);
	return rs;
}

char ICACHE_FLASH_ATTR uart0_peek_char()
{
	return (uart0_buf_pos > 0) ? uart0_buf[0]: -1;
}

int ICACHE_FLASH_ATTR uart0_count_chars()
{
	return uart0_buf_pos;
}

void ICACHE_FLASH_ATTR uart0_clean_chars()
{
	uart0_buf_pos = 0;
}

#define UART1_BUF_SIZE 512
static char uart1_buf[UART1_BUF_SIZE];
static int uart1_buf_pos = 0;

void ICACHE_FLASH_ATTR uart1_add_char(char c)
{
	uart1_buf[uart1_buf_pos] = c;
	uart1_buf_pos++;
	if(uart1_buf_pos >= sizeof(uart1_buf)) {
		uart1_buf_pos = sizeof(uart1_buf) - 1;
		// forget the oldest
		memmove(uart1_buf, uart1_buf+1, sizeof(uart1_buf) - 1);
	}
}

char ICACHE_FLASH_ATTR uart1_get_char()
{
	char rs = -1;
	if(uart1_buf_pos > 0) {
		rs =  uart1_buf[0];
		memmove(uart1_buf, uart1_buf+1, sizeof(uart1_buf) - 1);
		uart1_buf_pos--;
	}
//	os_printf("get char : %2X\n", rs);
	return rs;
}

int ICACHE_FLASH_ATTR uart1_count_chars()
{
	return uart1_buf_pos;
}

void ICACHE_FLASH_ATTR uart1_clean_chars()
{
	uart1_buf_pos = 0;
}

static os_event_t recvCharTaskQueue[recvCharTaskQueueLen];

static void ICACHE_FLASH_ATTR recvCharTaskCb(os_event_t *events)
{
	uint8_t temp;

	//add transparent determine
	while(READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
	{
		WRITE_PERI_REG(0X60000914, 0x73); //WTD

		temp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
		uart0_add_char(temp);
//		os_printf("char=%2X\n", temp);
	}
	if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
	} else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
	}
	ETS_UART_INTR_ENABLE();
}

/******************************************************************************
 * FunctionName : uart_config
 * Description	: Internal used function
 *				  UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *				  UART1 just used for debug output
 * Parameters	: uart_no, use UART0 or UART1 defined ahead
 * Returns		: NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR uart_config(uint8 uart_no)
{
	if (uart_no == UART1) {
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
	} else {
		/* rcv_buff size if 0x100 */
		ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, &(UartDev.rcv_buff));
		PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
	}

	uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));

	WRITE_PERI_REG(UART_CONF0(uart_no), UartDev.exist_parity
				   | UartDev.parity
				   | (UartDev.stop_bits << UART_STOP_BIT_NUM_S)
				   | (UartDev.data_bits << UART_BIT_NUM_S));


	//clear rx and tx fifo,not ready
	SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
	CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

	//set rx fifo trigger
	WRITE_PERI_REG(UART_CONF1(uart_no), (UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S);

	//clear all interrupt
	WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
	//enable rx_interrupt
	SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart0_tx_one_char
 * Description	: Internal used function
 *				  Use uart0 interface to transfer one char
 * Parameters	: uint8 TxChar - character to tx
 * Returns		: OK
*******************************************************************************/
STATUS ICACHE_FLASH_ATTR uart0_tx_one_char(uint8 TxChar)
{
	while (true)
	{
		uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(UART0)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
		if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
			break;
		}
	}

	WRITE_PERI_REG(UART_FIFO(UART0) , TxChar);
	return OK;
}

/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description	: Internal used function
 *				  Use uart1 interface to transfer one char
 * Parameters	: uint8 TxChar - character to tx
 * Returns		: OK
*******************************************************************************/
LOCAL STATUS ICACHE_FLASH_ATTR uart1_tx_one_char(uint8 TxChar)
{
	while (true)
	{
		uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(UART1)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
		if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
			break;
		}
	}

	WRITE_PERI_REG(UART_FIFO(UART1) , TxChar);
	return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description	: Internal used function
 *				  Do some special deal while tx char is '\r' or '\n'
 * Parameters	: char c - character to tx
 * Returns		: NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR uart1_write_char(char c)
{
	uart1_add_char(c);
	if (c == '\n') {
		uart1_tx_one_char('\r');
		uart1_tx_one_char('\n');
	} else if (c != '\r') {
		uart1_tx_one_char(c);
	}
}

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description	: UART0 interrupt handler
 * Parameters	: void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns		: NONE
*******************************************************************************/
LOCAL void uart0_rx_intr_handler(void *para)
{
  /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
	* uart1 and uart0 respectively
	*/
  uint8 uart_no = UART0;

  if(UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_FRM_ERR_INT_ST))
  {
	os_printf("FRM_ERR\r\n");
	WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
  }

  if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
  {
//	  os_printf("fifo full\r\n");
	ETS_UART_INTR_DISABLE();
	system_os_post(recvCharTaskPrio, 0, 0);

  }
  else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST))
  {
	ETS_UART_INTR_DISABLE();
	system_os_post(recvCharTaskPrio, 0, 0);
  }
}


/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description	: use uart0 to transfer buffer
 * Parameters	: uint8 *buf - point to send buffer
 *				  uint16 len - buffer len
 * Returns		:
*******************************************************************************/
void ICACHE_FLASH_ATTR uart0_tx_buffer(uint8 *buf, uint16 len)
{
	uint16 i;

	for (i = 0; i < len; i++) {
		uart0_tx_one_char((char)buf[i]);
	}
}

void ICACHE_FLASH_ATTR uart0_change_rate(UartBautRate uart0_br)
{
	ETS_UART_INTR_DISABLE();
	UartDev.baut_rate = uart0_br;
	uart_config(UART0);
	ETS_UART_INTR_ENABLE();
}

void ICACHE_FLASH_ATTR uart0_primary()
{
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

	PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);

	CLEAR_PERI_REG_MASK(0x3ff00028, BIT2);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);

	_uart0_primary = 1;
}

void ICACHE_FLASH_ATTR uart0_secondary()
{
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);

	PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDO_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_UART0_RTS);

	//SWAP PIN : U0TXD<==>U0RTS(MTDO, GPIO15) , U0RXD<==>U0CTS(MTCK, GPIO13)
	SET_PERI_REG_MASK(0x3ff00028, BIT2);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1);

	_uart0_primary = 0;
}

int ICACHE_FLASH_ATTR is_uart0_primary()
{
	return _uart0_primary;
}


/******************************************************************************
 * FunctionName : uart_init
 * Description	: user interface for init uart
 * Parameters	: UartBautRate uart0_br - uart0 bautrate
 *				  UartBautRate uart1_br - uart1 bautrate
 * Returns		: NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
	// rom use 74880 baut_rate, here reinitialize
	UartDev.baut_rate = uart0_br;
	uart_config(UART0);
	UartDev.baut_rate = uart1_br;
	uart_config(UART1);
	ETS_UART_INTR_ENABLE();

	system_os_task(recvCharTaskCb, recvCharTaskPrio, recvCharTaskQueue, recvCharTaskQueueLen);
}

