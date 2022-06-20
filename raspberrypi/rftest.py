from nrf24 import NRF24
from time import sleep
import binascii
import sys
import time
from os.path import exists
import leekoq

def build_header(command, counter):
    header = command << 16
    header = header + counter
    return leekoq.encrypt(header, key)

print("(re)Initializing nRF radio")
pipes = [[77, 66, 44, 33, 88]]
radio = NRF24()
radio.begin(0, 0, 23, 24)  # Set CE and IRQ pins
radio.setRetries(5, 15)
radio.setPayloadSize(32)
radio.setChannel(14)
radio.setCRCLength(NRF24.CRC_16)
radio.setDataRate(NRF24.BR_250KBPS)
radio.setPALevel(NRF24.PA_MAX)
radio.openWritingPipe(pipes[0])
radio.openReadingPipe(0, pipes[0])
radio.flush_rx()
radio.printDetails()
radio.startListening()

key = 0x1223344556788990

# load counter from file
cnt_file = './tx_counter.txt'
if exists(cnt_file):
    f = open(cnt_file, 'r')
    tx_counter = int(f.readline())
    f.close()
    
    print("TX COUNTER LOADED: " + str(tx_counter))
else:
    tx_counter = 1000
    print("TX COUNTER INIT TO 1000")

while True:
    radio.stopListening()

    # test 1
    # request telemetry data
    command = 0x6087
    rf_packet = bytearray()
    # rf_packet = rf_packet + tx_counter.to_bytes(2, sys.byteorder)
    # rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + build_header(command, tx_counter).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX RequestTelemetry OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX RequestTelemetry ERR!!!")
    
    radio.startListening()    
    
    sleep(1)
    
    pipe =[0]
    if radio.available(pipe):
        print("Received data:")
        recv_buffer = []
        radio.read(recv_buffer)
        print(bytearray(recv_buffer[6:recv_buffer.index(36)]).decode('ascii'))
    
    radio.stopListening()
    
    """
    print("Wait for data...")
    pipe =[0]
    while not radio.available(pipe):
        time.sleep(10000/1000000.0)

    print("Receive Data:")
    recv_buffer = []
    radio.read(recv_buffer)

    #Print the buffer
    print(bytearray(recv_buffer[6:recv_buffer.index(36)]).decode('ascii'))
    """

    # test 2
    # police lights
    command = 0x7683
    rf_packet = bytearray()
    # rf_packet = rf_packet + tx_counter.to_bytes(2, sys.byteorder) # bajgi counter, debugging
    # rf_packet = rf_packet + 0x7683.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + build_header(command, tx_counter).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + 0x02.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX PoliceLights OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX PoliceLights ERR!!!")

    radio.startListening()

    # save keeloq counter to file for next startup
    f = open(cnt_file,'w')
    f.write('{}'.format(tx_counter))
    f.close()    

    print("Sleeping 10sec...");
    sleep(10)

radio.end()
