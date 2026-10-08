#!/usr/bin/env python3
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

"""Order a target's Swift sources so the Swift driver's batches are balanced.

In batch mode the driver splits a module's files into max(-j, ceil(files / 25)) frontend jobs, filling them by
count in source order, so a module waits on its heaviest batch: two expensive files that happen to be adjacent
share a job and the module takes as long as both. This reorders the Swift entries of a source list (other
entries keep their positions) so that each batch, at the -j the build passes, gets a fair share of the work.

A file's cost comes from this machine's history when there is one: traced builds (SWIFT_NINJA_TRACE) record
each frontend job's primary files and instruction count, and --ingest-only folds those jobs into a per-machine
history file. Each job's cost is a fixed cost plus the sum of its files' costs, and batches change from build
to build as files are added, so the history determines each file's cost; it is solved here, non-negative and
pulled toward the static estimate where the history says little. Files the history has not seen, and machines
without one, use the static estimate: code lines plus #expect / #require uses, each of which costs about as
much to type-check as 42 lines.

usage: order_sources_for_batching.py --jobs N --base-dir DIR --input LIST --output LIST
           [--history FILE --source-dir DIR [--observe-jobs-log LOG --observe-stats-dir DIR]]
       order_sources_for_batching.py --ingest-only --history FILE --source-dir DIR
           --observe-jobs-log LOG --observe-stats-dir DIR
LIST files hold one source per line, as written in the CMake list (relative to DIR or absolute).
"""

import argparse
import heapq
import json
import os
import re
import sys
from pathlib import Path

MACRO_LINES = 42
FILE_LINES = 356
SIZE_LIMIT = 25
MACRO = re.compile(r'#(expect|require)\b')
MAX_OBSERVATIONS = 20000
RIDGE = 0.1         # how many observations' worth of trust the static estimate gets, per file
SOLVER_PASSES = 100


def is_swift(entry):
    """A plain Swift source; generator expressions are left where they are."""
    return entry.endswith('.swift') and '$<' not in entry


def weight(path):
    try:
        with open(path, errors='replace') as f:
            text = f.read()
    except OSError:
        return FILE_LINES
    lines, block = 0, False
    for line in text.splitlines():
        s = line.strip()
        if block:
            if '*/' not in s:
                continue
            block, s = False, s.split('*/', 1)[1].strip()
        if s.startswith('/*'):
            block = '*/' not in s
            continue
        if s and not s.startswith('//'):
            lines += 1
    return lines + MACRO_LINES * len(MACRO.findall(text)) + FILE_LINES


def batch_sizes(count, jobs):
    parts = max(jobs, -(-count // SIZE_LIMIT))
    size, extra = divmod(count, parts)
    return [size + (1 if i < extra else 0) for i in range(parts) if size + (1 if i < extra else 0)]


def balanced(files, weights, jobs):
    """An order of `files` that balances the driver's batches."""
    sizes = batch_sizes(len(files), jobs)
    bins = [[] for _ in sizes]
    load = [0] * len(sizes)
    heap = [(0, i) for i in range(len(sizes))]
    for f in sorted(files, key=lambda f: (-weights[f], f)):
        full = []
        while True:
            current, i = heapq.heappop(heap)
            if len(bins[i]) < sizes[i]:
                break
            full.append((current, i))
        bins[i].append(f)
        load[i] += weights[f]
        if len(bins[i]) < sizes[i]:
            heapq.heappush(heap, (load[i], i))
        for entry in full:
            heapq.heappush(heap, entry)
    while True:
        heaviest = max(range(len(bins)), key=lambda i: load[i])
        best = None
        for other in range(len(bins)):
            if other == heaviest:
                continue
            for a in bins[heaviest]:
                for b in bins[other]:
                    moved = weights[a] - weights[b]
                    if moved > 0:
                        result = max(load[heaviest] - moved, load[other] + moved)
                        if result < load[heaviest] and (best is None or result < best[0]):
                            best = (result, other, a, b)
        if best is None:
            break
        _, other, a, b = best
        bins[heaviest][bins[heaviest].index(a)] = b
        bins[other][bins[other].index(b)] = a
        load[heaviest] -= weights[a] - weights[b]
        load[other] += weights[a] - weights[b]
    position = {f: i for i, f in enumerate(files)}
    return [f for b in bins for f in sorted(b, key=position.get)]


def observe(jobs_log, stats_dir, source_dir):
    """One observation per finished Swift compile job of a traced build: its primary sources (relative to
    `source_dir`) and its instruction count, read the way ninja_build_trace.py reads them."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import ninja_build_trace as nbt
    if not os.path.exists(jobs_log) or not os.path.isdir(stats_dir):
        return []
    by_output = {}
    for job in nbt.load_stats_dir([Path(stats_dir)]):
        for name in nbt._output_names(job):
            by_output.setdefault(name, job)
    root = os.path.realpath(source_dir) + os.sep
    observations = []
    for invocation in nbt.load_jobs_log(Path(jobs_log)):
        for job in invocation.jobs:
            if job.name != 'compile' or job.unfinished or job.exit_status:
                continue
            inputs = [os.path.realpath(os.path.join(invocation.cwd, i)) for i in job.inputs if i.endswith('.swift')]
            if not inputs or not all(i.startswith(root) for i in inputs):
                continue
            stats = by_output.get(nbt.clean_name(os.path.basename(job.output or '')))
            cost = stats.stats.get('Frontend.NumInstructionsExecuted') if stats else None
            if cost:
                observations.append({'id': stats.path.name, 'files': sorted(i[len(root):] for i in inputs), 'cost': cost})
    return observations


def load_history(path):
    try:
        with open(path) as f:
            return json.load(f).get('observations', [])
    except (OSError, ValueError):
        return []


def ingest(path, observations):
    """Add `observations` to the history at `path`, keeping each job once and the most recent jobs."""
    known = load_history(path)
    ids = {o['id'] for o in known}
    known += [o for o in observations if o['id'] not in ids]
    known = known[-MAX_OBSERVATIONS:]
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path + '.tmp', 'w') as f:
        json.dump({'version': 1, 'observations': known}, f)
    os.replace(path + '.tmp', path)
    return len(known)


def solve(observations, prior):
    """Per-file costs, in instructions, for the files in `prior` (file -> static weight), from the observed jobs
    whose sources are all among them; None without such jobs. Files no job covered get the static weight scaled
    to instructions."""
    jobs = [o for o in observations if o['files'] and all(f in prior for f in o['files'])]
    if not jobs:
        return None
    # A job's cost ~ fixed + scale * (its files' static weights): the scale converts weights to instructions.
    xs = [sum(prior[f] for f in o['files']) for o in jobs]
    ys = [o['cost'] for o in jobs]
    n, mx, my = len(jobs), sum(xs) / len(jobs), sum(ys) / len(jobs)
    var = sum((x - mx) ** 2 for x in xs)
    scale = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / var if var else my / mx
    if scale <= 0:
        scale = my / mx
    fixed = max(0.0, my - scale * mx)
    cost = {f: scale * w for f, w in prior.items()}
    covering = {f: [] for f in prior}
    for k, o in enumerate(jobs):
        for f in o['files']:
            covering[f].append(k)
    totals = [fixed + sum(cost[f] for f in o['files']) for o in jobs]
    for _ in range(SOLVER_PASSES):
        for f, ks in covering.items():
            if not ks:
                continue
            # Minimize sum over f's jobs of (observed - predicted)^2 + RIDGE * (cost - scaled prior)^2.
            residual = sum(ys[k] - (totals[k] - cost[f]) for k in ks)
            new = max(0.0, (residual + RIDGE * scale * prior[f]) / (len(ks) + RIDGE))
            for k in ks:
                totals[k] += new - cost[f]
            cost[f] = new
        fixed = max(0.0, sum(y - (t - fixed) for y, t in zip(ys, totals)) / n)
        totals = [fixed + sum(cost[f] for f in o['files']) for o in jobs]
    return cost


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--jobs', type=int)
    parser.add_argument('--base-dir')
    parser.add_argument('--input')
    parser.add_argument('--output')
    parser.add_argument('--history', help='per-machine history of observed Swift compile jobs')
    parser.add_argument('--source-dir', help='the source tree the history names files relative to')
    parser.add_argument('--observe-jobs-log', help='a traced build\'s swift-jobs.jsonl, to add to the history')
    parser.add_argument('--observe-stats-dir', help='that build\'s -stats-output-dir')
    parser.add_argument('--ingest-only', action='store_true', help='only add the observed jobs to the history')
    args = parser.parse_args()
    if args.history and args.observe_jobs_log and args.observe_stats_dir and args.source_dir:
        try:
            ingest(args.history, observe(args.observe_jobs_log, args.observe_stats_dir, args.source_dir))
        except Exception as error:
            print(f'warning: could not add {args.observe_jobs_log} to {args.history}: {error}', file=sys.stderr)
    if args.ingest_only:
        return 0
    if None in (args.jobs, args.base_dir, args.input, args.output):
        parser.error('--jobs, --base-dir, --input and --output are required unless --ingest-only')
    with open(args.input) as f:
        entries = [line.rstrip('\n') for line in f if line.strip()]
    # A target lists a file once however often it is added; the driver gets the first occurrence.
    seen = set()
    entries = [e for e in entries if not (is_swift(e) and (e in seen or seen.add(e)))]
    swift = [e for e in entries if is_swift(e)]
    if len(swift) > 1 and args.jobs > 1:
        paths = {e: os.path.realpath(e if os.path.isabs(e) else os.path.join(args.base_dir, e)) for e in swift}
        weights = {e: weight(paths[e]) for e in swift}
        if args.history and args.source_dir:
            # The history names in-tree sources relative to the tree; sources outside it (generated ones) are
            # never in a recorded job, so they keep their static weight, in the same units as the rest.
            root = os.path.realpath(args.source_dir) + os.sep
            key = {e: paths[e][len(root):] if paths[e].startswith(root) else paths[e] for e in swift}
            learned = solve(load_history(args.history), {key[e]: weights[e] for e in swift})
            if learned:
                weights = {e: learned[key[e]] for e in swift}
        ordered = iter(balanced(swift, weights, args.jobs))
        entries = [next(ordered) if is_swift(e) else e for e in entries]
    with open(args.output, 'w') as f:
        f.write('\n'.join(entries) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
