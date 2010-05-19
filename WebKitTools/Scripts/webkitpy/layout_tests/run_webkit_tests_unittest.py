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

import codecs
import os
import sys
import unittest

from webkitpy.common import array_stream
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

def logging_run(args):
    options, args = run_webkit_tests.parse_args(args)
    port_obj = port.get(options.platform, options)
    buildbot_output = array_stream.ArrayStream()
    regular_output = array_stream.ArrayStream()
    res = run_webkit_tests.run(port_obj, options, args,
                               buildbot_output=buildbot_output,
                               regular_output=regular_output)
    return (res, buildbot_output, regular_output)


class MainTest(unittest.TestCase):
    def test_fast(self):
        self.assertTrue(passing_run(['--platform', 'test']))
        self.assertTrue(passing_run(['--platform', 'test', '--run-singly']))
        self.assertTrue(passing_run(['--platform', 'test',
                                     'text/article-element.html']))
        self.assertTrue(passing_run(['--platform', 'test',
                                    '--child-processes', '1',
                                     '--print', 'unexpected']))

    def test_child_processes(self):
        (res, buildbot_output, regular_output) = logging_run(
             ['--platform', 'test', '--print', 'config', '--child-processes',
              '1'])
        self.assertTrue('Running one DumpRenderTree\n'
                        in regular_output.get())

        (res, buildbot_output, regular_output) = logging_run(
             ['--platform', 'test', '--print', 'config', '--child-processes',
              '2'])
        self.assertTrue('Running 2 DumpRenderTrees in parallel\n'
                        in regular_output.get())

    def test_last_results(self):
        passing_run(['--platform', 'test'])
        (res, buildbot_output, regular_output) = logging_run(
            ['--platform', 'test', '--print-last-failures'])
        self.assertEqual(regular_output.get(), ['\n\n'])
        self.assertEqual(buildbot_output.get(), [])


def _mocked_open(original_open, file_list):
    def _wrapper(name, mode, encoding):
        if name.find("-expected.") != -1 and mode == "w":
            # we don't want to actually write new baselines, so stub these out
            name.replace('\\', '/')
            file_list.append(name)
            return original_open(os.devnull, mode, encoding)
        return original_open(name, mode, encoding)
    return _wrapper


class RebaselineTest(unittest.TestCase):
    def assertBaselines(self, file_list, file):
        "assert that the file_list contains the baselines."""
        for ext in [".txt", ".png", ".checksum"]:
            baseline = file + "-expected" + ext
            self.assertTrue(any(f.find(baseline) != -1 for f in file_list))

    def test_reset_results(self):
        file_list = []
        original_open = codecs.open
        try:
            # Test that we update expectations in place. If the expectation
            # is mssing, update the expected generic location.
            file_list = []
            codecs.open = _mocked_open(original_open, file_list)
            passing_run(['--platform', 'test', '--pixel-tests',
                         '--reset-results',
                         'image/canvas-bg.html',
                         'image/canvas-zoom.html',
                         'misc/missing-expectation.html'])
            self.assertEqual(len(file_list), 9)
            self.assertBaselines(file_list,
                "data/image/canvas-zoom")
            self.assertBaselines(file_list,
                "data/platform/test/image/canvas-bg")
            self.assertBaselines(file_list,
                "data/misc/missing-expectation")
        finally:
            codecs.open = original_open

    def test_new_baseline(self):
        file_list = []
        original_open = codecs.open
        try:
            # Test that we update the platform expectations. If the expectation
            # is mssing, then create a new expectation in the platform dir.
            file_list = []
            codecs.open = _mocked_open(original_open, file_list)
            passing_run(['--platform', 'test', '--pixel-tests',
                         '--new-baseline',
                         'image/canvas-zoom.html',
                         'image/canvas-bg.html',
                         'misc/missing-expectation.html'])
            self.assertEqual(len(file_list), 9)
            self.assertBaselines(file_list,
                "data/platform/test/image/canvas-zoom")
            self.assertBaselines(file_list,
                "data/platform/test/image/canvas-bg")
            self.assertBaselines(file_list,
                "data/platform/test/misc/missing-expectation")
        finally:
            codecs.open = original_open

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
        # FIXME: it's hard to know which platforms are safe to test; the
        # chromium platforms require a chromium checkout, and the mac platform
        # requires fcntl, so it can't be tested on win32, etc. There is
        # probably a better way of handling this.
        if sys.platform != "mac":
            return
        self.assertTrue(passing_run(['--platform', 'dryrun',
                                     'fast/html']))
        self.assertTrue(passing_run(['--platform', 'dryrun-mac',
                                     'fast/html']))


if __name__ == '__main__':
    unittest.main()
