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

"""Unit tests for deprecated_js_test_includes.py."""

import unittest

from webkitpy.style.checkers.deprecated_js_test_includes import DeprecatedJSTestIncludesChecker


class DeprecatedJSTestIncludesCheckerTest(unittest.TestCase):

    def _collect_errors(self, lines):
        errors = []

        def record(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        checker = DeprecatedJSTestIncludesChecker(record)
        checker.check(lines)
        return errors

    def assertError(self, lines, expected_line_number, expected_message_substring):
        errors = self._collect_errors(lines)
        self.assertEqual(1, len(errors), 'expected exactly one error, got: {}'.format(errors))
        line_number, category, confidence, message = errors[0]
        self.assertEqual(expected_line_number, line_number)
        self.assertEqual('build/deprecated/js-test-helpers', category)
        self.assertEqual(5, confidence)
        self.assertIn(expected_message_substring, message)
        self.assertIn('js-test.js', message)

    def assertNoError(self, lines):
        errors = self._collect_errors(lines)
        self.assertEqual([], errors)

    def test_flags_pre_include_relative(self):
        self.assertError(
            ['<script src="../../resources/js-test-pre.js"></script>'],
            1,
            'js-test-pre.js',
        )

    def test_flags_post_include_relative(self):
        self.assertError(
            ['<script src="../../resources/js-test-post.js"></script>'],
            1,
            'js-test-post.js',
        )

    def test_flags_absolute_http_path(self):
        self.assertError(
            ['<script src="/js-test-resources/js-test-pre.js"></script>'],
            1,
            'js-test-pre.js',
        )

    def test_flags_single_quoted_src(self):
        self.assertError(
            ["<script src='resources/js-test-pre.js'></script>"],
            1,
            'js-test-pre.js',
        )

    def test_flags_with_type_attribute(self):
        self.assertError(
            ['<script type="text/javascript" src="../../resources/js-test-pre.js"></script>'],
            1,
            'js-test-pre.js',
        )

    def test_flags_with_query_string(self):
        self.assertError(
            ['<script src="resources/js-test-pre.js?v=2"></script>'],
            1,
            'js-test-pre.js',
        )

    def test_reports_correct_line_number(self):
        self.assertError(
            [
                '<html>',
                '<head>',
                '<script src="../../resources/js-test-post.js"></script>',
                '</head>',
            ],
            3,
            'js-test-post.js',
        )

    def test_does_not_flag_modern_js_test(self):
        self.assertNoError(['<script src="../../resources/js-test.js"></script>'])

    def test_flags_post_async(self):
        self.assertError(
            ['<script src="../../resources/js-test-post-async.js"></script>'],
            1,
            'js-test-post-async.js',
        )

    def test_does_not_flag_unrelated_script(self):
        self.assertNoError(
            ['<script src="../../resources/unrelated-helper.js"></script>'],
        )

    def test_does_not_flag_plain_filename_mention(self):
        # Just referencing the filename in prose or expectations should not fire;
        # the rule is about <script src=...> includes.
        self.assertNoError(['This test once used js-test-pre.js as a helper.'])

    def test_case_insensitive_tag(self):
        self.assertError(
            ['<SCRIPT SRC="../../resources/js-test-pre.js"></SCRIPT>'],
            1,
            'js-test-pre.js',
        )


if __name__ == '__main__':
    unittest.main()
