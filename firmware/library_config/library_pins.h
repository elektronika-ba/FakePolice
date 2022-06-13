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
#define nrf24l01_CE_DDR			DDRD
#define nrf24l01_CE_PORT		PORTD
#define nrf24l01_CE				7
// CSN PIN
#define nrf24l01_CSN_DDR		DDRD
#define nrf24l01_CSN_PORT		PORTD
#define nrf24l01_CSN			6
// IRQ PIN
#define nrf24l01_IRQ_DDR		DDRD
#define nrf24l01_IRQ_PORT		PORTD
#define nrf24l01_IRQ			5

#endif /* LIBRARY_PINS_H_ */