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

from Crypto.Cipher import AES, PKCS1_OAEP
from Crypto.Util import Padding
from Crypto.PublicKey import RSA
from array import array

import secrets
import struct


class Utils:
    @staticmethod
    def LoadPrivateKey(keyBuffer):
        assert keyBuffer
        privateKey = RSA.import_key(keyBuffer, passphrase=None)
        assert isinstance(privateKey, RSA.RsaKey)
        return privateKey

    @staticmethod
    def GenRandom(size) -> bytes:
        return secrets.token_bytes(size)

    @staticmethod
    def RSADecryptKey(aesWrappedKey, privateKey) -> bytes:
        assert aesWrappedKey
        assert privateKey

        cipher = PKCS1_OAEP.new(privateKey)
        aesKey = cipher.decrypt(aesWrappedKey)
        return aesKey

    @staticmethod
    def AESEncryptDecrypt(inputBuffer, key, iv, opType, opMode):
        assert inputBuffer
        assert key

        cipher = None

        if opMode == SKDServerAESEncMode.CBC:
            cipher = AES.new(key, AES.MODE_CBC, iv)
        elif opMode == SKDServerAESEncMode.ECB:
            cipher = AES.new(key, AES.MODE_ECB)
        else:
            raise AssertionError

        paddedBuffer = Padding.pad(inputBuffer, 128)

        clearBuffer = None
        if opType == SKDServerAESEncType.Encrypt:
            clearBuffer = cipher.encrypt(paddedBuffer)
        elif opType == SKDServerAESEncType.Decrypt:
            clearBuffer = cipher.decrypt(paddedBuffer)
        else:
            raise AssertionError

        return clearBuffer[0:len(inputBuffer)]

    Assets = {
        "one": SKDAssetInfo("one", bytearray.fromhex('31313131313131313131313131313131'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "two": SKDAssetInfo("two", bytearray.fromhex('32323232323232323232323232323232'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "three": SKDAssetInfo("three", bytearray.fromhex('33333333333333333333333333333333'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "four": SKDAssetInfo("four", bytearray.fromhex('34343434343434343434343434343434'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "five": SKDAssetInfo("five", bytearray.fromhex('35353535353535353535353535353535'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "six": SKDAssetInfo("six", bytearray.fromhex('36363636363636363636363636363636'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "seven": SKDAssetInfo("seven", bytearray.fromhex('37373737373737373737373737373737'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "eight": SKDAssetInfo("eight", bytearray.fromhex('38383838383838383838383838383838'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "nine": SKDAssetInfo("nine", bytearray.fromhex('39393939393939393939393939393939'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "ten": SKDAssetInfo("ten", bytearray.fromhex('3A3A3A3A3A3A3A3A3A3A3A3A3A3A3A3A'), bytearray.fromhex('546865517569636B42726F776E466F78'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "eleven": SKDAssetInfo("eleven", bytearray.fromhex('3B3B3B3B3B3B3B3B3B3B3B3B3B3B3B3B'), bytearray.fromhex('D5FBD6B82ED93E4EF98AE40931EE33B7'), 86400, 0, 0, SKDHDCPInformation.Type0),
        "twelve": SKDAssetInfo("twelve", bytearray.fromhex('3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C'), bytearray.fromhex('D5FBD6B82ED93E4EF98AE40931EE33B7'), 86400, 0, 0, SKDHDCPInformation.Type0)
    }

    @staticmethod
    def FetchContentKeyAndIV(assetId) -> SKDAssetInfo:
        assert Utils.Assets[assetId.decode()]
        return Utils.Assets[assetId.decode()]

    @staticmethod
    def GetSupportedVersions() -> array:
        return array('b', [1])

    @staticmethod
    def GetBigEndian32(buffer, offset=0):
        if len(buffer) < 4:
            raise Exception()
        result = struct.unpack_from('>L', buffer, offset)
        if len(result) == 0:
            raise Exception()
        return result[0]

    @staticmethod
    def GetBigEndian64(buffer, offset=0):
        if len(buffer) < 8:
            raise Exception()
        result = struct.unpack_from('>Q', buffer, offset)
        if len(result) == 0:
            raise Exception()
        return result[0]

    @staticmethod
    def SetBigEndian32(x, buffer, offset=0):
        return struct.pack_into('>L', buffer, offset, x)

    @staticmethod
    def SetBigEndian64(x, buffer, offset=0):
        return struct.pack_into('>Q', buffer, offset, x)
