#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import atexit
import lldb
import lldb_webkit
import os
import sys
import unittest

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.port.config import Config

# Run just the tests in this file with ./Tools/Scripts/test-webkitpy lldb_webkit_unittest

# We cache the lldb debug session state so that we don't create it again for each call to a serial_test_ method.
# We store it in a global variable so that we can delete this cached state on exit(3).
# FIXME: Remove this once test-webkitpy supports class and module fixtures (i.e. setUpClass()/setUpModule()
# are called exactly once per class/module).
cached_debug_session = None


# FIXME: Remove this once test-webkitpy supports class and module fixtures (i.e. setUpClass()/setUpModule()
# are called exactly once per class/module).
@atexit.register
def destroy_cached_debug_session():
    if cached_debug_session:
        cached_debug_session.tearDown()


class LLDBDebugSession(object):
    @classmethod
    def setup(cls):
        LLDB_WEBKIT_TESTER_NAME = 'lldbWebKitTester'
        BREAK_FOR_TESTING_FUNCTION_NAME = 'breakForTestingSummaryProviders'

        cls.sbDebugger = lldb.SBDebugger.Create()
        cls.sbDebugger.SetAsync(False)

        host = SystemHost()
        config = Config(host.executive, host.filesystem)
        cls.lldbWebKitTesterExecutable = os.path.join(config.build_directory(config.default_configuration()), LLDB_WEBKIT_TESTER_NAME)

        cls.sbTarget = cls.sbDebugger.CreateTarget(str(cls.lldbWebKitTesterExecutable))
        assert cls.sbTarget
        cls.sbTarget.BreakpointCreateByName(BREAK_FOR_TESTING_FUNCTION_NAME, cls.sbTarget.GetExecutable().GetFilename())

        argv = None
        envp = None
        cls.sbProcess = cls.sbTarget.LaunchSimple(argv, envp, os.getcwd())
        assert cls.sbProcess
        assert cls.sbProcess.GetState() == lldb.eStateStopped

        cls.sbThread = cls.sbProcess.GetThreadAtIndex(0)
        assert cls.sbThread

        # Frame 0 is the function with name BREAK_FOR_TESTING_FUNCTION_NAME. We want the frame of the caller of
        # BREAK_FOR_TESTING_FUNCTION_NAME because it has all the interesting local variables we want to test.
        cls.sbFrame = cls.sbThread.GetFrameAtIndex(1)
        assert cls.sbFrame

    @classmethod
    def tearDown(cls):
        cls.sbProcess.Kill()


class TestSummaryProviders(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        global cached_debug_session
        if not cached_debug_session:
            cached_debug_session = LLDBDebugSession()
            cached_debug_session.setup()

    @property
    def _sbFrame(self):
        return cached_debug_session.sbFrame

    # The LLDB Python module does not work with Python multiprocessing and causes errors of the form:
    #     objc[76794]: +[__MDQuery initialize] may have been in progress in another thread when fork() was called.
    #                  We cannot safely call it or ignore it in the fork() child process. Crashing instead.
    # So, we run the following tests serially.

    # MARK: WTFStringImpl_SummaryProvider test cases

    def serial_test_WTFStringImpl_SummaryProvider_null_string(self):
        summary = lldb_webkit.WTFStringImpl_SummaryProvider(self._sbFrame.FindVariable('aNullStringImpl'), {})
        self.assertEqual(summary, "{ length = 0, is8bit = 1, contents = '' }")

    def serial_test_WTFStringImpl_SummaryProvider_empty_string(self):
        summary = lldb_webkit.WTFStringImpl_SummaryProvider(self._sbFrame.FindVariable('anEmptyStringImpl'), {})
        self.assertEqual(summary, "{ length = 0, is8bit = 1, contents = '' }")

    def serial_test_WTFStringImpl_SummaryProvider_8bit_string(self):
        summary = lldb_webkit.WTFStringImpl_SummaryProvider(self._sbFrame.FindVariable('an8BitStringImpl'), {})
        self.assertEqual(summary, "{ length = 8, is8bit = 1, contents = 'r\\xe9sum\\xe9' }")

    def serial_test_WTFStringImpl_SummaryProvider_16bit_string(self):
        summary = lldb_webkit.WTFStringImpl_SummaryProvider(self._sbFrame.FindVariable('a16BitStringImpl'), {})
        self.assertEqual(summary, u"{ length = 13, is8bit = 0, contents = '\\u1680Cappuccino\\u1680\\x00' }")

    # MARK: WTFString_SummaryProvider test cases

    def serial_test_WTFString_SummaryProvider_null_string(self):
        summary = lldb_webkit.WTFString_SummaryProvider(self._sbFrame.FindVariable('aNullString'), {})
        self.assertEqual(summary, "{ length = 0, contents = '' }")

    def serial_test_WTFString_SummaryProvider_empty_string(self):
        summary = lldb_webkit.WTFString_SummaryProvider(self._sbFrame.FindVariable('anEmptyString'), {})
        self.assertEqual(summary, "{ length = 0, contents = '' }")

    def serial_test_WTFString_SummaryProvider_8bit_string(self):
        summary = lldb_webkit.WTFString_SummaryProvider(self._sbFrame.FindVariable('an8BitString'), {})
        self.assertEqual(summary, "{ length = 8, contents = 'r\\xe9sum\\xe9' }")

    def serial_test_WTFString_SummaryProvider_16bit_string(self):
        summary = lldb_webkit.WTFString_SummaryProvider(self._sbFrame.FindVariable('a16BitString'), {})
        self.assertEqual(summary, u"{ length = 13, contents = '\\u1680Cappuccino\\u1680\\x00' }")

    # MARK: WTFVector_SummaryProvider test cases

    def serial_test_WTFVectorProvider_empty_vector(self):
        variable = self._sbFrame.FindVariable('anEmptyVector');
        self.assertIsNotNone(variable)
        summary = lldb_webkit.WTFVector_SummaryProvider(variable, {})
        self.assertEqual(summary, "{ size = 0, capacity = 0 }")

    def serial_test_WTFVectorProvider_vector_size_and_capacity(self):
        variable = self._sbFrame.FindVariable('aVectorWithOneItem');
        self.assertIsNotNone(variable)
        summary = lldb_webkit.WTFVector_SummaryProvider(variable, {})
        self.assertEqual(summary, "{ size = 1, capacity = 16 }")
