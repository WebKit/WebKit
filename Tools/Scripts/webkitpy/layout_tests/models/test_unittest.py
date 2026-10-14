# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Apple Inc. nor the names of its
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

import unittest

from webkitpy.layout_tests.models.test import Test


class TestNeedsServer(unittest.TestCase):
    def test_defaults(self):
        t = Test(test_path='fast/dom/foo.html')
        self.assertFalse(t.needs_http_server)
        self.assertFalse(t.needs_websocket_server)
        self.assertFalse(t.needs_wpt_server)
        self.assertFalse(t.needs_any_server)

    def test_http(self):
        t = Test(test_path='http/tests/foo.html', is_http_test=True)
        self.assertTrue(t.needs_http_server)
        self.assertFalse(t.needs_wpt_server)
        self.assertTrue(t.needs_any_server)

    def test_http_websocket(self):
        t = Test(test_path='http/tests/websocket/foo.html', is_http_test=True, is_websocket_test=True)
        self.assertTrue(t.needs_http_server)
        self.assertTrue(t.needs_websocket_server)
        self.assertTrue(t.needs_any_server)

    def test_websocket(self):
        t = Test(test_path='websocket/tests/standalone.html', is_websocket_test=True)
        self.assertFalse(t.needs_http_server)
        # Websocket server only starts when http or wpt also starts, so a websocket-only test
        # has needs_websocket_server=False even though is_websocket_test=True.
        self.assertFalse(t.needs_websocket_server)
        self.assertFalse(t.needs_any_server)

    def test_wpt(self):
        t = Test(test_path='imported/w3c/web-platform-tests/foo.html', is_wpt_test=True)
        self.assertFalse(t.needs_http_server)
        self.assertTrue(t.needs_wpt_server)
        self.assertTrue(t.needs_any_server)

    def test_wpt_websocket(self):
        # Not an HTTP test, but the websocket server still starts because the
        # WPT server does.
        t = Test(test_path='imported/w3c/web-platform-tests/websocket/foo.html', is_wpt_test=True, is_websocket_test=True)
        self.assertFalse(t.needs_http_server)
        self.assertTrue(t.needs_wpt_server)
        self.assertTrue(t.needs_websocket_server)
        self.assertTrue(t.needs_any_server)


class TestFilePathAndVariant(unittest.TestCase):
    def test_no_variant(self):
        t = Test(test_path='fast/dom/foo.html')
        self.assertEqual(t.file_path, 'fast/dom/foo.html')
        self.assertEqual(t.variant, '')

    def test_query_variant(self):
        t = Test(test_path='imported/w3c/web-platform-tests/css/foo.html?1-100')
        self.assertEqual(t.file_path, 'imported/w3c/web-platform-tests/css/foo.html')
        self.assertEqual(t.variant, '?1-100')

    def test_fragment_variant(self):
        t = Test(test_path='fast/dom/foo.html#bar')
        self.assertEqual(t.file_path, 'fast/dom/foo.html')
        self.assertEqual(t.variant, '#bar')

    def test_query_before_fragment(self):
        t = Test(test_path='foo.html?a=1#frag')
        self.assertEqual(t.file_path, 'foo.html')
        self.assertEqual(t.variant, '?a=1#frag')

    def test_fragment_before_query(self):
        t = Test(test_path='foo.html#frag?a=1')
        self.assertEqual(t.file_path, 'foo.html')
        self.assertEqual(t.variant, '#frag?a=1')

    def test_slash_in_query_variant(self):
        t = Test(test_path='foo.html?a/b')
        self.assertEqual(t.file_path, 'foo.html')
        self.assertEqual(t.variant, '?a/b')

    def test_slash_in_fragment_variant(self):
        t = Test(test_path='foo.html#frag/sub')
        self.assertEqual(t.file_path, 'foo.html')
        self.assertEqual(t.variant, '#frag/sub')

    def test_repeated_and_empty_separators(self):
        # The variant starts at the earliest '?' or '#', and everything after
        # it, including further separators, belongs to the variant.
        for test_path, file_path, variant in (
            ('foo.html?a?b', 'foo.html', '?a?b'),
            ('foo.html#a#b', 'foo.html', '#a#b'),
            ('foo.html?a#b#c', 'foo.html', '?a#b#c'),
            ('foo.html#a?b#c', 'foo.html', '#a?b#c'),
            ('foo.html?', 'foo.html', '?'),
            ('foo.html#', 'foo.html', '#'),
            ('foo.html?#', 'foo.html', '?#'),
            ('foo.html#?', 'foo.html', '#?'),
            ('foo.html?a%20b', 'foo.html', '?a%20b'),
            ('a.b/c.d/foo.html?x', 'a.b/c.d/foo.html', '?x'),
            ('', '', ''),
        ):
            with self.subTest(test_path):
                t = Test(test_path=test_path)
                self.assertEqual((t.file_path, t.variant), (file_path, variant))

    def test_file_path_and_variant_always_reassemble_to_the_test_path(self):
        for test_path in (
            'foo.html', 'dir/foo.html?a', 'dir/foo.html#a', 'dir/foo.html?a#b', 'dir/foo.html#a?b',
            'dir/foo.html?a?b', 'dir/foo.html#a#b', 'dir/foo.html?', 'dir/foo.html#', 'dir/foo.html?a/b#c/d',
        ):
            with self.subTest(test_path):
                t = Test(test_path=test_path)
                self.assertEqual(t.file_path + t.variant, test_path)
                self.assertNotIn('?', t.file_path)
                self.assertNotIn('#', t.file_path)
