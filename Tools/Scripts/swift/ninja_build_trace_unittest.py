#!/usr/bin/env python3
"""Tests for ninja_build_trace.py and swiftc_job_recorder.py.

Run with: Tools/Scripts/test-webkitpy swift
"""

from __future__ import annotations

import contextlib
import gzip
import io
import json
import tempfile
import unittest
from pathlib import Path

from . import ninja_build_trace as nbt
from . import swiftc_job_recorder as recorder

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

AUX = "Demo-f0.swift-arm64_apple_macosx26.0-o-Onone"


def write_stats(directory, start_us: int, wall_s: float, aux: str = AUX, **extra):
    """Write a stats-*.json the way swift-frontend names and shapes them."""
    path = directory / f"stats-{start_us}-swift-frontend-{aux}-12345.json"
    payload = {
        "AST.NumSourceLines": 42,
        f"time.swift-frontend.{aux}.wall": wall_s,
        f"time.swift-frontend.{aux}.user": wall_s / 2,
        "time.swift.Parsing.wall": 0.001,
    }
    payload.update(extra)
    path.write_text(json.dumps(payload))
    return path


def ninja_log(lines: list[str], version: int = 6) -> str:
    return "\n".join([f"# ninja log v{version}"] + lines) + "\n"


def write_jobs_log(directory, events: list[dict], name: str = "jobs.jsonl"):
    path = directory / name
    path.write_text("\n".join(json.dumps(e) for e in events) + "\n")
    return path


def framed(payload: dict) -> bytes:
    body = json.dumps(payload).encode()
    return str(len(body)).encode() + b"\n" + body


def make_job(start_us: int, dur_us: int, module: str = "A") -> nbt.FrontendJob:
    return nbt.FrontendJob(
        start_us=start_us,
        dur_us=dur_us,
        kind="frontend",
        module=module,
        input="f0.swift",
        triple="t",
        out="o",
        opt="Onone",
        stats={},
        path=Path("stats.json"),
    )


class ScratchDirectoryTest(unittest.TestCase):
    """Base class for the tests that read files; `self.tmp_path` is a fresh directory."""

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="ninja-build-trace-test-")
        self.addCleanup(directory.cleanup)
        self.tmp_path = Path(directory.name)


# ---------------------------------------------------------------------------
# .ninja_log parsing
# ---------------------------------------------------------------------------


class NinjaLogParsingTest(ScratchDirectoryTest):
    def test_multi_output_records_fold_into_one_edge(self):
        # A multi-output edge writes one record per output, sharing start/end/hash.
        log = self.tmp_path / ".ninja_log"
        log.write_text(
            ninja_log(
                [
                    "100\t900\t1700000000900000000\tCMakeFiles/A.dir/a.swift.o\tdeadbeef",
                    "100\t900\t1700000000900000000\tCMakeFiles/A.dir/b.swift.o\tdeadbeef",
                    "100\t900\t1700000000900000000\tA.swiftmodule\tdeadbeef",
                    "900\t950\t1700000000950000000\tlibA.a\tfeedface",
                ]
            )
        )
        edges = nbt.parse_ninja_log(log)
        self.assertEqual(len(edges), 2)
        self.assertEqual(len(edges[0].outputs), 3)
        self.assertEqual(edges[0].dur_ms, 800)
        self.assertEqual(edges[0].label, "A.swiftmodule")  # shortest output wins the label
        self.assertEqual(edges[1].outputs, ["libA.a"])

    def test_header_and_malformed_lines_are_skipped(self):
        log = self.tmp_path / ".ninja_log"
        log.write_text(
            ninja_log(["", "not\ta\tvalid", "x\ty\tz\tout\thash", "0\t5\t0\tout\thash"], version=7)
        )
        edges = nbt.parse_ninja_log(log)
        self.assertEqual(len(edges), 1)
        self.assertEqual(edges[0].outputs, ["out"])

    def test_edge_anchor_uses_freshest_output_mtime(self):
        log = self.tmp_path / ".ninja_log"
        log.write_text(
            ninja_log(
                [
                    # A restat edge, the one kind that logs output mtimes: the
                    # first output was not rewritten, so its record is stale.
                    "200\t1000\t1600000000000000000\tstale.swiftmodule\tabc",
                    "200\t1000\t1700000001000000000\tfresh.o\tabc",
                ]
            )
        )
        (edge,) = nbt.parse_ninja_log(log)
        self.assertEqual(edge.anchor_ms, 1700000000800.0)

    def test_edge_anchor_subtracts_the_start_not_the_end(self):
        # ninja writes edge->command_start_time_ into the column the format calls
        # "mtime", so the invocation's epoch start is that minus start_ms.
        # Subtracting end_ms biases the estimate early by the edge's whole
        # duration: 53s on this edge, a GTK build's slowest compile, which is what
        # made the reported anchor_spread_ms alarming.
        anchor_ms = 1_700_000_000_000
        edge = nbt.NinjaEdge(
            3_439_827, 3_492_825, int((anchor_ms + 3_439_827) * 1e6), "h", ["slow.cpp.o"]
        )
        self.assertEqual(edge.anchor_ms, anchor_ms)


# ---------------------------------------------------------------------------
# Run splitting
# ---------------------------------------------------------------------------


class RunSplittingTest(unittest.TestCase):
    def test_runs_are_split_by_anchor_gap(self):
        # Two invocations an hour apart, both with ms offsets starting near zero.
        # Each edge's logged time is its own command start, i.e. anchor + start_ms.
        old = nbt.NinjaEdge(0, 500, 1_700_000_000_000_000_000, "a", ["old.o"])
        new_a = nbt.NinjaEdge(0, 400, 1_700_003_600_000_000_000, "b", ["new_a.o"])
        new_b = nbt.NinjaEdge(400, 800, 1_700_003_600_400_000_000, "c", ["new_b.o"])
        runs, undated = nbt.group_runs([old, new_a, new_b])
        self.assertEqual(len(runs), 2)
        self.assertEqual([len(r.edges) for r in runs], [1, 2])
        self.assertEqual(runs[-1].anchor_ms, 1_700_003_600_000.0)
        self.assertEqual(runs[-1].window_ms, (0, 800))
        self.assertEqual(undated, [])

    def test_a_restat_edge_stays_with_its_own_invocation(self):
        # A restat edge logs one of its outputs' mtimes instead of a command start,
        # so its point estimate reads a whole duration late -- 93s for the Swift
        # edge of a GTK build. Clustering on point estimates alone split that edge
        # into a run of one, which then won run selection and produced a one-edge
        # trace; 15 other generated-file edges were dropped the same way.
        anchor = 1_700_000_000_000
        ordinary = [
            nbt.NinjaEdge(t, t + 100, int((anchor + t) * 1e6), f"h{t}", [f"f{t}.o"])
            for t in (0, 200, 400)
        ]
        restat = nbt.NinjaEdge(
            500, 93_600, int((anchor + 93_600) * 1e6), "swift", ["WebKit.swiftmodule"]
        )
        runs, undated = nbt.group_runs(ordinary + [restat])
        self.assertEqual(len(runs), 1)
        self.assertEqual(len(runs[0].edges), 4)
        # The ordinary edges outvote the restat one, so the anchor is still exact.
        self.assertEqual(runs[0].anchor_ms, anchor)
        self.assertEqual(undated, [])

    def test_undated_edges_are_held_back_from_every_run(self):
        # An edge with no output mtime cannot be dated: guessing by ms-offset overlap
        # would silently merge separate invocations.
        dated = nbt.NinjaEdge(0, 1000, 1_700_000_001_000_000_000, "a", ["a.o"])
        undated_edge = nbt.NinjaEdge(100, 200, 0, "b", ["b.o"])
        runs, undated = nbt.group_runs([dated, undated_edge])
        self.assertEqual(len(runs), 1)
        self.assertEqual(runs[0].edges, [dated])
        self.assertEqual(undated, [undated_edge])

    def test_merge_runs_rebases_each_invocation_to_its_own_wall_time(self):
        first = nbt.NinjaRun(1_000_000.0, [nbt.NinjaEdge(0, 500, 1, "a", ["a.o"])])
        second = nbt.NinjaRun(1_010_000.0, [nbt.NinjaEdge(0, 500, 1, "b", ["b.o"])])
        merged = nbt.merge_runs([first, second])
        self.assertEqual(merged.anchor_ms, 1_000_000.0)
        # The second run started 10s later, so its edge must not overlap the first.
        self.assertEqual(
            [(e.start_ms, e.end_ms) for e in merged.edges], [(0, 500), (10_000, 10_500)]
        )


# ---------------------------------------------------------------------------
# Stats loading
# ---------------------------------------------------------------------------


class StatsLoadingTest(ScratchDirectoryTest):
    def test_load_stats_dir_reads_start_and_duration(self):
        write_stats(self.tmp_path, 1_700_000_000_000_000, 0.25)
        (job,) = nbt.load_stats_dir([self.tmp_path])
        self.assertEqual(job.start_us, 1_700_000_000_000_000)
        self.assertEqual(job.dur_us, 250_000)
        self.assertEqual((job.module, job.input, job.opt), ("Demo", "f0.swift", "Onone"))
        self.assertEqual(job.label, "f0.swift")

    def test_whole_module_job_is_labelled_by_module(self):
        write_stats(self.tmp_path, 1_700_000_000_000_000, 0.1,
                    aux="Demo-all-arm64_apple_macosx26.0-swiftmodule-Onone")
        (job,) = nbt.load_stats_dir([self.tmp_path])
        self.assertEqual(job.label, "Demo (swiftmodule)")

    def test_missing_wall_timer_falls_back_to_one_microsecond(self):
        path = self.tmp_path / f"stats-1700000000000000-swift-frontend-{AUX}-1.json"
        path.write_text(json.dumps({"AST.NumSourceLines": 1}))
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            (job,) = nbt.load_stats_dir([self.tmp_path])
        self.assertEqual(job.dur_us, 1)
        self.assertIn("no whole-process .wall timer", stderr.getvalue())

    def test_non_stats_files_are_ignored(self):
        (self.tmp_path / "trace-1700000000000000-swift-frontend-x-1.csv").write_text("Time\n")
        (self.tmp_path / "notes.json").write_text("{}")
        self.assertEqual(nbt.load_stats_dir([self.tmp_path]), [])


# ---------------------------------------------------------------------------
# Clock alignment
# ---------------------------------------------------------------------------


class ClockAlignmentTest(unittest.TestCase):
    def test_refine_anchor_recovers_a_shifted_anchor(self):
        truth_ms = 1_700_000_000_000
        edges = [
            nbt.NinjaEdge(100, 500, 0, "a", ["A.swiftmodule"]),
            nbt.NinjaEdge(600, 1000, 0, "b", ["B.swiftmodule"]),
        ]
        # Jobs nearly fill their edges, which pins the anchor tightly.
        jobs = [
            make_job(int((truth_ms + 110) * 1000), 380_000),
            make_job(int((truth_ms + 610) * 1000), 380_000),
        ]
        guess = truth_ms - 700  # mtime-based estimate, 700ms too early
        refined, matched = nbt.refine_anchor(guess, edges, jobs)
        self.assertEqual(matched, 2)
        self.assertLessEqual(abs(refined - truth_ms), 20)

    def test_refine_anchor_corrects_a_stale_restat_mtime(self):
        # A restat edge whose outputs were not rewritten can put the mtime estimate a
        # whole edge duration out -- 11.2s in a measured rebuild -- so the search
        # window must be far wider than the sub-second spread of healthy mtimes.
        truth_ms = 1_700_000_000_000
        edges = [nbt.NinjaEdge(1, 11_212, 0, "a", ["CMakeFiles/B.dir/f2.swift.o"])]
        jobs = [make_job(int((truth_ms + 100) * 1000), 11_000_000)]
        refined, matched = nbt.refine_anchor(truth_ms - 11_212, edges, jobs)
        self.assertEqual(matched, 1)
        self.assertLessEqual(abs(refined - truth_ms), 200)

    def test_refine_anchor_is_a_noop_without_data(self):
        self.assertEqual(nbt.refine_anchor(123.0, [], []), (123.0, 0))

    def test_jobs_outside_the_run_window_are_separated(self):
        anchor = 1_700_000_000_000
        run = nbt.NinjaRun(anchor, [nbt.NinjaEdge(0, 1000, 0, "a", ["a.o"])])
        inside = make_job(int((anchor + 100) * 1000), 1000)
        # A configure-time try-compile from 30s before the build.
        outside = make_job(int((anchor - 30_000) * 1000), 1000)
        kept, dropped = nbt.partition_within_run([inside, outside], run, anchor)
        self.assertEqual(kept, [inside])
        self.assertEqual(dropped, [outside])


# ---------------------------------------------------------------------------
# Attributing frontend jobs to ninja edges
# ---------------------------------------------------------------------------


class AttributionTest(unittest.TestCase):
    def test_attribute_prefers_the_edge_naming_the_module(self):
        anchor = 1_700_000_000_000
        outer = nbt.NinjaEdge(0, 1000, 0, "outer", ["everything.stamp"])
        a_edge = nbt.NinjaEdge(100, 600, 0, "a", ["CMakeFiles/A.dir/f0.swift.o", "A.swiftmodule"])
        job = make_job(int((anchor + 200) * 1000), 100_000, module="A")
        self.assertEqual(nbt.attribute([job], [outer, a_edge], anchor), 0)
        self.assertIs(job.edge, a_edge)

    def test_attribute_matches_a_source_whose_name_swift_sanitized(self):
        # A stats file spells "Foundation+Extras.swift" as "Foundation_Extras.swift",
        # so scoring it against the edge's real object names needs both sides cleaned.
        anchor = 1_700_000_000_000
        edge = nbt.NinjaEdge(
            100, 600, 0, "a", ["CMakeFiles/A.dir/Foundation+Extras.swift.o"]
        )
        job = make_job(int((anchor + 200) * 1000), 100_000, module="A")
        job.input = "Foundation_Extras.swift"
        self.assertEqual(nbt._match_score(job, edge), 5)

    def test_attribute_reports_jobs_with_no_containing_edge(self):
        anchor = 1_700_000_000_000
        edge = nbt.NinjaEdge(0, 100, 0, "a", ["a.o"])
        job = make_job(int((anchor + 5000) * 1000), 10_000)
        self.assertEqual(nbt.attribute([job], [edge], anchor), 1)
        self.assertIsNone(job.edge)

    def test_identical_sources_in_several_targets_go_to_their_own_edges(self):
        # Regression: WebKit compiles the same TestWebKitAPI runner sources into seven
        # test targets, so seven concurrent edges share every output basename. Matching
        # on the object name alone tied across all of them and pooled the modules.
        anchor = 1_700_000_000_000
        edges = [
            nbt.NinjaEdge(
                0,
                1000 + i,
                0,
                f"h{i}",
                [
                    f"Tools/CMakeFiles/{target}.dir/Runner/TestRunner.swift.o",
                    f"Tools/CMakeFiles/{target}.dir/Runner/GoogleTestsController.swift.o",
                ],
            )
            for i, target in enumerate(("TestWTF", "TestIPC", "TestWebKit"))
        ]
        jobs = []
        for module in ("TestWTF", "TestIPC", "TestWebKit"):
            job = make_job(int((anchor + 100) * 1000), 500_000, module=module)
            job.input = "TestRunner.swift"
            jobs.append(job)

        self.assertEqual(nbt.attribute(jobs, edges, anchor), 0)
        # Each module to its own target's edge.
        self.assertEqual([j.edge for j in jobs], edges)

    def test_a_containing_clang_edge_does_not_claim_a_swift_job(self):
        # Regression: substring-matching the module name against output paths pinned a
        # WebKit Swift job onto a neighbouring UnifiedSource .mm.o clang edge, because
        # every path in a WebKit tree contains "WebKit".
        anchor = 1_700_000_000_000
        clang = nbt.NinjaEdge(
            0, 1000, 0, "c", ["WebKitBuild/WebKit.dir/UnifiedSource-mac-4-nonARC.mm.o"]
        )
        job = make_job(int((anchor + 200) * 1000), 100_000, module="WebKit")
        self.assertEqual(nbt.attribute([job], [clang], anchor), 1)
        self.assertIsNone(job.edge)

    def test_sibling_swift_edges_do_not_pool_two_modules(self):
        # Regression: TestWebKit's jobs landed in TestIPC's edge because neither edge
        # names TestWebKit in its outputs and TestIPC's window was shorter.
        anchor = 1_700_000_000_000
        ipc = nbt.NinjaEdge(0, 1000, 0, "a", ["CMakeFiles/TestIPC.dir/TestRunner.swift.o"])
        webkit = nbt.NinjaEdge(
            0, 1200, 0, "b", ["CMakeFiles/TestWebKit.dir/Foo.swift.o", "TestWebKit.swiftmodule"]
        )
        ipc_job = make_job(int((anchor + 100) * 1000), 100_000, module="TestIPC")
        ipc_job.input = "TestRunner.swift"
        wk_job = make_job(int((anchor + 100) * 1000), 100_000, module="TestWebKit")
        wk_job.input = "Foo.swift"

        self.assertEqual(nbt.attribute([ipc_job, wk_job], [ipc, webkit], anchor), 0)
        self.assertIs(ipc_job.edge, ipc)
        self.assertIs(wk_job.edge, webkit)

    def test_whole_module_job_matches_its_swiftmodule_edge(self):
        anchor = 1_700_000_000_000
        edge = nbt.NinjaEdge(0, 1000, 0, "a", ["CMakeFiles/A.dir/f0.swift.o", "A.swiftmodule"])
        job = make_job(int((anchor + 100) * 1000), 100_000, module="A")
        job.input = "all"
        self.assertEqual(nbt.attribute([job], [edge], anchor), 0)
        self.assertIs(job.edge, edge)

    def test_is_swift_edge_recognizes_swift_outputs(self):
        self.assertTrue(nbt.is_swift_edge(nbt.NinjaEdge(0, 1, 0, "a", ["A.swiftmodule"])))
        self.assertTrue(nbt.is_swift_edge(nbt.NinjaEdge(0, 1, 0, "a", ["d/f.swift.o"])))
        self.assertTrue(
            nbt.is_swift_edge(nbt.NinjaEdge(0, 1, 0, "a", ["WebKit-Swift-Generated.h"]))
        )
        self.assertFalse(
            nbt.is_swift_edge(nbt.NinjaEdge(0, 1, 0, "a", ["d/f.cpp.o", "d/f.mm.o"]))
        )


# ---------------------------------------------------------------------------
# Recorded driver jobs
# ---------------------------------------------------------------------------


class RecordedJobsTest(ScratchDirectoryTest):
    def test_load_jobs_log_pairs_began_with_finished(self):
        log = write_jobs_log(
            self.tmp_path,
            [
                {"t": 100.0, "kind": "driver-began", "rec": 7, "cwd": "/b", "compiler": "swiftc"},
                {"t": 100.1, "kind": "began", "rec": 7, "pid": 11, "name": "generate-pcm",
                 "output": "/mc/SwiftShims-ABC123.pcm"},
                {"t": 100.3, "kind": "finished", "rec": 7, "pid": 11, "exit_status": 0},
                {"t": 100.4, "kind": "began", "rec": 7, "pid": 12, "name": "compile",
                 "output": "/b/f0.swift.o", "inputs": [{"path": "/s/f0.swift", "type": "swift"}]},
                {"t": 101.0, "kind": "finished", "rec": 7, "pid": 12, "exit_status": 0},
                {"t": 101.1, "kind": "driver-finished", "rec": 7, "exit_status": 0},
            ],
        )
        (inv,) = nbt.load_jobs_log(log)
        self.assertEqual((inv.rec, inv.cwd), (7, "/b"))
        self.assertEqual(inv.start_us, 100_000_000)
        self.assertEqual(inv.end_us, 101_100_000)
        self.assertEqual([j.name for j in inv.jobs], ["generate-pcm", "compile"])
        self.assertAlmostEqual(inv.jobs[0].dur_us, 200_000, delta=0.2)
        self.assertEqual(inv.jobs[0].label, "SwiftShims (pcm)")
        self.assertTrue(inv.jobs[0].builds_a_dependency)
        self.assertEqual(inv.jobs[1].label, "f0.swift")  # named by its input
        self.assertFalse(inv.jobs[1].builds_a_dependency)

    def test_an_interrupted_recorder_keeps_its_span_and_in_flight_jobs(self):
        # A killed recorder never writes driver-finished. Seen for real: 311 began vs
        # 307 finished, which left the invocation a zero-length instant and threw away
        # the four module jobs that were still running.
        log = write_jobs_log(
            self.tmp_path,
            [
                {"t": 100.0, "kind": "driver-began", "rec": 3, "cwd": "/b"},
                {"t": 100.5, "kind": "began", "rec": 3, "pid": 11, "name": "generate-pcm",
                 "output": "/mc/Done-A.pcm"},
                {"t": 101.0, "kind": "finished", "rec": 3, "pid": 11, "exit_status": 0},
                {"t": 101.5, "kind": "began", "rec": 3, "pid": 12, "name": "generate-pcm",
                 "output": "/mc/Killed-B.pcm"},
                {"t": 102.0, "kind": "began", "rec": 3, "pid": 13, "name": "compile",
                 "output": "/b/a.o"},
                {"t": 103.0, "kind": "finished", "rec": 3, "pid": 13, "exit_status": 0},
            ],
        )
        (inv,) = nbt.load_jobs_log(log)
        self.assertEqual(inv.start_us, 100_000_000)
        self.assertEqual(inv.end_us, 103_000_000)  # the last thing heard, not the driver record
        unfinished = [j for j in inv.jobs if j.unfinished]
        self.assertEqual([j.label for j in unfinished], ["Killed (pcm)"])
        self.assertEqual(unfinished[0].end_us, 103_000_000)
        self.assertIsNone(unfinished[0].exit_status)

    def test_load_jobs_log_separates_concurrent_recorders(self):
        events = []
        for rec in (7, 8):
            events += [
                {"t": 100.0, "kind": "driver-began", "rec": rec, "cwd": "/b"},
                {"t": 100.1, "kind": "began", "rec": rec, "pid": 11, "name": "compile",
                 "output": f"/b/{rec}.o"},
                {"t": 100.5, "kind": "finished", "rec": rec, "pid": 11, "exit_status": 0},
                {"t": 100.6, "kind": "driver-finished", "rec": rec, "exit_status": 0},
            ]
        # Concurrent ninja edges interleave in one log and can reuse child pids.
        log = write_jobs_log(self.tmp_path, events)
        invocations = nbt.load_jobs_log(log)
        self.assertEqual(len(invocations), 2)
        self.assertEqual([len(inv.jobs) for inv in invocations], [1, 1])
        self.assertEqual({inv.jobs[0].output for inv in invocations}, {"/b/7.o", "/b/8.o"})

    def test_load_jobs_log_survives_a_torn_line(self):
        path = self.tmp_path / "jobs.jsonl"
        path.write_text(
            json.dumps({"t": 1.0, "kind": "driver-began", "rec": 1, "cwd": "/b"}) + "\n"
            + '{"t": 1.1, "kind": "beg\n'  # truncated by a concurrent append
            + json.dumps({"t": 1.2, "kind": "began", "rec": 1, "pid": 2, "name": "compile",
                          "output": "/b/a.o"}) + "\n"
            + json.dumps({"t": 1.5, "kind": "finished", "rec": 1, "pid": 2}) + "\n"
        )
        (inv,) = nbt.load_jobs_log(path)
        self.assertEqual(len(inv.jobs), 1)

    def test_anchor_from_invocations_ignores_a_misleading_mtime(self):
        # A restat edge whose outputs were not rewritten logs an mtime that is not an
        # output-write time -- observed in a real rebuild as the newest *input* mtime,
        # which predates the build. The recording pins the anchor without it.
        truth_ms = 1_700_000_000_000
        # A restat edge logs a real output mtime where every other edge logs a
        # command start time, so this one edge's estimate is out by its own
        # duration -- the case the recording has to override.
        edge = nbt.NinjaEdge(
            1, 11_212, int((truth_ms + 11_072) * 1e6), "h", ["CMakeFiles/B.dir/f2.swift.o"]
        )
        log = write_jobs_log(
            self.tmp_path,
            [
                {"t": (truth_ms + 1) / 1000.0, "kind": "driver-began", "rec": 5, "cwd": "/build"},
                {"t": (truth_ms + 10) / 1000.0, "kind": "began", "rec": 5, "pid": 9,
                 "name": "compile", "output": "/build/CMakeFiles/B.dir/f2.swift.o"},
                {"t": (truth_ms + 900) / 1000.0, "kind": "finished", "rec": 5, "pid": 9},
                {"t": (truth_ms + 950) / 1000.0, "kind": "driver-finished", "rec": 5},
            ],
        )
        invocations = nbt.load_jobs_log(log)
        # The bad estimate.
        self.assertAlmostEqual(edge.anchor_ms, truth_ms + 11_071, delta=1)
        anchor, matched = nbt.anchor_from_invocations(invocations, [edge])
        self.assertEqual(matched, 1)
        if anchor is None:
            self.fail("the recording should have pinned an anchor")
        self.assertAlmostEqual(anchor, truth_ms, delta=1)

    def test_anchor_from_invocations_reports_nothing_without_a_match(self):
        inv = nbt.DriverInvocation(rec=1, cwd="/build", start_us=0.0, end_us=1.0)
        self.assertEqual(nbt.anchor_from_invocations([inv], []), (None, 0))

    def test_batch_constituents_collapse_into_one_process(self):
        # swift-driver compiles several primaries in one frontend process, then
        # reports each of them under its own quasi-PID spanning that whole
        # process. Counted individually they multiply both the concurrency and the
        # total frontend time by the batch size -- 30 bars for 16 processes on a
        # measured macOS build.
        def began(pid, batch, source):
            return {
                "t": 100.1, "kind": "began", "rec": 1, "pid": pid, "name": "compile",
                "batch": f"/src/{batch}", "inputs": [f"/src/{source}"],
                "output": f"/build/{source}.o",
            }

        log = write_jobs_log(
            self.tmp_path,
            [
                {"t": 100.0, "kind": "driver-began", "rec": 1, "cwd": "/build"},
                began(-1000, "a.swift", "a.swift"),
                began(-1001, "a.swift", "b.swift"),
                began(-1002, "c.swift", "c.swift"),
                {"t": 110.0, "kind": "finished", "rec": 1, "pid": -1000, "exit_status": 0},
                {"t": 110.0, "kind": "finished", "rec": 1, "pid": -1001, "exit_status": 0},
                {"t": 112.0, "kind": "finished", "rec": 1, "pid": -1002, "exit_status": 0},
                {"t": 112.5, "kind": "driver-finished", "rec": 1},
            ],
        )
        (inv,) = nbt.load_jobs_log(log)
        self.assertEqual(len(inv.jobs), 2)
        batch, lone = inv.jobs
        self.assertEqual(batch.label, "a.swift +1")
        self.assertEqual(lone.label, "c.swift")
        # The process ran once, so its span is its own, not the sum of its files.
        self.assertAlmostEqual(batch.dur_us, 9_900_000, delta=1)
        # The lead primary's output is the one swift names the stats file after.
        self.assertEqual(batch.output, "/build/a.swift.o")

    def test_attribute_invocations_matches_on_resolved_output_paths(self):
        edge = nbt.NinjaEdge(0, 1000, 0, "h", ["CMakeFiles/A.dir/f0.swift.o", "A.swiftmodule"])
        other = nbt.NinjaEdge(0, 1000, 0, "g", ["CMakeFiles/Z.dir/f0.swift.o"])
        inv = nbt.DriverInvocation(rec=1, cwd="/build", start_us=0.0, end_us=1_000_000.0)
        inv.jobs.append(
            nbt.RecordedJob("compile", 0.0, 1.0, "/build/CMakeFiles/A.dir/f0.swift.o", [], 0)
        )
        self.assertEqual(nbt.attribute_invocations([inv], [other, edge], 0.0), 0)
        self.assertIs(inv.edge, edge)

    def test_recorded_and_stats_views_of_one_process_do_not_double_count(self):
        # The same emit-module process appears in both sources: the recorder knows it
        # as "A.swiftmodule", the stats file as module A with out=swiftmodule.
        stats_job = make_job(1_000_000, 330_000, module="A")
        stats_job.input, stats_job.out = "all", "swiftmodule"
        stats_job.stats = {"AST.NumSourceLines": 7}
        inv = nbt.DriverInvocation(rec=1, cwd="/build", start_us=0.0, end_us=2_000_000.0)
        inv.jobs.append(
            nbt.RecordedJob("emit-module", 1_000_000.0, 1_330_000.0, "/build/A.swiftmodule", [], 0)
        )

        spans = nbt.spans_for_edge([stats_job], inv)
        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0].cat, "emit-module")               # timing from the recorder
        self.assertEqual(spans[0].args["AST.NumSourceLines"], 7)    # counters from the stats file
        # A whole-module job is the one case where the stats label says more than
        # the output path does, so here it wins.
        self.assertEqual(spans[0].name, "A (swiftmodule)")

    def test_a_sanitized_stats_name_still_merges_with_the_recorded_job(self):
        # Regression, seen on a GTK build: swift rewrites everything but [A-Za-z0-9.]
        # when it names a stats file, so the stats for "Foundation+Extras.swift" call
        # it "Foundation_Extras.swift" while the recorder reports the real path. The
        # merge missed, drew both views of the one process, and inflated the load
        # counter -- peak 8 concurrent frontends reported where 6 ran.
        stats_job = make_job(1_000_000, 300_000, module="A")
        stats_job.input = "Foundation_Extras.swift"
        stats_job.stats = {"AST.NumSourceLines": 11}
        inv = nbt.DriverInvocation(rec=1, cwd="/build", start_us=0.0, end_us=2_000_000.0)
        inv.jobs.append(
            nbt.RecordedJob(
                "compile",
                1_000_000.0,
                1_300_000.0,
                "/build/CMakeFiles/A.dir/Foundation+Extras.swift.o",
                [],
                0,
            )
        )

        (span,) = nbt.spans_for_edge([stats_job], inv)
        self.assertEqual(span.args["AST.NumSourceLines"], 11)
        # Only the recorder knows the real spelling, so its label is the one to keep.
        self.assertEqual(span.name, "Foundation+Extras.swift")

    def test_the_in_process_dependency_scan_is_kept(self):
        # The scan never appears in -parseable-output, so it must survive the merge.
        scan = make_job(500_000, 100_000, module="A")
        scan.input, scan.out = "all", ""
        scan.stats = {nbt.DEP_SCAN_COUNTER: 1681}
        inv = nbt.DriverInvocation(rec=1, cwd="/build", start_us=0.0, end_us=2_000_000.0)
        inv.jobs.append(
            nbt.RecordedJob("generate-pcm", 700_000.0, 750_000.0, "/mc/SwiftShims-A.pcm", [], 0)
        )

        spans = nbt.spans_for_edge([scan], inv)
        self.assertEqual([s.name for s in spans], ["A (dependency scan)", "SwiftShims (pcm)"])

    def test_module_jobs_reach_the_trace_and_count_as_load(self):
        anchor = 1_700_000_000_000
        edge = nbt.NinjaEdge(0, 11_000, 0, "h", ["CMakeFiles/A.dir/f0.swift.o"])
        run = nbt.NinjaRun(anchor, [edge])
        inv = nbt.DriverInvocation(
            rec=1, cwd="/build", start_us=anchor * 1000.0, end_us=(anchor + 11_000) * 1000.0
        )
        base = anchor * 1000.0
        inv.jobs += [
            nbt.RecordedJob("compile-module-from-interface", base + 100, base + 10_000_000,
                            "/mc/Swift-ABC.swiftmodule", [], 0),
            nbt.RecordedJob("compile", base + 10_100_000, base + 10_400_000,
                            "/build/CMakeFiles/A.dir/f0.swift.o", [], 0),
        ]
        inv.edge = edge

        trace = nbt.build_trace(run, [], anchor, cores=8, relative=True, invocations=[inv])
        spans = [
            e for e in trace["traceEvents"]
            if e["ph"] == "X" and e["pid"] >= nbt.PID_SWIFT_BASE
        ]
        self.assertEqual({e["name"] for e in spans}, {"Swift (interface)", "f0.swift"})
        counters = [e for e in trace["traceEvents"] if e["ph"] == "C"]
        self.assertEqual(max(c["args"]["swift_frontends"] for c in counters), 1)
        # The edge is represented by its children, so it adds no load of its own.
        self.assertEqual(max(c["args"]["other_edges"] for c in counters), 0)


# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------


class TraceEmissionTest(unittest.TestCase):
    def test_emitted_timestamps_are_whole_microseconds(self):
        # Epoch microseconds sit near 1.8e15, where a float64 steps by a quarter of
        # a microsecond, so a fractional anchor put a slice's start and the previous
        # slice's start-plus-duration on different floats. Perfetto converts ts and
        # dur to integer nanoseconds separately, so the residue read as a 64ns
        # overlap and spilled 4052 of a GTK build's 11950 slices onto overflow
        # tracks. Whole microseconds serialize as JSON integers, which convert
        # exactly.
        anchor = 1_787_886_968_794.6
        edges = [
            nbt.NinjaEdge(0, 5, int(anchor * 1e6), "a", ["a.h"]),
            nbt.NinjaEdge(5, 11, int((anchor + 5) * 1e6), "b", ["b.h"]),
        ]
        trace = nbt.build_trace(nbt.NinjaRun(anchor, edges), [], anchor, cores=8)
        self.assertEqual(nbt.validate_trace(trace), [])
        slices = sorted(
            (e for e in trace["traceEvents"] if e["ph"] == "X"), key=lambda e: e["ts"]
        )
        for event in slices:
            self.assertIsInstance(event["ts"], int)
            self.assertIsInstance(event["dur"], int)
        # The edges are adjacent and share a lane, so the boundary has to be exact.
        self.assertEqual(slices[0]["tid"], slices[1]["tid"])
        self.assertEqual(slices[0]["ts"] + slices[0]["dur"], slices[1]["ts"])

    def test_pack_lanes_reuses_a_lane_once_it_is_free(self):
        #  A: 0-10   B: 5-15   C: 20-30
        self.assertEqual(nbt.pack_lanes([(0, 10), (5, 15), (20, 30)]), [0, 1, 0])

    def test_pack_lanes_handles_fully_concurrent_spans(self):
        self.assertEqual(sorted(nbt.pack_lanes([(0, 10)] * 4)), [0, 1, 2, 3])

    def test_edges_that_spawned_frontends_are_not_counted_twice(self):
        anchor = 1_700_000_000_000
        swift_edge = nbt.NinjaEdge(0, 1000, 0, "a", ["A.swiftmodule"])
        clang_edge = nbt.NinjaEdge(0, 1000, 0, "b", ["b.o"])
        run = nbt.NinjaRun(anchor, [swift_edge, clang_edge])
        jobs = [make_job(int((anchor + 100) * 1000), 100_000)]
        jobs[0].edge = swift_edge

        trace = nbt.build_trace(run, jobs, anchor, cores=1, relative=True)
        counters = [e for e in trace["traceEvents"] if e["ph"] == "C"]
        peak = max(counters, key=lambda c: c["args"]["swift_frontends"] + c["args"]["other_edges"])
        # One frontend plus the clang edge; the swift edge itself is represented by
        # its child, so the total is 2 rather than 3.
        self.assertEqual(
            peak["args"], {"swift_frontends": 1, "other_edges": 1, "over_cores": 1}
        )

    def test_counter_track_returns_to_zero(self):
        anchor = 1_700_000_000_000
        run = nbt.NinjaRun(anchor, [nbt.NinjaEdge(0, 10, 0, "a", ["a.o"])])
        trace = nbt.build_trace(run, [], anchor, cores=4, relative=True)
        counters = [e for e in trace["traceEvents"] if e["ph"] == "C"]
        self.assertEqual(counters[0]["args"]["other_edges"], 0)
        self.assertEqual(counters[-1]["args"]["other_edges"], 0)

    def test_rows_for_the_same_module_are_disambiguated_by_edge(self):
        anchor = 1_700_000_000_000
        compile_edge = nbt.NinjaEdge(0, 1000, 0, "a", ["CMakeFiles/A.dir/f0.swift.o"])
        header_edge = nbt.NinjaEdge(0, 1000, 0, "b", ["A-Swift-Generated.h"])
        run = nbt.NinjaRun(anchor, [compile_edge, header_edge])
        jobs = []
        for edge in (compile_edge, header_edge):
            job = make_job(int((anchor + 100) * 1000), 10_000, module="A")
            job.edge = edge
            jobs.append(job)

        trace = nbt.build_trace(run, jobs, anchor, cores=8, relative=True)
        names = sorted(
            e["args"]["name"]
            for e in trace["traceEvents"]
            if e["ph"] == "M" and e["name"] == "process_name" and "swiftc" in e["args"]["name"]
        )
        self.assertEqual(names, [
            "swiftc A [A-Swift-Generated.h]",
            "swiftc A [f0.swift.o]",
        ])

    def test_no_tid_doubles_as_a_pid(self):
        # Regression: trace_processor gives every process a main thread whose tid
        # equals its pid, so a lane numbered like a pid shares that thread's track.
        # A real WebKit trace had 22 such values -- tid 10 was a ninja lane while pid
        # 10 was a swiftc row -- and Perfetto spilled the overlaps onto extra tracks.
        anchor = 1_700_000_000_000
        edges = [
            nbt.NinjaEdge(i * 10, i * 10 + 5, 0, f"h{i}", [f"CMakeFiles/M{i}.dir/f.swift.o"])
            for i in range(30)
        ]
        run = nbt.NinjaRun(anchor, edges)
        jobs = []
        for i, edge in enumerate(edges):
            job = make_job(int((anchor + i * 10 + 1) * 1000), 3000, module=f"M{i}")
            job.edge = edge
            jobs.append(job)

        trace = nbt.build_trace(run, jobs, anchor, cores=8, relative=True)
        pids = {e["pid"] for e in trace["traceEvents"]}
        tids = {e["tid"] for e in trace["traceEvents"] if e["ph"] == "X"}
        self.assertEqual(pids & tids, set(), sorted(pids & tids))
        self.assertGreater(min(tids), max(pids))
        self.assertEqual(nbt.validate_trace(trace), [])

    def test_validate_trace_catches_a_shared_track(self):
        trace = {
            "traceEvents": [
                {"ph": "X", "pid": 1, "tid": 9, "ts": 0, "dur": 100, "name": "a", "args": {}},
                {"ph": "X", "pid": 2, "tid": 9, "ts": 50, "dur": 100, "name": "b", "args": {}},
            ]
        }
        problems = nbt.validate_trace(trace)
        self.assertTrue(any("overlapping" in p for p in problems), problems)

    def test_validate_trace_catches_a_pid_used_as_a_tid(self):
        trace = {
            "traceEvents": [
                {"ph": "M", "pid": 7, "tid": 0, "name": "process_name", "args": {"name": "x"}},
                {"ph": "X", "pid": 1, "tid": 7, "ts": 0, "dur": 1, "name": "a", "args": {}},
            ]
        }
        problems = nbt.validate_trace(trace)
        self.assertTrue(any("both a pid and a tid" in p for p in problems), problems)

    def test_validate_trace_passes_a_clean_trace(self):
        anchor = 1_700_000_000_000
        run = nbt.NinjaRun(anchor, [nbt.NinjaEdge(0, 10, 0, "a", ["a.o"])])
        self.assertEqual(nbt.validate_trace(nbt.build_trace(run, [], anchor, cores=1)), [])

    def test_no_tid_is_shared_by_two_processes(self):
        # Perfetto resolves thread tracks by tid alone, so per-process lane numbers
        # would collide onto one track and look like overlapping slices.
        anchor = 1_700_000_000_000
        a_edge = nbt.NinjaEdge(0, 1000, 0, "a", ["A.swiftmodule"])
        b_edge = nbt.NinjaEdge(0, 1000, 0, "b", ["B.swiftmodule"])
        run = nbt.NinjaRun(anchor, [a_edge, b_edge])
        jobs = []
        for edge, module in ((a_edge, "A"), (b_edge, "B")):
            for offset in (100, 110):  # concurrent, so each needs its own lane
                job = make_job(int((anchor + offset) * 1000), 500_000, module=module)
                job.edge = edge
                jobs.append(job)

        trace = nbt.build_trace(run, jobs, anchor, cores=8, relative=True)
        spans = [e for e in trace["traceEvents"] if e["ph"] == "X"]
        owners: dict[int, set[int]] = {}
        for span in spans:
            owners.setdefault(span["tid"], set()).add(span["pid"])
        self.assertTrue(all(len(pids) == 1 for pids in owners.values()), owners)
        # And no two slices on one track may overlap.
        per_tid: dict[int, list[dict]] = {}
        for span in spans:
            per_tid.setdefault(span["tid"], []).append(span)
        for group in per_tid.values():
            group.sort(key=lambda e: e["ts"])
            for first, second in zip(group, group[1:]):
                self.assertGreaterEqual(second["ts"], first["ts"] + first["dur"])

    def test_lanes_get_thread_names(self):
        anchor = 1_700_000_000_000
        run = nbt.NinjaRun(anchor, [nbt.NinjaEdge(0, 10, 0, "a", ["a.o"])])
        trace = nbt.build_trace(run, [], anchor, cores=1, relative=True)
        thread_names = [
            e for e in trace["traceEvents"] if e["ph"] == "M" and e["name"] == "thread_name"
        ]
        self.assertTrue(thread_names)
        self.assertEqual(thread_names[0]["args"]["name"], "lane 0")
        self.assertNotEqual(thread_names[0]["tid"], 0)  # tid 0 is reserved for process metadata

    def test_trace_shape_is_a_valid_catapult_object(self):
        anchor = 1_700_000_000_000
        edge = nbt.NinjaEdge(0, 1000, 0, "a", ["A.swiftmodule", "CMakeFiles/A.dir/f0.swift.o"])
        run = nbt.NinjaRun(anchor, [edge])
        job = make_job(int((anchor + 100) * 1000), 100_000)
        job.edge = edge

        trace = nbt.build_trace(run, [job], anchor, cores=8, relative=True)
        json.dumps(trace)  # must be serializable
        self.assertEqual(trace["displayTimeUnit"], "ms")

        names = {
            e["pid"]: e["args"]["name"]
            for e in trace["traceEvents"]
            if e["ph"] == "M" and e["name"] == "process_name"
        }
        self.assertEqual(names[nbt.PID_NINJA], "ninja")
        self.assertEqual(names[nbt.PID_SWIFT_BASE], "swiftc A")

        spans = [e for e in trace["traceEvents"] if e["ph"] == "X"]
        ninja_span = next(e for e in spans if e["pid"] == nbt.PID_NINJA)
        job_span = next(e for e in spans if e["pid"] == nbt.PID_SWIFT_BASE)
        self.assertEqual(ninja_span["ts"], 0)
        self.assertEqual(ninja_span["dur"], 1_000_000)
        # The frontend job must sit inside its edge's window.
        self.assertLessEqual(ninja_span["ts"], job_span["ts"])
        self.assertLessEqual(
            job_span["ts"] + job_span["dur"], ninja_span["ts"] + ninja_span["dur"]
        )
        self.assertEqual(job_span["args"]["ninja_edge"], ninja_span["name"])

    def test_relative_and_absolute_timestamps_differ_by_the_anchor(self):
        anchor = 1_700_000_000_000
        run = nbt.NinjaRun(anchor, [nbt.NinjaEdge(0, 10, 0, "a", ["a.o"])])
        rel = nbt.build_trace(run, [], anchor, cores=1, relative=True)
        absolute = nbt.build_trace(run, [], anchor, cores=1, relative=False)
        rel_span = next(e for e in rel["traceEvents"] if e["ph"] == "X")
        abs_span = next(e for e in absolute["traceEvents"] if e["ph"] == "X")
        self.assertEqual(abs_span["ts"] - rel_span["ts"], anchor * 1000)


# ---------------------------------------------------------------------------
# End to end
# ---------------------------------------------------------------------------


class MainTest(ScratchDirectoryTest):
    def test_main_writes_a_trace(self):
        anchor_ns = 1_700_000_001_000_000_000  # edge ends 1000ms after build start
        log = self.tmp_path / ".ninja_log"
        log.write_text(ninja_log([f"0\t1000\t{anchor_ns}\tA.swiftmodule\tabc"]))
        stats = self.tmp_path / "stats"
        stats.mkdir()
        write_stats(stats, 1_700_000_000_100_000, 0.5)  # 100ms in, 500ms long
        out = self.tmp_path / "trace.json"

        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            rc = nbt.main(
                [
                    "--ninja-log", str(log),
                    "--stats-dir", str(stats),
                    "-o", str(out),
                    "--cores", "4",
                    "--relative",
                ]
            )
        self.assertEqual(rc, 0)
        trace = json.loads(out.read_text())
        spans = [e for e in trace["traceEvents"] if e["ph"] == "X"]
        self.assertEqual(len(spans), 2)
        self.assertIn("swift jobs: 1 loaded, 1 attributed", stderr.getvalue())

    def test_main_errors_on_an_empty_log(self):
        log = self.tmp_path / ".ninja_log"
        log.write_text("# ninja log v6\n")

        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            self.assertEqual(nbt.main(["--ninja-log", str(log)]), 1)
        self.assertIn("no usable records", stderr.getvalue())

    def _two_invocation_log(self):
        """A big first invocation carrying all the stats, then a small later one."""
        first_ns, second_ns = 1_700_000_000_000_000_000, 1_700_003_600_000_000_000
        log = self.tmp_path / ".ninja_log"
        log.write_text(
            ninja_log(
                [
                    f"0\t1000\t{first_ns}\tA.swiftmodule\tabc",
                    f"0\t2000\t{first_ns}\tB.swiftmodule\tdef",
                    f"0\t500\t{second_ns}\tC.swiftmodule\tghi",
                ]
            )
        )
        stats = self.tmp_path / "stats"
        stats.mkdir()
        write_stats(stats, 1_700_000_000_100_000, 0.5)
        write_stats(stats, 1_700_000_000_200_000, 0.5, aux=AUX.replace("Demo", "Demo2"))
        return log, stats

    def _run_main(self, extra: list[str]) -> str:
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            self.assertEqual(nbt.main(extra), 0)
        return stderr.getvalue()

    def test_the_busiest_invocation_wins_when_nothing_bounds_the_time(self):
        log, stats = self._two_invocation_log()
        stderr = self._run_main(
            ["--ninja-log", str(log), "--stats-dir", str(stats), "-o", str(self.tmp_path / "t.json")]
        )
        self.assertIn("ninja run: 2 edges", stderr)

    def test_since_epoch_ms_picks_the_later_invocation(self):
        # Logs accumulate across builds, so without a lower bound the invocation
        # the stats files came from beats the one that just ran.
        log, stats = self._two_invocation_log()
        out = self.tmp_path / "t.json"
        stderr = self._run_main(
            [
                "--ninja-log", str(log),
                "--stats-dir", str(stats),
                "--since-epoch-ms", "1700003000000",
                "-o", str(out),
            ]
        )
        self.assertIn("ninja run: 1 edges", stderr)
        names = {e["name"] for e in json.loads(out.read_text())["traceEvents"] if e["ph"] == "X"}
        self.assertEqual(names, {"C.swiftmodule"})

    def test_since_epoch_ms_after_every_invocation_writes_nothing(self):
        log, stats = self._two_invocation_log()
        out = self.tmp_path / "t.json"
        stderr = self._run_main(
            [
                "--ninja-log", str(log),
                "--stats-dir", str(stats),
                "--since-epoch-ms", "1700007200000",
                "-o", str(out),
            ]
        )
        self.assertIn("nothing to trace", stderr)
        self.assertFalse(out.exists())

    def test_a_gz_suffix_writes_a_gzipped_trace(self):
        log, stats = self._two_invocation_log()
        out = self.tmp_path / "t.json.gz"
        self._run_main(["--ninja-log", str(log), "--stats-dir", str(stats), "-o", str(out)])
        with gzip.open(out, "rt") as f:
            trace = json.load(f)
        self.assertEqual(trace["displayTimeUnit"], "ms")


# ---------------------------------------------------------------------------
# The recorder
# ---------------------------------------------------------------------------


class RecorderTest(unittest.TestCase):
    def test_batch_key_names_the_process_a_quasi_pid_job_ran_in(self):
        # Every constituent of a batch repeats the batch's whole command line, so
        # its first -primary-file identifies the process they share.
        message = {
            "kind": "began",
            "name": "compile",
            "pid": -1001,
            "command_arguments": [
                "-frontend", "-c",
                "-primary-file", "f0.swift",
                "-primary-file", "f1.swift",
                "f2.swift", "-o", "f1.o",
            ],
        }
        self.assertEqual(recorder.batch_key(message), "f0.swift")
        # A real pid is a process in its own right and needs no grouping.
        self.assertIsNone(recorder.batch_key({**message, "pid": 4242}))

    def test_recorder_consumes_its_own_flags(self):
        compiler, jobs_log, args = recorder.parse_args(
            [
                "-c",
                "--original-swift-compiler=/usr/bin/swiftc",
                "f0.swift",
                "--jobs-log=/tmp/jobs.jsonl",
                "-Onone",
            ]
        )
        self.assertEqual(compiler, "/usr/bin/swiftc")
        self.assertEqual(jobs_log, "/tmp/jobs.jsonl")
        # Our flags must not reach the compiler, and everything else must survive in
        # order -- the flags can appear anywhere, since they ride in CMAKE_Swift_FLAGS.
        self.assertEqual(args, ["-c", "f0.swift", "-Onone"])

    def test_recorder_defaults_to_swiftc_on_path(self):
        # CMake's static-archive rule expands neither <FLAGS> nor link options, so
        # those edges arrive without the flag and must still work.
        compiler, jobs_log, args = recorder.parse_args(["-emit-library", "-static"])
        self.assertEqual(compiler, "swiftc")
        self.assertIsNone(jobs_log)
        self.assertEqual(args, ["-emit-library", "-static"])

    def test_recorder_forwards_every_flag_it_does_not_own(self):
        # swiftc-wrapper.py runs above the recorder and consumes its own flags
        # before they reach here, so anything left over belongs to the compiler.
        _, _, args = recorder.parse_args(
            ["-c", "--emit-ninja-depfile=/tmp/t.d", "f0.swift"]
        )
        self.assertEqual(args, ["-c", "--emit-ninja-depfile=/tmp/t.d", "f0.swift"])
        # Dropping every `--` argument instead would eat the version probe in
        # WebKitFeatures.cmake, and the operand of an `-Xcc --foo` pair.
        _, _, args = recorder.parse_args(["--version"])
        self.assertEqual(args, ["--version"])
        _, _, args = recorder.parse_args(["-Xcc", "--sysroot=/tmp/sdk", "f0.swift"])
        self.assertEqual(args, ["-Xcc", "--sysroot=/tmp/sdk", "f0.swift"])

    def test_recorder_reads_framed_messages_and_passes_text_through(self):
        stream = io.BytesIO(
            b"warning: something\n"
            + framed({"kind": "began", "name": "compile", "pid": 5})
            + b"\n"
            + framed({"kind": "finished", "name": "compile", "pid": 5, "exit-status": 0})
        )
        messages: list[dict] = []
        text: list[str] = []
        recorder.read_messages(stream, messages.append, text.append)
        self.assertEqual([m["kind"] for m in messages], ["began", "finished"])
        self.assertIn("warning: something\n", text)

    def test_recorder_first_output_reads_the_path(self):
        self.assertEqual(
            recorder.first_output({"outputs": [{"path": "/a/b.pcm", "type": "pcm"}]}), "/a/b.pcm"
        )
        self.assertIsNone(recorder.first_output({}))


if __name__ == "__main__":
    unittest.main()
