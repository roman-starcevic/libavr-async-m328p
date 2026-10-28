#include "uart.h"
#include "uart_internal.h"

#ifndef F_CPU
# error "F_CPU must be defined"
#endif

#if UART_BUFFER_SIZE < 2 || UART_BUFFER_SIZE > 256 || (UART_BUFFER_SIZE & (UART_BUFFER_SIZE - 1))
# error "UART_BUFFER_SIZE must be a power of two (2 - 256)"
#endif

volatile uint8_t	uart_errors = 0;

volatile uint8_t 	tx_buf[UART_BUFFER_SIZE];
volatile uint8_t 	rx_buf[UART_BUFFER_SIZE];
volatile uint8_t	tx_tail = 0;
volatile uint8_t	tx_head = 0;
volatile uint8_t	rx_tail = 0;
volatile uint8_t	rx_head = 0;

/*
	UART init
	Double Speed Mode U2X
	115200 8N1
	Baud 115200
	size 8 bits
	No parity
	1 stop bit
*/
void uart_init(uint16_t ubrr)
{
	// power reduction register
	PRR &= ~(1 << PRUSART0);

	// set baud rate on 16 bit
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;

	// enable Double Speed mode
	UCSR0A |= (1 << U2X0);

	// not enable receiver and transmitter 
	UCSR0B = 0; // (1<<RXEN0) | (1<<TXEN0);

	// set UCSZn mode 8 bit
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
	// we will not set UMSELn as we want an async start
	// we will not set USBS0 as we want only one STOP bit
	// we will not set UPMnx as we want the parity check disabled
	// we will not set UCPOLn as we want the signal catch on the rising edge (default)

	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}

/*
	async write:
	return false if buffer full

	will write in buffer non blocking op
	atomic block is used here to modifiy the error data of teh runnign process
	the same op is performed in the ISR but the ATOMIC block is not needed there as ISR is alredy ATOMIC
*/
bool	uart_write(uint8_t byte)
{
	uint8_t	next = NEXT(tx_head);

	// buffer full
	if (next == tx_tail)
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			uart_errors |= UART_ERR_TX_FULL;
		}
		return (false);
	}

	// rb update
	tx_buf[tx_head] = byte;
	tx_head = next;

	// enable TX UDRE0 "data reg empty" interrupt, 
	// this interrupt will be activated when the receiver have consume the data
	// the ISR() will disable it when queue is empty
	UCSR0B |= _BV(UDRIE0);

	return (true);
}

/*
	async read:
	return false if buffer empty
*/
bool	uart_read(uint8_t *byte)
{
	// buf empty
	if (rx_head == rx_tail)
		return (false);

	*byte = rx_buf[rx_tail];
	rx_tail = NEXT(rx_tail);
	
	return (true);
}

uint8_t	uart_tx_free(void)
{
	return ((uint8_t)(tx_tail - tx_head - 1)
			& (UART_BUFFER_SIZE - 1));
}


bool	uart_try_write(const uint8_t *src, uint8_t len)
{
	uint8_t head = tx_head;

	if (!len)
		return (true);
	if (!src || len > uart_tx_free())
		return (false);

	// prepare bytes
	for (uint8_t i = 0; i < len; i++)
	{
		tx_buf[head] = src[i];
		head = NEXT(head);
	}

	// publish
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		tx_head = head;
		UCSR0B |= _BV(UDRIE0);
	}

	return (true);
}

size_t	uart_write_bytes(const uint8_t *src, size_t len)
{
	size_t i = 0;

	while (i < len && uart_write(src[i]))
		i++;

	return (i);
}

size_t	uart_read_bytes(uint8_t *dst, size_t len)
{
	size_t i = 0;

	while (i < len && uart_read(&dst[i]))
		i++;

	return (i);
}

/*
	blocking function
*/
size_t uart_printstr(const char *str)
{
	size_t i = 0;

	if (!str)
		return (0);

	while (str[i])
	{
		// Wait until TX has space
		while (uart_tx_free() == 0)
			;

		// Queue the next byte
		if (uart_write((uint8_t)str[i]))
			i++;
	}

	return (i);
}
