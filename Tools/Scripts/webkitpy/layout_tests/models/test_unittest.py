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

from webkitpy.layout_tests.models.server_routing import ServerType
from webkitpy.layout_tests.models.test import Test
from webkitpy.layout_tests.models.test_input import TestInput


class TestNeedsServer(unittest.TestCase):
    def test_defaults(self):
        t = Test(test_path='fast/dom/foo.html')
        self.assertFalse(t.needs_http_server)
        self.assertFalse(t.needs_websocket_server)
        self.assertFalse(t.needs_wpt_server)
        self.assertFalse(t.needs_any_server)

    def test_http(self):
        t = Test(test_path='http/tests/foo.html', served_by=ServerType.HTTP)
        self.assertTrue(t.needs_http_server)
        self.assertFalse(t.needs_wpt_server)
        self.assertTrue(t.needs_any_server)

    def test_http_websocket(self):
        t = Test(test_path='http/tests/websocket/foo.html', served_by=ServerType.HTTP | ServerType.WEBSOCKET)
        self.assertTrue(t.needs_http_server)
        self.assertTrue(t.needs_websocket_server)
        self.assertTrue(t.needs_any_server)

    def test_websocket(self):
        t = Test(test_path='websocket/tests/standalone.html', served_by=ServerType.WEBSOCKET)
        self.assertFalse(t.needs_http_server)
        # Websocket server only starts when http or wpt also starts, so a websocket-only test
        # has needs_websocket_server=False even though ServerType.WEBSOCKET is set.
        self.assertFalse(t.needs_websocket_server)
        self.assertFalse(t.needs_any_server)

    def test_wpt(self):
        t = Test(test_path='imported/w3c/web-platform-tests/foo.html', served_by=ServerType.WPT)
        self.assertFalse(t.needs_http_server)
        self.assertTrue(t.needs_wpt_server)
        self.assertTrue(t.needs_any_server)

    def test_wpt_websocket(self):
        # Not an HTTP test, but the websocket server still starts because the
        # WPT server does.
        t = Test(test_path='imported/w3c/web-platform-tests/websocket/foo.html', served_by=ServerType.WPT | ServerType.WEBSOCKET)
        self.assertFalse(t.needs_http_server)
        self.assertTrue(t.needs_wpt_server)
        self.assertTrue(t.needs_websocket_server)
        self.assertTrue(t.needs_any_server)


class TestFlags(unittest.TestCase):
    def test_defaults(self):
        t = Test(test_path='foo/bar.html')
        self.assertFalse(t.https)
        self.assertFalse(t.h2)
        self.assertFalse(t.subdomain)

    def test_each_flag_is_independent(self):
        self.assertTrue(Test(test_path='a.https.html', flags=frozenset({'https'})).https)
        self.assertFalse(Test(test_path='a.https.html', flags=frozenset({'https'})).h2)
        self.assertFalse(Test(test_path='a.https.html', flags=frozenset({'https'})).subdomain)
        self.assertTrue(Test(test_path='a.h2.html', flags=frozenset({'h2'})).h2)
        self.assertFalse(Test(test_path='a.h2.html', flags=frozenset({'h2'})).https)
        self.assertTrue(Test(test_path='a.sub.html', flags=frozenset({'sub'})).subdomain)
        self.assertFalse(Test(test_path='a.sub.html', flags=frozenset({'sub'})).https)

    def test_flags_do_not_affect_ordering(self):
        t1 = Test(test_path='a.html', flags=frozenset({'https'}))
        t2 = Test(test_path='a.html', flags=frozenset({'h2'}))
        self.assertFalse(t1 < t2)
        self.assertFalse(t2 < t1)
        self.assertNotEqual(t1, t2)


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


class TestSortOrder(unittest.TestCase):
    def test_empty_is_least(self):
        self.assertLess(Test(test_path=''), Test(test_path='ab'))

    def test_alphabetical(self):
        self.assertLess(Test(test_path='a'), Test(test_path='ab'))
        self.assertLess(Test(test_path='a'), Test(test_path='b'))
        self.assertLess(Test(test_path='a'), Test(test_path='a2'))

    def test_numeric_ordering(self):
        self.assertLess(Test(test_path='1'), Test(test_path='2'))
        self.assertLess(Test(test_path='1'), Test(test_path='10'))
        self.assertLess(Test(test_path='2'), Test(test_path='10'))

    def test_leading_zeros(self):
        self.assertLess(Test(test_path='01'), Test(test_path='1'))
        self.assertLess(Test(test_path='001'), Test(test_path='01'))
        self.assertGreater(Test(test_path='a/foo1'), Test(test_path='a/foo01'))
        self.assertGreater(Test(test_path='a/foo01'), Test(test_path='a/foo001'))

    def test_numeric_in_filename(self):
        self.assertLess(Test(test_path='foo_1.html'), Test(test_path='foo_2.html'))
        self.assertLess(Test(test_path='foo_1.1.html'), Test(test_path='foo_2.html'))
        self.assertLess(Test(test_path='foo_1.html'), Test(test_path='foo_10.html'))
        self.assertLess(Test(test_path='foo_2.html'), Test(test_path='foo_10.html'))
        self.assertGreater(Test(test_path='foo_23.html'), Test(test_path='foo_10.html'))
        self.assertLess(Test(test_path='foo_23.html'), Test(test_path='foo_100.html'))

    def test_numeric_directory_components(self):
        self.assertLess(Test(test_path='a2'), Test(test_path='a10'))
        self.assertLess(Test(test_path='a2/foo'), Test(test_path='a10/foo'))

    def test_numeric_filename_components(self):
        self.assertGreater(Test(test_path='a/foo11'), Test(test_path='a/foo2'))

    def test_flat_vs_nested(self):
        self.assertLess(Test(test_path='ab'), Test(test_path='a/a/b'))
        self.assertGreater(Test(test_path='a/a/b'), Test(test_path='ab'))

    def test_variant_is_not_part_of_the_path(self):
        # A '/' inside a variant must not split the directory from the filename.
        self.assertLess(Test(test_path='a/b.html?z/z'), Test(test_path='a/c.html'))
        self.assertLess(Test(test_path='a/b.html#x/y'), Test(test_path='a/c.html'))
        self.assertLess(Test(test_path='dir/a.html?z/z'), Test(test_path='dir/b.html?a/a'))
        self.assertGreater(Test(test_path='dir/b.html?a/a'), Test(test_path='dir/a.html?z/z'))

    def test_variants_of_one_file_are_not_ordered(self):
        # Only the file path is compared, so variants of the same file keep
        # their discovery order under a stable sort.
        for first, second in (('a/b.html?c/d', 'a/b.html?z/z'), ('a/b.html?c', 'a/b.html#c')):
            self.assertFalse(Test(test_path=first) < Test(test_path=second))
            self.assertFalse(Test(test_path=second) < Test(test_path=first))
        tests = [Test(test_path=p) for p in ('a/b.html?z', 'a/b.html?c', 'a/a.html?q')]
        self.assertEqual([t.test_path for t in sorted(tests)], ['a/a.html?q', 'a/b.html?z', 'a/b.html?c'])

    def test_special_characters(self):
        self.assertLess(Test(test_path='foo-bar/baz'), Test(test_path='foo/baz'))
        self.assertLess(Test(test_path='foo!bar/baz'), Test(test_path='foo/bar/baz'))
        self.assertLess(Test(test_path='foo-bar/baz'), Test(test_path='foo/bar/baz'))
        self.assertGreater(Test(test_path='foo_bar/baz'), Test(test_path='foo/bar/baz'))

    def test_other_fields_do_not_affect_ordering(self):
        t1 = Test(test_path='a', expected_text_path='x', served_by=ServerType.FILE)
        t2 = Test(test_path='a', expected_text_path='y', served_by=ServerType.HTTP)
        self.assertNotEqual(t1, t2)
        self.assertFalse(t1 < t2)
        self.assertFalse(t2 < t1)
        self.assertLessEqual(t1, t2)
        self.assertGreaterEqual(t1, t2)

    def test_sorted_tests(self):
        tests = [Test(test_path=p) for p in ('a/foo10.html', 'a/foo2.html', 'a2/foo.html', 'a10/foo.html', 'a/foo02.html')]
        self.assertEqual(
            [t.test_path for t in sorted(tests)],
            ['a2/foo.html', 'a10/foo.html', 'a/foo02.html', 'a/foo2.html', 'a/foo10.html'],
        )

    def test_sorted_test_inputs(self):
        inputs = [
            TestInput(Test(test_path=p), timeout=timeout)
            for p, timeout in (('a/foo10.html', 1), ('a/foo2.html', 100), ('a10/foo.html', 5), ('a2/foo.html', 50))
        ]
        self.assertEqual(
            [i.test_name for i in sorted(inputs)],
            ['a2/foo.html', 'a10/foo.html', 'a/foo2.html', 'a/foo10.html'],
        )

    def test_test_input_delegates_to_test(self):
        # TestInput's other fields (timeout, is_slow, etc.) are order=False,
        # so ordering is purely delegated to the wrapped Test, even where
        # equality (which does consider every field) would disagree.
        self.assertLess(
            TestInput(Test(test_path='a'), timeout=100),
            TestInput(Test(test_path='b'), timeout=1),
        )
        same_test_different_timeout = TestInput(Test(test_path='a'), timeout=1)
        other_timeout = TestInput(Test(test_path='a'), timeout=100)
        self.assertFalse(same_test_different_timeout < other_timeout)
        self.assertFalse(other_timeout < same_test_different_timeout)
        self.assertNotEqual(same_test_different_timeout, other_timeout)
