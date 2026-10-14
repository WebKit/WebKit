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
from webkitpy.layout_tests.models.test import test_name_and_variant
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
        )

    def tearDown(self):
        self.port = None
        self.finder = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self.port.host.filesystem.sep, expected_sep)

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


class LayoutTestFinderLinuxTests(PyFakefsLinuxTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderWindowsTests(PyFakefsWindowsTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderMacOSTests(PyFakefsMacOSTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass
