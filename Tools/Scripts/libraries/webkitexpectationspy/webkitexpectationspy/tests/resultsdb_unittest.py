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

"""Tests for ResultsDB severity sorting."""

import unittest

from webkitexpectationspy.expectations import ResultStatus
from webkitexpectationspy.resultsdb import (
    ResultsDBStatus, sort_key, sorted_by_severity, to_resultsdb, from_resultsdb,
)
from webkitexpectationspy.suites.layout_tests import LayoutTestStatus


class SortKeyTest(unittest.TestCase):
    """sort_key returns the ResultsDB integer code so higher-level code
    sorts by severity without reinventing an ordering."""

    def test_sort_key_matches_resultsdb_codes(self):
        self.assertEqual(sort_key(ResultStatus.CRASH), ResultsDBStatus.CRASH.value)
        self.assertEqual(sort_key(ResultStatus.TIMEOUT), ResultsDBStatus.TIMEOUT.value)
        self.assertEqual(sort_key(ResultStatus.PASS), ResultsDBStatus.PASS.value)
        self.assertEqual(sort_key(ResultStatus.SKIP), ResultsDBStatus.SKIP.value)

    def test_sorted_by_severity_puts_crash_first(self):
        statuses = [ResultStatus.PASS, ResultStatus.CRASH, ResultStatus.FAIL, ResultStatus.TIMEOUT]
        ordered = sorted_by_severity(statuses)
        self.assertEqual(ordered[0], ResultStatus.CRASH)
        self.assertEqual(ordered[1], ResultStatus.TIMEOUT)
        self.assertEqual(ordered[-1], ResultStatus.PASS)

    def test_combined_flag_uses_most_severe_member(self):
        # A combined Flag value (e.g. PASS | FAIL) should sort by its most
        # severe constituent — FAIL here.
        combined = ResultStatus.PASS | ResultStatus.FAIL
        self.assertEqual(sort_key(combined), ResultsDBStatus.FAIL.value)

    def test_layout_test_statuses_sort_by_severity(self):
        statuses = [
            LayoutTestStatus.PASS,
            LayoutTestStatus.CRASH,
            LayoutTestStatus.IMAGE,
            LayoutTestStatus.LEAK,
        ]
        ordered = sorted_by_severity(statuses)
        self.assertEqual(ordered[0], LayoutTestStatus.CRASH)
        self.assertEqual(ordered[-1], LayoutTestStatus.PASS)


class ConversionRoundTripTest(unittest.TestCase):

    def test_to_and_from_resultsdb_round_trip(self):
        for status in (ResultStatus.PASS, ResultStatus.FAIL, ResultStatus.CRASH,
                       ResultStatus.TIMEOUT, ResultStatus.SKIP):
            code = to_resultsdb(status)
            self.assertEqual(from_resultsdb(code, ResultStatus), status)
