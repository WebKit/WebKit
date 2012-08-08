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

from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.models.test_configuration import *
from webkitpy.layout_tests.models.test_expectations import *
from webkitpy.layout_tests.models.test_configuration import *

try:
    from collections import OrderedDict
except ImportError:
    # Needed for Python < 2.7
    from webkitpy.thirdparty.ordered_dict import OrderedDict


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

    def test_suffixes_for_expectations(self):
        self.assertEquals(suffixes_for_expectations(set([TEXT])), set(['txt']))
        self.assertEquals(suffixes_for_expectations(set([IMAGE_PLUS_TEXT])), set(['txt', 'png']))
        self.assertEquals(suffixes_for_expectations(set([IMAGE])), set(['png']))
        self.assertEquals(suffixes_for_expectations(set([AUDIO])), set(['wav']))
        self.assertEquals(suffixes_for_expectations(set([TEXT, IMAGE, CRASH])), set(['txt', 'png']))
        self.assertEquals(suffixes_for_expectations(set()), set())


class Base(unittest.TestCase):
    # Note that all of these tests are written assuming the configuration
    # being tested is Windows XP, Release build.

    def __init__(self, testFunc):
        host = MockHost()
        self._port = host.port_factory.get('test-win-xp', None)
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
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        self._port.expectations_dict = lambda: expectations_dict
        self._exp = TestExpectations(self._port, self.get_basic_tests(), is_lint_mode)

    def assert_exp(self, test, result):
        self.assertEquals(self._exp.get_expectations(self.get_test(test)),
                          set([result]))

    def assert_bad_expectations(self, expectations, overrides=None):
        self.assertRaises(ParseError, self.parse_exp, expectations, is_lint_mode=True, overrides=overrides)


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

    def test_parse_warning(self):
        try:
            self.parse_exp("""FOO : failures/expected/text.html = TEXT
SKIP : failures/expected/image.html""", is_lint_mode=True)
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError, e:
            warnings = ("expectations:1 Test lacks BUG modifier. failures/expected/text.html\n"
                        "expectations:1 Unrecognized modifier 'foo' failures/expected/text.html\n"
                        "expectations:2 Missing expectations SKIP : failures/expected/image.html")
            self.assertEqual(str(e), warnings)

        try:
            self.parse_exp('SKIP : failures/expected/text.html = TEXT', is_lint_mode=True)
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError, e:
            warnings = u'expectations:1 Test lacks BUG modifier. failures/expected/text.html'
            self.assertEqual(str(e), warnings)

    def test_error_on_different_platform(self):
        # parse_exp uses a Windows port. Assert errors on Mac show up in lint mode.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST MAC : failures/expected/text.html = TEXT\nBUG_TEST MAC : failures/expected/text.html = TEXT',
            is_lint_mode=True)

    def test_error_on_different_build_type(self):
        # parse_exp uses a Release port. Assert errors on DEBUG show up in lint mode.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST DEBUG : failures/expected/text.html = TEXT\nBUG_TEST DEBUG : failures/expected/text.html = TEXT',
            is_lint_mode=True)

    def test_overrides(self):
        self.parse_exp("BUG_EXP: failures/expected/text.html = TEXT",
                       "BUG_OVERRIDE : failures/expected/text.html = IMAGE")
        self.assert_exp('failures/expected/text.html', IMAGE)

    def test_overrides__directory(self):
        self.parse_exp("BUG_EXP: failures/expected/text.html = TEXT",
                       "BUG_OVERRIDE: failures/expected = CRASH")
        self.assert_exp('failures/expected/text.html', CRASH)
        self.assert_exp('failures/expected/image.html', CRASH)

    def test_overrides__duplicate(self):
        self.assert_bad_expectations("BUG_EXP: failures/expected/text.html = TEXT",
                                     "BUG_OVERRIDE : failures/expected/text.html = IMAGE\n"
                                     "BUG_OVERRIDE : failures/expected/text.html = CRASH\n")

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


class SkippedTests(Base):
    def check(self, expectations, overrides, skips, lint=False):
        port = MockHost().port_factory.get('qt')
        port._filesystem.write_text_file(port._filesystem.join(port.layout_tests_dir(), 'failures/expected/text.html'), 'foo')
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        port.expectations_dict = lambda: expectations_dict
        port.skipped_layout_tests = lambda tests: set(skips)
        exp = TestExpectations(port, ['failures/expected/text.html'], lint)

        # Check that the expectation is for BUG_DUMMY SKIP : ... = PASS
        self.assertEquals(exp.get_modifiers('failures/expected/text.html'),
                          [TestExpectationParser.DUMMY_BUG_MODIFIER, TestExpectationParser.SKIP_MODIFIER, TestExpectationParser.WONTFIX_MODIFIER])
        self.assertEquals(exp.get_expectations('failures/expected/text.html'), set([PASS]))

    def test_skipped_tests_work(self):
        self.check(expectations='', overrides=None, skips=['failures/expected/text.html'])

    def test_duplicate_skipped_test_fails_lint(self):
        self.assertRaises(ParseError, self.check, expectations='BUGX : failures/expected/text.html = text\n', overrides=None, skips=['failures/expected/text.html'], lint=True)

    def test_skipped_file_overrides_expectations(self):
        self.check(expectations='BUGX : failures/expected/text.html = TEXT\n', overrides=None,
                   skips=['failures/expected/text.html'])

    def test_skipped_dir_overrides_expectations(self):
        self.check(expectations='BUGX : failures/expected/text.html = TEXT\n', overrides=None,
                   skips=['failures/expected'])

    def test_skipped_file_overrides_overrides(self):
        self.check(expectations='', overrides='BUGX : failures/expected/text.html = TEXT\n',
                   skips=['failures/expected/text.html'])

    def test_skipped_dir_overrides_overrides(self):
        self.check(expectations='', overrides='BUGX : failures/expected/text.html = TEXT\n',
                   skips=['failures/expected'])


class ExpectationSyntaxTests(Base):
    def test_missing_expectation(self):
        # This is missing the expectation.
        self.assert_bad_expectations('BUG_TEST: failures/expected/text.html')

    def test_missing_colon(self):
        # This is missing the modifiers and the ':'
        self.assert_bad_expectations('failures/expected/text.html = TEXT')

    def test_too_many_colons(self):
        self.assert_bad_expectations('BUG_TEST: failures/expected/text.html = PASS :')

    def test_too_many_equals_signs(self):
        self.assert_bad_expectations('BUG_TEST: failures/expected/text.html = TEXT = IMAGE')

    def test_unrecognized_expectation(self):
        self.assert_bad_expectations('BUG_TEST: failures/expected/text.html = UNKNOWN')

    def test_macro(self):
        exp_str = """
BUG_TEST WIN : failures/expected/text.html = TEXT
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', TEXT)


class SemanticTests(Base):
    def test_bug_format(self):
        self.assertRaises(ParseError, self.parse_exp, 'BUG1234 : failures/expected/text.html = TEXT', is_lint_mode=True)

    def test_bad_bugid(self):
        try:
            self.parse_exp('BUG1234 SLOW : failures/expected/text.html = TEXT', is_lint_mode=True)
            self.fail('should have raised an error about a bad bug identifier')
        except ParseError, exp:
            self.assertEquals(len(exp.warnings), 1)

    def test_missing_bugid(self):
        self.parse_exp('SLOW : failures/expected/text.html = TEXT')
        self.assertTrue(self._exp.has_warnings())

    def test_slow_and_timeout(self):
        # A test cannot be SLOW and expected to TIMEOUT.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST SLOW : failures/expected/timeout.html = TIMEOUT', is_lint_mode=True)

    def test_rebaseline(self):
        # Can't lint a file w/ 'REBASELINE' in it.
        self.assertRaises(ParseError, self.parse_exp,
            'BUG_TEST REBASELINE : failures/expected/text.html = TEXT',
            is_lint_mode=True)

    def test_duplicates(self):
        self.assertRaises(ParseError, self.parse_exp, """
BUG_EXP : failures/expected/text.html = TEXT
BUG_EXP : failures/expected/text.html = IMAGE""", is_lint_mode=True)

        self.assertRaises(ParseError, self.parse_exp,
            self.get_basic_expectations(), overrides="""
BUG_OVERRIDE : failures/expected/text.html = TEXT
BUG_OVERRIDE : failures/expected/text.html = IMAGE""", is_lint_mode=True)

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
        self.assert_bad_expectations("BUG_TEST RELEASE : passes/text.html = PASS\n"
                                     "BUG_TEST WIN : passes/text.html = FAIL\n")

    def test_more_modifiers(self):
        self.assert_bad_expectations("BUG_TEST RELEASE : passes/text.html = PASS\n"
                                     "BUG_TEST WIN RELEASE : passes/text.html = TEXT\n")

    def test_order_in_file(self):
        self.assert_bad_expectations("BUG_TEST WIN RELEASE : passes/text.html = TEXT\n"
                                     "BUG_TEST RELEASE : passes/text.html = PASS\n")

    def test_macro_overrides(self):
        self.assert_bad_expectations("BUG_TEST WIN : passes/text.html = PASS\n"
                                     "BUG_TEST XP : passes/text.html = TEXT\n")


class RemoveConfigurationsTest(Base):
    def test_remove(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-xp', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {"expectations": """BUGX LINUX WIN RELEASE : failures/expected/foo.html = TEXT
BUGY WIN MAC DEBUG : failures/expected/foo.html = CRASH
"""}
        expectations = TestExpectations(test_port, self.get_basic_tests())

        actual_expectations = expectations.remove_configuration_from_test('failures/expected/foo.html', test_config)

        self.assertEqual("""BUGX LINUX VISTA WIN7 RELEASE : failures/expected/foo.html = TEXT
BUGY WIN MAC DEBUG : failures/expected/foo.html = CRASH
""", actual_expectations)

    def test_remove_line(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-xp', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """BUGX WIN RELEASE : failures/expected/foo.html = TEXT
BUGY WIN DEBUG : failures/expected/foo.html = CRASH
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configuration_from_test('failures/expected/foo.html', test_config)
        actual_expectations = expectations.remove_configuration_from_test('failures/expected/foo.html', host.port_factory.get('test-win-vista', None).test_configuration())
        actual_expectations = expectations.remove_configuration_from_test('failures/expected/foo.html', host.port_factory.get('test-win-win7', None).test_configuration())

        self.assertEqual("""BUGY WIN DEBUG : failures/expected/foo.html = CRASH
""", actual_expectations)


class RebaseliningTest(Base):
    """Test rebaselining-specific functionality."""
    def assertRemove(self, input_expectations, tests, expected_expectations, filename):
        self.parse_exp(input_expectations, is_lint_mode=False)
        actual_expectations = self._exp.remove_rebaselined_tests(tests, filename)
        self.assertEqual(expected_expectations, actual_expectations)

    def test_remove(self):
        self.assertRemove('BUGX REBASELINE : failures/expected/text.html = TEXT\n'
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n',
                          ['failures/expected/text.html'],
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n',
                          'expectations')

        # test that we don't remove lines from the expectations if we're asking for the overrides
        self.assertRemove('BUGX REBASELINE : failures/expected/text.html = TEXT\n'
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n',
                          ['failures/expected/text.html'],
                          'BUGX REBASELINE : failures/expected/text.html = TEXT\n'
                          'BUGY : failures/expected/image.html = IMAGE\n'
                          'BUGZ REBASELINE : failures/expected/crash.html = CRASH\n',
                          'overrides')


    def test_no_get_rebaselining_failures(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEqual(len(self._exp.get_rebaselining_failures()), 0)


class TestExpectationParserTests(unittest.TestCase):
    def _tokenize(self, line):
        return TestExpectationParser._tokenize('path', line, 0)

    def test_tokenize_blank(self):
        expectation = self._tokenize('')
        self.assertEqual(expectation.comment, None)
        self.assertEqual(len(expectation.warnings), 0)

    def test_tokenize_missing_colon(self):
        expectation = self._tokenize('Qux.')
        self.assertEqual(str(expectation.warnings), '["Missing a \':\'"]')

    def test_tokenize_extra_colon(self):
        expectation = self._tokenize('FOO : : bar')
        self.assertEqual(str(expectation.warnings), '["Extraneous \':\'"]')

    def test_tokenize_empty_comment(self):
        expectation = self._tokenize('//')
        self.assertEqual(expectation.comment, '')
        self.assertEqual(len(expectation.warnings), 0)

    def test_tokenize_comment(self):
        expectation = self._tokenize('//Qux.')
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(len(expectation.warnings), 0)

    def test_tokenize_missing_equal(self):
        expectation = self._tokenize('FOO : bar')
        self.assertEqual(str(expectation.warnings), "['Missing expectations\']")

    def test_tokenize_extra_equal(self):
        expectation = self._tokenize('FOO : bar = BAZ = Qux.')
        self.assertEqual(str(expectation.warnings), '["Extraneous \'=\'"]')

    def test_tokenize_valid(self):
        expectation = self._tokenize('FOO : bar = BAZ')
        self.assertEqual(expectation.comment, None)
        self.assertEqual(len(expectation.warnings), 0)

    def test_tokenize_valid_with_comment(self):
        expectation = self._tokenize('FOO : bar = BAZ //Qux.')
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(str(expectation.modifiers), '[\'foo\']')
        self.assertEqual(str(expectation.expectations), '[\'baz\']')
        self.assertEqual(len(expectation.warnings), 0)

    def test_tokenize_valid_with_multiple_modifiers(self):
        expectation = self._tokenize('FOO1 FOO2 : bar = BAZ //Qux.')
        self.assertEqual(expectation.comment, 'Qux.')
        self.assertEqual(str(expectation.modifiers), '[\'foo1\', \'foo2\']')
        self.assertEqual(str(expectation.expectations), '[\'baz\']')
        self.assertEqual(len(expectation.warnings), 0)

    def test_parse_empty_string(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-xp', None)
        test_port.test_exists = lambda test: True
        full_test_list = []
        expectation_line = self._tokenize('')
        parser = TestExpectationParser(test_port, full_test_list, allow_rebaseline_modifier=False)
        parser._parse_line(expectation_line)
        self.assertFalse(expectation_line.is_invalid())


class TestExpectationSerializerTests(unittest.TestCase):
    def __init__(self, testFunc):
        host = MockHost()
        test_port = host.port_factory.get('test-win-xp', None)
        self._converter = TestConfigurationConverter(test_port.all_test_configurations(), test_port.configuration_specifier_macros())
        self._serializer = TestExpectationSerializer(self._converter)
        unittest.TestCase.__init__(self, testFunc)

    def _tokenize(self, line):
        return TestExpectationParser._tokenize('path', line, 0)

    def assert_round_trip(self, in_string, expected_string=None):
        expectation = self._tokenize(in_string)
        if expected_string is None:
            expected_string = in_string
        self.assertEqual(expected_string, self._serializer.to_string(expectation))

    def assert_list_round_trip(self, in_string, expected_string=None):
        expectations = TestExpectationParser._tokenize_list('path', in_string)
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
        expectation.warnings.append('Oh the horror.')
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
        expectation_line.matching_configurations = set([TestConfiguration('xp', 'x86', 'release')])
        self.assertEqual(self._serializer.to_string(expectation_line), 'BUGX XP RELEASE : test/name/for/realz.html = IMAGE')
        expectation_line.matching_configurations = set([TestConfiguration('xp', 'x86', 'release'), TestConfiguration('xp', 'x86', 'debug')])
        self.assertEqual(self._serializer.to_string(expectation_line), 'BUGX XP : test/name/for/realz.html = IMAGE')

    def test_parsed_expectations_string(self):
        expectation_line = TestExpectationLine()
        expectation_line.parsed_expectations = set([])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), '')
        expectation_line.parsed_expectations = set([IMAGE_PLUS_TEXT])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'image+text')
        expectation_line.parsed_expectations = set([PASS, IMAGE])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'pass image')
        expectation_line.parsed_expectations = set([TEXT, PASS])
        self.assertEqual(self._serializer._parsed_expectations_string(expectation_line), 'pass text')

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

        add_line(set([TestConfiguration('xp', 'x86', 'release')]), True)
        add_line(set([TestConfiguration('xp', 'x86', 'release'), TestConfiguration('xp', 'x86', 'debug')]), False)
        serialized = TestExpectationSerializer.list_to_string(lines, self._converter)
        self.assertEquals(serialized, "BUGX XP RELEASE : Yay = IMAGE\nBUGX XP : Yay = IMAGE")
        serialized = TestExpectationSerializer.list_to_string(lines, self._converter, reconstitute_only_these=reconstitute_only_these)
        self.assertEquals(serialized, "BUGX XP RELEASE : Yay = IMAGE\nNay")

    def test_string_whitespace_stripping(self):
        self.assert_round_trip('\n', '')
        self.assert_round_trip('  FOO : bar = BAZ', 'FOO : bar = BAZ')
        self.assert_round_trip('FOO    : bar = BAZ', 'FOO : bar = BAZ')
        self.assert_round_trip('FOO : bar = BAZ       // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO : bar =        BAZ // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO :       bar =    BAZ // Qux.', 'FOO : bar = BAZ // Qux.')
        self.assert_round_trip('FOO :       bar     =    BAZ // Qux.', 'FOO : bar = BAZ // Qux.')


if __name__ == '__main__':
    unittest.main()
