/**
SCuM programmer.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/structs/dma.h"
#include "hardware/regs/dma.h"
#include "hardware/irq.h"
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

// specific to RPI pico2
#define UART_ID uart1
#define BAUD_RATE 115200
// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

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

static void setup_uart(void) {

    // configure baud:
    float div = (125000000.0f) / (16.0f * (float)BAUD_RATE);
    uint32_t int_part = (uint32_t)div;
    uint32_t frac_part = (uint32_t)((div - int_part) * 64 + 0.5f);
    uart1_hw->ibrd = int_part;
    uart1_hw->fbrd = frac_part;

    uart1_hw->lcr_h =   (3 << UART_UARTLCR_H_WLEN_LSB) |  // 8 bits
                        (1 << UART_UARTLCR_H_FEN_LSB);    // FIFO enable

    uart1_hw->cr =  (1 << UART_UARTCR_UARTEN_LSB)   |
                    (1 << UART_UARTCR_TXE_LSB)      |
                    (1 << UART_UARTCR_RXE_LSB);

    // RX DMA
    dma_hw->ch[0].read_addr  = (uintptr_t)&uart1_hw->dr;
    dma_hw->ch[0].write_addr = (uintptr_t)&_programmer_vars.uart_rx_byte;
    dma_hw->ch[0].transfer_count = 1;

    dma_hw->ch[0].ctrl_trig =   (0 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) | 
                                (0 << DMA_CH0_CTRL_TRIG_INCR_READ_LSB)  |   // fixed read (DR)
                                (0 << DMA_CH0_CTRL_TRIG_INCR_WRITE_LSB) |   // fixed write (rx_byte)
                                (0 << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB) |     // retrigger self
                                DMA_CH0_CTRL_TRIG_EN_BITS;

    // TX DMA 
    dma_hw->ch[1].read_addr  = (uintptr_t)_programmer_vars.uart_tx_buf;
    dma_hw->ch[1].write_addr = (uintptr_t)&uart1_hw->dr;
    dma_hw->ch[1].transfer_count = 0; // set when sending

    dma_hw->ch[1].ctrl_trig =   (1 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) |
                                (1 << DMA_CH0_CTRL_TRIG_INCR_READ_LSB)  |  // increment buffer
                                (0 << DMA_CH0_CTRL_TRIG_INCR_WRITE_LSB) |  // fixed write (DR)
                                DMA_CH0_CTRL_TRIG_EN_BITS;

    // Enable UART DMA
    uart1_hw->dmacr = UART_UARTDMACR_RXDMAE_BITS | UART_UARTDMACR_TXDMAE_BITS;
    
    // enable DMA0 interrupts:
    dma_hw->inte0 = 1;

}




int64_t alarm_callback(alarm_id_t id, void *user_data) {
    // Put your timeout handler code in here
    return 0;
}




int main()
{
    stdio_init_all();

    // Timer example code - This example fires off the callback after 2000ms
    add_alarm_in_ms(2000, alarm_callback, NULL, false);
    // For more examples of timer use see https://github.com/raspberrypi/pico-examples/tree/master/timer

    printf("System Clock Frequency is %d Hz\n", clock_get_hz(clk_sys));
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));
    // For more examples of clocks use see https://github.com/raspberrypi/pico-examples/tree/master/clocks

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\n");
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}