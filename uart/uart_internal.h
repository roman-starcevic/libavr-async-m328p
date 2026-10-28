#pragma once

#include <stdint.h>
#include <stdbool.h>

// must be power of 2 (for fast modulo)
#ifndef UART_BUFFER_SIZE
# define UART_BUFFER_SIZE 32
#endif

// ring buffer access via bitwise AND for modulo : id %= BUFFER_SIZE
#define NEXT(i) (((i) + 1) & (UART_BUFFER_SIZE - 1))

//	error mgmt
extern volatile	uint8_t		uart_errors;

//	Ring Buffer implementation
extern volatile uint8_t 	tx_buf[UART_BUFFER_SIZE];
extern volatile uint8_t 	rx_buf[UART_BUFFER_SIZE];
extern volatile uint8_t		tx_tail;
extern volatile uint8_t		tx_head;
extern volatile uint8_t		rx_tail;
extern volatile uint8_t		rx_head;

