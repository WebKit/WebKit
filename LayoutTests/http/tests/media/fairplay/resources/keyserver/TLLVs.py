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

from .TLLV import *
from .Utils import *
from .DFunction import *

import inspect
import sys


class SK_R1(TLLV):
    NAME = "[SK..R1]"
    TAG = 0x3d1a10b8bffac2ec

    def parseValue(self):
        assert self.valueSize == 112

    @property
    def iv(self):
        return self._valueData[0:16]

    @iv.setter
    def iv(self, value):
        assert len(value) == 16
        self._valueData[0:16] = value

    @property
    def payload(self):
        return self._valueData[16:112]

    @property
    def isDecrypted(self):
        return bool(self._decryptedData)

    @property
    def sk(self):
        if not self._decryptedData:
            return None
        return self._decryptedData[0:16]

    @sk.setter
    def sk(self, value):
        assert len(value) == 16
        self._decryptedData[0:16] = value

    @property
    def hu(self):
        if not self._decryptedData:
            return None
        return self._decryptedData[16:36]

    @hu.setter
    def hu(self, value):
        assert len(value) == 20
        self._decryptedData[16:36] = value

    @property
    def r1(self):
        if not self._decryptedData:
            return None
        return self._decryptedData[36:80]

    @r1.setter
    def r1(self, value):
        assert len(value) == 44
        self._decryptedData[36:80] = value

    @property
    def integrity(self):
        if not self._decryptedData:
            return None
        return self._decryptedData[80:96]

    @integrity.setter
    def integrity(self, value):
        assert len(value) == 16
        self._decryptedData[80:96] = value

    def decrypt(self, DASk):
        self._decryptedData = bytearray(Utils.AESEncryptDecrypt(
            self.payload,
            DASk, self.iv,
            SKDServerAESEncType.Decrypt, SKDServerAESEncMode.CBC))

    def encrypt(self, DASk):
        self._valueData[16:112] = bytearray(Utils.AESEncryptDecrypt(
            self._decryptedData,
            DASk, self.iv,
            SKDServerAESEncType.Encrypt, SKDServerAESEncMode.CBC))

    def __str__(self):
        s = super().__str__()
        s += f"\tiv: {self.iv.hex()}\n"
        if not self._decryptedData:
            s += "\tNot Decrypted\n"
        else:
            s += f"\tpayload: {self.payload.hex()}\n"
            s += f"\tsk: {self.sk.hex()}\n"
            s += f"\thu: {self.hu.hex()}\n"
            s += f"\tr1: {self.r1.hex()}\n"
            s += f"\tintegrity: {self.integrity.hex()}\n"
        return s


class SK_R1_Integrity(TLLV):
    NAME = "[SK..R1] Integrity"
    TAG = 0xb349d4809e910687

    def parseValue(self):
        assert self.valueSize == 16

    @property
    def value(self):
        return self._valueData

    @value.setter
    def value(self, value):
        assert self.valueSize == 21
        self.valueData = value

    def __str__(self):
        s = super().__str__()
        s += f"\tvalue: {self.value.hex()}\n"
        return s


class Capbilities(TLLV):
    NAME = "Capabilities"
    TAG = 0x9c02af3253c07fb2

    def parseValue(self):
        assert self.valueSize == 16

    @property
    def capabilityFlags(self):
        return int.from_bytes(self._valueData, byteorder="big")

    def has(self, flags: SKDCapabilityFlags):
        assert isinstance(flags, SKDCapabilityFlags)
        return self.capabilityFlags & flags == flags

    def set(self, flags: SKDCapabilityFlags):
        assert isinstance(flags, SKDCapabilityFlags)
        newFlags = self.capabilityFlags | flags
        self._valueData = newFlags.to_bytes(16, byteorder="big")

    def clear(self, flags: SKDCapabilityFlags):
        assert isinstance(flags, SKDCapabilityFlags)
        keepBits = 0xFFFFFFFF ^ flags
        newFlags = self.capabilityFlags & keepBits
        self._valueData = newFlags.to_bytes(16, byteorder="big")


class AntiReplaySeed(TLLV):
    NAME = "Anti-Replay Seed"
    TAG = 0x89c90f12204106b2

    def parseValue(self):
        assert self.valueSize == 16

    @property
    def value(self):
        return self._valueData

    @value.setter
    def value(self, value):
        assert self.valueSize == 16
        self.valueData = value


class R2(TLLV):
    NAME = "R2"
    TAG = 0x71b5595ac1521133

    def parseValue(self):
        assert self.valueSize == 21

    @property
    def value(self):
        return self._valueData

    @value.setter
    def value(self, value):
        assert self.valueSize == 21
        self.valueData = value


class ReturnTagRequest(TLLV):
    NAME = "Return Tag Request"
    TAG = 0x19f9d4e5ab7609cb

    def parseValue(self):
        assert self.valueSize % 8 == 0

    @property
    def returnTags(self):
        offset = 0
        returnTags = []
        while offset < self.valueSize:
            returnTags.append(Utils.GetBigEndian64(self.valueData, offset))
            offset += 8
        return returnTags

    @returnTags.setter
    def returnTags(self, values):
        valueData = bytearray(8 * len(values))
        offset = 0
        for value in values:
            Utils.SetBigEndian64(value, valueData, offset)
            offset += 8
        self.valueData = valueData


class AssetID(TLLV):
    NAME = "Asset ID"
    TAG = 0x1bf7f53f5d5d5a1f

    def parseValue(self):
        assert 2 <= self.valueSize <= 200

    @property
    def value(self):
        return self._valueData

    @value.setter
    def value(self, value):
        assert 2 <= len(value) <= 200
        self.valueData = value


class TransactionID(TLLV):
    NAME = "Transaction ID"
    TAG = 0x47aa7ad3440577de

    def parseValue(self):
        assert self.valueSize == 8


class ProtocolVersionsSupported(TLLV):
    NAME = "Protocol Versions Supported"
    TAG = 0x67b8fb79ecce1a13

    def parseValue(self):
        assert self.valueSize % 4 == 0

    @property
    def supportedVersions(self):
        offset = 0
        supportedVersions = []
        while offset < self.valueSize:
            supportedVersions.append(Utils.GetBigEndian32(self.valueData, offset))
            offset += 4
        return supportedVersions

    @supportedVersions.setter
    def supportedVersions(self, values):
        valueData = bytearray(4 * len(values))
        offset = 0
        for value in values:
            Utils.SetBigEndian32(value, valueData, offset)
            offset += 4
        self.valueData = valueData


class ProtocolVersionUsed(TLLV):
    NAME = "Protocol Version Used"
    TAG = 0x5d81bcbcc7f61703

    def parseValue(self):
        assert self.valueSize == 4

    @property
    def usedVersion(self):
        return Utils.GetBigEndian32(self.valueData)

    @usedVersion.setter
    def usedVersion(self, value):
        valueData = bytearray(4)
        Utils.SetBigEndian32(value, valueData)
        self.valueData = valueData


class StreamingIndicator(TLLV):
    NAME = "Streaming Indicator"
    TAG = 0xabb0256a31843974

    def parseValue(self):
        assert self.valueSize == 8

    @property
    def value(self):
        return Utils.GetBigEndian64(self.valueData)

    @value.setter
    def value(self, value):
        valueData = bytearray(8)
        Utils.SetBigEndian64(value, valueData)
        self.valueData = valueData


class MediaPlaybackState(TLLV):
    NAME = "Media Playback State"
    TAG = 0xeb8efdf2b25ab3a0

    def parseValue(self):
        assert self.valueSize == 16

    @property
    def creationDate(self):
        return Utils.GetBigEndian32(self.valueData, 0)

    @creationDate.setter
    def creationDate(self, value):
        Utils.SetBigEndian32(value, self.valueData, 0)

    @property
    def playbackState(self):
        return Utils.GetBigEndian32(self.valueData, 4)

    @playbackState.setter
    def playbackState(self, value):
        Utils.SetBigEndian32(value, self.valueData, 4)

    @property
    def sessionID(self):
        return Utils.GetBigEndian32(self.valueData, 0)

    @sessionID.setter
    def sessionID(self, value):
        Utils.SetBigEndian32(value, self.valueData, 0)


class EncryptedContentKey(TLLV):
    NAME = "Encrypted Content Key"
    TAG = 0x58b38165af0e3d5a

    @classmethod
    def create(self, iv, ck):
        valueData = bytearray(32)
        assert len(iv) == 16
        valueData[0:16] = iv

        assert len(ck) == 16
        valueData[16:32] = ck

        return EncryptedContentKey(self.TAG, len(valueData), len(valueData), valueData, None)

    def parseValue(self):
        assert self.valueSize >= 32

    @property
    def iv(self):
        return self.valueData[0:16]

    @iv.setter
    def iv(self, value):
        assert len(value) == 16
        self.valueData[0:16] = value

    @property
    def ck(self):
        return self.valueData[16:32]

    @ck.setter
    def ck(self, value):
        assert len(value) == 16
        self.valueData[16:32] = value

    def __str__(self):
        s = super().__str__()
        s += f"\tiv: {self.iv.hex()}\n"
        s += f"\tck: {self.ck.hex()}\n"
        return s


class R1(TLLV):
    NAME = "R1"
    TAG = 0xea74c4645d5efee9

    @classmethod
    def create(self, value):
        valueSize = len(value)
        padding = TLLV.paddingForValueData(value)
        totalSize = valueSize + len(padding)
        return R1(self.TAG, totalSize, valueSize, value, padding)

    def parseValue(self):
        assert self.valueSize >= 44

    @property
    def value(self):
        return self._valueData

    @value.setter
    def value(self, value):
        assert self.valueSize == 44
        self.valueData = value


class ContentKeyDuration(TLLV):
    NAME = "Content Key Duration"
    TAG = 0x47acf6a418cd091a

    @classmethod
    def create(self, leaseDuration, rentalDuration, keyType):
        valueSize = 16
        valueData = bytearray(valueSize)
        Utils.SetBigEndian32(leaseDuration, valueData, 0)
        Utils.SetBigEndian32(rentalDuration, valueData, 4)
        Utils.SetBigEndian32(keyType, valueData, 8)
        Utils.SetBigEndian32(0x86d34a3a, valueData, 12)
        padding = TLLV.paddingForValueData(valueData)
        totalSize = len(padding) + valueSize

        return ContentKeyDuration(self.TAG, totalSize, valueSize, valueData, padding)

    def parseValue(self):
        assert self.valueSize >= 32

    @property
    def leaseDuration(self):
        return Utils.GetBigEndian32(self.valueData, 0)

    @leaseDuration.setter
    def leaseDuration(self, value):
        Utils.SetBigEndian32(value, self.valueData, 0)

    @property
    def rentalDuration(self):
        return Utils.GetBigEndian32(self.valueData, 4)

    @rentalDuration.setter
    def rentalDuration(self, value):
        Utils.SetBigEndian32(value, self.valueData, 4)

    @property
    def keyType(self):
        return Utils.GetBigEndian32(self.valueData, 8)

    @keyType.setter
    def keyType(self, value):
        Utils.SetBigEndian32(value, self.valueData, 8)

    def __str__(self):
        s = super().__str__()
        s += f"\tleaseDuration: {self.leaseDuration:#x}\n"
        s += f"\trentalDuration: {self.rentalDuration:#x}\n"
        s += f"\tkeyType: {self.keyType:#x}\n"
        return s


class HDCPEnforcement(TLLV):
    NAME = "HDCP Enforcement"
    TAG = 0x2e52f1530d8ddb4a

    def parseValue(self):
        assert self.valueSize == 8

    @property
    def value(self):
        return Utils.GetBigEndian32(self.valueData, 0)

    @value.setter
    def value(self, value):
        Utils.SetBigEndian32(value, self.valueData, 0)


# Create the TLLV_MAP
for cls in inspect.getmembers(sys.modules[__name__], inspect.isclass):
    if issubclass(cls[1], TLLV) and cls[1] != Unknown and cls[1] != TLLV:
        TLLV_MAP.update({cls[1].TAG: cls[1]})
