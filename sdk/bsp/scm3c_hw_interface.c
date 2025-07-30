#include <stdio.h>
#include <string.h>

#include "scum.h"
#include "helpers.h"
#include "scm3c_hw_interface.h"
#include "optical.h"
#include "radio.h"
#include "rftimer.h"
#include "scum_defs.h"

//=========================== definition ======================================

#define ASC_LEN            38
#define DAC_2M_SETTING_LEN 5

// initialized value for frequency configuration
#define INIT_HF_CLOCK_FINE   17
#define INIT_HF_CLOCK_COARSE 3
#define INIT_RC2M_COARSE     21
#define INIT_RC2M_FINE       15
#define INIT_RC2M_SUPERFINE  15
#define INIT_IF_CLK_TARGET   1600000
#define INIT_IF_COARSE       22
#define INIT_IF_FINE         18

// CRC
#define CRC_VALUE   (*((volatile unsigned int *)0x0000FFFC))
#define CODE_LENGTH (*((volatile unsigned int *)0x0000FFF8))

//=========================== variable ========================================

// default setting
static const uint32_t default_dac_2m_setting[] = { 31, 31, 29, 2, 2 };

typedef struct {
    uint32_t ASC[ASC_LEN];
    uint32_t dac_2M_settings[DAC_2M_SETTING_LEN];

    // HF_CLOCK tuning settings
    uint32_t HF_CLOCK_fine;
    uint32_t HF_CLOCK_coarse;

    // RC 2MHz tuning settings
    // This the transmitter chip clock
    uint32_t RC2M_coarse;
    uint32_t RC2M_fine;
    uint32_t RC2M_superfine;

    // Receiver clock settings
    // The receiver chip clock is derived from this clock
    uint32_t IF_clk_target;
    uint32_t IF_coarse;
    uint32_t IF_fine;
} scm3c_hw_interface_vars_t;

scm3c_hw_interface_vars_t scm3c_hw_interface_vars = { 0 };

//=========================== prototype =======================================

static void _update_bit(uint32_t position, bool set) {
    if (set) {
        set_asc_bit(position);
    } else {
        clear_asc_bit(position);
    }
}

//=========================== public ==========================================

//==== admin

void scm3c_hw_interface_init(void) {

    // HF_CLOCK tuning settings
    scm3c_hw_interface_vars.HF_CLOCK_fine = INIT_HF_CLOCK_FINE;
    scm3c_hw_interface_vars.HF_CLOCK_coarse = INIT_HF_CLOCK_COARSE;

    // RC 2MHz tuning settings
    // This the transmitter chip clock
    scm3c_hw_interface_vars.RC2M_coarse = INIT_RC2M_COARSE;
    scm3c_hw_interface_vars.RC2M_fine = INIT_RC2M_FINE;
    scm3c_hw_interface_vars.RC2M_superfine = INIT_RC2M_SUPERFINE;

    // Receiver clock settings
    // The receiver chip clock is derived from this clock
    scm3c_hw_interface_vars.IF_clk_target = INIT_IF_CLK_TARGET;
    scm3c_hw_interface_vars.IF_coarse = INIT_IF_COARSE;
    scm3c_hw_interface_vars.IF_fine = INIT_IF_FINE;

    // coarse1, coarse2, coarse3, fine, superfine dac settings
    memcpy(&scm3c_hw_interface_vars.dac_2M_settings[0], default_dac_2m_setting, DAC_2M_SETTING_LEN);
}

//==== get/set functions

//==== get functions
uint32_t scm3c_hw_interface_get_HF_CLOCK_fine(void) {
    return scm3c_hw_interface_vars.HF_CLOCK_fine;
}
uint32_t scm3c_hw_interface_get_HF_CLOCK_coarse(void) {
    return scm3c_hw_interface_vars.HF_CLOCK_coarse;
}
uint32_t scm3c_hw_interface_get_RC2M_coarse(void) {
    return scm3c_hw_interface_vars.RC2M_coarse;
}
uint32_t scm3c_hw_interface_get_RC2M_fine(void) {
    return scm3c_hw_interface_vars.RC2M_fine;
}
uint32_t scm3c_hw_interface_get_RC2M_superfine(void) {
    return scm3c_hw_interface_vars.RC2M_superfine;
}
uint32_t scm3c_hw_interface_get_IF_clk_target(void) {
    return scm3c_hw_interface_vars.IF_clk_target;
}
uint32_t scm3c_hw_interface_get_IF_coarse(void) {
    return scm3c_hw_interface_vars.IF_coarse;
}
uint32_t scm3c_hw_interface_get_IF_fine(void) {
    return scm3c_hw_interface_vars.IF_fine;
}

//===== set function

void scm3c_hw_interface_set_HF_CLOCK_fine(uint32_t value) {
    scm3c_hw_interface_vars.HF_CLOCK_fine = value;
}
void scm3c_hw_interface_set_HF_CLOCK_coarse(uint32_t value) {
    scm3c_hw_interface_vars.HF_CLOCK_coarse = value;
}
void scm3c_hw_interface_set_RC2M_coarse(uint32_t value) {
    scm3c_hw_interface_vars.RC2M_coarse = value;
}
void scm3c_hw_interface_set_RC2M_fine(uint32_t value) {
    scm3c_hw_interface_vars.RC2M_fine = value;
}
void scm3c_hw_interface_set_RC2M_superfine(uint32_t value) {
    scm3c_hw_interface_vars.RC2M_superfine = value;
}
void scm3c_hw_interface_set_IF_clk_target(uint32_t value) {
    scm3c_hw_interface_vars.IF_clk_target = value;
}
void scm3c_hw_interface_set_IF_coarse(uint32_t value) {
    scm3c_hw_interface_vars.IF_coarse = value;
}
void scm3c_hw_interface_set_IF_fine(uint32_t value) {
    scm3c_hw_interface_vars.IF_fine = value;
}

void scm3c_hw_interface_set_asc(uint32_t *asc_profile) {
    memcpy(&scm3c_hw_interface_vars.ASC[0], asc_profile, ASC_LEN * sizeof(uint32_t));
}

//==== from scm3c_hardware_interface.h

// Reverses (reflects) bits in a 32-bit word.
unsigned reverse(unsigned x) {
    x = ((x & 0x55555555) << 1) | ((x >> 1) & 0x55555555);
    x = ((x & 0x33333333) << 2) | ((x >> 2) & 0x33333333);
    x = ((x & 0x0F0F0F0F) << 4) | ((x >> 4) & 0x0F0F0F0F);
    x = (x << 24) | ((x & 0xFF00) << 8) | ((x >> 8) & 0xFF00) | (x >> 24);
    return x;
}

// Computes 32-bit crc from a starting address over 'length' dwords
unsigned int crc32c(unsigned char *message, unsigned int length) {
    int i, j;
    unsigned int byte, crc;

    i = 0;
    crc = 0xFFFFFFFF;
    while (i < length) {
        byte = message[i];          // Get next byte.
        byte = reverse(byte);       // 32-bit reversal.
        for (j = 0; j <= 7; j++) {  // Do eight times.
            if ((int)(crc ^ byte) < 0) {
                crc = (crc << 1) ^ 0x04C11DB7;
            } else {
                crc = crc << 1;
            }
            byte = byte << 1;  // Ready next msg bit.
        }
        i = i + 1;
    }
    return reverse(~crc);
}

void crc_check(void) {
    uint32_t calc_crc;

    // Check CRC to ensure there were no errors during optical programming
    printf("\r\n-------------------\r\n");
    printf("Validating program integrity...");

    calc_crc = crc32c(0x0000, CODE_LENGTH);

    if (calc_crc == CRC_VALUE) {
        printf("CRC OK\r\n");
    } else {
        printf(
            "\r\nProgramming Error - CRC DOES NOT MATCH - Halting "
            "Execution\r\n");
        while (1)
            ;
    }
}

unsigned char flipChar(unsigned char b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

void GPO_control(uint8_t row1, uint8_t row2, uint8_t row3, uint8_t row4) {

    for (uint8_t j = 0; j < 4; j++) {
        _update_bit(245 + j, (row1 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 4; j++) {
        _update_bit(249 + j, (row2 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 4; j++) {
        _update_bit(253 + j, (row3 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 4; j++) {
        _update_bit(257 + j, (row4 >> j) & 0x1);
    }
}

void GPI_control(uint8_t row1, uint8_t row2, uint8_t row3, uint8_t row4) {
    for (uint8_t j = 0; j < 2; j++) {
        _update_bit(261 + j, (row1 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 2; j++) {
        _update_bit(263 + j, (row2 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 2; j++) {
        _update_bit(265 + j, (row3 >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 2; j++) {
        _update_bit(267 + j, (row4 >> j) & 0x1);
    }
}

// Enable output drivers for GPIO based on 'mask'
// '1' = output enabled, so GPO_enables(0xFFFF) enables all output drivers
// GPO enables are active low on-chip
void GPO_enables(unsigned int mask) {
    // out_en<0:15> =
    // ASC<1131>,ASC<1133>,ASC<1135>,ASC<1137>,ASC<1140>,ASC<1142>,ASC<1144>,ASC<1146>,...
    // ASC<1115>,ASC<1117>,ASC<1119>,ASC<1121>,ASC<1124>,ASC<1126>,ASC<1128>,ASC<1130>
    const uint16_t asc_locations[16] = {
        1131, 1133, 1135, 1137, 1140, 1142, 1144, 1146,
        1115, 1117, 1119, 1121, 1124, 1126, 1128, 1130
    };

    for (uint8_t j = 0; j < 16; j++) {
        _update_bit(asc_locations[j], !((mask >> j) & 0x1));
    }
}

// Enable input path for GPIO based on 'mask'
// '1' = input enabled, so GPI_enables(0xFFFF) enables all inputs
// GPI enables are active high on-chip
void GPI_enables(unsigned int mask) {
    // in_en<0:15> =
    // ASC<1132>,ASC<1134>,ASC<1136>,ASC<1138>,ASC<1139>,ASC<1141>,ASC<1143>,ASC<1145>,...
    // ASC<1116>,ASC<1118>,ASC<1120>,ASC<1122>,ASC<1123>,ASC<1125>,ASC<1127>,ASC<1129>
    const uint16_t asc_locations[16] = {
        1132, 1134, 1136, 1138, 1139, 1141, 1143, 1145,
        1116, 1118, 1120, 1122, 1123, 1125, 1127, 1129
    };

    for (uint8_t j = 0; j < 16; j++) {
        _update_bit(asc_locations[j], (mask >> j) & 0x1);
    }
}

// Configure how radio and AUX LDOs are turned on and off
void init_ldo_control(void) {
    // Analog scan chain setup for radio LDOs
    // Memory mapped control signals from the cortex are connected to fsm_pon
    // signals
    clear_asc_bit(501);  // = scan_pon_if
    clear_asc_bit(502);  // = scan_pon_lo
    clear_asc_bit(503);  // = scan_pon_pa
    clear_asc_bit(504);  // = gpio_pon_en_if
    set_asc_bit(505);    // = fsm_pon_en_if
    clear_asc_bit(506);  // = gpio_pon_en_lo
    set_asc_bit(507);    // = fsm_pon_en_lo
    clear_asc_bit(508);  // = gpio_pon_en_pa
    set_asc_bit(509);    // = fsm_pon_en_pa
    set_asc_bit(510);    // = master_ldo_en_if
    set_asc_bit(511);    // = master_ldo_en_lo
    set_asc_bit(512);    // = master_ldo_en_pa
    clear_asc_bit(513);  // = scan_pon_div
    clear_asc_bit(514);  // = gpio_pon_en_div
    set_asc_bit(515);    // = fsm_pon_en_div
    set_asc_bit(516);    // = master_ldo_en_div

    // Initialize all radio LDOs off but leave AUX on
    SCUM_ANALOG_CFG_REG_10 = 0x0000;

    // AUX LDO Control:
    // ASC<914> chooses whether ASC<916> or analog_cfg<167> controls LDO
    // 0 = ASC<916> has control
    // 1 = analog_cfg<167> has control
    // Enable is inverted so 0=on
    set_asc_bit(914);
    // set_asc_bit(916);

    // Initialize all radio LDOs and AUX to off
    // SCUM_ANALOG_CFG_REG_10 = 0x0000;

    // Examples of controlling AUX LDO:

    // Turn on aux ldo from analog_cfg<167>
    // SCUM_ANALOG_CFG_REG_10 = 0x0080;

    // Aux LDO  = off via ASC
    // clear_asc_bit(914);
    // set_asc_bit(916);

    // Aux LDO  = on via ASC
    // clear_asc_bit(914);
    // clear_asc_bit(916);

    // Memory-mapped LDO control
    // SCUM_ANALOG_CFG_REG_10 = AUX_EN | DIV_EN | PA_EN | IF_EN | LO_EN | PA_MUX |
    // IF_MUX | LO_MUX For MUX signals, '1' = FSM control, '0' = memory mapped
    // control For EN signals, '1' = turn on LDO (except for AUX which is
    // inverted)

    // Some examples:

    // Assert all PON_XX signals for the radio via memory mapped register
    // SCUM_ANALOG_CFG_REG_10 = 0x0078;

    // Turn off all PON_XX signals for the radio via memory mapped register
    // SCUM_ANALOG_CFG_REG_10 = 0x0000;

    // Turn on only LO via memory mapped register
    // SCUM_ANALOG_CFG_REG_10 = 0x0008;

    // Give FSM control of all radio PON signals
    // SCUM_ANALOG_CFG_REG_10 = 0x0007;

    // Debug visibility
    // PON signals are available on GPIO<4-7> on bank 5
    // GPIO<4> = PON_LO
    // GPIO<5> = PON_PA
    // GPIO<6> = PON_IF
    // GPIO<7> = PON_DIV
    // GPO_control(0,5,0,0);
    // Enable the output direction
}

// SRAM Verification Test
// BW 2-25-18
// Adapted from March C- algorithm, Eqn (2) in [1]
// [1] Van De Goor, Ad J. "Using march tests to test SRAMs." IEEE Design & Test
// of Computers 10.1 (1993): 8-14. Only works for DMEM since you must be able to
// read and write
unsigned int sram_test(unsigned int *baseAddress, unsigned int num_dwords) {
    unsigned int *addr = baseAddress;
    uint32_t num_errors = 0;

    printf("\r\n\r\nStarting SRAM test from 0x%p to 0x%p...\r\n", (void *)baseAddress,
           (void *)(baseAddress + num_dwords));
    printf("This takes awhile...\r\n");

    // Write 0 to all bits, in any address order
    // Outer loop selects 32-bit dword, inner loop does single bit
    for (unsigned int i = 0; i < num_dwords; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            addr[i] &= ~(1UL << j);
        }
    }

    // (r0,w1) (incr addr)
    for (unsigned int i = 0; i < num_dwords; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            if ((addr[i] & (1UL << j)) != 0x0) {
                printf("\r\nERROR 1 @ address %X bit %d -- Value is %X",
                       i, j, addr[i]);
                num_errors++;
            }
            addr[i] |= 1UL << j;
        }
    }

    // (r1,w0) (incr addr)
    for (unsigned int i = 0; i < num_dwords; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            if ((addr[i] & (1UL << j)) != 1UL << j) {
                printf("\r\nERROR 2 @ address %X bit %d -- Value is %X",
                       i, j, addr[i]);
                num_errors++;
            }
            addr[i] &= ~(1UL << j);
        }
    }

    // (r0,w1) (decr addr)
    for (unsigned int i = (num_dwords - 1); i >= 0; i--) {
        for (uint8_t j = 31; j >= 0; j--) {
            if ((addr[i] & (1UL << j)) != 0x0) {
                printf("\r\nERROR 3 @ address %X bit %d -- Value is %X",
                       i, j, addr[i]);
                num_errors++;
            }
            addr[i] |= 1UL << j;
        }
    }

    // (r1,w0) (decr addr)
    for (unsigned int i = (num_dwords - 1); i >= 0; i--) {
        for (uint8_t j = 31; j >= 0; j--) {
            if ((addr[i] & (1UL << j)) != 1UL << j) {
                printf("\r\nERROR 4 @ address %X bit %d -- Value is %X",
                       i, j, addr[i]);
                num_errors++;
            }
            addr[i] &= ~(1UL << j);
        }
    }

    // r0 (any order)
    for (unsigned int i = 0; i < num_dwords; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            if ((addr[i] & (1UL << j)) != 0x0) {
                printf("\r\nERROR 5 @ address %X bit %d -- Value is %X",
                       i, j, addr[i]);
                num_errors++;
            }
        }
    }

    printf("\r\nSRAM Test Complete -- %lu Errors\r\n", num_errors);

    return num_errors;
}

// Change the reference voltage for the IF LDO
// 0 <= code <= 127
void set_IF_LDO_voltage(int code) {
    // ASC<492:498> = if_ldo_rdac<0:6> (<0:6(MSB)>)

    for (uint8_t j = 0; j < 7; j++) {
        _update_bit(492 + j, (code >> j) & 0x1);
    }
}

// Change the reference voltage for the VDDD LDO
// 0 <= code <= 127
void set_VDDD_LDO_voltage(int code) {
    // ASC(791:1:797) (LSB:MSB)

    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(797 + j, (code >> j) & 0x1);
    }

    // Two MSBs are inverted
    for (uint8_t j = 5; j < 7; j++) {
        _update_bit(797 + j, !((code >> j) & 0x1));
    }
}

// Change the reference voltage for the AUX LDO
// 0 <= code <= 127
void set_AUX_LDO_voltage(int code) {
    // ASC(923:-1:917) (MSB:LSB)

    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(917 + j, (code >> j) & 0x1);
    }

    // Two MSBs are inverted
    for (uint8_t j = 5; j < 7; j++) {
        _update_bit(917 + j, !((code >> j) & 0x1));
    }
}

// Change the reference voltage for the always-on LDO
// 0 <= code <= 127
void set_ALWAYSON_LDO_voltage(int code) {
    // ASC(924:929) (MSB:LSB)

    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(929 + j, (code >> j) & 0x1);
    }

    // MSB of normal DAC is inverted
    _update_bit(924, !((code >> 5) & 0x1));

    // Panic bit was added for 3B (inverted)
    _update_bit(557, !((code >> 6) & 0x1));
}

// Must set IF clock frequency AFTER calling this function

void set_zcc_demod_threshold(unsigned int thresh) {
    // counter threshold 122:107 MSB:LSB
    for (uint8_t j = 0; j < 16; j++) {
        _update_bit(107 + j, (thresh >> j) & 0x1);
    }
}

// Set the divider value for ZCC demod
// Should be equal to (IF_clock_rate / 2 MHz)
void set_IF_ZCC_clkdiv(unsigned int div_value) {
    // CLK_DIV = ASC<131:124> MSB:LSB
    for (uint8_t j = 0; j < 8; j++) {
        _update_bit(124 + j, (div_value >> j) & 0x1);
    }
}

// Set the early decision value for ZCC demod
void set_IF_ZCC_early(unsigned int early_value) {
    // ASC<224:209> MSB:LSB
    for (uint8_t j = 0; j < 16; j++) {
        _update_bit(209 + j, (early_value >> j) & 0x1);
    }
}

// Untested function
void set_IF_stg3gm_ASC(unsigned int Igm, unsigned int Qgm) {
    // Set all bits to zero
    for (uint8_t j = 0; j < 13; j++) {
        clear_asc_bit((uint32_t)472 + j);
        clear_asc_bit((uint32_t)278 + j);
    }

    // 472:484 = I stg3 gm 13:1
    for (uint8_t j = 0; j <= Igm; j++) {
        set_asc_bit((uint32_t)484 - j);
    }

    // 278:290 = Q stg3 gm 1:13
    for (uint8_t j = 0; j <= Qgm; j++) {
        clear_asc_bit((uint32_t)278 + j);
    }
}

// Adjust the comparator offset trim for I channel
// Valid input range 0-31
void set_IF_comparator_trim_I(unsigned int ptrim, unsigned int ntrim) {
    // I comparator N side = 452:456 LSB:MSB
    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(452 + j, (ntrim >> j) & 0x1);
    }

    // I comparator P side = 457:461 LSB:MSB
    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(457 + j, (ntrim >> j) & 0x1);
    }
}

// Adjust the comparator offset trim for Q channel
// Valid input range 0-31
void set_IF_comparator_trim_Q(unsigned int ptrim, unsigned int ntrim) {
    // I comparator N side = 340:344 MSB:LSB
    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(344 - j, (ntrim >> j) & 0x1);
    }

    // I comparator P side = 335:339 MSB:LSB
    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(339 - j, (ntrim >> j) & 0x1);
    }
}

// Untested function
void set_IF_gain_ASC(unsigned int Igain, unsigned int Qgain) {
    // 485:490 = I code 0:5
    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(485 + j, (Igain >> j) & 0x1);
    }

    // 272:277 = Q code 5:0
    for (uint8_t j = 0; j < 6; j++) {
        _update_bit(277 - j, (Qgain >> j) & 0x1);
    }
}

void radio_init_rx_MF() {

    // IF uses ASC<271:500>, mask off outside that range
    uint32_t mask1 = 0xFFFC0000;
    uint32_t mask2 = 0x000007FF;
    scm3c_hw_interface_vars.ASC[8] &= mask1;
    scm3c_hw_interface_vars.ASC[15] &= mask2;

    // A large number of bits in the radio scan chain have no need to be changed
    // These values were exported from Matlab during radio testing
    // Same settings as used for 122418 ADC data captures
    scm3c_hw_interface_vars.ASC[8] |= (0x4050FFE0 & ~mask1);   // 256-287
    scm3c_hw_interface_vars.ASC[9] = 0x00422188;               // 288-319
    scm3c_hw_interface_vars.ASC[10] = 0x88040031;              // 320-351
    scm3c_hw_interface_vars.ASC[11] = 0x113B4081;              // 352-383
    scm3c_hw_interface_vars.ASC[12] = 0x027E8102;              // 384-415
    scm3c_hw_interface_vars.ASC[13] = 0x03ED4844;              // 416-447
    scm3c_hw_interface_vars.ASC[14] = 0x60010000;              // 448-479
    scm3c_hw_interface_vars.ASC[15] |= (0xFFE02E03 & ~mask2);  // 480-511

    // Set clock mux to internal RC oscillator
    clear_asc_bit(424);
    set_asc_bit(425);

    // Set gain for I and Q (63 is the max)
    set_IF_gain_ASC(63, 63);

    // Set gm for stg3 ADC drivers
    // Sets the transconductance for the third amplifier which drives the ADC
    // (7 was experimentally found to be about the best choice)
    set_IF_stg3gm_ASC(7, 7);  //(I, Q)

    // Set comparator trims
    // These allow you to trim the comparator offset for both I and Q channels
    // Shouldn't make much of a difference in matched filter mode, but can for
    // zero-crossing demod Only way to observe effect of trim is to adjust and
    // look for increase/decrease in packet error rate
    set_IF_comparator_trim_I(0, 0);  //(p,n)
    set_IF_comparator_trim_Q(0, 0);  //(p,n)

    // Setup baseband

    // Choose matched filter demod
    // ASC<0:1> = [0 0]
    clear_asc_bit(0);
    clear_asc_bit(1);

    // IQ source select
    // '0' = from radio
    // '1' = from GPIO
    clear_asc_bit(96);

    // Automatic Gain Control Setup

    // Set gain control signals to come from ASC
    clear_asc_bit(271);
    clear_asc_bit(491);

    // ASC<100> = envelope detector
    // '0' to choose envelope detector,
    // '1' chooses original scm3 overload detector
    clear_asc_bit(100);

    // VGA gain select mux {102=MSB, 101=LSB}
    // Chooses the source of gain control signals connected to analog
    // 00 = AGC FSM
    // 01 or 10 = analog_cfg
    // 11 = GPIN
    clear_asc_bit(101);
    clear_asc_bit(102);

    // Activate TIA only mode
    // '1' = only control gain of TIA
    // '0' = control gain of TIA and stage1/2
    set_asc_bit(97);

    // Memory mapped config registers
    // analog_cfg[239:224]    AGC        {gain_imbalance_select 1, gain_offset
    // 3, vga_ctrl_Q_analogcfg 6, vga_ctrl_I_analogcfg 6} SCUM_ANALOG_CFG_REG_14
    // analog_cfg[255:240]    AGC        {envelope_threshold 4, wait_time 12}
    // SCUM_ANALOG_CFG_REG_15
    // gain_imbalance_select
    // '0' = subtract 'gain_offset' from Q channel
    // '1' = subtract 'gain_offset' from I channel
    // envelope_threshold = the max-min value of signal that will cause gain
    // reduction wait_time = how long FSM waits for settling before making
    // another adjustment
    SCUM_ANALOG_CFG_REG_14 = 0x0000;
    SCUM_ANALOG_CFG_REG_15 = 0xA00F;

    // Matched Filter/Clock & Data Recovery
    // Choose output polarity of demod
    // If RX LO is 2.5MHz below the channel, use ASC<103>=1
    // This bit just inverts the output data bits
    set_asc_bit(103);

    // CDR feedback parameters
    // Determined experimentally - unlikely to need to ever change
    uint8_t tau_shift = 11;
    uint8_t e_k_shift = 2;
    SCUM_ANALOG_CFG_REG_3 = (tau_shift << 11) | (e_k_shift << 7);

    // Threshold used for packet detection
    // This number corresponds to the Hamming distance threshold for determining
    // if incoming 15.4 chip stream is a packet
    uint8_t correlation_threshold = 5;
    SCUM_ANALOG_CFG_REG_9 = correlation_threshold;

    // Mux select bits to choose internal demod or external clk/data from gpio
    // '0' = on chip, '1' = external from GPIO
    clear_asc_bit(269);
    clear_asc_bit(270);

    // Set LDO reference voltage
    // Best performance was found with LDO at max voltage (0)
    // Some performance can be traded for power by turning this voltage down
    set_IF_LDO_voltage(0);

    // Set RST_B to analog_cfg[75]
    // This chooses whether the reset for the digital blocks is connected to a
    // memory mapped register or a scan bit
    set_asc_bit(240);

    // Mixer and polyphase control settings can be driven from either ASC or
    // memory mapped I/O Mixers and polyphase should both be enabled for RX and
    // both disabled for TX
    // --
    // For polyphase (1=enabled),
    //     mux select signal ASC<746>=0 gives control to ASC<971>
    //    mux select signal ASC<746>=1 gives control to analog_cfg<256> (bit 0
    //    of SCUM_ANALOG_CFG_REG_16)
    // --
    // For mixers (0=enabled), both I and Q should be enabled for matched filter
    // mode
    //     mux select signals ASC<744>=0 and ASC<745>=0 give control to ASC<298>
    //     and ASC<307>
    //    mux select signals ASC<744>=1 and ASC<745>=1 give control to
    //    analog_cfg<257> analog_cfg<258> (bits 1 and 2 in SCUM_ANALOG_CFG_REG_16)

    // Set mixer and polyphase control signals to memory mapped I/O
    set_asc_bit(744);
    set_asc_bit(745);
    set_asc_bit(746);

    // Enable both polyphase and mixers via memory mapped IO (...001 = 0x1)
    // To disable both you would invert these values (...110 = 0x6)
    SCUM_ANALOG_CFG_REG_16 = 0x1;
}

// Must set IF clock frequency AFTER calling this function
void radio_init_rx_ZCC() {

    // IF uses ASC<271:500>, mask off outside that range
    uint32_t mask1 = 0xFFFE0000;
    uint32_t mask2 = 0x000007FF;
    scm3c_hw_interface_vars.ASC[8] &= mask1;
    scm3c_hw_interface_vars.ASC[15] &= mask2;

    scm3c_hw_interface_vars.ASC[8] |= (0x0000FFF0 & ~mask1);   // 256-287
    scm3c_hw_interface_vars.ASC[9] = 0x00422188;               // 288-319
    scm3c_hw_interface_vars.ASC[10] = 0x88040071;              // 320-351
    scm3c_hw_interface_vars.ASC[11] = 0x100C4081;              // 352-383
    scm3c_hw_interface_vars.ASC[12] = 0x00188102;              // 384-415
    scm3c_hw_interface_vars.ASC[13] = 0x017FC844;              // 416-447
    scm3c_hw_interface_vars.ASC[14] = 0x70010001;              // 448-479
    scm3c_hw_interface_vars.ASC[15] |= (0xFFE00800 & ~mask2);  // 480-511

    // Set zcc demod parameters

    // Set clk/data mux to zcc
    // ASC<0:1> = [1 1]
    set_asc_bit(0);
    set_asc_bit(1);

    // Set counter threshold 122:107 MSB:LSB
    // for 76MHz, use 13
    set_zcc_demod_threshold(13);

    // Set clock divider value for zcc
    // The IF clock divided by this value must equal 2 MHz for 802.15.4
    set_IF_ZCC_clkdiv(38);

    // Set early decision margin to a large number to essentially disable it
    set_IF_ZCC_early(80);

    // Mux select bits to choose I branch as input to zcc demod
    set_asc_bit(238);
    set_asc_bit(239);

    // Mux select bits to choose internal demod or external clk/data from gpio
    // '0' = on chip, '1' = external from GPIO
    clear_asc_bit(269);
    clear_asc_bit(270);

    // Enable ZCC demod
    set_asc_bit(132);

    // Threshold used for packet detection
    uint8_t correlation_threshold = 5;
    SCUM_ANALOG_CFG_REG_9 = correlation_threshold;

    // Trim comparator offset
    set_IF_comparator_trim_I(0, 10);

    // Set LDO reference voltage
    set_IF_LDO_voltage(0);

    // Set RST_B to analog_cfg[75]
    set_asc_bit(240);

    // Leave baseband held in reset until RX activated
    // RST_B = 0 (it is active low)
    SCUM_ANALOG_CFG_REG_4 = 0x2000;
    SCUM_ANALOG_CFG_REG_4 = 0x2800;
}

void radio_init_tx() {
    // Set up 15.4 modulation source
    // ----
    // For FPGA, the TX modulation comes in at the external pad so need to set
    // the mod_logic mux to route this signal for modulation mod_logic<3:0> =
    // ASC<996:999> The two LSBs change the mux from cortex mod source to pad
    // The other bits are used for inverting the modulation bitstream
    // With these settings, the TX should start at +500 kHz above the channel
    // frequency A '1' data bit then causes the TX to decrease in frequency by 1
    // MHz (this generates proper 15.4 output) If for some reason you wanted to
    // start 500 kHz below the channel and step up by 1 MHz for a '1', then need
    // to change the settings here In the IC version, comment these two lines
    // out (they switch modulation source to the pad)
    // set_asc_bit(997);
    // set_asc_bit(996);
    // set_asc_bit(998);
    // set_asc_bit(999);
    // ----

    // Set 15.4 modulation tone spacing
    // ----
    // The correct tone spacing is 1 MHz.  This requires adjusting the cap DAC
    // in the TX The settings below are probably close enough
    // mod_15_4_tune<2:0> = ASC<1002:1000>
    set_asc_bit(1000);
    set_asc_bit(1001);
    set_asc_bit(1002);

    // set dummy bit to 1
    set_asc_bit(1003);
    // ----

    // If you need to adjust the tone spacing, turn on the LO and PA, and
    // uncomment the lines below one at a time to force the transmitter to each
    // of its two output tones Then adjust mod_15_4_tune until the spacing is
    // close to 1 MHz Note, I haven't tested this
    // -----------------
    // Force TX to output the 'high' FSK tone
    // set_asc_bit(999);
    // clear_asc_bit(998);

    // Force TX to output the 'low' FSK tone
    // clear_asc_bit(999);
    // set_asc_bit(998);
    // -----------------

    // Need to set analog_cfg<183> to 1 to select 15.4 for chips out
    SCUM_ANALOG_CFG_REG_11 = 0x0080;

    // Set current in LC tank
    set_LC_current(127);

    // Set LDO voltages for PA and LO
    set_PA_supply(63);
    set_LO_supply(127, 0);
}

void radio_init_divider(unsigned int div_value) {
    // Set divider LDO value to max
    set_DIV_supply(63, 0);

    // Set prescaler to div-by-2
    prescaler(4);

    // Activate 8MHz/20MHz output
    // set_asc_bit(1033);

    // set divider to div-by-480
    divProgram(480, 1, 1);

    // Set sel12 = 1 (choose whether x2 is active)
    // Want this set to 1 or else the divider output falling edges will be
    // messed up
    set_asc_bit(1012);
}

void read_counters_3B(unsigned int *count_2M, unsigned int *count_LC,
                      unsigned int *count_adc) {
    // Disable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x007F;

    // Read 2M counter
    *count_2M = SCUM_ANALOG_CFG_REG_6 + (SCUM_ANALOG_CFG_REG_7 << 16);

    // Read LC_div counter (via counter4)
    *count_LC = SCUM_ANALOG_CFG_REG_10 + (SCUM_ANALOG_CFG_REG_11 << 16);

    // Read adc counter
    *count_adc = SCUM_ANALOG_CFG_REG_12 + (SCUM_ANALOG_CFG_REG_12 << 16);

    // Reset all counters
    SCUM_ANALOG_CFG_REG_0 = 0x0000;

    // Enable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x3FFF;

    // printf("LC_count=%X\r\n",*count_LC);
    // printf("2M_count=%X\r\n",*count_2M);
    // printf("adc_count=%X\r\n\r\n",*count_adc);
}

// read IF estimate
unsigned int read_IF_estimate() {
    // Check valid flag
    if (SCUM_ANALOG_CFG_REG_16 & 0x400) {
        return SCUM_ANALOG_CFG_REG_16 & 0x3FF;
    } else {
        return 0;
    }
}

// Read Link Quality Indicator
unsigned int read_LQI() {
    return SCUM_ANALOG_CFG_REG_21 & 0xFF;
}

// Read RSSI - the gain control settings

unsigned int read_RSSI() {
    return SCUM_ANALOG_CFG_REG_15 & 0xF;
}

// set IF clock frequency
void set_IF_clock_frequency(int coarse, int fine, int high_range) {
    // Coarse and fine frequency tune, binary weighted
    // ASC<427:431> = RC_coarse<4:0> (<4(MSB):0>)
    // ASC<433:437> = RC_fine<4:0>   (<4(MSB):0>)

    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(431 - j, (coarse >> j) & 0x1);
    }

    for (uint8_t j = 0; j < 5; j++) {
        _update_bit(437 - j, (fine >> j) & 0x1);
    }

    // Switch between high and low speed ranges for IF RC:
    //'1' = high range
    // ASC<726> = RC_high_speed_mode
    _update_bit(726, high_range == 1);
}

// Set frequency for TI 20M oscillator
void set_sys_clk_secondary_freq(unsigned int coarse, unsigned int fine) {
    // coarse 0:4 = 860 861 875b 876b 877b
    // fine 0:4 870 871 872 873 874b

    for (uint8_t j = 0; j < 4; j++) {
        _update_bit(870 + j, (fine >> j) & 0x1);
    }
    _update_bit(874, !((fine >> 4) & 0x1));

    for (uint8_t j = 0; j < 2; j++) {
        _update_bit(860 + j, (coarse >> j) & 0x1);
    }
    for (uint8_t j = 2; j < 5; j++) {
        _update_bit(873 + j, !((coarse >> j) & 0x1));
    }
}

void initialize_mote() {
    scm3c_hw_interface_init();
#if defined(MODULE_OPTICAL)
    optical_init();
#endif

#if defined(MODULE_RADIO)
    radio_init();
#endif

#if defined(MODULE_RFTIMER)
    rftimer_init();
#endif

    //--------------------------------------------------------
    // SCM3C Analog Scan Chain Initialization
    //--------------------------------------------------------
    // Init LDO control
    init_ldo_control();

    // Set LDO reference voltages
    // set_VDDD_LDO_voltage(0);
    // set_AUX_LDO_voltage(0);
    // set_ALWAYSON_LDO_voltage(0);

    // Select banks for GPIO inputs
    GPI_control(0, 0, 1, 0);  // 1 in 3rd arg connects GPI8 to EXT_INTERRUPT<1>
                              // needed for 3WB cal

    // Select banks for GPIO outputs
    GPO_control(6, 6, 0,
                6);  // 0 in 3rd arg connects clk_3wb to GPO8 for 3WB cal

    // Set GPI enables
    // Hex entry 2: 0x1 = 1 = 0b0001 = GPI 8 on for 3WB cal clk interrupt
    GPI_enables(0x0100);

    // Set GPO enables
    GPO_enables(0xFFFF);

    // Set HCLK source as HF_CLOCK
    set_asc_bit(1147);

    // Set initial coarse/fine on HF_CLOCK
    // coarse 0:4 = 860 861 875b 876b 877b
    // fine 0:4 870 871 872 873 874b
    set_sys_clk_secondary_freq(scm3c_hw_interface_vars.HF_CLOCK_coarse,
                               scm3c_hw_interface_vars.HF_CLOCK_fine);

    // Set RFTimer source as HF_CLOCK
    set_asc_bit(1151);

    // Disable LF_CLOCK
    set_asc_bit(553);

    // HF_CLOCK will be trimmed to 20MHz, so set RFTimer div value to 40 to get
    // 500kHz (inverted, so 1101 0111)
    set_asc_bit(49);
    set_asc_bit(48);
    clear_asc_bit(47);
    set_asc_bit(46);
    clear_asc_bit(45);
    set_asc_bit(44);
    set_asc_bit(43);
    set_asc_bit(42);

    // Set 2M RC as source for chip CLK
    set_asc_bit(1156);

    // Enable 32k for cal
    set_asc_bit(623);

    // Enable passthrough on chip CLK divider
    set_asc_bit(41);

    // Init counter setup - set all to analog_cfg control
    // scm3c_hw_interface_vars.ASC[0] is leftmost
    // scm3c_hw_interface_vars.ASC[0] |= 0x6F800000;
    for (uint8_t t = 2; t < 9; t++) {
        set_asc_bit(t);
    }

//#if defined(MODULE_RADIO)
    // Init RX
    radio_init_rx_MF();

    // Init TX
    radio_init_tx();
//#endif

    // activate divider
    radio_init_divider(2000);

    // Set initial IF ADC clock frequency
    set_IF_clock_frequency(scm3c_hw_interface_vars.IF_coarse,
                           scm3c_hw_interface_vars.IF_fine, 0);

    // Set initial TX clock frequency
    set_2M_RC_frequency(31, 31, scm3c_hw_interface_vars.RC2M_coarse,
                        scm3c_hw_interface_vars.RC2M_fine,
                        scm3c_hw_interface_vars.RC2M_superfine);

    // Turn on RC 2M for cal
    set_asc_bit(1114);

    // Set initial LO frequency
    LC_monotonic(DEFAULT_INIT_LC_CODE);

#if defined(MODULE_RADIO)
    // Init divider settings
    radio_init_divider(2000);
#endif

    // Program analog scan chain
    analog_scan_chain_write();
    analog_scan_chain_load();
    //--------------------------------------------------------
}

unsigned int estimate_temperature_2M_32k() {

    // Reset all counters
    SCUM_ANALOG_CFG_REG_0 = 0x0000;

    // Enable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x3FFF;

    // Count for some arbitrary amount of time
    busy_wait_cycles(50000);

    // Disable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x007F;

    // Read 2M counter
    unsigned int count_2M = SCUM_ANALOG_CFG_REG_6 + (SCUM_ANALOG_CFG_REG_7 << 16);

    // Read 32k counter
    unsigned int count_32k = SCUM_ANALOG_CFG_REG_0 + (SCUM_ANALOG_CFG_REG_1 << 16);

    // printf("%d - %d - %d\r\n",count_2M,count_32k,(count_2M << 13) /
    // count_32k);

    return (count_2M << 13) / count_32k;
}

//==== from scm3_hardware_interface.h

// Reverse endianness of lower 16 bits
unsigned int flip_lsb8(unsigned int in) {
    uint32_t out = 0;
    out |= (0x01 & in) << 7;
    out |= (0x02 & in) << 5;
    out |= (0x04 & in) << 3;
    out |= (0x08 & in) << 1;

    out |= (0x10 & in) >> 1;
    out |= (0x20 & in) >> 3;
    out |= (0x40 & in) >> 5;
    out |= (0x80 & in) >> 7;

    return out;
}

void analog_scan_chain_write(void) {
    // analog_cfg<357> is resetb for chip shift register, so leave that high
    for (uint8_t i = 0; i < 38; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            // Write asc_reg to analog_cfg
            SCUM_ANALOG_CFG_REG_22 = 0x20 | !(scm3c_hw_interface_vars.ASC[38 - i - 1] & (1 << j));

            // Lower phi1
            SCUM_ANALOG_CFG_REG_22 &= ~(0x2);

            // Toggle phi2
            SCUM_ANALOG_CFG_REG_22 |= 0x4;
            SCUM_ANALOG_CFG_REG_22 &= ~(0x4);

            // Raise phi1
            SCUM_ANALOG_CFG_REG_22 |= 0x2;
        }
    }
}

void analog_scan_chain_load() {
    // Assert load signal (and cfg<357>)
    SCUM_ANALOG_CFG_REG_22 = 0x0028;

    // Lower load signal
    SCUM_ANALOG_CFG_REG_22 = 0x0020;
}
/* sets the 2 MHz RC DAC frequency.
-updates the local dac settings array
-flips endianness and sets the appropriate bits in the scanchain array
-writes it to the scanchain
-loads the scanchain
*/
void set_2M_RC_frequency(int coarse1, int coarse2, int coarse3, int fine,
                         int superfine) {

    /* update our local dac array */
    scm3c_hw_interface_vars.dac_2M_settings[0] = coarse1;
    scm3c_hw_interface_vars.dac_2M_settings[1] = coarse2;
    scm3c_hw_interface_vars.dac_2M_settings[2] = coarse3;
    scm3c_hw_interface_vars.dac_2M_settings[3] = fine;
    scm3c_hw_interface_vars.dac_2M_settings[4] = superfine;

    // make sure each argument is between 0-31, inclusive

    // scm3c_hw_interface_vars.ASC[34] covers 1088:1119
    uint32_t newval = scm3c_hw_interface_vars.ASC[34] & 0x8000001F;

    // flip endianness of each
    uint32_t newcoarse1 = (flip_lsb8(coarse1) >> 3) & 0x1F;
    uint32_t newcoarse2 = (flip_lsb8(coarse2) >> 3) & 0x1F;
    uint32_t newcoarse3 = (flip_lsb8(coarse3) >> 3) & 0x1F;
    uint32_t newfine = (flip_lsb8(fine) >> 3) & 0x1F;
    uint32_t newsuperfine = (flip_lsb8(superfine) >> 3) & 0x1F;

    newval |= newcoarse1 << 26;
    newval |= newcoarse2 << 21;
    newval |= newcoarse3 << 16;
    newval |= newfine << 11;
    newval |= newsuperfine << 6;

    // Enable bit
    newval |= 0x1 << 5;

    scm3c_hw_interface_vars.ASC[34] = newval;

    // write to analog scanchain and load
    // analog_scan_chain_write();
    // analog_scan_chain_load();

    // print_2MHz_DAC();
}

/* Initializes the 2MHz DAC with values set in the dac_2M_settings array. */
void initialize_2M_DAC(void) {
    set_2M_RC_frequency(scm3c_hw_interface_vars.dac_2M_settings[0],
                        scm3c_hw_interface_vars.dac_2M_settings[1],
                        scm3c_hw_interface_vars.dac_2M_settings[2],
                        scm3c_hw_interface_vars.dac_2M_settings[3],
                        scm3c_hw_interface_vars.dac_2M_settings[4]);
    // printf("Initialized 2MHz DAC\r\n");
    // print_2MHz_DAC();
}

void read_counters(unsigned int *count_2M, unsigned int *count_LC,
                   unsigned int *count_32k) {
    // Disable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x007F;

    // Read 2M counter
    *count_2M = SCUM_ANALOG_CFG_REG_6 + (SCUM_ANALOG_CFG_REG_7 << 16);

    // Read LC_div counter (via counter4)
    *count_LC = SCUM_ANALOG_CFG_REG_8 + (SCUM_ANALOG_CFG_REG_9 << 16);

    // Read 32k counter
    *count_32k = SCUM_ANALOG_CFG_REG_0 + (SCUM_ANALOG_CFG_REG_1 << 16);

    // Reset all counters
    SCUM_ANALOG_CFG_REG_0 = 0x0000;

    // Enable all counters
    SCUM_ANALOG_CFG_REG_0 = 0x3FFF;

    // printf("LC_count=%X\r\n",count_LC);
    // printf("2M_count=%X\r\n",count_2M);
    // printf("32k_count=%X\r\n\r\n",count_32k);
}

void update_PN31_byte(unsigned int *current_lfsr) {
    for (uint8_t i = 0; i < 8; i++) {
        int newbit = (((*current_lfsr >> 30) ^ (*current_lfsr >> 27)) & 1);
        *current_lfsr = ((*current_lfsr << 1) | newbit);
    }
}

void set_asc_bit(unsigned int position) {

    uint32_t index = position >> 5;
    scm3c_hw_interface_vars.ASC[index] |=
        0x80000000 >> (position - (index << 5));

    // Possibly more efficient
    // scm3c_hw_interface_vars.ASC[position/32] |= 1 << (position%32);
}

void clear_asc_bit(unsigned int position) {

    uint32_t index = position >> 5;
    scm3c_hw_interface_vars.ASC[index] &=
        ~(0x80000000 >> (position - (index << 5)));

    // Possibly more efficient
    // scm3c_hw_interface_vars.ASC[position/32] &= ~(1 << (position%32));
}

//==== from bucket_o_functions.h

void LC_FREQCHANGE(int coarse, int mid, int fine) {
    //    Inputs:
    //        coarse: 5-bit code (0-31) to control the ~15 MHz step frequency
    //        DAC mid: 5-bit code (0-31) to control the ~800 kHz step frequency
    //        DAC fine: 5-bit code (0-31) to control the ~100 kHz step frequency
    //        DAC
    //    Outputs:
    //        none, it programs the LC radio frequency immediately

    // mask to ensure that the coarse, mid, and fine are actually 5-bit
    // 1.1V (NOP)
    // The NOPs below are for the VDDD tap fix
    // They provide some extra delay so the registers can be loaded properly
    char coarse_m = (char)(coarse & 0x1F);
    char mid_m = (char)(mid & 0x1F);
    char fine_m = (char)(fine & 0x1F);

    // flip the bit order to make it fit more easily into the ACFG registers
    // 1.1V (NOP)
    // The NOPs below are for the VDDD tap fix
    // They provide some extra delay so the registers can be loaded properly
    unsigned int coarse_f = (unsigned int)(flipChar(coarse_m));
    unsigned int mid_f = (unsigned int)(flipChar(mid_m));
    unsigned int fine_f = (unsigned int)(flipChar(fine_m));

    // initialize registers
    unsigned int fcode =
        0x00000000;                    // contains everything but LSB of the fine DAC
    unsigned int fcode2 = 0x00000000;  // contains the LSB of the fine DAC

    fine_f &= 0x000000FF;
    mid_f &= 0x000000FF;
    coarse_f &= 0x000000FF;

    // printf("%d\r\n",fine_m);
    // printf("%d\r\n",mid_m);
    // printf("%d\r\n",coarse_m);

    fcode |= (unsigned int)((fine_f & 0x78) << 9);
    fcode |= (unsigned int)(mid_f << 3);
    fcode |= (unsigned int)(coarse_f >> 3);

    fcode2 |= (unsigned int)((fine_f & 0x80) >> 7);

    // printf("%X\r\n",fcode);
    // printf("%X\r\n",fcode2);

    // ACFG_LO_ADDR   = [ f1 | f2 | f3 | f4 | md | m0 | m1 | m2 | m3 | m4 | cd |
    // c0 | c1 | c2 | c3 | c4 ] ACFG_LO_ADDR_2 = [ xx | xx | xx | xx | xx | xx |
    // xx | xx | xx | xx | xx | xx | xx | xx | fd | f0 ]

    // set the memory and prevent any overwriting of other analog config
    SCUM_ANALOG_CFG_REG_7 = fcode;
    SCUM_ANALOG_CFG_REG_8 = fcode2;
}
void LC_monotonic(int LC_code) {
    // int coarse_divs = 440;
    // int mid_divs = 31; // For full fine code sweeps

    int fine_fix = 0;
    int mid_fix = 0;
    // int coarse_divs = 136;
    int mid_divs =
        23;  // works for Ioana's board, Fil's board, Brad's other board

    // int coarse_divs = 167;
    int coarse_divs = 140;
    // int mid_divs = 27; // works for Brad's board // 25 and 155 worked really
    // well @ low frequency, 27 167 worked great @ high frequency (Brad's board)

    int mid;
    int fine;
    int coarse = (((LC_code / coarse_divs + 19) & 0x000000FF));

    LC_code = LC_code % coarse_divs;
    // mid = ((((LC_code/mid_divs)*4 + mid_fix) & 0x000000FF)); // works for
    // boards (a)
    mid = ((((LC_code / mid_divs) * 3 + mid_fix) & 0x000000FF));
    // mid = ((((LC_code/mid_divs) + mid_fix) & 0x000000FF));
    if (LC_code / mid_divs >= 2) {
        fine_fix = 0;
    };
    fine = (((LC_code % mid_divs + fine_fix) & 0x000000FF));
    if (fine > 15) {
        fine++;
    };

    // coarse=24, mid=0, fine=10 worked at Inria for Tx Frequency
    LC_FREQCHANGE(coarse, mid, fine);
}

void set_LC_current(unsigned int current) {
    unsigned int current_msb = (current & 0x000000F0) >> 4;
    unsigned int current_lsb = (current & 0x0000000F) << 28;

    scm3c_hw_interface_vars.ASC[30] &= 0xFFFFFFF0;
    scm3c_hw_interface_vars.ASC[30] |= current_msb;

    scm3c_hw_interface_vars.ASC[31] &= 0x0FFFFFFF;
    scm3c_hw_interface_vars.ASC[31] |= current_lsb;
}
void disable_polyphase_ASC() {
    scm3c_hw_interface_vars.ASC[30] &= 0xFFEFFFFF;
}

void enable_polyphase_ASC() {
    scm3c_hw_interface_vars.ASC[30] |= 0x00100000;
}

void disable_div_power_ASC() {
    scm3c_hw_interface_vars.ASC[16] &= 0xB7FFFFFF;
}
void enable_div_power_ASC() {
    scm3c_hw_interface_vars.ASC[16] |= 0x48000000;
}

void ext_clk_ble_ASC() {
    scm3c_hw_interface_vars.ASC[32] |= 0x00080000;
}
void int_clk_ble_ASC() {
    scm3c_hw_interface_vars.ASC[32] &= 0xFFF7FFFF;
}

void enable_1mhz_ble_ASC() {
    scm3c_hw_interface_vars.ASC[32] &= 0xFFF9FFFF;
}

void disable_1mhz_ble_ASC() {
    scm3c_hw_interface_vars.ASC[32] |= 0x00060000;
}
void set_PA_supply(unsigned int code) {
    // 7-bit setting (between 0 and 127)
    // MSB is a "panic" bit that engages the high-voltage settings
    unsigned int code_ASC = ((~code) & 0x0000007F) << 13;
    scm3c_hw_interface_vars.ASC[30] &= 0xFFF01FFF;
    scm3c_hw_interface_vars.ASC[30] |= code_ASC;
}
void set_LO_supply(unsigned int code, unsigned char panic) {
    // 7-bit setting (between 0 and 127)
    // MSB is a "panic" bit that engages the high-voltage settings
    unsigned int code_ASC = ((~code) & 0x0000007F) << 5;
    scm3c_hw_interface_vars.ASC[30] &= 0xFFFFF017;
    scm3c_hw_interface_vars.ASC[30] |= code_ASC;
}
void set_DIV_supply(unsigned int code, unsigned char panic) {
    // 7-bit setting (between 0 and 127)
    // MSB is a "panic" bit that engages the high-voltage settings
    unsigned int code_ASC = ((~code) & 0x0000007F) << 5;
    scm3c_hw_interface_vars.ASC[30] &= 0xFFF01FFF;
    scm3c_hw_interface_vars.ASC[30] |= code_ASC;
}

void prescaler(int code) {
    // code is a number between 0 and 5
    // 0 -> disable pre-scaler entirely
    // 1 -> enable div-by-5 back-up pre-scaler
    // 2 -> enable div-by-2 back-up pre-scaler
    // 3 -> enable dynamic pre-scaler version 1 (div-by-5, strong)
    // 4 -> enable dynamic pre-scaler version 2 (div-by-2, strong)
    // 5 -> enable dynamic pre-scaler version 3 (div-by-5, weak)

    if (code == 0) {
        // disable div-by-5 backup, disable div-by-2 backup, disable dynamic
        // pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000004;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFD;  // disable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] |= 0x80000000;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFE;  // disable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] |=
            0x70000000;  // disable all of the dynamic pre-scalers
    } else if (code == 1) {
        // enable div-by-5 backup, disable div-by-2 backup, disable dynamic
        // pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000002;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFB;  // enable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] |= 0x80000000;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFE;  // disable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] |=
            0x70000000;  // disable all of the dynamic pre-scalers
    } else if (code == 2) {
        // disable div-by-5 backup, enable div-by-2 backup, disable dynamic
        // pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000004;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFD;  // disable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] &= 0x7FFFFFFF;
        scm3c_hw_interface_vars.ASC[31] |=
            0x00000001;  // enable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] |=
            0x70000000;  // disable all of the dynamic pre-scalers
    } else if (code == 3) {
        // disable div-by-5 backup, disable div-by-2 backup, enable setting #1
        // of dynamic pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000004;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFD;  // disable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] |= 0x80000000;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFE;  // disable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] &=
            0xBFFFFFFF;  // enable first bit of pre-scaler
    } else if (code == 4) {
        // disable div-by-5 backup, disable div-by-2 backup, enable setting #2
        // of dynamic pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000004;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFD;  // disable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] |= 0x80000000;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFE;  // disable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] &=
            0xDFFFFFFF;  // enable second bit of pre-scaler
    } else if (code == 5) {
        // disable div-by-5 backup, disable div-by-2 backup, enable setting #3
        // of dynamic pre-scaler
        scm3c_hw_interface_vars.ASC[31] |= 0x00000004;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFD;  // disable div-by-5 backup
        scm3c_hw_interface_vars.ASC[32] |= 0x80000000;
        scm3c_hw_interface_vars.ASC[31] &=
            0xFFFFFFFE;  // disable div-by-2 backup
        scm3c_hw_interface_vars.ASC[32] &=
            0x9FFFFFFF;  // enable third bit of pre-scaler
    }
}

void divProgram(unsigned int div_ratio, unsigned int reset,
                unsigned int enable) {
    // Inputs:
    //    div_ratio, a number between 1 and 65536 that determines the integer
    //    divide ratio of the static divider (after pre-scaler) reset, active
    //    low enable, active high
    // Outputs:
    //    none, it programs the divider immediately
    // Example:
    //    divProgram(480,1,1) will further divide the LC tank frequency by 480

    // For this function to work, the scan chain must have bitwise
    // scm3c_hw_interface_vars.ASC[1081]=0, or scm3c_hw_interface_vars.ASC[33]
    // &= 0xFFFFFFBF; BIG BUG:::: odd divide ratios DO NOT WORK when the input
    // to the divider is a high frequency (~1.2 GHz)

    // initialize the programming registers
    unsigned int div_code_1 = 0x00000000;
    unsigned int div_code_2 = 0x00000000;

    //     The two analog config registers look like this:

    // ACFG_CFG_REG__5   = [ d11 | d10 | xx | xx | xx | xx | xx | xx | xx | xx |
    // xx | xx | xx  | xx  | xx  | xx  ] ACFG_CFG_REG__6 = [ d3  | d2  | d1 | d0
    // | rb | en | d9 | d8 | d7 | d6 | d5 | d4 | d15 | d14 | d13 | d12 ]

    div_code_1 = ((div_ratio & 0x00000C00) << 4);
    div_code_2 |= ((div_ratio & 0x0000F000) >> 12);
    div_code_2 |= (div_ratio & 0x000003F0);
    div_code_2 |= (enable << 10);
    div_code_2 |= (reset << 11);
    div_code_2 |= ((div_ratio & 0x0000000F) << 12);

    // also every bit needs to be inverted, hooray
    SCUM_ANALOG_CFG_REG_5 = ~div_code_1;
    SCUM_ANALOG_CFG_REG_6 = ~div_code_2;
}
