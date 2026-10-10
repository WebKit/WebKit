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

import logging
import os
import posixpath
import unittest
from collections import OrderedDict
from unittest import mock

from pyfakefs.fake_filesystem import OSType

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.fakefs_testcase import (
    PyFakefsLinuxTestCaseMixin,
    PyFakefsMacOSTestCaseMixin,
    PyFakefsWindowsTestCaseMixin,
)
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.layout_tests.controllers import layout_test_finder
from webkitpy.layout_tests.controllers.layout_test_finder import (
    LayoutTestFinder,
)
from webkitpy.layout_tests.models.server_routing import ServerRoute, ServerType
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
        ):
            with self.subTest(name):
                file_path, variant = test_name_and_variant(name)
                dirname, basename = posixpath.split(file_path)
                self.assertIn((dirname, basename, variant), list(self.finder._split_glob(name)))

    def test_get_tests__double_star_glob(self):
        self.assertTestsFound(
            ['**/*test-crash-crash*'],
            ['imported/w3c/web-platform-tests/some/test-crash-crash.html'],
        )

    def test_generated_js_sources_are_only_tests_under_wpt_roots(self):
        any_js = '// META: global=window\ntest(() => {}, "pass");\n'
        self._add_test_file('fast/foo/bar.any.js', any_js)
        self._add_test_file('fast/foo/bar.window.js', any_js)
        self._add_test_file('fast/foo/bar.worker.js', any_js)
        self._add_test_file('imported/w3c/web-platform-tests/foo/bar.any.js', any_js)

        self.assertTestsFound(['fast/foo'], [])
        self.assertTestsFound(
            ['imported/w3c/web-platform-tests/foo'],
            ['imported/w3c/web-platform-tests/foo/bar.any.html'],
        )
    def test_get_tests_discovers_stubless_generated_variant(self):
        """LayoutTestFinder.get_tests() -- the public entry point real
        callers like rebaselineserver.py and test_importer.py use, and the
        one TestExpectationParser._test_exists() falls back to -- must
        discover a stub-less generated WPT variant (.any.worker.html) from
        just its .any.js source. Other tests exercise the lower-level
        _wpt_tests_for_path directly; this drives the same path real callers
        actually use.
        """
        self._add_test_file(
            'imported/w3c/web-platform-tests/foo/basic.any.js',
            '// META: global=window,worker\ntest(() => {}, "pass");\n',
        )
        tests = list(self.finder.get_tests(['imported/w3c/web-platform-tests/foo']))
        self.assertEqual(
            sorted(t.test_path for t in tests),
            [
                'imported/w3c/web-platform-tests/foo/basic.any' + suffix
                for suffix in ('.html', '.serviceworker.html', '.sharedworker.html', '.worker.html')
            ],
        )

    def _native(self, path):
        """Spell a POSIX-style fixture path with the host separator."""
        return self.port.host.filesystem.normpath(path)

    def test_baselines_and_references_discovered_independently(self):
        from collections import OrderedDict

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

    def _add_test_file(self, path, contents=''):
        fs = self.port.host.filesystem
        full_path = fs.join(self.port.layout_tests_dir(), path)
        fs.maybe_make_directory(fs.dirname(full_path))
        fs.write_text_file(full_path, contents)

    def _fuzzy_of(self, contents, test='fast/fuzzy/test.html', reference='fast/fuzzy/test-expected.html', query=None):
        """Discover a non-WPT test and return its Test.fuzzy. A sibling
        reference is needed for the test to be a reftest at all."""
        self._add_test_file(test, contents)
        if reference:
            self._add_test_file(reference, '<html></html>\n')
        (found,) = self.assertTestsFound([query or test], [query or test])
        return found.fuzzy

    def _abs(self, rel_path):
        fs = self.port.host.filesystem
        return fs.normpath(fs.join(self.port.layout_tests_dir(), rel_path))

    MATCH = '<link rel="match" href="test-expected.html">'

    def test_non_wpt_fuzzy_default_values(self):
        for description, value, expected in (
            ('named, with spaces', 'maxDifference = 15 ; totalPixels = 300', {None: [[15, 15], [300, 300]]}),
            ('nameless', ' 15 ; 300 ', {None: [[15, 15], [300, 300]]}),
            ('named ranges', 'maxDifference=5-15;totalPixels =  200 - 300 ', {None: [[5, 15], [200, 300]]}),
            ('nameless ranges', '5-15;  200 - 300 ', {None: [[5, 15], [200, 300]]}),
        ):
            with self.subTest(description):
                self.assertEqual(
                    self._fuzzy_of('<html><head>%s<meta name=fuzzy content="%s"></head></html>\n' % (self.MATCH, value)),
                    expected,
                )

    def test_non_wpt_fuzzy_per_reference_is_keyed_by_absolute_reference_path(self):
        fuzzy = self._fuzzy_of(
            '<html><head>%s'
            '<link rel="match" href="close-match-ref.html">'
            '<link rel="match" href="worse-match-ref.html">'
            '<meta name=fuzzy content="5-15;200-300 ">'
            '<meta name=fuzzy content="close-match-ref.html:5;20">'
            '<meta name=fuzzy content="worse-match-ref.html: 15;30">'
            '</head></html>\n' % self.MATCH
        )
        self.assertEqual(
            fuzzy,
            {
                None: [[5, 15], [200, 300]],
                self._abs('fast/fuzzy/close-match-ref.html'): [[5, 5], [20, 20]],
                self._abs('fast/fuzzy/worse-match-ref.html'): [[15, 15], [30, 30]],
            },
        )

    def test_non_wpt_fuzzy_relative_reference_is_resolved_to_an_absolute_key(self):
        fuzzy = self._fuzzy_of(
            '<html><head>%s'
            '<link rel="match" href="../resources/common-ref.html">'
            '<meta name=fuzzy content="maxDifference=15;totalPixels=300">'
            '<meta name=fuzzy content="../resources/common-ref.html:maxDifference=5-8;totalPixels=78-84">'
            '</head></html>\n' % self.MATCH
        )
        self.assertEqual(
            fuzzy,
            {
                None: [[15, 15], [300, 300]],
                self._abs('fast/resources/common-ref.html'): [[5, 8], [78, 84]],
            },
        )

    def test_non_wpt_fuzzy_in_xml_document(self):
        fuzzy = self._fuzzy_of(
            '<svg width="340" height="140" xmlns="http://www.w3.org/2000/svg" xmlns:html="http://www.w3.org/1999/xhtml">'
            '<html:meta name="fuzzy" content="maxDifference=0-1; totalPixels=0-2"/></svg>\n',
            test='fast/fuzzy/test.svg',
            reference='fast/fuzzy/test-expected.svg',
        )
        self.assertEqual(fuzzy, {None: [[0, 1], [0, 2]]})

    def test_non_wpt_without_fuzzy_metadata_has_no_fuzzy(self):
        self.assertIsNone(self._fuzzy_of('<html><head>%s</head></html>\n' % self.MATCH))

    def test_non_wpt_fuzzy_needs_a_sibling_reference(self):
        # Without a sibling reference the test isn't a reftest, so its
        # metadata is not read.
        self.assertIsNone(
            self._fuzzy_of(
                '<html><head>%s<meta name=fuzzy content="15;300"></head></html>\n' % self.MATCH,
                reference=None,
            )
        )

    def test_non_wpt_fuzzy_of_a_variant_comes_from_the_file_without_the_variant(self):
        # Including a fragment before a query: splitting at the first '?'
        # would leave '#frag' on the file name, so the file wouldn't be found.
        for query in ('fast/fuzzy/test.html?variant', 'fast/fuzzy/test.html#frag?variant'):
            with self.subTest(query):
                fuzzy = self._fuzzy_of(
                    '<html><head>%s<meta name=fuzzy content="15;300"></head></html>\n' % self.MATCH,
                    query=query,
                )
                self.assertEqual(fuzzy, {None: [[15, 15], [300, 300]]})

    def test_non_wpt_malformed_fuzzy_is_ignored_with_a_warning(self):
        for description, head in (
            ('one range', '<meta name=fuzzy content="1">'),
            ('unknown property', '<meta name=fuzzy content="foo=1;2">'),
            ('reference that is not linked', '<meta name=fuzzy content="other-ref.html:1;2">'),
            ('non-integer', '<meta name=fuzzy content="a;1">'),
        ):
            with self.subTest(description):
                with self.assertLogs("webkitpy.layout_tests.controllers.layout_test_finder", level=logging.WARNING) as cm:
                    fuzzy = self._fuzzy_of('<html><head>%s%s</head></html>\n' % (self.MATCH, head))
                self.assertIsNone(fuzzy)
                self.assertTrue(
                    any("Ignoring fuzzy metadata" in msg and "fast/fuzzy/test.html" in msg for msg in cm.output),
                    cm.output,
                )

    def test_non_wpt_fuzzy_when_the_file_cannot_be_read_through_the_filesystem(self):
        # The finder falls back to letting SourceFile read the file itself.
        with mock.patch.object(self.finder.fs, "read_binary_file", side_effect=IOError("gone")):
            fuzzy = self._fuzzy_of('<html><head>%s<meta name=fuzzy content="15;300"></head></html>\n' % self.MATCH)
        self.assertEqual(fuzzy, {None: [[15, 15], [300, 300]]})

    def assertTestsFound(self, queries, expected_paths):
        tests = list(self.finder.get_tests(queries))
        self.assertEqual([t.test_path for t in tests], expected_paths)
        return tests


class LayoutTestFinderLinuxTests(PyFakefsLinuxTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderWindowsTests(PyFakefsWindowsTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderMacOSTests(PyFakefsMacOSTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class WptTestsForPathTestsBase(object):
    """Tests for _wpt_tests_for_path dispatch on item_type."""

    WPT_PREFIX = "imported/w3c/web-platform-tests"
    WPT_URL_BASE = "/"

    def setUp(self):
        self.setUpPyfakefs()
        host = MockHost(create_stub_repository_files=True, filesystem=FileSystem())
        add_unit_tests_to_mock_filesystem(host.filesystem)
        self.port = TestPort(host)
        self.filesystem = self.port.host.filesystem
        self.layout_tests_dir = self.port.layout_tests_dir()
        self.test_routes = [
            ServerRoute(self.WPT_PREFIX, self.WPT_URL_BASE, ServerType.WPT),
        ]
        self.finder = LayoutTestFinder(
            self.filesystem,
            self.layout_tests_dir,
            self.port.baseline_search_path(),
            test_routes=self.test_routes,
        )

    def tearDown(self):
        self.port = None
        self.finder = None
        self.filesystem = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self.filesystem.sep, expected_sep)

    def _write_wpt_file(self, rel_path, contents):
        """Write a file under the WPT prefix, return its absolute path."""
        # WPT_PREFIX and rel_path use "/", so normalise to the host separator
        # to match the paths the finder reports.
        abs_path = self.filesystem.normpath(self.filesystem.join(self.layout_tests_dir, self.WPT_PREFIX, rel_path))
        self.filesystem.maybe_make_directory(self.filesystem.dirname(abs_path))
        self.filesystem.write_text_file(abs_path, contents)
        return abs_path

    def _get_wpt_tests(self, rel_path, variants=None):
        """Run _wpt_tests_for_path for a file under the WPT prefix."""
        parts = rel_path.rsplit("/", 1)
        if len(parts) == 2:
            dirname = self.WPT_PREFIX + "/" + parts[0]
            basename = parts[1]
        else:
            dirname = self.WPT_PREFIX
            basename = parts[0]
        from collections import OrderedDict
        non_test_files = OrderedDict()
        layout_dir = self.filesystem.join(self.layout_tests_dir, dirname)
        files = set()
        if self.filesystem.isdir(layout_dir):
            for entry in self.filesystem.scandir(layout_dir):
                if entry.is_file():
                    files.add(entry.name)
        non_test_files[layout_dir] = files
        return list(self.finder._wpt_tests_for_path(dirname, basename, variants, non_test_files))

    # -----------------------------------------------------------------------
    # Test: jsshell variant is skipped
    # -----------------------------------------------------------------------
    def test_any_js_jsshell_variant_skipped(self):
        """TestharnessTest with jsshell=True must not yield a Test."""
        # An .any.js declaring global=window,jsshell produces two items:
        # one for the .any.html window variant, and one for jsshell (.any.js URL).
        # Only the window variant should appear: the jsshell URL is the .any.js
        # source itself, which isn't loadable in a browser.
        self._write_wpt_file("blob/Blob-bytes.any.js", "// META: global=window,jsshell\ntest(() => {}, 'blob');\n")
        self.assertEqual(
            [t.test_path for t in self._get_wpt_tests("blob/Blob-bytes.any.js")],
            [self.WPT_PREFIX + "/blob/Blob-bytes.any.html"],
        )

    # -----------------------------------------------------------------------
    # Test: manual item type yields nothing
    # -----------------------------------------------------------------------
    def test_manual_file_yields_nothing(self):
        """-manual.window.js files produce a 'manual' item_type — yield nothing."""
        self._write_wpt_file("foo/test-manual.window.js", "// META: global=window\ntest(() => {}, 'manual');\n")
        tests = self._get_wpt_tests("foo/test-manual.window.js")
        self.assertEqual(tests, [])

    # -----------------------------------------------------------------------
    # Test: HTML support file → no Test yielded and no warning
    # -----------------------------------------------------------------------
    def test_html_support_file_yields_nothing(self):
        """Pure HTML support file under WPT root: no Test yielded, and nothing
        is logged (asymmetric with non-WPT silent-skip otherwise)."""
        self._write_wpt_file("foo/helper.html", "<html><body>helper</body></html>\n")
        logger = logging.getLogger("webkitpy.layout_tests.controllers.layout_test_finder")
        # assertNoLogs needs Python 3.10; log a sentinel so assertLogs
        # doesn't fail on an empty log, and check it is the only record.
        with self.assertLogs(logger, level=logging.WARNING) as cm:
            logger.warning("sentinel")
            tests = self._get_wpt_tests("foo/helper.html")
        self.assertEqual(tests, [])
        self.assertEqual(len(cm.output), 1, cm.output)

    # -----------------------------------------------------------------------
    # Test: .any.js produces expected .any.html and .any.worker.html Tests
    # -----------------------------------------------------------------------
    def test_any_js_produces_html_and_worker_variants(self):
        """A basic .any.js (global=window,worker) yields one Test for each
        generated variant, whether or not the importer wrote a stub."""
        self._write_wpt_file("foo/basic.any.js", "// META: global=window,worker\ntest(() => {}, 'pass');\n")
        tests = self._get_wpt_tests("foo/basic.any.js")
        self.assertEqual(
            sorted(t.test_path for t in tests),
            [
                self.WPT_PREFIX + "/foo/basic.any" + suffix
                for suffix in (".html", ".serviceworker.html", ".sharedworker.html", ".worker.html")
            ],
        )
        # All tests should be WPT server type
        for t in tests:
            self.assertEqual(t.served_by, ServerType.WPT)

    # -----------------------------------------------------------------------
    # Test: variant with spaces is percent-encoded in test_path
    # -----------------------------------------------------------------------
    def test_window_js_variant_with_spaces_is_percent_encoded(self):
        """A .window.js with '// META: variant=?foo bar' must yield a Test
        whose test_path ends with '?foo%20bar' (percent-encoded), and the
        baseline lookup must resolve to the canonical '…_foo_20bar-expected.txt'
        on-disk filename.
        """
        from collections import OrderedDict

        # Write the .window.js source with a variant that contains a space.
        self._write_wpt_file(
            "foo/test.window.js",
            "// META: variant=?foo bar\ntest(() => {}, 'pass');\n",
        )
        # Write a baseline file using the canonical on-disk form: space → %20
        # via _percent_encoded_variant, then % → _ via sanitized_variant.
        # So "?foo bar" → "?foo%20bar" → sanitized "foo_20bar" →
        # baseline "test.window_foo_20bar-expected.txt".
        baseline_filename = "test.window_foo_20bar-expected.txt"
        dirname = self.WPT_PREFIX + "/foo"
        layout_dir = self.filesystem.join(self.layout_tests_dir, dirname)
        self.filesystem.maybe_make_directory(layout_dir)
        self.filesystem.write_text_file(
            self.filesystem.join(layout_dir, baseline_filename),
            "PASS\n",
        )

        # Build non_test_files: include layout dir (with test file + baseline).
        non_test_files = OrderedDict()
        files = set()
        for entry in self.filesystem.scandir(layout_dir):
            if entry.is_file():
                files.add(entry.name)
        non_test_files[layout_dir] = files

        tests = list(
            self.finder._wpt_tests_for_path(dirname, "test.window.js", None, non_test_files)
        )

        # Should produce exactly one test (the .window.html variant with encoded query).
        self.assertEqual(len(tests), 1, f"Expected 1 test, got {len(tests)}: {[t.test_path for t in tests]}")
        t = tests[0]

        # test_path must carry the percent-encoded variant.
        self.assertTrue(
            t.test_path.endswith("?foo%20bar"),
            f"Expected test_path to end with '?foo%20bar', got: {t.test_path!r}",
        )

        # Baseline lookup must resolve to the canonical on-disk file.
        self.assertIsNotNone(
            t.expected_text_path,
            "Expected a non-None expected_text_path (baseline should be found on disk)",
        )
        self.assertTrue(
            t.expected_text_path.endswith(baseline_filename),
            f"Expected expected_text_path to end with {baseline_filename!r}, got: {t.expected_text_path!r}",
        )

    def test_baseline_paths_use_a_single_separator(self):
        # The baseline name carries the "/" of the WPT test path, but the path
        # reported to callers must be normalised to the host separator.
        self._write_wpt_file("foo/t.html", '<script src="/resources/testharness.js"></script>\n')
        baseline = self._write_wpt_file("foo/t-expected.txt", "PASS\n")
        (test,) = self._get_wpt_tests("foo/t.html")
        # Compare the whole path, not its basename: os.path.basename doesn't split on
        # backslashes under pyfakefs's Windows emulation (https://github.com/pytest-dev/pyfakefs/issues/1348).
        self.assertEqual(test.expected_text_path, baseline)

    def test_reftest_fuzzy_keyed_by_absolute_path(self):
        """Fuzzy dict keys must match Reference.path's format: absolute
        filesystem paths (see _wpt_references_for_item's docstring). The
        consumer, single_test_runner._fuzzy_tolerance_for_reference, looks
        tolerances up by exactly that absolute path -- a relative-path key
        here would never match, silently falling back to the None-default
        tolerance (or none at all) for every WPT reftest with a per-reference
        fuzzy value.
        """
        self._write_wpt_file(
            "foo/reftest.html",
            '<html><head>'
            '<link rel=match href="some-ref.html">'
            '<meta name="fuzzy" content="some-ref.html:0-5;0-100">'
            '</head></html>\n'
        )
        # Create the reference file so it resolves on disk
        self._write_wpt_file("foo/some-ref.html", "<html><body>ref</body></html>\n")

        tests = self._get_wpt_tests("foo/reftest.html")
        self.assertEqual(len(tests), 1)
        t = tests[0]
        self.assertIsNotNone(t.fuzzy)
        # The key must be the same absolute path used as the Reference's own
        # `path`, since that's what the consumer looks tolerances up by.
        self.assertEqual(len(t.reference_files), 1)
        ref_path = t.reference_files[0].path
        self.assertIn(ref_path, t.fuzzy,
                      f"Expected {ref_path!r} key in fuzzy dict, got: {list(t.fuzzy.keys())}")
        self.assertTrue(self.filesystem.isabs(ref_path), f"Expected an absolute path, got: {ref_path!r}")

    def test_reftest_prefers_sibling_over_manifest_ref(self):
        """When a reftest's manifest ref AND an imported `<stem>-expected.<ext>`
        sibling both exist on disk, the sibling must win — the WebKit WPT
        importer hand-tweaks the imported sibling for WebKit-specific
        rendering quirks, so TestExpectations is calibrated against the
        sibling, not the upstream manifest ref."""
        self._write_wpt_file(
            "foo/reftest.html",
            '<html><head>'
            '<link rel=match href="upstream-ref.html">'
            '</head></html>\n'
        )
        # Manifest target — resolves on disk
        self._write_wpt_file("foo/upstream-ref.html", "<html><body>upstream</body></html>\n")
        # Imported sibling — what TestExpectations is calibrated against
        self._write_wpt_file("foo/reftest-expected.html", "<html><body>imported</body></html>\n")

        tests = self._get_wpt_tests("foo/reftest.html")
        self.assertEqual(len(tests), 1)
        t = tests[0]
        self.assertIsNotNone(t.reference_files)
        ref_paths = [ref.path for ref in t.reference_files]
        # Sibling must win
        self.assertTrue(
            any(p.endswith("reftest-expected.html") for p in ref_paths),
            f"Expected sibling reftest-expected.html in references, got: {ref_paths}",
        )
        # Manifest ref must NOT appear
        self.assertFalse(
            any(p.endswith("upstream-ref.html") for p in ref_paths),
            f"Manifest ref upstream-ref.html should be overridden by sibling, got: {ref_paths}",
        )

    # -----------------------------------------------------------------------
    # Tests for union-match in _process_directory
    # -----------------------------------------------------------------------
    def test_process_directory_matches_generated_any_worker_html(self):
        """Path-spec naming a generated WPT variant (foo.any.worker.html)
        must yield that variant's Test even when only foo.any.js exists
        on disk — i.e. no importer stub. Exercises branch 2 of the union
        match in _process_directory's inner loop.
        """
        self._write_wpt_file(
            "foo/foo.any.js",
            "// META: global=window,worker\ntest(() => {}, 'pass');\n",
        )
        # Simulate: run-webkit-tests …/foo/foo.any.worker.html (no variant).
        # fnfilter = [("foo.any.worker.html", "")] matches the generated basename
        # even though only foo.any.js is on disk.
        dirname = self.WPT_PREFIX + "/foo"
        fnfilter = [("foo.any.worker.html", "")]
        tests = list(self.finder._process_directory(dirname, fnfilter=fnfilter))
        self.assertEqual(len(tests), 1, f"Expected 1 test, got: {[t.test_path for t in tests]}")
        self.assertTrue(
            tests[0].test_path.endswith("foo/foo.any.worker.html"),
            f"Expected test_path ending in foo.any.worker.html, got: {tests[0].test_path!r}",
        )

    # -----------------------------------------------------------------------
    # Exception narrowing around SourceFile.manifest_items()
    # -----------------------------------------------------------------------
    def test_manifest_items_value_error_is_skipped(self):
        """SourceFile.manifest_items() raising ValueError (malformed fuzzy,
        malformed reference keys, print reftests without refs, etc.) is
        treated as 'skip this file', not a discovery-aborting failure."""
        self._write_wpt_file("foo/malformed.html", "<html></html>\n")
        with mock.patch.object(
            layout_test_finder.SourceFile, "manifest_items", side_effect=ValueError("bad fuzzy value")
        ):
            tests = self._get_wpt_tests("foo/malformed.html")
        self.assertEqual(tests, [])

    def test_manifest_items_other_exception_propagates(self):
        """A non-ValueError from SourceFile.manifest_items() must propagate,
        rather than being silently swallowed like the old `except Exception`."""
        self._write_wpt_file("foo/broken.html", "<html></html>\n")
        with mock.patch.object(
            layout_test_finder.SourceFile, "manifest_items", side_effect=RuntimeError("boom")
        ):
            with self.assertRaises(RuntimeError):
                self._get_wpt_tests("foo/broken.html")

    def test_malformed_wpt_files_are_skipped_with_a_warning(self):
        """One real file per `raise ValueError` site in SourceFile; none is mocked."""
        ref = '<link rel=match href="ref.html">'
        for description, rel_path, head in (
            ("fuzzy key is not a reference", "foo/a.html", ref + '<meta name=fuzzy content="other-ref.html:0-1;0-1">'),
            ("malformed fuzzy value", "foo/b.html", ref + '<meta name=fuzzy content="1">'),
            ("invalid fuzzy property", "foo/c.html", ref + '<meta name=fuzzy content="foo=1;2">'),
            ("duplicate fuzzy property", "foo/d.html", ref + '<meta name=fuzzy content="maxDifference=1;maxDifference=2">'),
            ("non-integer fuzzy range", "foo/e.html", ref + '<meta name=fuzzy content="a;1">'),
            (
                "duplicate page-ranges",
                "print/f.html",
                ref + '<meta name=reftest-pages content="1"><meta name=reftest-pages content="2">',
            ),
            ("malformed page-range", "print/g.html", ref + '<meta name=reftest-pages content="a-b">'),
            ("zero page-range", "print/h.html", ref + '<meta name=reftest-pages content="0-1">'),
            ("single non-integer page-range", "print/i.html", ref + '<meta name=reftest-pages content="x">'),
            ("print reftest without references", "print/j.html", ""),
        ):
            with self.subTest(description):
                self._write_wpt_file(rel_path, "<html><head>%s</head></html>\n" % head)
                self._write_wpt_file(rel_path.rsplit("/", 1)[0] + "/ref.html", "<html></html>\n")
                with self.assertLogs("webkitpy.layout_tests.controllers.layout_test_finder", level=logging.WARNING) as cm:
                    tests = self._get_wpt_tests(rel_path)
                self.assertEqual(tests, [])
                self.assertTrue(
                    any("Skipping WPT file" in msg and rel_path in msg for msg in cm.output),
                    "Expected a warning naming %s, got: %s" % (rel_path, cm.output),
                )

    def test_unreadable_file_falls_back_to_sourcefile_read(self):
        self._write_wpt_file("foo/plain.html", '<script src="/resources/testharness.js"></script>\n')
        for error in (IOError("gone"), UnicodeDecodeError("utf-8", b"\xff", 0, 1, "invalid start byte")):
            with self.subTest(type(error).__name__):
                with mock.patch.object(self.filesystem, "read_binary_file", side_effect=error):
                    tests = self._get_wpt_tests("foo/plain.html")
                self.assertEqual([t.test_path for t in tests], [self.WPT_PREFIX + "/foo/plain.html"])

    def test_only_requested_variants_are_returned(self):
        self._write_wpt_file(
            "foo/test.window.js",
            "// META: variant=?a\n// META: variant=?b\n// META: variant=?c\ntest(() => {}, 'pass');\n",
        )
        self.assertEqual(
            [t.test_path for t in self._get_wpt_tests("foo/test.window.js")],
            [self.WPT_PREFIX + "/foo/test.window.html" + v for v in ("?a", "?b", "?c")],
        )
        self.assertEqual(
            [t.test_path for t in self._get_wpt_tests("foo/test.window.js", variants=["?b"])],
            [self.WPT_PREFIX + "/foo/test.window.html?b"],
        )
        self.assertEqual(self._get_wpt_tests("foo/test.window.js", variants=["?zzz"]), [])

    def test_reftest_uses_manifest_reference_when_there_is_no_sibling(self):
        self._write_wpt_file("foo/reftest.html", '<link rel=match href="manifest-ref.html">\n')
        ref_path = self._write_wpt_file("foo/manifest-ref.html", "<html></html>\n")
        (test,) = self._get_wpt_tests("foo/reftest.html")
        self.assertEqual([(r.relation, r.path) for r in test.reference_files], [("==", ref_path)])

    def test_reftest_with_missing_manifest_reference_and_no_sibling_warns(self):
        self._write_wpt_file("foo/reftest.html", '<link rel=match href="missing-ref.html">\n')
        with self.assertLogs("webkitpy.layout_tests.controllers.layout_test_finder", level=logging.WARNING) as cm:
            (test,) = self._get_wpt_tests("foo/reftest.html")
        self.assertIsNone(test.reference_files)
        self.assertTrue(
            any("manifest reference(s) missing" in msg and "missing-ref.html" in msg for msg in cm.output),
            cm.output,
        )

    def test_reftest_drops_only_the_missing_manifest_references(self):
        self._write_wpt_file(
            "foo/reftest.html",
            '<link rel=match href="present-ref.html"><link rel=mismatch href="missing-ref.html">\n',
        )
        present = self._write_wpt_file("foo/present-ref.html", "<html></html>\n")
        (test,) = self._get_wpt_tests("foo/reftest.html")
        self.assertEqual([(r.relation, r.path) for r in test.reference_files], [("==", present)])

    def test_url_base_other_than_root(self):
        finder = LayoutTestFinder(
            self.filesystem,
            self.layout_tests_dir,
            self.port.baseline_search_path(),
            test_routes=[ServerRoute("http/wpt", "/WebKit/", ServerType.WPT)],
        )
        abs_path = self.filesystem.join(self.layout_tests_dir, "http/wpt/foo/bar.html")
        self.filesystem.maybe_make_directory(self.filesystem.dirname(abs_path))
        self.filesystem.write_text_file(abs_path, '<script src="/resources/testharness.js"></script>\n')
        tests = list(finder.get_tests(["http/wpt/foo"]))
        self.assertEqual([t.test_path for t in tests], ["http/wpt/foo/bar.html"])
        self.assertEqual(tests[0].served_by, ServerType.WPT)

        self.assertEqual(finder._wpt_url_to_test_path("/WebKit/foo/bar.html?x", "http/wpt", "/WebKit/"), "http/wpt/foo/bar.html?x")
        self.assertIsNone(finder._wpt_url_to_test_path("/other/foo/bar.html", "http/wpt", "/WebKit/"))
        self.assertIsNone(finder._wpt_url_to_test_path("relative/bar.html", "http/wpt", "/WebKit/"))

    def test_discovery_through_tests_for_path_dispatches_on_wpt_route(self):
        self._write_wpt_file("foo/basic.any.js", "// META: global=window,worker\ntest(() => {}, 'pass');\n")
        dirname = self.WPT_PREFIX + "/foo"
        tests = list(self.finder._tests_for_path(dirname, "basic.any.js", None, OrderedDict()))
        self.assertEqual(
            sorted(t.test_path for t in tests),
            [
                self.WPT_PREFIX + "/foo/basic.any" + suffix
                for suffix in (".html", ".serviceworker.html", ".sharedworker.html", ".worker.html")
            ],
        )
        for t in tests:
            self.assertEqual(t.served_by, ServerType.WPT)

    def test_process_directory_branch1_unencoded_variant_matches(self):
        """Branch-1 encoding mismatch, fixed: _split_glob yields the user's
        variant unencoded (`?foo bar`), but _process_directory now encodes it
        before passing it down as wanted_variants, matching the encoded
        `?foo%20bar` that _wpt_tests_for_path compares against.
        """
        self._write_wpt_file(
            "foo/test.window.js",
            "// META: variant=?foo bar\ntest(() => {}, 'pass');\n",
        )
        # path-spec matches test.window.js on disk (branch 1), variant "?foo bar"
        dirname = self.WPT_PREFIX + "/foo"
        fnfilter = [("test.window.js", "?foo bar")]
        tests = list(self.finder._process_directory(dirname, fnfilter=fnfilter))
        self.assertEqual(len(tests), 1)
        self.assertTrue(tests[0].test_path.endswith("?foo%20bar"))

    def test_process_directory_branch2_unencoded_variant_matches(self):
        """Branch-2 encoding mismatch, fixed: the post-filter now compares
        the Test's encoded `?foo%20bar` against the user's variant encoded
        the same way, instead of against the raw unencoded `?foo bar`.
        """
        self._write_wpt_file(
            "foo/test.window.js",
            "// META: variant=?foo bar\ntest(() => {}, 'pass');\n",
        )
        # path-spec matches the generated test.window.html, not test.window.js
        # (branch 2 — must dispatch test.window.js and post-filter on generated
        # test_path basename + variant)
        dirname = self.WPT_PREFIX + "/foo"
        fnfilter = [("test.window.html", "?foo bar")]
        tests = list(self.finder._process_directory(dirname, fnfilter=fnfilter))
        self.assertEqual(len(tests), 1)
        self.assertTrue(tests[0].test_path.endswith("?foo%20bar"))

    def _process_window_js_with_variants(self, fnfilter):
        self._write_wpt_file(
            "foo/test.window.js",
            "// META: variant=?a\n// META: variant=?b\ntest(() => {}, 'pass');\n",
        )
        return [
            t.test_path
            for t in self.finder._process_directory(self.WPT_PREFIX + "/foo", fnfilter=fnfilter)
        ]

    def test_process_directory_generated_variant_with_variant_selects_only_that_variant(self):
        # test.window.html isn't on disk (branch 2), and ?a selects one of
        # the two generated variants.
        prefix = self.WPT_PREFIX + "/foo/test.window.html"
        self.assertEqual(self._process_window_js_with_variants([("test.window.html", "?a")]), [prefix + "?a"])
        self.assertEqual(self._process_window_js_with_variants([("test.window.html", "?b")]), [prefix + "?b"])
        self.assertEqual(self._process_window_js_with_variants([("test.window.html", "?zzz")]), [])

    def test_process_directory_generated_variant_without_variant_returns_all_variants(self):
        prefix = self.WPT_PREFIX + "/foo/test.window.html"
        self.assertEqual(
            self._process_window_js_with_variants([("test.window.html", "")]),
            [prefix + "?a", prefix + "?b"],
        )

    def test_process_directory_source_file_with_variant_selects_only_that_variant(self):
        # test.window.js is on disk (branch 1), and the variant is passed down.
        prefix = self.WPT_PREFIX + "/foo/test.window.html"
        self.assertEqual(self._process_window_js_with_variants([("test.window.js", "?b")]), [prefix + "?b"])
        self.assertEqual(self._process_window_js_with_variants([("test.window.js", "?zzz")]), [])

    def test_baselines_for_variants_with_special_characters(self):
        """The committed variant baselines are named after the sanitized
        variant; check that each resolves through the finder. The generated
        .window.html stub is on disk, as the importer writes it."""
        variants_to_baselines = {
            "?exclude=(file_javascript_mailto)": "t.window_exclude=(file_javascript_mailto)-expected.txt",
            "?include=SFrameTransform._": "t.window_include=SFrameTransform._-expected.txt",
            "?wpt_flags=h2": "t.window_wpt_flags=h2-expected.txt",
            "?worker=dedicated_worker": "t.window_worker=dedicated_worker-expected.txt",
            "?1-1": "t.window_1-1-expected.txt",
            "?13-last": "t.window_13-last-expected.txt",
        }
        self._write_wpt_file(
            "foo/t.window.js",
            "".join("// META: variant=%s\n" % variant for variant in variants_to_baselines)
            + "test(() => {}, 'pass');\n",
        )
        self._write_wpt_file("foo/t.window.html", "<!-- stub -->\n")
        baseline_paths = {
            variant: self._write_wpt_file("foo/" + baseline, "PASS\n")
            for variant, baseline in variants_to_baselines.items()
        }

        tests = self._get_wpt_tests("foo/t.window.js")

        # Compare whole paths, not basenames: os.path.basename doesn't split on
        # backslashes under pyfakefs's Windows emulation (https://github.com/pytest-dev/pyfakefs/issues/1348).
        self.assertEqual(
            {t.test_path: t.expected_text_path for t in tests},
            {
                self.WPT_PREFIX + "/foo/t.window.html" + variant: baseline_path
                for variant, baseline_path in baseline_paths.items()
            },
        )

    # -----------------------------------------------------------------------
    # Warning: on-disk stub for a generated variant classifies as non-support
    # -----------------------------------------------------------------------
    def test_generated_variant_stub_non_support_warns(self):
        """A .any.js source generates a variant whose on-disk stub classifies
        as `crashtest` (not `support`) — both sides yield a Test for the
        same URL, so we warn about the duplicate dispatch. Concrete case:
        `.any.js` under /crashtests/ with the importer-emitted `.any.html`
        stub beside it. (Symmetric non-crashtest case verified below is
        silent.)"""
        self._write_wpt_file(
            "IndexedDB/crashtests/create-index.any.js",
            "// META: global=window,worker\ntest(() => {}, 'pass');\n",
        )
        # Importer-emitted stub — HTML inside /crashtests/ classifies as
        # `crashtest` per SourceFile.
        self._write_wpt_file(
            "IndexedDB/crashtests/create-index.any.html",
            "<!DOCTYPE html>\n<title>crash</title>\n",
        )
        with self.assertLogs(
            "webkitpy.layout_tests.controllers.layout_test_finder",
            level=logging.WARNING,
        ) as cm:
            tests = self._get_wpt_tests("IndexedDB/crashtests/create-index.any.js")
        # The .any.js still yields its testharness-classified Tests (bug
        # remains — this warning surfaces it, doesn't fix it).
        self.assertTrue(tests, "Expected .any.js to yield at least one Test")
        # Warning must mention both the source and the stub, and the type each
        # classifies as (the path alone also contains "crashtests").
        joined = "\n".join(cm.output)
        self.assertIn("create-index.any.js (as testharness)", joined)
        self.assertIn("create-index.any.html (as crashtest)", joined)

    def test_generated_variant_support_stub_silent(self):
        """The mirror case: a .any.js OUTSIDE /crashtests/ with an on-disk
        `.any.html` stub. The stub classifies as `support`, so no dup is
        possible and the warning must NOT fire."""
        self._write_wpt_file(
            "foo/basic.any.js",
            "// META: global=window,worker\ntest(() => {}, 'pass');\n",
        )
        # Non-crashtest importer stub — classifies as `support`.
        self._write_wpt_file(
            "foo/basic.any.html",
            "<!DOCTYPE html>\n<title>basic</title>\n",
        )
        logger = logging.getLogger(
            "webkitpy.layout_tests.controllers.layout_test_finder"
        )
        # Suppress "no logs" AssertionError from assertNoLogs (only 3.10+);
        # instead capture at WARNING+ and assert nothing about the stub
        # was emitted.
        with self.assertLogs(logger, level=logging.DEBUG) as cm:
            logger.debug("sentinel")  # ensure the context has at least one record
            tests = self._get_wpt_tests("foo/basic.any.js")
        warnings = [r for r in cm.records if r.levelno >= logging.WARNING]
        stub_warnings = [
            r for r in warnings if "Duplicate WPT Test dispatch" in r.getMessage()
        ]
        self.assertEqual(
            stub_warnings, [],
            f"Expected no dup-dispatch warnings, got: "
            f"{[r.getMessage() for r in stub_warnings]}",
        )
        # Sanity: the .any.js still produces its expected variants.
        test_paths = {t.test_path for t in tests}
        self.assertIn(self.WPT_PREFIX + "/foo/basic.any.html", test_paths)
        self.assertIn(self.WPT_PREFIX + "/foo/basic.any.worker.html", test_paths)

class WptTestsForPathLinuxTests(PyFakefsLinuxTestCaseMixin, WptTestsForPathTestsBase, unittest.TestCase):
    pass


class WptTestsForPathWindowsTests(PyFakefsWindowsTestCaseMixin, WptTestsForPathTestsBase, unittest.TestCase):
    pass


class WptTestsForPathMacOSTests(PyFakefsMacOSTestCaseMixin, WptTestsForPathTestsBase, unittest.TestCase):
    pass


class AllBaselinesForTestTestsBase(object):
    def setUp(self):
        self.setUpPyfakefs()
        host = MockHost(create_stub_repository_files=True, filesystem=FileSystem())
        add_unit_tests_to_mock_filesystem(host.filesystem)
        self.port = TestPort(host)
        self._filesystem = self.port.host.filesystem
        self.layout_tests_dir = self.port.layout_tests_dir()

    def tearDown(self):
        self.port = None
        self._filesystem = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self._filesystem.sep, expected_sep)

    def _make_finder(self, baseline_search_paths, all_baseline_search_paths=None):
        return LayoutTestFinder(
            self._filesystem,
            self.layout_tests_dir,
            baseline_search_paths,
            all_baseline_search_paths=all_baseline_search_paths,
        )

    def test_all_baselines_found_in_multiple_platform_dirs(self):
        """Baseline files in multiple platform dirs are all returned."""
        fs = self._filesystem
        platform_a = self._filesystem.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        platform_b = self._filesystem.join(self.layout_tests_dir, 'platform', 'test-mac-snowleopard')
        fs.maybe_make_directory(fs.join(platform_a, 'fast'))
        fs.maybe_make_directory(fs.join(platform_b, 'fast'))
        fs.write_binary_file(fs.join(platform_a, 'fast', 'foo-expected.txt'), b'a')
        fs.write_binary_file(fs.join(platform_b, 'fast', 'foo-expected.txt'), b'b')

        finder = self._make_finder(
            baseline_search_paths=[platform_a],
            all_baseline_search_paths=[platform_a, platform_b],
        )
        result = finder.all_baselines_for_test('fast/foo.html', '.txt')
        self.assertEqual(result, [
            (platform_a, 'fast/foo-expected.txt'),
            (platform_b, 'fast/foo-expected.txt'),
        ])

    def test_all_baselines_not_found_returns_none_platform_dir(self):
        """When no baseline file exists, returns [(None, baseline_filename)]."""
        finder = self._make_finder(
            baseline_search_paths=self.port.baseline_search_path(),
        )
        result = finder.all_baselines_for_test('fast/nonexistent.html', '.txt')
        self.assertEqual(result, [(None, 'fast/nonexistent-expected.txt')])

    def test_all_baselines_defaults_to_baseline_search_paths(self):
        """When all_baseline_search_paths is None, defaults to baseline_search_paths."""
        finder = self._make_finder(
            baseline_search_paths=self.port.baseline_search_path(),
        )
        self.assertEqual(finder.all_baseline_search_paths, finder.baseline_search_paths)

    def test_all_baselines_variant_sanitization(self):
        """Variant in test_name is sanitized in the baseline filename."""
        fs = self._filesystem
        platform_dir = self._filesystem.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        fs.maybe_make_directory(fs.join(platform_dir, 'fast'))
        # ?foo=bar variant: variant[1:] = 'foo=bar', sanitized with [|* <>:/%] -> 'foo=bar'
        # so baseline filename is 'fast/foo_foo=bar-expected.txt'
        baseline_name = 'fast/foo_foo=bar-expected.txt'
        fs.write_binary_file(
            fs.join(platform_dir, 'fast', 'foo_foo=bar-expected.txt'), b'x'
        )

        finder = self._make_finder(
            baseline_search_paths=[platform_dir],
            all_baseline_search_paths=[platform_dir],
        )
        result = finder.all_baselines_for_test('fast/foo.html?foo=bar', '.txt')
        self.assertEqual(result, [
            (platform_dir, baseline_name),
        ])


    def test_all_baselines_percent_encoded_variant(self):
        """A variant that was percent-encoded at discovery is sanitized to '_'
        in the baseline's filename: '?a%20b' -> 'a_20b'."""
        fs = self._filesystem
        platform_dir = fs.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        fs.maybe_make_directory(fs.join(platform_dir, 'fast'))
        fs.write_binary_file(fs.join(platform_dir, 'fast', 'foo_a_20b-expected.txt'), b'x')

        finder = self._make_finder(baseline_search_paths=[platform_dir], all_baseline_search_paths=[platform_dir])
        self.assertEqual(
            finder.all_baselines_for_test('fast/foo.html?a%20b', '.txt'),
            [(platform_dir, 'fast/foo_a_20b-expected.txt')],
        )
        # The unvarianted baseline is a different file.
        self.assertEqual(
            finder.all_baselines_for_test('fast/foo.html', '.txt'),
            [(None, 'fast/foo-expected.txt')],
        )

    def test_all_baselines_includes_the_generic_directory_last(self):
        fs = self._filesystem
        platform_dir = fs.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        fs.maybe_make_directory(fs.join(platform_dir, 'fast'))
        fs.maybe_make_directory(fs.join(self.layout_tests_dir, 'fast'))
        fs.write_binary_file(fs.join(platform_dir, 'fast', 'foo-expected.txt'), b'a')
        fs.write_binary_file(fs.join(self.layout_tests_dir, 'fast', 'foo-expected.txt'), b'b')

        finder = self._make_finder(baseline_search_paths=[platform_dir], all_baseline_search_paths=[platform_dir])
        self.assertEqual(
            finder.all_baselines_for_test('fast/foo.html', '.txt'),
            [(platform_dir, 'fast/foo-expected.txt'), (self.layout_tests_dir, 'fast/foo-expected.txt')],
        )

    def test_all_baseline_search_paths_are_normalized_through_symlinks(self):
        if self.fs_os == OSType.WINDOWS:
            self.skipTest("pyfakefs doesn't resolve symlinks in realpath() under Windows emulation")
        fs = self._filesystem
        real_dir = fs.join(self.layout_tests_dir, 'platform', 'real-platform')
        link_dir = fs.join(self.layout_tests_dir, 'platform', 'linked-platform')
        fs.maybe_make_directory(fs.join(real_dir, 'fast'))
        os.symlink(real_dir, link_dir)
        fs.write_binary_file(fs.join(real_dir, 'fast', 'foo-expected.txt'), b'a')

        finder = self._make_finder(baseline_search_paths=[real_dir], all_baseline_search_paths=[link_dir])
        self.assertEqual(finder.all_baseline_search_paths, [real_dir])
        # The reported directory is the real one, so callers can compare it
        # against paths they normalize the same way.
        self.assertEqual(
            finder.all_baselines_for_test('fast/foo.html', '.txt'),
            [(real_dir, 'fast/foo-expected.txt')],
        )


class AllBaselinesForTestLinuxTests(PyFakefsLinuxTestCaseMixin, AllBaselinesForTestTestsBase, unittest.TestCase):
    pass


class AllBaselinesForTestWindowsTests(PyFakefsWindowsTestCaseMixin, AllBaselinesForTestTestsBase, unittest.TestCase):
    pass


class AllBaselinesForTestMacOSTests(PyFakefsMacOSTestCaseMixin, AllBaselinesForTestTestsBase, unittest.TestCase):
    pass


class BaselinesForTestTestsBase(object):
    def setUp(self):
        self.setUpPyfakefs()
        host = MockHost(create_stub_repository_files=True, filesystem=FileSystem())
        add_unit_tests_to_mock_filesystem(host.filesystem)
        self.port = TestPort(host)
        self._filesystem = self.port.host.filesystem
        self.layout_tests_dir = self.port.layout_tests_dir()

    def tearDown(self):
        self.port = None
        self._filesystem = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self._filesystem.sep, expected_sep)

    def _make_finder(self, baseline_search_paths):
        return LayoutTestFinder(
            self._filesystem,
            self.layout_tests_dir,
            baseline_search_paths,
        )

    def test_baselines_found_in_first_platform_dir(self):
        """Baseline found in first matching platform dir is returned (early exit)."""
        fs = self._filesystem
        platform_a = fs.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        platform_b = fs.join(self.layout_tests_dir, 'platform', 'test-mac-snowleopard')
        fs.maybe_make_directory(fs.join(platform_a, 'fast'))
        fs.maybe_make_directory(fs.join(platform_b, 'fast'))
        fs.write_binary_file(fs.join(platform_a, 'fast', 'foo-expected.txt'), b'a')
        fs.write_binary_file(fs.join(platform_b, 'fast', 'foo-expected.txt'), b'b')

        finder = self._make_finder(baseline_search_paths=[platform_a, platform_b])
        result = finder.baselines_for_test('fast/foo.html', '.txt')
        # Only the first match is returned.
        self.assertEqual(result, [(platform_a, 'fast/foo-expected.txt')])

    def test_baselines_not_found_returns_none_platform_dir(self):
        """When no baseline file exists, returns [(None, baseline_filename)]."""
        finder = self._make_finder(
            baseline_search_paths=self.port.baseline_search_path(),
        )
        result = finder.baselines_for_test('fast/nonexistent.html', '.txt')
        self.assertEqual(result, [(None, 'fast/nonexistent-expected.txt')])

    def test_baselines_fall_back_to_the_generic_directory(self):
        fs = self._filesystem
        platform_dir = fs.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        fs.maybe_make_directory(fs.join(platform_dir, 'fast'))
        fs.maybe_make_directory(fs.join(self.layout_tests_dir, 'fast'))
        fs.write_binary_file(fs.join(self.layout_tests_dir, 'fast', 'foo-expected.png'), b'b')

        finder = self._make_finder(baseline_search_paths=[platform_dir])
        self.assertEqual(
            finder.baselines_for_test('fast/foo.html', '.png'),
            [(self.layout_tests_dir, 'fast/foo-expected.png')],
        )

    def test_baselines_with_variant(self):
        fs = self._filesystem
        platform_dir = fs.join(self.layout_tests_dir, 'platform', 'test-mac-leopard')
        fs.maybe_make_directory(fs.join(platform_dir, 'fast'))
        fs.write_binary_file(fs.join(platform_dir, 'fast', 'foo_a_20b-expected.txt'), b'a')

        finder = self._make_finder(baseline_search_paths=[platform_dir])
        self.assertEqual(
            finder.baselines_for_test('fast/foo.html?a%20b', '.txt'),
            [(platform_dir, 'fast/foo_a_20b-expected.txt')],
        )


class BaselinesForTestLinuxTests(PyFakefsLinuxTestCaseMixin, BaselinesForTestTestsBase, unittest.TestCase):
    pass


class BaselinesForTestWindowsTests(PyFakefsWindowsTestCaseMixin, BaselinesForTestTestsBase, unittest.TestCase):
    pass


class BaselinesForTestMacOSTests(PyFakefsMacOSTestCaseMixin, BaselinesForTestTestsBase, unittest.TestCase):
    pass
