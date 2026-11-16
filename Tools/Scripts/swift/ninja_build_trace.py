#!/usr/bin/env python3
"""Merge Swift compiler timing data with a `.ninja_log` into a single
Chromium/catapult trace, so you can see when a ninja build oversubscribed the
machine.

Why this exists: CMake's Ninja generator emits *one* ninja edge per Swift target
and passes it `-j <cores> -num-threads <cores>`, so a single ninja lane can fan
out into ~`cores` `swift-frontend` processes. Ninja thinks it is running one job;
the machine sees N. `.ninja_log` alone cannot show that.

Two sources fill it in, and either or both can be used:

  * `-stats-output-dir` -- every frontend process writes a JSON whose *filename*
    carries its start time in epoch microseconds and whose contents carry its
    wall duration, counters, and per-phase timers;
  * a `swiftc_job_recorder.py` log -- adds the jobs that write no stats at all,
    above all the `-explicit-module-build` dependency compiles, and pins the
    clocks together exactly.

In WebKit, configure cmake with `-DSWIFT_NINJA_TRACE=ON` to collect the data
automatically. Or do it manually:

    mkdir -p /tmp/swiftstats                    # the dir must already exist
    cmake -B build -G Ninja \
        -DCMAKE_Swift_COMPILER="swiftc_job_recorder.py" \
        -DCMAKE_Swift_FLAGS="--original-swift-compiler=swiftc --jobs-log=/tmp/swift-jobs.jsonl -stats-output-dir /tmp/swiftstats" \
        ...
    ninja -C build
    ninja_build_trace.py --ninja-log build/.ninja_log --stats-dir /tmp/swiftstats \
        --jobs-log /tmp/swift-jobs.jsonl --relative -o trace.json

Then drag trace.json onto https://ui.perfetto.dev.
"""

from __future__ import annotations

import argparse
import collections
import gzip
import json
import os
import re
import statistics
import sys
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

# ---------------------------------------------------------------------------
# .ninja_log
#
# A header line followed by tab-separated records:
#
#     start_ms <TAB> end_ms <TAB> mtime_ns <TAB> output <TAB> cmdhash
#
# start_ms/end_ms are relative to the start of *that* ninja invocation, and the
# log accumulates across invocations. The third column is named "mtime" by the
# format, but ninja writes the edge's command *start* time there (build.cc,
# FinishCommand); only a restat or generator edge whose output changed gets that
# output's real mtime.
#
# A multi-output edge writes one record per output, all sharing start/end/hash,
# so records must be folded back into edges.
# ---------------------------------------------------------------------------


@dataclass
class NinjaEdge:
    start_ms: int
    end_ms: int
    mtime_ns: int
    cmdhash: str
    outputs: list[str] = field(default_factory=list)

    @property
    def dur_ms(self) -> int:
        return self.end_ms - self.start_ms

    @property
    def anchor_ms(self) -> float:
        """Epoch ms at which this edge's ninja invocation started."""
        return self.mtime_ns / 1e6 - self.start_ms

    @property
    def anchor_bounds_ms(self) -> tuple[float, float]:
        """Range of invocation starts this edge's logged time is consistent with.

        An ordinary edge logs its command start time, pinning the anchor at
        `logged - start_ms`; a restat edge logs an output mtime from somewhere
        inside the edge, so the anchor is no earlier than `logged - end_ms`.
        Nothing in the log says which kind an edge is.
        """
        logged_ms = self.mtime_ns / 1e6
        return (logged_ms - self.end_ms, logged_ms - self.start_ms)

    @property
    def label(self) -> str:
        if not self.outputs:
            return "(no output)"
        return os.path.basename(min(self.outputs, key=len))


def parse_ninja_log(path: Path) -> list[NinjaEdge]:
    """Parse a .ninja_log, folding multi-output records into single edges."""
    edges: dict[tuple[int, int, str], NinjaEdge] = {}
    order: list[NinjaEdge] = []
    for line in path.read_text(errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        try:
            start_ms, end_ms, mtime_ns = int(parts[0]), int(parts[1]), int(parts[2])
        except ValueError:
            continue
        output, cmdhash = parts[3], parts[4]
        key = (start_ms, end_ms, cmdhash)
        edge = edges.get(key)
        if edge is None:
            edge = NinjaEdge(start_ms, end_ms, mtime_ns, cmdhash)
            edges[key] = edge
            order.append(edge)
        edge.outputs.append(output)
        # Only matters for a restat edge, where the column is a real output mtime.
        edge.mtime_ns = max(edge.mtime_ns, mtime_ns)
    return order


@dataclass
class NinjaRun:
    anchor_ms: float
    edges: list[NinjaEdge]

    @property
    def spread_ms(self) -> float:
        """How tightly this run's edges agree on when it started.

        Trimmed: a restat edge logs an output mtime and so reads up to its own
        duration late, which would make a consistent run look minutes wide.
        """
        anchors = sorted(e.anchor_ms for e in self.edges if e.mtime_ns > 0)
        if len(anchors) < 2:
            return 0.0
        if len(anchors) < 100:
            return anchors[-1] - anchors[0]
        trim = len(anchors) // 100
        return anchors[-1 - trim] - anchors[trim]

    @property
    def window_ms(self) -> tuple[int, int]:
        return (min(e.start_ms for e in self.edges), max(e.end_ms for e in self.edges))


def group_runs(
    edges: list[NinjaEdge], tol_ms: float = 5_000.0
) -> tuple[list[NinjaRun], list[NinjaEdge]]:
    """Split edges into ninja invocations by clustering their epoch anchors.

    Clusters on each edge's feasible anchor *range*, so a restat edge -- whose
    point estimate reads up to its own duration late -- stays with the invocation
    it belongs to. The anchor is the median of the point estimates, which the
    ordinary edges dominate.

    Returns the runs plus the edges that carry no mtime and so cannot be placed on
    an absolute timeline at all.
    """
    dated = sorted(
        (e for e in edges if e.mtime_ns > 0), key=lambda e: e.anchor_bounds_ms[0]
    )
    undated = [e for e in edges if e.mtime_ns <= 0]
    clusters: list[list[NinjaEdge]] = []
    reach = 0.0
    for edge in dated:
        lo, hi = edge.anchor_bounds_ms
        if clusters and lo - tol_ms <= reach:
            clusters[-1].append(edge)
            reach = max(reach, hi)
        else:
            clusters.append([edge])
            reach = hi

    runs = [
        NinjaRun(statistics.median([e.anchor_ms for e in c]), list(c)) for c in clusters
    ]
    runs.sort(key=lambda r: r.anchor_ms)
    for run in runs:
        run.edges.sort(key=lambda e: e.start_ms)
    return runs, undated


# ---------------------------------------------------------------------------
# -stats-output-dir
#
# Filenames look like
#   stats-<epoch_usec>-swift-<kind>-<module>-<input>-<triple>-<out>-<opt>-<rand>.json
# and the JSON contains LLVM timers keyed
#   time.swift-<kind>.<module>-<input>-<triple>-<out>-<opt>.{wall,user,sys}
# The `.wall` timer of the process's own region is the job's duration.
#
# Regexes vendored from swift's utils/jobstats/jobstats.py so this script has no
# dependency on a swift checkout. Every aux field is hyphen-free: swift's
# cleanName() (lib/Basic/Statistic.cpp) rewrites anything but [A-Za-z0-9.] to '_'
# before building these names.
# ---------------------------------------------------------------------------

_AUX_RE = (
    r"(?P<module>[^-]+)-(?P<input>[^-]+)-(?P<triple>[^-]+)"
    r"-(?P<out>[^-]*)-(?P<opt>[^-]+)"
)
STATS_FILE_RE = re.compile(
    r"^stats-(?P<start>\d+)-swift-(?P<kind>\w+)-" + _AUX_RE + r"-(?P<rand>\d+)(-.*)?\.json$"
)
TIMER_RE = re.compile(r"^time\.swift-(?P<kind>\w+)\." + _AUX_RE + r"\.(?P<timerkind>\w+)$")

# Present only in a dependency-scan job's stats, which identifies it.
DEP_SCAN_COUNTER = "AST.NumDepScanFilesystemLookups"

# Because of that rewriting a stats file's `input` is not the source's real
# basename -- "Foundation+Extras.swift" arrives as "Foundation_Extras.swift" --
# and the mapping cannot be inverted, so comparisons put both sides through it.
_UNCLEAN_RE = re.compile(r"[^A-Za-z0-9.]")


def clean_name(name: str) -> str:
    """Sanitize a real filename the way swift sanitizes its stats filenames."""
    return _UNCLEAN_RE.sub("_", name)


# Counters worth surfacing in the trace viewer's args pane.
INTERESTING_COUNTERS = (
    "Frontend.WallClockMicroseconds",
    "Frontend.NumInstructionsExecuted",
    "Frontend.MaxMallocUsage",
    "Frontend.NumProcessFailures",
    "AST.NumSourceLines",
    "AST.NumSourceLinesPerSecond",
    "AST.NumLoadedModules",
)


@dataclass
class FrontendJob:
    start_us: int  # epoch microseconds
    dur_us: int
    kind: str
    module: str
    input: str
    triple: str
    out: str
    opt: str
    stats: dict
    path: Path
    edge: NinjaEdge | None = None

    @property
    def end_us(self) -> int:
        return self.start_us + self.dur_us

    @property
    def label(self) -> str:
        # Whole-module jobs (e.g. emit-module) record their input as "all".
        if self.input == "all":
            if DEP_SCAN_COUNTER in self.stats:
                # Scanned in-process, so it never appears in -parseable-output.
                return f"{self.module} (dependency scan)"
            return f"{self.module} ({self.out})" if self.out else self.module
        return self.input


def load_stats_dir(paths: list[Path], verbose: bool = False) -> list[FrontendJob]:
    """Load every stats-*.json under the given directories."""
    jobs: list[FrontendJob] = []
    missing_timer = 0
    for root_path in paths:
        for root, _dirs, files in os.walk(root_path):
            for name in sorted(files):
                match = STATS_FILE_RE.match(name)
                if not match:
                    continue
                full = Path(root) / name
                try:
                    stats = json.loads(full.read_text())
                except (OSError, ValueError) as exc:
                    print(f"warning: skipping {full}: {exc}", file=sys.stderr)
                    continue
                kind = match.group("kind")
                dur_us = _find_wall_usec(stats, kind)
                if dur_us is None:
                    missing_timer += 1
                    dur_us = 1
                jobs.append(
                    FrontendJob(
                        start_us=int(match.group("start")),
                        dur_us=dur_us,
                        kind=kind,
                        module=match.group("module"),
                        input=match.group("input"),
                        triple=match.group("triple"),
                        out=match.group("out"),
                        opt=match.group("opt"),
                        stats=stats,
                        path=full,
                    )
                )
    if missing_timer:
        print(
            f"warning: {missing_timer} stats file(s) had no whole-process .wall "
            "timer; those jobs get a 1us duration",
            file=sys.stderr,
        )
    jobs.sort(key=lambda j: j.start_us)
    return jobs


def _find_wall_usec(stats: dict, kind: str) -> int | None:
    """Pull the process's own wall-clock timer out of a stats dict."""
    for key, value in stats.items():
        match = TIMER_RE.match(key)
        if match and match.group("kind") == kind and match.group("timerkind") == "wall":
            return int(float(value) * 1e6)  # timers are in seconds
    return None


# ---------------------------------------------------------------------------
# Recorded driver jobs (see swiftc_job_recorder.py)
#
# `-stats-output-dir` only covers frontend jobs that inherit the driver's
# frontend flags. Under `-explicit-module-build` the driver synthesizes the
# module-dependency compiles from the scan result, so they write no stats file and
# appear only as a gap. The recorder captures them from `-parseable-output`, one
# JSON line per began/finished message, timestamped on arrival.
# ---------------------------------------------------------------------------

# Cache entries are named "<ModuleName>-<hash>.pcm" / ".swiftmodule" / ".pch",
# so the hash suffix is stripped to leave a readable label.
HASHED_OUTPUT_JOBS = {
    "generate-pcm": "pcm",
    "compile-module-from-interface": "interface",
    "generate-pch": "pch",
}

# Work the driver must finish before it can compile any of your sources.
DEPENDENCY_JOBS = frozenset(HASHED_OUTPUT_JOBS)


@dataclass
class RecordedJob:
    name: str
    start_us: float
    end_us: float
    output: str | None
    inputs: list[str]
    exit_status: int | None
    unfinished: bool = False
    batch: str | None = None  # set once several primaries share one process

    @property
    def dur_us(self) -> float:
        return self.end_us - self.start_us

    @property
    def label(self) -> str:
        kind = HASHED_OUTPUT_JOBS.get(self.name)
        if kind:
            base = os.path.basename(self.output or "?")
            module = os.path.splitext(base)[0].rsplit("-", 1)[0]
            return f"{module} ({kind})"
        if self.inputs:
            first = os.path.basename(self.inputs[0])
            if self.batch is not None and len(self.inputs) > 1:
                return f"{first} +{len(self.inputs) - 1}"
            return first
        if self.output:
            base = os.path.basename(self.output)
            for suffix in (".o", ".obj"):
                if base.endswith(suffix):
                    return base[: -len(suffix)]  # "f0.swift.o" -> "f0.swift"
            return base
        return self.name

    @property
    def builds_a_dependency(self) -> bool:
        return self.name in DEPENDENCY_JOBS


@dataclass
class DriverInvocation:
    rec: int
    cwd: str
    start_us: float
    end_us: float
    jobs: list[RecordedJob] = field(default_factory=list)
    edge: NinjaEdge | None = None

    def outputs(self) -> set[str]:
        return {
            os.path.normpath(os.path.join(self.cwd, j.output))
            for j in self.jobs
            if j.output
        }


def load_jobs_log(path: Path) -> list[DriverInvocation]:
    """Read a recorder log into one DriverInvocation per swiftc invocation."""
    invocations: dict[int, DriverInvocation] = {}
    pending: dict[tuple[int, int], dict] = {}
    last_seen: dict[int, float] = {}
    for line in path.read_text(errors="replace").splitlines():
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue  # a torn line from concurrent appends
        rec, kind, when = row.get("rec"), row.get("kind"), row.get("t")
        if rec is None or when is None:
            continue
        stamp = float(when) * 1e6
        last_seen[rec] = max(last_seen.get(rec, stamp), stamp)
        if kind == "driver-began":
            invocations[rec] = DriverInvocation(
                rec=rec, cwd=row.get("cwd", ""), start_us=stamp, end_us=stamp
            )
        elif kind == "driver-finished":
            if rec in invocations:
                invocations[rec].end_us = stamp
        elif kind == "began":
            pending[(rec, row.get("pid"))] = row | {"start_us": stamp}
        elif kind == "finished":
            start = pending.pop((rec, row.get("pid")), None)
            if start is None or rec not in invocations:
                continue
            invocations[rec].jobs.append(_recorded_job(start, stamp, row.get("exit_status")))

    # Anything still pending was running when the recorder died. Those jobs did
    # consume the machine, so keep them, ending at that recorder's last message.
    for (rec, _pid), start in pending.items():
        if rec in invocations:
            invocations[rec].jobs.append(
                _recorded_job(start, last_seen[rec], None, unfinished=True)
            )

    result = [inv for inv in invocations.values() if inv.jobs]
    for inv in result:
        inv.jobs = collapse_batches(inv.jobs)
        inv.jobs.sort(key=lambda j: j.start_us)
        # A killed recorder never writes driver-finished; its jobs know better.
        inv.start_us = min(inv.start_us, inv.jobs[0].start_us)
        inv.end_us = max(inv.end_us, max(j.end_us for j in inv.jobs))
    result.sort(key=lambda i: i.start_us)
    return result


def collapse_batches(jobs: list[RecordedJob]) -> list[RecordedJob]:
    """Fold each batch's constituent jobs back into the one process that ran them.

    The driver reports every primary of a batch separately, each spanning the
    whole process, so counting them individually multiplies both the concurrency
    and the total frontend time by the batch size.
    """
    batched: dict[str, list[RecordedJob]] = {}
    result = []
    for job in jobs:
        if job.batch is None:
            result.append(job)
        else:
            batched.setdefault(job.batch, []).append(job)

    for key, members in batched.items():
        # The batch key is its first primary, and that job's output is the one a
        # stats file can be matched against -- swift names the file after it too.
        lead = next(
            (m for m in members if m.inputs and m.inputs[0] == key), members[0]
        )
        rest = [m for m in members if m is not lead]
        result.append(
            RecordedJob(
                name=lead.name,
                start_us=min(m.start_us for m in members),
                end_us=max(m.end_us for m in members),
                output=lead.output,
                inputs=lead.inputs + [i for m in rest for i in m.inputs],
                exit_status=next(
                    (m.exit_status for m in members if m.exit_status),
                    lead.exit_status,
                ),
                unfinished=any(m.unfinished for m in members),
                batch=key,
            )
        )
    return result


def _recorded_job(
    began: dict, end_us: float, exit_status: int | None, unfinished: bool = False
) -> RecordedJob:
    return RecordedJob(
        name=began.get("name") or "job",
        start_us=began["start_us"],
        end_us=end_us,
        output=began.get("output"),
        inputs=[
            i.get("path", "") if isinstance(i, dict) else str(i)
            for i in (began.get("inputs") or [])
        ],
        exit_status=exit_status,
        unfinished=unfinished,
        batch=began.get("batch"),
    )


def attribute_invocations(
    invocations: list[DriverInvocation],
    edges: list[NinjaEdge],
    anchor_ms: float,
    slack_ms: float = 2_000.0,
) -> int:
    """Bind each recorded swiftc invocation to its ninja edge.

    The recorder runs in ninja's working directory and the driver reports absolute
    output paths, so an edge's outputs resolve to exactly the same paths -- no need
    for the name heuristics that stats files require.
    """
    unmatched = 0
    for inv in invocations:
        produced = inv.outputs()
        exact = [
            e
            for e in edges
            if produced
            & {os.path.normpath(os.path.join(inv.cwd, out)) for out in e.outputs}
        ]
        if exact:
            inv.edge = min(exact, key=lambda e: e.dur_ms)
            continue
        start_ms = inv.start_us / 1000.0 - anchor_ms
        end_ms = inv.end_us / 1000.0 - anchor_ms
        containing = [
            e
            for e in edges
            if e.start_ms - slack_ms <= start_ms and end_ms <= e.end_ms + slack_ms
            and is_swift_edge(e)
        ]
        if containing:
            inv.edge = min(containing, key=lambda e: e.dur_ms)
        else:
            unmatched += 1
    return unmatched


def anchor_from_invocations(
    invocations: list[DriverInvocation], edges: list[NinjaEdge], tol_ms: float = 5_000.0
) -> tuple[float | None, int]:
    """Derive the build-start anchor from recorded swiftc invocations.

    This needs no mtimes and no prior anchor: an invocation's output paths match an
    edge's outputs exactly and its start time is absolute, so
    `inv.start - edge.start_ms` *is* the anchor -- exact, where the logged command
    start times agree only to a few tens of ms.

    A recorder log accumulates across builds and output paths repeat between them,
    so take the largest cluster of agreeing offsets rather than the median of all
    of them, which would land between two builds and belong to neither.
    """
    offsets: list[float] = []
    for inv in invocations:
        produced = inv.outputs()
        if not produced:
            continue
        for edge in edges:
            resolved = {
                os.path.normpath(os.path.join(inv.cwd, out)) for out in edge.outputs
            }
            if produced & resolved:
                offsets.append(inv.start_us / 1000.0 - edge.start_ms)
                break
    if not offsets:
        return None, 0

    offsets.sort()
    best_start, best_count = 0, 0
    end = 0
    for start in range(len(offsets)):
        end = max(end, start)
        while end + 1 < len(offsets) and offsets[end + 1] - offsets[start] <= tol_ms:
            end += 1
        if end - start + 1 > best_count:
            best_start, best_count = start, end - start + 1
    cluster = offsets[best_start:best_start + best_count]
    return statistics.median(cluster), len(cluster)


# ---------------------------------------------------------------------------
# Aligning the two clocks
# ---------------------------------------------------------------------------


def refine_anchor(
    anchor_ms: float,
    edges: list[NinjaEdge],
    jobs: list[FrontendJob],
    window_ms: float = 30_000.0,
    max_pairs: int = 5_000_000,
) -> tuple[float, int]:
    """Nudge the anchor to maximize how many frontend jobs land inside an edge.

    For a job [js, je] (epoch ms) and edge [start, end] (ninja-relative ms), the
    job is contained in the edge exactly when the anchor A satisfies
        je - end <= A <= js - start
    so each (job, edge) pair contributes one feasible interval of anchors. The
    best anchor is the point covered by the most *jobs*, found with a sweep.

    The search window stays wide because a ninja old enough to log real output
    mtimes puts the starting estimate a whole edge duration out.
    """
    if not jobs or not edges:
        return anchor_ms, 0

    stride = 1
    while len(jobs) * len(edges) // stride > max_pairs:
        stride += 1
    sampled = jobs[::stride]

    lo_bound, hi_bound = anchor_ms - window_ms, anchor_ms + window_ms
    events: list[tuple[float, int]] = []
    for job in sampled:
        js, je = job.start_us / 1000.0, job.end_us / 1000.0
        spans: list[tuple[float, float]] = []
        for edge in edges:
            lo, hi = je - edge.end_ms, js - edge.start_ms
            if lo > hi or hi < lo_bound or lo > hi_bound:
                continue
            spans.append((max(lo, lo_bound), min(hi, hi_bound)))
        # Merge this job's intervals so it can never be counted twice.
        for lo, hi in _merge_spans(spans):
            events.append((lo, 1))
            events.append((hi, -1))

    if not events:
        return anchor_ms, 0

    events.sort()
    best_count, running = 0, 0
    best_spans: list[tuple[float, float]] = []
    prev = events[0][0]
    for point, delta in events:
        if point != prev:
            if running > best_count:
                best_count, best_spans = running, [(prev, point)]
            elif running == best_count and running > 0:
                best_spans.append((prev, point))
            prev = point
        running += delta
    if not best_spans:
        return anchor_ms, 0
    widest = max(best_spans, key=lambda s: s[1] - s[0])
    return (widest[0] + widest[1]) / 2.0, best_count


def _merge_spans(spans: list[tuple[float, float]]) -> list[tuple[float, float]]:
    merged: list[tuple[float, float]] = []
    for lo, hi in sorted(spans):
        if merged and lo <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], hi))
        else:
            merged.append((lo, hi))
    return merged


SWIFT_OUTPUT_SUFFIXES = (
    ".swiftmodule",
    ".swift.o",
    ".swift.obj",
    ".swiftdoc",
    ".swiftinterface",
    ".swiftsourceinfo",
    "-Swift.h",
)


def is_swift_edge(edge: NinjaEdge) -> bool:
    """Whether an edge looks like a Swift compile rather than a C/C++ one."""
    return any(
        out.endswith(SWIFT_OUTPUT_SUFFIXES) or "-Swift-Generated.h" in out
        for out in edge.outputs
    )


def _match_score(job: FrontendJob, edge: NinjaEdge) -> int:
    """How strongly an edge's outputs identify it as this job's parent.

    Substring-matching the module name against the whole output path is far too
    loose: in a WebKit tree nearly every path contains "WebKit". The per-source
    object name is exact but not unique either, since one source can be compiled
    into several targets, so the target's object directory -- which carries the
    module name -- has to agree as well. Both sides go through clean_name.
    """
    basenames = {clean_name(os.path.basename(out)) for out in edge.outputs}
    in_module_dir = any(
        f"{job.module}.dir" in map(clean_name, out.split("/")) for out in edge.outputs
    )
    builds_input = f"{job.input}.o" in basenames or f"{job.input}.obj" in basenames

    if builds_input and in_module_dir:
        return 5
    if builds_input:
        return 4
    if f"{job.module}.swiftmodule" in basenames:
        return 3
    if not is_swift_edge(edge):
        # A target's object dir holds its clang objects too, so "<module>.dir"
        # alone proves nothing about the language.
        return 0
    if in_module_dir:
        return 2
    return 1


def attribute(
    jobs: list[FrontendJob],
    edges: list[NinjaEdge],
    anchor_ms: float,
    slack_ms: float = 250.0,
) -> int:
    """Assign each frontend job to the ninja edge that spawned it."""
    by_start = sorted(edges, key=lambda e: e.start_ms)
    unattributed = 0
    for job in jobs:
        js, je = job.start_us / 1000.0 - anchor_ms, job.end_us / 1000.0 - anchor_ms
        candidates = [
            e
            for e in by_start
            if e.start_ms - slack_ms <= js and je <= e.end_ms + slack_ms
        ]
        scored = [(_match_score(job, e), e) for e in candidates]
        best = max((s for s, _ in scored), default=0)
        if best == 0:
            # Claiming an unrelated clang edge would be worse than saying so.
            unattributed += 1
            continue
        job.edge = min((e for s, e in scored if s == best), key=lambda e: e.dur_ms)
    return unattributed


def within_run(
    start_us: float,
    end_us: float,
    run: NinjaRun,
    anchor_ms: float,
    slack_ms: float = 2_000.0,
) -> bool:
    """Whether an absolute time span falls inside this ninja invocation."""
    lo, hi = run.window_ms
    return (
        (anchor_ms + lo) * 1000.0 - slack_ms * 1000.0 <= start_us
        and end_us <= (anchor_ms + hi) * 1000.0 + slack_ms * 1000.0
    )


def partition_within_run(
    items: list,
    run: NinjaRun,
    anchor_ms: float,
    slack_ms: float = 2_000.0,
) -> tuple[list, list]:
    """Split anything with start_us/end_us into inside/outside this ninja run.

    Both inputs accumulate across builds -- a stats directory keeps CMake's
    configure-time try-compiles, a recorder log is appended to -- and left in they
    show up as spikes in the concurrency track long before this build started.
    """
    inside: list = []
    outside: list = []
    for item in items:
        keep = within_run(item.start_us, item.end_us, run, anchor_ms, slack_ms)
        (inside if keep else outside).append(item)
    return inside, outside


# ---------------------------------------------------------------------------
# Trace emission
# ---------------------------------------------------------------------------

PID_COUNTERS = 0
PID_NINJA = 1
PID_SWIFT_BASE = 10  # leaves room to add fixed rows without renumbering


@dataclass
class Span:
    """One frontend process to draw in a swiftc row."""

    start_us: float
    dur_us: float
    name: str
    cat: str
    args: dict

    @property
    def end_us(self) -> float:
        return self.start_us + self.dur_us


def _stats_args(job: FrontendJob) -> dict:
    args = {k: job.stats[k] for k in INTERESTING_COUNTERS if k in job.stats}
    args.update({k: v for k, v in job.stats.items() if k.startswith("time.")})
    args["module"] = job.module
    args["opt"] = job.opt
    args["output_kind"] = job.out
    args["stats_file"] = job.path.name
    return args


def _output_names(job: FrontendJob) -> set[str]:
    """Output basenames a stats job could have produced, for merging with the
    recorder's view of the same process."""
    stem = os.path.splitext(job.input)[0]
    names = {f"{job.input}.o", f"{stem}.o", f"{job.input}.obj", f"{stem}.obj"}
    if job.input == "all" and job.out:
        names.add(f"{job.module}.{job.out}")  # e.g. "<Module>.swiftmodule"
    return names


def spans_for_edge(
    stats_jobs: list[FrontendJob], invocation: DriverInvocation | None
) -> list[Span]:
    """Build a row's spans, preferring recorded jobs and enriching them with stats.

    The recorder sees every spawned frontend job, including the module builds that
    write no stats; the stats files carry the counters, the phase timers, and the
    in-process dependency scan, which is never a spawned job. Merging on the output
    basename keeps each process represented exactly once.
    """
    if invocation is None:
        return [
            Span(float(j.start_us), float(j.dur_us), j.label, j.kind, _stats_args(j))
            for j in stats_jobs
        ]

    by_output: dict[str, FrontendJob] = {}
    for job in stats_jobs:
        for name in _output_names(job):
            by_output.setdefault(name, job)

    spans: list[Span] = []
    claimed: set[int] = set()
    for recorded in invocation.jobs:
        args: dict = {"job": recorded.name, "exit_status": recorded.exit_status}
        if recorded.unfinished:
            args["unfinished"] = True
        if recorded.output:
            args["output"] = recorded.output
        if recorded.batch is not None and len(recorded.inputs) > 1:
            args["batch_inputs"] = [os.path.basename(i) for i in recorded.inputs]
        stats_job = by_output.get(clean_name(os.path.basename(recorded.output or "")))
        name = recorded.label
        if stats_job is not None and id(stats_job) not in claimed:
            claimed.add(id(stats_job))
            args.update(_stats_args(stats_job))
            if stats_job.input == "all":
                # Only a whole-module job's stats label says more than its output
                # path does; elsewhere the recorder has the unsanitized filename.
                name = stats_job.label
        spans.append(
            Span(recorded.start_us, recorded.dur_us, name, recorded.name, args)
        )

    # Whatever the recorder could not see, above all the dependency scan.
    for job in stats_jobs:
        if id(job) not in claimed:
            spans.append(
                Span(float(job.start_us), float(job.dur_us), job.label, job.kind,
                     _stats_args(job))
            )
    spans.sort(key=lambda s: s.start_us)
    return spans


def pack_lanes(spans: Sequence[tuple[float, float]]) -> list[int]:
    """Greedy lane assignment: lowest lane whose previous span already ended."""
    lane_ends: list[float] = []
    lanes = [0] * len(spans)
    for index in sorted(range(len(spans)), key=lambda i: spans[i][0]):
        start, end = spans[index]
        for lane, lane_end in enumerate(lane_ends):
            if lane_end <= start:
                lane_ends[lane] = end
                lanes[index] = lane
                break
        else:
            lane_ends.append(end)
            lanes[index] = len(lane_ends) - 1
    return lanes


class TidAllocator:
    """Hands out thread ids that are unique across the whole trace.

    Perfetto resolves a slice's thread track from its `tid` alone, and gives every
    process a main thread whose tid equals its pid. So lane numbers must be unique
    across rows *and* disjoint from every pid, or unrelated slices share a track,
    fail to nest, and get spilled onto an overflow track.
    """

    def __init__(self, base: int = 1) -> None:
        self._next = base

    def assign(self, pid: int, lanes: list[int], events: list[dict]) -> dict[int, int]:
        mapping: dict[int, int] = {}
        for lane in sorted(set(lanes)):
            tid = self._next
            self._next += 1
            mapping[lane] = tid
            events.append(
                {
                    "ph": "M",
                    "pid": pid,
                    "tid": tid,
                    "name": "thread_name",
                    "args": {"name": f"lane {lane}"},
                }
            )
            events.append(
                {
                    "ph": "M",
                    "pid": pid,
                    "tid": tid,
                    "name": "thread_sort_index",
                    "args": {"sort_index": lane},
                }
            )
        return mapping


@dataclass
class Row:
    """One process row of the trace: a title and the spans drawn on it."""

    name: str
    kind: str  # "ninja" or "swiftc"
    spans: list[Span]


def _edge_span(edge: NinjaEdge, anchor_ms: float) -> Span:
    return Span(
        start_us=(anchor_ms + edge.start_ms) * 1000.0,
        dur_us=edge.dur_ms * 1000.0,
        name=edge.label,
        cat="ninja",
        args={
            "outputs": edge.outputs,
            "cmdhash": edge.cmdhash,
            "duration_ms": edge.dur_ms,
        },
    )


def build_rows(
    run: NinjaRun,
    jobs: list[FrontendJob],
    anchor_ms: float,
    invocations: list[DriverInvocation] | None = None,
) -> list[Row]:
    """Lay the trace out as rows: the ninja edges, then one row per swiftc run."""
    rows = [Row("ninja", "ninja", [_edge_span(e, anchor_ms) for e in run.edges])]

    grouped: dict[int, list[FrontendJob]] = {}
    for job in jobs:
        grouped.setdefault(id(job.edge) if job.edge is not None else 0, []).append(job)
    inv_by_edge = {
        id(inv.edge): inv for inv in (invocations or []) if inv.edge is not None
    }
    for key in inv_by_edge:
        grouped.setdefault(key, [])

    edge_by_key = {id(e): e for e in run.edges}
    keys = [id(e) for e in run.edges if id(e) in grouped]
    if 0 in grouped:
        keys.append(0)

    # A module can be built by several edges (compile, emit-module, generated
    # header), so name-collided rows get their edge label to tell them apart.
    titles: dict[int, str] = {}
    for key in keys:
        modules = sorted({j.module for j in grouped[key]})
        if not modules and key in edge_by_key:
            modules = [edge_by_key[key].label]
        titles[key] = "+".join(modules)
    collisions = collections.Counter(titles.values())

    for key in keys:
        if key == 0:
            name = "swiftc (unattributed)"
        else:
            name = f"swiftc {titles[key]}"
            if collisions[titles[key]] > 1:
                name += f" [{edge_by_key[key].label}]"
        spans = spans_for_edge(grouped[key], inv_by_edge.get(key))
        if key in edge_by_key:
            for span in spans:
                span.args["ninja_edge"] = edge_by_key[key].label
        rows.append(Row(name, "swiftc", spans))

    for inv in invocations or []:
        if inv.edge is None:
            rows.append(
                Row(f"swiftc (unmatched pid {inv.rec})", "swiftc", spans_for_edge([], inv))
            )
    return rows


def process_spans(rows: list[Row]) -> list[Span]:
    """Every span that represents a real process, i.e. everything but ninja edges."""
    return [span for row in rows if row.kind == "swiftc" for span in row.spans]


def _whole_microseconds(span: Span, origin_us: float) -> tuple[int, int]:
    """A span's bounds as whole microseconds, rounded together.

    Epoch microseconds land near 1.8e15, where a float64 steps by a quarter of a
    microsecond, so a slice's start and the previous slice's start-plus-duration
    need not be the same float. Perfetto converts the two to integer nanoseconds
    separately and reads the residue as an overlap that cannot nest. Rounding both
    ends here, and deriving the duration from them, keeps the boundary exact.
    """
    return round(span.start_us - origin_us), round(span.end_us - origin_us)


def emit_trace(
    rows: list[Row],
    run: NinjaRun,
    spawning: set[int],
    anchor_ms: float,
    cores: int,
    relative: bool = False,
) -> dict:
    """Render rows as a Chromium/catapult trace object."""
    origin_us = anchor_ms * 1000.0 if relative else 0.0

    pids: list[int] = []
    swiftc_rows = 0
    for row in rows:
        if row.kind == "ninja":
            pids.append(PID_NINJA)
        else:
            pids.append(PID_SWIFT_BASE + swiftc_rows)
            swiftc_rows += 1

    events = list(_metadata(PID_COUNTERS, "concurrency", 0))
    tids = TidAllocator(base=max([PID_COUNTERS, *pids], default=0) + 1)

    for row, pid in zip(rows, pids):
        events.extend(_metadata(pid, row.name, pid))
        bounds = [_whole_microseconds(s, origin_us) for s in row.spans]
        lanes = pack_lanes(bounds)
        row_tids = tids.assign(pid, lanes, events)
        for span, lane, (start, end) in zip(row.spans, lanes, bounds):
            events.append(
                {
                    "ph": "X",
                    "pid": pid,
                    "tid": row_tids[lane],
                    "ts": start,
                    "dur": end - start,
                    "name": span.name,
                    "cat": span.cat,
                    "args": span.args,
                }
            )

    drawn = process_spans(rows)
    events.extend(_counter_events(run, drawn, spawning, anchor_ms, cores, origin_us))

    return {
        "traceEvents": events,
        "displayTimeUnit": "ms",
        "otherData": {
            "anchor_epoch_ms": f"{anchor_ms:.3f}",
            "anchor_spread_ms": f"{run.spread_ms:.3f}",
            "cores": str(cores),
            "ninja_edges": str(len(run.edges)),
            "swift_frontend_jobs": str(len(drawn)),
            "timestamps": "relative to build start" if relative else "epoch microseconds",
        },
    }


def build_trace(
    run: NinjaRun,
    jobs: list[FrontendJob],
    anchor_ms: float,
    cores: int,
    relative: bool = False,
    invocations: list[DriverInvocation] | None = None,
) -> dict:
    """Convenience wrapper: lay out the rows and render them in one step."""
    rows = build_rows(run, jobs, anchor_ms, invocations)
    return emit_trace(
        rows, run, spawning_edge_ids(jobs, invocations), anchor_ms, cores, relative
    )


def validate_trace(trace: dict) -> list[str]:
    """Check the invariants Perfetto's importer cares about.

    It resolves a slice's track from its `tid` alone and gives every process a main
    thread whose tid equals its pid; two slices sharing a track but overlapping
    only partially cannot nest, and get spilled onto an overflow track. So:
      * no value is used as both a pid and a tid;
      * no two slices on one tid overlap;
      * every timestamp is a whole microsecond, since Perfetto rounds to integer
        nanoseconds and a fractional one turns a shared boundary into an overlap.
    """
    problems: list[str] = []
    slices = [e for e in trace["traceEvents"] if e["ph"] == "X"]
    pids = {e["pid"] for e in trace["traceEvents"]}
    tids = {e["tid"] for e in slices}
    clash = sorted(pids & tids)
    if clash:
        problems.append(
            f"{len(clash)} value(s) used as both a pid and a tid: {clash[:5]}"
        )

    fractional = sum(
        1 for e in slices if e["ts"] != int(e["ts"]) or e["dur"] != int(e["dur"])
    )
    if fractional:
        problems.append(
            f"{fractional} slice(s) carry sub-microsecond timestamps, which round "
            "to overlapping integer nanoseconds"
        )

    by_tid: dict[int, list[dict]] = {}
    for event in slices:
        by_tid.setdefault(event["tid"], []).append(event)
    overlaps = 0
    for group in by_tid.values():
        group.sort(key=lambda e: e["ts"])
        for first, second in zip(group, group[1:]):
            if second["ts"] < first["ts"] + first["dur"]:
                overlaps += 1
    if overlaps:
        problems.append(f"{overlaps} pair(s) of overlapping slices share a track")
    return problems


def _metadata(pid: int, name: str, sort_index: int) -> list[dict]:
    return [
        {
            "ph": "M",
            "pid": pid,
            "tid": 0,
            "name": "process_name",
            "args": {"name": name},
        },
        {
            "ph": "M",
            "pid": pid,
            "tid": 0,
            "name": "process_sort_index",
            "args": {"sort_index": sort_index},
        },
    ]


def spawning_edge_ids(
    jobs: list[FrontendJob], invocations: list[DriverInvocation] | None = None
) -> set[int]:
    """Edges whose load is represented by their child processes instead.

    Both stats files and recorded invocations supply those children; missing either
    double counts the edge against its own children.
    """
    ids = {id(j.edge) for j in jobs if j.edge is not None}
    ids |= {id(inv.edge) for inv in (invocations or []) if inv.edge is not None}
    return ids


def _load_transitions(
    run: NinjaRun, drawn: list[Span], spawning: set[int], anchor_ms: float
) -> list[tuple[float, int, int]]:
    """Machine-load deltas as (ts_us, d_swift_frontends, d_other_edges)."""
    transitions: list[tuple[float, int, int]] = []
    for edge in run.edges:
        if id(edge) in spawning:
            continue
        transitions.append((anchor_ms * 1000.0 + edge.start_ms * 1000.0, 0, 1))
        transitions.append((anchor_ms * 1000.0 + edge.end_ms * 1000.0, 0, -1))
    for span in drawn:
        transitions.append((span.start_us, 1, 0))
        transitions.append((span.end_us, -1, 0))
    transitions.sort()
    return transitions


def _counter_events(
    run: NinjaRun,
    drawn: list[Span],
    spawning: set[int],
    anchor_ms: float,
    cores: int,
    origin_us: float,
) -> list[dict]:
    """Emit stacked counter series for machine load and oversubscription."""
    transitions = _load_transitions(run, drawn, spawning, anchor_ms)
    if not transitions:
        return []

    events: list[dict] = []
    swift = other = 0
    index = 0
    first_ts = transitions[0][0]
    events.append(_counter(PID_COUNTERS, round(first_ts - 1 - origin_us), 0, 0, cores))
    while index < len(transitions):
        ts = transitions[index][0]
        while index < len(transitions) and transitions[index][0] == ts:
            _, d_swift, d_other = transitions[index]
            swift += d_swift
            other += d_other
            index += 1
        sample = _counter(PID_COUNTERS, round(ts - origin_us), swift, other, cores)
        # Rounding can collapse two transitions onto one timestamp; the later wins.
        if events[-1]["ts"] == sample["ts"]:
            events[-1] = sample
        else:
            events.append(sample)
    return events


def _counter(pid: int, ts: float, swift: int, other: int, cores: int) -> dict:
    return {
        "ph": "C",
        "pid": pid,
        "tid": 0,
        "ts": ts,
        "name": "running",
        "args": {
            "swift_frontends": swift,
            "other_edges": other,
            "over_cores": max(0, swift + other - cores),
        },
    }


def summarize(
    run: NinjaRun,
    rows: list[Row],
    jobs: list[FrontendJob],
    anchor_ms: float,
    cores: int,
    unattributed: int,
    matched: int,
    invocations: list[DriverInvocation] | None = None,
) -> None:
    """Print the headline numbers to stderr."""
    transitions = _load_transitions(
        run, process_spans(rows), spawning_edge_ids(jobs, invocations), anchor_ms
    )

    peak = running = 0
    over_us = 0.0
    prev_ts = transitions[0][0] if transitions else 0.0
    for ts, d_swift, d_other in transitions:
        if running > cores:
            over_us += (ts - prev_ts) * (running - cores)
        running += d_swift + d_other
        peak = max(peak, running)
        prev_ts = ts

    lo, hi = run.window_ms
    print(
        f"ninja run: {len(run.edges)} edges over {(hi - lo) / 1000.0:.1f}s "
        f"(anchor {anchor_ms / 1000.0:.3f}s epoch, mtime spread "
        f"{run.spread_ms / 1000.0:.3f}s)",
        file=sys.stderr,
    )
    if invocations:
        anchor_note = f"anchor from {matched} recorded invocation(s)"
    else:
        anchor_note = f"{matched} matched during anchor refinement"
    print(
        f"swift jobs: {len(jobs)} loaded, {len(jobs) - unattributed} attributed to "
        f"an edge, {unattributed} unattributed ({anchor_note})",
        file=sys.stderr,
    )
    if invocations:
        dependency_jobs = [
            j for i in invocations for j in i.jobs if j.builds_a_dependency
        ]
        dependency_us = sum(j.dur_us for j in dependency_jobs)
        print(
            f"recorded jobs: {sum(len(i.jobs) for i in invocations)} across "
            f"{len(invocations)} swiftc invocation(s), of which {len(dependency_jobs)} "
            f"built module dependencies ({dependency_us / 1e6:.1f}s of frontend time)",
            file=sys.stderr,
        )
    print(
        f"load: peak {peak} concurrent processes against {cores} cores; "
        f"oversubscription integral {over_us / 1e6:.1f} core-seconds",
        file=sys.stderr,
    )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--ninja-log", required=True, type=Path, help="path to .ninja_log")
    parser.add_argument(
        "--stats-dir",
        type=Path,
        action="append",
        default=[],
        help="a -stats-output-dir directory (repeatable)",
    )
    parser.add_argument(
        "--jobs-log",
        type=Path,
        default=None,
        help="a swiftc_job_recorder.py log; adds the explicit-module-build jobs that "
        "write no stats file",
    )
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        default=None,
        help="output JSON (default stdout); a .gz suffix writes gzip, which Perfetto reads",
    )
    parser.add_argument(
        "--cores", type=int, default=os.cpu_count() or 1, help="core budget for oversubscription"
    )
    parser.add_argument(
        "--anchor-epoch-ms",
        type=float,
        default=None,
        help="epoch ms of the ninja build start; skips anchor inference",
    )
    parser.add_argument(
        "--run",
        choices=("last", "all"),
        default="last",
        help="which ninja invocation in the log to emit (default: last)",
    )
    parser.add_argument(
        "--since-epoch-ms",
        type=float,
        default=None,
        help="ignore ninja invocations, stats files and recorded jobs from before this "
        "epoch time; picks one build out of logs that accumulate across builds",
    )
    parser.add_argument(
        "--relative",
        action="store_true",
        help="zero-base timestamps at build start instead of using epoch time",
    )
    parser.add_argument(
        "--keep-outside-run",
        action="store_true",
        help="keep stats jobs that fall outside the selected ninja run's window",
    )
    parser.add_argument(
        "--keep-undated",
        action="store_true",
        help="include ninja edges that recorded no output mtime (placement is a guess)",
    )
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    edges = parse_ninja_log(args.ninja_log)
    if not edges:
        print(f"error: no usable records in {args.ninja_log}", file=sys.stderr)
        return 1
    runs, undated = group_runs(edges)
    if args.since_epoch_ms is not None:
        recent = [r for r in runs if r.anchor_ms >= args.since_epoch_ms]
        if not recent:
            print(
                f"note: none of the {len(runs)} ninja invocation(s) in {args.ninja_log} "
                "started at or after --since-epoch-ms; nothing to trace",
                file=sys.stderr,
            )
            return 0
        runs = recent
    if not runs:
        if args.anchor_epoch_ms is None:
            print(
                f"error: no record in {args.ninja_log} carries an output mtime, so the "
                "build cannot be placed in time; pass --anchor-epoch-ms",
                file=sys.stderr,
            )
            return 1
        # With an explicit anchor the mtimes are not needed at all.
        runs, undated = [NinjaRun(args.anchor_epoch_ms, sorted(undated, key=lambda e: e.start_ms))], []
    if args.verbose:
        for run in runs:
            lo, hi = run.window_ms
            print(
                f"run anchor={run.anchor_ms / 1000.0:.3f}s edges={len(run.edges)} "
                f"window={lo}..{hi}ms",
                file=sys.stderr,
            )

    jobs = load_stats_dir(args.stats_dir, args.verbose) if args.stats_dir else []
    recorded = load_jobs_log(args.jobs_log) if args.jobs_log else []
    if args.since_epoch_ms is not None:
        since_us = args.since_epoch_ms * 1000.0
        jobs = [j for j in jobs if j.start_us >= since_us]
        recorded = [i for i in recorded if i.start_us >= since_us]

    if args.run == "all":
        run = merge_runs(runs)
    else:
        run = select_run(runs, jobs, recorded)

    if undated:
        # Nothing ties an undated record to an invocation, and guessing by
        # ms-offset overlap silently mixes runs and inflates the concurrency.
        if args.keep_undated:
            lo, hi = run.window_ms
            adopted = [e for e in undated if lo <= e.start_ms and e.end_ms <= hi]
            run.edges.extend(adopted)
            run.edges.sort(key=lambda e: e.start_ms)
            print(
                f"note: adopted {len(adopted)} of {len(undated)} undated edge(s) into "
                "this run; they may belong to a different invocation",
                file=sys.stderr,
            )
        else:
            print(
                f"note: skipped {len(undated)} edge(s) with no output mtime, which "
                "cannot be dated; pass --keep-undated to include them",
                file=sys.stderr,
            )

    anchor_ms = args.anchor_epoch_ms if args.anchor_epoch_ms is not None else run.anchor_ms
    matched = 0
    if args.anchor_epoch_ms is None:
        recorded_anchor, n_matched = anchor_from_invocations(recorded, run.edges)
        if recorded_anchor is not None and anchor_is_plausible(recorded_anchor, run):
            drift = recorded_anchor - anchor_ms
            if abs(drift) > 1000.0:
                print(
                    f"note: the mtime-derived anchor is {drift / 1000.0:+.1f}s off what "
                    f"{n_matched} recorded invocation(s) say; trusting the recording "
                    "(a restat edge logs an output mtime where others log a "
                    "command start time)",
                    file=sys.stderr,
                )
            anchor_ms, matched = recorded_anchor, n_matched
        else:
            if recorded_anchor is not None:
                print(
                    f"warning: the jobs log looks like a different build than the "
                    f"selected ninja run ({(recorded_anchor - anchor_ms) / 1000.0:+.0f}s "
                    "apart); keeping the mtime-derived anchor and ignoring the recording",
                    file=sys.stderr,
                )
                recorded = []
            if jobs:
                anchor_ms, matched = refine_anchor(anchor_ms, run.edges, jobs)

    if jobs and not args.keep_outside_run:
        jobs, outside = partition_within_run(jobs, run, anchor_ms)
        if outside:
            print(
                f"note: dropped {len(outside)} stats job(s) outside this ninja run "
                "(configure-time try-compiles or an earlier build); pass "
                "--keep-outside-run to keep them",
                file=sys.stderr,
            )

    unattributed = attribute(jobs, run.edges, anchor_ms) if jobs else 0

    invocations = recorded
    if invocations and not args.keep_outside_run:
        invocations, outside = partition_within_run(invocations, run, anchor_ms)
        if outside:
            print(
                f"note: dropped {len(outside)} recorded swiftc invocation(s) outside "
                "this ninja run",
                file=sys.stderr,
            )
    if invocations:
        unmatched = attribute_invocations(invocations, run.edges, anchor_ms)
        if unmatched:
            print(
                f"note: {unmatched} recorded swiftc invocation(s) matched no ninja edge",
                file=sys.stderr,
            )

    rows = build_rows(run, jobs, anchor_ms, invocations or None)
    trace = emit_trace(
        rows,
        run,
        spawning_edge_ids(jobs, invocations),
        anchor_ms,
        args.cores,
        args.relative,
    )
    summarize(
        run, rows, jobs, anchor_ms, args.cores, unattributed, matched, invocations
    )

    for problem in validate_trace(trace):
        print(f"warning: {problem} (Perfetto will report overlapping slices)",
              file=sys.stderr)

    text = json.dumps(trace)
    if args.out:
        if args.out.suffix == ".gz":
            with gzip.open(args.out, "wt", encoding="utf-8") as f:
                f.write(text)
        else:
            args.out.write_text(text)
        print(f"wrote {args.out} ({len(trace['traceEvents'])} events)", file=sys.stderr)
    else:
        sys.stdout.write(text)
    return 0


def merge_runs(runs: list[NinjaRun]) -> NinjaRun:
    """Flatten every ninja invocation onto one timeline at its true wall time.

    Each run's edge offsets restart near zero, so rebase them against the
    earliest run's anchor instead of stacking the runs on top of each other.
    """
    base = runs[0].anchor_ms
    edges: list[NinjaEdge] = []
    for run in runs:
        delta = int(round(run.anchor_ms - base))
        for edge in run.edges:
            edges.append(
                NinjaEdge(
                    start_ms=edge.start_ms + delta,
                    end_ms=edge.end_ms + delta,
                    mtime_ns=edge.mtime_ns,
                    cmdhash=edge.cmdhash,
                    outputs=list(edge.outputs),
                )
            )
    edges.sort(key=lambda e: e.start_ms)
    return NinjaRun(base, edges)


def anchor_is_plausible(recorded_anchor_ms: float, run: NinjaRun) -> bool:
    """Whether a recording could plausibly belong to this ninja invocation.

    A restat edge shifts the mtime estimate by at most one edge's duration, so a
    disagreement inside the run's own span is expected and the recording wins. A
    larger one means the log came from a different build -- output paths repeat
    between builds, so path matching alone cannot tell them apart.
    """
    lo, hi = run.window_ms
    return abs(recorded_anchor_ms - run.anchor_ms) <= max(5_000.0, float(hi - lo))


def select_run(
    runs: list[NinjaRun],
    jobs: list[FrontendJob],
    invocations: list[DriverInvocation],
) -> NinjaRun:
    """Pick the ninja invocation the supplied timing data came from."""
    candidates: list[tuple[int, float, NinjaRun]] = []
    for run in runs:
        anchor, matched = anchor_from_invocations(invocations, run.edges)
        if anchor is not None and anchor_is_plausible(anchor, run):
            candidates.append((matched, run.anchor_ms, run))
    if candidates:
        return max(candidates, key=lambda c: (c[0], c[1]))[2]
    return _select_run(runs, jobs)


def _select_run(runs: list[NinjaRun], jobs: list[FrontendJob]) -> NinjaRun:
    """Pick the ninja invocation the stats files came from, else the newest."""
    if not jobs:
        return runs[-1]

    def contained(run: NinjaRun) -> int:
        lo, hi = run.window_ms
        start = (run.anchor_ms + lo) * 1000.0
        end = (run.anchor_ms + hi) * 1000.0
        return sum(1 for j in jobs if start <= j.start_us and j.end_us <= end)

    best = max(runs, key=lambda r: (contained(r), r.anchor_ms))
    return best if contained(best) else runs[-1]


if __name__ == "__main__":
    sys.exit(main())
