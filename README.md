
# libavr-async-m328p

Lightweight, interrupt-driven communication drivers for the ATmega328P, written in C.

The library provides non-blocking APIs for UART, I²C (TWI), and SPI, allowing data transfers to run through hardware interrupts while the main loop remains available for other tasks.

## Features

- **UART** — Interrupt-driven RX/TX with ring buffers, error handling, and a line-oriented input API.
- **I²C (TWI)** — Asynchronous master read/write transactions, repeated START support, timeout detection, and bus error handling.
- **SPI** — Asynchronous master transmission using an internal buffer and interrupt-based completion tracking.

## Design

Each driver operates independently, using fixed-size buffers and direct register access.

Transfers are managed through interrupt service routines (ISRs) and finite-state machines (FSMs).

No RTOS, dynamic memory allocation, or external framework is required.

**Target:** ATmega328P · AVR-GCC · avr-libc

## Example

```c
#include "uart/uart.h"
#include "i2c/i2c.h"
#include "spi/spi.h"

// User provided millisecond clock
extern uint32_t millis(void);

int main(void)
{
	uint8_t byte;

	uart_init(UBRR_VALUE);
	i2c_init();
	spi_init();

	sei();

	while (1)
	{
		i2c_service(millis());

		// Non-blocking UART echo
		if (uart_read(&byte))
			uart_write(byte);

		// UART and SPI FSM will advance and 
		// can be checked in the following section
	}
}
```

## Sources

This project is based on the following official datasheets, application notes, and reference libraries.

| Reference | Description | Link |
| :--- | :--- | :---: |
| **ATmega328P Datasheet** | Microchip reference documentation for hardware peripherals, registers, and interrupts. | [PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061A.pdf) |
| **AVR-LibC** | Standard C library and hardware-specific utilities for 8-bit AVR microcontrollers. | [GitHub](https://github.com/avrdudes/avr-libc) |
| **Microchip AN2480 (AVR315)** | Application note covering interrupt-driven I²C master communication using the AVR TWI peripheral. | [PDF](https://ww1.microchip.com/downloads/en/Appnotes/00002480A.pdf) |
| **NXP AN4803** | Application note describing non-blocking I²C communication, state management, and error handling. | [PDF](https://www.nxp.com/docs/en/application-note/AN4803.pdf) |
