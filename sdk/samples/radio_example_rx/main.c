#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers.h"
#include "optical.h"
#include "radio.h"
#include "rftimer.h"
#include "scm3c_hw_interface.h"

// Number of for loop cycles between Hello World messages.
// 700000 for loop cycles roughly correspond to 1 second.
#define NUM_CYCLES_BETWEEN_TX (1000000UL)

int main(void) {

    LC_FREQCHANGE(0,0,0);
    radio_rxEnable();

    uint32_t g_tx_counter = 0;
    while (1) {
        printf("Hello World! %lu\n", g_tx_counter++);
        busy_wait_cycles(NUM_CYCLES_BETWEEN_TX);
    }
}
