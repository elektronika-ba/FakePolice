/*
 * library_pins.h
 *
 * Created: 09. 06. 2022
 *  Author: Trax
 */

#ifndef LIBRARY_PINS_H_
#define LIBRARY_PINS_H_

// nRF24L01 library definitions
// CE PIN
#define nrf24l01_CE_DDR			DDRB
#define nrf24l01_CE_PORT		PORTB
#define nrf24l01_CE				0
// CSN PIN
#define nrf24l01_CSN_DDR		DDRD
#define nrf24l01_CSN_PORT		PORTD
#define nrf24l01_CSN			7

#endif /* LIBRARY_PINS_H_ */