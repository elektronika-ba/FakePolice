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
volatile uint8_t rtc_ok = 0; // set to 1 when we receive SETRTC command and process it

volatile uint8_t hacking_attempts_cnt = 0;

// police light blinker
volatile uint8_t police_lights_count = 0;
volatile uint8_t police_lights_stage = 0; // red/blue
volatile uint8_t police_lights_stage_counter = 0; // counting pulsing of red/blue leds
volatile uint16_t police_lights_stage_on_timer = 0; // timer for tiggle-time of the LED itself
volatile uint8_t police_lights_busy = 0;

volatile uint32_t telemetry_timer_min = 0;

// rolling code sync value (stored/updated to EEPROM)
volatile uint16_t kl_rx_counter = 100;
volatile uint16_t kl_tx_counter = 1000;
volatile uint64_t kl_key = SECRET_KEELOQ_KEY;

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

	// salji i ovo:
	// hacking_attempts_cnt
	// charging_time_sec

	// TODO: pripremi paket i salji
	// imam 26 bajta ukupno za ovo

	hacking_attempts_cnt = 0; // we can clear this one now
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

	// TMP36F provides a 750 mV output at 25�C. Output scale factor of 10 mV/�C. offset is 100mV at -40�C
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

// stop police lights in ISR (if currently running)
void police_off() {
	while(police_lights_busy); // wait until ISR finishes (possibly)

	police_lights_count = 0; // end it
}

// start police lights in ISR for N times
void police_on(uint8_t times) {
	police_off();

	police_lights_stage = 0;
	police_lights_stage_on_timer = 0;
	police_lights_stage_counter = POLICE_LIGHTS_STAGE_COUNT;

	police_lights_count = times; // start it
}

uint8_t send_command(uint16_t command, uint8_t *param) {
	// todo
	// kl_tx_counter
}

void process_command(uint8_t *rx_buff) {
	// 32bytes maximum{[ROLLING ACCESS CODE 4 bytes][COMMAND 2 bytes][PARAM N bytes]}
	// COMMAND+Counter is also contained within the ROLLING ACCESS CODE
	// ROLLING ACCESS CODE 4 bytes IS: [COUNTER MSB][COUNTER LSB][COMMAND MSB][COMMAND LSB]

	// decrypt access code
	uint32_t encrypted = 0;
	encrypted |= (uint32_t)rx_buff[3] << 24;
	encrypted |= (uint32_t)rx_buff[2] << 16;
	encrypted |= (uint16_t)rx_buff[1] << 8;
	encrypted |= rx_buff[0];

	keeloq_decrypt(&encrypted, (uint64_t *)&kl_key);

	// variable "encrypted" is now "decrypted". we need to verify whether this is a valid data or not.
	// there is no MAC, so we treat entire data as MAC. encryption here does not
	// provide secrecy but only message authenticity. that's all we care about really.

	// extract command from the encrypted portion
	uint16_t dec_command = 0;
	dec_command |= encrypted & 0xFF00;
	dec_command |= (uint8_t)encrypted;

	// extract sync rx_counter from the encrypted portion
	uint16_t enc_rx_counter = 0;
	enc_rx_counter |= encrypted >> 16 & 0xFF00;
	enc_rx_counter |= (uint8_t)encrypted >> 24;

	// extract command from non-encrypted (plaintext) portion
	uint16_t raw_command = 0;
	raw_command |= rx_buff[4] << 8;
	raw_command |= rx_buff[5];

	// verify message authenticity:
	// 1. sequence must be within the window
	// 2. encrypted and raw commands must match
	if(
		next_within_window(enc_rx_counter, kl_rx_counter, 32)
		&& dec_command == raw_command
	)
	{
		kl_rx_counter = enc_rx_counter + 1; // keep track of the sync counter

		// OK to process further...

		// optional parameter pointer
		uint8_t *param = rx_buff + 6; // processed 6 bytes so far, we are left with 32-6 = 26 for the parameter. 32 because nRF24L01 packet is 32 bytes long max.

		switch(dec_command) {
			case RF_CMD_ABORT:
				police_off();
			break;

			case RF_CMD_POLICE:
				uint8_t times = param[0];
				police_on(times);
			break;

			case RF_CMD_CAMERA:
				speed_camera();
			break;

			case RF_CMD_SETRTC:
				// extract RTC from param

				// todo...
				// parse param

				rtc_ok = 1;
			break;

			case RF_CMD_GETTELE:
				send_telemetry();
			break;

			default:
				// wtf
		}
	}
	else {
		hacking_attempts_cnt++;
	}

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

// simulates police lights without ISR usage
void police_once() {
	for(uint8_t j=0; j<POLICE_LIGHTS_STAGE_COUNT; j++)	{
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(POLICE_LIGHTS_STAGE_ON_MS);
		setLow(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(POLICE_LIGHTS_STAGE_ON_MS);
	}

	for(uint8_t j=0; j<POLICE_LIGHTS_STAGE_COUNT; j++)	{
		setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(POLICE_LIGHTS_STAGE_ON_MS);
		setLow(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(POLICE_LIGHTS_STAGE_ON_MS);
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

	// Initialize Timer0 overflow ISR for 8.192ms interval, no need to go faster
	TCCR0A = 0;
	TCCR0B = _BV(CS12); // 1:256 prescaled, timer started!
	TIMSK0 |= _BV(TOIE0); // for ISR(TIMER0_OVF_vect)

	// Initialize Timer2 as async RTC counter @ 1Hz with 32.768kHz crystal
	TCNT2 = 0;
    TCCR2B |= (1<<CS22)|(1<<CS00); // 1 second by default
    //Enable asynchronous mode
    ASSR  = (1<<AS2);
    //wait for registers update
    // while (ASSR & ((1<<TCN2UB)|(1<<TCR2BUB)));
    // enable overflow interrupt
	TIMSK2 |= (1 << TOIE2);

	// Interrupts ON
	sei();

	delay_builtin_ms_(50);

	uint8_t rx_buff[32];

	// DEBUGGING
	#ifdef DEBUG
	police_once();
	#endif

	// main program
	while(1) {
		/*
		1. configure sleep mode
		// we can be woken up by:
		- Battery charge indicator
		- IRQ from nRF radio
		- Timer2

		2. sleep

		zZzzZZzzZZzzZzzz...

		3. wakeup event!

		4. check what woke us up
		- Timer2 = send telemetry and go back to 1.
		- Battery charge indicator = send telemetry and go back to 1.
		- IRQ from nRF radio = process command and go back to 1.
		*/

		// OK to sleep?
		if(!police_lights_count) {
			// make sure these are OFF during sleep so that we don't end up dead
			setLow(LED_RED_PORT, LED_RED_PIN);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);

			// configure sleep mode
			set_sleep_mode(SLEEP_MODE_PWR_SAVE);

			// go to sleep
			sleep_mode();

			// zzzZZZzzzZZZzzzZZZzzz
		}

		// (some interrupt happens) and we get into the ISR() of it... after it finishes, we continue here:

		// did charge event happen?
		if(charge_event) {

			// charging
			if( !(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN)) ) {
				// nothing (yet)
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

			// verify that RF packet arrived and process it
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

		// RTC has woken us up? every 1 or 8 seconds depending on situation
		if(rtc_event) {

			// send telemetry if it is time and if solar voltage is good
			if(!telemetry_timer_min) {
				if(read_solar_volt() >= LOWEST_SOLVOLT_GOOD) {
					send_telemetry();
				}
				telemetry_timer_min = TELEMETRY_MINUTES; // reload for next time
			}

			rtc_event = 0; // handled
		}

		// whatever wakes us up, we can tweak our RTC period (but every 5min)
		// hm...

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
// set to overflow at 8.192ms milliseconds
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
	police_lights_busy = 1; // for sync

	// blinker ON?
	if(police_lights_count) {

		// time to toggle the pin?
		// nope
		if(police_lights_stage_on_timer) {
			police_lights_stage_on_timer--;
		}
		// yep
		else {
			// time to change the stage?
			// nope
			if(police_lights_stage_counter) {
				police_lights_stage_counter--;
			}
			// yep
			else {
				// completed one full blink (both stages blinked at least once)
				if(police_lights_stage) {
					police_lights_count--; // full sequence (RED+BLUE) completed
				}

				police_lights_stage = !police_lights_stage; // change it
				police_lights_stage_counter = POLICE_LIGHTS_STAGE_COUNT; // reload counter
			}

			// toggle the pin according to the current stage, if there is more to do
			if(police_lights_count) {
				if(police_lights_stage) {
					setLow(LED_BLUE_PORT, LED_BLUE_PIN); // keep this one off
					togglePin(LED_RED_PORT, LED_RED_PIN); // blink this one
				}
				else {
					setLow(LED_RED_PORT, LED_RED_PIN); // keep this one off
					togglePin(LED_BLUE_PORT, LED_BLUE_PIN); // blink this one
				}

				police_lights_stage_on_timer = POLICE_LIGHTS_STAGE_ON_8MS; // reload timer
			}
			// this was the last pass? - turn them off finally
			else {
				setLow(LED_RED_PORT, LED_RED_PIN);
				setLow(LED_BLUE_PORT, LED_BLUE_PIN);
			}
		}

	}

	police_lights_busy = 0; // for sync
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
