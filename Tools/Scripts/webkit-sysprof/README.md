# webkit-sysprof

A command-line tool for processing [Sysprof](https://gitlab.gnome.org/GNOME/sysprof)
`.syscap` capture files recorded from WebKit (GTK/WPE ports). It extracts marks
(timeline events) and counters (time-series metrics) from a capture and lets you dump,
summarize, analyze (rendering/frame-timing statistics), or plot delta-time histograms for
them.

## Installation

```
pip install -e .
```

This installs the `webkit-sysprof` command.

## Commands

### `dump`

Dump marks or counters from a capture as CSV or JSON.

```
webkit-sysprof dump [--marks|--counters] [-f csv|json] CAPTURE_FILE
```

`--marks` is the default when neither flag is given. `-f`/`--format`: `csv` (default) or
`json` — `json` prints the same rows as `csv`, just as a JSON array of objects.

```
$ webkit-sysprof dump capture.syscap
group;name;message;time;duration;end_time
...

$ webkit-sysprof dump --counters capture.syscap
category;name;description;time;offset;value
...

$ webkit-sysprof dump -f json capture.syscap
[{"group": "...", "name": "...", "message": "...", "time": ..., "duration": ..., "end_time": ...}, ...]
```

### `summary`

Print a summary of a capture: document metadata, mark counts by name, and counter counts
by category/name.

```
webkit-sysprof summary CAPTURE_FILE
```

```
$ webkit-sysprof summary capture.syscap
File: capture.syscap
Title: capture.syscap
Subtitle: Recording at 14:00:04 04/27/26
Timespan: 0.0000 - 4.4673 [s]

Marks: 669
  AcquireTexture: 80
  ...

Counters: 53
  CPU Frequency / CPU 0: 23 values
  ...
```

### `analyze`

Compute rendering statistics (vblank intervals, theoretical FPS, frame compositions per
vblank) and duration/count statistics for a fixed set of WebKit-specific marks (styling,
layout, rendering, compositing and tiles).

```
webkit-sysprof analyze CAPTURE_FILE [-f text|json] [-t TIMESPAN]
```

- `-f`/`--format`: `text` (default) or `json`.
- `-t`/`--timespan`: restrict the analysis to a window in milliseconds relative to the
  capture start, e.g. `0-5000`. Either side may be omitted (`500-` means "from 500ms to
  the end", `-500` means "from the start to 500ms"). Defaults to the whole capture (`-`).

```
$ webkit-sysprof analyze capture.syscap
Timespan: 0.0000 - 4.4673 [s]
Duration: 4.4673 [s]

Rendering:
- vblanks: 35
...

$ webkit-sysprof analyze -f json -t 0-2000 capture.syscap
{"document": {...}, "statistics": {...}, "rendering": {...}}
```

Restricting the timespan matters more than it looks: page load dominates the tail of
most metrics, so a whole-capture run largely measures startup. Skipping the first
seconds is usually what you want when comparing configurations.

#### Reading the mark statistics

Three marks are easy to misread:

- `StyleRecalc` is an umbrella mark that nests `RenderTreeBuild`,
  `PerformSubtreesLayout` and `CompositingUpdate`. Its duration is not style resolution
  alone. Subtract the nested marks to get that.
- `CompositingUpdate` runs twice per rendering update, once inside `StyleRecalc` and
  once after it, and the second one is much shorter. Its percentiles are therefore
  bimodal rather than centered on a typical value.
- `PaintTile` runs on the painting threads, so summing it over a frame gives aggregate
  work rather than elapsed time. Its `#pixels` statistic is the dirty area taken from
  the mark's `dirty region <x>x<y>+<width>+<height>` message.

### `delta-histogram`

Plot a histogram of the time between consecutive occurrences of a given mark (e.g. to
visualize `DisplayLinkUpdate` jitter).

```
webkit-sysprof delta-histogram CAPTURE_FILE MARK_TYPE [-t TIMESPAN]
```

`-t`/`--timespan` behaves exactly as in `analyze`.

```
$ webkit-sysprof delta-histogram capture.syscap DisplayLinkUpdate
$ webkit-sysprof delta-histogram capture.syscap DisplayLinkUpdate -t 0-2000
```

Opens a matplotlib window with a linear-scale and a log-scale histogram of the deltas
(in milliseconds), annotated with mean/median/min/max lines. If the mark occurs fewer
than twice in the (optionally trimmed) capture, it prints `No data available for mark:
<MARK_TYPE>` instead.

## Development

Run all checks — lint, format-check, type-check, and the test suite (each in its own
isolated tox env):

```
tox
```

Run an individual check:

```
tox -e lint      # flake8
tox -e format    # black --check --diff
tox -e mypy      # mypy
```

Run just the test suite:

```
tox -e test
```
