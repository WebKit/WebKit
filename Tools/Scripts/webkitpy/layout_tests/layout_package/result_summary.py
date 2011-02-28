#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Run layout tests."""

import logging

import test_expectations

_log = logging.getLogger("webkitpy.layout_tests.run_webkit_tests")

TestExpectationsFile = test_expectations.TestExpectationsFile


class ResultSummary(object):
    """A class for partitioning the test results we get into buckets.

    This class is basically a glorified struct and it's private to this file
    so we don't bother with any information hiding."""

    def __init__(self, expectations, test_files):
        self.total = len(test_files)
        self.remaining = self.total
        self.expectations = expectations
        self.expected = 0
        self.unexpected = 0
        self.unexpected_failures = 0
        self.unexpected_crashes_or_timeouts = 0
        self.tests_by_expectation = {}
        self.tests_by_timeline = {}
        self.results = {}
        self.unexpected_results = {}
        self.failures = {}
        self.tests_by_expectation[test_expectations.SKIP] = set()
        for expectation in TestExpectationsFile.EXPECTATIONS.values():
            self.tests_by_expectation[expectation] = set()
        for timeline in TestExpectationsFile.TIMELINES.values():
            self.tests_by_timeline[timeline] = (
                expectations.get_tests_with_timeline(timeline))

    def add(self, result, expected):
        """Add a TestResult into the appropriate bin.

        Args:
          result: TestResult
          expected: whether the result was what we expected it to be.
        """

        self.tests_by_expectation[result.type].add(result.filename)
        self.results[result.filename] = result
        self.remaining -= 1
        if len(result.failures):
            self.failures[result.filename] = result.failures
        if expected:
            self.expected += 1
        else:
            self.unexpected_results[result.filename] = result
            self.unexpected += 1
            if len(result.failures):
                self.unexpected_failures += 1
            if result.type == test_expectations.CRASH or result.type == test_expectations.TIMEOUT:
                self.unexpected_crashes_or_timeouts += 1
