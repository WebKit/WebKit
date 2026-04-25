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

from .DFunction import *

import unittest


class TestDFunction(unittest.TestCase):
    def test_DFunctionHash(self):
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("a4c411c54d94727143504aece5613da8c6ee6dd2")).hex(), bytearray.fromhex("3a24a068c116842f10fac5c86cb1cb5c").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("ee810dd5779ce50aa8df3824b3f2594cf9ee68a9")).hex(), bytearray.fromhex("93c82dc4d1c4a338896bfce2e7dc12a0").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("9a0618db8a4ecc1c8f1fa8e9482dbbe56647afe1")).hex(), bytearray.fromhex("73b8445fd639462154f3503c6ea32c89").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("303838a2b4cab66f25d53bd331b5ee4c035b6e12")).hex(), bytearray.fromhex("c74f35704ec58c0882bc4014a74152a8").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("a4c411c54d94727143504aece5613da8c6ee6dd26ba5d07ec0dc44c84ec4")).hex(), bytearray.fromhex("4b46ab65bd04af0dd721718a68663390").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("ee810dd5779ce50aa8df3824b3f2594cf9ee68a9c6caa7a36709f55494d5")).hex(), bytearray.fromhex("c63d0c630bcd579d072901dba82ae2a6").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("9a0618db8a4ecc1c8f1fa8e9482dbbe56647afe10739e4bd00d29db0d9cc")).hex(), bytearray.fromhex("2d467d6fc4aab309e8b4618247567e06").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("303838a2b4cab66f25d53bd331b5ee4c035b6e12afcd05c5a5a85a382ff4")).hex(), bytearray.fromhex("353d491edc8f6f8191a3cb366bafe14c").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("a4c411c54d94727143504aece5613da8c6ee6dd26ba5d07ec0dc44c84ec48140b244d5be323d251a")).hex(), bytearray.fromhex("8439a36b359c7c1c5b698c594b56b020").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("ee810dd5779ce50aa8df3824b3f2594cf9ee68a9c6caa7a36709f55494d5c56971d390bc57a68223")).hex(), bytearray.fromhex("430b6db2ce11ce6bcd7076187c44aa1b").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("303838a2b4cab66f25d53bd331b5ee4c035b6e12afcd05c5a5a85a382ff4bfae230844aef7e2ae5c")).hex(), bytearray.fromhex("a77994aeda51df868cd82bd3b1131401").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("9a0618db8a4ecc1c8f1fa8e9482dbbe56647afe10739e4bd00d29db0d9ccf4ba5d556f4c869ebb9e")).hex(), bytearray.fromhex("fcff3ff19e2911e52ce252e85f0498da").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("a4c411c54d94727143504aece5613da8c6ee6dd26ba5d07ec0dc44c84ec48140b244d5be323d251acb3a4bb19898891973db")).hex(), bytearray.fromhex("f2272c254504fc4ce7af2873c1de184f").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("ee810dd5779ce50aa8df3824b3f2594cf9ee68a9c6caa7a36709f55494d5c56971d390bc57a68223d588edb0e48ec073d28b")).hex(), bytearray.fromhex("0f25a9bf940daca0a520937be1752f75").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("9a0618db8a4ecc1c8f1fa8e9482dbbe56647afe10739e4bd00d29db0d9ccf4ba5d556f4c869ebb9eceeb019acaa7ae6c8ca7")).hex(), bytearray.fromhex("10e41b10df29f8c2ea91fe7d126f50e1").hex())
        self.assertEqual(ComputeDFunctionHash(bytearray.fromhex("303838a2b4cab66f25d53bd331b5ee4c035b6e12afcd05c5a5a85a382ff4bfae230844aef7e2ae5c834671c1632afca943eb")).hex(), bytearray.fromhex("b1caecddef433ceb4d8ed554f1702dbd").hex())
