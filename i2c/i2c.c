#include "i2c.h"
#include "i2c_internal.h"

volatile s_i2c	g_i2c;

/*
found fonctional algo : https://www.idreammicro.com/post/Configuration-du-Bit-Rate-Generator-Unit

	 	F_CPU - (16 * F_SCL)
TWBR =	------------------
		2 * F_SCL * PRESCALER

the site says to privilege prescaler to 1, no proof found in datasheet
*/
void			i2c_init(void)
{
	// init global meta data
	g_i2c.state = I2C_STATE_READY;
	g_i2c.slarw = 0;
	g_i2c.error = I2C_NO_ERROR;
	g_i2c.stop = true;
	g_i2c.op = I2C_OP_NONE;
	// buffer
	g_i2c.buf.offset = 0;
	g_i2c.buf.size = 0;
	// watchdog
	g_i2c.watchdog.progress = 0;
	g_i2c.watchdog.bus_fault = false;
	g_i2c.watchdog.seen_progress = 0;
	g_i2c.watchdog.last_progress_ms = 0;


	PRR &= ~_BV(PRTWI); // power reduction reg

	// SDA = PC4, SCL = PC5
	
	// data direction
	DDRC &= ~(_BV(PC4) | _BV(PC5));
	// 22.5.1 pull up resistor
	PORTC |= _BV(PC4) | _BV(PC5);

	// 22.5.2 set bit rate
	TWBR = ((F_CPU - 16 * F_SCL) / (2 * F_SCL * SCL_PRESCALER));

	// default prescaler to 1
	TWSR = 0;

	// 22.9.2 activate enable TWI on Control reg
	// enable interrupt
	TWCR = _BV(TWEN) | _BV(TWIE);
}

void			i2c_service(uint32_t now_ms)
{
	uint8_t	state;
	uint8_t	progress;

	// atomic snapshot of isr shared data
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		state = g_i2c.state;
		progress = g_i2c.watchdog.progress;
	}

	// recover if previously stuck bus is now idle
	if (state == I2C_STATE_FAULT)
	{
		if ((PINC & I2C_BUS_MASK) == I2C_BUS_MASK)
		{
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
			{
				if (g_i2c.state == I2C_STATE_FAULT && (PINC & I2C_BUS_MASK) == I2C_BUS_MASK)
				{
					// restore inital state wait for start
					TWCR = _BV(TWEN);
					g_i2c.watchdog.bus_fault = false;
					g_i2c.state = I2C_STATE_READY;
				}
			}
		}

		return ;
	}

	// new hardware activity or transaction so we register it
	if (progress != g_i2c.watchdog.seen_progress)
	{
		g_i2c.watchdog.seen_progress = progress;
		g_i2c.watchdog.last_progress_ms = now_ms;
	}

	// STOP is not notified by an interrupt so we monitor it in a non blocking way
	if (state == I2C_STATE_STOP && !(TWCR & _BV(TWSTO)))
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			if (g_i2c.state == I2C_STATE_STOP && !(TWCR & _BV(TWSTO)))
				g_i2c.state = I2C_STATE_DONE;
		}

		return ;
	}

	// if no transimission or stop is happening we dont have any timout to check on
	if (state != I2C_STATE_MRX && state != I2C_STATE_MTX && state != I2C_STATE_STOP)
		return ;

	// if  timout valid -> return 
	if ((uint32_t)(now_ms - g_i2c.watchdog.last_progress_ms) < I2C_TIMEOUT_MS)
		return ;

	// timout is on 
	// recheck inside atomic section to avoid race condition
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if (g_i2c.state == state && g_i2c.watchdog.progress == progress)
		{
			// stop might have happen in the meantime
			// its legal so we set the STATE_DONE
			if (state == I2C_STATE_STOP && !(TWCR & _BV(TWSTO)))
				g_i2c.state = I2C_STATE_DONE;
			else
			{
				// timout has happen we restore
				TWCR = 0; // diable TWI and Interrupt

				g_i2c.error = I2C_TIMEOUT_ERROR;
				g_i2c.watchdog.bus_fault = (PINC & I2C_BUS_MASK) != I2C_BUS_MASK;

				
				// reset to activate TWI
				if (!g_i2c.watchdog.bus_fault)
					TWCR = _BV(TWEN);
				// else deactivate

				// preserve the error for the caller.
				g_i2c.state = I2C_STATE_DONE;
			}
		}
	}

}

e_i2c_request	i2c_write(uint8_t addr, const uint8_t *data, uint8_t len)
{
	if (addr > 127 || !data || !len)
		return (I2C_REQ_INVALID);

	if (len > I2C_BUFFER_SIZE)
		return (I2C_REQ_OVERFLOW);

	if (g_i2c.state == I2C_STATE_FAULT)
		return (I2C_REQ_BUS_FAULT);

	if (g_i2c.state != I2C_STATE_READY)
		return (I2C_REQ_BUSY);
	
	g_i2c.slarw = make_slarw(addr, TW_WRITE);
	g_i2c.stop = true; // no stop can only happen on rep start situation (not here)
	g_i2c.error = I2C_NO_ERROR; // 0xFF

	// make buffer
	g_i2c.buf.offset = 0;
	g_i2c.buf.size = len;

	// copy data
	for (uint8_t i = 0; i < len; i++)
		g_i2c.buf.buffer[i] = data[i];

	// update state
	g_i2c.state = I2C_STATE_MTX;
	g_i2c.op = I2C_OP_WRITE;

	g_i2c.watchdog.progress++;

	i2c_start();

	return (I2C_REQ_OK);
}

e_i2c_request	i2c_read(uint8_t addr, uint8_t len)
{
	if (addr > 127 || !len)
		return (I2C_REQ_INVALID);

	if (len > I2C_BUFFER_SIZE)
		return (I2C_REQ_OVERFLOW);

	if (g_i2c.state == I2C_STATE_FAULT)
		return (I2C_REQ_BUS_FAULT);

	if (g_i2c.state != I2C_STATE_READY)
		return (I2C_REQ_BUSY);

	g_i2c.slarw = make_slarw(addr, TW_READ);
	g_i2c.stop = true; // no stop can only happen on rep start situation (not here)
	g_i2c.error = I2C_NO_ERROR; // 0xFF

	// make buffer
	g_i2c.buf.offset = 0;
	g_i2c.buf.size = len;

	g_i2c.state = I2C_STATE_MRX;
	g_i2c.op = I2C_OP_READ;

	g_i2c.watchdog.progress++;

	i2c_start();

	return (I2C_REQ_OK);
}

e_i2c_request	i2c_write_read(uint8_t addr, const uint8_t *tx, uint8_t tx_len, uint8_t rx_len)
{
	if (addr > 127 || !tx || !tx_len || !rx_len)
		return (I2C_REQ_INVALID);

	if (tx_len > I2C_BUFFER_SIZE || rx_len > I2C_BUFFER_SIZE)
		return (I2C_REQ_OVERFLOW);

	if (g_i2c.state == I2C_STATE_FAULT)
		return (I2C_REQ_BUS_FAULT);

	if (g_i2c.state != I2C_STATE_READY)
		return (I2C_REQ_BUSY);

	// start MT mode
	g_i2c.slarw = make_slarw(addr, TW_WRITE);

	// switch to RX insteead of stop at end of TX
	g_i2c.stop = false;
	g_i2c.tmp_size = rx_len;
	g_i2c.error = I2C_NO_ERROR;

	// prep tx buffer
	g_i2c.buf.offset = 0;
	g_i2c.buf.size = tx_len;

	// copy write data to internal  buffer
	for (uint8_t i = 0; i < tx_len; i++)
		g_i2c.buf.buffer[i] = tx[i];

	g_i2c.state = I2C_STATE_MTX;
	g_i2c.op = I2C_OP_WRITE_READ;

	g_i2c.watchdog.progress++;

	i2c_start();

	return (I2C_REQ_OK);
}

static e_i2c_result	i2c_decode_error(uint8_t error)
{
	switch (error)
	{
		case I2C_NO_ERROR:
			return (I2C_RESULT_OK);

		case I2C_INTERNAL_ERROR:
			return (I2C_RESULT_PROTOCOL_ERROR);

		case I2C_TIMEOUT_ERROR:
			return (I2C_RESULT_TIMEOUT);

		case TW_MT_SLA_NACK:
		case TW_MR_SLA_NACK:
			return (I2C_RESULT_SLA_NACK);

		// not for MR as last byte will be nacked
		case TW_MT_DATA_NACK:
			return (I2C_RESULT_DATA_NACK);

		// same code for MT and MR
		case TW_MT_ARB_LOST:
			return (I2C_RESULT_ARB_LOST);

		default:
			return (I2C_RESULT_BUS_ERROR);
	}
}

bool			i2c_take_write_result(uint8_t addr, e_i2c_result *res)
{
	if (!res || g_i2c.state != I2C_STATE_DONE || (g_i2c.slarw >> 1) != addr || g_i2c.op != I2C_OP_WRITE)
		return (false);

	*res = i2c_decode_error(g_i2c.error);

	g_i2c.op = I2C_OP_NONE;
	g_i2c.state = g_i2c.watchdog.bus_fault ? I2C_STATE_FAULT : I2C_STATE_READY;
	return (true);
}

bool			i2c_take_read_result(uint8_t addr, e_i2c_result *res, uint8_t *dst, uint8_t dst_len)
{
	e_i2c_result	tmp_res; // to not update *res before error handling

	if (!res || g_i2c.state != I2C_STATE_DONE || (g_i2c.slarw >> 1) != addr || (g_i2c.op != I2C_OP_READ && g_i2c.op != I2C_OP_WRITE_READ))
		return (false);

	tmp_res = i2c_decode_error(g_i2c.error);

	// read only if successfull reception
	if (tmp_res == I2C_RESULT_OK)
	{
		if (!dst || dst_len < g_i2c.buf.size)
			return (false);

		// check size mismatch
		if (g_i2c.buf.offset != g_i2c.buf.size)
			tmp_res = I2C_RESULT_BUS_ERROR;
		else
		{
			// copy internal buffer
			for (uint8_t i = 0; i < g_i2c.buf.size; i++)
				dst[i] = g_i2c.buf.buffer[i];
		}
	}

	*res = tmp_res;
	g_i2c.op = I2C_OP_NONE;
	g_i2c.state = g_i2c.watchdog.bus_fault ? I2C_STATE_FAULT : I2C_STATE_READY;
	return (true);
}

ISR(TWI_vect)
{
	g_i2c.watchdog.progress++;

	switch (TW_STATUS)
	{
		case TW_START:
		case TW_REP_START:
			TWDR = g_i2c.slarw;
			i2c_answer(1);
			break ;

		
		/* MT Master Transmitter */

		case TW_MT_SLA_ACK:
		case TW_MT_DATA_ACK:

				if (g_i2c.buf.offset < g_i2c.buf.size)
				{
					TWDR = g_i2c.buf.buffer[g_i2c.buf.offset++];
					i2c_continue();
				}
				else if (!g_i2c.stop)
				{
					// tx completed prep rx phase
					g_i2c.slarw |= TW_READ; // dont modify addr only r/w flag

					g_i2c.buf.offset = 0;
					g_i2c.buf.size = g_i2c.tmp_size;

					g_i2c.stop = true;
					g_i2c.state = I2C_STATE_MRX;

					// rep start
					i2c_start();
				}
				else
					i2c_stop();

				break ;

		case TW_MT_SLA_NACK:

				g_i2c.error = TW_MT_SLA_NACK;
				i2c_stop();
				break ;

		case TW_MT_DATA_NACK:

				g_i2c.error = TW_MT_DATA_NACK;
				i2c_stop();
				break ;

		case TW_MT_ARB_LOST:

				g_i2c.error = TW_MT_ARB_LOST;
				i2c_release_bus();
				break ;
		
		/* MR Master Receiver */

		case TW_MR_SLA_ACK:

				i2c_answer(g_i2c.buf.size > 1);
				break ;

		case TW_MR_DATA_ACK:

				// an ack is not expected for the last byte (respect i2c protocol fr is it needed, this shall be user controlled)
				if (g_i2c.buf.offset >= g_i2c.buf.size - 1)
				{
					g_i2c.error = I2C_INTERNAL_ERROR;
					i2c_stop();
					break ;
				}

				g_i2c.buf.buffer[g_i2c.buf.offset++] = TWDR;
				i2c_answer(g_i2c.buf.offset < g_i2c.buf.size - 1);
				break ;

		case TW_MR_DATA_NACK:
				
				if (g_i2c.buf.offset != g_i2c.buf.size - 1)
				{
					g_i2c.error = I2C_INTERNAL_ERROR;
					i2c_stop();
					break ;
				}

				g_i2c.buf.buffer[g_i2c.buf.offset++] = TWDR;
				i2c_stop();
				break ;

		case TW_MR_SLA_NACK:

				g_i2c.error = TW_MR_SLA_NACK;
				i2c_stop();
				break ;

		/* Others */

		case TW_BUS_ERROR:

				g_i2c.error = TW_BUS_ERROR;
				i2c_stop();
				break ;

		default:
		
				g_i2c.error = I2C_INTERNAL_ERROR;
				i2c_stop();
				break ;
	}
}
