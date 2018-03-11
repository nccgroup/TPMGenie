/*
 * Miscellaneous Interposer Stuff
 * 
 * - Stub out print statements if you really want.
 * - Track the interposer state machine
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */
 
#ifndef __INTERPOSER_H__
#define __INTERPOSER_H__

/*
 * Comment out `ENABLE_PRINT_TABLE` to suppress the numerous print statements throughout this
 * interposer. Might be useful if you experience problems with the interposer related to 
 * timing.
 */
#define ENABLE_PRINT_TABLE

#ifdef ENABLE_PRINT_TABLE
  #define Serialprint( s )       Serial.print( s )
  #define Serialprintln( s )     Serial.println( s )
  #define Serialprintlnh( s, t ) Serial.println( s, t )
  #define Serialprinth( s, t )   Serial.print( s, t )
  #define Serialwrite( s, l )    Serial.write( s, l )

#else
  #define Serialprint( s )      
  #define Serialprintln( s )     
  #define Serialprintlnh( s, t ) 
  #define Serialprinth( s, t )
  #define Serialwrite( s, l )
#endif

// --------------------------------------------------------------------------
// Interposer State Machine
// --------------------------------------------------------------------------

/*
 * State machine that controls whether the Interposer will passthrough data
 * or modify data. Useful for demo w/o having to reload software.
 */
#define STATE_PASSTHROUGH          0
#define STATE_SPOOF_PCR_EXTEND     1
#define STATE_ALTER_HW_RNG         2
#define STATE_TRIGGER_KERNEL_CRASH 3
#define STATE_LAST                 4

typedef unsigned int interposer_state;

extern interposer_state STATE;


void handle_command( char c );
void print_state( void );

#endif // __INTERPOSER_H__

