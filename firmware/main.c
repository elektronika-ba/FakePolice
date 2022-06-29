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
#include <avr/wdt.h>

// Note: There is a library_pins.h file included globally by including entire directory "library_config". This is set in Project->Properties->Directories
// This is required for the included libraries

#include "main.h"
#include "../lib/uart/uart.h"
#include "../lib/spi/spi.h"
#include "../lib/nrf24l01/nrf24l01.h"
#include "keeloq_crypt.h" // yes, believe it or not :)

// misc
volatile uint8_t radio_event = 0;
volatile uint8_t rtc_event = 0;
volatile uint32_t charging_time_sec = 0;
volatile uint8_t rtc_ok = 0; // set to 1 when we receive SETRTC command and process it
volatile uint8_t hacking_attempts_cnt = 0;

// radio sleep stuff
volatile uint8_t radio_sleeping = 0; // state of the radio

// timer for putting radio to sleep
volatile uint16_t tmr_radio_sleeper = 0; // timer
// settings for how long radio can remain ON - two values (short listening time, and long listening time because there was RF RX activity)
volatile uint8_t radio_listen_short_sec = 0; // setting for small listening time (there was no RF activity)
volatile uint16_t radio_listen_long_sec = 0; // setting for longer listening time (there is RF activity)
volatile uint16_t radio_stayawake_sec = 0; // override of going to sleep, to stay awake this much instead of "radio_listen_long_sec" when this is > 0

// timer for waking radio up
volatile uint8_t tmr_radio_wakeuper = 0; // timer
volatile uint8_t radio_wakeuper_sec = 0; // setting

// setting for RF radio channel
volatile uint8_t radio_rf_channel = 0; // setting

// police light blinker
volatile uint8_t police_lights_count = 0;
volatile uint8_t police_lights_stage = 0; // red/blue
volatile uint8_t police_lights_stage_counter = 0; // counting pulsing of red/blue leds
volatile uint16_t police_lights_stage_on_timer = 0; // timer for tiggle-time of the LED itself
volatile uint8_t police_lights_busy = 0;
// settings saved to eeprom, and configurable remotely
volatile uint8_t police_lights_stage_count = 0;
volatile uint8_t police_lights_stage_on_8ms = 0;

volatile uint32_t telemetry_timer_min = 0;
// setting
volatile uint8_t telemetry_min = 0; // setting
volatile uint16_t telemetry_solvol_good = 0; // setting

// rolling code sync value (stored/updated to EEPROM)
volatile uint16_t kl_rx_counter;
volatile uint16_t kl_rx_counter_resync; // this follows value of kl_rx_counter
volatile uint16_t kl_tx_counter;
volatile uint64_t kl_master_crypt_key;

// RTC
volatile uint8_t RTC[7] = {22, 6, 9, 12, 0, 0, 4};			// YY MM DD HH MM SS WEEKDAY(1-MON...7-SUN)
volatile uint8_t bools[BOOL_BANK_SIZE] = { 0, 0 }; 				// 8 global boolean values per BOOL_BANK_SIZE
volatile uint64_t seconds_counter = 0;
volatile uint8_t rtc_busy = 0;

void send_telemetry() {
	double solar_volt = read_solar_volt();
	double boost_volt = read_boost_volt();
	double batt_volt = read_batt_volt();
	double temperature = read_temperature();

	// salji i ovo:
	// hacking_attempts_cnt
	// charging time in minutes

	// pripremi paket i salji, imam 26 bajta ukupno za ovo
	// C5.3#5.2#4.1#-17#HHH#TTT
	// 		C=charging, N=not charging --- this is not accurate, but we will leave it available
	// 		5.3=solar voltage
	//		5.2=booster voltage
	//		4.1=battery voltage
	//		-17=temperature, or without the "-" when positive
	//		HHH=hacking attempts 0-999min
	//		TTT=charging time in minutes 0-999min

	uint8_t charging_or_not = 'N';
	if(!(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN))) charging_or_not = 'C';

	uint16_t charging_time_min = charging_time_sec / 60;
	if(charging_time_min > 999) charging_time_min = 999;

	if(hacking_attempts_cnt >= 100) hacking_attempts_cnt = 99; // we dont have space in param to have 3 digits here

	uint8_t param[26];
	sprintf((char *)param, "%c#%.1f#%.1f#%.2f#%.0f#%d#%d#$", charging_or_not, solar_volt/1000.0, boost_volt/1000.0, batt_volt/1000.0, temperature, hacking_attempts_cnt, charging_time_min);

	send_command(RF_CMD_TELEDATA, param, 26);

	hacking_attempts_cnt = 0; // we can clear this one now
}

double read_temperature() {
	setHigh(TEMP_SHUT_PORT, TEMP_SHUT_PIN);
	delay_builtin_ms_(5);

	double adc = read_adc_mv(TEMP_C_ADMUX_VAL, 0, 0, 16);

	setLow(TEMP_SHUT_PORT, TEMP_SHUT_PIN);

	// TMP36F provides a 750 mV output at 25°C. Output scale factor of 10 mV/°C. offset is 100mV at -40°C
	return (adc - 100) / 10 - 40;
}

double read_solar_volt() {
	return read_adc_mv(SOL_VOLT_ADMUX_VAL, 47000.0, 47000.0, 16);
}

double read_boost_volt() {
	return read_adc_mv(BOOST_VOLT_ADMUX_VAL, 47000.0, 47000.0, 16);
}

double read_batt_volt() {
	return read_adc_mv(BATT_VOLT_ADMUX_VAL, 47000.0, 100000.0, 16);
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
	police_lights_stage_counter = police_lights_stage_count * 2;

	police_lights_count = times; // start it
}

void send_command(uint16_t command, uint8_t *param, uint8_t param_len) {
	// build access code
	uint32_t decrypted = kl_tx_counter; // add counter to lower 2 bytes
	decrypted |= ((uint32_t)command) << 16; // add command to upper 2 bytes

	// encrypt it
	keeloq_encrypt(&decrypted, (uint64_t *)&kl_master_crypt_key);

	// build the tx buffer
	uint8_t tx_buff[32];
	uint8_t *tx_buff_ptr = tx_buff;

	// ACCESS CODE
	memcpy(tx_buff_ptr, &decrypted, 4);
	tx_buff_ptr+=4;

	// COMMAND
	memcpy(tx_buff_ptr, &command, 2);
	tx_buff_ptr+=2;

	// PARAM
	memcpy(tx_buff_ptr, param, param_len);
	// tx_buff_ptr+=param_len; // not required

	// send!
	nrf24l01_write(tx_buff);

	// wait until the packet has been sent or the maximum number of retries has been active - all events are mapped to interrupt
	while( !(nrf24l01_irq_max_rt() || nrf24l01_irq_tx_ds()) )
	{
		_delay_ms(1); // da usporimo i malo rasteretimo SPI
	}

	// check if packet was not sent right away
	//error
	if( nrf24l01_irq_max_rt() )
	{
		nrf24l01_irq_clear_max_rt();
	}
	// sent ok!
	else
	{
		// we can see if package was received by the receiver and then increase the counter.
		kl_tx_counter++;
		update_settings_to_eeprom();

		nrf24l01_irq_clear_tx_ds();
	}

	// back to listening
	nrf24l01_setRX();

	// if radio was sleeping, put it back to sleep because we messed it up by transmitting and reverting back to RX mode
	if(radio_sleeping) {
		nrf24l01_ce_low();
	}
}

void process_command(uint8_t *rx_buff) {

	// RF PACKET 	  {[ROLLING ACCESS CODE 4 bytes][COMMAND 2 bytes][PARAM N bytes]}
	// 	[first addr 0]{																}[last addr 31]

	// ROLLING ACCESS CODE 4 bytes IS: {[COUNTER LSB][COUNTER MSB][COMMAND LSB][COMMAND MSB]}
	//						   [addr 0]{													}[addr 3]

	// decrypt access code
	uint32_t encrypted = rx_buff[0];
	encrypted |= (uint16_t)(rx_buff[1]) << 8;
	encrypted |= (uint32_t)(rx_buff[2]) << 16;
	encrypted |= (uint32_t)(rx_buff[3]) << 24;

	keeloq_decrypt(&encrypted, (uint64_t *)&kl_master_crypt_key);

	// variable "encrypted" is now "decrypted". we need to verify whether this is a valid data or not.
	// encryption here does not provide secrecy but only message authenticity. that's all we care about here.

	// extract sync rx_counter from the encrypted portion, it is at the lower 2 bytes
	uint16_t enc_rx_counter = encrypted & 0xFFFF;

	// extract command from the encrypted portion, it is at the upper 2 bytes
	uint16_t dec_command = encrypted >> 16;

	// extract command from non-encrypted (plaintext) portion
	uint16_t raw_command = rx_buff[4];
	raw_command |= (uint16_t)(rx_buff[5]) << 8;

	// verify message authenticity:
	// 1. sequence must be within the window
	// 2. command from encrypted portion and plaintext command must match (I know, I know, plaintext attack...)
	if(
		dec_command == raw_command
		&& next_within_window(enc_rx_counter, kl_rx_counter, 64)
	)
	{
		kl_rx_counter = enc_rx_counter; // keep track of the sync counter
		kl_rx_counter_resync = enc_rx_counter; // follow this one always
		update_settings_to_eeprom(); // save (everything every time)

		// optional parameter pointer
		uint8_t *param = rx_buff + 6; // processed 6 bytes so far, so skip them. (Note: we are left with 32-6 = 26 for the parameter. 32 because nRF24L01 packet is 32 bytes long max)

		switch(dec_command) {
			case RF_CMD_SYSRESET:
			{
				// https://microchipsupport.force.com/s/article/Software-Reset
				wdt_enable(WDTO_30MS);
				while(1) {}
			}
			break;
			
			case RF_CMD_ABORT:
				police_off();
			break;

			case RF_CMD_POLICE:
			{
				uint8_t times = param[0];
				police_on(times);
			}
			break;

			case RF_CMD_CAMERA:
				speed_camera();
			break;
			
			case RF_CMD_BLUECAMERA:
				speed_camera_blue();
			break;
			
			case RF_CMD_CONFIGPOLICE:
				police_lights_stage_count = param[0];
				police_lights_stage_on_8ms = param[1];

				update_settings_to_eeprom();
			break;

			case RF_CMD_SETTELEM:
			{
				uint8_t *tmp_ptr = param;
				// mind the byteorder
				memcpy((uint16_t *)&telemetry_solvol_good, tmp_ptr, 2);
				tmp_ptr+=2;

				memcpy((uint8_t *)&telemetry_min, tmp_ptr, 1);
				// tmp_ptr++;
			
				// apply/load immediately
				telemetry_timer_min = telemetry_min;
			
				update_settings_to_eeprom();
			}
			break;
			
			case RF_CMD_SETRFCHAN:
				radio_rf_channel = param[0];

				// RF_CH
				nrf24l01_writeregister(NRF24L01_REG_RF_CH, radio_rf_channel); // RF channel
				delay_builtin_ms_(5);

				update_settings_to_eeprom();
			break;

			case RF_CMD_SLEEP:
				tmr_radio_sleeper = 0; // go to radio sleep ASAP
				radio_stayawake_sec = 0; // clear this one as well
				radio_sleeping = 0; // make it go to sleep ASAP
			break;
			
			case RF_CMD_STAYAWAKE:
				// mind the byteorder
				memcpy((uint16_t *)&radio_stayawake_sec, param, 2);
			break;

			case RF_CMD_SETRADIOTMRS:
			{
				uint8_t *tmp_ptr = param;
				// mind the byteorder
				memcpy((uint16_t *)&radio_listen_long_sec, tmp_ptr, 2);
				tmp_ptr+=2;

				memcpy((uint8_t *)&radio_listen_short_sec, tmp_ptr, 1);
				tmp_ptr++;

				memcpy((uint8_t *)&radio_wakeuper_sec, tmp_ptr, 1);
				// tmp_ptr++;

				update_settings_to_eeprom();
			}
			break;

			// request for our RTC data, we will send it, in binary form, 7 bytes total
			case RF_CMD_GETRTC:
			{
				uint8_t param[26];
				memcpy(param, (uint8_t *)&RTC, 7);

				send_command(RF_CMD_RTCDATA, param, 26);
			}

			// RTC data coming, we will set it
			case RF_CMD_RTCDATA:
			{
				while(rtc_busy); // sync

				TCCR2B = 0; // pause RTC...

				// get RTC from param 7 bytes - mind the byteorder
				memcpy((uint8_t *)&RTC, param, 7);

				TCCR2B |= (1<<CS22)|(1<<CS00); // resume RTC...

				rtc_ok = 1;
			}
			break;

			case RF_CMD_NEWKEY:
			{
				// get new key from param 8 bytes - mind the byteorder
				memcpy((uint8_t *)&kl_master_crypt_key, param, 8);

				update_settings_to_eeprom();
			}
			break;

			case RF_CMD_GETTELE:
				send_telemetry();
			break;

			default:
			{
				// wtf
			}
		}

		// if it was not a sleep command, don't reload this
		if(dec_command != RF_CMD_SLEEP) {
			// is there an override of this?
			if(radio_stayawake_sec) {
				tmr_radio_sleeper = radio_stayawake_sec; // use this one, could be more or even less than "radio_listen_long_sec" so lets not use max() for this
			}
			else  {
				tmr_radio_sleeper = radio_listen_long_sec; // reload timer for going to radio sleep, use the long one since we are actively communicating now			
			}
		}
	}
	else {
		// maybe it is within the larger re-sync window?
		if(next_within_window(enc_rx_counter, kl_rx_counter_resync, 32767)) {
			// caught up?
			if(next_within_window(enc_rx_counter, kl_rx_counter_resync, 1)) {
				kl_rx_counter = enc_rx_counter;
			}
			kl_rx_counter_resync = enc_rx_counter;

			// is there an override of this?
			if(radio_stayawake_sec) {
				tmr_radio_sleeper = radio_stayawake_sec; // use this one
			}
			else  {
				tmr_radio_sleeper = radio_listen_long_sec; // reload timer for going to radio sleep, use the long one since we are actively communicating now
			}
		}
		else {
			hacking_attempts_cnt++;
		}

		// DEBUG
		#ifdef DEBUGZ
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(500);
		setLow(LED_RED_PORT, LED_RED_PIN);
		#endif
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

void speed_camera_blue() {
	for(uint8_t j=0; j<2; j++)	{
		setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(60);
		setLow(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(60);
	}
	delay_builtin_ms_(500);

	for(uint8_t j=0; j<2; j++)	{
		setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(60);
		setLow(LED_BLUE_PORT, LED_BLUE_PIN);
		delay_builtin_ms_(60);
	}
}

// simulates police lights without ISR usage
void police_inline(uint8_t times) {
	for(uint8_t i=0; i<times; i++) {
		for(uint8_t j=0; j<police_lights_stage_count; j++)	{
			setHigh(LED_RED_PORT, LED_RED_PIN);
			delay_builtin_ms_(police_lights_stage_on_8ms * 8);
			setLow(LED_RED_PORT, LED_RED_PIN);
			delay_builtin_ms_(police_lights_stage_on_8ms * 8);
		}

		for(uint8_t j=0; j<police_lights_stage_count; j++)	{
			setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(police_lights_stage_on_8ms * 8);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(police_lights_stage_on_8ms * 8);
		}
	}
}

void update_settings_to_eeprom() {
	// save all working stuff to eeprom, and mark if VALID

	// MASTER CRYPT-KEY
	eeprom_update_block((uint64_t *)&kl_master_crypt_key, (uint8_t *)EEPROM_MASTER_CRYPT_KEY, 8);

	// COUNTERS
	eeprom_update_word((uint16_t *)EEPROM_RX_COUNTER, kl_rx_counter);
	eeprom_update_word((uint16_t *)EEPROM_TX_COUNTER, kl_tx_counter);

	// POWER SAVER
	eeprom_update_byte((uint8_t *)EEPROM_RADIO_WAKEUPER, radio_wakeuper_sec);
	eeprom_update_word((uint16_t *)EEPROM_RADIO_LISTEN_LONG, radio_listen_long_sec);
	eeprom_update_byte((uint8_t *)EEPROM_RADIO_LISTEN_SHORT, radio_listen_short_sec);

	// POLICE LIGHTS TIMERS
	eeprom_update_byte((uint8_t *)EEPROM_POLICE_L_STAGE_CNT, police_lights_stage_count);
	eeprom_update_byte((uint8_t *)EEPROM_POLICE_L_STAGE_8MS, police_lights_stage_on_8ms);

	// TELEMETRY STUFF
	eeprom_update_byte((uint8_t *)EEPROM_TELEMETRY_MIN, telemetry_min);
	eeprom_update_word((uint16_t *)EEPROM_TELEMETRY_SOLVOL, telemetry_solvol_good);

	// RADIO RF CHANNEL
	eeprom_update_byte((uint8_t *)EEPROM_RF_CHANNEL, radio_rf_channel);

	// say eeprom is valid
	eeprom_update_byte((uint8_t *)EEPROM_MAGIC, EEPROM_MAGIC_VALUE);
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

	// read settings stuff from EEPROM
	// eeprom has some settings?
    if( eeprom_read_byte((uint8_t *)EEPROM_MAGIC) == EEPROM_MAGIC_VALUE) {
		eeprom_read_block((uint64_t *)&kl_master_crypt_key, (uint8_t *)EEPROM_MASTER_CRYPT_KEY, 8);
		kl_rx_counter = eeprom_read_word((uint16_t *)EEPROM_RX_COUNTER);
		kl_tx_counter = eeprom_read_word((uint16_t *)EEPROM_TX_COUNTER);

		radio_wakeuper_sec = eeprom_read_byte((uint8_t *)EEPROM_RADIO_WAKEUPER);
		radio_listen_long_sec = eeprom_read_word((uint16_t *)EEPROM_RADIO_LISTEN_LONG);
		radio_listen_short_sec = eeprom_read_byte((uint8_t *)EEPROM_RADIO_LISTEN_SHORT);

		police_lights_stage_count = eeprom_read_byte((uint8_t *)EEPROM_POLICE_L_STAGE_CNT);
		police_lights_stage_on_8ms = eeprom_read_byte((uint8_t *)EEPROM_POLICE_L_STAGE_8MS);
		
		telemetry_min = eeprom_read_byte((uint8_t *)EEPROM_TELEMETRY_MIN);
		telemetry_solvol_good = eeprom_read_word((uint16_t *)EEPROM_TELEMETRY_SOLVOL);

		radio_rf_channel = eeprom_read_byte((uint8_t *)EEPROM_RF_CHANNEL);
	}
	// nope, use defaults
	else {
		kl_master_crypt_key = DEFAULT_KEELOQ_CRYPT_KEY;
		kl_rx_counter = DEFAULT_KEELOQ_COUNTER;
		kl_tx_counter = DEFAULT_KEELOQ_COUNTER;

		radio_wakeuper_sec = DEFAULT_RADIO_WAKEUPER_SEC;
		radio_listen_long_sec = DEFAULT_RADIO_LISTEN_LONG_SEC;
		radio_listen_short_sec = DEFAULT_RADIO_LISTEN_SHORT_SEC;

		police_lights_stage_count = DEFAULT_POLICE_LIGHTS_STAGE_COUNT;
		police_lights_stage_on_8ms = DEFAULT_POLICE_LIGHTS_STAGE_ON_8MS;

		telemetry_min = DEFAULT_TELEMETRY_MINUTES;
		telemetry_solvol_good = DEFAULT_LOWEST_SOLVOLT_GOOD;

		radio_rf_channel = DEFAULT_RF_CHANNEL;

		update_settings_to_eeprom();
	}
	kl_rx_counter_resync = kl_rx_counter; // follow

	// Init the SPI port
	SPI_init();

	// nRF24L01 Init
	uint8_t my_rx_addr[5];
	my_rx_addr[0] = 77;
	my_rx_addr[1] = 66;
	my_rx_addr[2] = 44;
	my_rx_addr[3] = 33;
	my_rx_addr[4] = 88;
	nrf24l01_init(radio_rf_channel);
	nrf24l01_setrxaddr0(my_rx_addr);
	nrf24l01_settxaddr(my_rx_addr);
	nrf24l01_setRX(); // listening by default

	// configure timer to put radio to sleep ASAP
	tmr_radio_sleeper = 0;
	radio_sleeping = 0;

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

	// DEBUGGING
	#ifdef DEBUGZ
	for(uint8_t i=0; i<3; i++) {
		setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
		setHigh(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(100);
		setLow(LED_BLUE_PORT, LED_BLUE_PIN);
		setLow(LED_RED_PORT, LED_RED_PIN);
		delay_builtin_ms_(100);
	}
	delay_builtin_ms_(200);
	#endif

	// main program
	while(1) {
		/*
		1. configure sleep mode
		// we can be woken up by:
		- IRQ from nRF radio
		- Timer2, 1 second interval

		2. sleep

		zZzzZZzzZZzzZzzz...

		3. wakeup event!

		4. check what woke us up
		- Timer2 = send telemetry and go back to 1.
		- IRQ from nRF radio = process command and go back to 1.
		*/

		// OK to sleep?
		if(!police_lights_count) {
			// make sure these are OFF during sleep so that we don't end up dead
			setLow(LED_RED_PORT, LED_RED_PIN);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);

			#ifdef DEBUGZ
			setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(50);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(50);
			#endif

			// configure sleep mode
			set_sleep_mode(SLEEP_MODE_PWR_SAVE);

			// go to sleep
			sleep_mode(); // zzzZZZzzzZZZzzzZZZzzz

			// WOKEN UP!

			#ifdef DEBUGZ
			setHigh(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(50);
			setLow(LED_BLUE_PORT, LED_BLUE_PIN);
			delay_builtin_ms_(50);
			#endif
		}

		// (some interrupt happens) and we get into the ISR() of it... after it finishes, we continue here:

		// did radio packet arrive?
		if(radio_event) {

			// verify that RF packet arrived and process it
			if( nrf24l01_irq_rx_dr() )
			{
				while( !( nrf24l01_readregister(NRF24L01_REG_FIFO_STATUS) & NRF24L01_REG_RX_EMPTY) )
				{
					uint8_t rx_buff[32];

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
				if(read_solar_volt() >= telemetry_solvol_good) {
					send_telemetry();
					//set_rtc_speed(0); // 1-sec RTC
				}
				else {
					//set_rtc_speed(1); // 8-sec RTC
				}

				telemetry_timer_min = telemetry_min; // reload for next time
			}

			// time to put radio to sleep, if radio not sleeping already?
			if(!tmr_radio_sleeper && !radio_sleeping) {
				nrf24l01_ce_low(); // stop radio listening
				radio_sleeping = 1;
				radio_stayawake_sec = 0; // clear this one as well

				tmr_radio_wakeuper = radio_wakeuper_sec; // reload the timer for radio wakeup
			}

			// time to turn the radio ON, if radio is sleeping?
			if(!tmr_radio_wakeuper && radio_sleeping) {
				nrf24l01_ce_high(); // re-start radio listening
				radio_sleeping = 0;

				// set the sleeper to listen for "radio_listening_sec" and then go to sleep
				tmr_radio_sleeper = radio_listen_short_sec; // use short timer, this one is for expecting some RF command
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

	// with certain PV voltages, booster fails to produce enough current for charger
	// so the charger keeps restarting and wakes up the AVR constantly - abandoning this option!
	// I also commented out the ISR handler for this
//	BATT_CHRG_PCMSKREG |= _BV(BATT_CHRG_PCINTBIT); 					// set (un-mask) PCINTn pin for interrupt on change
//	PCICR |= _BV(BATT_CHRG_PCICRBIT); 								// enable wanted PCICR

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
	rtc_busy = 1; // for sync

	rtc_event = 1;

	// TODO: do something every second?
	if(tmr_radio_wakeuper) tmr_radio_wakeuper--;
	if(tmr_radio_sleeper) tmr_radio_sleeper--; // for keeping the radio ON
	seconds_counter++; // seconds ticker, for global use

	// accumulate charging time
	if( !(BATT_CHRG_PINREG & _BV(BATT_CHRG_PIN)) ) {
		charging_time_sec++;
	}

	RTC[TIME_S]++;

	// a minute!
	if(RTC[TIME_S] >= 60)
	{
		RTC[TIME_S] = 0;
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

	rtc_busy = 0; // for sync
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
				police_lights_stage_counter = police_lights_stage_count * 2; // reload counter
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

				police_lights_stage_on_timer = police_lights_stage_on_8ms; // reload timer
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

	// event is only on LOW pulse
	if( !(nrf24l01_IRQ_PINREG & _BV(nrf24l01_IRQ_PIN)) ) {
		radio_event = 1;
	}

	PCICR |= _BV(PCIE0); 								// ..re-enable interrupts for the entire section
}

/*
// Interrupt: pin change interrupt
// This one is connected to Li+ battery charger CHARGE indicator
ISR(PCINT1_vect, ISR_NOBLOCK) {
	sleep_disable();

	PCICR &= ~_BV(PCIE1); 								// ..disable interrupts for the entire section

	charge_event = 1;

	PCICR |= _BV(PCIE1); 								// ..re-enable interrupts for the entire section
}
*/

// calculate if given year is leap year
uint8_t isleapyear(uint16_t y)
{
	return ( ( !(y % 4) && (y % 100) ) || !(y % 400) );
}

// reads average ADC value
double read_adc_mv(uint8_t admux_val, double Rup, double Rdn, uint8_t how_many)
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
	double adc_voltage = (3300.0f / 1024.0f) * (double)volt_sum;

	// no voltage divider connected
	if(Rdn == 0) {
		return adc_voltage;
	}

	// +voltage divider (if any)
	return (adc_voltage * ((Rup + Rdn)/Rdn));
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
