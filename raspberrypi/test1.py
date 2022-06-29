from nrf24 import NRF24
from time import sleep
import binascii
import sys
import time
from os.path import exists
import leekoq

# RF Commands
RF_CMD_ABORT                = 0x2496  # aborts current command
RF_CMD_POLICE               = 0x7683  # police lights
RF_CMD_CAMERA               = 0x5628  # speed camera flash
RF_CMD_GETTELE              = 0x6087  # request for telemetry data
RF_CMD_TELEDATA             = 0x7806  # telemetry data packet (we send this)
RF_CMD_NEWKEY               = 0x4706  # keeloq key change
RF_CMD_SLEEP                = 0x9075  # goes to sleep mode. any other command wakes us up
RF_CMD_RTCDATA              = 0x5519  # RTC data
RF_CMD_GETRTC               = 0xE55C  # request of current RTC
RF_CMD_SETRADIOTMRS         = 0x3366  # set long & short inactivity sleep intervals, and wakeuper interval
RF_CMD_CONFIGPOLICE         = 0x6A3C  # configuring the police light blinker
RF_CMD_BLUECAMERA           = 0xB449  # blue speed camera flash
RF_CMD_SETTELEM             = 0x99BA  # set telemetry interval parameters
RF_CMD_SETRFCHAN            = 0x560C  # set radio RF channel
RF_CMD_SYSRESET             = 0xDEAD  # reset the ATMega328P
RF_CMD_STAYAWAKE            = 0x57A1  # stay awake for *param* minutes

# Default system stuff
DEFAULT_RF_CHANNEL          = 14                    # default rf channel for nRF24L01+ radio
DEFAULT_KEY                 = 0x1223344556788990    # default keeloq key
DEFAULT_TX_COUNTER          = 1000                  # for sending to device
DEFAULT_RX_COUNTER          = 1000                  # for verification of receiving from device

print("Loading settings...")

### LOAD PARAMETERS FROM FILE(S)
### TODO: do this the right way, save as one "config"
### object to file and load upon startup, don't be a fool

# load rf channel from file
rf_channel_file = './rf_channel.txt'
if exists(rf_channel_file):
    f = open(rf_channel_file, 'r')
    rf_channel = int(f.readline())
    f.close()

    print("RF CHANNEL LOADED: " + str(rf_channel))
else:
    rf_channel = DEFAULT_RF_CHANNEL
    print("RF CHANNEL IS DEFAULT: " + str(rf_channel))

# load keeloq key from file
key_file = './key.txt'
if exists(key_file):
    f = open(key_file, 'r')
    key = int(f.readline())
    f.close()
    
    print("KEY LOADED: " + str(hex(key)))
else:
    key = DEFAULT_KEY
    print("KEY IS DEFAULT: " + str(hex(key)))

# load TX counter from file
tx_counter_file = './tx_counter.txt'
if exists(tx_counter_file):
    f = open(tx_counter_file, 'r')
    tx_counter = int(f.readline())
    f.close()
    
    print("TX COUNTER LOADED: " + str(tx_counter))
else:
    tx_counter = DEFAULT_TX_COUNTER
    print("TX COUNTER IS DEFAULT: " + str(tx_counter))

# load RX counter from file
rx_counter_file = './rx_counter.txt'
if exists(rx_counter_file):
    f = open(rx_counter_file, 'r')
    rx_counter = int(f.readline())
    f.close()
    
    print("RX COUNTER LOADED: " + str(rx_counter))
else:
    rx_counter = DEFAULT_RX_COUNTER
    print("RX COUNTER IS DEFAULT: " + str(rx_counter))

print("Initializing nRF radio...")
pipes = [[77, 66, 44, 33, 88]]
radio = NRF24()
radio.begin(0, 0, 23, 24)  # Set CE and IRQ pins
radio.setRetries(5, 15)
radio.setPayloadSize(32)
radio.setChannel(rf_channel)
radio.setCRCLength(NRF24.CRC_16)
radio.setDataRate(NRF24.BR_250KBPS)
radio.setPALevel(NRF24.PA_MAX)
radio.openWritingPipe(pipes[0])
radio.openReadingPipe(0, pipes[0])
radio.flush_rx()
radio.printDetails()
radio.startListening()

print("Radio initialized.")

# building keeloq header
def build_header(command, counter, crypt_key):
    header = command << 16
    header = header + counter
    return leekoq.encrypt(header, crypt_key)

while True:
    radio.stopListening()

    # request telemetry data
    command = RF_CMD_GETTELE
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    # prvo probudi uredjaj   
    while True:
        if radio.write(rf_packet):
            print("TX RequestTelemetry OK ("+str(tx_counter)+")...")
            tx_counter = tx_counter + 1
            break
        else:
            print("TX RequestTelemetry ERR!!! Retry in 1s")
            radio.startListening()
            sleep(1)
            radio.stopListening()

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
    # change the RF channel to 100
    command = RF_CMD_SETRFCHAN
    rf_channel = 100
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + rf_channel.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX SetRFChann OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX SetRFChann ERR!!!")

    # save new rf channel
    f = open(rf_channel_file,'w')
    f.write('{}'.format(rf_channel))
    f.close()
    """
    
    """
    # reset the device remotely
    command = RF_CMD_SYSRESET
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    # rf_packet = rf_packet + rf_channel.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX SysReset OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX SysReset ERR!!!")
    """

    """    
    # change the KeeLoq key
    command = RF_CMD_NEWKEY
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    key = 0xDEADBEEFDEADBEEF
    rf_packet = rf_packet + key.to_bytes(8, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX KeyChange OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX KeyChange ERR!!!")

    # save new key to file
    f = open(key_file,'w')
    f.write('{}'.format(key))
    f.close()
    """

    """
    # blue camera blink
    command = RF_CMD_BLUECAMERA
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    # rf_packet = rf_packet + rf_channel.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX BlueCam OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX BlueCam ERR!!!")
    """
    
    # stay awake for 5 minutes test
    command = RF_CMD_STAYAWAKE
    seconds = 5 * 60
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + seconds.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX StayAwake OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX StayAwake ERR!!!")

    """
    # go to sleep immediatelly
    command = RF_CMD_SLEEP
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX Sleep OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX Sleep ERR!!!")
    """

    """
    # set RTC to device
    command = RF_CMD_RTCDATA
    yy = 22
    mm = 6
    dd = 29
    hh = 15
    mi = 35
    ss = 00
    wd = 3
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)

    rf_packet = rf_packet + yy.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + mm.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + dd.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + hh.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + mi.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + ss.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + wd.to_bytes(1, sys.byteorder)
    
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX SetRTC OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX SetRTC ERR!!!")
    """
    
    """
    # get RTC from the device
    command = RF_CMD_GETRTC
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)
    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX GetRTC OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX GetRTC ERR!!!")

    radio.startListening()    
    
    sleep(1)
    
    pipe =[0]
    if radio.available(pipe):
        print("Received RTC:")
        recv_buffer = []
        radio.read(recv_buffer)
        print(binascii.hexlify(bytearray(recv_buffer)))
    """
    
    # config police lights params

    command = RF_CMD_CONFIGPOLICE
    police_lights_stage_count = 4
    police_lights_stage_on_8ms = 5
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
    rf_packet = rf_packet + command.to_bytes(2, sys.byteorder)

    rf_packet = rf_packet + police_lights_stage_count.to_bytes(1, sys.byteorder)
    rf_packet = rf_packet + police_lights_stage_on_8ms.to_bytes(1, sys.byteorder)

    rf_packet = rf_packet + bytearray(32 - len(rf_packet))
    
    # print(binascii.hexlify(rf_packet))

    if radio.write(rf_packet):
        print("TX ConfigPolice OK ("+str(tx_counter)+")...")
        tx_counter = tx_counter + 1
    else:
        print("TX ConfigPolice ERR!!!")

    
    # police lights
    command = RF_CMD_POLICE
    rf_packet = bytearray()
    rf_packet = rf_packet + build_header(command, tx_counter, key).to_bytes(4, sys.byteorder)
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
    f = open(tx_counter_file,'w')
    f.write('{}'.format(tx_counter))
    f.close()

    print("Sleeping 10sec...");
    sleep(10)

radio.end()
