/*
 * main.h
 *
 * Created: 09. 06. 2022
 *  Author: Trax
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "lib/uart/uart.h"
#include "lib/i2c/i2c.h"

#define DEBUG 1

// Limitations: ATmega328P (and family) PORTB.4 can only be used as INPUT if SPI mode is enabled.

#ifndef F_CPU
	#error You must define CPU frequency (F_CPU) in AVR Studio (Project Properties -> Toolchain -> Symbols >> add F_CPU=16000000UL) or your Makefile
#endif

#ifndef _BV
	#define _BV(n) (1 << n)
#endif

#define	calc_UBRR(bau)			(F_CPU/16/bau-1)
#define calc_TWI(khz)			(((F_CPU/khz) - 16)/2)

#define nop() asm volatile("nop")

// RF IN data pin
#define	RX_PIN			2
#define	RX_DDR			DDRB
#define	RX_PINREG		PINB
#define	RX_PORT			PORTB
#define	RX_PCINTBIT		PCINT2
#define	RX_PCMSKREG		PCMSK0
#define	RX_PCICRBIT		PCIE0

// RF OUT data pin PORTB.1 (OC1A pin is data output)
#define	TX_PIN			1
#define	TX_DDR			DDRB
#define	TX_PORT			PORTB

// OPTOCOUPLERs outputs S0..S3
#define	S0_PIN			2
#define	S0_DDR			DDRC
#define	S0_PORT			PORTC

// BUTTONs inputs
#define	BTNS0_PIN			2
#define	BTNS0_DDR			DDRD
#define	BTNS0_PINREG		PIND
#define	BTNS0_PORT			PORTD
#define	BTNS0_PCINTBIT		PCINT18
#define	BTNS0_PCMSKREG		PCMSK2
#define	BTNS0_PCICRBIT		PCIE2

#define	BTNS1_PIN			3
#define	BTNS1_DDR			DDRD
#define	BTNS1_PINREG		PIND
#define	BTNS1_PORT			PORTD
#define	BTNS1_PCINTBIT		PCINT19
#define	BTNS1_PCMSKREG		PCMSK2
#define	BTNS1_PCICRBIT		PCIE2

#define	BTNS2_PIN			4
#define	BTNS2_DDR			DDRD
#define	BTNS2_PINREG		PIND
#define	BTNS2_PORT			PORTD
#define	BTNS2_PCINTBIT		PCINT20
#define	BTNS2_PCMSKREG		PCMSK2
#define	BTNS2_PCICRBIT		PCIE2

#define	BTNS3_PIN			5
#define	BTNS3_DDR			DDRD
#define	BTNS3_PINREG		PIND
#define	BTNS3_PORT			PORTD
#define	BTNS3_PCINTBIT		PCINT21
#define	BTNS3_PCMSKREG		PCMSK2
#define	BTNS3_PCICRBIT		PCIE2

// for prev_btn & btn_hold global variables
#define BTNS0_MASK			0b00000001
#define BTNS1_MASK			0b00000010
#define BTNS2_MASK			0b00000100
#define BTNS3_MASK			0b00001000

// LEDs
#define	LEDA_PIN		0
#define	LEDA_DDR		DDRB
#define	LEDA_PORT		PORTB

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

// masks for the ISR LED blinker
#define ISR_LED_A_MASK			0b00000001

#define ISR_LED_BLINK_XFAST_MS	100
#define ISR_LED_BLINK_FAST_MS	150
#define ISR_LED_BLINK_NORMAL_MS	400
#define ISR_LED_BLINK_SLOW_MS	850

// misc stuff
void misc_hw_init();
void delay_ms_(uint64_t);
void delay_builtin_ms_(uint16_t);

// LED helpers
void leda_on();
void leda_off();
void leda_blink(uint8_t);

// main app stuff

#endif /* MAIN_H_ */