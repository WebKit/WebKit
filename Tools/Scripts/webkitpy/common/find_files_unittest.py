# Copyright (C) 2011 Google Inc. All rights reserved.
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

import itertools
import random
import sys
import unittest

import webkitpy.common.find_files as find_files

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem


class MockWinFileSystem(object):
    def join(self, *paths):
        return '\\'.join(paths)

    def normpath(self, path):
        return path.replace('/', '\\')


class TestWinNormalize(unittest.TestCase):
    def assert_filesystem_normalizes(self, filesystem):
        self.assertEqual(
            find_files._normalize(
                filesystem,
                "c:\\foo",
                ['fast/html', 'fast/canvas/*', 'compositing/foo.html'],
            ),
            [
                'c:\\foo\\fast\\html',
                'c:\\foo\\fast\\canvas\\*',
                'c:\\foo\\compositing\\foo.html',
            ],
        )

    def test_mocked_win(self):
        # This tests test_files.normalize, using portable behavior emulating
        # what we think Windows is supposed to do. This test will run on all
        # platforms.
        self.assert_filesystem_normalizes(MockWinFileSystem())

    def test_win(self):
        # This tests the actual windows platform, to ensure we get the same
        # results that we get in test_mocked_win().
        if not sys.platform.startswith('win'):
            return
        self.assert_filesystem_normalizes(FileSystem())


class TestFindFiles(unittest.TestCase):
    def test_directory_sort_key(self):
        filenames = [chr(o) for o in range(ord("a"), ord("z") + 1)]
        fs = MockFileSystem(
            files={c: "" for c in random.sample(filenames, len(filenames))}
        )
        self.assertEqual(
            list(find_files.find(fs, "", directory_sort_key=lambda x: x)),
            sorted(filenames),
        )

    def test_directory_sort_key_with_paths(self):
        filenames = ["/".join(i) for i in itertools.product("abcde", "12345")]
        fs = MockFileSystem(
            files={c: "" for c in random.sample(filenames, len(filenames))}
        )

        test_subset = random.sample("abcde", 3)
        self.assertEqual(
            list(
                find_files.find(
                    fs,
                    "",
                    paths=[i + "/*" for i in test_subset],
                    directory_sort_key=lambda x: x,
                )
            ),
            ["/".join(i) for i in itertools.product(test_subset, "12345")],
        )

    def test_query_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html?variant"])),
            ["a/b.html?variant"],
        )

    def test_fragment_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html#frag"])),
            ["a/b.html#frag"],
        )

    def test_fragment_before_query_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html#frag?query"])),
            ["a/b.html#frag?query"],
        )

    def test_query_before_fragment_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html?query#frag"])),
            ["a/b.html?query#frag"],
        )

    def test_slash_in_query_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html?a/b"])),
            ["a/b.html?a/b"],
        )

    def test_slash_in_fragment_variant(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(
            list(find_files.find(fs, "", paths=["a/b.html#frag/sub"])),
            ["a/b.html#frag/sub"],
        )

    def test_repeated_and_empty_variant_separators(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        for path in ("a/b.html?a?b", "a/b.html#a#b", "a/b.html?a#b#c", "a/b.html#a?b#c", "a/b.html?", "a/b.html#", "a/b.html?#"):
            with self.subTest(path):
                self.assertEqual(list(find_files.find(fs, "", paths=[path])), [path])

    def test_variant_of_a_missing_file_is_not_found(self):
        fs = MockFileSystem(files={"a/b.html": ""})
        self.assertEqual(list(find_files.find(fs, "", paths=["a/missing.html?variant"])), [])
        self.assertEqual(list(find_files.find(fs, "", paths=["a/missing.html#frag"])), [])
