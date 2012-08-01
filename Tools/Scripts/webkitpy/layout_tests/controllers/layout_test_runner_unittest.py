#!/usr/bin/python
# Copyright (C) 2012 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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
#     * Neither the name of Google Inc. nor the names of its
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

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.controllers.layout_test_runner import Sharder


class SharderTests(unittest.TestCase):

    test_list = [
        "http/tests/websocket/tests/unicode.htm",
        "animations/keyframes.html",
        "http/tests/security/view-source-no-refresh.html",
        "http/tests/websocket/tests/websocket-protocol-ignored.html",
        "fast/css/display-none-inline-style-change-crash.html",
        "http/tests/xmlhttprequest/supported-xml-content-types.html",
        "dom/html/level2/html/HTMLAnchorElement03.html",
        "ietestcenter/Javascript/11.1.5_4-4-c-1.html",
        "dom/html/level2/html/HTMLAnchorElement06.html",
        "perf/object-keys.html",
    ]

    ref_tests = [
        "http/tests/security/view-source-no-refresh.html",
        "http/tests/websocket/tests/websocket-protocol-ignored.html",
        "ietestcenter/Javascript/11.1.5_4-4-c-1.html",
        "dom/html/level2/html/HTMLAnchorElement06.html",
    ]

    def get_test_input(self, test_file):
        return TestInput(test_file, requires_lock=(test_file.startswith('http') or test_file.startswith('perf')),
                         reference_files=(['foo'] if test_file in self.ref_tests else None))

    def get_shards(self, num_workers, fully_parallel, shard_ref_tests=False, test_list=None, max_locked_shards=1):
        def split(test_name):
            idx = test_name.rfind('/')
            if idx != -1:
                return (test_name[0:idx], test_name[idx + 1:])

        self.sharder = Sharder(split, '/', max_locked_shards)
        test_list = test_list or self.test_list
        return self.sharder.shard_tests([self.get_test_input(test) for test in test_list], num_workers, fully_parallel, shard_ref_tests)

    def assert_shards(self, actual_shards, expected_shard_names):
        self.assertEquals(len(actual_shards), len(expected_shard_names))
        for i, shard in enumerate(actual_shards):
            expected_shard_name, expected_test_names = expected_shard_names[i]
            self.assertEquals(shard.name, expected_shard_name)
            self.assertEquals([test_input.test_name for test_input in shard.test_inputs],
                              expected_test_names)

    def test_shard_by_dir(self):
        locked, unlocked = self.get_shards(num_workers=2, fully_parallel=False)

        # Note that although there are tests in multiple dirs that need locks,
        # they are crammed into a single shard in order to reduce the # of
        # workers hitting the server at once.
        self.assert_shards(locked,
             [('locked_shard_1',
               ['http/tests/security/view-source-no-refresh.html',
                'http/tests/websocket/tests/unicode.htm',
                'http/tests/websocket/tests/websocket-protocol-ignored.html',
                'http/tests/xmlhttprequest/supported-xml-content-types.html',
                'perf/object-keys.html'])])
        self.assert_shards(unlocked,
            [('animations', ['animations/keyframes.html']),
             ('dom/html/level2/html', ['dom/html/level2/html/HTMLAnchorElement03.html',
                                      'dom/html/level2/html/HTMLAnchorElement06.html']),
             ('fast/css', ['fast/css/display-none-inline-style-change-crash.html']),
             ('ietestcenter/Javascript', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html'])])

    def test_shard_by_dir_sharding_ref_tests(self):
        locked, unlocked = self.get_shards(num_workers=2, fully_parallel=False, shard_ref_tests=True)

        # Note that although there are tests in multiple dirs that need locks,
        # they are crammed into a single shard in order to reduce the # of
        # workers hitting the server at once.
        self.assert_shards(locked,
            [('locked_shard_1',
              ['http/tests/websocket/tests/unicode.htm',
               'http/tests/xmlhttprequest/supported-xml-content-types.html',
               'perf/object-keys.html',
               'http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/websocket-protocol-ignored.html'])])
        self.assert_shards(unlocked,
            [('animations', ['animations/keyframes.html']),
             ('dom/html/level2/html', ['dom/html/level2/html/HTMLAnchorElement03.html']),
             ('fast/css', ['fast/css/display-none-inline-style-change-crash.html']),
             ('~ref:dom/html/level2/html', ['dom/html/level2/html/HTMLAnchorElement06.html']),
             ('~ref:ietestcenter/Javascript', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html'])])

    def test_shard_every_file(self):
        locked, unlocked = self.get_shards(num_workers=2, fully_parallel=True)
        self.assert_shards(locked,
            [('.', ['http/tests/websocket/tests/unicode.htm']),
             ('.', ['http/tests/security/view-source-no-refresh.html']),
             ('.', ['http/tests/websocket/tests/websocket-protocol-ignored.html']),
             ('.', ['http/tests/xmlhttprequest/supported-xml-content-types.html']),
             ('.', ['perf/object-keys.html'])]),
        self.assert_shards(unlocked,
            [('.', ['animations/keyframes.html']),
             ('.', ['fast/css/display-none-inline-style-change-crash.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement03.html']),
             ('.', ['ietestcenter/Javascript/11.1.5_4-4-c-1.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False)
        self.assert_shards(locked,
            [('locked_tests',
              ['http/tests/websocket/tests/unicode.htm',
               'http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/websocket-protocol-ignored.html',
               'http/tests/xmlhttprequest/supported-xml-content-types.html',
               'perf/object-keys.html'])])
        self.assert_shards(unlocked,
            [('unlocked_tests',
              ['animations/keyframes.html',
               'fast/css/display-none-inline-style-change-crash.html',
               'dom/html/level2/html/HTMLAnchorElement03.html',
               'ietestcenter/Javascript/11.1.5_4-4-c-1.html',
               'dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two_sharding_ref_tests(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False, shard_ref_tests=True)
        self.assert_shards(locked,
            [('locked_tests',
              ['http/tests/websocket/tests/unicode.htm',
               'http/tests/xmlhttprequest/supported-xml-content-types.html',
               'perf/object-keys.html',
               'http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/websocket-protocol-ignored.html'])])
        self.assert_shards(unlocked,
            [('unlocked_tests',
              ['animations/keyframes.html',
               'fast/css/display-none-inline-style-change-crash.html',
               'dom/html/level2/html/HTMLAnchorElement03.html',
               'ietestcenter/Javascript/11.1.5_4-4-c-1.html',
               'dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two_has_no_locked_shards(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False,
             test_list=['animations/keyframe.html'])
        self.assertEquals(len(locked), 0)
        self.assertEquals(len(unlocked), 1)

    def test_shard_in_two_has_no_unlocked_shards(self):
        locked, unlocked = self.get_shards(num_workers=1, fully_parallel=False,
             test_list=['http/tests/websocket/tests/unicode.htm'])
        self.assertEquals(len(locked), 1)
        self.assertEquals(len(unlocked), 0)

    def test_multiple_locked_shards(self):
        locked, unlocked = self.get_shards(num_workers=4, fully_parallel=False, max_locked_shards=2)
        self.assert_shards(locked,
            [('locked_shard_1',
              ['http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/unicode.htm',
               'http/tests/websocket/tests/websocket-protocol-ignored.html']),
             ('locked_shard_2',
              ['http/tests/xmlhttprequest/supported-xml-content-types.html',
               'perf/object-keys.html'])])

        locked, unlocked = self.get_shards(num_workers=4, fully_parallel=False)
        self.assert_shards(locked,
            [('locked_shard_1',
              ['http/tests/security/view-source-no-refresh.html',
               'http/tests/websocket/tests/unicode.htm',
               'http/tests/websocket/tests/websocket-protocol-ignored.html',
               'http/tests/xmlhttprequest/supported-xml-content-types.html',
               'perf/object-keys.html'])])


class NaturalCompareTest(unittest.TestCase):
    def assert_cmp(self, x, y, result):
        self.assertEquals(cmp(Sharder.natural_sort_key(x), Sharder.natural_sort_key(y)), result)

    def test_natural_compare(self):
        self.assert_cmp('a', 'a', 0)
        self.assert_cmp('ab', 'a', 1)
        self.assert_cmp('a', 'ab', -1)
        self.assert_cmp('', '', 0)
        self.assert_cmp('', 'ab', -1)
        self.assert_cmp('1', '2', -1)
        self.assert_cmp('2', '1', 1)
        self.assert_cmp('1', '10', -1)
        self.assert_cmp('2', '10', -1)
        self.assert_cmp('foo_1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.html', 'foo_10.html', -1)
        self.assert_cmp('foo_2.html', 'foo_10.html', -1)
        self.assert_cmp('foo_23.html', 'foo_10.html', 1)
        self.assert_cmp('foo_23.html', 'foo_100.html', -1)


class KeyCompareTest(unittest.TestCase):
    def setUp(self):
        def split(test_name):
            idx = test_name.rfind('/')
            if idx != -1:
                return (test_name[0:idx], test_name[idx + 1:])

        self.sharder = Sharder(split, '/', 1)

    def assert_cmp(self, x, y, result):
        self.assertEquals(cmp(self.sharder.test_key(x), self.sharder.test_key(y)), result)

    def test_test_key(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)
