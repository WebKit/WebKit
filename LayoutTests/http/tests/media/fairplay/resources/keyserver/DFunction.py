#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from Crypto.Cipher import AES
from Crypto.Util import Padding
from array import array
import hashlib


def ComputeDFunctionHash(R2):
    assert 0 < len(R2) < 56

    pad = bytearray(64)
    padView = memoryview(pad)
    MBlock = array("L", [0] * 14)
    P = 813416437
    NB_RD = 16
    R2_sz = len(R2)

    padView[:R2_sz] = R2
    padView[R2_sz] = 0x80

    for i in range(0, 14):
        MBlock[i] = (pad[4 * i] << 24) | (pad[4 * i + 1] << 16) | (pad[4 * i + 2] << 8) | (pad[4 * i + 3])

    for i in range(1, 7):
        MBlock[0] = (MBlock[0] + MBlock[i]) % (1 << 32)

    MBlock[1] = 0
    for i in range(0, 7):
        MBlock[1] = (MBlock[1] + MBlock[i + 7]) % (1 << 32)

    for i in range(0, 2):
        for r in range(0, NB_RD):
            if MBlock[i] & 1:
                MBlock[i] >>= 1
            else:
                MBlock[i] = ((3 * MBlock[i] + 1) % (1 << 32)) % P

    MBlock0bytes = MBlock[0].to_bytes(4, byteorder='little')
    MBlock1bytes = MBlock[1].to_bytes(4, byteorder='little')

    for i in range(0, 4):
        pad[56 + i] = MBlock0bytes[i]
        pad[60 + i] = MBlock1bytes[i]

    ctx = hashlib.sha1()
    ctx.update(pad)
    return ctx.digest()[:16]


def Encrypt(inputBuffer, key):
    cipher = AES.new(key, AES.MODE_ECB)
    paddedBuffer = Padding.pad(inputBuffer, 128)
    clearBuffer = cipher.encrypt(paddedBuffer)
    return clearBuffer[0:len(inputBuffer)]


def DFunction(R2, ASk):
    assert 0 < len(R2) < 56
    assert ASk

    theHash = ComputeDFunctionHash(R2)
    dASK = Encrypt(theHash, ASk)

    return dASK
