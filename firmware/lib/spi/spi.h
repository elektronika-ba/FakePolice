#ifndef _SPI_H_
#define _SPI_H_

#include <avr/io.h>

// README: (for ATmega88/168/328P/...) PORTB.4 can only be used as INPUT if SPI mode is enabled.

// SPI port pins
#define SPI_DDR		DDRB
#define SPI_PORT	PORTB
#define SPI_SS		2		// this pin must be OUTPUT for SPI system to remain in MASTER MODE !!!
#define SPI_MOSI	3
#define SPI_MISO	4
#define SPI_SCK		5
// README: Slave-Select pin is not defined here, it is defined within slave's header files

extern void SPI_init();
extern uint8_t SPI_rw(uint8_t data);

#endif /* _SPI_H_ */
