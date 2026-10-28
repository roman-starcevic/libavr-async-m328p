#include "spi.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// CPOL / CPHA
#if SPI_MODE == 0
# define SPI_MODE_BITS 0
#elif SPI_MODE == 1
# define SPI_MODE_BITS _BV(CPHA)
#elif SPI_MODE == 2
# define SPI_MODE_BITS _BV(CPOL)
#elif SPI_MODE == 3
# define SPI_MODE_BITS (_BV(CPOL) | _BV(CPHA))
#else
# error "SPI_MODE must be 0, 1, 2, or 3"
#endif

// SCK divisors
#if SPI_CLOCK_DIV == 2
# define SPI_SPCR_CLOCK 0
# define SPI_SPSR_CLOCK _BV(SPI2X)
#elif SPI_CLOCK_DIV == 4
# define SPI_SPCR_CLOCK 0
# define SPI_SPSR_CLOCK 0
#elif SPI_CLOCK_DIV == 8
# define SPI_SPCR_CLOCK _BV(SPR0)
# define SPI_SPSR_CLOCK _BV(SPI2X)
#elif SPI_CLOCK_DIV == 16
# define SPI_SPCR_CLOCK _BV(SPR0)
# define SPI_SPSR_CLOCK 0
#elif SPI_CLOCK_DIV == 32
# define SPI_SPCR_CLOCK _BV(SPR1)
# define SPI_SPSR_CLOCK _BV(SPI2X)
#elif SPI_CLOCK_DIV == 64
# define SPI_SPCR_CLOCK _BV(SPR1)
# define SPI_SPSR_CLOCK 0
#elif SPI_CLOCK_DIV == 128
# define SPI_SPCR_CLOCK (_BV(SPR1) | _BV(SPR0))
# define SPI_SPSR_CLOCK 0
#else
# error "Unsupported SPI_CLOCK_DIV (use 2, 4, 8, 16, 32, 64 or 128)"
#endif

typedef enum enum_spi_state
{
	SPI_STATE_DISABLED = 0,
	SPI_STATE_IDLE,
	SPI_STATE_LOADING,
	SPI_STATE_BUSY,
	SPI_STATE_DONE
}			e_spi_state;

static volatile uint8_t s_state; // 8bit access is atomic on AVR
static volatile uint8_t s_size;
static volatile uint8_t s_next;
static uint8_t          s_data[SPI_BUFFER_SIZE]; // non volatile purpusly private buffer

void			spi_init(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		s_state = SPI_STATE_DISABLED;

		PRR &= (uint8_t)~_BV(PRSPI); // power reduction reg
		SPCR = 0; // stop SPI

		PORTB |= _BV(PB2); // SS slave select
		DDRB |= _BV(PB2) | _BV(PB3) | _BV(PB5); // input // SS(PB2) // MOSI (PB3) // SCK (PB5)
		DDRB &= (uint8_t)~_BV(PB4); // MISO output 

		SPSR = SPI_SPSR_CLOCK; // set clock

		// clear SPIF/WCOL status
		(void)SPSR;
		(void)SPDR;

		// control reg
		SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPIE) | SPI_MODE_BITS | SPI_SPCR_CLOCK;

		// init static [volatile ?] data
		s_size = 0;
		s_next = 0;
		s_state = SPI_STATE_IDLE;
	}
}

e_spi_request	spi_write_async(const uint8_t *data, uint8_t len)
{
	e_spi_request result = SPI_REQ_BUSY;

	if (!data || !len)
		return (SPI_REQ_INVALID);
	if (len > SPI_BUFFER_SIZE)
		return (SPI_REQ_OVERFLOW);

	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if (s_state == SPI_STATE_IDLE)
		{
			s_state = SPI_STATE_LOADING;
			result = SPI_REQ_OK;
		}
		else if (s_state == SPI_STATE_DISABLED)
			result = SPI_REQ_NOT_READY;
	}

	if (result != SPI_REQ_OK)
		return (result);

	for (uint8_t i = 0; i < len; ++i)
		s_data[i] = data[i];

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		s_size = len;
		s_next = 1;
		s_state = SPI_STATE_BUSY;

		// writing to SPDR start the transfer
		SPDR = s_data[0];
	}

	return (SPI_REQ_OK);
}

bool			spi_is_busy(void)
{
	return (s_state == SPI_STATE_LOADING || s_state == SPI_STATE_BUSY);
}

bool			spi_is_ready(void)
{
	return (s_state == SPI_STATE_IDLE);
}

bool			spi_take_done(void)
{
	bool done = false;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if (s_state == SPI_STATE_DONE)
		{
			s_state = SPI_STATE_IDLE;
			done = true;
		}
	}
	return (done);
}

ISR(SPI_STC_vect)
{
	// this for now anly a tx driver so i discard any receptions 
	// this also clear SPIF/WCOL status
	(void)SPSR;
	(void)SPDR;

	if (s_state != SPI_STATE_BUSY)
		return ;

	if (s_next < s_size)
	{
		SPDR = s_data[s_next];
		s_next++;
	}
	else
		s_state = SPI_STATE_DONE;
}
