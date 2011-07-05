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

"""Unit tests for manager.Manager()."""

import unittest

from webkitpy.common.system import filesystem_mock
from webkitpy.thirdparty.mock import Mock

from webkitpy.layout_tests.layout_package.manager import Manager, natural_sort_key, path_key, TestRunInterruptedException
from webkitpy.layout_tests.layout_package.result_summary import ResultSummary
from webkitpy.tool.mocktool import MockOptions


class ManagerWrapper(Manager):
    def _get_test_input_for_file(self, test_file):
        return test_file


class ManagerTest(unittest.TestCase):
    def test_shard_tests(self):
        # Test that _shard_tests in test_runner.TestRunner really
        # put the http tests first in the queue.
        port = Mock()
        port._filesystem = filesystem_mock.MockFileSystem()
        manager = ManagerWrapper(port=port, options=Mock(), printer=Mock())

        test_list = [
          "LayoutTests/websocket/tests/unicode.htm",
          "LayoutTests/animations/keyframes.html",
          "LayoutTests/http/tests/security/view-source-no-refresh.html",
          "LayoutTests/websocket/tests/websocket-protocol-ignored.html",
          "LayoutTests/fast/css/display-none-inline-style-change-crash.html",
          "LayoutTests/http/tests/xmlhttprequest/supported-xml-content-types.html",
          "LayoutTests/dom/html/level2/html/HTMLAnchorElement03.html",
          "LayoutTests/ietestcenter/Javascript/11.1.5_4-4-c-1.html",
          "LayoutTests/dom/html/level2/html/HTMLAnchorElement06.html",
        ]

        expected_tests_to_http_lock = set([
          'LayoutTests/websocket/tests/unicode.htm',
          'LayoutTests/http/tests/security/view-source-no-refresh.html',
          'LayoutTests/websocket/tests/websocket-protocol-ignored.html',
          'LayoutTests/http/tests/xmlhttprequest/supported-xml-content-types.html',
        ])

        # FIXME: Ideally the HTTP tests don't have to all be in one shard.
        single_thread_results = manager._shard_tests(test_list, False)
        multi_thread_results = manager._shard_tests(test_list, True)

        self.assertEqual("tests_to_http_lock", single_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(single_thread_results[0][1]))
        self.assertEqual("tests_to_http_lock", multi_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(multi_thread_results[0][1]))

    def test_interrupt_if_at_failure_limits(self):
        port = Mock()
        port._filesystem = filesystem_mock.MockFileSystem()
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


class PathCompareTest(unittest.TestCase):
    def setUp(self):
        self.filesystem = filesystem_mock.MockFileSystem()

    def path_key(self, k):
        return path_key(self.filesystem, k)

    def assert_cmp(self, x, y, result):
        self.assertEquals(cmp(self.path_key(x), self.path_key(y)), result)

    def test_path_compare(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)


if __name__ == '__main__':
    unittest.main()
