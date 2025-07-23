#include <stdio.h>
#include <string.h>

#include "scum.h"
#include "radio.h"
#include "scm3c_hw_interface.h"
#include "scum_defs.h"

//=========================== defines =========================================

#define CALIBRATION_ITERATIONS 10U

//=========================== variables =======================================

typedef struct {
    uint8_t optical_cal_iteration;
    uint8_t optical_cal_finished;

    uint32_t num_32k_ticks_in_100ms;
    uint32_t num_2MRC_ticks_in_100ms;
    uint32_t num_IFclk_ticks_in_100ms;
    uint32_t num_LC_ch11_ticks_in_100ms;
    uint32_t num_HFclock_ticks_in_100ms;

    // reference to calibrate
    uint32_t LC_target;
    uint32_t LC_code;
} optical_vars_t;

static optical_vars_t optical_vars = { 0 };

//=========================== prototypes ======================================

//=========================== public ==========================================

void optical_init(void) {

    // Target radio LO freq = 2.4025G
    // Divide ratio is currently 480*2
    // Calibration counts for 100ms
    optical_vars.LC_target = REFERENCE_LC_TARGET;
    optical_vars.LC_code = DEFAULT_INIT_LC_CODE;
}

void optical_enable(void) {
    NVIC_EnableIRQ(EXT_GPIO8_ACTIVEHIGH_IRQn);
    NVIC_EnableIRQ(OPTICAL_SFD_IRQn);
}

void perform_calibration(void) {
#ifndef NDEBUG
    puts("Performing calibration...");
#endif

#if defined(MODULE_RADIO)
    // For the LO, calibration for RX channel 11, so turn on AUX, IF, and LO
    // LDOs by calling radio rxEnable
    radio_rxEnable();
#endif

    // Enable optical SFD interrupt for optical calibration
    optical_enable();

    // Wait for optical cal to finish
    while (!optical_vars.optical_cal_finished) {
        __asm volatile("wfi");
    }

#if defined(MODULE_RADIO)
    // Disable the radio now that it is calibrated
    radio_rfOff();
#endif

#ifndef NDEBUG
    printf("Calibration complete\r\n");
#endif
}

//=========================== interrupt =======================================

// This interrupt goes off every time 32 new bits of data have been shifted into
// the optical register Do not recommend trying to do any CPU intensive actions
// while trying to receive optical data ex, printf will mess up the received
// data values
void EXT_OPTICAL_IRQ_IN_Handler(void) {
    // printf("Optical 32-bit interrupt triggered\r\n");

    // unsigned int LSBs, MSBs, optical_shiftreg;
    // int t;

    // 32-bit register is analog_rdata[335:304]
    // LSBs = SCUM_ANALOG_CFG_REG_19; //16 LSBs
    // MSBs = SCUM_ANALOG_CFG_REG_20; //16 MSBs
    // optical_shiftreg = (MSBs << 16) + LSBs;

    // Toggle GPIO 0
    // GPIO_REG__OUTPUT ^= 0x1;
}

// This interrupt goes off when the optical register holds the value {221, 176,
// 231, 47} This interrupt can also be used to synchronize to the start of an
// optical data transfer Need to make sure a new bit has been clocked in prior
// to returning from this ISR, or else it will immediately execute again
void OPTICAL_SFD_Handler(void) {
    // 1.1V/VDDD tap fix
    // helps reorder assembly code
    // Not completely sure why this works
    optical_vars.optical_cal_iteration++;

    uint32_t HF_CLOCK_fine = scm3c_hw_interface_get_HF_CLOCK_fine();
    uint32_t HF_CLOCK_coarse = scm3c_hw_interface_get_HF_CLOCK_coarse();
    uint32_t RC2M_coarse = scm3c_hw_interface_get_RC2M_coarse();
    uint32_t RC2M_fine = scm3c_hw_interface_get_RC2M_fine();
    uint32_t RC2M_superfine = scm3c_hw_interface_get_RC2M_superfine();
    uint32_t IF_clk_target = scm3c_hw_interface_get_IF_clk_target();
    uint32_t IF_coarse = scm3c_hw_interface_get_IF_coarse();
    uint32_t IF_fine = scm3c_hw_interface_get_IF_fine();

    // Disable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x007F;

    // Keep track of how many calibration iterations have been completed

    // Read 32k counter
    uint32_t count_32k = SCUM_ANALOG_CFG_REG_0 + (SCUM_ANALOG_CFG_REG_1 << 16);

    // Read HF_CLOCK counter
    uint32_t count_HFclock = SCUM_ANALOG_CFG_REG_4 + (SCUM_ANALOG_CFG_REG_5 << 16);

    // Read 2M counter
    uint32_t count_2M = SCUM_ANALOG_CFG_REG_6 + (SCUM_ANALOG_CFG_REG_7 << 16);

    // Read LC_div counter (via counter4)
    uint32_t count_LC = SCUM_ANALOG_CFG_REG_10 + (SCUM_ANALOG_CFG_REG_11 << 16);

    // Read IF ADC_CLK counter
    uint32_t count_IF = SCUM_ANALOG_CFG_REG_12 + (SCUM_ANALOG_CFG_REG_13 << 16);

    // Reset all counters
    SCUM_ANALOG_CFG_REG_0 = 0x0000;

    // Enable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x3FFF;

    // Don't make updates on the first two executions of this ISR
    if (optical_vars.optical_cal_iteration > 2) {
        // Do correction on HF CLOCK
        // Fine DAC step size is about 6000 counts
        if (count_HFclock < 1997000) {  // 1997000 original value
            if (HF_CLOCK_fine == 0) {
                HF_CLOCK_coarse--;
                HF_CLOCK_fine = 10;
                // optical_vars.optical_cal_iteration = 3;
            } else {
                HF_CLOCK_fine--;
            }
        } else if (count_HFclock > 2003000) {  // new value I picked was 2010000, originally 2003000
            if (HF_CLOCK_fine == 31) {
                HF_CLOCK_coarse++;
                HF_CLOCK_fine = 23;
                // optical_vars.optical_cal_iteration = 3;
            } else {
                HF_CLOCK_fine++;
            }
        }

        set_sys_clk_secondary_freq(HF_CLOCK_coarse, HF_CLOCK_fine);
        scm3c_hw_interface_set_HF_CLOCK_coarse(HF_CLOCK_coarse);
        scm3c_hw_interface_set_HF_CLOCK_fine(HF_CLOCK_fine);

        // Do correction on LC
        if (count_LC > optical_vars.LC_target) {
            optical_vars.LC_code -= 1;
        }
        if (count_LC < optical_vars.LC_target) {
            optical_vars.LC_code += 1;
        }
        LC_monotonic(optical_vars.LC_code);

        // Do correction on 2M RC
        // Coarse step ~1100 counts, fine ~150 counts, superfine ~25
        // Too fast
        if (count_2M > 200600) {
            RC2M_coarse += 1;
        } else if (count_2M > 200080) {
            RC2M_fine += 1;
        } else if (count_2M > 200015) {
            RC2M_superfine += 1;
        }

        // Too slow
        if (count_2M < 199400) {
            RC2M_coarse -= 1;
        } else if (count_2M < 199920) {
            RC2M_fine -= 1;
        } else if (count_2M < 199985) {
            RC2M_superfine -= 1;
        }

        set_2M_RC_frequency(31, 31, RC2M_coarse, RC2M_fine, RC2M_superfine);
        scm3c_hw_interface_set_RC2M_coarse(RC2M_coarse);
        scm3c_hw_interface_set_RC2M_fine(RC2M_fine);
        scm3c_hw_interface_set_RC2M_superfine(RC2M_superfine);

        // Do correction on IF RC clock
        // Fine DAC step size is ~2800 counts
        if (count_IF > (IF_clk_target + 1400)) {
            IF_fine += 1;
        }
        if (count_IF < (IF_clk_target - 1400)) {
            IF_fine -= 1;
        }

        // if calibration resulted in tuning overflow, iterate coarse code
        if (IF_fine >= 32) {
            IF_fine -= 8;
            IF_coarse += 1;
        }

        set_IF_clock_frequency(IF_coarse, IF_fine, 0);
        scm3c_hw_interface_set_IF_coarse(IF_coarse);
        scm3c_hw_interface_set_IF_fine(IF_fine);

        analog_scan_chain_write();
        analog_scan_chain_load();
    }

    // Debugging output
    // 1.1V/VDDD tap fix
    // The print is now broken down into 3 statements instead of one big
    // print statement
    // doing this prevent a long string of loads back to back
    printf(
        "HF=%lu-%lu-%lu   ",
        count_HFclock, HF_CLOCK_fine, HF_CLOCK_coarse);
    printf(
        "2M=%lu-%lu,%lu,%lu   ",
        count_2M, RC2M_coarse,
        RC2M_fine, RC2M_superfine);
    printf(
        "LC=%lu-%lu   ",
        count_LC, optical_vars.LC_code);
#if defined(MODULE_RADIO)
    printf(
        "IF=%lu-%lu",
        count_IF, IF_fine);
#endif
    puts("");

    if (optical_vars.optical_cal_iteration == CALIBRATION_ITERATIONS) {

        // Disable this ISR
        NVIC_DisableIRQ(EXT_GPIO8_ACTIVEHIGH_IRQn);
        NVIC_DisableIRQ(OPTICAL_SFD_IRQn);
        optical_vars.optical_cal_iteration = 0;
        optical_vars.optical_cal_finished = 1;

        // Store the last count values
        optical_vars.num_32k_ticks_in_100ms = count_32k;
        optical_vars.num_2MRC_ticks_in_100ms = count_2M;
        optical_vars.num_IFclk_ticks_in_100ms = count_IF;
        optical_vars.num_LC_ch11_ticks_in_100ms = count_LC;
        optical_vars.num_HFclock_ticks_in_100ms = count_HFclock;

        // DEBUG: program one last time... some chips don't work?

        // Debug prints
        // printf("LC_code=%d\r\n", optical_vars.LC_code);
        // printf("IF_fine=%d\r\n", IF_fine);

        // This was an earlier attempt to build out a complete table of LC_code
        // for TX/RX on each channel It doesn't really work well yet so leave it
        // commented
        // printf("Building channel table...");

        // radio_build_channel_table(LC_code);

        // printf("done\r\n");

        // radio_disable_all();

        // Halt all counters
        SCUM_ANALOG_CFG_REG_0 = 0x0000;
    }
}
