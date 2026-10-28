#include "uart.h"
#include "uart_internal.h"

// RXC0 is set if there is unread data in the receiver buffer
// is unset when the receiver buffer is emptied
ISR(USART_RX_vect)
{
	uint8_t status = UCSR0A;
	uint8_t byte = UDR0; // consume reg (important do not return without consuming)
	uint8_t next = NEXT(rx_head);

	if (status & _BV(FE0))
		uart_errors |= UART_ERR_FRAME;
	if (status & _BV(DOR0))
		uart_errors |= UART_ERR_OVERRUN;
	if (status & _BV(UPE0))
		uart_errors |= UART_ERR_PARITY;

	if (status & (_BV(FE0) | _BV(DOR0) | _BV(UPE0)))
		return ;

	if (next == rx_tail)
	{
		uart_errors |= UART_ERR_RX_FULL;
		return ;
	}

	// finally 
	rx_buf[rx_head] = byte;
	rx_head = next;
}

// Data Reg Empty
// is activated by uart_write
// the transmit buffer UDR0 is ready to receive new data
ISR(USART_UDRE_vect)
{
	// if empty buffer
	if (tx_tail == tx_head)
	{
		// deactivate uart tx interrupt UDRE0 : Data Reg EMpty 
		UCSR0B &= ~(_BV(UDRIE0));
		return ;
	}

	// rb update
	UDR0 = tx_buf[tx_tail];
	tx_tail = NEXT(tx_tail);
}