#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define UART_READLINE_BUDGET 16

/*
	error percent ~ 2.1 
	page 199 table 20-7
	for async double speed mode 


		 		F_CPU
	UBRR =	------------    - 1
			8 * BAUD_RATE

			give us 17.36 ... - 1 = 16.36 ...

			~ 16
*/
#define UBRR_VALUE ((F_CPU / (BAUD_RATE * 8)) - 1) // = 16


//	error mgmt
#define	UART_ERR_TX_FULL	(1 << 0)
#define	UART_ERR_RX_FULL	(1 << 1)
#define	UART_ERR_FRAME		(1 << 2)
#define	UART_ERR_OVERRUN	(1 << 3)
#define	UART_ERR_PARITY		(1 << 4)
#define UART_ERR_MASK		(UART_ERR_RX_FULL | UART_ERR_FRAME | UART_ERR_OVERRUN | UART_ERR_PARITY)

// core
void	uart_init(uint16_t ubrr);
bool	uart_write(uint8_t byte);
bool	uart_read(uint8_t *byte);
size_t	uart_printstr(const char *str);

size_t	uart_write_bytes(const uint8_t *src, size_t len);
size_t	uart_read_bytes(uint8_t *dst, size_t len);

// utils
uint8_t	uart_take_errors(void);
void	uart_rx_flush(void);
uint8_t	uart_tx_free(void);
bool	uart_try_write(const uint8_t *src, uint8_t len);

// uart_readline.c

typedef enum	enum_uart_line_state
{
	UART_LINE_WAIT,
	UART_LINE_READY,
	UART_LINE_INVALID,
	UART_LINE_CORRUPTED,
	UART_LINE_OVERFLOW
}				e_uart_line_state;

typedef struct	struct_uart_line
{
	uint8_t				offset;
	uint8_t				capacity;
	char				*buffer;
	e_uart_line_state	error;
	uint8_t				escape; // 1 == ESC // 2 == [ (CSI) // 3 == 0 (SS3)
}				s_uart_line;

s_uart_line			uart_line_init(char *buffer, uint8_t capacity);
e_uart_line_state	uart_readline(s_uart_line *line);
void				uart_line_invalidate(s_uart_line *line);
