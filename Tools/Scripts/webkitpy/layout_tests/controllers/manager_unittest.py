# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for manager.py."""

from collections import OrderedDict
from io import StringIO
import sys
import time
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers.manager import Manager
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models.test import Test
from webkitpy.layout_tests.models.test_expectations import *
from webkitpy.layout_tests.models.test_run_results import TestRunResults
from webkitpy.port.test import TestPort
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockOptions
from webkitpy.xcode.device_type import DeviceType


class ManagerTest(unittest.TestCase):
    def _get_manager(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        manager = Manager(port, options=MockOptions(test_list=None, http=True, verbose=False), printer=Mock())
        return manager

    def test_look_for_new_crash_logs(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        tests = ['failures/expected/crash.html']
        expectations = test_expectations.TestExpectations(port, tests)
        expectations.parse_all_expectations()
        run_results = TestRunResults(expectations, len(tests))
        manager = self._get_manager()
        manager._look_for_new_crash_logs(run_results, time.time())

    def test_print_expectations_for_subset(self):
        def get_test_names():
            return ['failures/expected/text.html',
                    'failures/expected/image_checksum.html',
                    'failures/expected/crash.html',
                    'failures/expected/leak.html',
                    'failures/expected/flaky-leak.html',
                    'failures/expected/missing_text.html',
                    'failures/expected/image.html',
                    'failures/expected/reftest.html',
                    'failures/expected/leaky-reftest.html',
                    'passes/text.html']

        def get_tests():
            return [Test(test) for test in get_test_names()]

        def get_expectations():
            return """
Bug(test) failures/expected/text.html [ Failure ]
Bug(test) failures/expected/crash.html [ WontFix ]
Bug(test) failures/expected/leak.html [ Leak ]
Bug(test) failures/expected/flaky-leak.html [ Failure Leak ]
Bug(test) failures/expected/missing_image.html [ Rebaseline Missing ]
Bug(test) failures/expected/image_checksum.html [ WontFix ]
Bug(test) failures/expected/image.html [ WontFix Mac ]
Bug(test) failures/expected/reftest.html [ ImageOnlyFailure ]
Bug(test) failures/expected/leaky-reftest.html [ ImageOnlyFailure Leak ]
"""

        def get_printed_expectations():
            # There are trailing whitespaces in this string, they are as intended.
            return """
Tests to run for DEVICE_TYPE (10)
failures/expected/crash.html           ['SKIP'] expectations:3 Bug(test) failures/expected/crash.html [ WontFix ]
failures/expected/flaky-leak.html      ['FAIL', 'LEAK'] expectations:5 Bug(test) failures/expected/flaky-leak.html [ Failure Leak ]
failures/expected/image.html           ['PASS']  
failures/expected/image_checksum.html  ['SKIP'] expectations:7 Bug(test) failures/expected/image_checksum.html [ WontFix ]
failures/expected/leak.html            ['LEAK'] expectations:4 Bug(test) failures/expected/leak.html [ Leak ]
failures/expected/leaky-reftest.html   ['IMAGE', 'LEAK'] expectations:10 Bug(test) failures/expected/leaky-reftest.html [ ImageOnlyFailure Leak ]
failures/expected/missing_text.html    ['PASS']  
failures/expected/reftest.html         ['IMAGE'] expectations:9 Bug(test) failures/expected/reftest.html [ ImageOnlyFailure ]
failures/expected/text.html            ['FAIL'] expectations:2 Bug(test) failures/expected/text.html [ Failure ]
passes/text.html                       ['PASS']  
"""

        def parse_exp(test_names, expectations):
            expectations_dict = OrderedDict()
            expectations_dict['expectations'] = expectations
            host = MockHost()
            port = host.port_factory.get('test-win-xp', None)
            port.expectations_dict = lambda **kwargs: expectations_dict
            exp = TestExpectations(port, test_names)
            exp.parse_all_expectations()
            return exp

        manager = self._get_manager()
        device_type = "DEVICE_TYPE"
        manager._expectations[device_type] = parse_exp(get_test_names(), get_expectations())
        test_col_width = max(len(test) for test in get_test_names()) + 1

        initial_stdout = sys.stdout
        stringIO = StringIO()
        sys.stdout = stringIO
        try:
            manager._print_expectations_for_subset(device_type, test_col_width, get_tests())
        finally:
            sys.stdout = initial_stdout

        out = stringIO.getvalue()
        self.maxDiff = None

        # Note: Buildbots compare `--print-expectations` outputs between revisions,
        # so if the output is not *exactly* as expected including whitespaces, this
        # could lead to unwanted effects, like blocking builds for a long time.
        self.assertEqual(get_printed_expectations(), out)
