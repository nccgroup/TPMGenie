/*
 * Simple FIFO Implementation.
 * 
 * Packets sent across the TPM's serial bus can sometimes be fragmented. 
 * This lets us track them so they can be parsed by TPM Genie.
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */

#ifndef __FIFO_H__
#define __FIFO_H__

#include "i2c_t3.h"

// --------------------------------------------------------------------------
// Typedefs
// --------------------------------------------------------------------------

/* 
 *  We keep our own copy of all data that is written to the data FIFO.
 */
typedef struct _tpm_data_fifo {
  size_t sz;
  byte data[I2C_RX_BUFFER_LENGTH];
} tpm_data_fifo;

// --------------------------------------------------------------------------
// Variables
// --------------------------------------------------------------------------


extern tpm_data_fifo FIFO;

// --------------------------------------------------------------------------
// Function  prototypes
// --------------------------------------------------------------------------
void fifo_add( byte *data, size_t num_bytes );
void fifo_clear( void );

#endif // __FIFO_H__

