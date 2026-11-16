# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""Unit tests for computed_style_inline_includes.py."""

import unittest

from webkitpy.style.checkers.computed_style_inline_includes import ComputedStyleInlineIncludesChecker


class ComputedStyleInlineIncludesCheckerTest(unittest.TestCase):

    def _collect_errors(self, file_path, lines, line_numbers=None):
        errors = []

        def record(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        checker = ComputedStyleInlineIncludesChecker(file_path, record)
        checker.check(lines, line_numbers)
        return errors

    def assertError(self, file_path, lines, expected_line_number, expected_header, line_numbers=None):
        errors = self._collect_errors(file_path, lines, line_numbers)
        self.assertEqual(1, len(errors), 'expected exactly one error, got: {}'.format(errors))
        line_number, category, confidence, message = errors[0]
        self.assertEqual(expected_line_number, line_number)
        self.assertEqual('build/include/computed-style-inlines', category)
        self.assertEqual(5, confidence)
        self.assertIn(expected_header, message)
        self.assertIn('6 seconds', message)
        self.assertIn('out of line helper', message)

    def assertNoError(self, file_path, lines, line_numbers=None):
        errors = self._collect_errors(file_path, lines, line_numbers)
        self.assertEqual([], errors)

    def test_flags_getters_include_quoted(self):
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include "StyleComputedStyle+GettersInlines.h"'],
            1,
            'StyleComputedStyle+GettersInlines.h',
        )

    def test_flags_setters_include_quoted(self):
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include "StyleComputedStyle+SettersInlines.h"'],
            1,
            'StyleComputedStyle+SettersInlines.h',
        )

    def test_flags_angle_bracket_include(self):
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include <WebCore/StyleComputedStyle+GettersInlines.h>'],
            1,
            'StyleComputedStyle+GettersInlines.h',
        )

    def test_flags_include_with_directory_prefix(self):
        self.assertError(
            'Source/WebKitLegacy/mac/DOM/DOM.mm',
            ['#include "style/computed/StyleComputedStyle+SettersInlines.h"'],
            1,
            'StyleComputedStyle+SettersInlines.h',
        )

    def test_reports_correct_line_number(self):
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            [
                '#include "config.h"',
                '#include "WebPage.h"',
                '#include "StyleComputedStyle+GettersInlines.h"',
            ],
            3,
            'StyleComputedStyle+GettersInlines.h',
        )

    def test_exempt_style_directory(self):
        self.assertNoError(
            'Source/WebCore/style/StyleAdjuster.cpp',
            ['#include "StyleComputedStyle+GettersInlines.h"'],
        )

    def test_exempt_rendering_directory(self):
        self.assertNoError(
            'Source/WebCore/rendering/RenderBox.cpp',
            ['#include "StyleComputedStyle+GettersInlines.h"'],
        )

    def test_exempt_layout_directory(self):
        self.assertNoError(
            'Source/WebCore/layout/LayoutState.cpp',
            ['#include "StyleComputedStyle+SettersInlines.h"'],
        )

    def test_exempt_header_in_computed_subdirectory_of_style(self):
        self.assertNoError(
            'Source/WebCore/style/computed/StyleComputedStyle+GettersInlines.h',
            ['#include "StyleComputedStyle+SettersInlines.h"'],
        )

    def test_does_not_flag_unrelated_include(self):
        self.assertNoError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include "StyleComputedStyle.h"'],
        )

    def test_does_not_flag_other_inline_header(self):
        self.assertNoError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include "StyleComputedStyleBase+GettersInlines.h"'],
        )

    def test_does_not_flag_plain_filename_mention(self):
        self.assertNoError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['// See StyleComputedStyle+GettersInlines.h for the closure.'],
        )

    def test_only_checks_modified_lines(self):
        lines = [
            '#include "StyleComputedStyle+GettersInlines.h"',
            '#include "StyleComputedStyle+SettersInlines.h"',
        ]
        # Only line 2 was modified, so only it should be flagged.
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            lines,
            2,
            'StyleComputedStyle+SettersInlines.h',
            line_numbers=[2],
        )

    def test_empty_line_numbers_suppresses_all_errors(self):
        self.assertNoError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['#include "StyleComputedStyle+GettersInlines.h"'],
            line_numbers=[],
        )

    def test_handles_extra_whitespace(self):
        self.assertError(
            'Source/WebKit/WebProcess/WebPage/WebPage.cpp',
            ['  #  include   "StyleComputedStyle+GettersInlines.h"'],
            1,
            'StyleComputedStyle+GettersInlines.h',
        )


if __name__ == '__main__':
    unittest.main()
