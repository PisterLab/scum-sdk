#include <stdio.h>
#include <string.h>

#include "scum.h"
#include "rftimer.h"

// ========================== definition ======================================

#define RFTIMER_MAX_COUNT 0xffffffff

#define MINIMUM_COMPAREVALE_ADVANCE 5
#define LARGEST_INTERVAL            0xffff
#define NUM_INTERRUPTS              8

// ========================== variable ========================================

typedef struct {
    rftimer_cbt rftimer_cbs[8];
    rftimer_cbt rftimer_action_cb;
    uint32_t last_compare_value;
    uint8_t noNeedClearFlag;
} rftimer_vars_t;

static rftimer_vars_t rftimer_vars = { 0 };

static bool delay_completed[NUM_INTERRUPTS] = { 0 };          // flag indicating whether the delay has
                                                              // completed. For use by
                                                              // delay_milliseoncds_synchronous method
static bool is_repeating[NUM_INTERRUPTS] = { 0 };             // flag indicating whether each COMPARE
                                                              // will repeat at a fixed rate
static unsigned int timer_durations[NUM_INTERRUPTS] = { 0 };  // indicates length each COMPARE
                                                              // interrupt was set to run for.
                                                              // Used for repeating delay.

// ========================== public ==========================================

void rftimer_init(void) {
    // set period of radiotimer
    SCUM_RFTIMER->MAX_COUNT = RFTIMER_MAX_COUNT;
    // enable timer and interrupt
    SCUM_RFTIMER->CONTROL = RFTIMER_REG__CONTROL_ENABLE |
                            RFTIMER_REG__CONTROL_INTERRUPT_ENABLE |
                            RFTIMER_REG__CONTROL_COUNT_RESET;
    NVIC_EnableIRQ(RFTIMER_IRQn);
}

void rftimer_set_callback(rftimer_cbt cb) {
    rftimer_set_callback_by_id(cb, 0);
}

void rftimer_set_callback_by_id(rftimer_cbt cb, uint8_t id) {
    rftimer_vars.rftimer_cbs[id] = cb;
}

void rftimer_setCompareIn(uint32_t val) {
    rftimer_setCompareIn_by_id(val, 0);
}

void rftimer_setCompareIn_by_id(uint32_t val, uint8_t id) {
    SCUM_RFTIMER->COMPARE[id] = val;
    rftimer_enable_interrupts_by_id(id);

    // A timer scheduled in the past

    // Note: this is a hack!!
    //   since the timer counter overflow after hitting the MAX_COUNT,
    //       theoretically, there is no way to find a timer is scheduled
    //       in the past or in the future. However, if we know the
    //       LARGEST_INTERVAL between each two adjacent timers: if the val -
    //       current count < largest interval:
    //           it's a timer scheduled in the future
    //       else:
    //           it's a timer scheduled in the past
    //           manually trigger an interrupt
    //       LARGEST_INTERVAL is application defined value.

    if ((val & SCUM_RFTIMER->MAX_COUNT) - SCUM_RFTIMER->COUNTER < LARGEST_INTERVAL) {
    } else {
        // seems doesn't work?
        SCUM_RFTIMER->INT = 1;
    }
}

uint32_t rftimer_readCounter(void) {
    return SCUM_RFTIMER->COUNTER;
}

// Enables the RF timer interrupt, which is required for individually enabled
// timer registers to fire. Any calls to rftimer_enable_interrupts_by_id must
// be followed (or preceded) by a call to this function.
void rftimer_enable_interrupts(void) {
    NVIC_EnableIRQ(RFTIMER_IRQn);
}

// Enables the RF timer interrupt for a specific timer register. The RF timer
// interrupt must be enabled for any timer compare registers to fire the
// interrupt.
void rftimer_enable_interrupts_by_id(uint8_t id) {
    // clear pending bit first
    rftimer_clear_interrupts_by_id(id);

    // enable compare interrupt (this also cancels any pending interrupts)
    SCUM_RFTIMER->COMPARE_CONTROL[id] = RFTIMER_COMPARE_ENABLE | RFTIMER_COMPARE_INTERRUPT_ENABLE;
}

// Disables the RF timer interrupt. This will disable all timer compare
// registers from firing the interrupt, but will not disable the registers
// themselves. To disable a specific timer compare register, use
// rftimer_disable_interrupts_by_id.
void rftimer_disable_interrupts(void) {
    NVIC_DisableIRQ(RFTIMER_IRQn);
}

// Disables the RF timer interrupt for a specific timer register.
void rftimer_disable_interrupts_by_id(uint8_t id) {
    // disable compare interrupt
    SCUM_RFTIMER->COMPARE_CONTROL[id] = 0;
}

void rftimer_clear_interrupts(void) {
    rftimer_clear_interrupts_by_id(0);
}

void rftimer_clear_interrupts_by_id(uint8_t id) {
    SCUM_RFTIMER->INT_CLEAR = (uint32_t)(1 << id);
}

// Sets a flag indicating whether to make the desired interrupt repeat right
// after finishing
void rftimer_set_repeat(bool should_repeat, uint8_t id) {
    is_repeating[id] = should_repeat;
}

/* Delays the chip for a period of time in milliseconds based off the rate
 * of the 500kHz RF TIMER. Internally, this function uses RFTIMER COMPARE 7.
 * This is an asynchronous delay, so the program can continue execution and
 * eventually the interrupt will be called indicating the end of the delay.
 * You will need to set callback if desired.
 *
 * @param delay_milli - the delay in milliseconds
 */
void delay_milliseconds_asynchronous(unsigned int delay_milli, uint8_t id) {
    // RF TIMER is derived from HF timer (20MHz) through a divide ratio of 40,
    // thus it is 500kHz. the count defined by SCUM_RFTIMER->MAX_COUNT and
    // SCUM_RFTIMER->COMPARE1 indicate how many counts the timer should go
    // through before triggering an interrupt. This is the basis for the
    // following calculation. For example a count of 0x0000C350 corresponds to
    // 100ms.
    unsigned int rf_timer_count = delay_milli * 500;  // same as (delay_milli * 500000) / 1000;
    rftimer_enable_interrupts_by_id(id);
    // rftimer_enable_interrupts();
    timer_durations[id] = delay_milli;

    rftimer_setCompareIn_by_id(rftimer_readCounter() + rf_timer_count, id);
}

/* Performs a delay that will not return until the delay has completed.
 * For additional documentation see the delay_milliseconds function.
 */
void delay_milliseconds_synchronous(unsigned int delay_milli, uint8_t id) {
    delay_completed[id] = false;

    delay_milliseconds_asynchronous(delay_milli, id);

    // do nothing until delay has finished
    while (!delay_completed[id]) {}
}

// ========================== interrupt =======================================

void handle_compare_interrupt(uint8_t id) {
    delay_completed[id] = true;  // used for delay synchronous function

    if (is_repeating[id]) {
        delay_milliseconds_asynchronous(timer_durations[id], id);
    }

    if (rftimer_vars.rftimer_cbs[id]) {
        rftimer_vars.rftimer_cbs[id]();
    }
}

void RFTIMER_Handler(void) {
    uint32_t interrupt = SCUM_RFTIMER->INT;
    SCUM_RFTIMER->INT_CLEAR = interrupt;

    switch (interrupt) {
        case RFTIMER_REG__INT_COMPARE0_INT:
            handle_compare_interrupt(0);
            break;
        case RFTIMER_REG__INT_COMPARE1_INT:
            handle_compare_interrupt(1);
            break;
        case RFTIMER_REG__INT_COMPARE2_INT:
            handle_compare_interrupt(2);
            break;
        case RFTIMER_REG__INT_COMPARE3_INT:
            handle_compare_interrupt(3);
            break;
        case RFTIMER_REG__INT_COMPARE4_INT:
            handle_compare_interrupt(4);
            break;
        case RFTIMER_REG__INT_COMPARE5_INT:
            handle_compare_interrupt(5);
            break;
        case RFTIMER_REG__INT_COMPARE6_INT:
            handle_compare_interrupt(6);
            break;
        case RFTIMER_REG__INT_COMPARE7_INT:
            handle_compare_interrupt(7);
            break;
#ifdef ENABLE_PRINTF
        case RFTIMER_REG__INT_CAPTURE0_INT:
            printf("CAPTURE0 TRIGGERED AT: 0x%lx\r\n", SCUM_RFTIMER->CAPTURE[0]);
            break;
        case RFTIMER_REG__INT_CAPTURE1_INT:
            printf("CAPTURE0 TRIGGERED AT: 0x%lx\r\n", SCUM_RFTIMER->CAPTURE[1]);
            break;
        case RFTIMER_REG__INT_CAPTURE2_INT:
            printf("CAPTURE0 TRIGGERED AT: 0x%lx\r\n", SCUM_RFTIMER->CAPTURE[2]);
            break;
        case RFTIMER_REG__INT_CAPTURE3_INT:
            printf("CAPTURE0 TRIGGERED AT: 0x%lx\r\n", SCUM_RFTIMER->CAPTURE[3]);
            break;
#endif
        default:
            break;
    }
}
