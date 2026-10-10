# Copyright (c) 2015, Canon Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/gcor other materials provided with the distribution.
# 3.  Neither the name of Canon Inc. nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import unittest

from pyfakefs.fake_filesystem import OSType

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.fakefs_testcase import (
    PyFakefsLinuxTestCaseMixin,
    PyFakefsMacOSTestCaseMixin,
    PyFakefsWindowsTestCaseMixin,
)
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import (
    LayoutTestFinder,
    _is_reference_html_file,
)
from webkitpy.layout_tests.models.server_routing import ServerType
from webkitpy.layout_tests.models.test import Reference, Test, _file_path_sort_key
from webkitpy.port.test import (
    TestPort,
    add_unit_tests_to_mock_filesystem,
)


class IsReferenceHtmlFileTests(unittest.TestCase):
    def test_is_reference_html_file(self):
        filesystem = MockFileSystem()
        self.assertTrue(_is_reference_html_file(filesystem, '', 'foo-expected.html'))
        self.assertTrue(_is_reference_html_file(filesystem, '', 'foo-expected-mismatch.xml'))
        self.assertTrue(_is_reference_html_file(filesystem, '', 'foo-ref.xhtml'))
        self.assertTrue(_is_reference_html_file(filesystem, '', 'foo-notref.svg'))
        self.assertFalse(_is_reference_html_file(filesystem, '', 'foo.html'))
        self.assertFalse(_is_reference_html_file(filesystem, '', 'foo-expected.txt'))
        self.assertFalse(_is_reference_html_file(filesystem, '', 'foo-expected.shtml'))
        self.assertFalse(_is_reference_html_file(filesystem, '', 'foo-expected.php'))
        self.assertFalse(_is_reference_html_file(filesystem, '', 'foo-expected.mht'))


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
        self.finder = LayoutTestFinder(self.port, None)

    def tearDown(self):
        self.port = None
        self.finder = None

    def test_emulated_os(self):
        self.assertEqual(self.fs.os, self.fs_os)
        expected_sep = "\\" if self.fs_os == OSType.WINDOWS else "/"
        self.assertEqual(self.port.host.filesystem.sep, expected_sep)

    def test_find_no_paths_specified(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([])]
        self.assertNotEqual(tests, [])

    def test_find_no_paths_sorted(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([])]
        sorted_tests = sorted(tests, key=_file_path_sort_key)
        self.assertEqual(tests, sorted_tests)

    def test_find_all_no_paths(self):
        finder = self.finder
        empty_tests = [t.test_path for t in finder.find_tests_by_path([])]
        star_tests = [t.test_path for t in finder.find_tests_by_path(['*'])]
        self.assertEqual(empty_tests, star_tests)

    def test_includes_other_platforms(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([])]
        self.assertEqual(self.port.name(), 'test-mac-leopard')
        self.assertIn('platform/test-snow-leopard/websocket/test.html', tests)

    def test_includes_other_platforms_fallback(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([]) if t.test_path.endswith("/http/test.html")]
        self.assertEqual(self.port.name(), "test-mac-leopard")
        self.assertEqual(
            tests,
            [
                "platform/test-mac-leopard/http/test.html",
                "platform/test-snow-leopard/http/test.html",
                "platform/test-win-7sp0/http/test.html",
            ],
        )

    def test_find_one_test(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/image.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])

    def test_find_non_existent(self):
        finder = self.finder

        tests = [
            t.test_path
            for t in finder.find_tests_by_path(["this-path-does-not-actually-exist"])
        ]
        self.assertEqual(tests, [])

        tests = [
            t.test_path
            for t in finder.find_tests_by_path(["this/path/does/not/actually/exist"])
        ]
        self.assertEqual(tests, [])

    def test_find_redundant_single_dot(self):
        finder = self.finder
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(["./failures/expected/image.html"])
        ]
        self.assertEqual(tests, ["failures/expected/image.html"])

        tests = [
            t.test_path
            for t in finder.find_tests_by_path(["failures/./expected/image.html"])
        ]
        self.assertEqual(tests, ["failures/expected/image.html"])

    def test_find_redundant_double_dot(self):
        finder = self.finder
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(
                ["passes/../failures/expected/image.html"]
            )
        ]
        self.assertEqual(tests, ["failures/expected/image.html"])

    def test_find_platform(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['platform'])]
        self.assertEqual(
            tests,
            [
                'platform/test-mac-leopard/http/test.html',
                'platform/test-mac-leopard/overridden/test.html',
                'platform/test-mac-leopard/passes/platform-specific-test.html',
                'platform/test-mac-leopard/platform-specific-dir/platform-specific-test.html',
                'platform/test-snow-leopard/http/test.html',
                'platform/test-snow-leopard/websocket/test.html',
                'platform/test-win-7sp0/http/test.html',
                'platform/test-win-7sp0/overridden/test.html',
            ],
        )
        with_slash = [t.test_path for t in finder.find_tests_by_path(['platform/'])]
        self.assertEqual(tests, with_slash)
        with_star = [t.test_path for t in finder.find_tests_by_path(['platform/*'])]
        self.assertEqual(tests, with_star)

    def test_find_platform_self(self):
        finder = self.finder
        self.assertEqual(self.port.name(), 'test-mac-leopard')
        tests = [t.test_path for t in finder.find_tests_by_path(['platform/test-mac-leopard'])]
        self.assertEqual(
            tests,
            [
                'platform/test-mac-leopard/http/test.html',
                'platform/test-mac-leopard/overridden/test.html',
                'platform/test-mac-leopard/passes/platform-specific-test.html',
                'platform/test-mac-leopard/platform-specific-dir/platform-specific-test.html',
            ],
        )

    def test_find_platform_other(self):
        finder = self.finder
        self.assertEqual(self.port.name(), 'test-mac-leopard')
        tests = [t.test_path for t in finder.find_tests_by_path(['platform/test-snow-leopard'])]
        self.assertEqual(
            tests,
            [
                'platform/test-snow-leopard/http/test.html',
                'platform/test-snow-leopard/websocket/test.html',
            ],
        )

    def test_find_platform_specific_directory(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir'])]
        self.assertEqual(tests, [])
        with_slash = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir/'])]
        self.assertEqual(tests, with_slash)
        with_star = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir/*'])]
        self.assertEqual(tests, with_star)

    def test_find_file_excludes_platform_specific(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['http/test.html'])]
        self.assertEqual(tests, [])

    def test_find_directory_excludes_platform_specific(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['http'])]
        self.assertEqual(
            tests,
            [
                'http/tests/passes/image.html',
                'http/tests/passes/text.html',
                'http/tests/ssl/text.html',
                'http/tests/websocket/passes/text.html',
            ],
        )

    def test_find_overridden(self):
        finder = self.finder
        self.assertEqual(self.port.name(), "test-mac-leopard")
        tests = [
            t.test_path for t in finder.find_tests_by_path(["overridden/test.html"])
        ]
        self.assertEqual(
            tests,
            [
                "overridden/test.html",
            ],
        )

    def test_find_overridden_default(self):
        finder = self.finder
        self.assertEqual(self.port.name(), "test-mac-leopard")
        tests = [
            t.test_path
            for t in finder.find_tests_by_path([])
            if t.test_path.endswith("overridden/test.html")
        ]
        self.assertEqual(
            tests,
            [
                "overridden/test.html",
                "platform/test-mac-leopard/overridden/test.html",
                "platform/test-win-7sp0/overridden/test.html",
            ],
        )

    def test_find_overridden_platform_self(self):
        finder = self.finder
        self.assertEqual(self.port.name(), "test-mac-leopard")
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(
                [
                    "overridden/test.html",
                    "platform/test-mac-leopard/overridden/test.html",
                ]
            )
        ]
        self.assertEqual(
            tests,
            [
                "overridden/test.html",
                "platform/test-mac-leopard/overridden/test.html",
            ],
        )

    def test_find_overridden_platform_other(self):
        finder = self.finder
        self.assertEqual(self.port.name(), "test-mac-leopard")
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(
                ["overridden/test.html", "platform/test-win-7sp0/overridden/test.html"]
            )
        ]
        self.assertEqual(
            tests,
            [
                "overridden/test.html",
                "platform/test-win-7sp0/overridden/test.html",
            ],
        )

    def test_find_glob(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/im*'])]
        self.assertEqual(
            tests, ['failures/expected/image.html', 'failures/expected/image_checksum.html']
        )

    def test_find_glob_b(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/i?age.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])

    def test_find_glob_b2(self):
        finder = self.finder
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(["failures/expected/?mage.html"])
        ]
        self.assertEqual(tests, ["failures/expected/image.html"])

    def test_find_glob_c(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/i[m]age.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])

    def test_find_glob_d(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(["*/*.html"])]
        self.assertEqual(
            tests,
            [
                "overridden/test.html",
                "passes/args.html",
                "passes/audio-tolerance.html",
                "passes/audio.html",
                "passes/checksum_in_image.html",
                "passes/error.html",
                "passes/image.html",
                "passes/mismatch.html",
                "passes/platform_image.html",
                "passes/reftest.html",
                "passes/text.html",
                "variant/variant.any.html",
            ],
        )

    def test_find_glob_query(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/image.html?variant'])]
        self.assertEqual(tests, ['failures/expected/image.html?variant'])

    def test_find_glob_mixed_file_type_sorted(self):
        finder = self.finder
        # this should expand the *, sort the result, then recurse;
        # notably this therefore puts passes/skipped/ < passes/svgreftest.svg

        # test the short, simple case first
        tests = [t.test_path for t in finder.find_tests_by_path(['passes/s*'])]
        self.assertEqual(tests, ['passes/skipped/skip.html', 'passes/svgreftest.svg'])

        # then test the whole directory (check directories aren't just sorted first)
        tests = [t.test_path for t in finder.find_tests_by_path(['passes/*'])]
        self.assertEqual(
            tests,
            [
                'passes/args.html',
                'passes/audio-tolerance.html',
                'passes/audio.html',
                'passes/checksum_in_image.html',
                'passes/error.html',
                'passes/image.html',
                'passes/mismatch.html',
                'passes/phpreftest.php',
                'passes/platform_image.html',
                'passes/reftest.html',
                'passes/skipped/skip.html',
                'passes/svgreftest.svg',
                'passes/text.html',
                'passes/xhtreftest.xht',
            ],
        )

    def test_find_glob_directory(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['fai*ures/expected/image.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/*xpected/image.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])

    def test_find_glob_directory_b(self):
        finder = self.finder
        glob_tests = [t.test_path for t in finder.find_tests_by_path(['fai*ures'])]
        plain_tests = [t.test_path for t in finder.find_tests_by_path(['failures'])]
        self.assertEqual(plain_tests, glob_tests)
        glob_slash_tests = [t.test_path for t in finder.find_tests_by_path(['fai*ures/'])]
        self.assertEqual(plain_tests, glob_slash_tests)

    def test_find_glob_directory_e(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['h*tp'])]
        # contrast with test_find_directory_includes_platform_specific;
        # this excludes the platform/*/http/ tests
        self.assertEqual(
            tests,
            [
                'http/tests/passes/image.html',
                'http/tests/passes/text.html',
                'http/tests/ssl/text.html',
                'http/tests/websocket/passes/text.html',
            ],
        )

    def test_find_directory(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['websocket/tests'])]
        self.assertEqual(tests, ['websocket/tests/passes/text.html'])

    def test_find_directory_trailing_slash(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['websocket/tests/'])]
        self.assertEqual(tests, ['websocket/tests/passes/text.html'])

    def test_find_directory_star(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['websocket/tests/*'])]
        self.assertEqual(tests, ['websocket/tests/passes/text.html'])

    def test_preserves_order(self):
        tests_to_find = [
            'passes/audio.html',
            'failures/expected/text.html',
            'failures/expected/missing_text.html',
            'passes/args.html',
        ]
        finder = self.finder
        tests_found = [t.test_path for t in finder.find_tests_by_path(tests_to_find)]
        self.assertEqual(tests_to_find, tests_found)

    def test_preserves_order_multiple_times(self):
        tests_to_find = [
            'passes/args.html',
            'passes/audio.html',
            'passes/audio.html',
            'passes/args.html',
        ]
        finder = self.finder
        tests_found = [t.test_path for t in finder.find_tests_by_path(tests_to_find)]
        self.assertEqual(tests_to_find, tests_found)

    def test_preserves_order_variants(self):
        finder = self.finder
        tests_found = [
            t.test_path
            for t in finder.find_tests_by_path(
                ["passes/*.html?abc", "passes/*.html?xyz"]
            )
        ]
        self.assertEqual(
            tests_found,
            [
                "passes/args.html?abc",
                "passes/audio-tolerance.html?abc",
                "passes/audio.html?abc",
                "passes/checksum_in_image.html?abc",
                "passes/error.html?abc",
                "passes/image.html?abc",
                "passes/mismatch.html?abc",
                "passes/platform_image.html?abc",
                "passes/reftest.html?abc",
                "passes/text.html?abc",
                "passes/args.html?xyz",
                "passes/audio-tolerance.html?xyz",
                "passes/audio.html?xyz",
                "passes/checksum_in_image.html?xyz",
                "passes/error.html?xyz",
                "passes/image.html?xyz",
                "passes/mismatch.html?xyz",
                "passes/platform_image.html?xyz",
                "passes/reftest.html?xyz",
                "passes/text.html?xyz",
            ],
        )

    def test_find_template_variants_meta(self):
        # Variant discovery for WPT HTML tests is delegated to SourceFile,
        # which reads <meta name="variant"> elements. We exercise the
        # integration here with a small set of valid variants; the full
        # variant-parsing semantics (validation, percent-encoding, etc.) are
        # tested upstream.
        find_paths = ["imported/w3c/web-platform-tests/foo/variant_test.html"]
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/foo/variant_test.html"

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, """<!doctype html>
<script src="/resources/testharness.js"></script>
<meta name="variant" content="">
<meta name="variant" content="?1-10">
<meta name="variant" content="?11-20">
<meta name="variant" content="#a-m">
<meta name="variant" content="#n-z">
<meta name="variant" content="?1#a">
        """)
        tests_found = [t.test_path for t in finder.find_tests_by_path(find_paths)]
        self.assertEqual(
            [
                "imported/w3c/web-platform-tests/foo/variant_test.html",
                "imported/w3c/web-platform-tests/foo/variant_test.html?1-10",
                "imported/w3c/web-platform-tests/foo/variant_test.html?11-20",
                "imported/w3c/web-platform-tests/foo/variant_test.html#a-m",
                "imported/w3c/web-platform-tests/foo/variant_test.html#n-z",
                "imported/w3c/web-platform-tests/foo/variant_test.html?1#a",
            ],
            tests_found,
        )

    def test_find_template_variants_meta_passed_variants(self):
        # Variants requested explicitly via the path argument should filter the
        # variants produced by SourceFile.
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/foo/variant_test.html"

        find_paths = [
            path + "?a-b",
            path + "?c-d",
            path + "#m-n",
            path + "#o-p",
        ]

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(
            path,
            """<!doctype html>
<script src="/resources/testharness.js"></script>
<meta name=variant content="?a-b">
<meta name=variant content="?c-d">
<meta name=variant content="#m-n">
<meta name=variant content="#o-p">
        """,
        )
        tests_found = sorted(t.test_path for t in finder.find_tests_by_path(find_paths))
        self.assertEqual(
            ['imported/w3c/web-platform-tests/foo/variant_test.html#m-n',
             'imported/w3c/web-platform-tests/foo/variant_test.html#o-p',
             'imported/w3c/web-platform-tests/foo/variant_test.html?a-b',
             'imported/w3c/web-platform-tests/foo/variant_test.html?c-d'],
            tests_found,
        )


    def test_find_template_variants_any_js(self):
        """Test that .any.js source files are processed and produce multiple test URLs,
        one per generated basename (.any.html, .any.worker.html) times variant, regardless
        of whether an on-disk importer stub exists for a given generated basename."""
        find_paths = ["imported/w3c/web-platform-tests/foo/variant_test.any.js"]
        finder = self.finder

        js_path = finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/foo/variant_test.any.js"
        html_path = finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/foo/variant_test.any.html"

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(js_path))
        # The .any.js source has variant metadata.
        finder._filesystem.write_text_file(js_path, """// META: variant=
// META: variant=?1-10
// META: variant=?11-20
""")
        # WebKit also writes a generated .html stub which should be skipped.
        finder._filesystem.write_text_file(html_path, "<!-- generated stub -->")

        tests_found = sorted(t.test_path for t in finder.find_tests_by_path(find_paths))
        self.assertEqual(
            [
                "imported/w3c/web-platform-tests/foo/variant_test.any.html",
                "imported/w3c/web-platform-tests/foo/variant_test.any.html?1-10",
                "imported/w3c/web-platform-tests/foo/variant_test.any.html?11-20",
                "imported/w3c/web-platform-tests/foo/variant_test.any.worker.html",
                "imported/w3c/web-platform-tests/foo/variant_test.any.worker.html?1-10",
                "imported/w3c/web-platform-tests/foo/variant_test.any.worker.html?11-20",
            ],
            tests_found,
        )

    def test_wpt_crash(self):
        tests_to_find = ["imported/w3c/web-platform-tests"]
        finder = self.finder
        tests_found = [
            t.test_path
            for t in finder.find_tests_by_path(tests_to_find)
            if t.is_crash_test
        ]
        self.assertEqual(
            tests_found,
            [
                "imported/w3c/web-platform-tests/crashtests/crash.html",
                "imported/w3c/web-platform-tests/crashtests/pass.html",
                "imported/w3c/web-platform-tests/crashtests/timeout.html",
                "imported/w3c/web-platform-tests/crashtests/dir/test.html",
                "imported/w3c/web-platform-tests/some/test-crash-crash.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.https.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.tentative.html",
                "imported/w3c/web-platform-tests/some/test-timeout-crash.html",
            ],
        )

    def test_wpt_crash_requires_wpt_directory_prefix(self):
        finder = self.finder
        fs = finder._filesystem
        fs.chdir(self.port.layout_tests_dir())
        path = "weird/imported/w3c/web-platform-tests/crashtests/crash.html"
        fs.maybe_make_directory(fs.dirname(path))
        fs.write_text_file(path, "")

        tests = list(finder.find_tests_by_path([path]))
        self.assertEqual([t.test_path for t in tests], [path])
        self.assertFalse(tests[0].is_crash_test)

    def test_preserves_order_directories(self):
        tests_to_find = ['http/tests/ssl', 'http/tests/passes']
        finder = self.finder
        tests_found = [t.test_path for t in finder.find_tests_by_path(tests_to_find)]
        self.assertEqual(
            tests_found,
            [
                'http/tests/ssl/text.html',
                'http/tests/passes/image.html',
                'http/tests/passes/text.html',
            ],
        )

    def test_preserves_order_mixed_file_type(self):
        tests_to_find = ['passes/skipped', 'passes/args.html']
        finder = self.finder
        tests_found = [t.test_path for t in finder.find_tests_by_path(tests_to_find)]
        self.assertEqual(tests_found, ['passes/skipped/skip.html', 'passes/args.html'])

    def test_preserves_order_mixed_file_type_b(self):
        tests_to_find = ['passes/args.html', 'passes/skipped']
        finder = self.finder
        tests_found = [t.test_path for t in finder.find_tests_by_path(tests_to_find)]
        self.assertEqual(tests_found, ['passes/args.html', 'passes/skipped/skip.html'])

    def test_find_directory_multiple_times(self):
        finder = self.finder
        tests = [
            t.test_path
            for t in finder.find_tests_by_path(['websocket/tests/', 'websocket/tests/'])
        ]
        self.assertEqual(
            tests, ['websocket/tests/passes/text.html', 'websocket/tests/passes/text.html']
        )

    def test_no_reference(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['passes/reftest-expected.html'])]
        self.assertEqual(tests, [])

    def test_glob_no_references(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['passes/*reftest*'])]
        self.assertEqual(
            tests,
            [
                'passes/phpreftest.php',
                'passes/reftest.html',
                'passes/svgreftest.svg',
                'passes/xhtreftest.xht',
            ],
        )

    def test_find_with_skipped_directories(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['userscripts'])]
        self.assertNotIn('userscripts/resources/iframe.html', tests)

    def test_find_with_skipped_directories_2(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['userscripts/resources'])]
        self.assertEqual(tests, [])
        tests = [t.test_path for t in finder.find_tests_by_path(['userscripts/resources/'])]
        self.assertEqual(tests, [])
        tests = [t.test_path for t in finder.find_tests_by_path(['userscripts/resources/*'])]
        self.assertEqual(tests, [])

    def test_order_directory_name(self):
        finder = self.finder
        fs = finder._filesystem

        fs.chdir(self.port.layout_tests_dir())

        dir_names = ["a", "b", "b0", "bA", "b-x", "by", "b_z", "c"]
        for d in dir_names:
            fs.maybe_make_directory(fs.join("order", d))
            fs.write_text_file(fs.join("order", d, "test.html"), "1")

        tests = [t.test_path for t in finder.find_tests_by_path(["order"])]
        sorted_tests = sorted(tests, key=_file_path_sort_key)
        self.assertEqual(tests, sorted_tests)
        self.assertEqual(
            tests,
            [
                "order/a/test.html",
                "order/b0/test.html",
                "order/b-x/test.html",
                "order/b/test.html",
                "order/bA/test.html",
                "order/b_z/test.html",
                "order/by/test.html",
                "order/c/test.html",
            ],
        )

    def test_is_test_file(self):
        finder = self.finder
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.html'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.shtml'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.svg'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'test-ref-test.html'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.py'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo.png'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected.html'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected.svg'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected.xht'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected-mismatch.html'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected-mismatch.svg'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-expected-mismatch.xhtml'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-ref.html'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-notref.html'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-notref.xht'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo-ref.xhtml'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'ref-foo.html'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'notref-foo.xhr'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'foo_wsh.py'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', '_wsh.py'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'boot.xml'))
        self.assertFalse(finder._is_test_file(finder._filesystem, '', 'root.xml'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo-boot.xml'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo-root.xml'))

    def test_is_w3c_resource_file(self):
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/imported/w3c/resources/resource-files.json"

        resource_files_obj = {
            "directories": [
                "web-platform-tests/common",
                "web-platform-tests/dom/nodes/Document-createElement-namespace-tests",
                "web-platform-tests/fonts",
                "web-platform-tests/html/browsers/browsing-the-web/navigating-across-documents/source/support",
                "web-platform-tests/html/browsers/browsing-the-web/unloading-documents/support",
                "web-platform-tests/html/browsers/history/the-history-interface/non-automated",
                "web-platform-tests/html/browsers/history/the-location-interface/non-automated",
                "web-platform-tests/images",
                "web-platform-tests/service-workers",
                "web-platform-tests/tools",
            ],
            "files": [
                "web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-block-defer-scripts-subframe.html",
                "web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-not-hang-scriptloader-subframe.html",
            ],
        }

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, json.dumps(resource_files_obj))

        self.assertTrue(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests",
                "wpt.py",
            )
        )

        self.assertFalse(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir() + "/imported/w3",
                "resource_file.html",
            )
        )

        self.assertFalse(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir() + "/imported/w3c",
                "resource_file.html",
            )
        )

        self.assertFalse(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir()
                + "/imported/w3c/web-platform-tests/XMLHttpRequest",
                "xmlhttprequest-sync-block-defer-scripts-subframe.html.html",
            )
        )

        self.assertTrue(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/common/subdir",
                "test.html",
            )
        )

        self.assertTrue(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir()
                + "/imported/w3c/web-platform-tests/XMLHttpRequest",
                "xmlhttprequest-sync-block-defer-scripts-subframe.html",
            )
        )

        self.assertTrue(
            finder._is_w3c_resource_file(
                finder._filesystem,
                finder._port.layout_tests_dir()
                + "/imported/w3c/web-platform-tests/dom/nodes/Document-createElement-namespace-tests",
                "test.html",
            )
        )

    def test_find_w3c_resource_file(self):
        fs = self.port._filesystem

        w3c_path = fs.join(self.port.layout_tests_dir(), "imported", "w3c")
        path = fs.join(w3c_path, "resources", "resource-files.json")

        resource_files_obj = {
            "directories": [
                "web-platform-tests/common",
                "web-platform-tests/crashtests/dir",
                "web-platform-tests/dom/nodes/Document-createElement-namespace-tests",
                "web-platform-tests/fonts",
                "web-platform-tests/html/browsers/browsing-the-web/navigating-across-documents/source/support",
                "web-platform-tests/html/browsers/browsing-the-web/unloading-documents/support",
                "web-platform-tests/html/browsers/history/the-history-interface/non-automated",
                "web-platform-tests/html/browsers/history/the-location-interface/non-automated",
                "web-platform-tests/images",
                "web-platform-tests/service-workers",
                "web-platform-tests/tools",
            ],
            "files": [
                "web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-block-defer-scripts-subframe.html",
                "web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-not-hang-scriptloader-subframe.html",
                "web-platform-tests/some/new.html",
            ],
        }

        fs.maybe_make_directory(fs.dirname(path))
        fs.write_text_file(path, json.dumps(resource_files_obj))

        # We need to create a new Finder after changing resource-files.json
        finder = LayoutTestFinder(self.port, None)

        for d in resource_files_obj["directories"]:
            path = fs.join(w3c_path, d)
            fs.maybe_make_directory(path)
            if not fs.exists(fs.join(path, "test.html")):
                fs.write_text_file(fs.join(path, "test.html"), "foo")

        for f in resource_files_obj["files"]:
            path = fs.join(w3c_path, f)
            fs.maybe_make_directory(fs.dirname(path))
            if not fs.exists(path):
                fs.write_text_file(path, "foo")

        tests = [t.test_path for t in finder.find_tests_by_path(["imported/w3c"])]
        self.assertEqual(
            tests,
            [
                "imported/w3c/web-platform-tests/crashtests/crash.html",
                "imported/w3c/web-platform-tests/crashtests/pass.html",
                "imported/w3c/web-platform-tests/crashtests/timeout.html",
                "imported/w3c/web-platform-tests/some/test-crash-crash.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.https.html",
                "imported/w3c/web-platform-tests/some/test-pass-crash.tentative.html",
                "imported/w3c/web-platform-tests/some/test-timeout-crash.html",
            ],
        )

    def test_no_w3c(self):
        fs = self.port._filesystem

        w3c_path = fs.join(self.port.layout_tests_dir(), "imported", "w3c")
        fs.rmtree(w3c_path)

        # fs.rmtree passes ignore_errors=True, so make sure it's deleted.
        self.assertFalse(fs.isdir(w3c_path))

        finder = LayoutTestFinder(self.port, None)
        tests = [t.test_path for t in finder.find_tests_by_path(["imported/w3c"])]
        self.assertEqual(tests, [])

    def test_missing_w3c_resource_file(self):
        fs = self.port._filesystem

        w3c_path = fs.join(self.port.layout_tests_dir(), "imported", "w3c")
        path = fs.join(w3c_path, "resources", "resource-files.json")
        fs.remove(path)

        with self.assertRaises(FileNotFoundError):
            finder = LayoutTestFinder(self.port, None)
            finder.find_tests_by_path(["imported/w3c"])

    def test_chooses_best_expectation(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.txt"), "a")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(), "foo", "test-expected.txt"
                    ),
                )
            ],
        )

        fs.maybe_make_directory("platform/test-mac-snowleopard/foo")
        fs.write_text_file(
            fs.join("platform", "test-mac-snowleopard", "foo", "test-expected.txt"), "b"
        )

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-snowleopard",
                        "foo",
                        "test-expected.txt",
                    ),
                )
            ],
        )

        fs.maybe_make_directory("platform/test-mac-leopard/foo")
        fs.write_text_file(
            fs.join("platform", "test-mac-leopard", "foo", "test-expected.txt"), "c"
        )

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "foo",
                        "test-expected.txt",
                    ),
                )
            ],
        )

    def test_is_http_test(self):
        finder = self.finder
        fs = finder._filesystem

        files = [
            "http/tests/foo.html",
            # http/tests/local/ still expects an HTTP server, just serves the
            # test itself from a different URL; it should still count as an
            # HTTP test.
            "http/tests/local/foo.html",
            # A path that merely contains "http/tests/" isn't itself an HTTP test.
            "some/http/tests/foo.html",
            # A directory inserted between "http" and "tests" isn't a match either.
            "http/some/tests/foo.html",
            # Near-miss on the first segment.
            "httpfoo/tests/foo.html",
            # Near-miss on the second segment that still shares a "test" prefix.
            "http/testing/foo.html",
            # Near-miss on the second segment: "tests" is a prefix of the
            # directory name, but there's no "/" boundary after it.
            "http/testsfoo/bar.html",
            # "http/" without any "tests" segment at all.
            "http/foo/bar.html",
        ]

        fs.chdir(self.port.layout_tests_dir())

        for f in files:
            fs.maybe_make_directory(fs.dirname(f))
            fs.write_text_file(f, "XXX")

        tests = finder.find_tests_by_path(files, with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(test_path="http/tests/foo.html", served_by=ServerType.HTTP),
                Test(test_path="http/tests/local/foo.html", served_by=ServerType.HTTP),
                Test(test_path="some/http/tests/foo.html", served_by=ServerType.FILE),
                Test(test_path="http/some/tests/foo.html", served_by=ServerType.FILE),
                Test(test_path="httpfoo/tests/foo.html", served_by=ServerType.FILE),
                Test(test_path="http/testing/foo.html", served_by=ServerType.FILE),
                Test(test_path="http/testsfoo/bar.html", served_by=ServerType.FILE),
                Test(test_path="http/foo/bar.html", served_by=ServerType.FILE),
            ],
        )

    def test_filename_and_directory_flags(self):
        finder = self.finder
        fs = finder._filesystem

        expected = [
            ("foo/a.h2.html", ServerType.FILE, {"h2"}),
            ("foo/b.https.html", ServerType.FILE, {"https"}),
            ("foo/c.sub.html", ServerType.FILE, {"sub"}),
            ("foo/d.h2.https.sub.html", ServerType.FILE, {"h2", "https", "sub"}),
            # Flags are matched case-insensitively.
            ("foo/e.HTTPS.html", ServerType.FILE, {"https"}),
            # A flag needs a "." on both sides: these are all plain names.
            ("foo/f-https.html", ServerType.FILE, set()),
            ("foo/https.html", ServerType.FILE, set()),
            ("foo/g.httpsfoo.html", ServerType.FILE, set()),
            # An HTTP test under ssl/ is served over HTTPS...
            ("http/tests/ssl/h.html", ServerType.HTTP, {"https"}),
            ("http/tests/security/ssl/i.html", ServerType.HTTP, {"https"}),
            ("http/tests/security/j.https.html", ServerType.HTTP, {"https"}),
            ("http/tests/security/k.html", ServerType.HTTP, set()),
            # ...but ssl/ outside an HTTP root has no such meaning.
            ("ssl/l.html", ServerType.FILE, set()),
            ("foo/ssl/m.html", ServerType.FILE, set()),
            # A directory merely containing "ssl" is not ssl/.
            ("http/tests/sslfoo/n.html", ServerType.HTTP, set()),
        ]

        fs.chdir(self.port.layout_tests_dir())
        for path, _, _ in expected:
            fs.maybe_make_directory(fs.dirname(path))
            fs.write_text_file(path, "XXX")

        tests = finder.find_tests_by_path([path for path, _, _ in expected], with_expectations=True)
        self.assertEqual(
            tests,
            [Test(test_path=path, served_by=served_by, flags=frozenset(flags)) for path, served_by, flags in expected],
        )

    def test_is_websocket_test(self):
        finder = self.finder
        fs = finder._filesystem

        files = [
            "websocket/tests/standalone.html",
            "http/tests/test.html",
            "http/tests/websocket/construct-in-detached-frame.html",
            "http/tests/security/mixedContent/websocket/insecure-websocket-in-iframe.html",
            "http/tests/security/contentSecurityPolicy/connect-src-star-secure-websocket-allowed.html",
            "imported/w3c/web-platform-tests/service-workers/service-worker/websocket.https.html",
            # Not under websocket/ or http/tests/ at all -- the substring
            # match with no directory-boundary requirement still fires here,
            # unlike origin/main's is_websocket_test, which is gated on
            # being under http/tests/ (or the "websocket/" directory itself).
            "fast/websocket-something.html",
        ]

        fs.chdir(self.port.layout_tests_dir())

        for f in files:
            fs.maybe_make_directory(fs.dirname(f))
            # Use testharness.js script so SourceFile classifies WPT files as
            # tests rather than support files.
            fs.write_text_file(f, '<script src="/resources/testharness.js"></script>')

        tests = finder.find_tests_by_path(
            files + ["http/tests/test.html?websocket"], with_expectations=True
        )
        self.assertEqual(
            tests,
            [
                # A bare "websocket/" directory outside http/tests/ is still
                # a websocket test, just not an HTTP one.
                Test(
                    test_path="websocket/tests/standalone.html",
                    served_by=ServerType.WEBSOCKET,
                ),
                Test(
                    test_path="http/tests/test.html",
                    served_by=ServerType.HTTP,
                ),
                Test(
                    test_path="http/tests/websocket/construct-in-detached-frame.html",
                    served_by=ServerType.HTTP | ServerType.WEBSOCKET,
                ),
                Test(
                    test_path="http/tests/security/mixedContent/websocket/insecure-websocket-in-iframe.html",
                    served_by=ServerType.HTTP | ServerType.WEBSOCKET,
                ),
                Test(
                    test_path="http/tests/security/contentSecurityPolicy/connect-src-star-secure-websocket-allowed.html",
                    served_by=ServerType.HTTP | ServerType.WEBSOCKET,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/service-workers/service-worker/websocket.https.html",
                    served_by=ServerType.WPT,
                    flags={'https'},
                ),
                Test(
                    test_path="fast/websocket-something.html",
                    served_by=ServerType.WEBSOCKET,
                ),
                Test(
                    test_path="http/tests/test.html?websocket",
                    served_by=ServerType.HTTP | ServerType.WEBSOCKET,
                ),
            ],
        )

    def test_is_wpt_test(self):
        finder = self.finder
        fs = finder._filesystem

        files = [
            "imported/w3c/web-platform-tests/some/fresh.html",
            "http/wpt/some/foo.html",
            # A path that merely contains the WPT dir isn't itself a WPT test.
            "some/imported/w3c/web-platform-tests/foo.html",
            # A directory inserted before the last segment isn't a match either.
            "imported/w3c/foo/web-platform-tests/text.html",
            # Near-miss on the last segment: it's a prefix of the directory
            # name, but there's no "/" boundary after it.
            "imported/w3c/web-platform-testsfoo/bar.html",
            # The WPT dir without its final segment at all.
            "imported/w3c/foo.html",
            # is_wpt_test also accepts the local "http/wpt/" dir; the same
            # anchoring and boundary checks apply to it independently.
            "some/http/wpt/foo.html",
            "http/wptfoo/bar.html",
        ]

        fs.chdir(self.port.layout_tests_dir())

        for f in files:
            fs.maybe_make_directory(fs.dirname(f))
            # Use testharness.js script so SourceFile classifies WPT files as
            # tests rather than support files.
            fs.write_text_file(f, '<script src="/resources/testharness.js"></script>')

        tests = finder.find_tests_by_path(files, with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="imported/w3c/web-platform-tests/some/fresh.html",
                    served_by=ServerType.WPT,
                ),
                Test(test_path="http/wpt/some/foo.html", served_by=ServerType.WPT),
                Test(
                    test_path="some/imported/w3c/web-platform-tests/foo.html",
                    served_by=ServerType.FILE,
                ),
                Test(
                    test_path="imported/w3c/foo/web-platform-tests/text.html",
                    served_by=ServerType.FILE,
                ),
                Test(
                    test_path="imported/w3c/web-platform-testsfoo/bar.html",
                    served_by=ServerType.FILE,
                ),
                Test(test_path="imported/w3c/foo.html", served_by=ServerType.FILE),
                Test(test_path="some/http/wpt/foo.html", served_by=ServerType.FILE),
                Test(test_path="http/wptfoo/bar.html", served_by=ServerType.FILE),
            ],
        )

    def test_is_wpt_crash_test(self):
        finder = self.finder
        fs = finder._filesystem

        files = [
            "fast/crashtests/test.html",
            "fast/css/end-of-buffer-crash.html",
            "imported/w3c/web-platform-tests/editing/run/empty-editable-crash.html",
            "imported/w3c/web-platform-tests/html/semantics/popovers/popover-hint-crash.tentative.html",
            "imported/w3c/web-platform-tests/mathml/crashtests/mtd-as-multicol.html",
        ]

        fs.chdir(self.port.layout_tests_dir())

        for f in files:
            fs.maybe_make_directory(fs.dirname(f))
            fs.write_text_file(f, "XXX")

        tests = finder.find_tests_by_path(files, with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="fast/crashtests/test.html",
                    served_by=ServerType.FILE,
                    is_crash_test=False,
                ),
                Test(
                    test_path="fast/css/end-of-buffer-crash.html",
                    served_by=ServerType.FILE,
                    is_crash_test=False,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/editing/run/empty-editable-crash.html",
                    served_by=ServerType.WPT,
                    is_crash_test=True,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/html/semantics/popovers/popover-hint-crash.tentative.html",
                    served_by=ServerType.WPT,
                    is_crash_test=True,
                    flags={'tentative'},
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/mathml/crashtests/mtd-as-multicol.html",
                    served_by=ServerType.WPT,
                    is_crash_test=True,
                ),
            ],
        )

    def test_overridden_expectations(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        # Sanity check we have the platform expectation
        self.assertTrue(
            fs.exists(
                "/test.checkout/LayoutTests/platform/test-mac-leopard/overridden/test-expected.txt"
            )
        )

        # Both generic and platform tests give the platform expectation
        tests = finder.find_tests_by_path(
            ["overridden/test.html", "platform/test-mac-leopard/overridden/test.html"],
            with_expectations=True,
        )
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.txt",
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
                Test(
                    test_path="platform/test-mac-leopard/overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.txt",
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
            ],
        )

        # Remove the platform expectation
        fs.remove(
            "/test.checkout/LayoutTests/platform/test-mac-leopard/overridden/test-expected.txt"
        )

        # Now the generic gives the generic expectation, and the platform None
        tests = finder.find_tests_by_path(
            ["overridden/test.html", "platform/test-mac-leopard/overridden/test.html"],
            with_expectations=True,
        )
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(), "overridden", "test-expected.txt"
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
                Test(
                    test_path="platform/test-mac-leopard/overridden/test.html",
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
            ],
        )

        # Add a platform-in-a-platform
        fs.maybe_make_directory(
            "/test.checkout/LayoutTests/platform/test-mac-leopard/platform/test-mac-leopard/overridden"
        )
        fs.write_text_file(
            "/test.checkout/LayoutTests/platform/test-mac-leopard/platform/test-mac-leopard/overridden/test-expected.txt",
            "foo",
        )

        # Now the platform gives the platform-in-a-platform
        tests = finder.find_tests_by_path(
            ["overridden/test.html", "platform/test-mac-leopard/overridden/test.html"],
            with_expectations=True,
        )
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(), "overridden", "test-expected.txt"
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
                Test(
                    test_path="platform/test-mac-leopard/overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.txt",
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
            ],
        )

    def test_overridden_reference_expectation(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.html"), "a")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=(
                        Reference(
                            relation="==",
                            path=fs.join(
                                self.port.layout_tests_dir(),
                                "foo",
                                "test-expected.html",
                            ),
                        ),
                    ),
                )
            ],
        )

        fs.maybe_make_directory("platform/test-mac-snowleopard/foo")
        fs.write_text_file(
            "platform/test-mac-snowleopard/foo/test-expected-mismatch.html", "b"
        )

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=(
                        Reference(
                            relation="!=",
                            path=fs.join(
                                self.port.layout_tests_dir(),
                                "platform",
                                "test-mac-snowleopard",
                                "foo",
                                "test-expected-mismatch.html",
                            ),
                        ),
                    ),
                )
            ],
        )

    def test_reference_expectation_order(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.html"), "a")
        fs.write_text_file(fs.join("foo", "test-expected.xhtml"), "b")
        fs.write_text_file(fs.join("foo", "test-expected.xht"), "c")
        fs.write_text_file(fs.join("foo", "test-expected.svg"), "d")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.html"), "e")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.xhtml"), "f")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.xht"), "g")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.svg"), "h")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)

        expected_reference_files = tuple(
            Reference(relation=reftype, path=fs.join(self.port.layout_tests_dir(), "foo", refpath))
            for reftype, refpath in (
                ("==", "test-expected.html"),
                ("==", "test-expected.svg"),
                ("==", "test-expected.xht"),
                ("==", "test-expected.xhtml"),
                ("!=", "test-expected-mismatch.html"),
                ("!=", "test-expected-mismatch.svg"),
                ("!=", "test-expected-mismatch.xht"),
                ("!=", "test-expected-mismatch.xhtml"),
            )
        )

        self.assertEqual(
            tests[0].reference_files,
            expected_reference_files,
        )

        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=expected_reference_files,
                )
            ],
        )

    def test_reference_expectation_order_html_xhtml(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.html"), "a")
        fs.write_text_file(fs.join("foo", "test-expected.xhtml"), "c")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.html"), "e")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.xhtml"), "g")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)

        expected_reference_files = tuple(
            Reference(relation=reftype, path=fs.join(self.port.layout_tests_dir(), "foo", refpath))
            for reftype, refpath in (
                ("==", "test-expected.html"),
                ("==", "test-expected.xhtml"),
                ("!=", "test-expected-mismatch.html"),
                ("!=", "test-expected-mismatch.xhtml"),
            )
        )

        self.assertEqual(
            tests[0].reference_files,
            expected_reference_files,
        )

        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=expected_reference_files,
                )
            ],
        )

    def test_reference_expectation_order_html_xht(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.html"), "a")
        fs.write_text_file(fs.join("foo", "test-expected.xht"), "c")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.html"), "e")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.xht"), "g")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)

        expected_reference_files = tuple(
            Reference(relation=reftype, path=fs.join(self.port.layout_tests_dir(), "foo", refpath))
            for reftype, refpath in (
                ("==", "test-expected.html"),
                ("==", "test-expected.xht"),
                ("!=", "test-expected-mismatch.html"),
                ("!=", "test-expected-mismatch.xht"),
            )
        )

        self.assertEqual(
            tests[0].reference_files,
            expected_reference_files,
        )

        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=expected_reference_files,
                )
            ],
        )

    def test_reference_expectation_order_html_svg(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(fs.join("foo", "test-expected.html"), "a")
        fs.write_text_file(fs.join("foo", "test-expected.svg"), "d")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.html"), "e")
        fs.write_text_file(fs.join("foo", "test-expected-mismatch.svg"), "h")

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)

        expected_reference_files = tuple(
            Reference(relation=reftype, path=fs.join(self.port.layout_tests_dir(), "foo", refpath))
            for reftype, refpath in (
                ("==", "test-expected.html"),
                ("==", "test-expected.svg"),
                ("!=", "test-expected-mismatch.html"),
                ("!=", "test-expected-mismatch.svg"),
            )
        )

        self.assertEqual(
            tests[0].reference_files,
            expected_reference_files,
        )

        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    reference_files=expected_reference_files,
                )
            ],
        )

    def test_webarchive_expectation_order(self):
        finder = self.finder
        fs = finder._filesystem

        self.assertEqual(self.port.name(), "test-mac-leopard")

        fs.chdir(self.port.layout_tests_dir())
        fs.maybe_make_directory("foo")
        fs.maybe_make_directory("platform/test-mac-leopard/foo")
        fs.maybe_make_directory("platform/test-mac-snowleopard/foo")

        fs.write_text_file(fs.join("foo", "test.html"), "test")
        fs.write_text_file(
            fs.join("platform/test-mac-leopard/foo", "test-expected.webarchive"), "b"
        )

        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "foo",
                        "test-expected.webarchive",
                    ),
                )
            ],
        )

        # An -expected.txt wins over an -expected.webarchive, even generic.
        fs.write_text_file(fs.join("foo", "test-expected.txt"), "a")
        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(), "foo", "test-expected.txt"
                    ),
                )
            ],
        )

        # An -expected.txt wins over an -expected.webarchive, even further up the
        # baseline search path.
        fs.write_text_file(
            fs.join("platform/test-mac-snowleopard/foo", "test-expected.txt"), "a"
        )
        tests = finder.find_tests_by_path(["foo/test.html"], with_expectations=True)
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="foo/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-snowleopard",
                        "foo",
                        "test-expected.txt",
                    ),
                )
            ],
        )

    def test_abspath(self):
        finder = self.finder
        fs = finder._filesystem

        tests = finder.find_tests_by_path(
            [
                fs.abspath(
                    fs.join(self.port.layout_tests_dir(), "overridden", "test.html")
                )
            ],
            with_expectations=True,
        )
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="overridden/test.html",
                    expected_text_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.txt",
                    ),
                    expected_image_path=fs.join(
                        self.port.layout_tests_dir(),
                        "platform",
                        "test-mac-leopard",
                        "overridden",
                        "test-expected.png",
                    ),
                ),
            ],
        )

    def test_tempdir_abspath(self):
        # This is really just representative of something _not_ in the layout
        # test directory specified with an abspath.
        finder = self.finder
        fs = finder._filesystem

        with fs.mkdtemp() as path:
            test_path = fs.join(path, "test.html")
            fs.write_text_file(test_path, "test")
            expected_path = fs.join(path, "test-expected.txt")
            fs.write_text_file(expected_path, "A")

            tests = finder.find_tests_by_path([test_path], with_expectations=True)
            self.assertEqual(
                tests,
                [
                    Test(
                        test_path=test_path,
                        expected_text_path=fs.join(
                            path,
                            "test-expected.txt",
                        ),
                    )
                ],
            )

    def test_tempdir_relpath(self):
        # This is really just representative of something _not_ in the layout
        # test directory specified with an relpath.
        finder = self.finder
        fs = finder._filesystem

        with fs.mkdtemp() as path:
            test_path = fs.join(path, "test.html")
            fs.write_text_file(test_path, "test")
            expected_path = fs.join(path, "test-expected.txt")
            fs.write_text_file(expected_path, "A")

            rel_path = fs.relpath(path, self.port.layout_tests_dir())
            rel_test_path = fs.join(rel_path, "test.html")

            # Sanity check
            self.assertTrue(fs.isabs(test_path))
            self.assertFalse(fs.isabs(rel_test_path))

            tests = finder.find_tests_by_path([rel_test_path], with_expectations=True)

            self.assertEqual(
                tests,
                [
                    Test(
                        test_path=test_path,
                        expected_text_path=fs.join(
                            path,
                            "test-expected.txt",
                        ),
                    )
                ],
            )

    def test_dir_glob_matches_file(self):
        finder = self.finder
        fs = finder._filesystem

        # The first `*` here will match several files, which must be ignored.
        tests = [t.test_path for t in finder.find_tests_by_path(["passes/*/*"])]
        self.assertEqual(
            tests,
            ["passes/skipped/skip.html"],
        )


class LayoutTestFinderLinuxTests(PyFakefsLinuxTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderWindowsTests(PyFakefsWindowsTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass


class LayoutTestFinderMacOSTests(PyFakefsMacOSTestCaseMixin, LayoutTestFinderTestsBase, unittest.TestCase):
    pass
