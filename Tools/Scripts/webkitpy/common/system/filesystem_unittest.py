# vim: set fileencoding=utf-8 :
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

# NOTE: The fileencoding comment on the first line of the file is
# important; without it, Python will choke while trying to parse the file,
# since it includes non-ASCII characters.

from __future__ import with_statement

import os
import stat
import sys
import tempfile
import unittest

from filesystem import FileSystem


class FileSystemTest(unittest.TestCase):
    def setUp(self):
        self._this_dir = os.path.dirname(os.path.abspath(__file__))
        self._missing_file = os.path.join(self._this_dir, 'missing_file.py')
        self._this_file = os.path.join(self._this_dir, 'filesystem_unittest.py')

    def test_chdir(self):
        fs = FileSystem()
        cwd = fs.getcwd()
        newdir = '/'
        if sys.platform == 'win32':
            newdir = 'c:\\'
        fs.chdir(newdir)
        self.assertEquals(fs.getcwd(), newdir)
        fs.chdir(cwd)

    def test_chdir__notexists(self):
        fs = FileSystem()
        newdir = '/dirdoesnotexist'
        if sys.platform == 'win32':
            newdir = 'c:\\dirdoesnotexist'
        self.assertRaises(OSError, fs.chdir, newdir)

    def test_exists__true(self):
        fs = FileSystem()
        self.assertTrue(fs.exists(self._this_file))

    def test_exists__false(self):
        fs = FileSystem()
        self.assertFalse(fs.exists(self._missing_file))

    def test_getcwd(self):
        fs = FileSystem()
        self.assertTrue(fs.exists(fs.getcwd()))

    def test_isdir__true(self):
        fs = FileSystem()
        self.assertTrue(fs.isdir(self._this_dir))

    def test_isdir__false(self):
        fs = FileSystem()
        self.assertFalse(fs.isdir(self._this_file))

    def test_join(self):
        fs = FileSystem()
        self.assertEqual(fs.join('foo', 'bar'),
                         os.path.join('foo', 'bar'))

    def test_listdir(self):
        fs = FileSystem()
        with fs.mkdtemp(prefix='filesystem_unittest_') as d:
            self.assertEqual(fs.listdir(d), [])
            new_file = os.path.join(d, 'foo')
            fs.write_text_file(new_file, u'foo')
            self.assertEqual(fs.listdir(d), ['foo'])
            os.remove(new_file)

    def test_maybe_make_directory__success(self):
        fs = FileSystem()

        with fs.mkdtemp(prefix='filesystem_unittest_') as base_path:
            sub_path = os.path.join(base_path, "newdir")
            self.assertFalse(os.path.exists(sub_path))
            self.assertFalse(fs.isdir(sub_path))

            fs.maybe_make_directory(sub_path)
            self.assertTrue(os.path.exists(sub_path))
            self.assertTrue(fs.isdir(sub_path))

            # Make sure we can re-create it.
            fs.maybe_make_directory(sub_path)
            self.assertTrue(os.path.exists(sub_path))
            self.assertTrue(fs.isdir(sub_path))

            # Clean up.
            os.rmdir(sub_path)

        self.assertFalse(os.path.exists(base_path))
        self.assertFalse(fs.isdir(base_path))

    def test_maybe_make_directory__failure(self):
        # FIXME: os.chmod() doesn't work on Windows to set directories
        # as readonly, so we skip this test for now.
        if sys.platform in ('win32', 'cygwin'):
            return

        fs = FileSystem()
        with fs.mkdtemp(prefix='filesystem_unittest_') as d:
            # Remove write permissions on the parent directory.
            os.chmod(d, stat.S_IRUSR)

            # Now try to create a sub directory - should fail.
            sub_dir = fs.join(d, 'subdir')
            self.assertRaises(OSError, fs.maybe_make_directory, sub_dir)

            # Clean up in case the test failed and we did create the
            # directory.
            if os.path.exists(sub_dir):
                os.rmdir(sub_dir)

    def test_read_and_write_file(self):
        fs = FileSystem()
        text_path = None
        binary_path = None

        unicode_text_string = u'Ūnĭcōde̽'
        hex_equivalent = '\xC5\xAA\x6E\xC4\xAD\x63\xC5\x8D\x64\x65\xCC\xBD'
        try:
            text_path = tempfile.mktemp(prefix='tree_unittest_')
            binary_path = tempfile.mktemp(prefix='tree_unittest_')
            fs.write_text_file(text_path, unicode_text_string)
            contents = fs.read_binary_file(text_path)
            self.assertEqual(contents, hex_equivalent)

            fs.write_text_file(binary_path, hex_equivalent)
            text_contents = fs.read_text_file(binary_path)
            self.assertEqual(text_contents, unicode_text_string)
        except:
            if text_path:
                os.remove(text_path)
            if binary_path:
                os.remove(binary_path)

    def test_read_binary_file__missing(self):
        fs = FileSystem()
        self.assertRaises(IOError, fs.read_binary_file, self._missing_file)

    def test_read_text_file__missing(self):
        fs = FileSystem()
        self.assertRaises(IOError, fs.read_text_file, self._missing_file)

    def test_remove_file_with_retry(self):
        FileSystemTest._remove_failures = 2

        def remove_with_exception(filename):
            FileSystemTest._remove_failures -= 1
            if FileSystemTest._remove_failures >= 0:
                try:
                    raise WindowsError
                except NameError:
                    raise FileSystem._WindowsError

        fs = FileSystem()
        self.assertTrue(fs.remove('filename', remove_with_exception))
        self.assertEquals(-1, FileSystemTest._remove_failures)

    def test_sep(self):
        fs = FileSystem()

        self.assertEquals(fs.sep, os.sep)
        self.assertEquals(fs.join("foo", "bar"),
                          os.path.join("foo", "bar"))

    def test_sep__is_readonly(self):
        def assign_sep():
            fs.sep = ' '
        fs = FileSystem()
        self.assertRaises(AttributeError, assign_sep)


if __name__ == '__main__':
    unittest.main()
