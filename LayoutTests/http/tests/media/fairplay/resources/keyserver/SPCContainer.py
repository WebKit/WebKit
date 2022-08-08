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

from .CKCContainer import *
from .Constants import *
from .Structs import *
from .Utils import *
from .TLLVs import *
import hashlib


class SPCContainer():
    @classmethod
    def parse(self, buffer, offset: Offset = None):
        if not offset:
            offset = Offset(0)
        assert offset.value + TLLV.HEADER_SZ <= len(buffer)

        spcVersion = Utils.GetBigEndian32(buffer, offset.value)
        offset.value += 4

        # reserved
        offset.value += 4

        spcIV = buffer[offset.value:offset.value + 16]
        offset.value += 16

        wrappedKey = buffer[offset.value:offset.value + 128]
        offset.value += 128

        certificateHash = buffer[offset.value:offset.value + 20]
        offset.value += 20

        payloadSize = Utils.GetBigEndian32(buffer, offset.value)
        offset.value += 4

        payloadData = buffer[offset.value:offset.value + payloadSize]

        container = SPCContainer(spcVersion, spcIV, wrappedKey, certificateHash, payloadSize, payloadData)

        return container

    def __init__(self, spcVersion, spcIV, wrappedKey, certificateHash, payloadSize, payloadData):
        assert payloadSize == len(payloadData)
        self._spcVersion = spcVersion
        self._spcIV = spcIV
        self._wrappedKey = wrappedKey
        self._certificateHash = certificateHash
        self._payloadSize = payloadSize
        self._payloadData = payloadData
        self._tllvs = []
        self._tllvTags = set()
        self._decryptedData = None

    @property
    def spcVersion(self):
        return self._spcVersion

    @property
    def spcIV(self):
        return self._spcIV

    @property
    def wrappedKey(self):
        return self._wrappedKey

    @property
    def certificateHash(self):
        return self._certificateHash

    @property
    def payloadSize(self):
        return self._payloadSize

    @property
    def payloadData(self):
        return self._payloadData

    @property
    def tllvs(self):
        return self._tllvs

    def decrypt(self, SPCk):
        self._decryptedData = bytearray(Utils.AESEncryptDecrypt(
            self.payloadData,
            SPCk, self.spcIV,
            SKDServerAESEncType.Decrypt, SKDServerAESEncMode.CBC))

        offset = Offset(0)
        while offset.value < self.payloadSize:
            tllv = TLLV.parse(self._decryptedData, offset)
            assert tllv.tagValue not in self._tllvTags

            self._tllvs.append(tllv)
            self._tllvTags.add(tllv.tagValue)

    def getTLLV(self, tagValue):
        return next(filter(lambda tllv: tllv.tagValue == tagValue, self.tllvs), None)

    def checkIntegrity(self):
        assert self._decryptedData
        sk_r1 = self.getTLLV(SK_R1.TAG)
        assert sk_r1

        sk_r1_integrity = self.getTLLV(SK_R1_Integrity.TAG)
        assert sk_r1_integrity

        assert sk_r1.integrity == sk_r1_integrity.value

    def generateCKC(self, assetInfo, ck, r1):
        ckcContainer = CKCContainer(1, Utils.GenRandom(16))

        ckcContainer.addTLLV(EncryptedContentKey.create(assetInfo.iv, ck))
        ckcContainer.addTLLV(R1.create(r1))
        ckcContainer.addTLLV(ContentKeyDuration.create(assetInfo.leaseDuration, assetInfo.rentalDuration, 0x1a4bde7e))

        returnTagRequest = self.getTLLV(ReturnTagRequest.TAG)
        for tag in returnTagRequest.returnTags:
            ckcContainer.addTLLV(self.getTLLV(tag))

        return ckcContainer

    def generateARSeed(self):
        sk_r1 = self.getTLLV(SK_R1.TAG)
        assert sk_r1

        arSeed = self.getTLLV(AntiReplaySeed.TAG)
        assert arSeed

        ctx = hashlib.sha1()
        ctx.update(sk_r1.r1)
        arKey = ctx.digest()[:16]

        return Utils.AESEncryptDecrypt(
            arSeed.value, arKey, None,
            SKDServerAESEncType.Encrypt, SKDServerAESEncMode.ECB)

    def __str__(self):
        s = "SPC:\n"
        s += f"\tSPC Version: {self.spcVersion:#x}\n"
        s += f"\tSPC IV: {self.spcIV.hex()}\n"
        s += f"\tWrapped Key: {self.wrappedKey.hex()}\n"
        s += f"\tCertificate Hash: {self.certificateHash.hex()}\n"
        s += f"\tPayload Size: {self.payloadSize:#x}\n"
        s += "\n"
        s += "TLLVs:\n"
        for tllv in self.tllvs:
            s += tllv.__str__()
            s += "\n"
        return s
