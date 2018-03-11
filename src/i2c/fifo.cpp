/*
 * Simple FIFO Implementation.
 * 
 * Packets sent across the TPM's serial bus can sometimes be fragmented. 
 * This lets us track them so they can be parsed by TPM Genie.
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */

#include "i2c_t3.h"
#include <Arduino.h>
#include "fifo.h"

// --------------------------------------------------------------------------
// Variables
// --------------------------------------------------------------------------

// Initialize the FIFO
tpm_data_fifo FIFO = { .sz=0, .data={0} };

// --------------------------------------------------------------------------
// Function Definitions
// --------------------------------------------------------------------------

/*
 * The host may send a command to the TPM in multiple fragments. These are seen
 * as multiple writes to DATA_FIFO register (0x05). We need to FIFO so that we can parse
 * it correctly when the host writes '0x20' to the STS register (0x01), which effectively
 * means "go execute that command now".
 */
void fifo_add( byte *data, size_t num_bytes ) {
  if( num_bytes == 0 ) {
    Serial.println( "Trying to add 0 bytes to FIFO" );
    return;
  }
  
  // As unlikely as it is with the sizes of data we're working with, would be good to
  // check for integer overflow here.
  if( (num_bytes + FIFO.sz) > I2C_RX_BUFFER_LENGTH ) {
    Serial.println( "***************************************" );
    Serial.println( "FIFO overflow" );
    Serial.print(   "  current size: " );
    Serial.println( FIFO.sz );
    Serial.print(   "  adding bytes: " );
    Serial.println( num_bytes );
    Serial.println( "***************************************" );
    return;
  }

  // FIFO is empty, so just copy in the first fragment
  if( FIFO.sz == 0 ) {
    memcpy( FIFO.data, data, num_bytes );
    FIFO.sz = num_bytes;
  }
  // FIFO is non-empty, so append new fragment
  else {
    memcpy( FIFO.data + FIFO.sz, data, num_bytes );
    FIFO.sz = FIFO.sz + num_bytes;
  }
}

/*
 * The FIFO is cleared simply by setting its size to zero rather than actually memset-zeroing the
 * buffer. This is a performance optimization because I found that the Arduino interposer can't
 * quite meet the timing demands of the Kernel's TPM driver.
 */
void fifo_clear( ) {
  if( FIFO.sz != 0 ) {
    FIFO.sz = 0;
  }
}
