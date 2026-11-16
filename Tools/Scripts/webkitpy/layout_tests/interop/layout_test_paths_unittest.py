# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.interop.layout_test_paths import (
    imported_layout_test_paths,
    layout_test_path_for_wpt_test,
)

LAYOUT_TESTS_DIR = '/mock-checkout/LayoutTests'
WPT_DIR = LAYOUT_TESTS_DIR + '/imported/w3c/web-platform-tests'


class LayoutTestPathForWPTTestTest(unittest.TestCase):
    def test_test_url(self):
        self.assertEqual(
            'imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html',
            layout_test_path_for_wpt_test('/css/css-scroll-snap/snap-area.html'),
        )

    def test_url_without_leading_slash(self):
        self.assertEqual(
            'imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html',
            layout_test_path_for_wpt_test('css/css-scroll-snap/snap-area.html'),
        )

    def test_variant_is_kept(self):
        self.assertEqual(
            'imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html?basic',
            layout_test_path_for_wpt_test('/css/css-scroll-snap/snap-area.html?basic'),
        )

    def test_directory(self):
        self.assertEqual(
            'imported/w3c/web-platform-tests/css/css-scroll-snap',
            layout_test_path_for_wpt_test('/css/css-scroll-snap/'),
        )


class ImportedLayoutTestPathsTest(unittest.TestCase):
    def paths_for(self, wpt_tests):
        fs = MockFileSystem(
            files={
                WPT_DIR + '/css/css-scroll-snap/snap-area.html': '',
                WPT_DIR + '/css/css-scroll-snap/variants.html': '',
                WPT_DIR + '/fetch/api/basic/accept-header.any.worker.html': '',
            },
            dirs={WPT_DIR + '/css/css-scroll-snap'},
        )
        return imported_layout_test_paths(fs, LAYOUT_TESTS_DIR, wpt_tests)

    def test_imported_test(self):
        self.assertEqual(
            (['imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html'], []),
            self.paths_for(['/css/css-scroll-snap/snap-area.html']),
        )

    def test_test_which_is_not_imported(self):
        self.assertEqual(
            ([], ['/css/css-scroll-snap/not-imported.html']),
            self.paths_for(['/css/css-scroll-snap/not-imported.html']),
        )

    def test_variants_and_fragments_are_not_part_of_the_file_name(self):
        self.assertEqual(([
            'imported/w3c/web-platform-tests/css/css-scroll-snap/variants.html?a=b',
            'imported/w3c/web-platform-tests/css/css-scroll-snap/variants.html#frag',
        ], []), self.paths_for([
            '/css/css-scroll-snap/variants.html?a=b',
            '/css/css-scroll-snap/variants.html#frag',
        ]))

    def test_generated_test(self):
        # WebKit's WPT import writes out the tests generated from .any.js files.
        self.assertEqual(
            (['imported/w3c/web-platform-tests/fetch/api/basic/accept-header.any.worker.html'], []),
            self.paths_for(['/fetch/api/basic/accept-header.any.worker.html']),
        )

    def test_directory_of_tests(self):
        self.assertEqual(
            (['imported/w3c/web-platform-tests/css/css-scroll-snap'], []),
            self.paths_for(['/css/css-scroll-snap/']),
        )

    def test_order_is_preserved(self):
        paths, not_imported = self.paths_for([
            '/css/css-scroll-snap/variants.html',
            '/css/css-scroll-snap/not-imported.html',
            '/css/css-scroll-snap/snap-area.html',
        ])
        self.assertEqual([
            'imported/w3c/web-platform-tests/css/css-scroll-snap/variants.html',
            'imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html',
        ], paths)
        self.assertEqual(['/css/css-scroll-snap/not-imported.html'], not_imported)


if __name__ == '__main__':
    unittest.main()
