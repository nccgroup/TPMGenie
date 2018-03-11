/*
 * Pretty Print TPM command and response packets
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 * 
 * Some of these functions are highly optimized, using sprintf() to construct
 * a large string, rather than issueing lots of tiny individual transactions
 * on the console with many calls to Serial.print().
 */

#include <Arduino.h>
#include "tpm_cmd_parser.h"
#include "pretty_print_packets.h"
#include "interposer.h"

#define swap16 __builtin_bswap16
#define swap32 __builtin_bswap32

// Idea:
// - Rearchitect this code to reduce serial bus traffic and potential issues w.r.t. buffering. 
// - So, rather than sending strings over the bus, send serialized packet data.
// - Then, let a host-side application parse that data stream and pretty-print it.
// - Then, rather than attaching to the console using minicom, you'd use this new host application.

void pp_header( void ) {
  Serialprintln( "/-------------------|--------|-------------------------------------------------\\" );
  Serialprintln( "|     Direction     |  reg   | data                                            |" );
  Serialprintln( "|-------------------|--------|-------------------------------------------------|" );
}

void pp_rowsep( void ) {
  // TODO: May want to consider removing this function, because it results in a lot of extra overhead
  // that can mess up the interposer's ability to keep up with the kernel's timing constaints.
  Serialprintln( "|-------------------|--------|-------------------------------------------------|" );
}

void pp_dir_and_reg( interposer_dir dir, tpm_register reg ) {
  char str[64];
  const char *dir_str;
  const char *reg_str;

  switch( dir ) {
    case HOST_2_INTERPOSER: dir_str = "Host > Tnsy      "; break;
    case INTERPOSER_2_TPM:  dir_str = "       Tnsy > TPM"; break;
    case TPM_2_INTERPOSER:  dir_str = "       Tnsy < TPM"; break;
    case INTERPOSER_2_HOST: dir_str = "Host < Tnsy      "; break;
    default:                dir_str = "                 "; break;
  }

  switch( reg ) {
    case TPM_ACCESS:    reg_str = "ACCESS"; break;
    case TPM_STS:       reg_str = "STS";    break;
    case TPM_BURST_CNT: reg_str = "BURST";  break;
    case TPM_DATA_FIFO: reg_str = "DATA";   break;
    case TPM_DID_VID:   reg_str = "VIDDID"; break;
    case TPM_REG_NONE:  reg_str = " ";      break;
    default:            reg_str = "???";    break;
  }
  
  sprintf( str, "| %s | %-6s |", dir_str, reg_str );
  Serialprint( str );
}


// Sufficient size since we split on every 16th byte when printing
char hex_str[128] = {0}; 

void pp_hex_data( byte *data, size_t num_bytes ) { 
  unsigned int str_offset = 0;
  
  for( size_t j=0; j<num_bytes; j++ ) {
    if( (j>0) && ((j % 16) == 0) && (j < num_bytes) ) {
      Serialwrite( hex_str, str_offset );
      Serialprintln();
      Serialprint( "|                   |        |" );
      str_offset = 0;
    }
    sprintf(hex_str + str_offset, " %02X", data[j]);
    str_offset += 3;
  }
  Serialwrite( hex_str, str_offset );
  Serialprintln();

}

/* 
 *  Print the ordinal associated with this command.
 */

void pp_command_ordinal( unsigned int ord, const char *type_str ) {
  char str[128] = {0};
  const char *cmd_str;

  switch( ord ) {
    case TPM_ORD_OIAP:                   cmd_str = "OIAP";                   break;
    case TPM_ORD_Extend:                 cmd_str = "TPM_ORD_Extend";         break;
    case TPM_ORD_PcrRead:                cmd_str = "PcrRead";                break;
    case TPM_ORD_GetRandom:              cmd_str = "GetRandom";              break;
    case TPM_ORD_ContinueSelfTest:       cmd_str = "ContinueSelfTest";       break;
    case TPM_ORD_GetCapability:          cmd_str = "GetCapability";          break;
    case TPM_ORD_GetCapabilityOwner:     cmd_str = "GetCapabilityOwner";     break;
    case TPM_ORD_Startup:                cmd_str = "Startup";                break;
    case TPM_ORD_ReadPubek:              cmd_str = "ReadPubek";              break;
    case TPM_ORD_PhysicalEnable:         cmd_str = "PhysicalEnable";         break;
    case TPM_ORD_PhysicalSetDeactivated: cmd_str = "PhysicalSetDeactivated"; break;
    case TPM_ORD_SelfTest:               cmd_str = "SelfTest";               break;
    case TSC_ORD_PhysicalPresence:       cmd_str = "PhysicalPresence";       break;
    case TPM_ORD_ForceClear:             cmd_str = "ForceClear";             break;
    case TPM_ORD_NV_ReadValue:           cmd_str = "NV_ReadValue";           break;
    case TPM_ORD_TakeOwnership:          cmd_str = "TakeOwnership";          break;
    case TPM_ORD_FlushSpecific:          cmd_str = "FlushSpecific";          break;
    case TPM_ORD_OwnerClear:             cmd_str = "OwnerClear";             break;
    case TPM_ORD_OwnerSetDisable:        cmd_str = "OwnerSetDisable";        break;
    case TPM_ORD_SetOwnerInstall:        cmd_str = "SetOwnerInstall";        break;
    case TPM_ORD_OwnerReadInternalPub:   cmd_str = "OwnerReadInternalPub";   break;
    case TPM_ORD_SetOperatorAuth:        cmd_str = "SetOperatorAuth";        break;
    default:                             cmd_str = "Unknown";                break;
  }

  sprintf( str, "| %8s          |        | %s (0x%08X)", type_str, cmd_str, ord );
  Serialprintln( str );
}

/* 
 *  Print the request header and body for a few of the commands we find interesting.
 */
void pp_request_packet( byte *data, size_t num_bytes ) {
  tpm_input_header *hdr;

  // Safe to do this inline swap because operating on the DATA_FIFO copy,
  // not the actual packet we're about to send along to the TPM.
  hdr = (tpm_input_header*)data;
  hdr->ord = swap32( hdr->ord );

  Serialprintln( "|-------------------|--------|--------------------------------");
  pp_command_ordinal( hdr->ord, "REQUEST" );

  switch( hdr->ord ) {
    case TPM_ORD_GetRandom: {
      tpm_get_rnd_req_body *bdy = (tpm_get_rnd_req_body*)(data+10);
      Serialprint( "|                   |        | Num Bytes: ");
      Serialprintln( swap32(bdy->num_bytes) );
      break;
    }
    case TPM_ORD_PcrRead: {
      tpm_pcr_read_req_body *bdy = (tpm_pcr_read_req_body*)(data+10);
      Serialprint( "|                   |        | PCR Index: ");
      Serialprintln( swap32(bdy->pcr_index) );
      break;
    }
    case TPM_ORD_Extend: {
      tpm_pcr_extend_req_body *bdy = (tpm_pcr_extend_req_body*)(data+10);
      Serialprint(   "|                   |        | PCR Index: ");
      Serialprintln( swap32(bdy->pcr_index) );
      Serialprintln( "|                   |        | In Digest: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->in_digest, 20 );
      break;
    }
    case TPM_ORD_OSAP: {
      tpm_osap_req_body *bdy = (tpm_osap_req_body*)(data+10);
      Serialprint(   "|                   |        | Entity Type:  ");
      Serialprintln( swap16(bdy->entity_type) );
      Serialprint(   "|                   |        | Entity Value: ");
      Serialprintln( swap32(bdy->entity_value) );      
      Serialprintln( "|                   |        | Nonce Odd OSAP: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->nonce_odd_osap, 20 );
      break;
    }
    case TPM_ORD_SetOperatorAuth: {
      tpm_op_auth_req_body *bdy = (tpm_op_auth_req_body*)(data+10);
      Serialprintln( "|                   |        | Operator Auth Data [sha1sum(password)]: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->operator_auth, 20 );
      break;
    }
    case TPM_ORD_GetCapability: {
      tpm_get_cap_req_body *bdy = (tpm_get_cap_req_body*)(data+10);
      unsigned int sub_cap_size = swap32(bdy->sub_cap_size);
      Serialprint( "|                   |        | Cap Area: ");
      Serialprintln( swap32(bdy->cap_area) );
      
      if( sub_cap_size == 0 ) {
        Serialprintln( "|                   |        | Sub Cap: n/a");
      } else {
        Serialprint(   "|                   |        | Sub Cap Size: ");
        Serialprintln( sub_cap_size );
        Serialprintln( "|                   |        | Sub Cap Data: ");
        Serialprint(   "|                   |        |");
        pp_hex_data( bdy->sub_cap, sub_cap_size );
      }
      break;
    }
    case TPM_ORD_Startup: {
      tpm_startup_req_body *bdy = (tpm_startup_req_body*)(data+10);
      Serialprint( "|                   |        | Startup Type: ");
      Serialprintln( swap16(bdy->startup_type) );
      break;
    }
    case TPM_ORD_ReadPubek: {
      tpm_read_pubek_req_body *bdy = (tpm_read_pubek_req_body*)(data+10);
        Serialprintln( "|                   |        | Anti Replay: ");
        Serialprint(   "|                   |        |");
        pp_hex_data( bdy->anti_replay, 20 );
      break;
    }

  /*
    case TPM_ORD_TakeOwnership: {
      byte *bdy = (data+10);
      unsigned short protocol_id = *bdy;
      break;
    }
 */
    case TPM_ORD_ContinueSelfTest:
    case TPM_ORD_OIAP:
      Serialprintln( "|                   |        | No args.");
      break;
    default:
      Serialprintln( "|                   |        | Not supported yet... ");
      break;
  }
  Serialprintln( "|-------------------|--------|--------------------------------");
}

/*
 *  Print the response header and body for a few of the commands we find interesting.
 */
void pp_resp_body( byte *data, size_t num_bytes ) {
  // XXX: Absolute hack to print only the response body and not the header. Ugh.
  if( num_bytes == 10 ) {
    return;
  }
  Serialprintln( "|-------------------|--------|--------------------------------");
  pp_command_ordinal( prev_command, "RESPONSE" );
  
  switch( prev_command ) {
    case TPM_ORD_Extend: {
      tpm_pcr_extend_resp_body *bdy = (tpm_pcr_extend_resp_body*)data;
      Serialprintln( "|                   |        | Out Digest: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->out_digest, 20 );
      break;
    }
    case TPM_ORD_PcrRead: {
      tpm_pcr_read_resp_body *bdy = (tpm_pcr_read_resp_body*)data;
      Serialprintln( "|                   |        | Out Digest: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->out_digest, 20 );
      break;
    }
    
    case TPM_ORD_GetRandom: {
      tpm_get_rnd_resp_body *bdy = (tpm_get_rnd_resp_body*)data;
      unsigned int random_bytes_size = bdy->random_bytes_size;
      random_bytes_size = swap32(random_bytes_size);
      Serialprint(   "|                   |        | Random Bytes Size: ");
      Serialprintln( random_bytes_size );
      Serialprintln( "|                   |        | Random Bytes: ... ");
      //Serialprint(   "|                   |        |");
      //pp_hex_data( bdy->random_bytes, min(random_bytes_size, num_bytes) );
      break;
    }
    
    case TPM_ORD_OSAP: {
      tpm_osap_resp_body *bdy = (tpm_osap_resp_body*)data;
      Serialprint(   "|                   |        | Auth Handle: ");
      Serialprintln( swap32(bdy->auth_handle) );
      Serialprintln( "|                   |        | Nonce Even: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->nonce_even, 20 );
      Serialprintln( "|                   |        | Nonce Even OSAP: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->nonce_even_osap, 20 );
      break;
    }
    case TPM_ORD_OIAP: {
      tpm_oiap_resp_body *bdy = (tpm_oiap_resp_body *)data;
      Serialprint(   "|                   |        | Auth Handle: ");
      Serialprintln( swap32(bdy->auth_handle) );
      Serialprintln( "|                   |        | Nonce Even: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->nonce_even, 20 );
      break;
    }
    case TPM_ORD_GetCapability: {
      tpm_get_cap_resp_body *bdy = (tpm_get_cap_resp_body *)data;
      Serialprint(   "|                   |        | Resp Size: ");
      Serialprintln( swap32(bdy->resp_size) );
      Serialprintln( "|                   |        | Resp: ");
      Serialprint(   "|                   |        |");
      pp_hex_data( bdy->resp, swap32(bdy->resp_size) );
      break;
    }

    case TPM_ORD_Startup:
    case TPM_ORD_ContinueSelfTest:
    case TPM_ORD_SetOperatorAuth:
      Serialprintln( "|                   |        | No args.");
      break;
    default:
      Serialprintln( "|                   |        | Not supported yet... ");
      break;
  }

  Serialprintln( "|-------------------|--------|--------------------------------");
}


