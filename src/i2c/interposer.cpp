/*
 * TPM Interposer
 * 
 * This source file is responsible for tracking the current state of
 * TPM Genie. Depending on the state, TPM Genie will interpose different
 * commands and responses that are transmitted over the bus.
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */

#include <Arduino.h>
#include "interposer.h"

interposer_state STATE = STATE_PASSTHROUGH;

/*
 * Pretty print the current state
 */
void print_state( void ) {
  switch( STATE ) {
    case STATE_PASSTHROUGH:          Serial.print( "Passthrough" );          break;
    case STATE_SPOOF_PCR_EXTEND:     Serial.print( "Spoof PCR Extend" );     break;
    case STATE_ALTER_HW_RNG:         Serial.print( "Alter Hardware RNG" );   break;
    case STATE_TRIGGER_KERNEL_CRASH: Serial.print( "Trigger Kernel Crash" ); break;
    default:                         Serial.print( "**UNKNOWN**" );          break;
  }
}

/*
 * Switch state whenever the user hits 'm' key. 
 */
void handle_command( char c ) {
  Serial.println( );
  Serial.print( "[*] Switching state from '" );
  print_state( );
  Serial.print( "' to '" );
  
  STATE = STATE + 1;
  // Loop back to start when cycle through to last state.
  if( STATE >= STATE_LAST ) {
    STATE = STATE_PASSTHROUGH;
  }
  
  print_state( );
  Serial.println( "' mode." );
  Serial.println( );
}

