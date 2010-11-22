# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import os
import unittest

import base
import test_files


class TestFilesTest(unittest.TestCase):
    def test_find_no_paths_specified(self):
        port = base.Port()
        layout_tests_dir = port.layout_tests_dir()
        port.layout_tests_dir = lambda: os.path.join(layout_tests_dir,
                                                     'fast', 'html')
        tests = test_files.find(port, [])
        self.assertNotEqual(tests, 0)

    def test_find_one_test(self):
        port = base.Port()
        # This is just a test picked at random but known to exist.
        tests = test_files.find(port, ['fast/html/keygen.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = base.Port()
        tests = test_files.find(port, ['fast/html/key*'])
        self.assertEqual(len(tests), 1)

    def test_find_with_skipped_directories(self):
        port = base.Port()
        tests = port.tests('userscripts')
        self.assertTrue('userscripts/resources/frame1.html' not in tests)

    def test_find_with_skipped_directories_2(self):
        port = base.Port()
        tests = test_files.find(port, ['userscripts/resources'])
        self.assertEqual(tests, set([]))

    def test_is_test_file(self):
        self.assertTrue(test_files._is_test_file('foo.html'))
        self.assertTrue(test_files._is_test_file('foo.shtml'))
        self.assertFalse(test_files._is_test_file('foo.png'))
        self.assertFalse(test_files._is_test_file('foo-expected.html'))
        self.assertFalse(test_files._is_test_file('foo-expected-mismatch.html'))


if __name__ == '__main__':
    unittest.main()
