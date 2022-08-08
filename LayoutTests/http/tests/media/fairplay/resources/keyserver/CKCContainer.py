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

from .Constants import *
from .Structs import *
from .Utils import *
from .TLLVs import *


class CKCContainer():
    @classmethod
    def __init__(self, ckcVersion, iv):
        self._ckcVersion = ckcVersion
        self._iv = iv
        self._tllvs = []
        self._tllvTags = set()
        self._encryptedPayload = None

    @property
    def version(self):
        return self._ckcVersion

    @version.setter
    def version(self, value):
        self._ckcVersion = value

    @property
    def iv(self):
        return self._iv

    @iv.setter
    def iv(self, value):
        self._iv = value

    @property
    def tllvs(self):
        return self._tllvs

    @property
    def encryptedPayload(self):
        return self._encryptedPayload

    def addTLLV(self, tllv):
        assert isinstance(tllv, TLLV)
        assert tllv.tagValue not in self._tllvTags
        self._tllvs.append(tllv)
        self._tllvTags.add(tllv.tagValue)

    def getTLLV(self, tagValue):
        return next(filter(lambda tllv: tllv.tagValue == tagValue, self.tllvs), None)

    def encrypt(self, AR):
        clearPayload = bytearray()

        assert EncryptedContentKey.TAG in self._tllvTags
        assert R1.TAG in self._tllvTags
        assert ContentKeyDuration.TAG in self._tllvTags

        for tllv in self._tllvs:
            clearPayload += tllv.serialize()

        self._encryptedPayload = bytearray(Utils.AESEncryptDecrypt(
            clearPayload,
            AR, self.iv,
            SKDServerAESEncType.Encrypt, SKDServerAESEncMode.CBC))

    def serialize(self):
        assert self._encryptedPayload
        serializedData = bytearray(28 + len(self._encryptedPayload))

        Utils.SetBigEndian32(self.version, serializedData, 0)
        serializedData[8:24] = self.iv
        Utils.SetBigEndian32(len(self._encryptedPayload), serializedData, 24)
        serializedData[28:] = self._encryptedPayload
        return serializedData

    def __str__(self):
        s = "CKC:\n"
        s += f"\tCKC Version: {self.version:#x}\n"
        s += f"\tCKC IV: {self.iv.hex()}\n"
        s += "\n"
        s += "TLLVs:\n"
        for tllv in self.tllvs:
            s += tllv.__str__()
            s += "\n"
        return s
