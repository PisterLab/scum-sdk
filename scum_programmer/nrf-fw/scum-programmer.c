/**
SCuM programmer.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nrf.h>
#include <nrf52840_peripherals.h>

#include "hdlc.h"

//=========================== defines =========================================

#define UART_BUF_SIZE    (32U)
#define COMMAND_BUF_SIZE (2048U)
#define CHUNK_SIZE       (1024U)
#define SCUM_MEM_SIZE    (1 << 16)  // 64KiB

#define CALIBRATION_PORT    0UL
#define PROGRAMMER_EN_PIN   30UL
#define PROGRAMMER_HRST_PIN 31UL
#define PROGRAMMER_CLK_PIN  28UL
#define PROGRAMMER_DATA_PIN 29UL
#define PROGRAMMER_TAP_PIN  3UL

#define CALIBRATION_CLK_PIN          28UL
#define CALIBRATION_PULSE_WIDTH      50   // approximate duty cycle (out of 100)
#define CALIBRATION_PERIOD           100  // period in ms
#define CALIBRATION_FUDGE            308  // # of clock cycles of "fudge"
#define CALIBRATION_NUMBER_OF_PULSES 10   // # of rising edges at 100ms

#define PROGRAMMER_VDDD_HI_PIN 27UL
#define PROGRAMMER_VDDD_LO_PIN 15UL

#define GPIOTE_CALIBRATION_CLOCK 0

//=========================== variables =======================================

typedef enum {
    COMMAND_START = 0x01,
    COMMAND_CHUNK = 0x02,
    COMMAND_BOOT = 0x03,
    COMMAND_CALIBRATE = 0x04,
} command_type_t;

typedef struct __attribute__((packed)) {
    command_type_t type;
    uint8_t buffer[COMMAND_BUF_SIZE];
} uart_command_t;

typedef struct {
    bool uart_byte_received;
    uint8_t uart_rx_byte;
    uint8_t uart_tx_buf[UART_BUF_SIZE];
    uart_command_t uart_command;

    uint32_t chunk_idx;

    bool calibration_done;
    uint32_t calibration_counter;
} programmer_vars_t;

static programmer_vars_t _programmer_vars = { 0 };

static const char *UART_ACK = "ACK\n";

static void busy_wait_ms(uint32_t ms) {
    uint32_t cycles = 3000 * ms;
    while (cycles--) {
        asm volatile("" :::);
    }
}

static void setup_clock(void) {
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (!NRF_CLOCK->EVENTS_HFCLKSTARTED) {}
}

static void setup_uart(void) {
    // configure
    NRF_UARTE0->CONFIG = 0;
    NRF_UARTE0->PSEL.TXD = (0 << UARTE_PSEL_TXD_PORT_Pos | 6 << UARTE_PSEL_TXD_PIN_Pos);
    NRF_UARTE0->PSEL.RXD = (0 << UARTE_PSEL_RXD_PORT_Pos | 8 << UARTE_PSEL_RXD_PIN_Pos);
    NRF_UARTE0->RXD.PTR = (uint32_t)&_programmer_vars.uart_rx_byte;
    NRF_UARTE0->RXD.MAXCNT = 1;  // Only receive one byte at a time
    NRF_UARTE0->BAUDRATE = UARTE_BAUDRATE_BAUDRATE_Baud460800 << UARTE_BAUDRATE_BAUDRATE_Pos;
    NRF_UARTE0->SHORTS = (UARTE_SHORTS_ENDRX_STARTRX_Enabled << UARTE_SHORTS_ENDRX_STARTRX_Pos);
    NRF_UARTE0->INTENSET = (UARTE_INTENSET_ENDRX_Enabled << UARTE_INTENSET_ENDRX_Pos);
    NRF_UARTE0->ENABLE = (UARTE_ENABLE_ENABLE_Enabled << UARTE_ENABLE_ENABLE_Pos);
    NRF_UARTE0->TASKS_STARTRX = 1;

    // enable interrupts
    NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}

static void uart_write(const uint8_t *buffer, size_t len) {
    memcpy(_programmer_vars.uart_tx_buf, buffer, len);
    NRF_UARTE0->TXD.PTR = (uint32_t)_programmer_vars.uart_tx_buf;
    NRF_UARTE0->TXD.MAXCNT = len;
    NRF_UARTE0->EVENTS_ENDTX = 0;
    NRF_UARTE0->TASKS_STARTTX = 1;
    while (!NRF_UARTE0->EVENTS_ENDTX) {}
}

static void setup_programmer(void) {
    NRF_P0->PIN_CNF[PROGRAMMER_DATA_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                            GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    NRF_P0->PIN_CNF[PROGRAMMER_CLK_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                           GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    NRF_P0->PIN_CNF[PROGRAMMER_HRST_PIN] = GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos;  // 0x00 configures the pin as an input, input buffer disconnected, pull up/down disabled (no pull)
    NRF_P0->PIN_CNF[PROGRAMMER_EN_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                          GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    NRF_P0->PIN_CNF[PROGRAMMER_TAP_PIN] = GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos;  // default to hi-Z
    NRF_P0->PIN_CNF[PROGRAMMER_VDDD_HI_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                               GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos |
                                               GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
    NRF_P1->PIN_CNF[PROGRAMMER_VDDD_LO_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                               GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos |
                                               GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
    ;

    NRF_P0->OUTSET = 1 << PROGRAMMER_VDDD_HI_PIN;
    NRF_P1->OUTCLR = 1 << PROGRAMMER_VDDD_LO_PIN;
}

static void setup_gpiote(void) {
    NRF_GPIOTE->CONFIG[GPIOTE_CALIBRATION_CLOCK] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos |
                                                    CALIBRATION_CLK_PIN << GPIOTE_CONFIG_PSEL_Pos |
                                                    CALIBRATION_PORT << GPIOTE_CONFIG_PORT_Pos |
                                                    GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos |
                                                    GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);
}

static void setup_timer2(void) {
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER2->PRESCALER = 0;  // set prescaler to zero => 16MHz timer

    NRF_TIMER2->CC[1] = 40;
    NRF_TIMER2->CC[2] = CALIBRATION_PERIOD * 16000 - CALIBRATION_FUDGE;  // artificially remove the N clk cycle delay in the PPI
    NRF_TIMER2->SHORTS = TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos;
}

static void setup_ppi(void) {
    // connect endpoints
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[2];
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CALIBRATION_CLOCK];
    NRF_PPI->FORK[0].TEP = (uint32_t)&NRF_TIMER2->TASKS_START;

    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[1];
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CALIBRATION_CLOCK];

    // enable channels
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos) | (PPI_CHENSET_CH1_Enabled << PPI_CHENSET_CH1_Pos);
}

static void run_calibration(void) {
    setup_gpiote();
    setup_timer2();
    setup_ppi();

    NVIC_EnableIRQ(TIMER2_IRQn);
    NRF_TIMER2->INTENCLR = TIMER_INTENCLR_COMPARE2_Enabled << TIMER_INTENCLR_COMPARE2_Pos;
    NRF_TIMER2->INTENSET = TIMER_INTENSET_COMPARE2_Enabled << TIMER_INTENSET_COMPARE2_Pos;
    NRF_TIMER2->TASKS_START = 1;

    while (!_programmer_vars.calibration_done) {
        asm volatile("" :::);
    }
    _programmer_vars.calibration_counter = 0;
    _programmer_vars.calibration_done = false;
}

static void bitband_byte(uint8_t byte, bool latch) {
    for (uint8_t j = 0; j < 8; j++) {
        if ((byte >> j) & 0x01) {
            NRF_P0->OUTSET = 1 << PROGRAMMER_DATA_PIN;
        } else if (!((byte >> j) & 0x01)) {
            NRF_P0->OUTCLR = 1 << PROGRAMMER_DATA_PIN;
        }
        if (latch && (j == 7)) {
            NRF_P0->OUTSET = 1 << PROGRAMMER_EN_PIN;
        } else {
            NRF_P0->OUTCLR = 1 << PROGRAMMER_EN_PIN;
        }
        // toggle the clock
        NRF_P0->OUTSET = 1 << PROGRAMMER_CLK_PIN;
        NRF_P0->OUTCLR = 1 << PROGRAMMER_CLK_PIN;
    }
}

static void _process_command(void) {
    hdlc_decode((uint8_t *)&_programmer_vars.uart_command);
    switch (_programmer_vars.uart_command.type) {
        case COMMAND_START:
        {
            puts("START");
            _programmer_vars.chunk_idx = 0;

            NRF_P0->OUTCLR = 1 << PROGRAMMER_CLK_PIN;
            NRF_P0->OUTCLR = 1 << PROGRAMMER_DATA_PIN;
            NRF_P0->OUTCLR = 1 << PROGRAMMER_EN_PIN;
            // execute hard reset (debug for now)
            NRF_P0->PIN_CNF[PROGRAMMER_HRST_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                                    GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);  // configure as output, set low
            busy_wait_ms(14);
            NRF_P0->PIN_CNF[PROGRAMMER_HRST_PIN] = GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos;  // return to input
            busy_wait_ms(14);
            break;
        }
        case COMMAND_CHUNK:
        {
            for (uint32_t idx = 1; idx < CHUNK_SIZE + 1; idx++) {
                bitband_byte(_programmer_vars.uart_command.buffer[idx - 1], (idx % 4 == 0));
            }
            _programmer_vars.chunk_idx++;
            break;
        }
        case COMMAND_BOOT:
        {
            puts("BOOT");
            uint32_t received_bytes = _programmer_vars.chunk_idx * CHUNK_SIZE;
            uint32_t remaining_bytes = SCUM_MEM_SIZE - received_bytes;
            for (uint32_t idx = 1; idx < remaining_bytes + 1; idx++) {
                bitband_byte(0x00, (idx % 4 == 0));
            }

            NRF_P0->OUTSET = (1 << PROGRAMMER_TAP_PIN);  // first set pin high - NEVER CLEAR!!! scum will hate it if you do
            NRF_P0->PIN_CNF[PROGRAMMER_TAP_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                                   GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);  // then enable output
            break;
        }
        case COMMAND_CALIBRATE:
            puts("CALIBRATE");
            run_calibration();
            break;
        default:
            break;
    }

    uart_write((uint8_t *)UART_ACK, strlen(UART_ACK));
}

int main(void) {
    setup_clock();
    setup_uart();
    setup_programmer();

    while (1) {
        if (_programmer_vars.uart_byte_received) {
            hdlc_state_t state = hdlc_rx_byte(_programmer_vars.uart_rx_byte);
            if (state == HDLC_STATE_READY) {
                _process_command();
            }
            _programmer_vars.uart_byte_received = false;
        }

        // wait for event
        __SEV();  // set event
        __WFE();  // wait for event
        __WFE();  // wait for event
    }
}

void TIMER2_IRQHandler(void) {
    // handle compare[1]
    if (NRF_TIMER2->EVENTS_COMPARE[1]) {
        NRF_TIMER2->EVENTS_COMPARE[1] = 0;
        NRF_TIMER2->CC[1] = 0;
    }

    // handle compare[2]
    if (NRF_TIMER2->EVENTS_COMPARE[2]) {
        // no need to clear - it is done w/ a short
        NRF_TIMER2->TASKS_CLEAR = 1;
        NRF_TIMER2->EVENTS_COMPARE[2] = 0;
        _programmer_vars.calibration_counter++;

        // re-enable CC[1]
        NRF_TIMER2->CC[1] = 300;

        if (_programmer_vars.calibration_counter > 10) {
            NRF_P0->PIN_CNF[PROGRAMMER_TAP_PIN] = GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos;  // turn the tap off after 5 100ms cycles!
        }

        if (_programmer_vars.calibration_counter > CALIBRATION_NUMBER_OF_PULSES) {
            _programmer_vars.calibration_counter = 0;

            NRF_TIMER2->TASKS_STOP = 1;  // stop the count!
            NRF_GPIOTE->CONFIG[GPIOTE_CALIBRATION_CLOCK] = 0;
            NRF_P0->PIN_CNF[CALIBRATION_CLK_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos |
                                                    GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
            _programmer_vars.calibration_done = true;
        }
    }
}

void UARTE0_UART0_IRQHandler(void) {
    if (NRF_UARTE0->EVENTS_ENDRX) {
        NRF_UARTE0->EVENTS_ENDRX = 0;
        if (NRF_UARTE0->RXD.AMOUNT != 0) {
            _programmer_vars.uart_byte_received = true;
        }
    }
}
