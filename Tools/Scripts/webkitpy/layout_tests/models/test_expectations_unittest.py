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

import unittest

from webkitpy.layout_tests import port
from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.models.test_configuration import *
from webkitpy.layout_tests.models.test_expectations import *
from webkitpy.layout_tests.models.test_configuration import *


class MockBugManager(object):
    def close_bug(self, bug_id, reference_bug_id=None):
        pass

    def create_bug(self):
        return "BUG_NEWLY_CREATED"


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
        # FIXME: Remove this routine and just reference test names directly.
        return test_name

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

    def test_get_modifiers(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEqual(self._exp.get_modifiers(
                         self.get_test('passes/text.html')), [])

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
        s = self._exp.get_test_set(WONTFIX)
        self.assertEqual(s,
            set([self.get_test('failures/expected/crash.html'),
                 self.get_test('failures/expected/image_checksum.html')]))
        s = self._exp.get_test_set(WONTFIX, CRASH)
        self.assertEqual(s,
            set([self.get_test('failures/expected/crash.html')]))
        s = self._exp.get_test_set(WONTFIX, CRASH, include_skips=False)
        self.assertEqual(s, set([]))

    def test_parse_error_fatal(self):
        try:
            self.parse_exp("""FOO : failures/expected/text.html = TEXT
SKIP : failures/expected/image.html""")
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError, e:
            self.assertTrue(e.fatal)
            exp_errors = [u"Line:1 Unrecognized modifier 'foo' failures/expected/text.html",
                          u"Line:2 Missing expectations SKIP : failures/expected/image.html"]
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
BUG_TEST WIN : failures/expected/text.html = TEXT
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', TEXT)


class SemanticTests(Base):
    def test_bug_format(self):
        self.assertRaises(ParseError, self.parse_exp, 'BUG1234 : failures/expected/text.html = TEXT')

    def test_missing_bugid(self):
        # This should log a non-fatal error.
        self.parse_exp('SLOW : failures/expected/text.html = TEXT')
        self.assertTrue(self._exp.has_warnings())

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
        self.assertTrue(self._exp.has_warnings(), 1)


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

    def test_macro_overrides(self):
        exp_str = """
BUG_TEST WIN : passes/text.html = PASS
BUG_TEST XP : passes/text.html = TEXT
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


class TestExpectationParserTests(unittest.TestCase):
    def test_tokenize_blank(self):
        expectation = TestExpectationParser.tokenize('')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, None)
        self.assertEqual(len(expectation.errors), 0)

    def test_tokenize_missing_colon(self):
        expectation = TestExpectationParser.tokenize('Qux.')
        self.assertEqual(expectation.is_malformed(), True)
        self.assertEqual(str(expectation.errors), '["Missing a \':\'"]')

    def test_tokenize_extra_colon(self):
        expectation = TestExpectationParser.tokenize('FOO : : bar')
        self.assertEqual(expectation.is_malformed(), True)
        self.assertEqual(str(expectation.errors), '["Extraneous \':\'"]')

    def test_tokenize_empty_comment(self):
        expectation = TestExpectationParser.tokenize('//')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, '')
        self.assertEqual(len(expectation.errors), 0)

    def test_tokenize_comment(self):
        expectation = TestExpectationParser.tokenize('//Qux.')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(len(expectation.errors), 0)

    def test_tokenize_missing_equal(self):
        expectation = TestExpectationParser.tokenize('FOO : bar')
        self.assertEqual(expectation.is_malformed(), True)
        self.assertEqual(str(expectation.errors), "['Missing expectations\']")

    def test_tokenize_extra_equal(self):
        expectation = TestExpectationParser.tokenize('FOO : bar = BAZ = Qux.')
        self.assertEqual(expectation.is_malformed(), True)
        self.assertEqual(str(expectation.errors), '["Extraneous \'=\'"]')

    def test_tokenize_valid(self):
        expectation = TestExpectationParser.tokenize('FOO : bar = BAZ')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, None)
        self.assertEqual(len(expectation.errors), 0)

    def test_tokenize_valid_with_comment(self):
        expectation = TestExpectationParser.tokenize('FOO : bar = BAZ //Qux.')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(str(expectation.modifiers), '[\'foo\']')
        self.assertEqual(str(expectation.expectations), '[\'baz\']')
        self.assertEqual(len(expectation.errors), 0)

    def test_tokenize_valid_with_multiple_modifiers(self):
        expectation = TestExpectationParser.tokenize('FOO1 FOO2 : bar = BAZ //Qux.')
        self.assertEqual(expectation.is_malformed(), False)
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(str(expectation.modifiers), '[\'foo1\', \'foo2\']')
        self.assertEqual(str(expectation.expectations), '[\'baz\']')
        self.assertEqual(len(expectation.errors), 0)

    def test_parse_empty_string(self):
        test_port = port.get('test-win-xp', None)
        test_port.test_exists = lambda test: True
        test_config = test_port.test_configuration()
        full_test_list = []
        expectation_line = TestExpectationParser.tokenize('')
        parser = TestExpectationParser(test_port, full_test_list, allow_rebaseline_modifier=False)
        parser.parse(expectation_line)
        self.assertFalse(expectation_line.is_invalid())


class TestExpectationSerializerTests(unittest.TestCase):
    def __init__(self, testFunc):
        test_port = port.get('test-win-xp', None)
        self._converter = TestConfigurationConverter(test_port.all_test_configurations(), test_port.configuration_specifier_macros())
        self._serializer = TestExpectationSerializer(self._converter)
        unittest.TestCase.__init__(self, testFunc)

    def assert_round_trip(self, in_string, expected_string=None):
        expectation = TestExpectationParser.tokenize(in_string)
        if expected_string is None:
            expected_string = in_string
        self.assertEqual(expected_string, self._serializer.to_string(expectation))

    def assert_list_round_trip(self, in_string, expected_string=None):
        expectations = TestExpectationParser.tokenize_list(in_string)
        if expected_string is None:
            expected_string = in_string
        self.assertEqual(expected_string, TestExpectationSerializer.list_to_string(expectations, self._converter))

    def test_unparsed_to_string(self):
        expectation = TestExpectationLine()
        serializer = TestExpectationSerializer()

        self.assertEqual(serializer.to_string(expectation), '')
        expectation.comment = 'Qux.'
        self.assertEqual(serializer.to_string(expectation), '//Qux.')
        expectation.name = 'bar'
        self.assertEqual(serializer.to_string(expectation), ' : bar =  //Qux.')
        expectation.modifiers = ['foo']
        self.assertEqual(serializer.to_string(expectation), 'FOO : bar =  //Qux.')
        expectation.expectations = ['bAz']
        self.assertEqual(serializer.to_string(expectation), 'FOO : bar = BAZ //Qux.')
        expectation.expectations = ['bAz1', 'baZ2']
        self.assertEqual(serializer.to_string(expectation), 'FOO : bar = BAZ1 BAZ2 //Qux.')
        expectation.modifiers = ['foo1', 'foO2']
        self.assertEqual(serializer.to_string(expectation), 'FOO1 FOO2 : bar = BAZ1 BAZ2 //Qux.')
        expectation.errors.append('Oh the horror.')
        self.assertEqual(serializer.to_string(expectation), '')
        expectation.original_string = 'Yes it is!'
        self.assertEqual(serializer.to_string(expectation), 'Yes it is!')

    def test_unparsed_list_to_string(self):
        expectation = TestExpectationLine()
        expectation.comment = 'Qux.'
        expectation.name = 'bar'
        expectation.modifiers = ['foo']
        expectation.expectations = ['bAz1', 'baZ2']
        self.assertEqual(TestExpectationSerializer.list_to_string([expectation]), 'FOO : bar = BAZ1 BAZ2 //Qux.')

    def test_parsed_to_string(self):
        expectation_line = TestExpectationLine()
        expectation_line.parsed_bug_modifiers = ['BUGX']
        expectation_line.name = 'test/name/for/realz.html'
        expectation_line.parsed_expectations = set([IMAGE])
        self.assertEqual(self._serializer.to_string(expectation_line), None)
        expectation_line.matching_configurations = set([TestConfiguration('xp', 'x86', 'release', 'cpu')])
        self.assertEqual(self._serializer.to_string(expectation_line), 'BUGX XP RELEASE CPU : test/name/for/realz.html = IMAGE')
        expectation_line.matching_configurations = set([TestConfiguration('xp', 'x86', 'release', 'cpu'), TestConfiguration('xp', 'x86', 'release', 'gpu')])
        self.assertEqual(self._serializer.to_string(expectation_line), 'BUGX XP RELEASE : test/name/for/realz.html = IMAGE')
        expectation_line.matching_configurations = set([TestConfiguration('xp', 'x86', 'release', 'cpu'), TestConfiguration('xp', 'x86', 'debug', 'gpu')])
        self.assertEqual(self._serializer.to_string(expectation_line), 'BUGX XP RELEASE CPU : test/name/for/realz.html = IMAGE\nBUGX XP DEBUG GPU : test/name/for/realz.html = IMAGE')

    def test_parsed_expectations_string(self):
        expectation_line = TestExpectationLine()
        expectation_line.parsed_expectations = set([])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), '')
        expectation_line.parsed_expectations = set([IMAGE_PLUS_TEXT])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'image+text')
        expectation_line.parsed_expectations = set([PASS, FAIL])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'pass fail')
        expectation_line.parsed_expectations = set([FAIL, PASS])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'pass fail')

    def test_parsed_modifier_string(self):
        expectation_line = TestExpectationLine()
        expectation_line.parsed_bug_modifiers = ['garden-o-matic']
        expectation_line.parsed_modifiers = ['for', 'the']
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, []), 'garden-o-matic for the')
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, ['win']), 'garden-o-matic for the win')
        expectation_line.parsed_bug_modifiers = []
        expectation_line.parsed_modifiers = []
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, []), '')
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, ['win']), 'win')
        expectation_line.parsed_bug_modifiers = ['garden-o-matic', 'total', 'is']
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, ['win']), 'garden-o-matic is total win')
        expectation_line.parsed_bug_modifiers = []
        expectation_line.parsed_modifiers = ['garden-o-matic', 'total', 'is']
        self.assertEqual(self._serializer._parsed_modifier_string(expectation_line, ['win']), 'garden-o-matic is total win')

    def test_format_result(self):
        self.assertEqual(TestExpectationSerializer._format_result('modifiers', 'name', 'expectations', 'comment'), 'MODIFIERS : name = EXPECTATIONS //comment')
        self.assertEqual(TestExpectationSerializer._format_result('modifiers', 'name', 'expectations', None), 'MODIFIERS : name = EXPECTATIONS')

    def test_string_roundtrip(self):
        self.assert_round_trip('')
        self.assert_round_trip('FOO')
        self.assert_round_trip(':')
        self.assert_round_trip('FOO :')
        self.assert_round_trip('FOO : bar')
        self.assert_round_trip('  FOO :')
        self.assert_round_trip('    FOO : bar')
        self.assert_round_trip('FOO : bar = BAZ')
        self.assert_round_trip('FOO : bar = BAZ //Qux.')
        self.assert_round_trip('FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO : bar = BAZ // Qux.     ')
        self.assert_round_trip('FOO : bar = BAZ //        Qux.     ')
        self.assert_round_trip('FOO : : bar = BAZ')
        self.assert_round_trip('FOO : : bar = BAZ')
        self.assert_round_trip('FOO : : bar ==== BAZ')
        self.assert_round_trip('=')
        self.assert_round_trip('//')
        self.assert_round_trip('// ')
        self.assert_round_trip('// Foo')
        self.assert_round_trip('// Foo')
        self.assert_round_trip('// Foo :')
        self.assert_round_trip('// Foo : =')

    def test_list_roundtrip(self):
        self.assert_list_round_trip('')
        self.assert_list_round_trip('\n')
        self.assert_list_round_trip('\n\n')
        self.assert_list_round_trip('bar')
        self.assert_list_round_trip('bar\n//Qux.')
        self.assert_list_round_trip('bar\n//Qux.\n')

    def test_reconstitute_only_these(self):
        lines = []
        reconstitute_only_these = []

        def add_line(matching_configurations, reconstitute):
            expectation_line = TestExpectationLine()
            expectation_line.original_string = "Nay"
            expectation_line.parsed_bug_modifiers = ['BUGX']
            expectation_line.name = 'Yay'
            expectation_line.parsed_expectations = set([IMAGE])
            expectation_line.matching_configurations = matching_configurations
            lines.append(expectation_line)
            if reconstitute:
                reconstitute_only_these.append(expectation_line)

        add_line(set([TestConfiguration('xp', 'x86', 'release', 'cpu')]), False)
        add_line(set([TestConfiguration('xp', 'x86', 'release', 'cpu'), TestConfiguration('xp', 'x86', 'release', 'gpu')]), True)
        add_line(set([TestConfiguration('xp', 'x86', 'release', 'cpu'), TestConfiguration('xp', 'x86', 'debug', 'gpu')]), False)
        serialized = TestExpectationSerializer.list_to_string(lines, self._converter)
        self.assertEquals(serialized, "BUGX XP RELEASE CPU : Yay = IMAGE\nBUGX XP RELEASE : Yay = IMAGE\nBUGX XP RELEASE CPU : Yay = IMAGE\nBUGX XP DEBUG GPU : Yay = IMAGE")
        serialized = TestExpectationSerializer.list_to_string(lines, self._converter, reconstitute_only_these=reconstitute_only_these)
        self.assertEquals(serialized, "Nay\nBUGX XP RELEASE : Yay = IMAGE\nNay")

    def test_string_whitespace_stripping(self):
        self.assert_round_trip('\n', '')
        self.assert_round_trip('  FOO : bar = BAZ', 'FOO : bar = BAZ')
        self.assert_round_trip('FOO    : bar = BAZ', 'FOO : bar = BAZ')
        self.assert_round_trip('FOO : bar = BAZ       // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO : bar =        BAZ // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO :       bar =    BAZ // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO :       bar     =    BAZ // Qux.', 'FOO : bar = BAZ // Qux.')


class TestExpectationEditorTests(unittest.TestCase):
    WIN_RELEASE_CPU_CONFIGS = set([
        TestConfiguration('vista', 'x86', 'release', 'cpu'),
        TestConfiguration('win7', 'x86', 'release', 'cpu'),
        TestConfiguration('xp', 'x86', 'release', 'cpu'),
    ])

    RELEASE_CONFIGS = set([
        TestConfiguration('vista', 'x86', 'release', 'cpu'),
        TestConfiguration('win7', 'x86', 'release', 'cpu'),
        TestConfiguration('xp', 'x86', 'release', 'cpu'),
        TestConfiguration('vista', 'x86', 'release', 'gpu'),
        TestConfiguration('win7', 'x86', 'release', 'gpu'),
        TestConfiguration('xp', 'x86', 'release', 'gpu'),
        TestConfiguration('snowleopard', 'x86', 'release', 'cpu'),
        TestConfiguration('leopard', 'x86', 'release', 'cpu'),
        TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
        TestConfiguration('leopard', 'x86', 'release', 'gpu'),
        TestConfiguration('lucid', 'x86', 'release', 'cpu'),
        TestConfiguration('lucid', 'x86_64', 'release', 'cpu'),
        TestConfiguration('lucid', 'x86', 'release', 'gpu'),
        TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
    ])

    def __init__(self, testFunc):
        self.test_port = port.get('test-win-xp', None)
        self.full_test_list = ['failures/expected/keyboard.html', 'failures/expected/audio.html']
        unittest.TestCase.__init__(self, testFunc)

    def make_parsed_expectation_lines(self, in_string):
        expectation_lines = TestExpectationParser.tokenize_list(in_string)
        parser = TestExpectationParser(self.test_port, self.full_test_list, allow_rebaseline_modifier=False)
        for expectation_line in expectation_lines:
            self.assertFalse(expectation_line.is_invalid())
            parser.parse(expectation_line)
        return expectation_lines

    def assert_remove_roundtrip(self, in_string, test, expected_string, remove_flakes=False):
        test_config_set = set([self.test_port.test_configuration()])
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        editor.remove_expectation(test, test_config_set, remove_flakes)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        result = TestExpectationSerializer.list_to_string(expectation_lines, converter)
        self.assertEquals(result, expected_string)

    def assert_update_roundtrip(self, in_string, test, expectation_set, expected_string, expected_update_count, remove_flakes=False, parsed_bug_modifiers=None, test_configs=None):
        test_config_set = test_configs or set([self.test_port.test_configuration()])
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        updated_expectation_lines = editor.update_expectation(test, test_config_set, expectation_set, parsed_bug_modifiers=parsed_bug_modifiers)
        for updated_expectation_line in updated_expectation_lines:
            self.assertTrue(updated_expectation_line in expectation_lines)
        self.assertEquals(len(updated_expectation_lines), expected_update_count)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        result = TestExpectationSerializer.list_to_string(expectation_lines, converter)
        self.assertEquals(result, expected_string)

    def test_remove_expectation(self):
        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/hang.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 MAC : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html',  """
BUGX1 MAC : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 LINUX MAC VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected = PASS
BUGX2 XP RELEASE : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 WIN : failures/expected = PASS
BUGX2 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected = FAIL""", 'failures/expected/keyboard.html', """
BUGX1 WIN : failures/expected = FAIL""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE PASS
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = PASS IMAGE
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE PASS
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", remove_flakes=True)

    def test_remove_expectation_multiple(self):
        in_string = """
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE"""
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        test = "failures/expected/keyboard.html"

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'release', 'cpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'cpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('vista', 'x86', 'debug', 'gpu'), TestConfiguration('win7', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG GPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA RELEASE : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'gpu'), TestConfiguration('xp', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA RELEASE : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('vista', 'x86', 'debug', 'cpu'), TestConfiguration('vista', 'x86', 'debug', 'gpu'), TestConfiguration('vista', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set(self.test_port.all_test_configurations()))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        test = "failures/expected/audio.html"

        editor.remove_expectation(test, set(self.test_port.all_test_configurations()))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), "")

    def test_update_expectation(self):
        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), '', 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE CPU : failures/expected = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP RELEASE CPU : failures/expected = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = PASS""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([TEXT]), """
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 0)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1, parsed_bug_modifiers=['BUGAWESOME'])

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, parsed_bug_modifiers=['BUGAWESOME'])

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT""", 1)

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), '', 1, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 LINUX MAC RELEASE CPU : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 MAC : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 MAC : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

    def test_update_expectation_relative(self):
        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT
BUGX2 MAC : failures/expected/audio.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 MAC : failures/expected/audio.html = TEXT""", 2, parsed_bug_modifiers=['BUGAWESOME'])

    def test_update_expectation_multiple(self):
        in_string = """
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE"""
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        test = "failures/expected/keyboard.html"

        editor.update_expectation(test, set([TestConfiguration('xp', 'x86', 'release', 'cpu')]), set([IMAGE_PLUS_TEXT]), ['BUG_UPDATE1'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUG_UPDATE1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE+TEXT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'cpu')]), set([TEXT]), ['BUG_UPDATE2'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE+TEXT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, self.WIN_RELEASE_CPU_CONFIGS, set([CRASH]), ['BUG_UPDATE3'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA XP GPU : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE3 WIN RELEASE CPU : failures/expected/keyboard.html = CRASH
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, self.RELEASE_CONFIGS, set([FAIL]), ['BUG_UPDATE4'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP DEBUG GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE4 RELEASE : failures/expected/keyboard.html = FAIL
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, set(self.test_port.all_test_configurations()), set([TIMEOUT]), ['BUG_UPDATE5'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUG_UPDATE5 : failures/expected/keyboard.html = TIMEOUT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")


if __name__ == '__main__':
    unittest.main()
