#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Unit tests for run_webkit_tests."""

import os
import sys
import unittest

from webkitpy.layout_tests import port
from webkitpy.layout_tests import run_webkit_tests

from webkitpy.thirdparty.mock import Mock


def passing_run(args, port_obj=None, logging_included=False):
    if not logging_included:
        args.extend(['--print', 'nothing'])
    options, args = run_webkit_tests.parse_args(args)
    if port_obj is None:
        port_obj = port.get(options.platform, options)
    res = run_webkit_tests.run(port_obj, options, args)
    return res == 0


class MainTest(unittest.TestCase):
    def test_fast(self):
        self.assertTrue(passing_run(['--platform', 'test',
                                     'fast/html']))
        self.assertTrue(passing_run(['--platform', 'test',
                                     '--run-singly',
                                     'fast/html']))
        self.assertTrue(passing_run(['--platform', 'test',
                                     'fast/html/article-element.html']))
        self.assertTrue(passing_run(['--platform', 'test',
                                    '--child-processes', '1',
                                     '--print', 'unexpected',
                                     'fast/html']))


class TestRunnerTest(unittest.TestCase):
    def test_results_html(self):
        mock_port = Mock()
        mock_port.relative_test_filename = lambda name: name
        mock_port.filename_to_uri = lambda name: name

        runner = run_webkit_tests.TestRunner(port=mock_port, options=Mock(), printer=Mock())
        expected_html = u"""<html>
  <head>
    <title>Layout Test Results (time)</title>
  </head>
  <body>
    <h2>Title (time)</h2>
        <p><a href='test_path'>test_path</a><br />
</p>
</body></html>
"""
        html = runner._results_html(["test_path"], {}, "Title", override_time="time")
        self.assertEqual(html, expected_html)


class DryrunTest(unittest.TestCase):
    def test_basics(self):
        self.assertTrue(passing_run(['--platform', 'dryrun',
                                     'fast/html']))
        #self.assertTrue(passing_run(['--platform', 'dryrun-mac',
        #                             'fast/html']))
        #self.assertTrue(passing_run(['--platform', 'dryrun-chromium-mac',
        #                             'fast/html']))
        #self.assertTrue(passing_run(['--platform', 'dryrun-chromium-win',
        #                             'fast/html']))
        #self.assertTrue(passing_run(['--platform', 'dryrun-chromium-linux',
        #                             'fast/html']))

if __name__ == '__main__':
    unittest.main()
