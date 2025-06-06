#!/usr/bin/env python3
#
# Copyright 2021 Richard Hulme
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Script to perform sanity checks on one or more UF2 files
# If multiple UF2 files are provided, the target addresses between files
# must be contiguous (padding will be added up to the next 4k boundary)
#
# Using the '-o' parameter allows multiple UF2 files to be concatenated and
# output
#

import argparse
import struct;

UF2_MAGIC_START0 = 0x0A324655 # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157 # Randomly selected
UF2_MAGIC_END    = 0x0AB16F30 # Ditto

FAMILY_ID_RP2040 = 0xe48bff56

def auto_int(x):
    return int(x, 0)

# Pad to the next multiple of 4096
def pad(start, end, buf):
    blocks = 0;

    while start < end:
        length = 256 - (start % 256)
        header = (UF2_MAGIC_START0,
                  UF2_MAGIC_START1,
                  0x00002000,
                  start,
                  length,
                  0,                    # Block number - will be updated later
                  0,                    # Numblocks - will be updated lated
                  FAMILY_ID_RP2040
                  )

        buf.extend(struct.pack(b"<IIIIIIII", *header))
        buf.extend(bytearray(b'\xff' * 476))  # Payload - all 0xff
        buf.extend(struct.pack(b"<I", UF2_MAGIC_END))
        start += length;
        blocks += 1

    return (start, blocks)

# Update the individual block number and the total number of blocks field in
# each block
def updateBuf(buf):
    numblocks = len(buf) // 512
    curblock = 0

    for blockno in range(numblocks):
        ptr = blockno * 512
        struct.pack_into(b"<II", buf, ptr + 20, curblock, numblocks)
        curblock += 1

# Check the uf2 file passed in buf is valid and contiguous
def check_uf2(start, buf, data_len_min=None, data_len_max=None):
    numblocks = len(buf) // 512
    numpadblocks = 0
    curaddr = start
    curblock = 0
    newbuf = bytearray()

    assert len(buf) % 512 == 0, "Length ({}) is not a multiple of 512".format(len(buf))

    for blockno in range(numblocks):
        ptr = blockno * 512
        block = buf[ptr:ptr + 512]
        hd = struct.unpack(b"<IIIIIIII", block[0:32])
        endMagic = struct.unpack(b"<I", block[508:512])

        assert hd[0] == UF2_MAGIC_START0, f"Bad magic start0 at {ptr}"
        assert hd[1] == UF2_MAGIC_START1, f"Bad magic start1 at {ptr}"
        assert endMagic[0] == UF2_MAGIC_END, f"Bad magic end 0x{endMagic[0]:08x} at {ptr}"

        # This is not a block to be flashed.  This is not currently supported
        # by the code so flag it as bad
        assert hd[2] & 1 == 0, f"Bad flags (0x{hd[2]:08x} at {ptr}"

        assert hd[6] == numblocks, f"Number of blocks incorrect ({hd[6]}, expected {numblocks} at {ptr})"

        datalen = hd[4]
        assert datalen <= 476, f"Invalid UF2 data size at {ptr}"
        if data_len_min is not None:
            assert datalen >= data_len_min, f"UF2 data size ({datalen}) at {ptr} is less than specified minimum ({data_len_min})"
        if data_len_max is not None:
            assert datalen <= data_len_max, f"UF2 data size ({datalen}) at {ptr} is more than specified minimum ({data_len_max})"

        newaddr = hd[3]
        if (hd[2] & 0x2000):
            assert hd[7] == FAMILY_ID_RP2040

        padding = newaddr - curaddr
        assert padding >= 0, f"Address jumps backwards at {ptr}. Start is 0x{newaddr:08x}, expected 0x{curaddr:08x}"   # not UF2 requirement

        if padding > 0:
            curaddr, blocks = pad(curaddr, newaddr, newbuf)
            numpadblocks += blocks

        assert blockno == hd[5], f"Missing block detected at {ptr}"

        curaddr = newaddr + datalen
        newbuf.extend(block)

    # Add the number of padding blocks to the total
    numblocks += numpadblocks

    return (curaddr, numblocks, newbuf)


def process(start, infiles, outfile, data_len_min=None, data_len_max=None):
    data = bytearray()
    block = 0
    curaddr = start

    for filename in infiles:
        with open(filename, mode='rb') as f:
            try:

                buf = f.read()
                curaddr, blocks, buf = check_uf2(curaddr, buf, data_len_min, data_len_max)
                data.extend(buf)

                block += blocks

            except AssertionError as e:
                print("***************************************************************")
                print(f"UF2 sanity check of {filename} failed:")
                print(e)
                print("***************************************************************")
                exit(1)

    if outfile is not None:
        updateBuf(data)
        try:
            check_uf2(start, data, data_len_min, data_len_max)
        except AssertionError as e:
            print("***************************************************************")
            print(f"UF2 sanity check of combined file failed:")
            print(e)
            print("***************************************************************")
            exit(1)

        with open(outfile, mode='wb') as output:
            output.write(data)
            print(f"Written {outfile}")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--start', type=auto_int, default=0x10000000)
    parser.add_argument('-o', dest='outfile', nargs='?')
    parser.add_argument('infile', nargs='+')
    parser.add_argument('--data-len-min', type=int, default=None)
    parser.add_argument('--data-len-max', type=int, default=None)

    args = parser.parse_args()

    if (args.outfile is not None) and (len(args.infile) == 1):
        print("Ignoring output file setting with only one input file\n");

    process(args.start, args.infile, args.outfile, args.data_len_min, args.data_len_max)

main()
