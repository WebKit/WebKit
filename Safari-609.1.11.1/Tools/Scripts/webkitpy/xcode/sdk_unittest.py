#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2019 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

import webkitpy.xcode.sdk as sdk
from webkitpy.common.system.executive_mock import MockExecutive2


class SDKTest(unittest.TestCase):

    TEST_XCODEBUILD_SHOWSDKS_OUTPUT = """\
        iOS SDKs:
        \tiOS 12.0                      \t-sdk iphoneos12.0
        \tiOS 12.0 Internal             \t-sdk iphoneos12.0.internal

        iOS Simulator SDKs:
        \tSimulator - iOS 12.0          \t-sdk iphonesimulator12.0

        macOS SDKs:
        \tmacOS 10.13                   \t-sdk macosx10.13
        \tmacOS 10.13 Internal          \t-sdk macosx10.13internal
        \tmacOS 10.14                   \t-sdk macosx10.14
        \tmacOS 10.14 Internal          \t-sdk macosx10.14internal

        tvOS SDKs:
        \ttvOS 12.0                     \t-sdk appletvos12.0
        \ttvOS 12.0 Internal            \t-sdk appletvos12.0.internal

        tvOS Simulator SDKs:
        \tSimulator - tvOS 12.0         \t-sdk appletvsimulator12.0

        watchOS SDKs:
        \twatchOS 4.0                   \t-sdk watchos4.0
        \twatchOS 4.0 Internal          \t-sdk watchos4.0.internal

        watchOS Simulator SDKs:
        \tSimulator - watchOS 4.0       \t-sdk watchsimulator4.0
        """

    @classmethod
    def setUpClass(cls):
        cls.executive = MockExecutive2(output=cls.TEST_XCODEBUILD_SHOWSDKS_OUTPUT)

    @classmethod
    def tearDownClass(cls):
        cls.executive = None

    def test_iphoneos(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("iphoneos", self.executive)
        self.assertEquals("iphoneos", preferred_sdk.platform)
        self.assertEquals("12.0", preferred_sdk.version)
        self.assertTrue(preferred_sdk.internal)

    def test_iphonesim(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("iphonesimulator", self.executive)
        self.assertEquals("iphonesimulator", preferred_sdk.platform)
        self.assertEquals("12.0", preferred_sdk.version)
        self.assertFalse(preferred_sdk.internal)

    def test_macos(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("macosx", self.executive)
        self.assertEquals("macosx", preferred_sdk.platform)
        self.assertEquals("10.14", preferred_sdk.version)
        self.assertTrue(preferred_sdk.internal)

    def test_appletvos(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("appletvos", self.executive)
        self.assertEquals("appletvos", preferred_sdk.platform)
        self.assertEquals("12.0", preferred_sdk.version)
        self.assertTrue(preferred_sdk.internal)

    def test_appletvsim(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("appletvsimulator", self.executive)
        self.assertEquals("appletvsimulator", preferred_sdk.platform)
        self.assertEquals("12.0", preferred_sdk.version)
        self.assertFalse(preferred_sdk.internal)

    def test_watchos(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("watchos", self.executive)
        self.assertEquals("watchos", preferred_sdk.platform)
        self.assertEquals("4.0", preferred_sdk.version)
        self.assertTrue(preferred_sdk.internal)

    def test_watchsimulator(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("watchsimulator", self.executive)
        self.assertEquals("watchsimulator", preferred_sdk.platform)
        self.assertEquals("4.0", preferred_sdk.version)
        self.assertFalse(preferred_sdk.internal)

    def test_prodos(self):
        preferred_sdk = sdk.SDK.get_preferred_sdk_for_platform("prodos", self.executive)
        self.assertEquals("prodos", preferred_sdk.platform)
        self.assertEquals(None, preferred_sdk.version)
        self.assertEquals(None, preferred_sdk.internal)
