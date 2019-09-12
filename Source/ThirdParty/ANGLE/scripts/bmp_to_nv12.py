#!/usr/bin/python2
#
# Copyright 2016 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# bmp_to_nv12.py:
#   Script to convert a simple BMP file to an NV12 format. Used to create
#   test images for the NV12 texture stream end to end tests

import sys
import struct

if len(sys.argv) != 4:
    print("Usage: bmp_to_nv12.py input output prefix")
    exit(0)

bmp_file = open(sys.argv[1], "rb")

magic = bmp_file.read(2)
if (magic != "BM"):
    print("Invalid BMP magic")
    exit(1)

file_size, = struct.unpack("I", bmp_file.read(4))

# eat reserved bytes
bmp_file.read(4)

offset, = struct.unpack("I", bmp_file.read(4))

headersize, = struct.unpack("I", bmp_file.read(4))
width, = struct.unpack("i", bmp_file.read(4))
height, = struct.unpack("i", bmp_file.read(4))
planes, = struct.unpack("H", bmp_file.read(2))
bpp, = struct.unpack("H", bmp_file.read(2))
compression, = struct.unpack("i", bmp_file.read(4))
image_size, = struct.unpack("i", bmp_file.read(4))

if (bpp != 24 or compression != 0):
    print("Unsupported BMP file")
    bmp_file.close()
    exit(1)

bmp_file.seek(offset, 0)
pixels = bmp_file.read(width * height * 3)
bmp_file.close()

# convert to YUV 4:4:4
converted_pixels = bytearray(pixels)
for i in range(0, width * height):
    R, = struct.unpack("B", pixels[i*3+2])
    G, = struct.unpack("B", pixels[i*3+1])
    B, = struct.unpack("B", pixels[i*3])
    converted_pixels[i*3] = ((66*R + 129*G + 25*B + 128) >> 8) + 16
    converted_pixels[i*3+1] = ((-38*R - 74*G + 112*B + 128) >> 8) + 128
    converted_pixels[i*3+2] = ((112*R - 94*G - 18*B + 128) >> 8) + 128

# downsample to packed UV buffer
uv_buffer = bytearray(width * height / 2)
for i in range(0, width * height / 2, 2):
    U1 = converted_pixels[((((i / width) * 2) * width) + (i % width)) * 3 + 1]
    U2 = converted_pixels[((((i / width) * 2) * width) + width + (i % width)) * 3 + 1]
    V1 = converted_pixels[((((i / width) * 2) * width) + (i % width)) * 3 + 2]
    V2 = converted_pixels[((((i / width) * 2) * width) + width + (i % width)) * 3 + 2]
    uv_buffer[i] = (U1 + U2) / 2
    uv_buffer[i + 1] = (V1 + V2) / 2

# extract the Y buffer
y_buffer = bytearray(width * height)
for i in range(0, width * height):
    y_buffer[i] = converted_pixels[i * 3]

# write out the file as a C header
nv12_file = open(sys.argv[2], "w")
nv12_file.write("// Automatically generated from " + sys.argv[1] + "\n")
nv12_file.write("static const size_t " + sys.argv[3] + "_width = " + str(width) + ";\n")
nv12_file.write("static const size_t " + sys.argv[3] + "_height = " + str(height) + ";\n")
nv12_file.write("static const unsigned char " + sys.argv[3] + "_data[] = \n{")
for i in range(0, width * height):
    if (i % 16) == 0:
        nv12_file.write("\n    ")
    nv12_file.write(str(y_buffer[i]) + ",")
for i in range(0, width * height / 2):
    if (i % 16) == 0:
        nv12_file.write("\n    ")
    nv12_file.write(str(uv_buffer[i]) + ",")
nv12_file.write("\n};")
nv12_file.close()
