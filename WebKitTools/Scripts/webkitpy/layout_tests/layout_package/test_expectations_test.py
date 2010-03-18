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

try:
   d = os.path.dirname(__file__)
except NameError:
   d = os.path.dirname(sys.argv[0])

sys.path.append(os.path.abspath(os.path.join(d, '..')))
sys.path.append(os.path.abspath(os.path.join(d, '../../thirdparty')))

import port
from test_expectations import *

class TestExpectationsTest(unittest.TestCase):

    def __init__(self, testFunc, setUp=None, tearDown=None, description=None):
        self._port = port.get('test', None)
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_test(self, test_name):
        return os.path.join(self._port.layout_tests_dir(), test_name)

    def get_basic_tests(self):
        return [self.get_test('fast/html/article-element.html'),
                self.get_test('fast/html/header-element.html'),
                self.get_test('fast/events/space-scroll-event.html'),
                self.get_test('fast/events/tab-imagemap.html')]

    def get_basic_expectations(self):
        return """
BUG_TEST : fast/html/article-element.html = TEXT
BUG_TEST : fast/events = IMAGE
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
       self.assert_exp('fast/html/article-element.html', TEXT)
       self.assert_exp('fast/events/tab-imagemap.html', IMAGE)
       self.assert_exp('fast/html/header-element.html', PASS)

    def test_duplicates(self):
       self.assertRaises(SyntaxError, self.parse_exp, """
BUG_TEST : fast/html/article-element.html = TEXT
BUG_TEST : fast/html/article-element.html = IMAGE""")
       self.assertRaises(SyntaxError, self.parse_exp,
           self.get_basic_expectations(), """
BUG_TEST : fast/html/article-element.html = TEXT
BUG_TEST : fast/html/article-element.html = IMAGE""")

    def test_overrides(self):
       self.parse_exp(self.get_basic_expectations(), """
BUG_OVERRIDE : fast/html/article-element.html = IMAGE""")
       self.assert_exp('fast/html/article-element.html', IMAGE)

if __name__ == '__main__':
    unittest.main()
