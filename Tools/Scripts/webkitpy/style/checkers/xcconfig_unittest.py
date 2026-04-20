# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
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

from webkitpy.style.checkers.xcconfig import XcconfigChecker


class XcconfigCheckerTest(unittest.TestCase):

    def setUp(self):
        self.errors = []

        def handle_style_error(line_number, category, confidence, message):
            self.errors.append((line_number, category, confidence, message))

        self.checker = XcconfigChecker("fake_path.xcconfig", handle_style_error)

    def test_versioned_sdk_conditional_detected(self):
        """Test that versioned SDK conditionals are properly detected and flagged."""
        lines = [
            'WK_FOO[sdk=macosx26*] = some_value;',
            'ANOTHER_SETTING[sdk=iphoneos15*] = another_value;',
            'THIRD_SETTING[sdk=iphonesimulator16.0*] = third_value;'
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 3)

        # Check first error - versioned SDK conditional
        self.assertEqual(self.errors[0][0], 1)  # Line number
        self.assertEqual(self.errors[0][1], 'xcconfig/sdk-conditional-prefer-deployment-target')  # Category
        self.assertIn('macosx26*', self.errors[0][3])  # Message contains sdk condition
        self.assertIn('WebKitTargetConditionals.xcconfig', self.errors[0][3])

        # Check second error
        self.assertEqual(self.errors[1][0], 2)
        self.assertEqual(self.errors[1][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('iphoneos15*', self.errors[1][3])

        # Check third error
        self.assertEqual(self.errors[2][0], 3)
        self.assertEqual(self.errors[2][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('iphonesimulator16.0*', self.errors[2][3])

    def test_unversioned_sdk_conditionals_flagged(self):
        """Test that unversioned SDK conditionals are flagged with WK_PLATFORM_NAME recommendation."""
        lines = [
            'PLATFORM_SETTING[sdk=macosx*] = value;',
            'ANOTHER_PLATFORM[sdk=iphoneos*] = another_value;',
            'THIRD_PLATFORM[sdk=iphonesimulator*] = third_value;',
            'FOURTH_PLATFORM[sdk=watchos*] = fourth_value;'
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 4)

        # Check that all are flagged with the WK_PLATFORM_NAME category
        for i, error in enumerate(self.errors):
            self.assertEqual(error[0], i + 1)  # Line number
            self.assertEqual(error[1], 'xcconfig/sdk-conditional-prefer-wk-platform')  # Category
            self.assertIn('WK_PLATFORM_NAME', error[3])  # Message recommends WK_PLATFORM_NAME

    def test_valid_lines_not_flagged(self):
        """Test that valid xcconfig lines without SDK conditionals are not flagged."""
        lines = [
            '// This is a comment',
            '',
            'NORMAL_SETTING = value;',
            'SETTING_WITH_VARIABLE = $(SOME_VAR);',
            'WK_FOO = $(WK_FOO$(WK_MACOS_2600));',
            'WK_FOO_MACOS_SINCE_2600 = some_value;',
            'WK_PLATFORM_SETTING = $(WK_PLATFORM_SETTING$(WK_PLATFORM_NAME));',
            '#include "Base.xcconfig"'
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 0)

    def test_mixed_content(self):
        """Test files with both versioned and unversioned SDK conditionals."""
        lines = [
            '// Configuration file',
            'VALID_SETTING = value;',
            'UNVERSIONED_SDK[sdk=macosx*] = platform_specific;',  # Should be flagged with wk-platform
            '',
            'ANOTHER_VALID = $(SOME_VAR);',
            'VERSIONED_SDK[sdk=iphoneos15*] = version_specific;',  # Should be flagged with deployment-target
            '// End of file'
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 2)

        # Check unversioned SDK conditional
        self.assertEqual(self.errors[0][0], 3)
        self.assertEqual(self.errors[0][1], 'xcconfig/sdk-conditional-prefer-wk-platform')
        self.assertIn('macosx*', self.errors[0][3])
        self.assertIn('WK_PLATFORM_NAME', self.errors[0][3])

        # Check versioned SDK conditional
        self.assertEqual(self.errors[1][0], 6)
        self.assertEqual(self.errors[1][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('iphoneos15*', self.errors[1][3])
        self.assertIn('WebKitTargetConditionals.xcconfig', self.errors[1][3])

    def test_whitespace_handling(self):
        """Test that various whitespace patterns are handled correctly."""
        lines = [
            '   VERSIONED[sdk=macosx26*]   =   value;  ',
            '\tUNVERSIONED[sdk=iphoneos*]\t=\tvalue;\t',
            'NO_SPACE[sdk=tvos9*]=value;'
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 3)

        # Check categories are correct
        self.assertEqual(self.errors[0][1], 'xcconfig/sdk-conditional-prefer-deployment-target')  # versioned
        self.assertEqual(self.errors[1][1], 'xcconfig/sdk-conditional-prefer-wk-platform')       # unversioned
        self.assertEqual(self.errors[2][1], 'xcconfig/sdk-conditional-prefer-deployment-target')  # versioned

    def test_complex_setting_names(self):
        """Test that complex setting names with underscores and prefixes work."""
        lines = [
            'WK_COMPLEX_SETTING_NAME[sdk=macosx26*] = value;',         # versioned
            'OTHER_WEBKIT_FEATURE[sdk=iphoneos16*] = feature_value;',  # versioned
            'ENABLE_SOMETHING[sdk=watchos9*] = YES;',                  # versioned
            'UNVERSIONED_COMPLEX[sdk=macosx*] = platform_value;'       # unversioned
        ]
        self.checker.check(lines)
        self.assertEqual(len(self.errors), 4)

        # Check that versioned ones use deployment-target category
        self.assertEqual(self.errors[0][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('macosx26*', self.errors[0][3])

        self.assertEqual(self.errors[1][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('iphoneos16*', self.errors[1][3])

        self.assertEqual(self.errors[2][1], 'xcconfig/sdk-conditional-prefer-deployment-target')
        self.assertIn('watchos9*', self.errors[2][3])

        # Check that unversioned one uses wk-platform category
        self.assertEqual(self.errors[3][1], 'xcconfig/sdk-conditional-prefer-wk-platform')
        self.assertIn('macosx*', self.errors[3][3])


if __name__ == '__main__':
    unittest.main()
