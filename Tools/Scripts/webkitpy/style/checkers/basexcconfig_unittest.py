# Copyright (C) 2010-2023 Apple Inc. All rights reserved.
# Copyright (C) 2011 Patrick Gansterer <paroga@paroga.com>
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit test for basexcconfig.py."""

import unittest

from webkitpy.style.checkers.basexcconfig import BaseXcconfigChecker


class BaseXcconfigCheckerTest(unittest.TestCase):
    """Tests BaseXcconfigChecker class."""

    # The CommonBase.xcconfig variables the check() tests below assert against. Supplying them
    # directly keeps those tests self-contained.
    def make_checker(self, file_path, handle_style_error):
        checker = BaseXcconfigChecker(file_path, handle_style_error)
        inherited_vars = {name: 1 for name in (
            'GCC_PREPROCESSOR_DEFINITIONS',
            'OTHER_CFLAGS',
            'OTHER_CPLUSPLUSFLAGS',
            'OTHER_LDFLAGS',
            'SWIFT_ACTIVE_COMPILATION_CONDITIONS',
        )}
        wk_common_vars = {name: 1 for name in (
            'OTHER_CFLAGS',
            'OTHER_CPLUSPLUSFLAGS',
            'OTHER_LDFLAGS',
        )}
        checker.read_common_base_xcconfig_variables = lambda: (inherited_vars, {}, wk_common_vars)
        return checker

    def assert_no_error(self, data):
        def handle_style_error(line_number, category, confidence, message):
            self.fail('Unexpected error: %d %s %d %s for\n%s' % (line_number, category, confidence, message, data))
        lines = data.split('\n')
        checker = self.make_checker('Base.xcconfig', handle_style_error)
        checker.check(lines)

    def assert_error(self, expected_line_number, expected_category, expected_confidence, data):
        self.had_error = False

        def handle_style_error(line_number, category, confidence, message):
            self.had_error = True
            self.assertEqual(expected_line_number, line_number)
            self.assertEqual(expected_category, category)
            self.assertEqual(expected_confidence, confidence)
        lines = data.split('\n')
        checker = self.make_checker('Base.xcconfig', handle_style_error)
        checker.check(lines)
        self.assertTrue(self.had_error)

    def mock_handle_style_error(self):
        pass

    def test_init(self):
        checker = BaseXcconfigChecker('Base.xcconfig', self.mock_handle_style_error)
        self.assertEqual(checker._file_path, 'Base.xcconfig')
        self.assertEqual(checker._handle_style_error, self.mock_handle_style_error)

    def test_gcc_optimization_level(self):
        self.assert_error(1, 'basexcconfig/missing-wk_default-prefix', 5,
                          'GCC_OPTIMIZATION_LEVEL = 3;\n')
        self.assert_error(1, 'basexcconfig/missing-wk_default-prefix', 5,
                          'GCC_OPTIMIZATION_LEVEL[config=Debug] = 0;\n')

    def test_wk_default_gcc_optimization_level(self):
        self.assert_no_error('WK_DEFAULT_GCC_OPTIMIZATION_LEVEL = 3;\n')
        self.assert_no_error('WK_DEFAULT_GCC_OPTIMIZATION_LEVEL[config=Debug] = 0;\n')

    def test_other_cflags(self):
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'OTHER_CFLAGS = -DMOCK;\n')
        self.assert_no_error('OTHER_CFLAGS = $(inherited) -DMOCK;\n')
        self.assert_no_error('OTHER_CFLAGS = $(WK_COMMON_OTHER_CFLAGS) -DMOCK;\n')

    def test_other_cplusplusflags(self):
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'OTHER_CPLUSPLUSFLAGS = -DMOCK;\n')
        self.assert_no_error('OTHER_CPLUSPLUSFLAGS = $(inherited) -DMOCK;\n')
        self.assert_no_error('OTHER_CPLUSPLUSFLAGS = $(WK_COMMON_OTHER_CPLUSPLUSFLAGS) -DMOCK;\n')

    def test_other_ldflags(self):
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'OTHER_LDFLAGS = -fsanitize=address;\n')
        self.assert_no_error('OTHER_LDFLAGS = $(inherited) -fsanitize=address;\n')
        self.assert_no_error('OTHER_LDFLAGS = $(WK_COMMON_OTHER_LDFLAGS) -fsanitize=address;\n')

    def test_gcc_preprocessor_definitions(self):
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'GCC_PREPROCESSOR_DEFINITIONS = $(DEBUG_DEFINES);\n')
        self.assert_no_error('GCC_PREPROCESSOR_DEFINITIONS = $(inherited) $(DEBUG_DEFINES);\n')
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'GCC_PREPROCESSOR_DEFINITIONS = $(WK_COMMON_GCC_PREPROCESSOR_DEFINITIONS) $(DEBUG_DEFINES);\n')

    def test_swift_active_compilation_conditions(self):
        # SWIFT_ACTIVE_COMPILATION_CONDITIONS may also be set by CommonBase.xcconfig.
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'SWIFT_ACTIVE_COMPILATION_CONDITIONS = WTF_SWIFT_CXX_INTEROP;\n')
        self.assert_no_error('SWIFT_ACTIVE_COMPILATION_CONDITIONS = $(inherited) WTF_SWIFT_CXX_INTEROP;\n')
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'SWIFT_ACTIVE_COMPILATION_CONDITIONS = $(WK_COMMON_SWIFT_ACTIVE_COMPILATION_CONDITIONS) WTF_SWIFT_CXX_INTEROP;\n')

    def test_nested_inherited_is_not_an_append(self):
        # MACOSX_DEPLOYMENT_TARGET is only used as a fallback by SDKVariant.xcconfig
        # ($(WK_MACOSX_DEPLOYMENT_TARGET:default=$(inherited))), so overriding it is allowed.
        self.assert_no_error('MACOSX_DEPLOYMENT_TARGET = 15.0;\n')

        # A nested $(inherited) does not append to the inherited value either, so it does not
        # satisfy the requirement to preserve it.
        self.assert_error(1, 'basexcconfig/missing-inherited', 5,
                          'OTHER_CFLAGS = $(WK_MOCK_CFLAGS:default=$(inherited));\n')

    def test_duplicate_commonbase_definitions(self):
        # $(inherited) already includes the value CommonBase.xcconfig contributes, so also listing
        # $(WK_COMMON_<name>) duplicates it; the $(WK_COMMON_<name>) reference must be removed.
        self.assert_error(1, 'basexcconfig/duplicate-commonbase-definitions', 5,
                          'GCC_PREPROCESSOR_DEFINITIONS = $(inherited) $(WK_COMMON_GCC_PREPROCESSOR_DEFINITIONS) $(DEBUG_DEFINES);\n')
        self.assert_error(1, 'basexcconfig/duplicate-commonbase-definitions', 5,
                          'OTHER_CFLAGS = $(inherited) $(WK_COMMON_OTHER_CFLAGS) -DMOCK;\n')

    def test_line_continuation_is_not_a_separate_assignment(self):
        # A setting may span physical lines with a trailing '\'. The continuation lines can contain
        # '=' (e.g. a preprocessor macro value), which must be treated as part of the same logical
        # line rather than parsed as a separate assignment that falsely trips the checker.
        self.assert_no_error('OTHER_CFLAGS = $(inherited) \\\n    OTHER_LDFLAGS=-fmock;\n')

    def test_read_xcconfig_variables_joins_line_continuations(self):
        # read_xcconfig_variables must splice '\' continuations so that '=' on a continuation line
        # (a macro value, not a setting) is not recorded as a spurious CommonBase override variable.
        lines = ('GCC_PREPROCESSOR_DEFINITIONS = $(inherited) \\\n'
                 '    ENABLE_FOO=1 \\\n'
                 '    ENABLE_BAR=2;\n').split('\n')
        checker = BaseXcconfigChecker('Base.xcconfig', self.mock_handle_style_error)
        inherited_vars, override_vars, wk_common_vars = {}, {}, {}
        checker.read_xcconfig_variables(lines, inherited_vars, override_vars, wk_common_vars)
        self.assertEqual(sorted(inherited_vars), ['GCC_PREPROCESSOR_DEFINITIONS'])
        self.assertEqual(override_vars, {})
