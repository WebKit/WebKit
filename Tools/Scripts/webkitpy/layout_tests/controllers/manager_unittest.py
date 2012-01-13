#!/usr/bin/python
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

import StringIO
import sys
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system import outputcapture
from webkitpy.thirdparty.mock import Mock
from webkitpy import layout_tests
from webkitpy.layout_tests.port import port_testcase

from webkitpy import layout_tests
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers.manager import interpret_test_failures,  Manager, natural_sort_key, test_key, TestRunInterruptedException, TestShard
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models.result_summary import ResultSummary
from webkitpy.layout_tests.views import printing
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.host_mock import MockHost


class ManagerWrapper(Manager):
    def _get_test_input_for_file(self, test_file):
        return test_file


class ShardingTests(unittest.TestCase):
    test_list = [
        "http/tests/websocket/tests/unicode.htm",
        "animations/keyframes.html",
        "http/tests/security/view-source-no-refresh.html",
        "http/tests/websocket/tests/websocket-protocol-ignored.html",
        "fast/css/display-none-inline-style-change-crash.html",
        "http/tests/xmlhttprequest/supported-xml-content-types.html",
        "dom/html/level2/html/HTMLAnchorElement03.html",
        "ietestcenter/Javascript/11.1.5_4-4-c-1.html",
        "dom/html/level2/html/HTMLAnchorElement06.html",
    ]

    def get_shards(self, num_workers, fully_parallel, test_list=None, max_locked_shards=None):
        test_list = test_list or self.test_list
        host = MockHost()
        port = host.port_factory.get(port_name='test')
        port._filesystem = MockFileSystem()
        options = MockOptions(max_locked_shards=max_locked_shards)
        self.manager = ManagerWrapper(port=port, options=options, printer=Mock())
        return self.manager._shard_tests(test_list, num_workers, fully_parallel)

    def test_shard_by_dir(self):
        locked, unlocked = self.get_shards(num_workers=2, fully_parallel=False)

        # Note that although there are tests in multiple dirs that need locks,
        # they are crammed into a single shard in order to reduce the # of
        # workers hitting the server at once.
        self.assertEquals(locked,
            [TestShard('locked_shard_1',
              ['http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/unicode.htm',
               'http/tests/websocket/tests/websocket-protocol-ignored.html',
               'http/tests/xmlhttprequest/supported-xml-content-types.html'])])
        self.assertEquals(unlocked,
            [TestShard('animations',
                       ['animations/keyframes.html']),
             TestShard('dom/html/level2/html',
                       ['dom/html/level2/html/HTMLAnchorElement03.html',
                        'dom/html/level2/html/HTMLAnchorElement06.html']),
             TestShard('fast/css',
                       ['fast/css/display-none-inline-style-change-crash.html']),
             TestShard('ietestcenter/Javascript',
                       ['ietestcenter/Javascript/11.1.5_4-4-c-1.html'])])

    def test_shard_every_file(self):
        locked, unlocked = self.get_shards(num_workers=2, fully_parallel=True)
        self.assertEquals(locked,
            [TestShard('.', ['http/tests/websocket/tests/unicode.htm']),
             TestShard('.', ['http/tests/security/view-source-no-refresh.html']),
             TestShard('.', ['http/tests/websocket/tests/websocket-protocol-ignored.html']),
             TestShard('.', ['http/tests/xmlhttprequest/supported-xml-content-types.html'])])
        self.assertEquals(unlocked,
            [TestShard('.', ['animations/keyframes.html']),
             TestShard('.', ['fast/css/display-none-inline-style-change-crash.html']),
             TestShard('.', ['dom/html/level2/html/HTMLAnchorElement03.html']),
             TestShard('.', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html']),
             TestShard('.', ['dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False)
        self.assertEquals(locked,
            [TestShard('locked_tests',
                       ['http/tests/websocket/tests/unicode.htm',
                        'http/tests/security/view-source-no-refresh.html',
                        'http/tests/websocket/tests/websocket-protocol-ignored.html',
                        'http/tests/xmlhttprequest/supported-xml-content-types.html'])])
        self.assertEquals(unlocked,
            [TestShard('unlocked_tests',
                       ['animations/keyframes.html',
                        'fast/css/display-none-inline-style-change-crash.html',
                        'dom/html/level2/html/HTMLAnchorElement03.html',
                        'ietestcenter/Javascript/11.1.5_4-4-c-1.html',
                        'dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two_has_no_locked_shards(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False,
             test_list=['animations/keyframe.html'])
        self.assertEquals(len(locked), 0)
        self.assertEquals(len(unlocked), 1)

    def test_shard_in_two_has_no_unlocked_shards(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False,
             test_list=['http/tests/webcoket/tests/unicode.htm'])
        self.assertEquals(len(locked), 1)
        self.assertEquals(len(unlocked), 0)

    def test_multiple_locked_shards(self):
        locked, unlocked = self.get_shards(num_workers=4, fully_parallel=False, max_locked_shards=2)
        self.assertEqual(locked,
            [TestShard('locked_shard_1',
                       ['http/tests/security/view-source-no-refresh.html',
                        'http/tests/websocket/tests/unicode.htm',
                        'http/tests/websocket/tests/websocket-protocol-ignored.html']),
             TestShard('locked_shard_2',
                        ['http/tests/xmlhttprequest/supported-xml-content-types.html'])])

        locked, unlocked = self.get_shards(num_workers=4, fully_parallel=False)
        self.assertEquals(locked,
            [TestShard('locked_shard_1',
                       ['http/tests/security/view-source-no-refresh.html',
                        'http/tests/websocket/tests/unicode.htm',
                        'http/tests/websocket/tests/websocket-protocol-ignored.html',
                        'http/tests/xmlhttprequest/supported-xml-content-types.html'])])


class ManagerTest(unittest.TestCase):
    def get_options(self):
        return MockOptions(pixel_tests=False, new_baseline=False, time_out_ms=6000, slow_time_out_ms=30000, worker_model='inline')

    def get_printer(self):
        class FakePrinter(object):
            def __init__(self):
                self.output = []

            def print_config(self, msg):
                self.output.append(msg)

        return FakePrinter()

    def test_fallback_path_in_config(self):
        options = self.get_options()
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard', options=options)
        printer = self.get_printer()
        manager = Manager(port, options, printer)
        manager.print_config()
        self.assertTrue('Baseline search path: test-mac-leopard -> test-mac-snowleopard -> generic' in printer.output)

    def test_http_locking(tester):
        class LockCheckingManager(Manager):
            def __init__(self, port, options, printer):
                super(LockCheckingManager, self).__init__(port, options, printer)
                self._finished_list_called = False

            def handle_finished_list(self, source, list_name, num_tests, elapsed_time):
                if not self._finished_list_called:
                    tester.assertEquals(list_name, 'locked_tests')
                    tester.assertTrue(self._remaining_locked_shards)
                    tester.assertTrue(self._has_http_lock)

                super(LockCheckingManager, self).handle_finished_list(source, list_name, num_tests, elapsed_time)

                if not self._finished_list_called:
                    tester.assertEquals(self._remaining_locked_shards, [])
                    tester.assertFalse(self._has_http_lock)
                    self._finished_list_called = True

        options, args = run_webkit_tests.parse_args(['--platform=test', '--print=nothing', 'http/tests/passes', 'passes'])
        host = MockHost()
        port = host.port_factory.get(port_name=options.platform, options=options)
        run_webkit_tests._set_up_derived_options(port, options)
        printer = printing.Printer(port, options, StringIO.StringIO(), StringIO.StringIO(), configure_logging=False)
        manager = LockCheckingManager(port, options, printer)
        manager.collect_tests(args)
        manager.parse_expectations()
        result_summary = manager.set_up_run()
        num_unexpected_results = manager.run(result_summary)
        manager.clean_up_run()
        printer.cleanup()
        tester.assertEquals(num_unexpected_results, 0)

    def test_interrupt_if_at_failure_limits(self):
        port = Mock()  # FIXME: This should be a tighter mock.
        port.TEST_PATH_SEPARATOR = '/'
        port._filesystem = MockFileSystem()
        manager = Manager(port=port, options=MockOptions(), printer=Mock())

        manager._options = MockOptions(exit_after_n_failures=None, exit_after_n_crashes_or_timeouts=None)
        result_summary = ResultSummary(expectations=Mock(), test_files=[])
        result_summary.unexpected_failures = 100
        result_summary.unexpected_crashes = 50
        result_summary.unexpected_timeouts = 50
        # No exception when the exit_after* options are None.
        manager._interrupt_if_at_failure_limits(result_summary)

        # No exception when we haven't hit the limit yet.
        manager._options.exit_after_n_failures = 101
        manager._options.exit_after_n_crashes_or_timeouts = 101
        manager._interrupt_if_at_failure_limits(result_summary)

        # Interrupt if we've exceeded either limit:
        manager._options.exit_after_n_crashes_or_timeouts = 10
        self.assertRaises(TestRunInterruptedException, manager._interrupt_if_at_failure_limits, result_summary)

        manager._options.exit_after_n_crashes_or_timeouts = None
        manager._options.exit_after_n_failures = 10
        exception = self.assertRaises(TestRunInterruptedException, manager._interrupt_if_at_failure_limits, result_summary)

    def test_needs_servers(self):
        def get_manager_with_tests(test_names):
            port = Mock()  # FIXME: Use a tighter mock.
            port.TEST_PATH_SEPARATOR = '/'
            manager = Manager(port, options=MockOptions(http=True), printer=Mock())
            manager._test_files = set(test_names)
            manager._test_files_list = test_names
            return manager

        manager = get_manager_with_tests(['fast/html'])
        self.assertFalse(manager.needs_servers())

        manager = get_manager_with_tests(['http/tests/misc'])
        self.assertTrue(manager.needs_servers())

    def integration_test_needs_servers(self):
        def get_manager_with_tests(test_names):
            host = MockHost()
            port = host.port_factory.get()
            manager = Manager(port, options=MockOptions(test_list=None, http=True), printer=Mock())
            manager.collect_tests(test_names)
            return manager

        manager = get_manager_with_tests(['fast/html'])
        self.assertFalse(manager.needs_servers())

        manager = get_manager_with_tests(['http/tests/mime'])
        self.assertTrue(manager.needs_servers())

        if sys.platform == 'win32':
            manager = get_manager_with_tests(['fast\\html'])
            self.assertFalse(manager.needs_servers())

            manager = get_manager_with_tests(['http\\tests\\mime'])
            self.assertTrue(manager.needs_servers())


class NaturalCompareTest(unittest.TestCase):
    def assert_cmp(self, x, y, result):
        self.assertEquals(cmp(natural_sort_key(x), natural_sort_key(y)), result)

    def test_natural_compare(self):
        self.assert_cmp('a', 'a', 0)
        self.assert_cmp('ab', 'a', 1)
        self.assert_cmp('a', 'ab', -1)
        self.assert_cmp('', '', 0)
        self.assert_cmp('', 'ab', -1)
        self.assert_cmp('1', '2', -1)
        self.assert_cmp('2', '1', 1)
        self.assert_cmp('1', '10', -1)
        self.assert_cmp('2', '10', -1)
        self.assert_cmp('foo_1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.html', 'foo_10.html', -1)
        self.assert_cmp('foo_2.html', 'foo_10.html', -1)
        self.assert_cmp('foo_23.html', 'foo_10.html', 1)
        self.assert_cmp('foo_23.html', 'foo_100.html', -1)


class KeyCompareTest(unittest.TestCase):
    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get('test')

    def assert_cmp(self, x, y, result):
        self.assertEquals(cmp(test_key(self.port, x), test_key(self.port, y)), result)

    def test_test_key(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)


class ResultSummaryTest(unittest.TestCase):

    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')

    def test_interpret_test_failures(self):
        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/reftest-expected.html'))])
        self.assertTrue('is_reftest' in test_dict)
        self.assertFalse('is_mismatch_reftest' in test_dict)

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatch(self.port.abspath_for_test('foo/common.html'))])
        self.assertTrue('is_reftest' in test_dict)
        self.assertFalse('is_mismatch_reftest' in test_dict)
        self.assertEqual(test_dict['ref_file'], 'foo/common.html')

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/reftest-expected-mismatch.html'))])
        self.assertFalse('is_reftest' in test_dict)
        self.assertTrue(test_dict['is_mismatch_reftest'])

        test_dict = interpret_test_failures(self.port, 'foo/reftest.html',
            [test_failures.FailureReftestMismatchDidNotOccur(self.port.abspath_for_test('foo/common.html'))])
        self.assertFalse('is_reftest' in test_dict)
        self.assertTrue(test_dict['is_mismatch_reftest'])
        self.assertEqual(test_dict['ref_file'], 'foo/common.html')


if __name__ == '__main__':
    port_testcase.main()
