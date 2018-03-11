#!/usr/bin/env python3
"""
Read TPM PCR banks

Author: Jeremy Boone, NCC Group
Date  : Feb 2, 2018
"""

import time, optparse

tpm0 = None

###############################################################################
# Helper Functions
###############################################################################

def parse_options():
    parser = optparse.OptionParser()
    parser.add_option("-i", dest="index", metavar="<index>", help="PCR Index" )
    options, args = parser.parse_args()

    if options.index is not None:
        options.index = int(options.index)

    return options

def tpm_init():
    global tpm0
    tpm0 = open("/dev/tpm0","r+b", buffering=0)

def tpm_deinit():
    global tpm0
    tpm0.close()

def tpm_transmit( cmd ):
    global tpm0
    tpm0.write(cmd)
    tpm0.flush()
    resp = tpm0.read()
    return resp

###############################################################################
# Main function that reads the PCRs
###############################################################################

def main( options ): 
    cmd_hdr = bytearray()
    cmd_hdr += b'\x00\xC1'         # Tag: 0x00C1
    cmd_hdr += b'\x00\x00\x00\x0E' # Len: 14 bytes
    cmd_hdr += b'\x00\x00\x00\x15' # Ord: TPM_ORD_PcrRead

    pcr_index = bytearray( b'\x00\x00\x00\x00' )

    if options.index is not None:
        pcr_index[3] += options.index
        cmd_bdy = bytes(pcr_index)
        cmd     = cmd_hdr + cmd_bdy
        resp    = tpm_transmit( bytes(cmd) )
        rsp_bdy = resp[10:]

        s = "PCR %2d: " % pcr_index[3]
        s += " ".join( "{:02x}".format(c) for c in rsp_bdy )
        print( s )
    else:
        for i in range(0,24):
            cmd_bdy = bytes(pcr_index)
            cmd     = cmd_hdr + cmd_bdy
            resp    = tpm_transmit( bytes(cmd) )
            rsp_bdy = resp[10:]

            s = "PCR %2d: " % pcr_index[3]
            s += " ".join( "{:02x}".format(c) for c in rsp_bdy )
            print( s )

            # Increment index for next iteration
            pcr_index[3] += 1
            time.sleep(0.2)


if __name__ == "__main__":
    options = parse_options()
    tpm_init()
    main( options )
    tpm_deinit()

