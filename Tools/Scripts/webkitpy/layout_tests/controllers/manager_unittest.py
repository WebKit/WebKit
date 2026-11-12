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
import json
import sys
import time
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers.manager import Manager
from webkitpy.layout_tests.models import test_expectations, test_failures, test_results
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
        manager = Manager(port, options=MockOptions(test_list=None, http=True, verbose=False, driver_names=list(port.DEFAULT_SUPPORTED_DRIVERS)), printer=Mock())
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
        driver_name = manager._driver_names[0]
        manager._current_driver_name = driver_name
        manager._expectations[(driver_name, device_type)] = parse_exp(get_test_names(), get_expectations())
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

    def test_print_expectations_no_tests_found(self):
        # Passing a non-existent path should not raise ValueError from max() on an
        # empty sequence; it should return 0 cleanly.
        manager = self._get_manager()
        manager._options.update(repeat_each=1, iterations=1)
        device_type_list = manager._port.supported_device_types()
        manager._create_port_for_driver = Mock(return_value=manager._port)
        manager._collect_tests = Mock(return_value=({dt: [] for dt in device_type_list}, set()))
        exit_code = manager.print_expectations(['this/file/does/not/exist.html'])
        self.assertEqual(exit_code, 0)

    def test_collect_tests_skips_wpt_print_reftests_in_dump_render_tree(self):
        print_reftest = Test('passes/image.html', is_wpt_test=True, is_wpt_print_reftest=True)
        other_test = Test('passes/text.html', is_wpt_test=True)

        def collect_tests(driver_name):
            host = MockHost()
            port = host.port_factory.get('test-mac-leopard')
            options = MockOptions(
                test_list=None, http=True, verbose=False, driver_names=[driver_name], force=False,
                skip_failing_tests=False, skip_flaky_tests=False, skipped='default', order='natural',
                run_chunk=None, run_part=None,
            )
            manager = Manager(port, options=options, printer=Mock())
            manager._current_driver_name = driver_name
            manager._finder = Mock()
            manager._finder.find_tests = Mock(return_value=([], [print_reftest, other_test]))
            manager._finder.get_test_list_expectations = Mock(return_value=[])
            tests_to_run_by_device, tests_to_skip = manager._collect_tests([], [None], driver_name)
            return tests_to_run_by_device[None], tests_to_skip

        self.assertEqual(collect_tests('DumpRenderTree'), ([other_test], {print_reftest}))
        self.assertEqual(collect_tests('WebKitTestRunner'), ([print_reftest, other_test], set()))

    DIFF = (
        '--- /Volumes/Data/worker/Apple-Tahoe-Release-WK2-Tests/build/layout-test-results/fast/css/text-expected.txt\n'
        '+++ /Volumes/Data/worker/Apple-Tahoe-Release-WK2-Tests/build/layout-test-results/fast/css/text-actual.txt\n'
        '@@ -1,2 +1,2 @@\n'
        ' PASS successfullyParsed is true\n'
        '-PASS The initiator type for \'@font-face\' resources must be \'css\'\n'
        '+FAIL The initiator type for \'@font-face\' resources must be \'css\' promise_test: Unhandled rejection\n'
    )
    STDERR = 'ASSERTION FAILED: m_absCaretBounds == m_selection.visibleStart().absoluteCaretBounds()\nSource/WebCore/editing/FrameSelection.cpp(1948) : bool WebCore::FrameSelection::recomputeCaretRect()\n'
    CRASH_REPORT = '{"app_name":"WebKitTestRunner"}\n' + json.dumps({
        'procName': 'WebKitTestRunner',
        'exception': {'type': 'EXC_CRASH', 'signal': 'SIGABRT'},
        'faultingThread': 0,
        'threads': [{'triggered': True, 'queue': 'com.apple.main-thread', 'frames': [{'imageOffset': 38480, 'symbol': '__pthread_kill', 'imageIndex': 0}]}],
        'usedImages': [{'name': 'libsystem_kernel.dylib'}],
    })

    def _upload_logs(self, files, failures_by_test, expected_tests=()):
        manager = self._get_manager()
        manager._results_directory = '/test.checkout/layout-test-results'
        for name, contents in files.items():
            manager._filesystem.write_binary_file(manager._filesystem.join(manager._results_directory, name), contents)
        expectations = test_expectations.TestExpectations(manager._port, list(failures_by_test))
        expectations.parse_all_expectations()
        run_results = TestRunResults(expectations, len(failures_by_test))
        for test_name, failures in failures_by_test.items():
            run_results.add(test_results.TestResult(test_name, failures=failures), expected=test_name in expected_tests)
        return manager._logs_to_upload(run_results)

    def test_upload_log_for_text_failure_is_its_diff(self):
        logs = self._upload_logs({'fast/css/text-diff.txt': self.DIFF}, {'fast/css/text.html': [test_failures.FailureTextMismatch()]})
        self.assertEqual(logs, {'fast/css/text.html': self.DIFF.split('\n', 2)[2]})

    def test_upload_log_for_diff_without_file_names(self):
        logs = self._upload_logs({'webaudio/gain-diff.txt': 'Audio sample mismatch at 1024\n'}, {'webaudio/gain.html': [test_failures.FailureAudioMismatch()]})
        self.assertEqual(logs, {'webaudio/gain.html': 'Audio sample mismatch at 1024\n'})

    def test_upload_log_for_timeout_without_diff_is_its_stderr(self):
        logs = self._upload_logs({'fast/css/timeout-stderr.txt': 'Timed out waiting for rendering of window.\n'}, {'fast/css/timeout.html': [test_failures.FailureTimeout()]})
        self.assertEqual(logs, {'fast/css/timeout.html': 'Timed out waiting for rendering of window.\n'})

    def test_upload_log_for_crash_is_its_stderr_and_crashed_thread(self):
        logs = self._upload_logs({
            'editing/crash-stderr.txt': self.STDERR,
            'editing/crash-crash-log.txt': self.CRASH_REPORT,
        }, {'editing/crash.html': [test_failures.FailureCrash()]})
        self.assertEqual(logs, {'editing/crash.html': (
            self.STDERR + '\n'
            'WebKitTestRunner: EXC_CRASH (SIGABRT)\n'
            'Thread 0 crashed: com.apple.main-thread\n'
            '0   libsystem_kernel.dylib  __pthread_kill'
        )})

    def test_upload_log_for_crash_without_crash_report_is_its_stderr(self):
        logs = self._upload_logs({
            'editing/crash-stderr.txt': self.STDERR,
            'editing/crash-crash-log.txt': 'No crash log found for WebKitTestRunner:4496.\n\nstdout:\n\nstderr:\n' + self.STDERR,
        }, {'editing/crash.html': [test_failures.FailureCrash()]})
        self.assertEqual(logs, {'editing/crash.html': self.STDERR})

    def test_upload_log_for_crash_without_stderr_is_its_crashed_thread(self):
        self.assertEqual(self._upload_logs({'editing/crash-crash-log.txt': self.CRASH_REPORT}, {'editing/crash.html': [test_failures.FailureCrash()]}), {'editing/crash.html': (
            'WebKitTestRunner: EXC_CRASH (SIGABRT)\n'
            'Thread 0 crashed: com.apple.main-thread\n'
            '0   libsystem_kernel.dylib  __pthread_kill'
        )})

    def test_no_upload_log_for_failure_without_output(self):
        self.assertEqual(self._upload_logs({}, {'fast/images/image.html': [test_failures.FailureImageHashMismatch()]}), {})

    def test_no_upload_log_for_unexpected_pass(self):
        self.assertEqual(self._upload_logs({'fast/css/pass-stderr.txt': self.STDERR}, {'fast/css/pass.html': []}), {})

    def test_no_upload_log_for_expected_failure(self):
        logs = self._upload_logs({'fast/css/text-diff.txt': self.DIFF}, {'fast/css/text.html': [test_failures.FailureTextMismatch()]}, expected_tests={'fast/css/text.html'})
        self.assertEqual(logs, {})

    def test_upload_log_for_every_failure(self):
        tests = [f'fast/css/text-{index:02}' for index in range(40)]
        logs = self._upload_logs({f'{test}-stderr.txt': 'x' * 14610 for test in tests}, {f'{test}.html': [test_failures.FailureTimeout()] for test in tests})
        self.assertEqual(sorted(logs), [f'{test}.html' for test in tests])
        self.assertEqual({len(log) for log in logs.values()}, {8191})
