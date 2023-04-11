#!/usr/bin/env python3
# Derived from pad_checksum in the Pico SDK, which carries the following
# LICENSE.txt:
# Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
#    disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import binascii
import struct
import sys

parser = argparse.ArgumentParser()
parser.add_argument("ifile", help="Input application binary (binary)")
parser.add_argument("ofile", help="Output header file (binary)")
parser.add_argument("--linker-defines-file", help="Linker defines file")
args = parser.parse_args()

try:
    idata = open(args.ifile, "rb").read()
except:
    sys.exit("Could not open input file '{}'".format(args.ifile))

linker_args = {}
with open(args.linker_defines_file, 'r') as fd:
    for line in fd.readlines():
        while line.find('*/') > line.find('/*'):
            line = line[:line.find('/*')] + line[line.find('*/') + 2:]
        if '//' in line:
            line = line[:line.find('//')]
        if '=' in line:
            key_value_pair = line.split('=')
            key = key_value_pair[0].strip()
            value = key_value_pair[1].strip().rstrip(';').replace('k', '*1024')
            for k,v in linker_args.items():
                value = value.replace(k, str(v))
            eval_value = eval(value)
            linker_args[key] = eval_value

print('Parsed linker defines:')
for k,v in linker_args.items():
    print('{} = 0x{:08x}'.format(k,v))

flash_addr = 0x10000000
app_addr = flash_addr + linker_args['__APPLICATION_OFFSET']
header_addr = flash_addr + linker_args['__APPLICATION_HEADER_OFFSET']

# Remove header from data
offset = app_addr - header_addr
idata = idata[offset:]


vtor = app_addr
size = len(idata)
crc = binascii.crc32(idata)

odata = vtor.to_bytes(4, byteorder='little') + size.to_bytes(4, byteorder='little') + crc.to_bytes(4, byteorder='little')

try:
    with open(args.ofile, "wb") as ofile:
        ofile.write(odata)
except:
    sys.exit("Could not open output file '{}'".format(args.ofile))
