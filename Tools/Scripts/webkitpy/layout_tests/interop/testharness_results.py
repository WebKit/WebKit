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

"""Parsing of testharness.js text output as dumped by WebKitTestRunner/DumpRenderTree.

WebKit does not retain a structured model of testharness subtest results; a
web-platform-test is scored as a single whole-file text comparison. To compute
an interop score at the subtest level (matching wpt.fyi's methodology) we parse
the dumped text output back into subtest pass/total counts.

A typical dump looks like:

    This is a testharness.js-based test.
    PASS first subtest
    FAIL second subtest assert_equals: expected 1 but got 2
    Harness: the test ran to completion.

Some tests instead emit a harness error, in which case no subtests ran to
completion:

    Harness Error (harness_status.status = 1) , message = uncaught exception
"""

import re

# Subtest status tokens defined by testharness.js. Every subtest result line in
# the WebKit dump begins with one of these followed by whitespace.
_SUBTEST_STATUSES = ("PASS", "FAIL", "TIMEOUT", "NOTRUN", "PRECONDITION_FAILED", "ERROR")

_SUBTEST_LINE_RE = re.compile(r"^(%s)\s" % "|".join(_SUBTEST_STATUSES))
_HARNESS_ERROR_RE = re.compile(r"^Harness Error\b")


class SubtestCounts(object):
    """Result of parsing a testharness dump: passing subtests, total subtests,
    and whether the harness itself errored before completing."""

    def __init__(self, passed=0, total=0, harness_error=False):
        self.passed = passed
        self.total = total
        self.harness_error = harness_error

    @property
    def is_testharness(self):
        """Whether the parsed text looked like testharness output at all
        (either subtests were reported or the harness errored)."""
        return self.total > 0 or self.harness_error

    @property
    def ratio(self):
        """Fraction of subtests that passed, in [0, 1]. Zero when the harness
        errored or reported no subtests."""
        if self.total == 0:
            return 0.0
        return self.passed / self.total

    def __eq__(self, other):
        return (self.passed, self.total, self.harness_error) == \
               (other.passed, other.total, other.harness_error)

    def __repr__(self):
        return "SubtestCounts(passed=%d, total=%d, harness_error=%r)" % (
            self.passed, self.total, self.harness_error)


def parse_subtest_counts(text):
    """Parse a testharness.js text dump into a SubtestCounts.

    Lines that do not begin with a subtest status token (the header, the
    "Harness:" footer, blank lines, and multi-line assertion detail) are
    ignored. A harness error with no reported subtests yields total == 0.
    """
    if not text:
        return SubtestCounts()

    passed = 0
    total = 0
    harness_error = False
    for line in text.splitlines():
        if _HARNESS_ERROR_RE.match(line):
            harness_error = True
            continue
        match = _SUBTEST_LINE_RE.match(line)
        if not match:
            continue
        total += 1
        if match.group(1) == "PASS":
            passed += 1
    return SubtestCounts(passed=passed, total=total, harness_error=harness_error)
