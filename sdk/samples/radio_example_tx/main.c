#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers.h"
#include "optical.h"
#include "radio.h"
#include "rftimer.h"
#include "scm3c_hw_interface.h"
#include "scum.h"

// Number of for loop cycles between Hello World messages.
// 700000 for loop cycles roughly correspond to 1 second.
#define NUM_CYCLES_BETWEEN_TX (1000000UL)
#define NUM_CYCLES_BETWEEN_PACKET (1000UL)
#define TX_PACKET_LEN (64UL)

void tx_endframe_callback(uint32_t timestamp);
uint8_t packet[TX_PACKET_LEN] = {0};

uint8_t fine_code = 0;
uint8_t mid_code = 0;

int main(void) {

    printf("Radio Transmitter Example\n");
    radio_init();
    LC_FREQCHANGE(0,0,0);
    radio_setEndFrameTxCb(tx_endframe_callback);
    radio_loadPacket(packet, TX_PACKET_LEN + 2);
    radio_txEnable();
    busy_wait_cycles(NUM_CYCLES_BETWEEN_PACKET);
    radio_txNow();

    while (1) {
        //printf("Hello World! %lu\n", g_tx_counter++);
        //busy_wait_cycles(NUM_CYCLES_BETWEEN_TX);
    }
}

void tx_endframe_callback(uint32_t timestamp) {
    radio_rfOff();
    printf("sent a packet\r\n");
    radio_loadPacket(packet, TX_PACKET_LEN + 2);
    radio_txEnable();
    busy_wait_cycles(NUM_CYCLES_BETWEEN_PACKET);
    radio_txNow();
}
