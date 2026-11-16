# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Tests for the generic TestSuiteFormat contract."""

import unittest

from webkitexpectationspy.expectations import ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase
from webkitexpectationspy.parser import ExpectationParser
from webkitexpectationspy.suites.base import TestSuiteFormat


class GenericSuiteRoundTripTest(unittest.TestCase):
    """TestSuiteFormat must be usable standalone — a caller should be able to
    instantiate it and parse a minimal expectations file without any suite
    subclass."""

    def test_generic_suite_is_instantiable(self):
        suite = TestSuiteFormat()
        self.assertEqual(suite.modifier_class, ModifiersBase)
        self.assertIn('pass', suite.expectation_map)
        self.assertEqual(suite.expectation_map['pass'], ResultStatus.PASS)

    def test_parser_works_with_generic_suite(self):
        suite = TestSuiteFormat()
        parser = ExpectationParser(suite=suite)
        results = parser.parse('generic.txt', 'my_test [ Fail ]\nother_test [ Pass Crash ]\n')
        # Filter to successful parses.
        expectations = [exp for exp, warnings in results if exp is not None]
        self.assertEqual(len(expectations), 2)
        first, second = expectations
        self.assertEqual(first.test_pattern, 'my_test')
        self.assertIn(ResultStatus.FAIL, first.expected)
        self.assertEqual(second.test_pattern, 'other_test')
        self.assertIn(ResultStatus.PASS, second.expected)
        self.assertIn(ResultStatus.CRASH, second.expected)

    def test_generic_suite_modifiers_only_skip_and_slow(self):
        suite = TestSuiteFormat()
        self.assertEqual(suite.modifier_map, {'skip', 'slow'})
