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
from .Utils import *
import unittest


class TestUtils(unittest.TestCase):
    TestPrivKey = bytearray(b"""-----BEGIN RSA PRIVATE KEY-----
MIICXAIBAAKBgQDTgbAd5HOtk+kkPf7jrZYzLWFnqXqfo7cch6KEJY/BYi8J5aoV
Fn3JT5HMPqj1rh4e+clAMR6sYE/nFgr2BnD37MDaPa5fxoKSSOjmau0h0kpoR9Ih
FlQX9+nMs+GtRQtK+vDwVMbGPTzQLJ3ePwOm3NqYSbtSZavoqeFODeoyeQIDAQAB
AoGAHgRYz1U9yZOlUdxukTdwiqDVIxYdbgyZDzZ8rQ57oXFixZ/PjKCdh3WAdqEp
73wfkDvZAQA3nzUoAd7R/Wqam4Konz+k4LqXhIfUA7rIX9JXF0Tt+Sf+/WtNydlu
s1LEpFK1W4Btttw3JNZu2Yrn5hCjS1NgYpin0ddfN+KF5AECQQD/3MBd2Fck/D4o
dHgVxOxqJC5I4/Am92z6VMhv9pj4kzpk1QYT2evy4OvJ/2G+cBC/ccXvgxA7Oi+f
yYd8P5j5AkEA057Tbu3xFcdrWPJIU+NayswmyJdu8n7lcXrIOsPkVtGnE9jcajuQ
K6RjgJnrBHyMtuaGQgG08H6zTyYUvntFgQJAK9fJ5srRc/b9N7Vtdl1+HVoBzF+y
oRq4w9SPXBAnekDQOsL9/ZzT/5RnEv/94cIWPJfkUPxmZZ+81WaVHsdPEQJAacUU
gHlE6lHGAar3I5abJolrSQ8HUQTDwThRI64NTqdzDqRxZldm86kyYnCL2q411qTZ
rdwUh6+MB59M9ve+AQJBAJh9fsNntLk3Z4eAx3ABZInMhmYM2VHEXbgzAhH6DP0i
M4zO8ULFQ0UzXaIoXF0tad4Ukg4KEGa2XjfEFHKVUe0=
-----END RSA PRIVATE KEY-----""")

    TestCypherText = bytes.fromhex("""
4947 3969 cf9b 033b fc7c 7f1d 1a39 9a22
dc1b e696 4fb9 6877 d1c0 75a6 98e7 727a
b8f0 dcb1 e888 8fb2 a185 ec5e d68e 4cbb
705b 056b 3d50 de50 53b7 abe2 78f7 1220
b7e6 e2b2 c065 26e2 4a6e c500 9b6b 8035
8e6b f8e5 1dfe f51d f3ca f5d1 ff90 3140
3e06 6307 ea30 0afb acef 8219 e6e9 1ae1
2ef3 5784 2568 306f 98d6 fd80 090e 25f2""")

    TestAESKey = bytes.fromhex("ca25ce20836c5f89215c5cad614a5790")
    TestWrappedKey = bytes.fromhex("""
99bc7d5d86ecba6ce55c0a6b0178fa29bfee456ed1a4cdf1d4897538e875
1668fd9c8fed4692c0b7c08ffe925b7a901d9185c142d2c409bfb1080ab0
a86ca506e6271a74f7370bdce156af480a85f032ccc7967209533aceff93
989a56af6fb1de9c0a78a1ed92650b035c5d7d5d1d80c40ca4102ed7952a
a8d0740e719a45f3""")
    TestIV = bytes.fromhex("63c617fc46c5110efa9587802907c85f")

    def test_GenRandom16(self):
        self.assertEqual(len(Utils.GenRandom(16)), 16)
        self.assertNotEqual(Utils.GenRandom(16), Utils.GenRandom(16))

    def test_GenRandom20(self):
        self.assertEqual(len(Utils.GenRandom(20)), 20)
        self.assertNotEqual(Utils.GenRandom(20), Utils.GenRandom(20))

    def test_RSADecryptKey(self):

        self.assertEqual(len(self.TestCypherText), 128)
        privateKey = Utils.LoadPrivateKey(self.TestPrivKey)
        self.assertEqual(Utils.RSADecryptKey(self.TestCypherText, privateKey), b"foo bar baz")
        self.assertEqual(Utils.RSADecryptKey(self.TestWrappedKey, privateKey), self.TestAESKey)

    def test_AESEncryptDecrypt(self):
        self.assertEqual(Utils.AESEncryptDecrypt(b"foo bar baz biz\0", self.TestAESKey, self.TestIV, SKDServerAESEncType.Encrypt, SKDServerAESEncMode.CBC), bytes.fromhex("3b24aca79f81eeb61c6a58b7fb7dee51"))
        self.assertEqual(Utils.AESEncryptDecrypt(b"foo bar baz biz\0", self.TestAESKey, None, SKDServerAESEncType.Encrypt, SKDServerAESEncMode.ECB), bytes.fromhex("9d00976a08abdc8a5ec231fd6cb4da2a"))
        self.assertTrue(Utils.AESEncryptDecrypt(bytes.fromhex("3b24aca79f81eeb61c6a58b7fb7dee51"), self.TestAESKey, self.TestIV, SKDServerAESEncType.Decrypt, SKDServerAESEncMode.CBC).startswith(b"foo bar baz biz"))
        self.assertTrue(Utils.AESEncryptDecrypt(bytes.fromhex("9d00976a08abdc8a5ec231fd6cb4da2a"), self.TestAESKey, None, SKDServerAESEncType.Decrypt, SKDServerAESEncMode.ECB).startswith(b"foo bar baz biz"))
