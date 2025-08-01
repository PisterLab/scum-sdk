#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers.h"
#include "optical.h"
#include "radio.h"
#include "rftimer.h"
#include "scm3c_hw_interface.h"
#include "scum.h"

static uint8_t packet[125] = {0};
static uint8_t packet_len = 125;
static int8_t rxpk_rssi = 0;
static uint8_t rxpk_lqi = 0;

static void rx_endframe_callback(uint32_t timestamp);

int main(void) {

    printf("Radio Receiver Example\n");

    radio_init();
    LC_FREQCHANGE(0,20,15);
    radio_setEndFrameRxCb(rx_endframe_callback);
    radio_rxEnable();
    radio_rxNow();

    while (1) {
        __WFE();
    }
}

void rx_endframe_callback(uint32_t timestamp) {
    
    radio_getReceivedFrame(     &(packet[0]), 
                                &packet_len,
                                sizeof(packet), 
                                &rxpk_rssi,
                                &rxpk_lqi      );
    
    for (uint8_t i = 0; i < packet_len - 2; i++) {
        printf("%d ", packet[i]);
    }
    printf("\n");

    radio_rxEnable();
    radio_rxNow();
                    
}
