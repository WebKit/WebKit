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

import optparse
import unittest

from collections import OrderedDict

from pyfakefs.fake_filesystem_unittest import TestCaseMixin

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import (
    LayoutTestFinder,
    Port,
    _is_reference_html_file,
    _supported_test_extensions,
)
from webkitpy.port.test import add_unit_tests_to_mock_filesystem, TestPort, unit_test_list


class MockLayoutTestFinder(LayoutTestFinder):
    def _real_tests(self, paths):
        return [path for path in paths if path.endswith('.html')]


class LayoutTestFinderTests(unittest.TestCase, TestCaseMixin):
    def __init__(self, *args, **kwargs):
        super(LayoutTestFinderTests, self).__init__(*args, **kwargs)
        self.port = None
        self.finder = None

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.is_windows_fs = False
        host = MockHost(create_stub_repository_files=True, filesystem=FileSystem())
        add_unit_tests_to_mock_filesystem(host.filesystem)
        self.port = TestPort(host)
        self.finder = LayoutTestFinder(self.port, None)

    def tearDown(self):
        self.port = None
        self.finder = None

    def test_supported_test_extensions(self):
        self.assertEqual(_supported_test_extensions & Port._supported_reference_extensions, Port._supported_reference_extensions)

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
            self.port.baseline_search_path(),
            [
                "/test.checkout/LayoutTests/platform/test-mac-leopard",
                "/test.checkout/LayoutTests/platform/test-mac-snowleopard",
            ],
        )
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

    def test_find_platform(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['platform'])]
        self.assertEqual(
            tests,
            [
                'platform/test-mac-leopard/http/test.html',
                'platform/test-mac-leopard/passes/platform-specific-test.html',
                'platform/test-mac-leopard/platform-specific-dir/platform-specific-test.html',
                'platform/test-snow-leopard/http/test.html',
                'platform/test-snow-leopard/websocket/test.html',
                'platform/test-win-7sp0/http/test.html',
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

    def test_find_platform_specific(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['http/test.html'])]
        self.assertEqual(tests, [])

    def test_find_platform_specific_directory(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir'])]
        self.assertEqual(tests, [])
        with_slash = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir/'])]
        self.assertEqual(tests, with_slash)
        with_star = [t.test_path for t in finder.find_tests_by_path(['platform-specific-dir/*'])]
        self.assertEqual(tests, with_star)

    def test_find_directory_includes_platform_specific(self):
        # contrast with test_find_platform_specific above
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['http'])]
        self.assertEqual(
            tests,
            [
                'http/tests/passes/image.html',
                'http/tests/passes/text.html',
                'http/tests/ssl/text.html',
                'platform/test-mac-leopard/http/test.html',
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

    def test_find_glob_c(self):
        finder = self.finder
        tests = [t.test_path for t in finder.find_tests_by_path(['failures/expected/i[m]age.html'])]
        self.assertEqual(tests, ['failures/expected/image.html'])

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

    def test_find_template_variants(self):
        find_paths = ["template_test"]
        finder = self.finder

        path = finder._port.layout_tests_dir() + "/template_test/variant_test.any.html"

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, """<!-- This file is required for WebKit test infrastructure to run the templated test -->
<!-- META: variant=?1-10 -->
<!-- META: variant=?11-20 -->
        """)
        tests_found = [t.test_path for t in finder.find_tests_by_path(find_paths)]
        self.assertEqual(['template_test/variant_test.any.html?1-10', 'template_test/variant_test.any.html?11-20'], tests_found)

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

    def test_is_test_file(self):
        finder = self.finder
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.html'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.shtml'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'foo.svg'))
        self.assertTrue(finder._is_test_file(finder._filesystem, '', 'test-ref-test.html'))
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

        finder._filesystem.maybe_make_directory(finder._filesystem.dirname(path))
        finder._filesystem.write_text_file(path, """
{"directories": [
"web-platform-tests/common",
"web-platform-tests/dom/nodes/Document-createElement-namespace-tests",
"web-platform-tests/fonts",
"web-platform-tests/html/browsers/browsing-the-web/navigating-across-documents/source/support",
"web-platform-tests/html/browsers/browsing-the-web/unloading-documents/support",
"web-platform-tests/html/browsers/history/the-history-interface/non-automated",
"web-platform-tests/html/browsers/history/the-location-interface/non-automated",
"web-platform-tests/images",
"web-platform-tests/service-workers",
"web-platform-tests/tools"
], "files": [
"web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-block-defer-scripts-subframe.html",
"web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-not-hang-scriptloader-subframe.html"
]}""")
        self.assertFalse(finder._is_w3c_resource_file(finder._filesystem, finder._port.layout_tests_dir() + "/imported/w3", "resource_file.html"))
        self.assertFalse(finder._is_w3c_resource_file(finder._filesystem, finder._port.layout_tests_dir() + "/imported/w3c", "resource_file.html"))
        self.assertFalse(finder._is_w3c_resource_file(finder._filesystem, finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/XMLHttpRequest", "xmlhttprequest-sync-block-defer-scripts-subframe.html.html"))
        self.assertTrue(finder._is_w3c_resource_file(finder._filesystem, finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/XMLHttpRequest", "xmlhttprequest-sync-block-defer-scripts-subframe.html"))
        self.assertTrue(finder._is_w3c_resource_file(finder._filesystem, finder._port.layout_tests_dir() + "/imported/w3c/web-platform-tests/dom/nodes/Document-createElement-namespace-tests", "test.html"))
