/*
 * main.c
 *
 * Created: 09. 06. 2022
 * Author : Trax
 */

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include <string.h>
#include <math.h>
#include <util/delay.h>

#include "main.h"

// ticker timer
volatile uint64_t milliseconds = 0;
volatile uint64_t delay_milliseconds = 0;

// ISR led blinked related
volatile uint8_t isr_led_blinker = 0;
volatile uint16_t isr_led_blinker_tmr = 0;
volatile uint16_t isr_led_blinker_rate = ISR_LED_BLINK_NORMAL_MS;

int main(void)
{
	// Misc hardware init
	// this turns off all outputs & leds
	misc_hw_init();

	// UART init
	setInput(DDRD, 0);
	setOutput(DDRD, 1);
	uart_init(calc_UBRR(19200));

	// I2C init
	setInput(DDRC, 4); // SDA data pin input
	setOutput(PORTC, 4); // turn ON internal pullup
	setInput(DDRC, 5); // SCL clock pin input
	setOutput(PORTC, 5); // turn ON internal pullup
	twi_init(calc_TWI(400000)); // Hz

	// Initialize Timer0 overflow ISR for 1ms interval
	TCCR0A = 0;
	TCCR0B = _BV(CS01) | _BV(CS00); // 1:64 prescaled, timer started! at 16MHz this will overflow at 1.024ms
	TIMSK0 |= _BV(TOIE0); // for ISR(TIMER0_OVF_vect)

	// Interrupts ON
	// Note: global interrupts should NOT be disabled, everything is relying on them from now on!
	sei();

	delay_ms_(50);

	#ifdef DEBUG
	char tmp[128];
	#endif

	// DEBUGGING: todo
	#ifdef DEBUG

	#endif

	uart_puts("RESUME>\r\nEND>\r\n");

	// main program
	while(1) {

		#ifdef DEBUG
		uart_puts("YO!\r\n");
		delay_ms_(500);
		#endif

	}

}

void misc_hw_init() {
	// Buttons S0..S3
	BTNS0_DDR &= ~_BV(BTNS0_PIN); 						// pin is input
	BTNS0_PORT |= _BV(BTNS0_PIN); 						// turn ON internal pullup
	BTNS0_PCMSKREG |= _BV(BTNS0_PCINTBIT); 				// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(BTNS0_PCICRBIT); 						// enable wanted PCICR

	BTNS1_DDR &= ~_BV(BTNS1_PIN); 						// pin is input
	BTNS1_PORT |= _BV(BTNS1_PIN); 						// turn ON internal pullup
	BTNS1_PCMSKREG |= _BV(BTNS1_PCINTBIT); 				// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(BTNS1_PCICRBIT); 						// enable wanted PCICR

	BTNS2_DDR &= ~_BV(BTNS2_PIN); 						// pin is input
	BTNS2_PORT |= _BV(BTNS2_PIN); 						// turn ON internal pullup
	BTNS2_PCMSKREG |= _BV(BTNS2_PCINTBIT); 				// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(BTNS2_PCICRBIT); 						// enable wanted PCICR

	BTNS3_DDR &= ~_BV(BTNS3_PIN); 						// pin is input
	BTNS3_PORT |= _BV(BTNS3_PIN); 						// turn ON internal pullup
	BTNS3_PCMSKREG |= _BV(BTNS3_PCINTBIT); 				// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(BTNS3_PCICRBIT); 						// enable wanted PCICR

	// Optocouplers S0..S3
	setOutput(S0_DDR, S0_PIN);
	setLow(S0_PORT, S0_PIN);

	setOutput(S1_DDR, S1_PIN);
	setLow(S1_PORT, S1_PIN);

	setOutput(S2_DDR, S2_PIN);
	setLow(S2_PORT, S2_PIN);

	setOutput(S3_DDR, S3_PIN);
	setLow(S3_PORT, S3_PIN);

	// LEDs
	setOutput(LEDA_DDR, LEDA_PIN);
	setLow(LEDA_PORT, LEDA_PIN);

	setOutput(LEDB_DDR, LEDB_PIN);
	setLow(LEDB_PORT, LEDB_PIN);

	setOutput(LEDC_DDR, LEDC_PIN);
	setLow(LEDC_PORT, LEDC_PIN);
}

//////////////////////////////////// LED_HELPERS

// LED helpers
void leda_on() {
	setHigh(LEDA_PORT, LEDA_PIN);
}
void leda_off() {
	setLow(LEDA_PORT, LEDA_PIN);
}
void leda_blink(uint8_t times) {
	while(times--) {
		setHigh(LEDA_PORT, LEDA_PIN);
		delay_builtin_ms_(100);
		setLow(LEDA_PORT, LEDA_PIN);
		delay_builtin_ms_(350);
	}
}
void led_isrblink(uint8_t isr_led_mask, uint16_t rate) {
	if(!rate) {
		isr_led_blinker &= ~isr_led_mask;
	}
	else {
		isr_led_blinker |= isr_led_mask;
		isr_led_blinker_rate = rate;
		isr_led_blinker_tmr = rate;
	}
}

//////////////////////////////////// END: LED_HELPERS

// interrupt based delay function
void delay_ms_(uint64_t ms) {
	delay_milliseconds = ms;

	// waiting until the end...
	while(delay_milliseconds > 0);
}

// built-in delay wrapper
void delay_builtin_ms_(uint16_t delay_ms) {
	while(delay_ms--) {
		_delay_ms(1);
	}
}

//##############################
// Interrupt: TIMER0 OVERFLOW //
//##############################
// set to overflow at 1.024 millisecond
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
	milliseconds++; // these are actually 1.024ms

	if(delay_milliseconds) delay_milliseconds--;	// for the isr based delay
	if(btn_hold_timer) btn_hold_timer--;			// to detect button holds, we shouldn't overflow!
	if(btn_expect_timer) btn_expect_timer--;		// to detect button idling
	if(action_expecter_timer) action_expecter_timer--; // for expecting misc actions

	// blinking LEDs from ISR. limitation: all leds can blink at the same rate at any time
	if(isr_led_blinker) {
		isr_led_blinker_tmr--;
		// time to toggle LED(s)?
		if (isr_led_blinker_tmr == 0) {
			isr_led_blinker_tmr = isr_led_blinker_rate; // reload for toggling
			if (isr_led_blinker & ISR_LED_A_MASK) {
				togglePin(LEDA_PORT, LEDA_PIN);
			}
			if (isr_led_blinker & ISR_LED_B_MASK) {
				togglePin(LEDB_PORT, LEDB_PIN);
			}
			if (isr_led_blinker & ISR_LED_C_MASK) {
				togglePin(LEDC_PORT, LEDC_PIN);
			}
		}
	}
}

// Interrupt: pin change interrupt
// FOR BUTTONS
ISR(PCINT2_vect, ISR_NOBLOCK) { // this interrupt routine can be interrupted by any other, and MUST BE! - not anymore, since we are using built-in delay from now
	PCICR &= ~_BV(PCIE2); 								// ..disable interrupts for the entire section


	// bla


	PCICR |= _BV(PCIE2); 								// ..re-enable interrupts for the entire section
}
