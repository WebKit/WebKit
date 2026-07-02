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
from .TLLVs import *
import unittest


class TestTLLVs(unittest.TestCase):

    def test_SK_R1(self):
        TestDASK = bytearray.fromhex("d9f11e0d0e8e08bc875173eba62526e1")
        TestSK_R1 = bytearray.fromhex("3d1a10b8bffac2ec0000007000000070cdfdd1b658e8f671010d42be671ef3e097cbf1992626f1e144d778c7f2fc3a7f6859cec85ac81ff5ae27edfa17dbbc178b57b2281b27a19fa47cf2151757ddd5dbdf11ed7bf4a84920f308bbc503530b774f8bda1406fb7f4d8166ecfc6a7b340e3e18d6ba42cd44e59e9561052121e5")
        tllv = TLLV.parse(TestSK_R1)
        self.assertTrue(isinstance(tllv, SK_R1))
        self.assertEqual(tllv.tagValue, SK_R1.TAG)
        self.assertEqual(tllv.totalSize, 112)
        self.assertEqual(tllv.valueSize, 112)
        self.assertEqual(tllv.paddingSize, 0)
        self.assertEqual(tllv.iv.hex(), "cdfdd1b658e8f671010d42be671ef3e0")
        tllv.decrypt(TestDASK)
        self.assertEqual(tllv.sk.hex(), "7e88d66fef9394f893ead034a8f5cb2e")
        self.assertEqual(tllv.hu.hex(), "c16ee4e768b6a501af5dda8d31bfa1ff15fbb47f")
        self.assertEqual(tllv.r1.hex(), "810e309ffe133d6438d811a3198a387c4c181580f961ce3d1f7ee5dccc15cd30616fc87b7fb72bab6df1d4bc")
        self.assertEqual(tllv.integrity.hex(), "e64ec97c7f6200d41dc18ae43af8e175")

        tllv.iv = bytearray.fromhex("9f2e29cda7f368976cabf2b70c15aa97")
        tllv.sk = bytearray.fromhex("b8300686fe38f5e02a234d84c09c8a35")
        tllv.hu = bytearray.fromhex("b149a28820b8c56a72c9c5409a4bbf27f5422ecb")
        tllv.r1 = bytearray.fromhex("6510e2d4c9fe0a501d1a66eb6150f755a1d131c5c9909e093a439c44775a8698960a43eb06a4dc3e627cb4b1")
        tllv.integrity = bytearray.fromhex("9942eb0edaced80f13e7ac736aef65b5")
        tllv.encrypt(TestDASK)
        self.assertEqual(tllv.payload.hex(), "3370dc92acbff36fbcf280957ff5ae743cb21a4430307136917b1ebe8d288c2b5b2a780a32286f574c9b068879c5486b957a07c4a237625844512b2a4040d070de65267874f624dbd123c68a7df79a0befdd0386d05ddbc0a7c28e27faa250b9")
        self.assertEqual(tllv.serialize().hex(), "3d1a10b8bffac2ec00000070000000709f2e29cda7f368976cabf2b70c15aa973370dc92acbff36fbcf280957ff5ae743cb21a4430307136917b1ebe8d288c2b5b2a780a32286f574c9b068879c5486b957a07c4a237625844512b2a4040d070de65267874f624dbd123c68a7df79a0befdd0386d05ddbc0a7c28e27faa250b9")

    def test_EncryptedContentKey(self):
        TestEncryptedContentKey = bytearray.fromhex("58b38165af0e3d5a0000002000000020bf0a92588ba454cb145a877bfdceb128f54afaa839c58eff332710517b87518f")
        tllv = TLLV.parse(TestEncryptedContentKey)
        self.assertTrue(isinstance(tllv, EncryptedContentKey))
        self.assertEqual(tllv.iv.hex(), "bf0a92588ba454cb145a877bfdceb128")
        self.assertEqual(tllv.ck.hex(), "f54afaa839c58eff332710517b87518f")
        tllv.iv = bytearray.fromhex("a9dfbc63a7b78a11f3a9eb2ed94d93e0")
        tllv.ck = bytearray.fromhex("620f96bedfba9925b0281c0038d0164e")
        self.assertEqual(tllv.valueData.hex(), "a9dfbc63a7b78a11f3a9eb2ed94d93e0620f96bedfba9925b0281c0038d0164e")
        self.assertEqual(tllv.serialize().hex(), "58b38165af0e3d5a0000002000000020a9dfbc63a7b78a11f3a9eb2ed94d93e0620f96bedfba9925b0281c0038d0164e")

    def test_Capabilities(self):
        TestCapabilities = bytearray.fromhex("9C02AF3253C07FB2000000100000001000000000000000000000000000000003")
        tllv = TLLV.parse(TestCapabilities)
        self.assertTrue(isinstance(tllv, Capbilities))
        self.assertTrue(tllv.has(SKDCapabilityFlags.HDCPEnforcement))
        self.assertTrue(tllv.has(SKDCapabilityFlags.OfflineKey))
        self.assertFalse(tllv.has(SKDCapabilityFlags.SecureInvalidation))
        self.assertFalse(tllv.has(SKDCapabilityFlags.OfflineKeyTLLVv2))
        tllv.set(SKDCapabilityFlags.SecureInvalidation)
        self.assertEqual(tllv.valueData.hex(), "00000000000000000000000000000007")
        tllv.clear(SKDCapabilityFlags.HDCPEnforcement)
        self.assertEqual(tllv.valueData.hex(), "00000000000000000000000000000006")

    def test_ReturnTagRequest(self):
        TestReturnTagRequest = bytearray.fromhex("19f9d4e5ab7609cb00000020000000203d1a10b8bffac2ec58b38165af0e3d5a9c02af3253c07fb289c90f12204106b2")
        tllv = TLLV.parse(TestReturnTagRequest)
        self.assertTrue(isinstance(tllv, ReturnTagRequest))
        self.assertEqual(len(tllv.returnTags), 4)
        self.assertEqual(hex(tllv.returnTags[0]), "0x3d1a10b8bffac2ec")
        self.assertEqual(hex(tllv.returnTags[1]), "0x58b38165af0e3d5a")
        self.assertEqual(hex(tllv.returnTags[2]), "0x9c02af3253c07fb2")
        self.assertEqual(hex(tllv.returnTags[3]), "0x89c90f12204106b2")
        tllv.returnTags = [0x71b5595ac1521133, 0x19f9d4e5ab7609cb]
        self.assertEqual(tllv.serialize().hex(), "19f9d4e5ab7609cb000000100000001071b5595ac152113319f9d4e5ab7609cb")

    def test_AssetID(self):
        TestAssetID = bytearray.fromhex("1bf7f53f5d5d5a1f00000060000000555c2f0692182e11744fc2b9c7c5d35d0f868f693f925503f21d27a8e8989e13ed3ac5bf8b4d607bc065e044cc178ee83cee295b70a6679c6f56bae2a279057ee54afa9e1bd0f7d1b7e31433f77dec74278869b503e0e7d8e26b11379dd4fdbeab")
        tllv = TLLV.parse(TestAssetID)
        self.assertTrue(isinstance, AssetID)
        self.assertEqual(tllv.value.hex(), "5c2f0692182e11744fc2b9c7c5d35d0f868f693f925503f21d27a8e8989e13ed3ac5bf8b4d607bc065e044cc178ee83cee295b70a6679c6f56bae2a279057ee54afa9e1bd0f7d1b7e31433f77dec74278869b503e0")
        tllv.value = bytearray.fromhex("8b9c0e388ba8a06ca3226f6085932194ef")
        self.assertEqual(tllv.serialize().hex(), "1bf7f53f5d5d5a1f00000020000000118b9c0e388ba8a06ca3226f6085932194ef" + tllv.paddingData.hex())
