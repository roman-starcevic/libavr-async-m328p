#include "uart.h"
#include "uart_internal.h"

uint8_t	uart_take_errors(void)
{
	uint8_t res;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		res = uart_errors;
		uart_errors = 0;
	}

	return (res);
}

void	uart_rx_flush(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		rx_tail = rx_head;
	}
}
