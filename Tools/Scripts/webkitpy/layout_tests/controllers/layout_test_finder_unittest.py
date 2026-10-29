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

import unittest

from pyfakefs.fake_filesystem_unittest import TestCaseMixin

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.layout_tests.controllers.layout_test_finder import (
    LayoutTestFinder,
)
from webkitpy.port.test import (
    TestPort,
    add_unit_tests_to_mock_filesystem,
)


class LayoutTestFinderTests(unittest.TestCase, TestCaseMixin):
    def __init__(self, *args, **kwargs):
        super(LayoutTestFinderTests, self).__init__(*args, **kwargs)
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

    def test_get_tests__double_star_glob(self):
        tests = list(self.finder.get_tests(['**/*test-crash-crash*']))
        self.assertEqual(
            [t.test_path for t in tests],
            ['imported/w3c/web-platform-tests/some/test-crash-crash.html'],
        )

    def _write_files(self, files):
        fs = self.port.host.filesystem
        for name, contents in files.items():
            path = fs.join(self.port.layout_tests_dir(), *name.split("/"))
            fs.maybe_make_directory(fs.dirname(path))
            fs.write_binary_file(path, contents)

    def _reference_files(self, glob):
        layout_tests_dir = self.finder.layout_tests_base_dir

        def relative(path):
            return path if path == "about:blank" else self.port.host.filesystem.relpath(path, layout_tests_dir)

        return {
            test.test_path: (
                [(expectation, relative(path)) for expectation, path in test.reference_files]
                if test.reference_files is not None
                else None
            )
            for test in self.finder.get_tests([glob])
        }

    def test_link_references(self):
        wpt = "imported/w3c/web-platform-tests/"
        self._write_files({
            wpt + "ref/match.html": b'<!DOCTYPE html><link rel="match" href="match-ref.html">',
            wpt + "ref/mismatch.html": b"<link rel=mismatch href='mismatch-notref.html'>",
            wpt + "ref/both.html": b'<link rel="mismatch" href="b.html">\n<link rel="match" href="a.html">\n<link rel="match" href="c.html">',
            wpt + "ref/absolute.html": b'<link rel="match" href="/common/reference/green.html">',
            wpt + "ref/parent.html": b'<link href="../reference/green.html" rel="match">',
            wpt + "ref/query.html": b'<link rel="match" href=" query-ref.html?a=1&amp;b=2#c ">',
            wpt + "ref/blank.html": b'<link rel="match" href="about:blank">',
            wpt + "ref/uppercase.html": b'<LINK REL="match" HREF="uppercase-ref.html">',
            wpt + "ref/prefixed.svg": b'<svg xmlns="http://www.w3.org/2000/svg" xmlns:h="http://www.w3.org/1999/xhtml"><h:link rel="match" href="prefixed-ref.svg"/></svg>',
            wpt + "ref/utf16.xht": '<html xmlns="http://www.w3.org/1999/xhtml"><link rel="match" href="utf16-ref.xht"/></html>'.encode("utf-16"),
            wpt + "ref/duplicate-attribute.html": b'<link rel="match" rel="help" href="first-ref.html" href="second-ref.html">',
        })

        self.assertEqual(
            self._reference_files("imported/w3c/web-platform-tests/ref"),
            {
                wpt + "ref/absolute.html": [("==", wpt + "common/reference/green.html")],
                wpt + "ref/blank.html": [("==", "about:blank")],
                wpt + "ref/both.html": [("==", wpt + "ref/a.html"), ("==", wpt + "ref/c.html"), ("!=", wpt + "ref/b.html")],
                wpt + "ref/duplicate-attribute.html": [("==", wpt + "ref/first-ref.html")],
                wpt + "ref/match.html": [("==", wpt + "ref/match-ref.html")],
                wpt + "ref/mismatch.html": [("!=", wpt + "ref/mismatch-notref.html")],
                wpt + "ref/parent.html": [("==", wpt + "reference/green.html")],
                wpt + "ref/prefixed.svg": [("==", wpt + "ref/prefixed-ref.svg")],
                wpt + "ref/query.html": [("==", wpt + "ref/query-ref.html?a=1&b=2#c")],
                wpt + "ref/uppercase.html": [("==", wpt + "ref/uppercase-ref.html")],
                wpt + "ref/utf16.xht": [("==", wpt + "ref/utf16-ref.xht")],
            },
        )

    def test_link_references_not_references(self):
        wpt = "imported/w3c/web-platform-tests/"
        self._write_files({
            wpt + "notref/comment.html": b'<!-- <link rel="match" href="comment-ref.html"> -->',
            wpt + "notref/bang-comment.html": b'<!-- <link rel="match" href="comment-ref.html"> --!>',
            wpt + "notref/help.html": b'<link rel="help" href="https://drafts.csswg.org/#match">',
            wpt + "notref/rel-list.html": b'<link rel="match stylesheet" href="rel-list-ref.html">',
            wpt + "notref/rel-case.html": b'<link rel="Match" href="rel-case-ref.html">',
            wpt + "notref/no-href.html": b'<link rel="match">',
            wpt + "notref/other-origin.html": b'<link rel="match" href="https://example.com/ref.html">',
            wpt + "notref/linked.html": b'<linked rel="match" href="linked-ref.html">',
            wpt + "notref/testharness.html": b'<script src="/resources/testharness.js"></script>\n<link rel="match" href="testharness-ref.html">',
            wpt + "notref/text.html": b"<p>This should match.</p>",
            wpt + "crashtests/link.html": b'<link rel="match" href="link-ref.html">',
            "fast/link.html": b'<link rel="match" href="link-ref.html">',
        })

        for glob in ("imported/w3c/web-platform-tests/notref", "imported/w3c/web-platform-tests/crashtests/link.html", "fast/link.html"):
            references = self._reference_files(glob)
            self.assertTrue(references)
            for test_path, reference_files in references.items():
                self.assertIsNone(reference_files, test_path)

    def test_link_references_after_comment(self):
        wpt = "imported/w3c/web-platform-tests/"
        self._write_files({
            wpt + "ref/after-comment.html": b'<!-- comment --><!-->\n<link rel="match" href="after-comment-ref.html">',
            wpt + "ref/after-bang-comment.html": b'<!-- comment --!>\n<link rel="match" href="after-bang-comment-ref.html">',
        })

        self.assertEqual(
            self._reference_files("imported/w3c/web-platform-tests/ref"),
            {
                wpt + "ref/after-bang-comment.html": [("==", wpt + "ref/after-bang-comment-ref.html")],
                wpt + "ref/after-comment.html": [("==", wpt + "ref/after-comment-ref.html")],
            },
        )

    def test_link_references_override_expected_references(self):
        wpt = "imported/w3c/web-platform-tests/"
        self._write_files({
            wpt + "ref/overridden.html": b'<link rel="match" href="overridden-ref.html">',
            wpt + "ref/overridden-ref.html": b"",
            wpt + "ref/overridden-expected.html": b"",
            wpt + "ref/not-imported.html": b'<link rel="match" href="reference/not-imported-ref.html">',
            wpt + "ref/not-imported-expected.html": b"",
        })

        self.assertEqual(
            self._reference_files("imported/w3c/web-platform-tests/ref"),
            {
                wpt + "ref/not-imported.html": [("==", wpt + "ref/not-imported-expected.html")],
                wpt + "ref/overridden.html": [("==", wpt + "ref/overridden-ref.html")],
            },
        )

    def test_link_references_variants(self):
        wpt = "imported/w3c/web-platform-tests/"
        self._write_files({
            wpt + "ref/variants.html": b'<link rel="match" href="variants-ref.html">\n'
                                       b'<link rel="mismatch" href="about:blank">\n'
                                       b'<meta name="variant" content="?a">\n'
                                       b'<meta name="variant" content="#b">',
        })

        self.assertEqual(
            self._reference_files("imported/w3c/web-platform-tests/ref"),
            {
                wpt + "ref/variants.html?a": [("==", wpt + "ref/variants-ref.html?a"), ("!=", "about:blank")],
                wpt + "ref/variants.html#b": [("==", wpt + "ref/variants-ref.html#b"), ("!=", "about:blank")],
            },
        )
