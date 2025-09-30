#ifndef __HDLC_H
#define __HDLC_H

#include <stdlib.h>
#include <stdint.h>

//=========================== definitions ======================================

/// Internal state of the HDLC decoder
typedef enum {
    HDLC_STATE_IDLE,       ///< Waiting for incoming HDLC frames
    HDLC_STATE_RECEIVING,  ///< An HDLC frame is being received
    HDLC_STATE_READY,      ///< An HDLC frame is ready to be decoded
    HDLC_STATE_ERROR,      ///< The FCS value is invalid
} hdlc_state_t;

//=========================== public ===========================================

/**
 * @brief   Handle a byte received in HDLC internal state
 *
 * @param[in]   byte    The received byte
 */
hdlc_state_t hdlc_rx_byte(uint8_t byte);

/**
 * @brief   Decode an HDLC frame
 *
 * @param[out]  payload     Decoded payload contained in the input buffer
 *
 * @return the number of bytes decoded
 */
size_t hdlc_decode(uint8_t *payload);

#endif
