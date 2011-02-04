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

import sys
import unittest

from webkitpy.layout_tests.port import test
import test_files

class TestFilesTest(unittest.TestCase):
    def test_find_no_paths_specified(self):
        port = test.TestPort()
        layout_tests_dir = port.layout_tests_dir()
        tests = test_files.find(port, [])
        self.assertNotEqual(len(tests), 0)

    def test_find_one_test(self):
        port = test.TestPort()
        tests = test_files.find(port, ['failures/expected/image.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = test.TestPort()
        tests = test_files.find(port, ['failures/expected/im*'])
        self.assertEqual(len(tests), 2)

    def test_find_with_skipped_directories(self):
        port = test.TestPort()
        tests = port.tests('userscripts')
        self.assertTrue('userscripts/resources/iframe.html' not in tests)

    def test_find_with_skipped_directories_2(self):
        port = test.TestPort()
        tests = test_files.find(port, ['userscripts/resources'])
        self.assertEqual(tests, set([]))

    def test_is_test_file(self):
        port = test.TestPort()
        fs = port._filesystem
        self.assertTrue(test_files._is_test_file(fs, '', 'foo.html'))
        self.assertTrue(test_files._is_test_file(fs, '', 'foo.shtml'))
        self.assertFalse(test_files._is_test_file(fs, '', 'foo.png'))
        self.assertFalse(test_files._is_test_file(fs, '', 'foo-expected.html'))
        self.assertFalse(test_files._is_test_file(fs, '', 'foo-expected-mismatch.html'))


class MockWinFileSystem(object):
    def join(self, *paths):
        return '\\'.join(paths)

    def normpath(self, path):
        return path.replace('/', '\\')


class TestWinNormalize(unittest.TestCase):
    def assert_filesystem_normalizes(self, filesystem):
        self.assertEquals(test_files.normalize(filesystem, "c:\\foo",
            ['fast/html', 'fast/canvas/*', 'compositing/foo.html']),
            ['c:\\foo\\fast\html', 'c:\\foo\\fast\canvas\*', 'c:\\foo\compositing\\foo.html'])

    def test_mocked_win(self):
        # This tests test_files.normalize, using portable behavior emulating
        # what we think Windows is supposed to do. This test will run on all
        # platforms.
        self.assert_filesystem_normalizes(MockWinFileSystem())

    def test_win(self):
        # This tests the actual windows platform, to ensure we get the same
        # results that we get in test_mocked_win().
        if sys.platform != 'win':
            return
        self.assert_filesystem_normalizes(FileSystem())


if __name__ == '__main__':
    unittest.main()
