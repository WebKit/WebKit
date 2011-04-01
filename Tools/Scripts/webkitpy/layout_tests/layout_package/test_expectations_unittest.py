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

import unittest

from webkitpy.layout_tests import port
from webkitpy.layout_tests.port import base
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


class Base(unittest.TestCase):
    # Note that all of these tests are written assuming the configuration
    # being tested is Windows XP, Release build.

    def __init__(self, testFunc, setUp=None, tearDown=None, description=None):
        self._port = port.get('test-win-xp', None)
        self._fs = self._port._filesystem
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_test(self, test_name):
        return self._fs.join(self._port.layout_tests_dir(), test_name)

    def get_basic_tests(self):
        return [self.get_test('failures/expected/text.html'),
                self.get_test('failures/expected/image_checksum.html'),
                self.get_test('failures/expected/crash.html'),
                self.get_test('failures/expected/missing_text.html'),
                self.get_test('failures/expected/image.html'),
                self.get_test('passes/text.html')]

    def get_basic_expectations(self):
        return """
BUG_TEST : failures/expected/text.html = TEXT
BUG_TEST WONTFIX SKIP : failures/expected/crash.html = CRASH
BUG_TEST REBASELINE : failures/expected/missing_image.html = MISSING
BUG_TEST WONTFIX : failures/expected/image_checksum.html = IMAGE
BUG_TEST WONTFIX MAC : failures/expected/image.html = IMAGE
"""

    def parse_exp(self, expectations, overrides=None, is_lint_mode=False):
        test_config = self._port.test_configuration()
        self._exp = TestExpectations(self._port,
             tests=self.get_basic_tests(),
             expectations=expectations,
             test_config=test_config,
             is_lint_mode=is_lint_mode,
             overrides=overrides)

    def assert_exp(self, test, result):
        self.assertEquals(self._exp.get_expectations(self.get_test(test)),
                          set([result]))


class BasicTests(Base):
    def test_basic(self):
        self.parse_exp(self.get_basic_expectations())
        self.assert_exp('failures/expected/text.html', TEXT)
        self.assert_exp('failures/expected/image_checksum.html', IMAGE)
        self.assert_exp('passes/text.html', PASS)
        self.assert_exp('failures/expected/image.html', PASS)


class MiscTests(Base):
    def test_multiple_results(self):
        self.parse_exp('BUGX : failures/expected/text.html = TEXT CRASH')
        self.assertEqual(self._exp.get_expectations(
            self.get_test('failures/expected/text.html')),
            set([TEXT, CRASH]))

    def test_category_expectations(self):
        # This test checks unknown tests are not present in the
        # expectations and that known test part of a test category is
        # present in the expectations.
        exp_str = """
BUGX WONTFIX : failures/expected = IMAGE
"""
        self.parse_exp(exp_str)
        test_name = 'failures/expected/unknown-test.html'
        unknown_test = self.get_test(test_name)
        self.assertRaises(KeyError, self._exp.get_expectations,
                          unknown_test)
        self.assert_exp('failures/expected/crash.html', IMAGE)

    def test_get_options(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEqual(self._exp.get_options(
                         self.get_test('passes/text.html')), [])

    def test_expectations_json_for_all_platforms(self):
        self.parse_exp(self.get_basic_expectations())
        json_str = self._exp.get_expectations_json_for_all_platforms()
        # FIXME: test actual content?
        self.assertTrue(json_str)

    def test_get_expectations_string(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEquals(self._exp.get_expectations_string(
                          self.get_test('failures/expected/text.html')),
                          'TEXT')

    def test_expectation_to_string(self):
        # Normal cases are handled by other tests.
        self.parse_exp(self.get_basic_expectations())
        self.assertRaises(ValueError, self._exp.expectation_to_string,
                          -1)

    def test_get_test_set(self):
        # Handle some corner cases for this routine not covered by other tests.
        self.parse_exp(self.get_basic_expectations())
        s = self._exp._expected_failures.get_test_set(WONTFIX)
        self.assertEqual(s,
            set([self.get_test('failures/expected/crash.html'),
                 self.get_test('failures/expected/image_checksum.html')]))
        s = self._exp._expected_failures.get_test_set(WONTFIX, CRASH)
        self.assertEqual(s,
            set([self.get_test('failures/expected/crash.html')]))
        s = self._exp._expected_failures.get_test_set(WONTFIX, CRASH,
                                                      include_skips=False)
        self.assertEqual(s, set([]))

    def test_parse_error_fatal(self):
        try:
            self.parse_exp("""FOO : failures/expected/text.html = TEXT
SKIP : failures/expected/image.html""")
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError, e:
            self.assertTrue(e.fatal)
            exp_errors = [u"Line:1 Unrecognized option 'foo' failures/expected/text.html",
                          u"Line:2 Missing expectations. [' failures/expected/image.html']"]
            self.assertEqual(str(e), '\n'.join(map(str, exp_errors)))
            self.assertEqual(e.errors, exp_errors)

    def test_parse_error_nonfatal(self):
        try:
            self.parse_exp('SKIP : failures/expected/text.html = TEXT',
                           is_lint_mode=True)
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError, e:
            self.assertFalse(e.fatal)
            exp_errors = [u'Line:1 Test lacks BUG modifier. failures/expected/text.html']
            self.assertEqual(str(e), '\n'.join(map(str, exp_errors)))
            self.assertEqual(e.errors, exp_errors)

    def test_overrides(self):
        self.parse_exp("BUG_EXP: failures/expected/text.html = TEXT",
                       "BUG_OVERRIDE : failures/expected/text.html = IMAGE")
        self.assert_exp('failures/expected/text.html', IMAGE)

    def test_overrides__duplicate(self):
        self.assertRaises(ParseError, self.parse_exp,
             "BUG_EXP: failures/expected/text.html = TEXT",
             """
BUG_OVERRIDE : failures/expected/text.html = IMAGE
BUG_OVERRIDE : failures/expected/text.html = CRASH
""")

    def test_pixel_tests_flag(self):
        def match(test, result, pixel_tests_enabled):
            return self._exp.matches_an_expected_result(
                self.get_test(test), result, pixel_tests_enabled)

        self.parse_exp(self.get_basic_expectations())
        self.assertTrue(match('failures/expected/text.html', TEXT, True))
        self.assertTrue(match('failures/expected/text.html', TEXT, False))
        self.assertFalse(match('failures/expected/text.html', CRASH, True))
        self.assertFalse(match('failures/expected/text.html', CRASH, False))
        self.assertTrue(match('failures/expected/image_checksum.html', IMAGE,
                              True))
        self.assertTrue(match('failures/expected/image_checksum.html', PASS,
                              False))
        self.assertTrue(match('failures/expected/crash.html', SKIP, False))
        self.assertTrue(match('passes/text.html', PASS, False))

    def test_more_specific_override_resets_skip(self):
        self.parse_exp("BUGX SKIP : failures/expected = TEXT\n"
                       "BUGX : failures/expected/text.html = IMAGE\n")
        self.assert_exp('failures/expected/text.html', IMAGE)
        self.assertFalse(self._port._filesystem.join(self._port.layout_tests_dir(),
                                                     'failures/expected/text.html') in
                         self._exp.get_tests_with_result_type(SKIP))

class ExpectationSyntaxTests(Base):
    def test_missing_expectation(self):
        # This is missing the expectation.
        self.assertRaises(ParseError, self.parse_exp,
                          'BUG_TEST: failures/expected/text.html')

    def test_missing_colon(self):
        # This is missing the modifiers and the ':'
        self.assertRaises(ParseError, self.parse_exp,
                          'failures/expected/text.html = TEXT')

    def disabled_test_too_many_colons(self):
        # FIXME: Enable this test and fix the underlying bug.
        self.assertRaises(ParseError, self.parse_exp,
                          'BUG_TEST: failures/expected/text.html = PASS :')

    def test_too_many_equals_signs(self):
        self.assertRaises(ParseError, self.parse_exp,
                          'BUG_TEST: failures/expected/text.html = TEXT = IMAGE')

    def test_unrecognized_expectation(self):
        self.assertRaises(ParseError, self.parse_exp,
                          'BUG_TEST: failures/expected/text.html = UNKNOWN')

    def test_macro(self):
        exp_str = """
BUG_TEST WIN-XP : failures/expected/text.html = TEXT
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', TEXT)


class SemanticTests(Base):
    def test_bug_format(self):
        self.assertRaises(ParseError, self.parse_exp, 'BUG1234 : failures/expected/text.html = TEXT')

    def test_missing_bugid(self):
        # This should log a non-fatal error.
        self.parse_exp('SLOW : failures/expected/text.html = TEXT')
        self.assertEqual(
            len(self._exp._expected_failures.get_non_fatal_errors()), 1)

    def test_slow_and_timeout(self):
        # A test cannot be SLOW and expected to TIMEOUT.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST SLOW : failures/expected/timeout.html = TIMEOUT')

    def test_rebaseline(self):
        # Can't lint a file w/ 'REBASELINE' in it.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST REBASELINE : failures/expected/text.html = TEXT',
            is_lint_mode=True)

    def test_duplicates(self):
        self.assertRaises(ParseError, self.parse_exp, """
BUG_EXP : failures/expected/text.html = TEXT
BUG_EXP : failures/expected/text.html = IMAGE""")

        self.assertRaises(ParseError, self.parse_exp,
            self.get_basic_expectations(), overrides="""
BUG_OVERRIDE : failures/expected/text.html = TEXT
BUG_OVERRIDE : failures/expected/text.html = IMAGE""", )

    def test_missing_file(self):
        # This should log a non-fatal error.
        self.parse_exp('BUG_TEST : missing_file.html = TEXT')
        self.assertEqual(
            len(self._exp._expected_failures.get_non_fatal_errors()), 1)


class PrecedenceTests(Base):
    def test_file_over_directory(self):
        # This tests handling precedence of specific lines over directories
        # and tests expectations covering entire directories.
        exp_str = """
BUGX : failures/expected/text.html = TEXT
BUGX WONTFIX : failures/expected = IMAGE
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', TEXT)
        self.assert_exp('failures/expected/crash.html', IMAGE)

        exp_str = """
BUGX WONTFIX : failures/expected = IMAGE
BUGX : failures/expected/text.html = TEXT
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', TEXT)
        self.assert_exp('failures/expected/crash.html', IMAGE)

    def test_ambiguous(self):
        self.assertRaises(ParseError, self.parse_exp, """
BUG_TEST RELEASE : passes/text.html = PASS
BUG_TEST WIN : passes/text.html = FAIL
""")

    def test_more_modifiers(self):
        exp_str = """
BUG_TEST RELEASE : passes/text.html = PASS
BUG_TEST WIN RELEASE : passes/text.html = TEXT
"""
        self.assertRaises(ParseError, self.parse_exp, exp_str)

    def test_order_in_file(self):
        exp_str = """
BUG_TEST WIN RELEASE : passes/text.html = TEXT
BUG_TEST RELEASE : passes/text.html = PASS
"""
        self.assertRaises(ParseError, self.parse_exp, exp_str)

    def test_version_overrides(self):
        exp_str = """
BUG_TEST WIN : passes/text.html = PASS
BUG_TEST WIN XP : passes/text.html = TEXT
"""
        self.assertRaises(ParseError, self.parse_exp, exp_str)

    def test_macro_overrides(self):
        exp_str = """
BUG_TEST WIN : passes/text.html = PASS
BUG_TEST WIN-XP : passes/text.html = TEXT
"""
        self.assertRaises(ParseError, self.parse_exp, exp_str)


class RebaseliningTest(Base):
    """Test rebaselining-specific functionality."""
    def assertRemove(self, input_expectations, tests, expected_expectations):
        self.parse_exp(input_expectations)
        actual_expectations = self._exp.remove_rebaselined_tests(tests)
        self.assertEqual(expected_expectations, actual_expectations)

    def test_remove(self):
        self.assertRemove('BUGX REBASELINE : failures/expected/text.html = TEXT\n'
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n',
                          ['failures/expected/text.html'],
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n')

    def test_no_get_rebaselining_failures(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEqual(len(self._exp.get_rebaselining_failures()), 0)


class ModifierTests(unittest.TestCase):
    def setUp(self):
        port_obj = port.get('test-win-xp', None)
        self.config = port_obj.test_configuration()
        self.matcher = ModifierMatcher(self.config)

    def match(self, modifiers, expected_num_matches=-1, values=None, num_errors=0):
        matcher = self.matcher
        if values:
            matcher = ModifierMatcher(self.FakeTestConfiguration(values))
        match_result = matcher.match(modifiers)
        self.assertEqual(len(match_result.warnings), 0)
        self.assertEqual(len(match_result.errors), num_errors)
        self.assertEqual(match_result.num_matches, expected_num_matches,
             'match(%s, %s) returned -> %d, expected %d' %
             (modifiers, str(self.config.values()),
              match_result.num_matches, expected_num_matches))

    def test_bad_match_modifier(self):
        self.match(['foo'], num_errors=1)

    def test_none(self):
        self.match([], 0)

    def test_one(self):
        self.match(['xp'], 1)
        self.match(['win'], 1)
        self.match(['release'], 1)
        self.match(['cpu'], 1)
        self.match(['x86'], 1)
        self.match(['leopard'], -1)
        self.match(['gpu'], -1)
        self.match(['debug'], -1)

    def test_two(self):
        self.match(['xp', 'release'], 2)
        self.match(['win7', 'release'], -1)
        self.match(['win7', 'xp'], 1)

    def test_three(self):
        self.match(['win7', 'xp', 'release'], 2)
        self.match(['xp', 'debug', 'x86'], -1)
        self.match(['xp', 'release', 'x86'], 3)
        self.match(['xp', 'cpu', 'release'], 3)

    def test_four(self):
        self.match(['xp', 'release', 'cpu', 'x86'], 4)
        self.match(['win7', 'xp', 'release', 'cpu'], 3)
        self.match(['win7', 'xp', 'debug', 'cpu'], -1)

    def test_case_insensitivity(self):
        self.match(['Win'], num_errors=1)
        self.match(['WIN'], num_errors=1)
        self.match(['win'], 1)

    def test_duplicates(self):
        self.match(['release', 'release'], num_errors=1)
        self.match(['win-xp', 'xp'], num_errors=1)
        self.match(['win-xp', 'win-xp'], num_errors=1)
        self.match(['xp', 'release', 'xp', 'release'], num_errors=2)
        self.match(['rebaseline', 'rebaseline'], num_errors=1)

    def test_unknown_option(self):
        self.match(['vms'], num_errors=1)

    def test_duplicate_bugs(self):
        # BUG* regexes can appear multiple times.
        self.match(['bugfoo', 'bugbar'], 0)

    def test_invalid_combinations(self):
        # FIXME: This should probably raise an error instead of NO_MATCH.
        self.match(['mac', 'xp'], num_errors=0)

    def test_regexes_are_ignored(self):
        self.match(['bug123xy', 'rebaseline', 'wontfix', 'slow', 'skip'], 0)

    def test_none_is_invalid(self):
        self.match(['none'], num_errors=1)


if __name__ == '__main__':
    unittest.main()
