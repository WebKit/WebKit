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

from pyfakefs.fake_filesystem_unittest import TestCaseMixin

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import (
    LayoutTestFinder,
    _is_reference_html_file,
)
from webkitpy.layout_tests.models.test import Test
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
        self.finder = LayoutTestFinder(self.port, None)

    def tearDown(self):
        self.port = None
        self.finder = None

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

    def test_find_no_paths_specified(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([])]
        self.assertNotEqual(tests, [])

    def test_find_no_paths_sorted(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path([])]
        sorted_tests = sorted(tests, key=self.port.test_key)
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
        find_paths = ["web-platform-tests"]
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/web-platform-tests/variant_test.html"

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, """<!doctype html>
<meta name="variant" content="">
<meta name="variant" content="?">
<meta name="variant" content="?1-10">
<meta name="variant" content="?11-20">
<meta name="variant" content="#">
<meta name="variant" content="#a-m">
<meta name="variant" content="#n-z">
<meta name="variant" content="?#">
<meta name="variant" content="?1#a">
<meta name="variant" content="nonsense">
<meta name=variant content="?only open()ed, not aborted">
<meta name=variant content="?aborted immediately after send()">
<meta name=variant content="?call abort() after TIME_NORMAL_LOAD">
        """)
        tests_found = [t.test_path for t in finder.find_tests_by_path(find_paths)]
        self.assertEqual(
            [
                "web-platform-tests/variant_test.html",
                "web-platform-tests/variant_test.html?1-10",
                "web-platform-tests/variant_test.html?11-20",
                "web-platform-tests/variant_test.html#a-m",
                "web-platform-tests/variant_test.html#n-z",
                "web-platform-tests/variant_test.html?1#a",
                "web-platform-tests/variant_test.html?only%20open()ed,%20not%20aborted",
                "web-platform-tests/variant_test.html?aborted%20immediately%20after%20send()",
                "web-platform-tests/variant_test.html?call%20abort()%20after%20TIME_NORMAL_LOAD",
            ],
            tests_found,
        )

    def test_find_template_variants_meta_passed_variants(self):
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/web-platform-tests/variant_test.html"

        find_paths = [
            path + "?a b",
            path + "?c%20d",
            path + "#m n",
            path + "#o%20p",
            path + "?e%20f#q%20r",
        ]

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(
            path,
            """<!doctype html>
<meta name=variant content="?a b">
<meta name=variant content="?c%20d">
<meta name=variant content="#m n">
<meta name=variant content="#o%20p">
<meta name=variant content="?e f#q r">
        """,
        )
        tests_found = [t.test_path for t in finder.find_tests_by_path(find_paths)]
        self.assertEqual(
            ['web-platform-tests/variant_test.html?a%20b',
             'web-platform-tests/variant_test.html?c%20d',
             'web-platform-tests/variant_test.html#m%20n',
             'web-platform-tests/variant_test.html#o%20p',
             'web-platform-tests/variant_test.html?e%20f#q%20r'],
            tests_found,
        )


    def test_find_template_variants_comment(self):
        find_paths = ["web-platform-tests"]
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/web-platform-tests/variant_test.any.html"

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, """<!-- This file is required for WebKit test infrastructure to run the templated test -->
<!-- META: variant= -->
<!-- META: variant=? -->
<!-- META: variant=?1-10 -->
<!-- META: variant=?11-20 -->
<!-- META: variant=# -->
<!-- META: variant=#a-m -->
<!-- META: variant=#n-z -->
<!-- META: variant=?# -->
<!-- META: variant=?1#a -->
<!-- META: variant=nonsense -->
        """)
        tests_found = [t.test_path for t in finder.find_tests_by_path(find_paths)]
        self.assertEqual(
            [
                "web-platform-tests/variant_test.any.html",
                "web-platform-tests/variant_test.any.html?1-10",
                "web-platform-tests/variant_test.any.html?11-20",
                "web-platform-tests/variant_test.any.html#a-m",
                "web-platform-tests/variant_test.any.html#n-z",
                "web-platform-tests/variant_test.any.html?1#a",
            ],
            tests_found,
        )

    def test_wpt_crash(self):
        tests_to_find = ["imported/w3c/web-platform-tests"]
        finder = self.finder
        tests_found = [
            t.test_path
            for t in finder.find_tests_by_path(tests_to_find)
            if t.is_wpt_crash_test
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
                "imported/w3c/web-platform-tests/some/test-pass-crash.tentative.html",
                "imported/w3c/web-platform-tests/some/test-timeout-crash.html",
            ],
        )

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
        sorted_tests = sorted(tests, key=self.port.test_key)
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

    def test_is_websocket_test(self):
        finder = self.finder
        fs = finder._filesystem

        files = [
            "http/tests/test.html",
            "http/tests/websocket/construct-in-detached-frame.html",
            "http/tests/security/mixedContent/websocket/insecure-websocket-in-iframe.html",
            "http/tests/security/contentSecurityPolicy/connect-src-star-secure-websocket-allowed.html",
            "imported/w3c/web-platform-tests/service-workers/service-worker/websocket.https.html",
        ]

        fs.chdir(self.port.layout_tests_dir())

        for f in files:
            fs.maybe_make_directory(fs.dirname(f))
            fs.write_text_file(f, "XXX")

        tests = finder.find_tests_by_path(
            files + ["http/tests/test.html?websocket"], with_expectations=True
        )
        self.assertEqual(
            tests,
            [
                Test(
                    test_path="http/tests/test.html",
                    is_http_test=True,
                    is_websocket_test=False,
                ),
                Test(
                    test_path="http/tests/websocket/construct-in-detached-frame.html",
                    is_http_test=True,
                    is_websocket_test=True,
                ),
                Test(
                    test_path="http/tests/security/mixedContent/websocket/insecure-websocket-in-iframe.html",
                    is_http_test=True,
                    is_websocket_test=True,
                ),
                Test(
                    test_path="http/tests/security/contentSecurityPolicy/connect-src-star-secure-websocket-allowed.html",
                    is_http_test=True,
                    is_websocket_test=True,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/service-workers/service-worker/websocket.https.html",
                    is_wpt_test=True,
                    is_websocket_test=False,
                ),
                Test(
                    test_path="http/tests/test.html?websocket",
                    is_http_test=True,
                    is_websocket_test=True,
                ),
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
                    is_wpt_test=False,
                    is_wpt_crash_test=False,
                ),
                Test(
                    test_path="fast/css/end-of-buffer-crash.html",
                    is_wpt_test=False,
                    is_wpt_crash_test=False,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/editing/run/empty-editable-crash.html",
                    is_wpt_test=True,
                    is_wpt_crash_test=True,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/html/semantics/popovers/popover-hint-crash.tentative.html",
                    is_wpt_test=True,
                    is_wpt_crash_test=True,
                ),
                Test(
                    test_path="imported/w3c/web-platform-tests/mathml/crashtests/mtd-as-multicol.html",
                    is_wpt_test=True,
                    is_wpt_crash_test=True,
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
                        (
                            "==",
                            fs.join(
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
                        (
                            "!=",
                            fs.join(
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
            (reftype, fs.join(self.port.layout_tests_dir(), "foo", refpath))
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
            (reftype, fs.join(self.port.layout_tests_dir(), "foo", refpath))
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
            (reftype, fs.join(self.port.layout_tests_dir(), "foo", refpath))
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
            (reftype, fs.join(self.port.layout_tests_dir(), "foo", refpath))
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
