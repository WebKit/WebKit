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

"""Tests for expectations model."""

import unittest

from webkitexpectationspy.model import ExpectationsModel
from webkitexpectationspy.expectations import Expectation, ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase, LayoutTestModifiers
from webkitexpectationspy.version_specifier import VersionSpecifier

PASS = ResultStatus.PASS
FAIL = ResultStatus.FAIL
CRASH = ResultStatus.CRASH
TIMEOUT = ResultStatus.TIMEOUT


class ExpectationsModelTest(unittest.TestCase):

    def test_add_and_get_expectation(self):
        model = ExpectationsModel()
        exp = Expectation('TestWebKitAPI.WTF.Test', expected={FAIL})
        model.add_expectation(exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, exp)

    def test_get_expectation_not_found(self):
        model = ExpectationsModel()
        result = model.get_expectation('NonExistent.Test')
        self.assertIsNone(result)

    def test_wildcard_matching(self):
        model = ExpectationsModel()
        exp = Expectation('TestWebKitAPI.WTF.*', modifiers=ModifiersBase(skip=True))
        model.add_expectation(exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, exp)

        result = model.get_expectation('TestWebKitAPI.WebKit.Test')
        self.assertIsNone(result)

    def test_exact_overrides_wildcard(self):
        model = ExpectationsModel()
        wildcard_exp = Expectation('TestWebKitAPI.WTF.*', modifiers=ModifiersBase(skip=True))
        exact_exp = Expectation('TestWebKitAPI.WTF.Specific', expected={FAIL})
        model.add_expectation(wildcard_exp)
        model.add_expectation(exact_exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Specific')
        self.assertEqual(result, exact_exp)

        result = model.get_expectation('TestWebKitAPI.WTF.Other')
        self.assertEqual(result, wildcard_exp)

    def test_more_specific_wildcard_wins(self):
        model = ExpectationsModel()
        broad = Expectation('TestWebKitAPI.*', modifiers=ModifiersBase(skip=True))
        specific = Expectation('TestWebKitAPI.WTF.*', expected={FAIL})
        model.add_expectation(broad)
        model.add_expectation(specific)

        result = model.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertEqual(result, specific)

        result = model.get_expectation('TestWebKitAPI.WebKit.Test')
        self.assertEqual(result, broad)

    def test_get_skipped_tests(self):
        model = ExpectationsModel()
        model.add_expectation(Expectation('Test1', modifiers=ModifiersBase(skip=True)))
        model.add_expectation(Expectation('Test2', expected={FAIL}))
        model.add_expectation(Expectation('TestWTF.*', modifiers=ModifiersBase(skip=True)))

        all_tests = ['Test1', 'Test2', 'TestWTF.A', 'TestWTF.B', 'Other']
        skipped = model.get_skipped_tests(all_tests)

        self.assertIn('Test1', skipped)
        self.assertNotIn('Test2', skipped)
        self.assertIn('TestWTF.A', skipped)
        self.assertIn('TestWTF.B', skipped)

    def test_get_slow_tests(self):
        model = ExpectationsModel()
        model.add_expectation(Expectation('Test1', modifiers=ModifiersBase(slow=-1.0)))
        model.add_expectation(Expectation('Test2', modifiers=ModifiersBase(slow=120.0)))
        model.add_expectation(Expectation('Test3', expected={FAIL}))

        all_tests = ['Test1', 'Test2', 'Test3']
        slow = model.get_slow_tests(all_tests)

        self.assertIn('Test1', slow)
        self.assertIsNone(slow['Test1'])
        self.assertEqual(slow['Test2'], 120.0)
        self.assertNotIn('Test3', slow)

    def test_get_wontfix_tests(self):
        model = ExpectationsModel()
        model.add_expectation(Expectation('Test1', modifiers=LayoutTestModifiers(wontfix=True)))
        model.add_expectation(Expectation('Test2', expected={FAIL}))

        all_tests = ['Test1', 'Test2']
        wontfix = model.get_wontfix_tests(all_tests)

        self.assertIn('Test1', wontfix)
        self.assertNotIn('Test2', wontfix)

    def test_configuration_filtering(self):
        model = ExpectationsModel()
        debug_exp = Expectation('Test', expected={FAIL}, configurations={'debug'})
        model.add_expectation(debug_exp)

        result = model.get_expectation('Test', {'debug'})
        self.assertEqual(result, debug_exp)

        result = model.get_expectation('Test', {'release'})
        self.assertIsNone(result)

    def test_version_specifier_matching(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        exp = Expectation('Test', expected={FAIL}, configurations={'mac'},
                          version_specifiers=[spec])
        model = ExpectationsModel()
        model.add_expectation(exp)

        version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

        result = model.get_expectation('Test', {'mac'}, 'sonoma', version_order)
        self.assertEqual(result, exp)

        result = model.get_expectation('Test', {'mac'}, 'monterey', version_order)
        self.assertIsNone(result)

    def test_get_expectation_or_pass(self):
        model = ExpectationsModel()
        model.add_expectation(Expectation('Test1', expected={FAIL}))

        result = model.get_expectation_or_pass('Test1')
        self.assertIn(FAIL, result.expected)

        result = model.get_expectation_or_pass('Unknown')
        self.assertIn(PASS, result.expected)

    def test_empty_model(self):
        model = ExpectationsModel()
        self.assertIsNone(model.get_expectation('Test'))
        self.assertEqual(len(model.get_skipped_tests(['Test'])), 0)
        self.assertEqual(len(model.get_slow_tests(['Test'])), 0)
        self.assertEqual(len(model.all_expectations()), 0)

    def test_clear(self):
        model = ExpectationsModel()
        model.add_expectation(Expectation('Test', expected={FAIL}))
        self.assertIsNotNone(model.get_expectation('Test'))

        model.clear()
        self.assertIsNone(model.get_expectation('Test'))

    def test_same_pattern_different_configurations(self):
        model = ExpectationsModel()

        mac_exp = Expectation('Test', expected={FAIL}, configurations={'mac'})
        warning1 = model.add_expectation(mac_exp)
        self.assertIsNone(warning1)

        linux_exp = Expectation('Test', expected={PASS}, configurations={'linux'})
        warning2 = model.add_expectation(linux_exp)
        self.assertIsNone(warning2)

        result_mac = model.get_expectation('Test', {'mac'})
        self.assertEqual(result_mac, mac_exp)

        result_linux = model.get_expectation('Test', {'linux'})
        self.assertEqual(result_linux, linux_exp)

    def test_true_duplicate_same_configuration(self):
        model = ExpectationsModel()

        exp1 = Expectation('Test', expected={FAIL}, configurations={'mac'}, filename='test.txt', line_number=1)
        warning1 = model.add_expectation(exp1)
        self.assertIsNone(warning1)

        exp2 = Expectation('Test', expected={PASS}, configurations={'mac'}, filename='test.txt', line_number=5)
        warning2 = model.add_expectation(exp2)
        self.assertIsNotNone(warning2)
        self.assertIn('Duplicate', warning2.error)

    def test_skip_with_expected_results(self):
        model = ExpectationsModel()
        exp = Expectation('Test', expected={FAIL}, modifiers=ModifiersBase(skip=True))
        model.add_expectation(exp)

        result = model.get_expectation('Test')
        self.assertTrue(result.skip)
        self.assertIn(FAIL, result.expected)

    def test_combined_modifiers(self):
        model = ExpectationsModel()
        exp = Expectation('Test',
                          expected={FAIL},
                          modifiers=ModifiersBase(skip=True, slow=-1.0))
        model.add_expectation(exp)

        result = model.get_expectation('Test')
        self.assertTrue(result.skip)
        self.assertTrue(result.slow)

    def test_expectation_ordering(self):
        exp_fail = Expectation('Test', expected={FAIL})
        exp_fail_crash = Expectation('Test', expected={FAIL, CRASH})
        self.assertTrue(exp_fail_crash > exp_fail)
        self.assertTrue(exp_fail < exp_fail_crash)

    def test_expectation_frozen(self):
        exp = Expectation('Test', expected={FAIL})
        with self.assertRaises(AttributeError):
            exp.test_pattern = 'Other'

    def test_expectation_hashable(self):
        exp1 = Expectation('Test', expected={FAIL})
        exp2 = Expectation('Test', expected={FAIL})
        self.assertEqual(hash(exp1), hash(exp2))
        self.assertEqual({exp1, exp2}, {exp1})


if __name__ == '__main__':
    unittest.main()
