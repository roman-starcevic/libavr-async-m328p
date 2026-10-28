#include "uart.h"
#include "uart_internal.h"

void				uart_line_invalidate(s_uart_line *line)
{
	line->error = UART_LINE_CORRUPTED;
}

s_uart_line			uart_line_init(char *addr, uint8_t capacity)
{
	s_uart_line out;

	out.offset = 0;
	out.capacity = capacity;
	out.buffer = addr;
	out.error = UART_LINE_WAIT;
	out.escape = 0;

	return (out);
}

/*
https://www.gnu.org/software/teseq/manual/teseq.html
*/
static bool			handle_ansi_esc(s_uart_line *line, char c)
{
	if (line->escape == 1)
	{
		if (c == 0x1B)
			line->escape = 1; // rep start
		else if (c == '[')
			line->escape = 2; // CSI
		else if (c == 'O')
			line->escape = 3; // SS3
		else
			return (false);
	}
	else if (line->escape == 2)
	{
		// ANSI CSI final byte
		if (c >= 0x40 && c <= 0x7E)
			line->escape = 0;
		else if (c < 0x20 || c > 0x3F) // error
			return (false);
	}
	else if (line->escape == 3)
	{
		// SS3 sequence final byte
		if (c < 0x40 || c > 0x7E)
			return (false);
		line->escape = 0;
	}
	return (true);
}

e_uart_line_state	uart_readline(s_uart_line *line)
{
	uint8_t				c;
	e_uart_line_state	state;

	if (!line || !line->buffer || line->capacity < 2)
		return (UART_LINE_INVALID);
	
	for (uint8_t i = 0; i < UART_READLINE_BUDGET; i++)
	{
		if (uart_tx_free() < 3)
			break ;

		if (!uart_read(&c))
			break ;
		
		// EOF
		if (c == '\r')
		{
			state = line->error == UART_LINE_WAIT ? UART_LINE_READY : line->error;

			if (state == UART_LINE_READY || state == UART_LINE_OVERFLOW)
				line->buffer[line->offset] = '\0';
			else
				line->buffer[0] = '\0';

			line->offset = 0;
			line->error = UART_LINE_WAIT;
			line->escape = 0;

			uart_printstr("\r\n");

			return (state);
		}

		// ANSI esc sequence
		if (line->escape)
		{
			if (!handle_ansi_esc(line, c))
			{
				line->escape = 0;
				line->error = UART_LINE_CORRUPTED;
			}
			continue;
		}

		if (c == 0x1B)
		{
			line->escape = 1;
			continue ;
		}

		// ater an error, discard until EOF
		if (line->error != UART_LINE_WAIT)
			continue ;

		// DEL
		if (c == 127 || c == '\b')
		{
			if (line->offset)
			{
				line->offset--;
				line->buffer[line->offset] = '\0';
				uart_write_bytes((const uint8_t *)"\b \b", 3);
			}
			continue ; 
		}

		// reject non printable
		if (c < 32 || c > 126)
		{
			line->error = UART_LINE_CORRUPTED;
			continue ;
		}

		// out of bounds you decide if you accept what remains or not
		if (line->offset >= line->capacity - 1)
		{
			line->error = UART_LINE_OVERFLOW;
			continue ;
		}
		
		// add to buffer and print
		uart_write(c);
		line->buffer[line->offset] = (char)c;
		line->offset++;
	}

	return (UART_LINE_WAIT);
}