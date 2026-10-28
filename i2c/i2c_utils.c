#include "i2c.h"
#include "i2c_internal.h"

void	i2c_start(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
}

void	i2c_stop(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO);

	g_i2c.state = I2C_STATE_STOP;
}

void	i2c_answer(bool ack)
{
	if (ack)
		TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
	else
		TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
}

void	i2c_continue(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
}

void	i2c_disable(void)
{
	// 22.9.2 diable what was set in i2c_init()
	TWCR = 0;

	// 22.5.1 pull up resistor down
	PORTC &= ~(_BV(PC4) | _BV(PC5));
}

void	i2c_release_bus(void)
{
	TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);

	g_i2c.state = I2C_STATE_DONE;
}

uint8_t	make_slarw(uint8_t addr, uint8_t rw)
{
	return ((addr << 1) | rw);
}
