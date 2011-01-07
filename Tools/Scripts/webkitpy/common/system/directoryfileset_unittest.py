# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import with_statement

import unittest

from webkitpy.common.system.directoryfileset import DirectoryFileSet
from webkitpy.common.system.filesystem_mock import MockFileSystem


class DirectoryFileSetTest(unittest.TestCase):
    def setUp(self):
        files = {}
        files['/test/some-file'] = 'contents'
        files['/test/some-other-file'] = 'other contents'
        files['/test/b/c'] = 'c'
        self._filesystem = MockFileSystem(files)
        self._fileset = DirectoryFileSet('/test', self._filesystem)

    def test_files_in_namelist(self):
        self.assertTrue('some-file' in self._fileset.namelist())
        self.assertTrue('some-other-file' in self._fileset.namelist())
        self.assertTrue('b/c' in self._fileset.namelist())

    def test_read(self):
        self.assertEquals('contents', self._fileset.read('some-file'))

    def test_open(self):
        file = self._fileset.open('some-file')
        self.assertEquals('some-file', file.name())
        self.assertEquals('contents', file.contents())

    def test_extract(self):
        self._fileset.extract('some-file', '/test-directory')
        contents = self._filesystem.read_text_file('/test-directory/some-file')
        self.assertEquals('contents', contents)

    def test_extract_deep_file(self):
        self._fileset.extract('b/c', '/test-directory')
        self.assertTrue(self._filesystem.exists('/test-directory/b/c'))

    def test_delete(self):
        self.assertTrue(self._filesystem.exists('/test/some-file'))
        self._fileset.delete('some-file')
        self.assertFalse(self._filesystem.exists('/test/some-file'))


if __name__ == '__main__':
    unittest.main()
