#!/usr/bin/env python3
"""
Extend TPM PCRs

Author: Jeremy Boone, NCC Group
Date  : Feb 2, 2018
"""

import optparse, os, hashlib

tpm0 = None

###############################################################################
# Helper Functions
###############################################################################

def parse_options():
    parser = optparse.OptionParser()
    parser.add_option("-i", dest="index", metavar="<index>", help="PCR Index" )
    parser.add_option("-f", dest="file",  metavar="<path>",  help="File to hash" )
    options, args = parser.parse_args()

    if options.index is None:
        parser.error( "Index (-i) argument is required" )
    else:
        options.index = int(options.index)

    if options.file is None:
        parser.error( "File (-f) argument is required" )
    elif not os.path.exists( options.file ):
        parser.error( "File %s does not exist" % options.file )

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
# Main function that extends a PCR
###############################################################################

def main( options ):
    # Create the command header
    cmd_hdr = bytearray()
    cmd_hdr += b'\x00\xC1'         # Tag: 0x00C1
    cmd_hdr += b'\x00\x00\x00\x22' # Len: 34 bytes incl pcr_index & in_digest
    cmd_hdr += b'\x00\x00\x00\x14' # Ord: TPM_ORD_PcrExtend

    # Create the command body
    pcr_index     = bytearray( b'\x00\x00\x00\x00' )
    pcr_index[3] += options.index
    pcr_index     = bytes(pcr_index)

    # Read file and calculate hash
    contents = open( options.file, "r" ).read()
    m = hashlib.sha1()
    m.update( contents.encode('utf-8') )
    in_digest = m.digest()

    print( "[*] Reading this file:        %s" % options.file )
    print( "[*] Computed this digest:     %s" % m.hexdigest() )
    print( "[*] Extending into PCR index: %s" % options.index )

    # Transmit command and extract resposne body.
    cmd        = cmd_hdr + pcr_index + in_digest
    resp       = tpm_transmit( bytes(cmd) )
    out_digest = resp[10:]

    s = "[*] Output digest:            "
    s += "".join( "{:02x}".format(c) for c in out_digest )
    print( s )


if __name__ == "__main__":
    options = parse_options()
    tpm_init()
    main( options )
    tpm_deinit()

