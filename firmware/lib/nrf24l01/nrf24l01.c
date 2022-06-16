#include <avr/io.h>
#include <util/delay.h>

#include "nrf24l01.h"
#include "../spi/spi.h"

void nrf24l01_init(uint8_t channel)
{
	// uC pin directions
	//nrf24l01_IRQ_DDR &= ~(1 << nrf24l01_IRQ);		// input
	nrf24l01_CE_DDR |= (1 << nrf24l01_CE);			// output
	nrf24l01_CSN_DDR |= (1 << nrf24l01_CSN);		// output
	// --

	// low CE
	nrf24l01_CE_PORT &= ~_BV(nrf24l01_CE);
	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

    _delay_ms(5); //wait for the radio to init

	// CONFIG
	nrf24l01_writeregister(NRF24L01_REG_CONFIG, (NRF24L01_REG_PRIM_RX | NRF24L01_REG_PWR_UP | NRF24L01_REG_CRCO | NRF24L01_REG_EN_CRC));

	// EN_AA, only pipe 0
	nrf24l01_writeregister(NRF24L01_REG_EN_AA, NRF24L01_REG_ENAA_P0);

	// EN_RXADDR
	nrf24l01_writeregister(NRF24L01_REG_EN_RXADDR, NRF24L01_REG_ERX_P0);

	// SETUP_AW
	nrf24l01_writeregister(NRF24L01_REG_SETUP_AW, 0b00000011); // 5 bytes address width

	// SETUP_RETR
	nrf24l01_writeregister(NRF24L01_REG_SETUP_RETR, 0b11111111); // maximum retry time and retry count

	// RF_CH
	nrf24l01_writeregister(NRF24L01_REG_RF_CH, channel); // RF channel

	// RF_SETUP
	nrf24l01_writeregister(NRF24L01_REG_RF_SETUP, 0b00100110); // max rf power, 125kbps

	// STATUS - not required
	//nrf24l01_writeregister(NRF24L01_REG_STATUS, ?);

	// RX ADDR 0 - not set here in init()
	//nrf24l01_writeregisters(NRF24L01_REG_RX_ADDR_P0, ?, 5);

	// TX ADDR - not set here in init()
	//nrf24l01_writeregisters(NRF24L01_REG_TX_ADDR, ?, 5);

	// RX_PW_P0
	nrf24l01_writeregister(NRF24L01_REG_RX_PW_P0, 32); // payload width pipe0 = 32 bytes

	// DYNPD
	nrf24l01_writeregister(NRF24L01_REG_DYNPD, 0); // no dynamic payload

	//set rx mode
	nrf24l01_setRX();
}

uint8_t nrf24l01_readregister(uint8_t reg)
{
	uint8_t result;

	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_R_REGISTER | (NRF24L01_CMD_REGISTER_MASK & reg));
    result = SPI_rw( NRF24L01_CMD_NOP );

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

    return result;
}

void nrf24l01_readregisters(uint8_t reg, uint8_t *value, uint8_t len)
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_R_REGISTER | (NRF24L01_CMD_REGISTER_MASK & reg));
	for(uint8_t i=0; i<len; i++)
	{
		value[i] = SPI_rw(NRF24L01_CMD_NOP); //read write register
	}

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return;
}

void nrf24l01_writeregister(uint8_t reg, uint8_t value)
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_W_REGISTER | (NRF24L01_CMD_REGISTER_MASK & reg));
	SPI_rw(value); //write register

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return;
}

void nrf24l01_writeregisters(uint8_t reg, uint8_t *value, uint8_t len)
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

    SPI_rw(NRF24L01_CMD_W_REGISTER | (NRF24L01_CMD_REGISTER_MASK & reg));
	for(uint8_t i=0; i<len; i++)
	{
		 SPI_rw(value[i]); //write register
	}

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return;
}

void nrf24l01_setrxaddr0(uint8_t *addr)
{
	nrf24l01_writeregisters(NRF24L01_REG_RX_ADDR_P0, addr, 5);

	return;
}

void nrf24l01_settxaddr(uint8_t *addr)
{
	nrf24l01_writeregisters(NRF24L01_REG_RX_ADDR_P0, addr, 5); //set rx address for ack on pipe 0
	nrf24l01_writeregisters(NRF24L01_REG_TX_ADDR, addr, 5); //set tx address

	return;
}

void nrf24l01_flushRXfifo()
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_FLUSH_RX);

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return;
}

void nrf24l01_flushTXfifo()
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_FLUSH_TX);

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return;
}

void nrf24l01_setRX()
{
	//nrf24l01_setrxaddr(0, nrf24l01_addr0); //restore pipe 0 address

	nrf24l01_writeregister(NRF24L01_REG_CONFIG, nrf24l01_readregister(NRF24L01_REG_CONFIG) | (NRF24L01_REG_PRIM_RX)); //prx mode
	nrf24l01_writeregister(NRF24L01_REG_CONFIG, nrf24l01_readregister(NRF24L01_REG_CONFIG) | (NRF24L01_REG_PWR_UP)); //power up
	nrf24l01_writeregister(NRF24L01_REG_STATUS, (NRF24L01_REG_RX_DR) | (NRF24L01_REG_TX_DS) | (NRF24L01_REG_MAX_RT)); //reset status

	nrf24l01_flushRXfifo(); //flush rx fifo
	nrf24l01_flushTXfifo(); //flush tx fifo

	// high CE
	nrf24l01_CE_PORT |= _BV(nrf24l01_CE); // start listening

	_delay_us(150); //wait for the radio to power up
}

void nrf24l01_setTX()
{
	// low CE
	nrf24l01_CE_PORT &= ~_BV(nrf24l01_CE); // stop listening

	nrf24l01_writeregister(NRF24L01_REG_CONFIG, nrf24l01_readregister(NRF24L01_REG_CONFIG) & ~(NRF24L01_REG_PRIM_RX)); //ptx mode
	nrf24l01_writeregister(NRF24L01_REG_CONFIG, nrf24l01_readregister(NRF24L01_REG_CONFIG) | (NRF24L01_REG_PWR_UP)); //power up
	//nrf24l01_writeregister(NRF24L01_REG_STATUS, (NRF24L01_REG_RX_DR) | (NRF24L01_REG_TX_DS) | (NRF24L01_REG_MAX_RT)); //reset status
	nrf24l01_writeregister(NRF24L01_REG_STATUS, (NRF24L01_REG_TX_DS) | (NRF24L01_REG_MAX_RT)); //reset status

	nrf24l01_flushTXfifo(); //flush tx fifo

	_delay_us(150); //wait for the radio to power up
}

uint8_t nrf24l01_getstatus()
{
	uint8_t status = 0;

	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	status = SPI_rw(NRF24L01_CMD_NOP); //get status, send NOP request

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	return status;
}

void nrf24l01_read(uint8_t *data)
{
	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

    SPI_rw(NRF24L01_CMD_R_RX_PAYLOAD);

    for(uint8_t i=0; i<32; i++)
    {
    	data[i] = SPI_rw(NRF24L01_CMD_NOP);
	}

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

    // reset RX_DR bit
    //nrf24l01_writeregister(NRF24L01_REG_STATUS, NRF24L01_REG_RX_DR);

    /*
    //handle ack payload receipt
	if (nrf24l01_getstatus() & (NRF24L01_REG_TX_DS))
		nrf24l01_writeregister(NRF24L01_REG_STATUS, (NRF24L01_REG_TX_DS));
	*/

	return;
}

void nrf24l01_write(uint8_t *data)
{
	uint8_t i = 0;
	//uint8_t ret = 0;

	//set tx mode
	nrf24l01_setTX();

	//write data

	// low CSN
	nrf24l01_CSN_PORT &= ~_BV(nrf24l01_CSN);

	SPI_rw(NRF24L01_CMD_W_TX_PAYLOAD);
	for (i=0; i<32; i++)
	{
		SPI_rw(data[i]);
	}

	// high CSN
	nrf24l01_CSN_PORT |= _BV(nrf24l01_CSN);

	//start transmission
	// high CE
	nrf24l01_CE_PORT |= _BV(nrf24l01_CE);

	_delay_us(20); // 15uS

	// low CE
	nrf24l01_CE_PORT &= ~_BV(nrf24l01_CE);

	/*
	//stop if max_retries reached or send is ok
	do {
		_delay_us(10);
	}
	while( !(nrf24l01_getstatus() & (1<<NRF24L01_REG_MAX_RT | 1<<NRF24L01_REG_TX_DS)) );

	if(nrf24l01_getstatus() & 1<<NRF24L01_REG_TX_DS)
		ret = 1;

	//reset PLOS_CNT
	nrf24l01_writeregister(NRF24L01_REG_RF_CH, NRF24L01_CH);

	//power down
	nrf24l01_writeregister(NRF24L01_REG_CONFIG, nrf24l01_readregister(NRF24L01_REG_CONFIG) & ~(1<<NRF24L01_REG_PWR_UP));

	//set rx mode
	nrf24l01_setRX();

	return ret;
	*/

	return;
}

void nrf24l01_ce_high()
{
	// high CE
	nrf24l01_CE_PORT |= _BV(nrf24l01_CE); // start listening

	return;
}

void nrf24l01_ce_low()
{
	// low CE
	nrf24l01_CE_PORT &= ~_BV(nrf24l01_CE); // stop listening

	return;
}

/*
uint8_t nrf24l01_irq()
{
	if( nrf24l01_IRQ_PIN & _BV(nrf24l01_IRQ) )
		return 0;
	else
		return 1;
}
*/

//returns 1 if RX_DR interrupt is active, 0 otherwise
uint8_t nrf24l01_irq_rx_dr()
{
	return (nrf24l01_getstatus() & NRF24L01_REG_RX_DR);
}

//returns 1 if TX_DS interrupt is active, 0 otherwise
uint8_t nrf24l01_irq_tx_ds()
{
	return (nrf24l01_getstatus() & NRF24L01_REG_TX_DS);
}

//returns 1 if MAX_RT interrupt is active, 0 otherwise
uint8_t nrf24l01_irq_max_rt()
{
	return (nrf24l01_getstatus() & NRF24L01_REG_MAX_RT);
}

//clear all interrupts in the status register
void nrf24l01_irq_clear_all()
{
	nrf24l01_writeregister(NRF24L01_REG_STATUS, (NRF24L01_REG_RX_DR) | (NRF24L01_REG_TX_DS) | (NRF24L01_REG_MAX_RT)); // clear all interrupts
}

//clears only the RX_DR interrupt
void nrf24l01_irq_clear_rx_dr()
{
	nrf24l01_writeregister(NRF24L01_REG_STATUS, NRF24L01_REG_RX_DR);
}

//clears only the TX_DS interrupt
void nrf24l01_irq_clear_tx_ds()
{
	nrf24l01_writeregister(NRF24L01_REG_STATUS, NRF24L01_REG_TX_DS);
}

//clears only the MAX_RT interrupt
void nrf24l01_irq_clear_max_rt()
{
	nrf24l01_writeregister(NRF24L01_REG_STATUS, NRF24L01_REG_MAX_RT);
}
