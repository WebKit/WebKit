# Copyright (C) 2026 Apple Inc. All rights reserved.
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
#     * Neither the name of Apple Inc. nor the names of its
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

from webkitpy.layout_tests.interop.testharness_results import parse_subtest_counts, SubtestCounts


class TestharnessResultsTest(unittest.TestCase):
    def test_typical_output(self):
        text = (
            "This is a testharness.js-based test.\n"
            "PASS first subtest\n"
            "FAIL second subtest assert_equals: expected 1 but got 2\n"
            "PASS third subtest\n"
            "Harness: the test ran to completion.\n")
        counts = parse_subtest_counts(text)
        self.assertEqual(counts, SubtestCounts(passed=2, total=3))
        self.assertTrue(counts.is_testharness)
        self.assertAlmostEqual(counts.ratio, 2.0 / 3.0)

    def test_all_pass(self):
        counts = parse_subtest_counts("PASS a\nPASS b\n")
        self.assertEqual(counts, SubtestCounts(passed=2, total=2))
        self.assertEqual(counts.ratio, 1.0)

    def test_non_pass_statuses_count_as_subtests(self):
        text = "PASS a\nTIMEOUT b\nNOTRUN c\nPRECONDITION_FAILED d\n"
        counts = parse_subtest_counts(text)
        self.assertEqual(counts, SubtestCounts(passed=1, total=4))
        self.assertEqual(counts.ratio, 0.25)

    def test_harness_error(self):
        counts = parse_subtest_counts("Harness Error (harness_status.status = 1) , message = boom\n")
        self.assertEqual(counts, SubtestCounts(passed=0, total=0, harness_error=True))
        self.assertTrue(counts.is_testharness)
        self.assertEqual(counts.ratio, 0.0)

    def test_empty_is_not_testharness(self):
        counts = parse_subtest_counts("")
        self.assertEqual(counts, SubtestCounts())
        self.assertFalse(counts.is_testharness)
        self.assertEqual(counts.ratio, 0.0)

    def test_non_testharness_text_ignored(self):
        counts = parse_subtest_counts("layer at (0,0) size 800x600\n  RenderView\n")
        self.assertEqual(counts, SubtestCounts())
        self.assertFalse(counts.is_testharness)

    def test_multiline_failure_detail_not_double_counted(self):
        # Assertion detail wrapped onto a following line must not be counted.
        text = (
            "FAIL a subtest\n"
            "  expected foo\n"
            "  got bar\n"
            "PASS b subtest\n")
        counts = parse_subtest_counts(text)
        self.assertEqual(counts, SubtestCounts(passed=1, total=2))


if __name__ == "__main__":
    unittest.main()
