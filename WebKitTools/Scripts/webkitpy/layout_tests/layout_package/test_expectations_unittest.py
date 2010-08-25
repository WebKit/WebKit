#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Unit tests for test_expectations.py."""

import os
import sys
import unittest

from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package.test_expectations import *

class FunctionsTest(unittest.TestCase):
    def test_result_was_expected(self):
        # test basics
        self.assertEquals(result_was_expected(PASS, set([PASS]),
                                              False, False), True)
        self.assertEquals(result_was_expected(TEXT, set([PASS]),
                                              False, False), False)

        # test handling of FAIL expectations
        self.assertEquals(result_was_expected(IMAGE_PLUS_TEXT, set([FAIL]),
                                              False, False), True)
        self.assertEquals(result_was_expected(IMAGE, set([FAIL]),
                                              False, False), True)
        self.assertEquals(result_was_expected(TEXT, set([FAIL]),
                                              False, False), True)
        self.assertEquals(result_was_expected(CRASH, set([FAIL]),
                                              False, False), False)

        # test handling of SKIPped tests and results
        self.assertEquals(result_was_expected(SKIP, set([CRASH]),
                                              False, True), True)
        self.assertEquals(result_was_expected(SKIP, set([CRASH]),
                                              False, False), False)

        # test handling of MISSING results and the REBASELINE modifier
        self.assertEquals(result_was_expected(MISSING, set([PASS]),
                                              True, False), True)
        self.assertEquals(result_was_expected(MISSING, set([PASS]),
                                              False, False), False)

    def test_remove_pixel_failures(self):
        self.assertEquals(remove_pixel_failures(set([TEXT])),
                          set([TEXT]))
        self.assertEquals(remove_pixel_failures(set([PASS])),
                          set([PASS]))
        self.assertEquals(remove_pixel_failures(set([IMAGE])),
                          set([PASS]))
        self.assertEquals(remove_pixel_failures(set([IMAGE_PLUS_TEXT])),
                          set([TEXT]))
        self.assertEquals(remove_pixel_failures(set([PASS, IMAGE, CRASH])),
                          set([PASS, CRASH]))


class TestExpectationsTest(unittest.TestCase):

    def __init__(self, testFunc, setUp=None, tearDown=None, description=None):
        self._port = port.get('test', None)
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_test(self, test_name):
        return os.path.join(self._port.layout_tests_dir(), test_name)

    def get_basic_tests(self):
        return [self.get_test('failures/expected/text.html'),
                self.get_test('failures/expected/image_checksum.html'),
                self.get_test('failures/expected/crash.html'),
                self.get_test('failures/expected/missing_text.html'),
                self.get_test('passes/text.html')]

    def get_basic_expectations(self):
        return """
BUG_TEST : failures/expected/text.html = TEXT
BUG_TEST SKIP : failures/expected/crash.html = CRASH
BUG_TEST REBASELINE : failure/expected/missing_image.html = MISSING
BUG_TEST : failures/expected/image_checksum.html = IMAGE
"""

    def parse_exp(self, expectations, overrides=None):
        self._exp = TestExpectations(self._port,
             tests=self.get_basic_tests(),
             expectations=expectations,
             test_platform_name=self._port.test_platform_name(),
             is_debug_mode=False,
             is_lint_mode=False,
             tests_are_present=True,
             overrides=overrides)

    def assert_exp(self, test, result):
        self.assertEquals(self._exp.get_expectations(self.get_test(test)),
                          set([result]))

    def test_basic(self):
        self.parse_exp(self.get_basic_expectations())
        self.assert_exp('failures/expected/text.html', TEXT)
        self.assert_exp('failures/expected/image_checksum.html', IMAGE)
        self.assert_exp('passes/text.html', PASS)

    def test_duplicates(self):
        self.assertRaises(SyntaxError, self.parse_exp, """
BUG_TEST : failures/expected/text.html = TEXT
BUG_TEST : failures/expected/text.html = IMAGE""")
        self.assertRaises(SyntaxError, self.parse_exp,
            self.get_basic_expectations(), """
BUG_TEST : failures/expected/text.html = TEXT
BUG_TEST : failures/expected/text.html = IMAGE""")

    def test_overrides(self):
        self.parse_exp(self.get_basic_expectations(), """
BUG_OVERRIDE : failures/expected/text.html = IMAGE""")
        self.assert_exp('failures/expected/text.html', IMAGE)

    def test_matches_an_expected_result(self):

        def match(test, result, pixel_tests_enabled):
            return self._exp.matches_an_expected_result(
                self.get_test(test), result, pixel_tests_enabled)

        self.parse_exp(self.get_basic_expectations())
        self.assertTrue(match('failures/expected/text.html', TEXT, True))
        self.assertTrue(match('failures/expected/text.html', TEXT, False))
        self.assertFalse(match('failures/expected/text.html', CRASH, True))
        self.assertFalse(match('failures/expected/text.html', CRASH, False))
        self.assertTrue(match('failures/expected/image_checksum.html', IMAGE, True))
        self.assertTrue(match('failures/expected/image_checksum.html', PASS, False))
        self.assertTrue(match('failures/expected/crash.html', SKIP, False))
        self.assertTrue(match('passes/text.html', PASS, False))

if __name__ == '__main__':
    unittest.main()
