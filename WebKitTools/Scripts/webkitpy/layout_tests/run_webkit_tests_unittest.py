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

import webkitpy.layout_tests.run_webkit_tests as run_webkit_tests


def passing_run(args):
    options, args = run_webkit_tests.parse_args(args)
    res = run_webkit_tests.main(options, args, False)
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
                                     '--log', 'unexpected',
                                     'fast/html']))


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
