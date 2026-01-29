# Copyright (C) 2024 Apple Inc. All rights reserved.
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

import unittest

from webkitpy.api_tests.test_expectations import (
    APITestExpectation,
    APITestExpectationParser,
    APITestExpectationsModel,
    APITestExpectationsLinter,
    VersionSpecifier,
    ConfigurationCategory,
    get_token_category,
    PASS, FAIL, CRASH, TIMEOUT, SKIP, SLOW,
    EXPECTATION_MAP,
    PLATFORM_TOKENS,
    STYLE_TOKENS,
    HARDWARE_TOKENS,
    ARCHITECTURE_TOKENS,
)


class APITestExpectationParserTest(unittest.TestCase):

    def setUp(self):
        self.parser = APITestExpectationParser()

    def test_parse_simple_failure(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Fail ]')
        self.assertEqual(len(results), 1)
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertEqual(exp.test_pattern, 'TestWebKitAPI.WTF.Test')
        self.assertIn(FAIL, exp.expectations)

    def test_parse_multiple_expectations(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Pass Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(PASS, exp.expectations)
        self.assertIn(FAIL, exp.expectations)
        self.assertTrue(exp.is_flaky())

    def test_parse_with_bug_id(self):
        results = self.parser.parse('test.txt', 'webkit.org/b/12345 TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(exp.bug_ids, ('webkit.org/b/12345',))
        self.assertIn(CRASH, exp.expectations)

    def test_parse_with_bug_parenthesis(self):
        results = self.parser.parse('test.txt', 'Bug(test) TestWebKitAPI.WTF.Test [ Timeout ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(exp.bug_ids, ('Bug(test)',))
        self.assertIn(TIMEOUT, exp.expectations)

    def test_parse_skip(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Skip ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.is_skip())

    def test_parse_slow(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.is_slow())
        self.assertIsNone(exp.slow_timeout)  # No custom timeout

    def test_parse_slow_with_custom_timeout(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow:120s ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.is_slow())
        self.assertEqual(exp.slow_timeout, 120)

    def test_parse_slow_with_custom_timeout_no_s(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow:60 ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.is_slow())
        self.assertEqual(exp.slow_timeout, 60)

    def test_parse_slow_timeout_too_large(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow:9999 ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 1)
        self.assertIn('3600', str(warnings[0]))  # Max timeout is 3600 seconds (1 hour)
        self.assertIsNone(exp)

    def test_parse_slow_timeout_zero(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow:0 ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 1)
        self.assertIn('1 and 3600', str(warnings[0]))
        self.assertIsNone(exp)

    def test_parse_configuration_debug(self):
        results = self.parser.parse('test.txt', '[ Debug ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('debug', exp.configurations)
        self.assertIn(FAIL, exp.expectations)

    def test_parse_configuration_release_arm64(self):
        results = self.parser.parse('test.txt', '[ Release arm64 ] TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('release', exp.configurations)
        self.assertIn('arm64', exp.configurations)

    def test_parse_comment_line(self):
        results = self.parser.parse('test.txt', '# This is a comment')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertEqual(len(warnings), 0)

    def test_parse_empty_line(self):
        results = self.parser.parse('test.txt', '')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertEqual(len(warnings), 0)

    def test_parse_inline_comment(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Fail ] # comment')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertEqual(exp.test_pattern, 'TestWebKitAPI.WTF.Test')

    def test_parse_wildcard(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.* [ Skip ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.is_wildcard)
        self.assertEqual(exp.test_pattern, 'TestWebKitAPI.WTF.*')

    def test_parse_error_missing_bracket(self):
        results = self.parser.parse('test.txt', '[ Debug TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertTrue(len(warnings) > 0)

    def test_parse_error_skip_with_other_expectations(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Skip Fail ]')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertTrue(len(warnings) > 0)
        self.assertTrue(any('Skip must not have other' in str(w) for w in warnings))

    def test_parse_error_slow_and_timeout(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow Timeout ]')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertTrue(len(warnings) > 0)
        self.assertTrue(any('Slow and Timeout' in str(w) for w in warnings))

    def test_parse_multiline(self):
        content = '''# Comment
TestWebKitAPI.WTF.Test1 [ Fail ]
TestWebKitAPI.WTF.Test2 [ Pass Crash ]
[ Debug ] TestWebKitAPI.WTF.Test3 [ Skip ]
'''
        results = self.parser.parse('test.txt', content)
        self.assertEqual(len(results), 5)  # Including empty line at start

        # Test1
        exp, warnings = results[1]
        self.assertEqual(exp.test_pattern, 'TestWebKitAPI.WTF.Test1')

        # Test2
        exp, warnings = results[2]
        self.assertTrue(exp.is_flaky())

        # Test3
        exp, warnings = results[3]
        self.assertIn('debug', exp.configurations)


class APITestExpectationTest(unittest.TestCase):

    def test_matches_test_exact(self):
        exp = APITestExpectation('TestWebKitAPI.WTF.Test', {PASS})
        self.assertTrue(exp.matches_test('TestWebKitAPI.WTF.Test'))
        self.assertFalse(exp.matches_test('TestWebKitAPI.WTF.Other'))

    def test_matches_test_wildcard(self):
        exp = APITestExpectation('TestWebKitAPI.WTF.*', {SKIP})
        self.assertTrue(exp.matches_test('TestWebKitAPI.WTF.Test'))
        self.assertTrue(exp.matches_test('TestWebKitAPI.WTF.Other'))
        self.assertTrue(exp.matches_test('TestWebKitAPI.WTF.Suite.Test'))
        self.assertFalse(exp.matches_test('TestWebKitAPI.WebKit.Test'))

    def test_matches_configuration_empty(self):
        exp = APITestExpectation('Test', {PASS}, configurations=set())
        self.assertTrue(exp.matches_configuration(set()))
        self.assertTrue(exp.matches_configuration({'debug'}))

    def test_matches_configuration_single(self):
        exp = APITestExpectation('Test', {PASS}, configurations={'debug'})
        self.assertTrue(exp.matches_configuration({'debug'}))
        self.assertTrue(exp.matches_configuration({'debug', 'arm64'}))
        self.assertFalse(exp.matches_configuration({'release'}))
        self.assertFalse(exp.matches_configuration(set()))

    def test_matches_configuration_multiple(self):
        exp = APITestExpectation('Test', {PASS}, configurations={'debug', 'arm64'})
        self.assertTrue(exp.matches_configuration({'debug', 'arm64'}))
        self.assertTrue(exp.matches_configuration({'debug', 'arm64', 'mac'}))
        self.assertFalse(exp.matches_configuration({'debug'}))
        self.assertFalse(exp.matches_configuration({'arm64'}))

    def test_matches_configuration_with_version_specifier_and_later(self):
        """Test matches_configuration with Sonoma+ style version specifier."""
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        exp = APITestExpectation('Test', {PASS}, configurations={'mac'},
                                 version_specifiers=[spec])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should match mac config + ventura or later
        self.assertTrue(exp.matches_configuration({'mac'}, 'ventura', version_order))
        self.assertTrue(exp.matches_configuration({'mac'}, 'sonoma', version_order))
        self.assertTrue(exp.matches_configuration({'mac'}, 'sequoia', version_order))

        # Should not match earlier versions
        self.assertFalse(exp.matches_configuration({'mac'}, 'monterey', version_order))

        # Should not match without version info
        self.assertFalse(exp.matches_configuration({'mac'}))

    def test_matches_configuration_with_version_specifier_and_earlier(self):
        """Test matches_configuration with Sonoma- style version specifier."""
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_EARLIER)
        exp = APITestExpectation('Test', {PASS}, configurations={'mac'},
                                 version_specifiers=[spec])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should match mac config + ventura or earlier
        self.assertTrue(exp.matches_configuration({'mac'}, 'monterey', version_order))
        self.assertTrue(exp.matches_configuration({'mac'}, 'ventura', version_order))

        # Should not match later versions
        self.assertFalse(exp.matches_configuration({'mac'}, 'sonoma', version_order))
        self.assertFalse(exp.matches_configuration({'mac'}, 'sequoia', version_order))

    def test_matches_configuration_with_version_specifier_range(self):
        """Test matches_configuration with Ventura-Sonoma style version range."""
        spec = VersionSpecifier('ventura', 'sonoma', VersionSpecifier.Type.RANGE)
        exp = APITestExpectation('Test', {PASS}, configurations={'mac'},
                                 version_specifiers=[spec])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should match within range
        self.assertTrue(exp.matches_configuration({'mac'}, 'ventura', version_order))
        self.assertTrue(exp.matches_configuration({'mac'}, 'sonoma', version_order))

        # Should not match outside range
        self.assertFalse(exp.matches_configuration({'mac'}, 'monterey', version_order))
        self.assertFalse(exp.matches_configuration({'mac'}, 'sequoia', version_order))

    def test_matches_configuration_with_version_specifier_exact(self):
        """Test matches_configuration with exact version match."""
        spec = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        exp = APITestExpectation('Test', {PASS}, configurations={'mac'},
                                 version_specifiers=[spec])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should match exact version only
        self.assertTrue(exp.matches_configuration({'mac'}, 'sonoma', version_order))
        self.assertFalse(exp.matches_configuration({'mac'}, 'ventura', version_order))
        self.assertFalse(exp.matches_configuration({'mac'}, 'sequoia', version_order))

    def test_matches_configuration_wrong_platform(self):
        """Test that version specifier match requires platform match too."""
        spec = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.AND_LATER)
        exp = APITestExpectation('Test', {PASS}, configurations={'mac'},
                                 version_specifiers=[spec])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should not match ios even with correct version
        self.assertFalse(exp.matches_configuration({'ios'}, 'sonoma', version_order))

    def test_matches_configuration_multiple_version_specifiers(self):
        """Multiple version specifiers must all match (AND logic)."""
        # Create a range by combining AND_LATER and AND_EARLIER
        spec1 = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        spec2 = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.AND_EARLIER)
        exp = APITestExpectation('Test', {PASS}, version_specifiers=[spec1, spec2])
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        # Should match versions in range [ventura, sonoma]
        self.assertTrue(exp.matches_configuration(set(), 'ventura', version_order))
        self.assertTrue(exp.matches_configuration(set(), 'sonoma', version_order))

        # Should not match versions outside range
        self.assertFalse(exp.matches_configuration(set(), 'monterey', version_order))
        self.assertFalse(exp.matches_configuration(set(), 'sequoia', version_order))

    def test_result_is_expected_pass(self):
        exp = APITestExpectation('Test', {PASS})
        self.assertTrue(exp.result_is_expected(PASS))
        self.assertFalse(exp.result_is_expected(FAIL))

    def test_result_is_expected_flaky(self):
        exp = APITestExpectation('Test', {PASS, FAIL})
        self.assertTrue(exp.result_is_expected(PASS))
        self.assertTrue(exp.result_is_expected(FAIL))
        self.assertFalse(exp.result_is_expected(CRASH))

    def test_result_is_expected_skip(self):
        exp = APITestExpectation('Test', {SKIP})
        self.assertTrue(exp.result_is_expected(SKIP))

    def test_get_timeout_default(self):
        exp = APITestExpectation('Test', {PASS})
        self.assertEqual(exp.get_timeout(30), 30)

    def test_get_timeout_slow(self):
        exp = APITestExpectation('Test', {SLOW})
        self.assertEqual(exp.get_timeout(30), 150)  # 5x default

    def test_get_timeout_custom(self):
        exp = APITestExpectation('Test', {SLOW}, slow_timeout=120)
        self.assertEqual(exp.get_timeout(30), 120)


class APITestExpectationsModelTest(unittest.TestCase):

    def test_add_and_get_expectation(self):
        model = APITestExpectationsModel()
        exp = APITestExpectation('TestWebKitAPI.WTF.Test', {FAIL})
        model.add_expectation(exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, exp)

    def test_get_expectation_not_found(self):
        model = APITestExpectationsModel()
        result = model.get_expectation('NonExistent.Test')
        self.assertIsNone(result)

    def test_wildcard_matching(self):
        model = APITestExpectationsModel()
        exp = APITestExpectation('TestWebKitAPI.WTF.*', {SKIP})
        model.add_expectation(exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, exp)

        result = model.get_expectation('TestWebKitAPI.WebKit.Test')
        self.assertIsNone(result)

    def test_exact_overrides_wildcard(self):
        model = APITestExpectationsModel()
        wildcard_exp = APITestExpectation('TestWebKitAPI.WTF.*', {SKIP})
        exact_exp = APITestExpectation('TestWebKitAPI.WTF.Specific', {FAIL})
        model.add_expectation(wildcard_exp)
        model.add_expectation(exact_exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Specific')
        self.assertEqual(result, exact_exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Other')
        self.assertEqual(result, wildcard_exp)

    def test_more_specific_wildcard_wins(self):
        model = APITestExpectationsModel()
        broad = APITestExpectation('TestWebKitAPI.*', {SKIP})
        specific = APITestExpectation('TestWebKitAPI.WTF.*', {FAIL})
        model.add_expectation(broad)
        model.add_expectation(specific)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, specific)

        result = model.get_expectation('TestWebKitAPI.WebKit.Test')
        self.assertEqual(result, broad)

    def test_get_skipped_tests(self):
        model = APITestExpectationsModel()
        model.add_expectation(APITestExpectation('Test1', {SKIP}))
        model.add_expectation(APITestExpectation('Test2', {FAIL}))
        model.add_expectation(APITestExpectation('TestWTF.*', {SKIP}))

        all_tests = ['Test1', 'Test2', 'TestWTF.A', 'TestWTF.B', 'Other']
        skipped = model.get_skipped_tests(all_tests)

        self.assertIn('Test1', skipped)
        self.assertNotIn('Test2', skipped)
        self.assertIn('TestWTF.A', skipped)
        self.assertIn('TestWTF.B', skipped)
        self.assertNotIn('Other', skipped)

    def test_get_slow_tests(self):
        model = APITestExpectationsModel()
        model.add_expectation(APITestExpectation('Test1', {SLOW}))
        model.add_expectation(APITestExpectation('Test2', {SLOW}, slow_timeout=120))
        model.add_expectation(APITestExpectation('Test3', {FAIL}))

        all_tests = ['Test1', 'Test2', 'Test3']
        slow = model.get_slow_tests(all_tests)

        self.assertIn('Test1', slow)
        self.assertIsNone(slow['Test1'])  # Default 5x
        self.assertIn('Test2', slow)
        self.assertEqual(slow['Test2'], 120)
        self.assertNotIn('Test3', slow)

    def test_configuration_filtering(self):
        model = APITestExpectationsModel()
        debug_exp = APITestExpectation('Test', {FAIL}, configurations={'debug'})
        model.add_expectation(debug_exp)

        # Should match debug config
        result = model.get_expectation('Test', {'debug'})
        self.assertEqual(result, debug_exp)

        # Should not match release config
        result = model.get_expectation('Test', {'release'})
        self.assertIsNone(result)

    def test_get_expectation_or_pass(self):
        model = APITestExpectationsModel()
        model.add_expectation(APITestExpectation('Test1', {FAIL}))

        result = model.get_expectation_or_pass('Test1')
        self.assertIn(FAIL, result.expectations)

        result = model.get_expectation_or_pass('Unknown')
        self.assertIn(PASS, result.expectations)


class VersionSpecifierTest(unittest.TestCase):

    def test_parse_exact_version(self):
        """Exact version should return None from parse (handled separately)."""
        spec = VersionSpecifier.parse('Sonoma')
        self.assertIsNone(spec)  # Exact versions handled by token categorization

    def test_parse_and_later(self):
        spec = VersionSpecifier.parse('Sonoma+')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'sonoma')
        self.assertEqual(spec.type, VersionSpecifier.Type.AND_LATER)
        self.assertIsNone(spec.end_version)

    def test_parse_and_earlier(self):
        spec = VersionSpecifier.parse('Ventura-')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'ventura')
        self.assertEqual(spec.type, VersionSpecifier.Type.AND_EARLIER)
        self.assertIsNone(spec.end_version)

    def test_parse_range(self):
        spec = VersionSpecifier.parse('Ventura-Sequoia')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'ventura')
        self.assertEqual(spec.end_version, 'sequoia')
        self.assertEqual(spec.type, VersionSpecifier.Type.RANGE)

    def test_parse_empty_modifiers(self):
        """Empty strings with just modifiers should return None or handle gracefully."""
        # Just a plus sign - would create empty base_version
        spec = VersionSpecifier.parse('+')
        # This creates a spec with empty base_version, which won't match anything
        if spec:
            self.assertEqual(spec.base_version, '')

        # Just a minus sign
        spec = VersionSpecifier.parse('-')
        if spec:
            self.assertEqual(spec.base_version, '')

    def test_matches_exact(self):
        spec = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertTrue(spec.matches('sonoma', version_order))
        self.assertFalse(spec.matches('ventura', version_order))

    def test_matches_and_later(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertFalse(spec.matches('monterey', version_order))
        self.assertTrue(spec.matches('ventura', version_order))
        self.assertTrue(spec.matches('sonoma', version_order))
        self.assertTrue(spec.matches('sequoia', version_order))

    def test_matches_and_earlier(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_EARLIER)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertTrue(spec.matches('monterey', version_order))
        self.assertTrue(spec.matches('ventura', version_order))
        self.assertFalse(spec.matches('sonoma', version_order))
        self.assertFalse(spec.matches('sequoia', version_order))

    def test_matches_range(self):
        spec = VersionSpecifier('ventura', 'sonoma', VersionSpecifier.Type.RANGE)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertFalse(spec.matches('monterey', version_order))
        self.assertTrue(spec.matches('ventura', version_order))
        self.assertTrue(spec.matches('sonoma', version_order))
        self.assertFalse(spec.matches('sequoia', version_order))

    def test_matches_unknown_version(self):
        """Unknown version in specifier should not match anything."""
        spec = VersionSpecifier('unknown', specifier_type=VersionSpecifier.Type.AND_LATER)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertFalse(spec.matches('sonoma', version_order))
        self.assertFalse(spec.matches('unknown', version_order))

    def test_matches_unknown_current_version(self):
        """Unknown current version should not match."""
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertFalse(spec.matches('unknown', version_order))

    def test_matches_case_insensitive(self):
        """Version matching should be case insensitive."""
        spec = VersionSpecifier('Sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.assertTrue(spec.matches('SONOMA', version_order))
        self.assertTrue(spec.matches('sonoma', version_order))
        self.assertTrue(spec.matches('Sonoma', version_order))


class ConfigurationCategoryTest(unittest.TestCase):

    def test_get_token_category_platform(self):
        self.assertEqual(get_token_category('mac'), ConfigurationCategory.PLATFORM)
        self.assertEqual(get_token_category('ios'), ConfigurationCategory.PLATFORM)
        self.assertEqual(get_token_category('linux'), ConfigurationCategory.PLATFORM)

    def test_get_token_category_style(self):
        self.assertEqual(get_token_category('debug'), ConfigurationCategory.STYLE)
        self.assertEqual(get_token_category('release'), ConfigurationCategory.STYLE)
        self.assertEqual(get_token_category('asan'), ConfigurationCategory.STYLE)
        self.assertEqual(get_token_category('guardmalloc'), ConfigurationCategory.STYLE)

    def test_get_token_category_hardware(self):
        self.assertEqual(get_token_category('simulator'), ConfigurationCategory.HARDWARE)
        self.assertEqual(get_token_category('device'), ConfigurationCategory.HARDWARE)
        self.assertEqual(get_token_category('iphone'), ConfigurationCategory.HARDWARE)
        self.assertEqual(get_token_category('ipad'), ConfigurationCategory.HARDWARE)

    def test_get_token_category_architecture(self):
        self.assertEqual(get_token_category('arm64'), ConfigurationCategory.ARCHITECTURE)
        self.assertEqual(get_token_category('x86_64'), ConfigurationCategory.ARCHITECTURE)

    def test_get_token_category_version_specifier(self):
        self.assertEqual(get_token_category('Sonoma+'), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('Ventura-'), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('Monterey-Sonoma'), ConfigurationCategory.VERSION)

    def test_get_token_category_version_token(self):
        version_tokens = {'sonoma', 'ventura', 'monterey'}
        self.assertEqual(get_token_category('sonoma', version_tokens), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('Ventura', version_tokens), ConfigurationCategory.VERSION)

    def test_get_token_category_flavor(self):
        # Freeform tokens that don't match other categories
        self.assertEqual(get_token_category('wk1'), ConfigurationCategory.FLAVOR)
        self.assertEqual(get_token_category('wk2'), ConfigurationCategory.FLAVOR)
        self.assertEqual(get_token_category('siteisolation'), ConfigurationCategory.FLAVOR)

    def test_category_ordering(self):
        """Categories should be ordered from least to most specific."""
        self.assertLess(ConfigurationCategory.PLATFORM, ConfigurationCategory.VERSION)
        self.assertLess(ConfigurationCategory.VERSION, ConfigurationCategory.STYLE)
        self.assertLess(ConfigurationCategory.STYLE, ConfigurationCategory.HARDWARE)
        self.assertLess(ConfigurationCategory.HARDWARE, ConfigurationCategory.ARCHITECTURE)
        self.assertLess(ConfigurationCategory.ARCHITECTURE, ConfigurationCategory.FLAVOR)


class RadarBugTest(unittest.TestCase):

    def test_parse_radar_simple(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', 'rdar://12345 TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertEqual(exp.bug_ids, ('rdar://12345',))

    def test_parse_radar_with_problem(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', 'rdar://problem/12345 TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertEqual(exp.bug_ids, ('rdar://problem/12345',))

    def test_parse_radar_invalid_format(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', 'rdar://abc TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertTrue(len(warnings) > 0)
        self.assertTrue(any('Invalid radar format' in str(w) for w in warnings))

    def test_parse_multiple_bugs(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', 'webkit.org/b/12345 rdar://67890 TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(len(exp.bug_ids), 2)
        self.assertIn('webkit.org/b/12345', exp.bug_ids)
        self.assertIn('rdar://67890', exp.bug_ids)


class FlavorTokenTest(unittest.TestCase):

    def test_parse_flavor_wk1(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', '[ wk1 ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('wk1', exp.configurations)

    def test_parse_flavor_wk2(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', '[ wk2 ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('wk2', exp.configurations)

    def test_parse_flavor_siteisolation(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', '[ siteisolation ] TestWebKitAPI.WTF.Test [ Pass ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('siteisolation', exp.configurations)

    def test_parse_combined_config_and_flavor(self):
        parser = APITestExpectationParser()
        results = parser.parse('test.txt', '[ mac debug wk2 ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('mac', exp.configurations)
        self.assertIn('debug', exp.configurations)
        self.assertIn('wk2', exp.configurations)


class LinterTest(unittest.TestCase):

    def test_lint_category_ordering(self):
        content = '[ arm64 mac ] TestWebKitAPI.WTF.Test [ Fail ]'  # Wrong order
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('out of order' in w.message for w in warnings))

    def test_lint_correct_category_ordering(self):
        content = '[ mac arm64 ] TestWebKitAPI.WTF.Test [ Fail ]'  # Correct order
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertFalse(any('out of order' in w.message for w in warnings))

    def test_lint_alphabetical_ordering_within_category(self):
        content = '[ release debug ] TestWebKitAPI.WTF.Test [ Fail ]'  # Wrong alpha order
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('alphabetically ordered' in w.message for w in warnings))

    def test_lint_combination_collapse(self):
        # Cover all style values: debug, release, asan, guardmalloc
        content = '''[ debug ] TestWebKitAPI.WTF.Test [ Skip ]
[ release ] TestWebKitAPI.WTF.Test [ Skip ]
[ asan ] TestWebKitAPI.WTF.Test [ Skip ]
[ guardmalloc ] TestWebKitAPI.WTF.Test [ Skip ]'''
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('collapsed' in w.message for w in warnings))

    def test_lint_universal_skip(self):
        content = 'TestWebKitAPI.WTF.Test [ Skip ]'  # Unconditional skip
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('skipped on all configurations' in w.message for w in warnings))

    def test_apply_fixes_reorder(self):
        content = '[ arm64 mac ] TestWebKitAPI.WTF.Test [ Fail ]'
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        linter.lint()
        fixed = linter.apply_fixes()
        self.assertIn('[ mac arm64 ]', fixed)

    def test_no_warnings_for_valid_file(self):
        content = '''# Comment
[ mac debug arm64 ] TestWebKitAPI.WTF.Test1 [ Fail ]
[ ios release ] TestWebKitAPI.WTF.Test2 [ Crash ]
'''
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        # Filter out alphabetical order warnings for test entries
        significant_warnings = [w for w in warnings if 'should appear before' not in w.message]
        self.assertEqual(len(significant_warnings), 0)


class VersionSpecifierParsingTest(unittest.TestCase):
    """Tests for parsing version specifiers through the full parser."""

    def test_parse_version_specifier_and_later(self):
        """Parser should extract Sonoma+ as a version specifier."""
        version_tokens = {'sonoma', 'ventura', 'monterey', 'sequoia'}
        parser = APITestExpectationParser(version_tokens=version_tokens)
        results = parser.parse('test.txt', '[ mac Sonoma+ ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertIn('mac', exp.configurations)
        # Version specifier should be extracted, not in configurations
        self.assertNotIn('sonoma+', exp.configurations)
        self.assertNotIn('sonoma', exp.configurations)
        self.assertEqual(len(exp.version_specifiers), 1)
        self.assertEqual(exp.version_specifiers[0].type, VersionSpecifier.Type.AND_LATER)
        self.assertEqual(exp.version_specifiers[0].base_version, 'sonoma')

    def test_parse_version_specifier_and_earlier(self):
        """Parser should extract Ventura- as a version specifier."""
        version_tokens = {'sonoma', 'ventura', 'monterey'}
        parser = APITestExpectationParser(version_tokens=version_tokens)
        results = parser.parse('test.txt', '[ mac Ventura- ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(len(exp.version_specifiers), 1)
        self.assertEqual(exp.version_specifiers[0].type, VersionSpecifier.Type.AND_EARLIER)
        self.assertEqual(exp.version_specifiers[0].base_version, 'ventura')

    def test_parse_version_specifier_range(self):
        """Parser should extract Ventura-Sonoma as a version range."""
        version_tokens = {'sonoma', 'ventura', 'monterey'}
        parser = APITestExpectationParser(version_tokens=version_tokens)
        results = parser.parse('test.txt', '[ mac Ventura-Sonoma ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(len(exp.version_specifiers), 1)
        self.assertEqual(exp.version_specifiers[0].type, VersionSpecifier.Type.RANGE)
        self.assertEqual(exp.version_specifiers[0].base_version, 'ventura')
        self.assertEqual(exp.version_specifiers[0].end_version, 'sonoma')

    def test_parse_version_specifier_exact(self):
        """Parser should extract plain version name as exact version specifier."""
        version_tokens = {'sonoma', 'ventura', 'monterey'}
        parser = APITestExpectationParser(version_tokens=version_tokens)
        results = parser.parse('test.txt', '[ mac Sonoma ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(len(exp.version_specifiers), 1)
        self.assertEqual(exp.version_specifiers[0].type, VersionSpecifier.Type.EXACT)
        self.assertEqual(exp.version_specifiers[0].base_version, 'sonoma')


class EmptyModelTest(unittest.TestCase):
    """Tests for empty model operations."""

    def test_empty_model_get_expectation(self):
        model = APITestExpectationsModel()
        self.assertIsNone(model.get_expectation('NonExistent'))

    def test_empty_model_get_expectation_or_pass(self):
        model = APITestExpectationsModel()
        result = model.get_expectation_or_pass('NonExistent')
        self.assertIsNotNone(result)
        self.assertIn(PASS, result.expectations)

    def test_empty_model_get_skipped_tests(self):
        model = APITestExpectationsModel()
        all_tests = ['Test1', 'Test2', 'Test3']
        skipped = model.get_skipped_tests(all_tests)
        self.assertEqual(len(skipped), 0)

    def test_empty_model_get_slow_tests(self):
        model = APITestExpectationsModel()
        all_tests = ['Test1', 'Test2', 'Test3']
        slow = model.get_slow_tests(all_tests)
        self.assertEqual(len(slow), 0)

    def test_empty_model_all_expectations(self):
        model = APITestExpectationsModel()
        self.assertEqual(len(model.all_expectations()), 0)


class LinterAllSkipTest(unittest.TestCase):
    """Tests to verify the all_skip bug fix."""

    def test_all_skip_with_single_skip_entry(self):
        """Single unconditional skip should trigger warning."""
        content = 'TestWebKitAPI.WTF.Test [ Skip ]'
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('skipped on all configurations' in w.message for w in warnings))

    def test_all_skip_with_non_skip_entry(self):
        """Non-skip entry should not trigger universal skip warning."""
        content = 'TestWebKitAPI.WTF.Test [ Fail ]'
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertFalse(any('skipped on all configurations' in w.message for w in warnings))

    def test_all_skip_with_mixed_entries(self):
        """Mixed skip and non-skip for same test should not trigger warning."""
        content = '''[ debug ] TestWebKitAPI.WTF.Test [ Skip ]
[ release ] TestWebKitAPI.WTF.Test [ Fail ]'''
        linter = APITestExpectationsLinter(None, content, 'test.txt')
        warnings = linter.lint()
        self.assertFalse(any('skipped on all configurations' in w.message for w in warnings))


if __name__ == '__main__':
    unittest.main()
