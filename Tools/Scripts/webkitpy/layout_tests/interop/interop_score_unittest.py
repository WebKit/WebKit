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

import json
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.interop import interop_score
from webkitpy.layout_tests.interop.interop_score import (
    InteropCategory, InteropData, compute_interop_score, format_interop_score,
    load_interop_data, wpt_path_to_test_name)
from webkitpy.layout_tests.models import test_expectations


class FakeResult(object):
    def __init__(self, result_type):
        self.type = result_type


class FakePort(object):
    """Minimal stand-in exposing only what the scorer touches."""

    def __init__(self, filesystem, results_directory, baselines):
        self._filesystem = filesystem
        self._results_directory = results_directory
        self._baselines = baselines  # test name -> baseline path (or absent for none).

    def results_directory(self):
        return self._results_directory

    def expected_filename(self, test_name, suffix, return_default=True, device_type=None):
        return self._baselines.get(test_name)


class InteropScoreTest(unittest.TestCase):
    def test_wpt_path_to_test_name(self):
        self.assertEqual(wpt_path_to_test_name("/css/foo.html"),
                         "imported/w3c/web-platform-tests/css/foo.html")
        self.assertEqual(wpt_path_to_test_name("css/foo.html"),
                         "imported/w3c/web-platform-tests/css/foo.html")

    def test_load_interop_data(self):
        fs = MockFileSystem()
        fs.write_text_file("/data/interop-2026.json", json.dumps({
            "year": 2026,
            "categories": {
                "scroll-snap": {
                    "name": "Scroll Snap",
                    "label": "interop-2026-scroll-snap",
                    "tests": ["/css/css-scroll-snap/a.html", "/css/css-scroll-snap/b.html"],
                },
            },
        }))
        data = load_interop_data(fs, "/data/interop-2026.json")
        self.assertEqual(data.year, 2026)
        self.assertEqual(len(data.categories), 1)
        category = data.categories[0]
        self.assertEqual(category.key, "scroll-snap")
        self.assertEqual(category.name, "Scroll Snap")
        self.assertEqual(category.test_names, [
            "imported/w3c/web-platform-tests/css/css-scroll-snap/a.html",
            "imported/w3c/web-platform-tests/css/css-scroll-snap/b.html",
        ])

    def test_load_interop_data_missing_file(self):
        fs = MockFileSystem()
        self.assertRaises(ValueError, load_interop_data, fs, "/does/not/exist.json")

    def _score_fixture(self):
        results_directory = "/results"
        names = {letter: wpt_path_to_test_name("/css/%s.html" % letter)
                 for letter in "abcdefg"}

        testharness_baseline = (
            "This is a testharness.js-based test.\n"
            "PASS one\nPASS two\nPASS three\nFAIL four boom\n"
            "Harness: the test ran to completion.\n")  # 3/4 pass.
        reftext_baseline = "layer at (0,0) size 800x600\n"  # Non-testharness -> full pass.
        actual_c = "PASS one\nFAIL two\n"  # 1/2 pass.

        fs = MockFileSystem()
        fs.write_text_file("/baselines/b-expected.txt", testharness_baseline)
        fs.write_text_file("/baselines/g-expected.txt", reftext_baseline)
        actual_c_path = fs.join(results_directory, TestResultWriter.actual_filename(names["c"], fs))
        fs.write_text_file(actual_c_path, actual_c)

        baselines = {names["b"]: "/baselines/b-expected.txt",
                     names["g"]: "/baselines/g-expected.txt"}
        port = FakePort(fs, results_directory, baselines)

        results_by_name = {
            names["a"]: FakeResult(test_expectations.PASS),   # pass, no baseline -> 1.0
            names["b"]: FakeResult(test_expectations.PASS),   # pass w/ testharness baseline -> 0.75
            names["c"]: FakeResult(test_expectations.FAIL),   # fail w/ actual -> 0.5
            names["d"]: FakeResult(test_expectations.FAIL),   # fail, no actual -> 0.0
            names["e"]: FakeResult(test_expectations.SKIP),   # skipped -> excluded
            names["g"]: FakeResult(test_expectations.PASS),   # pass w/ non-testharness baseline -> 1.0
            # names["f"] deliberately absent -> not run.
        }
        category = InteropCategory("demo", "Demo", "interop-2026-demo",
                                   [names[letter] for letter in "abcdefg"])
        data = InteropData(2026, [category])
        return data, results_by_name, port

    def test_compute_interop_score(self):
        data, results_by_name, port = self._score_fixture()
        score = compute_interop_score(data, results_by_name, port)

        self.assertEqual(len(score.category_scores), 1)
        category = score.category_scores[0]
        self.assertEqual(category.tests_total, 7)
        self.assertEqual(category.tests_run, 5)
        self.assertEqual(category.tests_skipped, 1)
        # (1.0 + 0.75 + 0.5 + 0.0 + 1.0) / 5
        self.assertAlmostEqual(category.score, 0.65)
        self.assertAlmostEqual(score.overall, 0.65)
        self.assertFalse(score.is_complete)

    def test_overall_none_when_nothing_run(self):
        category = InteropCategory("demo", "Demo", "interop-2026-demo",
                                   [wpt_path_to_test_name("/css/x.html")])
        data = InteropData(2026, [category])
        port = FakePort(MockFileSystem(), "/results", {})
        score = compute_interop_score(data, {}, port)
        self.assertIsNone(score.overall)
        self.assertFalse(score.category_scores[0].has_coverage)

    def test_verbose_omits_breakdown_for_uncovered_category(self):
        category = InteropCategory("demo", "Demo", "interop-2026-demo",
                                   [wpt_path_to_test_name("/css/x.html")])
        data = InteropData(2026, [category])
        port = FakePort(MockFileSystem(), "/results", {})
        score = compute_interop_score(data, {}, port)
        text = format_interop_score(score, verbose=True)
        # The category ran nothing, so no per-test breakdown lines should appear.
        self.assertNotIn("not-run", text)
        self.assertNotIn("css/x.html", text)

    def test_format_interop_score(self):
        data, results_by_name, port = self._score_fixture()
        score = compute_interop_score(data, results_by_name, port)
        text = format_interop_score(score)
        self.assertIn("Interop 2026 score", text)
        self.assertIn("Demo", text)
        self.assertIn("65.0", text)  # Overall and single-category score.
        self.assertIn("5/7", text)
        self.assertIn("skipped", text)
        self.assertIn("coverage is incomplete", text)

    def test_format_interop_score_verbose_breakdown(self):
        data, results_by_name, port = self._score_fixture()
        score = compute_interop_score(data, results_by_name, port)
        text = format_interop_score(score, verbose=True)
        # Per-test breakdown lines with provenance appear only when verbose.
        self.assertNotIn("all-pass", format_interop_score(score))
        self.assertIn("all-pass", text)   # pass, no baseline
        self.assertIn("baseline", text)   # pass w/ testharness baseline
        self.assertIn("actual", text)     # failure w/ actual output
        self.assertIn("not-run", text)    # test absent from results
        self.assertIn("css/c.html", text)
        self.assertIn("3/4", text)        # subtest counts from the baseline case
        self.assertIn("75%", text)


if __name__ == "__main__":
    unittest.main()
