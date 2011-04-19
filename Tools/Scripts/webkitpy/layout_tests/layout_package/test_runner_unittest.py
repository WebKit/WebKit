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

"""Unit tests for TestRunner()."""

import unittest

from webkitpy.common.system import filesystem_mock
from webkitpy.thirdparty.mock import Mock

import test_runner


class TestRunnerWrapper(test_runner.TestRunner):
    def _get_test_input_for_file(self, test_file):
        return test_file


class TestRunnerTest(unittest.TestCase):
    def test_shard_tests(self):
        # Test that _shard_tests in test_runner.TestRunner really
        # put the http tests first in the queue.
        port = Mock()
        port._filesystem = filesystem_mock.MockFileSystem()
        runner = TestRunnerWrapper(port=port, options=Mock(),
            printer=Mock())

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
        single_thread_results = runner._shard_tests(test_list, False)
        multi_thread_results = runner._shard_tests(test_list, True)

        self.assertEqual("tests_to_http_lock", single_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(single_thread_results[0][1]))
        self.assertEqual("tests_to_http_lock", multi_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(multi_thread_results[0][1]))


if __name__ == '__main__':
    unittest.main()
