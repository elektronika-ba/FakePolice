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
#include <avr/power.h>
#include <avr/sleep.h>

// Note: There is a library_pins.h file included globally by including entire directory "library_config". This is set in Project->Properties->Directories
// This is required for the included libraries

#include "main.h"
#include "../lib/uart/uart.h"
#include "../lib/spi/spi.h"
#include "../lib/nrf24l01/nrf24l01.h"
#include "keeloq_crypt.h" // yes, believe it or not :)

// misc
volatile uint8_t charge_event = 0;
volatile uint8_t radio_event = 0;
volatile uint8_t rtc_event = 0;
volatile uint32_t charging_time_sec = 0;

volatile uint32_t telemetry_timer_min = 0;

// rolling code sync value (stored/updated to EEPROM)
volatile uint16_t kl_counter = 100;
volatile uint64_t kl_key = SECRET_KEELOQ_KEY;

// ticker timer
volatile uint64_t delay_milliseconds = 0;
volatile uint16_t timer0rtc_step = 0;							// for RTC

// RTC
volatile uint8_t RTC[7] = {22, 6, 9, 12, 0, 0, 5};			// YY MM DD HH MM SS WEEKDAY(1-MON...7-SUN)
volatile uint8_t bools[BOOL_BANK_SIZE] = { 0, 0 }; 				// 8 global boolean values per BOOL_BANK_SIZE
volatile uint8_t rtc_slow_mode = 0;
volatile uint64_t seconds_counter = 0;

void send_telemetry() {
	float solar_volt = read_solar_volt();
	float boost_volt = read_boost_volt();
	float batt_volt = read_batt_volt();
	float temperature = read_temperature();
	uint8_t batt_charging = !(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN));

	// TODO: pripremi paket i salji
	// imam 26 bajta ukupno za ovo
}

void set_rtc_speed(uint8_t slow) {
	rtc_slow_mode = slow;

	// set prescaller to 128 .. 1second interrupt
	if(slow) {
		TCCR2B &= ~(1<<CS10);
	}
	// set prescaller 1024 .. 8second interrupt
	else {
		TCCR2B |= (1<<CS10);
	}
}

float read_temperature() {
	setHigh(TEMP_SHUT_PORT, TEMP_SHUT_PIN);
	delay_ms_(5);

	float adc = read_adc_mv(TEMP_C_ADMUX_VAL, 0, 0, 16);

	setLow(TEMP_SHUT_PORT, TEMP_SHUT_PIN);

	// TMP36F provides a 750 mV output at 25°C. Output scale factor of 10 mV/°C. offset is 100mV at -40°C
	return (adc - 100) / 10 - 40;
}

float read_solar_volt() {
	return read_adc_mv(SOL_VOLT_ADMUX_VAL, 47000, 47000, 16);
}

float read_boost_volt() {
	return read_adc_mv(BOOST_VOLT_ADMUX_VAL, 47000, 47000, 16);
}

float read_batt_volt() {
	return read_adc_mv(BATT_VOLT_ADMUX_VAL, 47000, 100000, 16);
}

void process_command(uint8_t *rx_buff) {
/*
	// 32bytes maximum{[ROLLING ACCESS CODE 4 bytes][COMMAND 2 bytes][PARAM N bytes]}
	// COMMAND+Counter is also contained within the ROLLING ACCESS CODE

	// decrypt access code
	uint32_t encrypted = 0;
	encrypted |= (uint32_t)rx_buff[3] << 24;
	encrypted |= (uint32_t)rx_buff[2] << 16;
	encrypted |= (uint16_t)rx_buff[1] << 8;
	encrypted |= rx_buff[0];

	keeloq_decrypt(&encrypted, (uint64_t *)&kl_key);

	// it is now decrypted. we need to verify whether this is a valid data or not.
	// there is no MAC, so we treat entire data as MAC. encryption here does not
	// provide secrecy but only message authenticity. that's all we care about really.

	// verify message authenticity
	if(
		next_within_window(100, 100, 32)
		// &&
	)
	{

	}

	uint8_t cmd_count = rx_buff[0];

	while(cmd_count-- > 0) {
		uint8_t *cmd = rx_buff + cmd_index;
		cmd_index++; // move away from the command as we read it
		uint8_t *param = rx_buff + cmd_index; // this is where the optional parameter starts

		switch(*cmd) {
			case RF_CMD_SEGMENT_SET_VALUE:
				SEGMENT_display_number(&segment_ctx, (uint32_t)((uint8_t)(*param)));
				cmd_index += 1; // param was 1 byte
			break;
			case RF_CMD_SEGMENT_SET_IDLE:
				SEGMENT_display_idle(&segment_ctx);
			break;
			case RF_CMD_SEGMENT_SET_ZEROS:
				SEGMENT_display_zeros(&segment_ctx);
			break;
			case RF_CMD_SEGMENT_CLEAR:
				SEGMENT_display_clear(&segment_ctx);
			break;
			case RF_CMD_SET_PROGRESSBAR_VAL:
			{
				uint8_t red = *param;
				param++;
				uint8_t green = *param;
				param++;
				uint8_t blue = *param;
				param++;
				uint8_t value = *param;

				// stupid failsafe
				if(value > 10) value = 10;

				showProgress(red, green, blue, value);

				cmd_index += 4; // param was 4 bytes
			}
			break;
			case RF_CMD_SEGMENT_ANIMATE:
			{
				uint16_t delay = 0;
				memcpy(&delay, param, 2);
				param+=2; // skip the "delay" param, goto "times" param

				uint8_t times = *param;
				param+=1; // skip the "times" param, goto "type" param

				uint8_t type = *param;

				while(times-- > 0) {
					if(type == 0)
						SEGMENT_animate1(&segment_ctx, delay);
					else
						SEGMENT_animate2(&segment_ctx, delay);
				}

				cmd_index += 4; // param was 4 bytes
			}
			break;
			case RF_CMD_SET_OPTOOUTPUT:
			{
				uint8_t state = *param;
				if(state) {
					OUT_PORT |= _BV(OUT_PIN); // turn it on
				}
				else {
					OUT_PORT &= ~_BV(OUT_PIN); // turn it off
				}

				cmd_index += 1; // param was 1 byte
			}
			break;
			case RF_CMD_PULSE_OPTOOUTPUT:
			{
				uint8_t delay = *param;
				while(delay-- > 0) {
					_delay_ms(100);
				}

				cmd_index += 1; // param was 1 byte
			}
			break;
			case RF_CMD_GET_OPTOINPUT:
			{
				uint8_t state = 0;
				if( !(BTNB_PINREG & _BV(BTNB_PIN)) )
				{
					state = 1;
				}
				// salji stanje na RPi
				// TODO send RX packet
			}
			break;
			default:
				return; // samo obustavi
		}
	}
	*/
}

// simulates the speed camera flash
void speed_camera() {
	for(uint8_t j=0; j<2; j++)	{
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(60);
		setLow(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(60);
	}
	delay_builtin_ms_(500);

	for(uint8_t j=0; j<2; j++)	{
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(60);
		setLow(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(60);
	}
}

// simulates police lights
void police_once() {
	for(uint8_t j=0; j<5; j++)	{
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(30);
		setLow(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(30);
	}

	for(uint8_t j=0; j<5; j++)	{
		setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(30);
		setLow(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(30);
	}
}

int main(void)
{
	// Misc hardware init
	// this turns off all outputs & leds
	misc_hw_init();

	// UART init
	setInput(DDRD, 0);
	setOutput(DDRD, 1);
	uart_init(calc_UBRR(19200));

	// Init the SPI port
	SPI_init();

	// nRF24L01 Init
	uint8_t my_rx_addr[5];
	my_rx_addr[0] = 77;
	my_rx_addr[1] = 66;
	my_rx_addr[2] = 44;
	my_rx_addr[3] = 33;
	my_rx_addr[4] = 88;
	nrf24l01_init(DEFAULT_RF_CHANNEL);
	nrf24l01_setrxaddr0(my_rx_addr);
	nrf24l01_setRX();

	// Initialize Timer0 overflow ISR for 1ms interval
	TCCR0A = 0;
	TCCR0B = _BV(CS01) | _BV(CS00); // 1:64 prescaled, timer started!
	TIMSK0 |= _BV(TOIE0); // for ISR(TIMER0_OVF_vect)

	// Initialize Timer2 as async RTC counter @ 1Hz with 32.768kHz crystal
	// https://embedds.com/avr-timer2-asynchronous-mode/
	// https://maxembedded.com/2011/06/avr-timers-timer2/

	// initialize counter
	TCNT2 = 0;
    TCCR2B |= (1<<CS22)|(1<<CS00); // 1 second by default
    //Enable asynchronous mode
    ASSR  = (1<<AS2);
    //wait for registers update
    // while (ASSR & ((1<<TCN2UB)|(1<<TCR2BUB)));
    // enable overflow interrupt
	TIMSK2 |= (1 << TOIE2);

	// Interrupts ON
	// Note: global interrupts should NOT be disabled, everything is relying on them from now on!
	sei();

	delay_builtin_ms_(50);

	uint8_t rx_buff[32];

	// DEBUGGING
	#ifdef DEBUG
	for(uint8_t i=0; i<2; i++)	{

		for(uint8_t j=0; j<5; j++)	{
			setHigh(LED_RED_PORT, LED_RED_PIN);
			delay_builtin_ms_(30);
			setLow(LED_RED_PORT, LED_RED_PIN);
			delay_builtin_ms_(30);
		}

		for(uint8_t j=0; j<5; j++)	{
			setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(30);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(30);
		}
	}
	setLow(LED_BLUE_PORT, LED_BLUE_PIN);
	setLow(LED_RED_PORT, LED_RED_PIN);
	#endif

	// main program
	while(1) {
		/*
		1. configure sleep mode
		// we can be woken up by:
		- Timer2
		- IRQ from nRF radio
		- Battery charge indicator

		2. sleep

		zZzzZZzzZZzzZzzz...

		3. wakeup event!

		4. check what woke us up
		- Timer2 = send telemetry and go back to 1.
		- Battery charge indicator = send telemetry and go back to 1.
		- IRQ from nRF radio = process command and go back to 1.
		*/

		// configure sleep mode
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);

		// go to sleep
		sleep_mode();

		// zzzZZZzzzZZZzzzZZZzzz
		// (some interrupt happens) and we get into the ISR() of it... after it finishes, we continue here:

		// did charge event happen?
		if(charge_event) {

			// charging
			if( !(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN)) ) {
				// ...
			}
			// not charging (anymore)
			else {
				charging_time_sec = 0;
			}

			send_telemetry();

			charge_event = 0;  // handled
		}

		// did radio packet arrive?
		if(radio_event) {

			// verify that RF packet arrived?
			if( nrf24l01_irq_rx_dr() )
			{
				while( !( nrf24l01_readregister(NRF24L01_REG_FIFO_STATUS) & NRF24L01_REG_RX_EMPTY) )
				{
					nrf24l01_read(rx_buff);
					nrf24l01_irq_clear_rx_dr();

					process_command(rx_buff);
				}
			}

			radio_event = 0; // handled
		}

		// RTC has woken us up?
		if(rtc_event) {

			// send telemetry if it is time and if solar voltage is good
			if(!telemetry_timer_min) {
				if(read_solar_volt() >= LOWEST_SOLVOLT_4_TELEM_MV) {
					send_telemetry();
				}
				telemetry_timer_min = TELEMETRY_MINUTES;
			}

			rtc_event = 0; // handled
		}

	}

}

void misc_hw_init() {
	// nRF Radio IRQ
	setInput(nrf24l01_IRQ_DDR, nrf24l01_IRQ_PIN);
	nrf24l01_IRQ_PORT |= _BV(nrf24l01_IRQ_PIN); 						// turn ON internal pullup
	nrf24l01_IRQ_PCMSKREG |= _BV(nrf24l01_IRQ_PCINTBIT); 				// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(nrf24l01_IRQ_PCICRBIT); 								// enable wanted PCICR

	// Battery charging indicator
	setInput(BATT_CHRG_DDR, BATT_CHRG_PIN);
	BATT_CHRG_PORT |= _BV(BATT_CHRG_PIN); 							// turn ON internal pullup
	BATT_CHRG_PCMSKREG |= _BV(BATT_CHRG_PCINTBIT); 					// set (un-mask) PCINTn pin for interrupt on change
	PCICR |= _BV(BATT_CHRG_PCICRBIT); 								// enable wanted PCICR

	// Temp. sensor shutdown pin
	setOutput(TEMP_SHUT_DDR, TEMP_SHUT_PIN);
	setLow(TEMP_SHUT_PORT, TEMP_SHUT_PIN);				// shutdown by default

	// RED LED
	setOutput(LED_RED_DDR, LED_RED_PIN);
	setLow(LED_RED_PORT, LED_RED_PIN);

	// BLUE LED
	setOutput(LED_BLUE_DDR, LED_BLUE_PIN);
	setLow(LED_BLUE_PORT, LED_BLUE_PIN);

	// Solar panel voltage
	setInput(SOL_VOLT_DDR, SOL_VOLT_PIN);
	SOL_VOLT_DIDR |= SOL_VOLT_DIDR_VAL;

	// Booster voltage
	setInput(BOOST_VOLT_DDR, BOOST_VOLT_PIN);
	SOL_VOLT_DIDR |= SOL_VOLT_DIDR_VAL;

	// Battery voltage
	setInput(BATT_VOLT_DDR, BATT_VOLT_PIN);
	BOOST_VOLT_DIDR |= BOOST_VOLT_DIDR_VAL;

	// Battery voltage
	setInput(BATT_VOLT_DDR, BATT_VOLT_PIN);
	// no DIDR for battery voltage measurement pin

	// Temperature voltage/value
	setInput(TEMP_C_DDR, TEMP_C_PIN);
	TEMP_C_DIDR |= TEMP_C_DIDR_VAL;
}

// interrupt based delay function
void delay_ms_(uint64_t ms) {
	delay_milliseconds = ms/2;

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
// Interrupt: TIMER2 OVERFLOW //
//##############################
// This interrupt can happen on every 1s or every 8s
ISR(TIMER2_OVF_vect, ISR_NOBLOCK)
{
	rtc_event = 1;

	uint8_t sec_step = 1;

	// RTC is normal, 1 second interval
	if(!rtc_slow_mode) {

		// TODO: do something every second?
	}
	// RTC advances on each 8 seconds
	else {
		sec_step = 8;

		// TODO: do something every 8 seconds?
	}

	RTC[TIME_S] += sec_step;
	seconds_counter += sec_step; // seconds ticker, for global use

	// measure charge time
	if( !(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN)) ) {
		charging_time_sec += sec_step;
	}

	// a minute!
	if(RTC[TIME_S] >= 60)
	{
		// correction if in slow mode
		if(rtc_slow_mode) {
			RTC[TIME_S] = RTC[TIME_S] - 60; // keep remainder!
		}
		// normal
		else {
			RTC[TIME_S] = 0;
		}

		RTC[TIME_M]++;

		// TODO: do something every minute?
		if(telemetry_timer_min) telemetry_timer_min--;

	} // if(RTC[TIME_S] >= 60)

	// an hour...
	if(RTC[TIME_M] >= 60)
	{
		RTC[TIME_M] = 0;
		RTC[TIME_H]++;

		// TODO: do something every hour
	}

	// a day....
	if(RTC[TIME_H] >= 24)
	{
		RTC[TIME_H] = 0;
		RTC[DATE_D]++;

		// advance weekday
		RTC[DATE_W]++;
		if(RTC[DATE_W] > 7)
		{
			RTC[DATE_W] = 1;
		}
	}

	// a full month with leap year checking!
	if(
		(RTC[DATE_D] > 31)
		|| (
			(RTC[DATE_D] == 31)
			&& (
				(RTC[DATE_M] == 4)
				|| (RTC[DATE_M] == 6)
				|| (RTC[DATE_M] == 9)
				|| (RTC[DATE_M] == 11)
				)
		)
		|| (
			(RTC[DATE_D] == 30)
			&& (RTC[DATE_M] == 2)
		)
		|| (
			(RTC[DATE_D] == 29)
			&& (RTC[DATE_M] == 2)
			&& !isleapyear(2000+RTC[DATE_Y])
		)
	)
	{
		RTC[DATE_D] = 1;
		RTC[DATE_M]++;
	}

	// HAPPY NEW YEAR!
	if(RTC[DATE_M] >= 13)
	{
		RTC[DATE_Y]++;
		RTC[DATE_M] = 1;
	}
}

//##############################
// Interrupt: TIMER0 OVERFLOW //
//##############################
// set to overflow at 2.048 milliseconds
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
	if(delay_milliseconds) delay_milliseconds--;	// for the isr based delay

	// every 1000ms
	if( ++timer0rtc_step >= 488 ) // 999,424 ms
	{
		timer0rtc_step = 0;

		// do something every ~1ms

	} // if( ++timer0rtc_step >= ? )
}

// Interrupt: pin change interrupt
// This one is connected to nRF24L01 IRQ pin only
ISR(PCINT0_vect, ISR_NOBLOCK) {
	sleep_disable();

	PCICR &= ~_BV(PCIE0); 								// ..disable interrupts for the entire section

	radio_event = 1;

	PCICR |= _BV(PCIE0); 								// ..re-enable interrupts for the entire section
}

// Interrupt: pin change interrupt
// This one is connected to Li+ battery charger CHARGE indicator
ISR(PCINT1_vect, ISR_NOBLOCK) {
	sleep_disable();

	PCICR &= ~_BV(PCIE1); 								// ..disable interrupts for the entire section

	charge_event = 1;

	PCICR |= _BV(PCIE1); 								// ..re-enable interrupts for the entire section
}

// calculate if given year is leap year
uint8_t isleapyear(uint16_t y)
{
	return ( ( !(y % 4) && (y % 100) ) || !(y % 400) );
}

// reads average ADC value
float read_adc_mv(uint8_t admux_val, uint32_t Rup, uint32_t Rdn, uint8_t how_many)
{
	uint64_t volt_sum = 0;

	if(how_many==0) how_many++;

	// set ADMUX channel and reference voltage
	ADMUX = admux_val;

	// initialize the ADC circuit
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1); 		// enabled, division factor: 64

	// no effect since ADATE in ADCSRA is = 0
	ADCSRB = 0x00;

	// make "n" measurements, find average value
	for(uint8_t i=0; i<how_many; i++)
	{
		// Start conversion by setting ADSC on ADCSRA Register
		ADCSRA |= _BV(ADSC);

		// wait until conversion complete ADSC=0 -> Complete
		while (ADCSRA & (1<<ADSC));

		// Sum the current voltage
		volt_sum += ADCW;
	}
	volt_sum /= how_many;

	// convert from 0..1023 to 0..Vref
	float adc_voltage = (4962.0f / 1024.0f) * (float)volt_sum;

	// no voltage divider connected
	if(Rup == 0 && Rdn == 0) {
		return adc_voltage;
	}

	// +voltage divider (if any)
	return adc_voltage * Rdn / (Rup + Rdn);
}

// this looks stupid
uint8_t next_within_window(uint16_t next, uint16_t baseline, uint16_t window) {
	// no overflow of window
	if(baseline + window > baseline) {
		// NEXT(BASELINE .. BASELINE + WINDOW)
		if(next > baseline && next <= (baseline + window)) {
			return 1;
		}
	}
	// window overflows
	else {
		// upper window NEXT(BASELINE .. MAX)
		if(next > baseline && next <= 0xFFFF) {
			return 1;
		}
		// lower window NEXT(0 .. BASELINE + WINDOW that overflowen)
		else {
			if(next <= baseline + window) {
				return 1;
			}
		}
	}
	return 0;
}
