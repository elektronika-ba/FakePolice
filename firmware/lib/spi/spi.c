#include <avr/io.h>

#include "spi.h"

// Init the SPI port
void SPI_init()
{
    SPI_DDR &= (uint8_t)~_BV(SPI_MISO);		// MISO - input
    SPI_DDR |= (uint8_t)_BV(SPI_MOSI);		// MOSI - output
    SPI_DDR |= (uint8_t)_BV(SPI_SCK);		// SCK - output
    SPI_DDR |= (uint8_t)_BV(SPI_SS);		// SS - output - must remain OUTPUT !

    SPCR = ((1<<SPE)|               		// SPI Enable
            (0<<SPIE)|              		// SPI Interupt Enable
            (0<<DORD)|              		// Data Order (0:MSB first / 1:LSB first)
            (1<<MSTR)|              		// Master/Slave select
            (1<<SPR1)|(1<<SPR0)|    		// SPI Clock Rate -------- SLOWEST
            (0<<CPOL)|              		// Clock Polarity (0:SCK low / 1:SCK hi when idle)
            (0<<CPHA));             		// Clock Phase (0:leading / 1:trailing edge sampling)

    SPSR = (0<<SPI2X); 						// Double SPI Speed Bit ------- SLOWEST
}

// SPI transfer
uint8_t SPI_rw(uint8_t data)
{
    SPDR = data;

    while( !(SPSR & (1<<SPIF)) );

    return SPDR;
}
