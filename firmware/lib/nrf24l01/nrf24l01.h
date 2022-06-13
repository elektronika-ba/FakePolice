#ifndef NRF24L01_H_
#define NRF24L01_H_

#include <avr/io.h>

#include "library_pins.h" // -I"../library_config"

//defines for uC pins CE pin is connected to
//This is used so that the routines can send TX payload data and
//	properly initialize the nrf24l01 in TX and RX states.
//Change these definitions (and then recompile) to suit your particular application.
/* // moved to library_pins.h
#define nrf24l01_CE_DDR		DDRD
#define nrf24l01_CE_PORT	PORTD
#define nrf24l01_CE			6
*/

//defines for uC pins CSN pin is connected to
//This is used so that the routines can send properly operate the SPI interface
// on the nrf24l01.
//Change these definitions (and then recompile) to suit your particular application.
/* // moved to main.h
#define nrf24l01_CSN_DDR	DDRD
#define nrf24l01_CSN_PORT	PORTD
#define nrf24l01_CSN		5
*/

//defines for uC pins IRQ pin is connected to
//This is used so that the routines can poll for IRQ or create an ISR.
//Change these definitions (and then recompile) to suit your particular application.
/*
#define nrf24l01_IRQ_DDR	DDRC
#define nrf24l01_IRQ_PORT	PORTC
#define nrf24l01_IRQ_PIN	PINC
#define nrf24l01_IRQ		2
*/

/* Memory Map */
#define NRF24L01_REG_CONFIG      0x00
#define NRF24L01_REG_EN_AA       0x01
#define NRF24L01_REG_EN_RXADDR   0x02
#define NRF24L01_REG_SETUP_AW    0x03
#define NRF24L01_REG_SETUP_RETR  0x04
#define NRF24L01_REG_RF_CH       0x05
#define NRF24L01_REG_RF_SETUP    0x06
#define NRF24L01_REG_STATUS      0x07
#define NRF24L01_REG_OBSERVE_TX  0x08
#define NRF24L01_REG_CD          0x09
#define NRF24L01_REG_RX_ADDR_P0  0x0A
#define NRF24L01_REG_RX_ADDR_P1  0x0B
#define NRF24L01_REG_RX_ADDR_P2  0x0C
#define NRF24L01_REG_RX_ADDR_P3  0x0D
#define NRF24L01_REG_RX_ADDR_P4  0x0E
#define NRF24L01_REG_RX_ADDR_P5  0x0F
#define NRF24L01_REG_TX_ADDR     0x10
#define NRF24L01_REG_RX_PW_P0    0x11
#define NRF24L01_REG_RX_PW_P1    0x12
#define NRF24L01_REG_RX_PW_P2    0x13
#define NRF24L01_REG_RX_PW_P3    0x14
#define NRF24L01_REG_RX_PW_P4    0x15
#define NRF24L01_REG_RX_PW_P5    0x16
#define NRF24L01_REG_FIFO_STATUS 0x17
#define NRF24L01_REG_DYNPD	     0x1C
#define NRF24L01_REG_FEATURE     0x1D

/* Bit Mnemonics */
/*
#define NRF24L01_REG_MASK_RX_DR  0b01000000
#define NRF24L01_REG_MASK_TX_DS  0b00100000
#define NRF24L01_REG_MASK_MAX_RT 0b00010000
*/
#define NRF24L01_REG_EN_CRC      0b00001000
#define NRF24L01_REG_CRCO        0b00000100
#define NRF24L01_REG_PWR_UP      0b00000010
#define NRF24L01_REG_PRIM_RX     0b00000001
#define NRF24L01_REG_ENAA_P5     0b00100000
#define NRF24L01_REG_ENAA_P4     0b00010000
#define NRF24L01_REG_ENAA_P3     0b00001000
#define NRF24L01_REG_ENAA_P2     0b00000100
#define NRF24L01_REG_ENAA_P1     0b00000010
#define NRF24L01_REG_ENAA_P0     0b00000001
#define NRF24L01_REG_ERX_P5      0b00100000
#define NRF24L01_REG_ERX_P4      0b00010000
#define NRF24L01_REG_ERX_P3      0b00001000
#define NRF24L01_REG_ERX_P2      0b00000100
#define NRF24L01_REG_ERX_P1      0b00000010
#define NRF24L01_REG_ERX_P0      0b00000001
#define NRF24L01_REG_AW          0b00000001
#define NRF24L01_REG_ARD         0b00010000
#define NRF24L01_REG_ARC         0b00000000
#define NRF24L01_REG_PLL_LOCK    0b00010000
#define NRF24L01_REG_RF_DR       0b00001000
#define NRF24L01_REG_RF_PWR      0b00000010
#define NRF24L01_REG_LNA_HCURR   0b00000001
#define NRF24L01_REG_RX_DR       0b01000000
#define NRF24L01_REG_TX_DS       0b00100000
#define NRF24L01_REG_MAX_RT      0b00010000
#define NRF24L01_REG_RX_P_NO     0b00000010
#define NRF24L01_REG_TX_FULL     0b00000001
#define NRF24L01_REG_PLOS_CNT    0b00010000
#define NRF24L01_REG_ARC_CNT     0b00000001
#define NRF24L01_REG_TX_REUSE    0b01000000
#define NRF24L01_REG_FIFO_FULL   0b00100000
#define NRF24L01_REG_TX_EMPTY    0b00010000
#define NRF24L01_REG_RX_FULL     0b00000010
#define NRF24L01_REG_RX_EMPTY    0b00000001
#define NRF24L01_REG_RPD         0x09
#define NRF24L01_REG_RF_DR_LOW   0b00100000
#define NRF24L01_REG_RF_DR_HIGH  0b00001000
#define NRF24L01_REG_RF_PWR_LOW  0b00000010
#define NRF24L01_REG_RF_PWR_HIGH 0b00000100

/* Instruction Mnemonics */
#define NRF24L01_CMD_R_REGISTER    0x00
#define NRF24L01_CMD_W_REGISTER    0x20
#define NRF24L01_CMD_REGISTER_MASK 0x1F
#define NRF24L01_CMD_R_RX_PAYLOAD  0x61
#define NRF24L01_CMD_W_TX_PAYLOAD  0xA0
#define NRF24L01_CMD_FLUSH_TX      0xE1
#define NRF24L01_CMD_FLUSH_RX      0xE2
#define NRF24L01_CMD_REUSE_TX_PL   0xE3
#define NRF24L01_CMD_NOP           0xFF


extern void nrf24l01_init(uint8_t);
extern uint8_t nrf24l01_readregister(uint8_t);
extern void nrf24l01_readregisters(uint8_t, uint8_t *, uint8_t);
extern void nrf24l01_writeregister(uint8_t, uint8_t);
extern void nrf24l01_writeregisters(uint8_t, uint8_t *, uint8_t);
extern void nrf24l01_setrxaddr0(uint8_t *);
extern void nrf24l01_settxaddr(uint8_t *);
extern void nrf24l01_flushRXfifo();
extern void nrf24l01_flushTXfifo();
extern void nrf24l01_setRX();
extern void nrf24l01_setTX();
extern uint8_t nrf24l01_getstatus();
extern void nrf24l01_read(uint8_t *data);
extern void nrf24l01_write(uint8_t *data);
extern void nrf24l01_ce_high();
extern void nrf24l01_ce_low();
extern uint8_t nrf24l01_irq();
extern uint8_t nrf24l01_irq_rx_dr();
extern uint8_t nrf24l01_irq_tx_ds();
extern uint8_t nrf24l01_irq_max_rt();
extern void nrf24l01_irq_clear_all();
extern void nrf24l01_irq_clear_rx_dr();
extern void nrf24l01_irq_clear_tx_ds();
extern void nrf24l01_irq_clear_max_rt();

#endif /*NRF24L01_H_*/
