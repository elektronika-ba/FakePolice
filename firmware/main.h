/*
 * main.h
 *
 * Created: 09. 06. 2022
 *  Author: Trax
 */

#ifndef MAIN_H_
#define MAIN_H_

#define DEBUG 1

#define SECRET_KEELOQ_KEY 0x1223344556788990 // chose a secret code for yourself

// Limitations: ATmega328P (and family) PORTB.4 can only be used as INPUT if SPI mode is enabled.

#ifndef F_CPU
	#error You must define CPU frequency (F_CPU) in AVR Studio (Project Properties -> Toolchain -> Symbols >> add F_CPU=8000000UL) or your Makefile
#endif

#ifndef _BV
	#define _BV(n) (1 << n)
#endif

#define	calc_UBRR(bau)			(F_CPU/16/bau-1)
#define calc_TWI(khz)			(((F_CPU/khz) - 16)/2)

#define nop() asm volatile("nop")

// Default RF channel
#define	DEFAULT_RF_CHANNEL		14		// this is system's default RF channel

#define NRF_POLL_TMR_MS			300		// every 300ms poll data from nRF chip

#define LOWEST_BATT_VOLT_MV		3750	// mV lowest battery voltage allowed for blinking the LEDs

// RF Commands (just random values known to both sides)
#define RF_CMD_ABORT			0x2496	// aborts current command
#define RF_CMD_POLICE			0x7683	// police lights
#define RF_CMD_CAMERA			0x5628	// speed camera flash

// ADC pins

// PV cell voltage
#define	SOL_VOLT_PIN			5
#define	SOL_VOLT_DDR			DDRC
#define	SOL_VOLT_DIDR			DIDR0
#define	SOL_VOLT_DIDR_VAL		0b00100000 // ADC5 digitals disabled
#define	SOL_VOLT_ADMUX_VAL		0b00000101 // AREF (internal Vref turned off), ADC5

// Booster voltage
#define	BOOST_VOLT_PIN			4
#define	BOOST_VOLT_DDR			DDRC
#define	BOOST_VOLT_DIDR			DIDR0
#define	BOOST_VOLT_DIDR_VAL		0b00010000 // ADC4 digitals disabled
#define	BOOST_VOLT_ADMUX_VAL	0b00000100 // AREF (internal Vref turned off), ADC4

// Battery voltage
#define	BATT_VOLT_PIN			6
#define	BATT_VOLT_DDR			DDRC
#define	BATT_VOLT_DIDR			DIDR0
//#define	BATT_VOLT_DIDR_VAL		// no digital pin for PortC.6
#define	BATT_VOLT_ADMUX_VAL		0b00000110 // AREF (internal Vref turned off), ADC6

// Temperature value
#define	TEMP_C_PIN				2
#define	TEMP_C_DDR				DDRC
#define	TEMP_C_DIDR				DIDR0
#define	TEMP_C_DIDR_VAL			0b00000100 // ADC2 digitals disabled
#define	TEMP_C_ADMUX_VAL		0b00000010 // AREF (internal Vref turned off), ADC2

// Digital pins

// Charging battery status INPUT pin
#define	BATT_CHRG_PIN			3
#define	BATT_CHRG_DDR			DDRC
#define	BATT_CHRG_PINREG		PINC
#define	BATT_CHRG_PORT			PORTC
#define	BATT_CHRG_PCINTBIT		PCINT11
#define	BATT_CHRG_PCMSKREG		PCMSK1
#define	BATT_CHRG_PCICRBIT		PCIE1

// LED blue OUTPUT pin
#define	LED_BLUE_PIN			6
#define	LED_BLUE_DDR			DDRD
#define	LED_BLUE_PORT			PORTD

// LED red OUTPUT pin
#define	LED_RED_PIN				5
#define	LED_RED_DDR				DDRD
#define	LED_RED_PORT			PORTD

// Temperature measurement IC shutdown OUTPUT pin
#define	TEMP_SHUT_PIN			0
#define	TEMP_SHUT_DDR			DDRC
#define	TEMP_SHUT_PORT			PORTC

// NOTE: nRF24L01 pins are defined in "./library_config/library_pins.h"
// except for optional IRQ pin which is here
// nRF24L01 IRQ PIN
#define nrf24l01_IRQ_DDR		DDRB
#define nrf24l01_IRQ_PORT		PORTB
#define nrf24l01_IRQ_PIN		1
#define	nrf24l01_IRQ_PCINTBIT	PCINT5
#define	nrf24l01_IRQ_PCMSKREG	PCMSK0
#define	nrf24l01_IRQ_PCICRBIT	PCIE0

// Code macros
#define setInput(ddr, pin)		( (ddr) &= (uint8_t)~_BV(pin) )
#define setOutput(ddr, pin)		( (ddr) |= (uint8_t)_BV(pin) )

#define set0(port, pin)			( (port) &= (uint8_t)~_BV(pin) )
#define setLow(port, pin)		( (port) &= (uint8_t)~_BV(pin) )

#define set1(port, pin)			( (port) |= (uint8_t)_BV(pin) )
#define setHigh(port, pin)		( (port) |= (uint8_t)_BV(pin) )

#define togglePin(port, pin)	( (port) ^= (uint8_t)_BV(pin) )

// Boolean macros
#define	BSYS					0										// bank index[0] name.. used for system work
#define	BAPP1					1										// bank index[1] name.. used for application work
#define BOOL_BANK_SIZE			2										// how many banks (bytes)
#define bv(bit,bank) 			( bools[bank] & (uint8_t)_BV(bit) ) 	// boolean values GET/READ macro: if(bv(3,BSYS))...
#define	bs(bit,bank) 			( bools[bank] |= (uint8_t)_BV(bit) ) 	// boolean values SET macro: bs(5,BSYS)
#define	bc(bit,bank) 			( bools[bank] &= (uint8_t)~_BV(bit) ) 	// boolean values CLEAR macro: bc(4,BAPP1)
#define	bt(bit,bank) 			( bools[bank] ^= (uint8_t)_BV(bit) ) 	// boolean values TOGGLE macro: bt(6,BAPP1)

// Custom boolean bit/flags, 0..7 in bank 1 - SYSTEM SPECIFIC
#define	bUART_RX				0	// received byte is valid (1) or not (0)

#define	DATE_Y					0		// for the RTC[] array
#define	DATE_M					1		// for the RTC[] array
#define	DATE_D					2		// for the RTC[] array
#define	TIME_H					3		// for the RTC[] array
#define	TIME_M					4		// for the RTC[] array
#define	TIME_S					5		// for the RTC[] array
#define DATE_W					6		// for the RTC[] array

// misc stuff
extern void misc_hw_init();
extern void delay_ms_(uint64_t);
extern void delay_builtin_ms_(uint16_t);
extern uint8_t isleapyear(uint16_t); // calculate if given year is leap year

// main app stuff
extern uint8_t next_within_window(uint16_t, uint16_t, uint16_t);
float read_adc_mv(uint8_t admux_val, uint32_t Rup, uint32_t Rdn, uint8_t how_many);

#endif /* MAIN_H_ */