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

from .Structs import *
from typing import Final
from .Utils import *
from collections import defaultdict


class TLLV():
    TAG_SZ: Final = 8
    TOTAL_LENGTH_SZ: Final = 4
    VALUE_LENGTH_SZ: Final = 4
    HEADER_SZ: Final = TAG_SZ + TOTAL_LENGTH_SZ + VALUE_LENGTH_SZ

    @classmethod
    def parse(self, buffer, offset: Offset = None):
        if not offset:
            offset = Offset(0)
        assert offset.value + self.HEADER_SZ <= len(buffer)

        tagValue = Utils.GetBigEndian64(buffer, offset.value)
        offset.value += self.TAG_SZ

        totalSize = Utils.GetBigEndian32(buffer, offset.value)
        offset.value += self.TOTAL_LENGTH_SZ

        valueSize = Utils.GetBigEndian32(buffer, offset.value)
        offset.value += self.VALUE_LENGTH_SZ

        assert valueSize <= totalSize
        assert offset.value + totalSize <= len(buffer)

        valueData = buffer[offset.value:offset.value + valueSize]
        offset.value += valueSize

        paddingSize = totalSize - valueSize
        paddingData = buffer[offset.value:offset.value + paddingSize]
        offset.value += paddingSize

        tllv = TLLV_MAP[tagValue](tagValue, totalSize, valueSize, valueData, paddingData)
        tllv.parseValue()
        return tllv

    @classmethod
    def paddingForValueData(self, valueData):
        valueSize = len(valueData)
        remainder = valueSize % 16
        if remainder != 0:
            return Utils.GenRandom(16 - remainder)
        return bytearray()

    def __init__(self, tagValue, totalSize, valueSize, valueData, paddingData):
        assert valueSize == len(valueData)
        assert (self.HEADER_SZ + totalSize) % 16 == 0
        self._tagValue = tagValue
        self._totalSize = totalSize
        self._valueSize = valueSize
        self._valueData = bytearray(valueData)
        self._paddingData = bytearray(paddingData) if paddingData else bytearray()

    @property
    def tagValue(self):
        return self._tagValue

    @property
    def totalSize(self):
        return self._totalSize

    @property
    def valueSize(self):
        return self._valueSize

    @property
    def paddingSize(self):
        return self._totalSize - self._valueSize

    @property
    def valueData(self):
        return self._valueData

    @valueData.setter
    def valueData(self, value):
        self._valueData = bytearray(value)
        self._valueSize = len(value)
        self._paddingData = self.paddingForValueData(self._valueData)
        self._totalSize = len(self._valueData) + len(self._paddingData)

    @property
    def paddingData(self):
        return self._paddingData

    def parseValue(self):
        return

    def serialize(self):
        serializedData = bytearray(self.HEADER_SZ + self.totalSize)

        Utils.SetBigEndian64(self.tagValue, serializedData, 0)
        Utils.SetBigEndian32(self.totalSize, serializedData, 8)
        Utils.SetBigEndian32(self.valueSize, serializedData, 12)
        serializedData[16:] = self._valueData
        if self._paddingData:
            serializedData[16 + len(self.valueData):] = self._paddingData
        return serializedData

    def __str__(self):
        s = f"{self.NAME} -- {self.tagValue:#x}\n"
        s += f"\tTotal Size: {self.totalSize:#x}\n"
        s += f"\tValue Size: {self.valueSize:#x}\n"
        s += f"\t\t{self.valueData.hex(' ', 8)}\n"
        return s


class Unknown(TLLV):
    NAME = "Unknown"


TLLV_MAP = defaultdict(lambda: Unknown)
