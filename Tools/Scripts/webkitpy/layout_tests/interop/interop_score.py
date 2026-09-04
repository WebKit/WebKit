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

"""Computes an "interop score" from a run-webkit-tests result set.

The score mirrors the wpt.fyi Interop dashboard methodology: each focus area
(category) is defined by a set of web-platform-tests, each test is scored by the
fraction of its testharness subtests that pass, the category score is the mean
of its tests' per-test scores, and the overall score is the mean of the category
scores.

WebKit has no structured subtest model -- a WPT test is scored as a single
whole-file text comparison, and a WebKit "PASS" only means the output matched a
baseline that may itself contain FAIL lines. So per-test subtest ratios are
recovered from the dumped testharness text (see testharness_results), choosing
the source by the test's outcome:

  * failure          -> the "-actual.txt" written into the results directory
  * pass w/ baseline -> the "-expected.txt" baseline (equals the actual output,
                        since the comparison passed), which reveals any FAIL
                        subtests baked into the expectation
  * pass w/o baseline-> an implicit all-pass, scored 1.0 (also covers reftests)

Only tests actually run are scored; coverage is reported so partial runs are
not mistaken for full ones.
"""

import json

from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.controllers.layout_test_finder import IMPORTED_WPT_DIR
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.interop.testharness_results import parse_subtest_counts
from webkitpy.layout_tests.models import test_expectations


def default_data_path(filesystem, year):
    """Absolute path to the checked-in interop data file for a given year,
    under metadata/interop/ at the top of the WebKit tree."""
    return WebKitFinder(filesystem).path_from_webkit_base("metadata", "interop", "interop-{}.json".format(year))


def wpt_path_to_test_name(wpt_path):
    """Convert a wpt.fyi-style test path (e.g. "/css/foo.html") to a WebKit
    layout test name (e.g. "imported/w3c/web-platform-tests/css/foo.html")."""
    return "{}/{}".format(IMPORTED_WPT_DIR, wpt_path.lstrip("/"))


class InteropCategory(object):
    def __init__(self, key, name, label, test_names):
        self.key = key
        self.name = name
        self.label = label
        self.test_names = test_names  # WebKit layout test names.


class InteropData(object):
    def __init__(self, year, categories):
        self.year = year
        self.categories = categories  # List of InteropCategory.


def load_interop_data(filesystem, path):
    """Load and normalize an interop data file. Raises ValueError if the file
    is missing or malformed."""
    if not filesystem.exists(path):
        raise ValueError("Interop data file not found: {}".format(path))
    try:
        raw = json.loads(filesystem.read_text_file(path))
    except ValueError as error:
        raise ValueError("Could not parse interop data file {}: {}".format(path, error))

    categories = []
    for key, value in sorted(raw.get("categories", {}).items()):
        test_names = [wpt_path_to_test_name(p) for p in value.get("tests", [])]
        categories.append(InteropCategory(
            key=key,
            name=value.get("name", key),
            label=value.get("label", key),
            test_names=test_names))
    return InteropData(year=raw.get("year"), categories=categories)


class TestScore(object):
    """Per-test contribution to a category score, retained for verbose reporting."""

    def __init__(self, test_name, ratio, passed, total, source):
        self.test_name = test_name
        self.ratio = ratio      # Contribution in [0, 1], or None when not scored.
        self.passed = passed    # Passing subtests, or None when not subtest-based.
        self.total = total      # Total subtests, or None when not subtest-based.
        self.source = source    # Where the ratio came from (see _score_single_test).

    @property
    def scored(self):
        return self.ratio is not None


class CategoryScore(object):
    def __init__(self, category, score, tests_total, tests_run, tests_skipped, test_scores=None):
        self.key = category.key
        self.name = category.name
        self.label = category.label
        self.score = score  # Mean per-test subtest ratio over run tests, [0, 1].
        self.tests_total = tests_total
        self.tests_run = tests_run
        self.tests_skipped = tests_skipped
        self.test_scores = test_scores or []  # All TestScore entries, for verbose output.

    @property
    def has_coverage(self):
        return self.tests_run > 0


class InteropScore(object):
    def __init__(self, year, category_scores):
        self.year = year
        self.category_scores = category_scores

    @property
    def overall(self):
        """Mean of the category scores that had at least one test run. Returns
        None when nothing was run."""
        covered = [c.score for c in self.category_scores if c.has_coverage]
        if not covered:
            return None
        return sum(covered) / len(covered)

    @property
    def is_complete(self):
        """Whether every test in every category was run."""
        return all(c.tests_run == c.tests_total for c in self.category_scores)


def _score_single_test(test_name, result, port, filesystem, results_directory):
    """Return a TestScore (subtest pass ratio in [0, 1] plus provenance) for a
    single run test."""
    if result.type == test_expectations.PASS:
        baseline = port.expected_filename(test_name, ".txt", return_default=False)
        if baseline and filesystem.exists(baseline):
            counts = parse_subtest_counts(filesystem.read_text_file(baseline))
            if counts.is_testharness:
                return TestScore(test_name, counts.ratio, counts.passed, counts.total, "baseline")
        # No baseline means an implicit all-pass testharness result, or a
        # passing reftest/non-testharness test: either way, fully passing.
        return TestScore(test_name, 1.0, None, None, "all-pass")

    # Any failure: recover the real subtest results from the actual output that
    # the result writer dumped into the results directory.
    actual_path = filesystem.join(results_directory, TestResultWriter.actual_filename(test_name, filesystem))
    if filesystem.exists(actual_path):
        counts = parse_subtest_counts(filesystem.read_text_file(actual_path))
        if counts.total > 0:
            return TestScore(test_name, counts.ratio, counts.passed, counts.total, "actual")
        if counts.harness_error:
            return TestScore(test_name, 0.0, 0, 0, "harness-error")
    # Crash/timeout/missing output, or a failing reftest: no subtests passed.
    return TestScore(test_name, 0.0, None, None, "no-output")


def compute_interop_score(interop_data, results_by_name, port):
    """Compute an InteropScore from a mapping of test name -> TestResult (e.g.
    RunDetails.initial_results.results_by_name)."""
    filesystem = port._filesystem
    results_directory = port.results_directory()

    category_scores = []
    for category in interop_data.categories:
        ratios = []
        skipped = 0
        test_scores = []
        for test_name in category.test_names:
            result = results_by_name.get(test_name)
            if result is None:
                test_scores.append(TestScore(test_name, None, None, None, "not-run"))
                continue
            if result.type == test_expectations.SKIP:
                test_scores.append(TestScore(test_name, None, None, None, "skipped"))
                skipped += 1
                continue
            test_score = _score_single_test(test_name, result, port, filesystem, results_directory)
            test_scores.append(test_score)
            ratios.append(test_score.ratio)
        score = sum(ratios) / len(ratios) if ratios else 0.0
        category_scores.append(CategoryScore(
            category=category,
            score=score,
            tests_total=len(category.test_names),
            tests_run=len(ratios),
            tests_skipped=skipped,
            test_scores=test_scores))
    return InteropScore(year=interop_data.year, category_scores=category_scores)


def _short_test_name(test_name):
    prefix = IMPORTED_WPT_DIR + "/"
    if test_name.startswith(prefix):
        return test_name[len(prefix):]
    return test_name


def _format_category_breakdown(category):
    """Indented per-test lines showing how a category's score was computed."""
    lines = []
    for test_score in category.test_scores:
        ratio_text = "{:.0f}%".format(100.0 * test_score.ratio) if test_score.scored else "-"
        if test_score.passed is not None and test_score.total is not None:
            subtests = "{}/{}".format(test_score.passed, test_score.total)
        else:
            subtests = "-"
        lines.append("      {:>5}  {:>7}  {:<13}  {}".format(
            ratio_text, subtests, test_score.source, _short_test_name(test_score.test_name)))
    return lines


def format_interop_score(score, verbose=False):
    """Render an InteropScore as a human-readable multi-line report. When verbose,
    each category is followed by a per-test breakdown of its subtest ratios."""
    lines = []
    title = "Interop {} score".format(score.year) if score.year else "Interop score"
    lines.append("")
    lines.append(title)
    lines.append("=" * len(title))

    name_width = max([len(c.name) for c in score.category_scores] + [len("Focus area")])
    header = "{:<{w}}  {:>7}  {:>10}".format("Focus area", "Score", "Coverage", w=name_width)
    lines.append(header)
    lines.append("-" * len(header))

    for category in score.category_scores:
        if category.has_coverage:
            score_text = "{:.1f}".format(100.0 * category.score)
        else:
            score_text = "-"
        coverage = "{}/{}".format(category.tests_run, category.tests_total)
        if category.tests_skipped:
            coverage += " (+{} skipped)".format(category.tests_skipped)
        lines.append("{:<{w}}  {:>7}  {:>10}".format(category.name, score_text, coverage, w=name_width))
        # Only break a category down when something in it actually ran; otherwise
        # it would just be a wall of "not-run" lines.
        if verbose and category.has_coverage:
            lines.extend(_format_category_breakdown(category))

    lines.append("-" * len(header))
    overall = score.overall
    if overall is None:
        lines.append("No interop tests were run.")
    else:
        overall_text = "{:.1f}".format(100.0 * overall)
        lines.append("{:<{w}}  {:>7}".format("Overall", overall_text, w=name_width))
        if not score.is_complete:
            lines.append("")
            lines.append("Note: coverage is incomplete; score reflects only the tests that were run.")
    lines.append("")
    return "\n".join(lines)
