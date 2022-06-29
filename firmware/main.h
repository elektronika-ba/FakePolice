/*
 * main.h
 *
 * Created: 09. 06. 2022
 *  Author: Trax
 */

#ifndef MAIN_H_
#define MAIN_H_

//#define DEBUGZ 1

#define DEFAULT_KEELOQ_CRYPT_KEY 	0x1223344556788990 // chose a default secret code for yourself, or leave this one as it will change during operation
#define DEFAULT_KEELOQ_COUNTER		1000 // also...

// Limitations: ATmega328P (and family) PORTB.4 can only be used as INPUT if SPI mode is enabled.

#ifndef F_CPU
	#error You must define CPU frequency (F_CPU) in AVR Studio (Project Properties -> Toolchain -> Symbols >> add F_CPU=8000000UL) or your Makefile
#endif

#ifndef _BV
	#define _BV(n) (1 << n)
#endif

#define nop() asm volatile("nop")

#define	calc_UBRR(bau)				(F_CPU/16/bau-1)
#define calc_TWI(khz)				(((F_CPU/khz) - 16)/2)

// EEPROM addresses
#define EEPROM_MAGIC_VALUE			0xAA

#define	EEPROM_START				(0)									// start of our data in eeprom is here
#define EEPROM_MAGIC				(EEPROM_START + 0)					// 1 byte, eeprom OK - magic value
#define EEPROM_MASTER_CRYPT_KEY		(EEPROM_MAGIC + 1)					// 8 bytes, master crypt key address
#define EEPROM_RX_COUNTER			(EEPROM_MASTER_CRYPT_KEY + 8)		// 2 bytes, keeloq counter for receiving data
#define EEPROM_TX_COUNTER			(EEPROM_RX_COUNTER + 2)				// 2 bytes, keeloq counter for sending data
#define EEPROM_RADIO_WAKEUPER		(EEPROM_TX_COUNTER + 2)				// 1 byte, radio checker timer
#define EEPROM_RADIO_LISTEN_LONG	(EEPROM_RADIO_WAKEUPER + 1)			// 2 bytes, long timer before putting radio back to sleep (used for RF RX activity)
#define EEPROM_RADIO_LISTEN_SHORT	(EEPROM_RADIO_LISTEN_LONG + 2)		// 1 byte, short timer before putting radio back to sleep (used when no RF RX activity)
#define EEPROM_POLICE_L_STAGE_CNT	(EEPROM_RADIO_LISTEN_SHORT + 1)		// 1 byte, number of times to blink one color
#define EEPROM_POLICE_L_STAGE_8MS	(EEPROM_POLICE_L_STAGE_CNT + 1)		// 1 byte, leds ON/OFF time
#define EEPROM_TELEMETRY_MIN		(EEPROM_POLICE_L_STAGE_8MS + 1)		// 1 byte, time in minutes
#define EEPROM_TELEMETRY_SOLVOL		(EEPROM_TELEMETRY_MIN + 1)			// 2 bytes, voltage in mV
#define EEPROM_RF_CHANNEL			(EEPROM_TELEMETRY_SOLVOL + 2)		// 1 byte1, nRF24L01 RF channel

#define	DEFAULT_RF_CHANNEL			14		// this is system's default RF channel

#define DEFAULT_TELEMETRY_MINUTES			30		// on every X minutes send telemetry data to "home"
#define DEFAULT_LOWEST_SOLVOLT_GOOD			3000	// mV required at solar panel in order to send the timed-telemetry on every TELEMETRY_MINUTES

#define DEFAULT_POLICE_LIGHTS_STAGE_COUNT	4		// keep this value even! how many times should one color blink before switching to other color
#define DEFAULT_POLICE_LIGHTS_STAGE_ON_8MS	5		// delay&on-time between blinks/toggles of the LED ... in 8ms steps!!!

#define DEFAULT_RADIO_WAKEUPER_SEC			4		// radio waking up every this many seconds
#define DEFAULT_RADIO_LISTEN_SHORT_SEC		1		// listening for this many seconds
#define DEFAULT_RADIO_LISTEN_LONG_SEC		120		// after how many seconds of radio RX inactivity should we put radio back to sleep

// RF Commands (just random values known to both sides)
#define RF_CMD_SLEEP				0x9075	// goes to sleep mode. any other command wakes us up
#define RF_CMD_ABORT				0x2496	// aborts current command
#define RF_CMD_POLICE				0x7683	// police lights
#define RF_CMD_CAMERA				0x5628	// speed camera flash
#define RF_CMD_RTCDATA				0x5519	// RTC data
#define RF_CMD_GETRTC				0xE55C	// request of current RTC
#define RF_CMD_GETTELE				0x6087	// request for telemetry data
#define RF_CMD_TELEDATA				0x7806	// telemetry data packet (we send this)
#define RF_CMD_NEWKEY				0x4706	// keeloq key change
#define RF_CMD_SETRADIOTMRS			0x3366	// set long & short inactivity sleep intervals, and wakeuper interval
#define RF_CMD_CONFIGPOLICE			0x6A3C	// configuring the police light blinker
#define RF_CMD_BLUECAMERA			0xB449	// blue speed camera flash
#define RF_CMD_SETTELEM				0x99BA	// set telemetry interval parameters
#define RF_CMD_SETRFCHAN			0x560C	// set radio RF channel
#define RF_CMD_SYSRESET				0xDEAD	// reset the ATMega328P 
#define RF_CMD_STAYAWAKE			0x57A1	// stay awake for *param* minutes

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
#define	nrf24l01_IRQ_PINREG		PINB
#define	nrf24l01_IRQ_PCINTBIT	PCINT1
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

// functions
extern void misc_hw_init();
extern void delay_builtin_ms_(uint16_t);
extern uint8_t isleapyear(uint16_t); // calculate if given year is leap year
extern uint8_t next_within_window(uint16_t, uint16_t, uint16_t);
extern double read_adc_mv(uint8_t admux_val, double Rup, double Rdn, uint8_t how_many);
extern void send_telemetry();
extern double read_temperature();
extern double read_solar_volt();
extern double read_boost_volt();
extern double read_batt_volt();
extern void police_off();
extern void police_on(uint8_t times);
extern void send_command(uint16_t command, uint8_t *param, uint8_t param_len);
extern void process_command(uint8_t *rx_buff);
extern void speed_camera();
extern void police_inline(uint8_t times);
extern void update_settings_to_eeprom();
extern void speed_camera_blue();

#endif /* MAIN_H_ */