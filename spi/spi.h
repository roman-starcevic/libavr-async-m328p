#pragma once

#include <stdbool.h>
#include <stdint.h>

// compile time conf
#ifndef SPI_BUFFER_SIZE
# define SPI_BUFFER_SIZE 32
#endif

#ifndef SPI_MODE
# define SPI_MODE 0
#endif

#ifndef SPI_CLOCK_DIV
# define SPI_CLOCK_DIV 16
#endif

#if SPI_BUFFER_SIZE < 1 || SPI_BUFFER_SIZE > 255
# error "SPI_BUFFER_SIZE must be in [1, 255]"
#endif

typedef enum	enum_spi_request
{
	SPI_REQ_OK = 0,
	SPI_REQ_BUSY,
	SPI_REQ_INVALID,
	SPI_REQ_OVERFLOW,
	SPI_REQ_NOT_READY
}				e_spi_request;

/*
	init as SPI MASTER, MSB first
	use SPI_MODE and SPU_CLOCK to selet the desire settings
*/
void          spi_init(void);

/*
	copy data to a private buffer
	start transfer
	non blocking return immediatly

	can only accept a new transfer if the previous result was conumed
*/
e_spi_request spi_write_async(const uint8_t *data, uint8_t len);

bool          spi_is_busy(void);   /* Copying or transmitting. */
bool          spi_is_ready(void);  /* A new transfer can start. */
bool          spi_take_done(void); /* True once per completed transfer. */
