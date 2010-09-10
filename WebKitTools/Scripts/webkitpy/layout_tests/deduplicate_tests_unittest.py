#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for deduplicate_tests.py."""

import deduplicate_tests
import os
import unittest
import webkitpy.common.checkout.scm as scm


class MockExecutive(object):
    last_run_command = []
    response = ''

    class Executive(object):
        def run_command(self,
                        args,
                        cwd=None,
                        input=None,
                        error_handler=None,
                        return_exit_code=False,
                        return_stderr=True,
                        decode_output=True):
            MockExecutive.last_run_command += [args]
            return MockExecutive.response


class ListDuplicatesTest(unittest.TestCase):
    def setUp(self):
        MockExecutive.last_run_command = []
        MockExecutive.response = ''
        deduplicate_tests.executive = MockExecutive
        self._original_cwd = os.getcwd()
        checkout_root = scm.find_checkout_root()
        self.assertNotEqual(checkout_root, None)
        os.chdir(checkout_root)

    def tearDown(self):
        os.chdir(self._original_cwd)

    def test_parse_git_output(self):
        git_output = (
            '100644 blob 5053240b3353f6eb39f7cb00259785f16d121df2\tLayoutTests/mac/foo-expected.txt\n'
            '100644 blob a004548d107ecc4e1ea08019daf0a14e8634a1ff\tLayoutTests/platform/chromium/foo-expected.txt\n'
            '100644 blob d6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-linux/foo-expected.txt\n'
            '100644 blob abcdebc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-linux/animage.png\n'
            '100644 blob d6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-win/foo-expected.txt\n'
            '100644 blob abcdebc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-win/animage.png\n'
            '100644 blob 4303df5389ca87cae83dd3236b8dd84e16606517\tLayoutTests/platform/mac/foo-expected.txt\n')
        hashes = deduplicate_tests.parse_git_output(git_output, '*')
        expected = {('mac/foo-expected.txt', '5053240b3353f6eb39f7cb00259785f16d121df2'): set(['mac/foo-expected.txt']),
                    ('animage.png', 'abcdebc762e3aec5df03b5c04485b2cb3b65ffb1'): set(['platform/chromium-linux/animage.png', 'platform/chromium-win/animage.png']),
                    ('foo-expected.txt', '4303df5389ca87cae83dd3236b8dd84e16606517'): set(['platform/mac/foo-expected.txt']),
                    ('foo-expected.txt', 'd6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1'): set(['platform/chromium-linux/foo-expected.txt', 'platform/chromium-win/foo-expected.txt']),
                    ('foo-expected.txt', 'a004548d107ecc4e1ea08019daf0a14e8634a1ff'): set(['platform/chromium/foo-expected.txt'])}
        self.assertEquals(expected, hashes)

        hashes = deduplicate_tests.parse_git_output(git_output, '*.png')
        expected = {('animage.png', 'abcdebc762e3aec5df03b5c04485b2cb3b65ffb1'): set(['platform/chromium-linux/animage.png', 'platform/chromium-win/animage.png'])}
        self.assertEquals(expected, hashes)

    def test_extract_platforms(self):
        self.assertEquals({'foo': 'platform/foo/bar',
                           'zoo': 'platform/zoo/com'},
                           deduplicate_tests.extract_platforms(['platform/foo/bar', 'platform/zoo/com']))
        self.assertEquals({'foo': 'platform/foo/bar',
                           deduplicate_tests._BASE_PLATFORM: 'what/'},
                           deduplicate_tests.extract_platforms(['platform/foo/bar', 'what/']))

    def test_has_intermediate_results(self):
        test_cases = (
            # If we found a duplicate in our first fallback, we have no
            # intermediate results.
            (False, ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'chromium-win',
                     lambda path: True)),
            # Since chromium-win has a result, we have an intermediate result.
            (True,  ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'chromium',
                     lambda path: True)),
            # There are no intermediate results.
            (False, ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'chromium',
                     lambda path: False)),
            # There are no intermediate results since a result for chromium is
            # our duplicate file.
            (False, ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'chromium',
                     lambda path: path == 'LayoutTests/platform/chromium/fast/foo-expected.txt')),
            # We have an intermediate result in 'chromium' even though our
            # duplicate is with the file in 'base'.
            (True,  ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'base',
                     lambda path: path == 'LayoutTests/platform/chromium/fast/foo-expected.txt')),
            # We have an intermediate result in 'chromium-win' even though our
            # duplicate is in 'base'.
            (True,  ('fast/foo-expected.txt',
                     ['chromium-win', 'chromium', 'base'],
                     'base',
                     lambda path: path == 'LayoutTests/platform/chromium-win/fast/foo-expected.txt')),
        )
        for expected, inputs in test_cases:
            self.assertEquals(expected,
                deduplicate_tests.has_intermediate_results(*inputs))

    def test_unique(self):
        MockExecutive.response = (
            '100644 blob 5053240b3353f6eb39f7cb00259785f16d121df2\tLayoutTests/mac/foo-expected.txt\n'
            '100644 blob a004548d107ecc4e1ea08019daf0a14e8634a1ff\tLayoutTests/platform/chromium/foo-expected.txt\n'
            '100644 blob abcd0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-linux/foo-expected.txt\n'
            '100644 blob d6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-win/foo-expected.txt\n'
            '100644 blob 4303df5389ca87cae83dd3236b8dd84e16606517\tLayoutTests/platform/mac/foo-expected.txt\n')
        result = deduplicate_tests.deduplicate('*')
        self.assertEquals(1, len(MockExecutive.last_run_command))
        self.assertEquals(('git', 'ls-tree', '-r', 'HEAD', 'LayoutTests'), MockExecutive.last_run_command[-1])
        self.assertEquals(0, len(result))

    def test_duplicates(self):
        MockExecutive.response = (
            '100644 blob 5053240b3353f6eb39f7cb00259785f16d121df2\tLayoutTests/mac/foo-expected.txt\n'
            '100644 blob a004548d107ecc4e1ea08019daf0a14e8634a1ff\tLayoutTests/platform/chromium/foo-expected.txt\n'
            '100644 blob d6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-linux/foo-expected.txt\n'
            '100644 blob abcdebc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-linux/animage.png\n'
            '100644 blob d6bb0bc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-win/foo-expected.txt\n'
            '100644 blob abcdebc762e3aec5df03b5c04485b2cb3b65ffb1\tLayoutTests/platform/chromium-win/animage.png\n'
            '100644 blob 4303df5389ca87cae83dd3236b8dd84e16606517\tLayoutTests/platform/mac/foo-expected.txt\n')

        result = deduplicate_tests.deduplicate('*')
        self.assertEquals(1, len(MockExecutive.last_run_command))
        self.assertEquals(('git', 'ls-tree', '-r', 'HEAD', 'LayoutTests'), MockExecutive.last_run_command[-1])
        self.assertEquals(2, len(result))
        self.assertEquals({'test': 'animage.png',
                           'path': 'LayoutTests/platform/chromium-linux/animage.png',
                           'fallback': 'chromium-win',
                           'platform': 'chromium-linux'},
                          result[0])
        self.assertEquals({'test': 'foo-expected.txt',
                           'path': 'LayoutTests/platform/chromium-linux/foo-expected.txt',
                           'fallback': 'chromium-win',
                           'platform': 'chromium-linux'},
                          result[1])

        result = deduplicate_tests.deduplicate('*.txt')
        self.assertEquals(2, len(MockExecutive.last_run_command))
        self.assertEquals(('git', 'ls-tree', '-r', 'HEAD', 'LayoutTests'), MockExecutive.last_run_command[-1])
        self.assertEquals(1, len(result))
        self.assertEquals({'test': 'foo-expected.txt',
                           'path': 'LayoutTests/platform/chromium-linux/foo-expected.txt',
                           'fallback': 'chromium-win',
                           'platform': 'chromium-linux'},
                          result[0])

        result = deduplicate_tests.deduplicate('*.png')
        self.assertEquals(3, len(MockExecutive.last_run_command))
        self.assertEquals(('git', 'ls-tree', '-r', 'HEAD', 'LayoutTests'), MockExecutive.last_run_command[-1])
        self.assertEquals(1, len(result))
        self.assertEquals({'test': 'animage.png',
                           'path': 'LayoutTests/platform/chromium-linux/animage.png',
                           'fallback': 'chromium-win',
                           'platform': 'chromium-linux'},
                          result[0])
