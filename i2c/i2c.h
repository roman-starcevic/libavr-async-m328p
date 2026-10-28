#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/twi.h>

typedef enum		enum_i2c_request
{
	I2C_REQ_OK = 0,
	I2C_REQ_BUSY,
	I2C_REQ_INVALID,
	I2C_REQ_OVERFLOW,
	I2C_REQ_BUS_FAULT
}					e_i2c_request;

typedef enum		enum_i2c_result
{
	I2C_RESULT_OK = 0,
	I2C_RESULT_SLA_NACK,
	I2C_RESULT_DATA_NACK,
	I2C_RESULT_ARB_LOST,
	I2C_RESULT_BUS_ERROR,
	I2C_RESULT_TIMEOUT,
	I2C_RESULT_PROTOCOL_ERROR
}					e_i2c_result;

void				i2c_init(void);
void				i2c_service(uint32_t now_ms);

e_i2c_request		i2c_write(uint8_t addr, const uint8_t *data, uint8_t len);
e_i2c_request		i2c_read(uint8_t addr, uint8_t len);
e_i2c_request		i2c_write_read(uint8_t addr, const uint8_t *tx, uint8_t tx_len, uint8_t rx_len);

bool				i2c_take_write_result(uint8_t addr, e_i2c_result *res);
bool				i2c_take_read_result(uint8_t addr, e_i2c_result *res, uint8_t *dst, uint8_t dst_len);
