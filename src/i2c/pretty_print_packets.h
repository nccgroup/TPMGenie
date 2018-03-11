/*
 * Pretty Print TPM command and response packets
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 * 
 */

#ifndef __PRETTY_PRINT_PACKETS_H__
#define __PRETTY_PRINT_PACKETS_H__

#include "tpm_cmd_parser.h"

// --------------------------------------------------------------------------
// Function  prototypes
// --------------------------------------------------------------------------

void pp_header( void );
void pp_rowsep( void );
void pp_dir_and_reg( interposer_dir dir, tpm_register reg );
void pp_hex_data( byte *data, size_t num_bytes );
void pp_request_packet( byte *data, size_t num_bytes );
void pp_resp_body( byte *data, size_t num_bytes );

#endif // __PRETTY_PRINT_PACKETS_H__

