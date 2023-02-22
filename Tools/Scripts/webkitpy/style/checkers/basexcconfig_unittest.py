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

    def assert_no_error(self, data):
        def handle_style_error(line_number, category, confidence, message):
            self.fail('Unexpected error: %d %s %d %s for\n%s' % (line_number, category, confidence, message, data))
        lines = data.split('\n')
        checker = BaseXcconfigChecker('Base.xcconfig', handle_style_error)
        checker.check(lines)

    def assert_error(self, expected_line_number, expected_category, expected_confidence, data):
        self.had_error = False

        def handle_style_error(line_number, category, confidence, message):
            self.had_error = True
            self.assertEqual(expected_line_number, line_number)
            self.assertEqual(expected_category, category)
            self.assertEqual(expected_confidence, confidence)
        lines = data.split('\n')
        checker = BaseXcconfigChecker('Base.xcconfig', handle_style_error)
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
