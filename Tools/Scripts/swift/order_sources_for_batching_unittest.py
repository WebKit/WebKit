#!/usr/bin/env python3
"""Unit tests for order_sources_for_batching.py.

Run with: Tools/Scripts/test-webkitpy swift
"""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

# The scripts under test are run by CMake, not imported, so they import each
# other by plain name. test-webkitpy imports this file as swift.<name>, which
# puts Tools/Scripts on sys.path but not this directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import order_sources_for_batching as order  # noqa: E402


class BatchSizesTest(unittest.TestCase):
    def test_remainder_goes_to_the_first_batches(self):
        self.assertEqual(order.batch_sizes(62, 10), [7, 7, 6, 6, 6, 6, 6, 6, 6, 6])

    def test_batches_hold_at_most_25_files(self):
        self.assertEqual(order.batch_sizes(60, 2), [20, 20, 20])

    def test_fewer_files_than_jobs(self):
        self.assertEqual(order.batch_sizes(3, 8), [1, 1, 1])


class WeightTest(unittest.TestCase):
    def test_counts_code_lines_and_assertions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.swift"
            path.write_text("// comment\n/* block\n   comment */\nlet a = 1\n\n#expect(a == 1)\ntry #require(b)\n")
            self.assertEqual(order.weight(str(path)), 3 + 2 * order.MACRO_LINES + order.FILE_LINES)

    def test_missing_file_weighs_one_file(self):
        self.assertEqual(order.weight("/nonexistent/a.swift"), order.FILE_LINES)


class BalancedTest(unittest.TestCase):
    def setUp(self):
        self.files = [f"f{i}.swift" for i in range(20)]
        self.weights = {f: 100 for f in self.files}
        self.weights["f0.swift"] = self.weights["f1.swift"] = 50000

    def batches(self, files, jobs):
        out, start = [], 0
        for size in order.batch_sizes(len(files), jobs):
            out.append(files[start:start + size])
            start += size
        return out

    def test_heavy_neighbors_land_in_different_batches(self):
        ordered = order.balanced(self.files, self.weights, 4)
        self.assertEqual(sorted(ordered), sorted(self.files))
        self.assertFalse(any({"f0.swift", "f1.swift"} <= set(b) for b in self.batches(ordered, 4)))

    def test_order_is_deterministic(self):
        self.assertEqual(order.balanced(self.files, self.weights, 4), order.balanced(self.files, self.weights, 4))


class MainTest(unittest.TestCase):
    def run_main(self, entries, jobs, base_dir):
        with tempfile.TemporaryDirectory() as directory:
            source, result = Path(directory) / "in", Path(directory) / "out"
            source.write_text("\n".join(entries) + "\n")
            argv = sys.argv
            sys.argv = ["order", "--jobs", str(jobs), "--base-dir", base_dir, "--input", str(source), "--output", str(result)]
            try:
                self.assertEqual(order.main(), 0)
            finally:
                sys.argv = argv
            return result.read_text().splitlines()

    def test_only_swift_entries_move(self):
        with tempfile.TemporaryDirectory() as base:
            entries = ["a.mm", "Two Words/x.swift", "b.cpp", "y.swift", "z.swift", "c.mm"]
            (Path(base) / "Two Words").mkdir()
            (Path(base) / "Two Words" / "x.swift").write_text("let x = 1\n" * 2000)
            result = self.run_main(entries, 2, base)
            self.assertEqual([e for e in result if not e.endswith(".swift")], ["a.mm", "b.cpp", "c.mm"])
            self.assertEqual([i for i, e in enumerate(result) if not e.endswith(".swift")], [0, 2, 5])
            self.assertEqual(sorted(e for e in result if e.endswith(".swift")), ["Two Words/x.swift", "y.swift", "z.swift"])

    def test_duplicates_and_generator_expressions(self):
        with tempfile.TemporaryDirectory() as base:
            entries = ["a.swift", "b.swift", "a.swift", "$<$<CONFIG:Debug>:debug.swift>", "c.swift"]
            result = self.run_main(entries, 2, base)
            self.assertEqual(len(result), 4)
            self.assertEqual(result.count("a.swift"), 1)
            self.assertEqual(result[2], "$<$<CONFIG:Debug>:debug.swift>")
            self.assertEqual(sorted(e for e in result if order.is_swift(e)), ["a.swift", "b.swift", "c.swift"])

    def test_one_job_keeps_the_listed_order(self):
        with tempfile.TemporaryDirectory() as base:
            entries = ["b.swift", "a.swift"]
            self.assertEqual(self.run_main(entries, 1, base), entries)


class SolveTest(unittest.TestCase):
    def test_recovers_file_costs_from_shifting_batches(self):
        true = {"a.swift": 100.0, "b.swift": 10.0, "c.swift": 10.0, "d.swift": 50.0}
        # Batches of different sizes, as the driver forms at different -j, separate a job's fixed cost from its files'.
        batches = [["a.swift", "b.swift"], ["c.swift", "d.swift"], ["a.swift", "c.swift", "d.swift"], ["b.swift"],
                   ["a.swift"], ["b.swift", "c.swift", "d.swift"], ["d.swift"], ["a.swift", "b.swift", "c.swift"]]
        observations = [{"id": str(i), "files": b, "cost": 5.0 + sum(true[f] for f in b)} for i, b in enumerate(batches) for _ in range(20)]
        learned = order.solve(observations, {f: 1.0 for f in true})
        for f, cost in true.items():
            self.assertAlmostEqual(learned[f], cost, delta=0.1 * cost)

    def test_unseen_files_get_the_scaled_static_weight(self):
        observations = [{"id": "1", "files": ["a.swift"], "cost": 300.0}, {"id": "2", "files": ["b.swift"], "cost": 100.0}]
        learned = order.solve(observations, {"a.swift": 3.0, "b.swift": 1.0, "new.swift": 2.0})
        self.assertGreater(learned["new.swift"], learned["b.swift"])
        self.assertLess(learned["new.swift"], learned["a.swift"])

    def test_no_relevant_jobs(self):
        self.assertIsNone(order.solve([{"id": "1", "files": ["other.swift"], "cost": 1.0}], {"a.swift": 1.0}))


class HistoryTest(unittest.TestCase):
    def test_ingest_keeps_each_job_once_and_the_newest(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "sub", "history.json")
            self.assertEqual(order.ingest(path, [{"id": "1", "files": ["a.swift"], "cost": 1}]), 1)
            self.assertEqual(order.ingest(path, [{"id": "1", "files": ["a.swift"], "cost": 1}, {"id": "2", "files": ["b.swift"], "cost": 2}]), 2)
            saved = order.MAX_OBSERVATIONS
            order.MAX_OBSERVATIONS = 2
            try:
                order.ingest(path, [{"id": "3", "files": ["c.swift"], "cost": 3}])
            finally:
                order.MAX_OBSERVATIONS = saved
            self.assertEqual([o["id"] for o in order.load_history(path)], ["2", "3"])

    def test_observe_reads_a_traced_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src"
            (source / "Tests").mkdir(parents=True)
            for name in ("a.swift", "b.swift"):
                (source / "Tests" / name).write_text("let x = 1\n")
            stats = root / "stats"
            stats.mkdir()
            aux = "Demo-a.swift-arm64_apple_macosx27.0-o-Onone"
            (stats / f"stats-1700000000000000-swift-frontend-{aux}-1.json").write_text(json.dumps({
                f"time.swift-frontend.{aux}.wall": 2.0, "Frontend.NumInstructionsExecuted": 12345}))
            log = root / "swift-jobs.jsonl"
            inputs = [str(source / "Tests" / "a.swift"), str(source / "Tests" / "b.swift")]
            rows = [{"t": 100.0, "kind": "driver-began", "rec": 1, "cwd": str(root)}]
            for pid, path in ((11, inputs[0]), (12, inputs[1])):
                rows.append({"t": 100.1, "kind": "began", "rec": 1, "pid": pid, "name": "compile", "inputs": [path],
                             "output": os.path.basename(path) + ".o", "batch": inputs[0]})
            for pid in (11, 12):
                rows.append({"t": 102.1, "kind": "finished", "rec": 1, "pid": pid, "exit_status": 0})
            rows.append({"t": 102.2, "kind": "driver-finished", "rec": 1})
            log.write_text("".join(json.dumps(r) + "\n" for r in rows))
            observed = order.observe(str(log), str(stats), str(source))
            self.assertEqual(len(observed), 1)
            self.assertEqual(observed[0]["files"], ["Tests/a.swift", "Tests/b.swift"])
            self.assertEqual(observed[0]["cost"], 12345)

    def test_history_separates_files_the_static_estimate_misses(self):
        with tempfile.TemporaryDirectory() as base:
            names = [f"f{i}.swift" for i in range(8)]
            for name in names:
                (Path(base) / name).write_text("let x = 1\n")
            heavy = {"f0.swift", "f1.swift"}
            observations = []
            for i in range(40):
                batch = [names[(i + k) % 8] for k in range(2)]
                observations.append({"id": str(i), "files": batch, "cost": sum(1000 if f in heavy else 10 for f in batch)})
            history = os.path.join(base, "history.json")
            order.ingest(history, observations)
            with tempfile.TemporaryDirectory() as directory:
                source, result = Path(directory) / "in", Path(directory) / "out"
                source.write_text("\n".join(names) + "\n")
                argv = sys.argv
                sys.argv = ["order", "--jobs", "4", "--base-dir", base, "--input", str(source), "--output", str(result),
                            "--history", history, "--source-dir", base]
                try:
                    self.assertEqual(order.main(), 0)
                finally:
                    sys.argv = argv
                ordered = result.read_text().splitlines()
            batches = [ordered[k:k + 2] for k in range(0, 8, 2)]
            self.assertFalse(any(heavy <= set(b) for b in batches))


if __name__ == "__main__":
    unittest.main()
