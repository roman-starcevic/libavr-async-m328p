#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define F_SCL			100000UL // 100KHz
#define SCL_PRESCALER	1

#define I2C_BUFFER_SIZE	32
#define I2C_TIMEOUT_MS	50UL

#define I2C_STATE_READY	0
#define I2C_STATE_MRX	1
#define I2C_STATE_MTX	2
#define I2C_STATE_STOP	3
#define I2C_STATE_DONE	4
#define I2C_STATE_FAULT	5

#define I2C_NO_ERROR		0xFF
#define I2C_INTERNAL_ERROR	0xFE
#define I2C_TIMEOUT_ERROR	0xFD

#define I2C_BUS_MASK		(_BV(PC4) | _BV(PC5))

typedef enum		enum_i2c_op
{
	I2C_OP_NONE = 0,
	I2C_OP_WRITE,
	I2C_OP_READ,
	I2C_OP_WRITE_READ
}					e_i2c_op;

typedef	struct		struct_i2c_watchdog
{
	uint8_t			progress;
	bool			bus_fault;
	uint8_t			seen_progress;
	uint32_t		last_progress_ms;
}					s_i2c_watchdog;

typedef struct		struct_i2c_buffer
{
	uint8_t			buffer[I2C_BUFFER_SIZE];
	uint8_t			offset;
	uint8_t			size;
}					s_i2c_buffer;

typedef struct		struct_i2c
{
	uint8_t			state;
	uint8_t			slarw;
	uint8_t			error;
	bool			stop;
	uint8_t			tmp_size; // will be used to preserve rx_len during a write_read op
	e_i2c_op		op;
	s_i2c_buffer	buf;
	s_i2c_watchdog	watchdog;
}					s_i2c;

extern volatile s_i2c	g_i2c;

void	i2c_start(void);
void	i2c_stop(void);

void	i2c_answer(bool ack);
void	i2c_continue(void);

void	i2c_disable(void);
void	i2c_release_bus(void);

uint8_t	make_slarw(uint8_t addr, uint8_t rw);