# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

import argparse
import collections
import datetime
import html
import json
import os
import sys

from webkitcorepy import Terminal, run
from webkitscmpy import local


class CommitGraph(object):
    DEFAULT_YEARS = 5
    DEFAULT_AVERAGE = 8
    DEFAULT_OUTPUT = 'commit-graph.html'

    # Line colors are assigned to repositories in this order, never cycled
    PALETTE_LIGHT = ('#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4', '#008300', '#4a3aa7', '#e34948')
    PALETTE_DARK = ('#3987e5', '#d95926', '#199e70', '#c98500', '#d55181', '#008300', '#9085e9', '#e66767')

    @classmethod
    def parser(cls) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(
            description='Chart the number of commits landed per week on one or more repositories',
        )
        parser.add_argument(
            'repositories', nargs='*', default=[],
            help='Paths of the repositories to compare, defaults to the current directory',
        )
        parser.add_argument(
            '--branch', '-b', type=str, default=None,
            help="Branch to count commits on, defaults to each repository's default branch",
        )
        parser.add_argument(
            '--years', type=int, default=cls.DEFAULT_YEARS,
            help='Number of years to chart, ending this week (default {})'.format(cls.DEFAULT_YEARS),
        )
        parser.add_argument(
            '--since', type=datetime.date.fromisoformat, default=None,
            help='First day (YYYY-MM-DD) to chart, overrides --years',
        )
        parser.add_argument(
            '--until', type=datetime.date.fromisoformat, default=None,
            help='Day (YYYY-MM-DD) to stop charting before, defaults to today',
        )
        parser.add_argument(
            '--average', type=int, default=cls.DEFAULT_AVERAGE,
            help='Number of weeks in the rolling average (default {})'.format(cls.DEFAULT_AVERAGE),
        )
        parser.add_argument(
            '--output', '-o', type=str, default=cls.DEFAULT_OUTPUT,
            help='File to write the HTML chart to (default {})'.format(cls.DEFAULT_OUTPUT),
        )
        parser.add_argument(
            '--csv', dest='csv', action='store_true', default=False,
            help='Print the weekly counts as CSV instead of writing a chart',
        )
        parser.add_argument(
            '--open', dest='open', action='store_true', default=True,
            help='Open the chart once it has been written (default)',
        )
        parser.add_argument(
            '--no-open', dest='open', action='store_false',
            help='Do not open the chart once it has been written',
        )
        return parser

    @classmethod
    def weeks(cls, since: datetime.date, until: datetime.date) -> list[datetime.date]:
        # Only count full, Monday-start weeks which end before 'until'
        week = since + datetime.timedelta(days=(7 - since.weekday()) % 7)
        result = []
        while week + datetime.timedelta(days=7) <= until:
            result.append(week)
            week += datetime.timedelta(days=7)
        return result

    @classmethod
    def timestamps(cls, repository: local.Git, branch: str, since: datetime.date) -> list[int] | None:
        # Following first parents counts each commit when it landed on the branch
        output = run(
            [repository.executable(), 'log', '--first-parent', '--format=%ct', '--since={}'.format(since.isoformat()), branch],
            cwd=repository.root_path, capture_output=True, encoding='utf-8',
        )
        if output.returncode:
            return None
        return [int(line) for line in output.stdout.splitlines() if line.strip()]

    @classmethod
    def count(cls, timestamps: list[int], weeks: list[datetime.date]) -> list[int]:
        counter: collections.Counter[datetime.date] = collections.Counter()
        for timestamp in timestamps:
            day = datetime.datetime.fromtimestamp(timestamp).date()
            counter[day - datetime.timedelta(days=day.weekday())] += 1
        return [counter[week] for week in weeks]

    @classmethod
    def csv(cls, weeks: list[datetime.date], series: list[tuple[str, list[int]]]) -> str:
        lines = [','.join(['week'] + [name for name, _ in series])]
        for index, week in enumerate(weeks):
            lines.append(','.join([week.isoformat()] + [str(counts[index]) for _, counts in series]))
        return '\n'.join(lines) + '\n'

    @classmethod
    def date(cls, day: datetime.date) -> str:
        return '{} {}, {}'.format(day.strftime('%b'), day.day, day.year)

    @classmethod
    def html(cls, weeks: list[datetime.date], series: list[tuple[str, list[int]]], branches: list[str], average: int) -> str:
        names = [name for name, _ in series]
        if len(names) > 1:
            title = 'Weekly commit volume: {}'.format(' vs '.join(names))
        else:
            title = 'Weekly commit volume: {}'.format(names[0])
        on = ', '.join(sorted(set(branches)))
        subtitle = 'Commits landed on {} (first-parent) per week, {} to {}. Thin lines show weekly counts; bold lines show the {}-week rolling average.'.format(
            on, cls.date(weeks[0]), cls.date(weeks[-1] + datetime.timedelta(days=6)), average,
        )
        data = json.dumps(dict(
            weeks=[week.isoformat() for week in weeks],
            series=[dict(name=name, counts=counts) for name, counts in series],
            average=average,
        )).replace('</', '<\\/')

        palette = ''.join('--series-{}:{};'.format(index + 1, color) for index, color in enumerate(cls.PALETTE_LIGHT))
        palette_dark = ''.join('--series-{}:{};'.format(index + 1, color) for index, color in enumerate(cls.PALETTE_DARK))
        return TEMPLATE.replace(
            '__PALETTE_LIGHT__', palette,
        ).replace(
            '__PALETTE_DARK__', palette_dark,
        ).replace(
            '__TITLE__', html.escape(title),
        ).replace(
            '__SUBTITLE__', html.escape(subtitle),
        ).replace('__DATA__', data)

    @classmethod
    def main(cls, argv: list[str] | None = None) -> int:
        args = cls.parser().parse_args(argv)

        repositories: list[local.Scm] = []
        for path in args.repositories or [os.getcwd()]:
            try:
                repositories.append(local.Scm.from_path(path=path))
            except OSError:
                sys.stderr.write("No repository found at '{}'\n".format(path))
                return 1

        if len(repositories) > len(cls.PALETTE_LIGHT):
            sys.stderr.write('Cannot chart more than {} repositories\n'.format(len(cls.PALETTE_LIGHT)))
            return 1
        if args.average < 1:
            sys.stderr.write('Rolling average must be at least 1 week\n')
            return 1

        until = args.until or datetime.date.today()
        since = args.since
        if not since:
            try:
                since = until.replace(year=until.year - args.years)
            except ValueError:
                since = until.replace(year=until.year - args.years, day=28)
        weeks = cls.weeks(since, until)
        if not weeks:
            sys.stderr.write('No full weeks between {} and {}\n'.format(since.isoformat(), until.isoformat()))
            return 1

        series: list[tuple[str, list[int]]] = []
        branches: list[str] = []
        for repo in repositories:
            if not isinstance(repo, local.Git):
                sys.stderr.write("Can only chart git repositories, '{}' is not one\n".format(repo.root_path))
                return 1
            name = os.path.basename(repo.root_path or '')
            branch = args.branch or repo.default_branch
            if not branch:
                sys.stderr.write("Failed to determine the default branch of '{}'\n".format(name))
                return 1
            timestamps = cls.timestamps(repo, branch, since)
            if timestamps is None:
                sys.stderr.write("Failed to retrieve commits on '{}' in '{}'\n".format(branch, name))
                return 1
            series.append((name, cls.count(timestamps, weeks)))
            branches.append(branch)

        if args.csv:
            sys.stdout.write(cls.csv(weeks, series))
            return 0

        with open(args.output, 'w') as file:
            file.write(cls.html(weeks, series, branches, args.average))
        for name, counts in series:
            print('{}: {} commits over {} weeks'.format(name, sum(counts), len(weeks)))
        output = os.path.abspath(args.output)
        print('Wrote chart to {}'.format(output))
        if args.open:
            Terminal.open_url('file://{}'.format(output))
        return 0


TEMPLATE = '''<!doctype html>
<html><head><meta charset="utf-8"><title>__TITLE__</title>
<style>
.viz-root{color-scheme:light;--page:#f9f9f7;--surface-1:#fcfcfb;--text-primary:#0b0b0b;--text-secondary:#52514e;--muted:#898781;--grid:#e1e0d9;--axis:#c3c2b7;--border:rgba(11,11,11,.10);__PALETTE_LIGHT__}
@media (prefers-color-scheme:dark){:root:where(:not([data-theme="light"])) .viz-root{color-scheme:dark;--page:#0d0d0d;--surface-1:#1a1a19;--text-primary:#fff;--text-secondary:#c3c2b7;--grid:#2c2c2a;--axis:#383835;--border:rgba(255,255,255,.10);__PALETTE_DARK__}}
body{margin:0}
.viz-root{font:14px system-ui,-apple-system,"Segoe UI",sans-serif;background:var(--page);color:var(--text-primary);min-height:100vh;padding:24px;box-sizing:border-box}
.card{background:var(--surface-1);border:1px solid var(--border);border-radius:12px;padding:20px 24px;max-width:1200px;margin:auto}
h1{font-size:18px;margin:0 0 4px}.sub{color:var(--text-secondary);margin:0 0 12px}
.legend{display:flex;flex-wrap:wrap;gap:20px;color:var(--text-secondary);margin-bottom:8px}.legend span{display:flex;align-items:center;gap:6px}
.sw{width:14px;height:3px;border-radius:2px;display:inline-block}
svg{display:block;width:100%;height:auto;overflow:visible}
.tick{fill:var(--muted);font-size:12px;font-variant-numeric:tabular-nums}
.tip{position:fixed;pointer-events:none;background:var(--surface-1);border:1px solid var(--border);border-radius:8px;padding:8px 10px;box-shadow:0 4px 12px rgba(0,0,0,.12);font-size:13px;display:none;font-variant-numeric:tabular-nums}
.tip b{display:block;margin-bottom:4px}.tip div{display:flex;align-items:center;gap:6px;color:var(--text-secondary)}.tip div strong{color:var(--text-primary);margin-left:auto;padding-left:12px}
.tip .avg{font-size:12px;margin-left:6px}
details{margin-top:16px;color:var(--text-secondary)}table{border-collapse:collapse;font-variant-numeric:tabular-nums;margin-top:8px}td,th{padding:2px 12px;text-align:right;border-bottom:1px solid var(--grid)}th:first-child,td:first-child{text-align:left}
</style></head>
<body><div class="viz-root"><div class="card">
<h1>__TITLE__</h1>
<p class="sub">__SUBTITLE__</p>
<div class="legend" id="legend"></div>
<svg id="chart" viewBox="0 0 1150 440" role="img" aria-label="__TITLE__"></svg>
<details><summary>Table view</summary><table id="table"><thead></thead><tbody></tbody></table></details>
</div><div class="tip" id="tip"></div></div>
<script>
const DATA = __DATA__;
const WEEKS = DATA.weeks.map(w => new Date(w + "T00:00:00"));
const SERIES = DATA.series.map((s, k) => {
    const color = `var(--series-${k + 1})`;
    const average = s.counts.map((_, j) => {
        const span = s.counts.slice(Math.max(0, j - DATA.average + 1), j + 1);
        return span.reduce((a, b) => a + b, 0) / span.length;
    });
    return {name: s.name, counts: s.counts, average, color};
});
const esc = text => text.replace(/[&<>"]/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", "\\"": "&quot;"}[c]));

const W = 1150, H = 440, m = {l: 48, r: 110, t: 12, b: 28}, pw = W - m.l - m.r, ph = H - m.t - m.b;
const peak = Math.max(1, ...SERIES.flatMap(s => s.counts));
const magnitude = Math.pow(10, Math.floor(Math.log10(peak / 6)));
const step = [1, 2, 5, 10].map(n => n * magnitude).find(n => peak / n <= 8);
const ymax = Math.ceil(peak / step) * step;
const last = WEEKS.length - 1;
const x = j => m.l + (last ? j / last : 0.5) * pw, y = v => m.t + ph - v / ymax * ph;
const svg = document.getElementById("chart"), NS = "http://www.w3.org/2000/svg";
const el = (tag, attrs, parent = svg) => {
    const e = document.createElementNS(NS, tag);
    for (const key in attrs)
        e.setAttribute(key, attrs[key]);
    parent.appendChild(e);
    return e;
};

for (let v = 0; v <= ymax; v += step) {
    el("line", {x1: m.l, x2: m.l + pw, y1: y(v), y2: y(v), stroke: v ? "var(--grid)" : "var(--axis)", "stroke-width": 1});
    el("text", {x: m.l - 8, y: y(v) + 4, "text-anchor": "end", class: "tick"}).textContent = v;
}
const yearly = WEEKS.length > 78;
WEEKS.forEach((d, j) => {
    if (d.getDate() > 7 || (yearly && d.getMonth()))
        return;
    el("line", {x1: x(j), x2: x(j), y1: m.t + ph, y2: m.t + ph + 5, stroke: "var(--axis)"});
    el("text", {x: x(j), y: H - 6, "text-anchor": "middle", class: "tick"}).textContent = yearly ? d.getFullYear() : d.toLocaleDateString(undefined, {month: "short", year: "2-digit"});
});

const path = (values, attrs) => el("path", Object.assign({
    d: values.map((v, j) => (j ? "L" : "M") + x(j).toFixed(1) + "," + y(v).toFixed(1)).join(""),
    fill: "none", "stroke-linejoin": "round", "stroke-linecap": "round",
}, attrs));
SERIES.forEach(s => path(s.counts, {stroke: s.color, "stroke-width": 1, opacity: 0.35}));
SERIES.forEach(s => path(s.average, {stroke: s.color, "stroke-width": 2.5}));

// Direct labels at the end of each line, nudged apart so they don't collide
if (SERIES.length <= 4) {
    const labels = SERIES.map(s => ({name: s.name, y: y(s.average[last])})).sort((a, b) => a.y - b.y);
    labels.forEach((label, i) => { if (i && label.y - labels[i - 1].y < 16) label.y = labels[i - 1].y + 16; });
    labels.forEach(label => { el("text", {x: x(last) + 8, y: label.y + 4, fill: "var(--text-primary)", "font-size": 13}).textContent = label.name; });
}

document.getElementById("legend").innerHTML = SERIES.map(s => `<span><i class="sw" style="background:${s.color}"></i>${esc(s.name)}</span>`).join("");

const cross = el("line", {y1: m.t, y2: m.t + ph, stroke: "var(--axis)", visibility: "hidden"});
const dots = SERIES.map(s => el("circle", {r: 4, fill: s.color, stroke: "var(--surface-1)", "stroke-width": 2, visibility: "hidden"}));
const hit = el("rect", {x: m.l, y: m.t, width: pw, height: ph, fill: "transparent"});
const tip = document.getElementById("tip");
const format = d => d.toLocaleDateString(undefined, {month: "short", day: "numeric", year: "numeric"});
hit.addEventListener("mousemove", event => {
    const point = svg.createSVGPoint();
    point.x = event.clientX;
    point.y = event.clientY;
    const local = point.matrixTransform(svg.getScreenCTM().inverse());
    const j = Math.max(0, Math.min(last, Math.round((local.x - m.l) / pw * last)));
    cross.setAttribute("x1", x(j));
    cross.setAttribute("x2", x(j));
    cross.setAttribute("visibility", "visible");
    SERIES.forEach((s, k) => {
        dots[k].setAttribute("cx", x(j));
        dots[k].setAttribute("cy", y(s.counts[j]));
        dots[k].setAttribute("visibility", "visible");
    });
    tip.innerHTML = `<b>Week of ${format(WEEKS[j])}</b>` + SERIES.map(s =>
        `<div><i class="sw" style="background:${s.color}"></i>${esc(s.name)}<strong>${s.counts[j]}</strong><span class="avg">avg ${Math.round(s.average[j])}</span></div>`
    ).join("");
    tip.style.display = "block";
    tip.style.left = (event.clientX + 16 + tip.offsetWidth > innerWidth ? event.clientX - 16 - tip.offsetWidth : event.clientX + 16) + "px";
    tip.style.top = (event.clientY - 20) + "px";
});
hit.addEventListener("mouseleave", () => {
    tip.style.display = "none";
    [cross, ...dots].forEach(node => node.setAttribute("visibility", "hidden"));
});

document.querySelector("#table thead").innerHTML = "<tr><th>Week of</th>" + SERIES.map(s => `<th>${esc(s.name)}</th>`).join("") + "</tr>";
document.querySelector("#table tbody").innerHTML = DATA.weeks.map((w, j) => `<tr><td>${w}</td>` + SERIES.map(s => `<td>${s.counts[j]}</td>`).join("") + "</tr>").join("");
</script></body></html>
'''
