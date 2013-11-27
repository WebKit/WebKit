# Copyright (C) 2012, 2013 Nokia Corporation and/or its subsidiary(-ies).
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest2 as unittest
import os
from copy import deepcopy

from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import port_testcase
from webkitpy.port.nix import NixPort
from webkitpy.tool.mocktool import MockOptions


class NixPortTest(port_testcase.PortTestCase):
    port_name = 'nix'
    port_maker = NixPort
    search_paths_cases = [
            {'search_paths': ['nix', 'wk2'], 'os_name':'linux'}]
    expectation_files_cases = [
        {'search_paths': ['', 'nix', 'wk2'], 'os_name':'linux'}]

    def _assert_search_path(self, search_paths, os_name):
        host = MockSystemHost(os_name=os_name)
        port = self.make_port(port_name=self.port_name, host=host, options=MockOptions(webkit_test_runner=True))
        absolute_search_paths = map(port._webkit_baseline_path, search_paths)

        unittest.TestCase.assertEqual(self, port.baseline_search_path(), absolute_search_paths)

    def _assert_expectations_files(self, search_paths, os_name):
        host = MockSystemHost(os_name=os_name)
        port = self.make_port(port_name=self.port_name, host=host, options=MockOptions(webkit_test_runner=True))
        unittest.TestCase.assertEqual(self, port.expectations_files(), search_paths)

    def test_baseline_search_path(self):
        for case in self.search_paths_cases:
            self._assert_search_path(**case)

    def test_expectations_files(self):
        for case in self.expectation_files_cases:
            expectations_case = deepcopy(case)
            expectations_case['search_paths'] = map(lambda path: '/mock-checkout/LayoutTests/TestExpectations'  if not path else '/mock-checkout/LayoutTests/platform/%s/TestExpectations' % (path), expectations_case['search_paths'])
            self._assert_expectations_files(**expectations_case)

    def test_default_timeout_ms(self):
        unittest.TestCase.assertEqual(self, self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 80000)
        unittest.TestCase.assertEqual(self, self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 80000)
