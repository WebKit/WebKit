# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import ntpath
import posixpath
import unittest
from collections import OrderedDict

from pyfakefs.fake_filesystem import OSType

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.fakefs_testcase import (
    PyFakefsLinuxTestCaseMixin,
    PyFakefsMacOSTestCaseMixin,
    PyFakefsWindowsTestCaseMixin,
)
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.layout_tests.controllers.layout_test_finder import (
    LayoutTestFinder,
)
from webkitpy.layout_tests.models.test import Reference, test_name_and_variant
from webkitpy.port.test import (
    TestPort,
    add_unit_tests_to_mock_filesystem,
)


class LayoutTestFinderTestsBase(object):
    def __init__(self, *args, **kwargs):
        super(LayoutTestFinderTestsBase, self).__init__(*args, **kwargs)
        self.port = None
        self.finder = None

    def setUp(self):
        self.setUpPyfakefs()
        host = MockHost(create_stub_repository_files=True, filesystem=FileSystem())
        add_unit_tests_to_mock_filesystem(host.filesystem)
        self.port = TestPort(host)
        self.finder = LayoutTestFinder(
            self.port.host.filesystem,
            self.port.layout_tests_dir(),
            self.port.baseline_search_path(),
            test_routes=self.port.test_routes(),
        )

    def tearDown(self):
        self.port = None
        self.finder = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self.port.host.filesystem.sep, expected_sep)

    def test_test_routes_prefixes_use_forward_slashes(self):
        # test_file_dir is compared as a string against "/"-separated test
        # names, so it must not use the host separator.
        for route in self.port.test_routes():
            self.assertNotIn("\\", route.test_file_dir)

    def test_split_glob(self):
        v = list(self.finder._split_glob("a"))
        self.assertEqual([("", "a", "")], v)

        v = list(self.finder._split_glob("a/"))
        self.assertEqual([("a", "", "")], v)

        v = list(self.finder._split_glob("a/b"))
        self.assertEqual([("a", "b", "")], v)

        v = list(self.finder._split_glob("a/b/"))
        self.assertEqual([("a/b", "", "")], v)

        v = list(self.finder._split_glob("a/b/c"))
        self.assertEqual([("a/b", "c", "")], v)

        v = list(self.finder._split_glob("a/b#c"))
        self.assertEqual([("a", "b", "#c")], v)

        v = list(self.finder._split_glob("a/b?c"))
        self.assertEqual([("a", "b", "?c"), ("a", "b?c", "")], v)

        v = list(self.finder._split_glob("a/b?c?d"))
        self.assertEqual(
            [("a", "b", "?c?d"), ("a", "b?c", "?d"), ("a", "b?c?d", "")], v
        )

        v = list(self.finder._split_glob("a/b?c#d"))
        self.assertEqual([("a", "b", "?c#d"), ("a", "b?c", "#d")], v)

        v = list(self.finder._split_glob("a/?b"))
        self.assertEqual([("a", "?b", "")], v)

        v = list(self.finder._split_glob("a/?b?c"))
        self.assertEqual([("a", "?b", "?c"), ("a", "?b?c", "")], v)

        v = list(self.finder._split_glob("a?b"))
        self.assertEqual([("", "a", "?b"), ("", "a?b", "")], v)

        v = list(self.finder._split_glob("a??b"))
        self.assertEqual([("", "a", "??b"), ("", "a?", "?b"), ("", "a??b", "")], v)

        v = list(self.finder._split_glob("a?b?c"))
        self.assertEqual([("", "a", "?b?c"), ("", "a?b", "?c"), ("", "a?b?c", "")], v)

        v = list(self.finder._split_glob("a/#b"))
        self.assertEqual([], v)

        v = list(self.finder._split_glob("a/b?c/d"))
        self.assertEqual([("a", "b", "?c/d"), ("a/b?c", "d", "")], v)

        v = list(self.finder._split_glob("a/b#c/d"))
        self.assertEqual([("a", "b", "#c/d")], v)

    def test_split_glob_agrees_with_test_name_and_variant(self):
        # Every way of writing a variant must be split into the same file and
        # variant by _split_glob as by test_name_and_variant, since one is
        # used to find tests on disk and the other to name them.
        for name in (
            "a/b.html?x",
            "a/b.html#x",
            "a/b.html?x#y",
            "a/b.html#x?y",
            "a/b.html?x?y",
            "a/b.html?",
            "a/b.html#",
            "a/b.html?x/y",
            "a/b.html#x/y",
            "a/b.html?x#y/z",
            "a/b.html?a%20b",
            "a.b/c.d/e.html?x",
            "a/b.html#x#y",
            "a/b.html?x#y#z",
            "a/b#c/d#e",
        ):
            with self.subTest(name):
                file_path, variant = test_name_and_variant(name)
                dirname, basename = posixpath.split(file_path)
                self.assertIn((dirname, basename, variant), list(self.finder._split_glob(name)))

    def test_split_glob_agrees_with_test_name_and_variant_backslashes(self):
        if self.fs_os != OSType.WINDOWS:
            self.skipTest("a backslash is only a separator on Windows")
        for name in (
            "a\\b.html#x#y",
            "a\\b#c\\d#e",
        ):
            with self.subTest(name):
                file_path, variant = test_name_and_variant(name)
                dirname, basename = ntpath.split(file_path)
                self.assertIn((dirname, basename, variant), list(self.finder._split_glob(name)))

    def _native(self, path):
        """Spell a POSIX-style fixture path with the host separator."""
        return self.port.host.filesystem.normpath(path)

    def test_baselines_and_references_discovered_independently(self):
        non_test_files_by_search_path = OrderedDict([
            (self._native('/dir'), {
                'foo-expected.txt',
                'foo-expected.png',
                'foo-expected.html',
                'foo-expected-mismatch.svg',
            }),
        ])

        text, image, audio = self.finder._baselines_for_test('foo.html', '', non_test_files_by_search_path)
        self.assertEqual(text, self._native('/dir/foo-expected.txt'))
        self.assertEqual(image, self._native('/dir/foo-expected.png'))
        self.assertIsNone(audio)

        references = self.finder._sibling_references_for_test('foo.html', '', non_test_files_by_search_path)
        self.assertEqual(
            [(r.relation, r.path) for r in references],
            [('==', self._native('/dir/foo-expected.html')), ('!=', self._native('/dir/foo-expected-mismatch.svg'))],
        )

        # _expectations_for_test still glues both halves back together.
        self.assertEqual(
            self.finder._expectations_for_test('foo.html', '', non_test_files_by_search_path),
            (
                self._native('/dir/foo-expected.txt'),
                self._native('/dir/foo-expected.png'),
                None,
                [Reference('==', self._native('/dir/foo-expected.html')), Reference('!=', self._native('/dir/foo-expected-mismatch.svg'))],
            ),
        )

    def test_references_come_from_the_most_specific_directory_only(self):
        # Directories are ordered from least to most specific, and searched
        # from the most specific.
        non_test_files_by_search_path = OrderedDict([
            (self._native('/generic'), {'foo-expected.html'}),
            (self._native('/platform'), {'foo-expected-mismatch.html'}),
        ])
        self.assertEqual(
            self.finder._sibling_references_for_test('foo.html', '', non_test_files_by_search_path),
            [Reference('!=', self._native('/platform/foo-expected-mismatch.html'))],
        )

        non_test_files_by_search_path = OrderedDict([
            (self._native('/generic'), {'foo-expected-mismatch.html'}),
            (self._native('/platform'), {'foo-expected.html'}),
        ])
        self.assertEqual(
            self.finder._sibling_references_for_test('foo.html', '', non_test_files_by_search_path),
            [Reference('==', self._native('/platform/foo-expected.html'))],
        )

        self.assertIsNone(
            self.finder._sibling_references_for_test('bar.html', '', non_test_files_by_search_path)
        )

    def test_baselines_come_from_the_most_specific_directory_per_slot(self):
        non_test_files_by_search_path = OrderedDict([
            (self._native('/generic'), {'foo-expected.txt', 'foo-expected.png', 'foo-expected.wav'}),
            (self._native('/platform'), {'foo-expected.png'}),
        ])
        self.assertEqual(
            self.finder._baselines_for_test('foo.html', '', non_test_files_by_search_path),
            (self._native('/generic/foo-expected.txt'), self._native('/platform/foo-expected.png'), self._native('/generic/foo-expected.wav')),
        )

    def test_text_baseline_prefers_txt_over_webarchive(self):
        both = OrderedDict([(self._native('/dir'), {'foo-expected.txt', 'foo-expected.webarchive'})])
        self.assertEqual(self.finder._baselines_for_test('foo.html', '', both)[0], self._native('/dir/foo-expected.txt'))

        webarchive_only = OrderedDict([(self._native('/dir'), {'foo-expected.webarchive'})])
        self.assertEqual(
            self.finder._baselines_for_test('foo.html', '', webarchive_only)[0], self._native('/dir/foo-expected.webarchive')
        )

        # A more specific webarchive does not hide a less specific .txt.
        layered = OrderedDict([(self._native('/generic'), {'foo-expected.txt'}), (self._native('/platform'), {'foo-expected.webarchive'})])
        self.assertEqual(self.finder._baselines_for_test('foo.html', '', layered)[0], self._native('/generic/foo-expected.txt'))

    def test_baselines_and_references_with_variant(self):
        non_test_files_by_search_path = OrderedDict([
            (self._native('/dir'), {'foo_foo-expected.txt', 'foo_foo-expected.png', 'foo-expected.html', 'foo-expected.txt'}),
        ])
        # The baseline is named after the sanitized variant...
        self.assertEqual(
            self.finder._baselines_for_test('foo.html', '?foo', non_test_files_by_search_path),
            (self._native('/dir/foo_foo-expected.txt'), self._native('/dir/foo_foo-expected.png'), None),
        )
        # ...while the reference is the unvarianted file with the variant appended.
        self.assertEqual(
            self.finder._sibling_references_for_test('foo.html', '?foo', non_test_files_by_search_path),
            [Reference('==', self._native('/dir/foo-expected.html?foo'))],
        )


class LayoutTestFinderLinuxTests(PyFakefsLinuxTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderWindowsTests(PyFakefsWindowsTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderMacOSTests(PyFakefsMacOSTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass
