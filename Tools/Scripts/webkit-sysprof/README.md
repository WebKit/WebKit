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
`json`. `json` prints the same rows as `csv`, just as a JSON array of objects. `group`
names the kind of process a mark came from, e.g. `WebKit (Web)`, and `pid` the process
itself, which two of one kind share a group name for.

```
$ webkit-sysprof dump capture.syscap
group;pid;name;message;time;duration;end_time
...

$ webkit-sysprof dump --counters capture.syscap
category;name;description;time;offset;value
...

$ webkit-sysprof dump -f json capture.syscap
[{"group": "...", "pid": ..., "name": "...", "message": "...", "time": ..., "duration": ..., "end_time": ...}, ...]
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
vblank), a frame cycle breakdown, and duration/count statistics for a fixed set of
marks (styling, layout, rendering, compositing and tiles).

```
webkit-sysprof analyze CAPTURE_FILE [-f text|json] [-t TIMESPAN] [-e]
```

- `-f`/`--format`: `text` (default) or `json`.
- `-t`/`--timespan`: restrict the analysis to a window in milliseconds relative to the
  capture start, e.g. `0-5000`. Either side may be omitted (`500-` or `500` mean "from
  500ms to the end", `-500` means "from the start to 500ms"). Defaults to the whole
  capture (`-`), and is clamped to it.
- `-e`/`--explain`: interleave the text report with paragraphs explaining how to read
  it, in particular what theoretical FPS and the frame cycle phases do and do not
  measure.

```
$ webkit-sysprof analyze capture.syscap
Timespan: 0.0000 - 4.4673 [s]
Duration: 4.4673 [s]

Rendering:
- vblanks: 35
...

$ webkit-sysprof analyze -f json -t 0-2000 capture.syscap
{"document": {...}, "statistics": {...}, "rendering": {...}, "frame_cycle": {...}}
```

Restricting the timespan matters more than it looks: page load dominates the tail of
most metrics, so a whole-capture run largely measures startup. Skipping the first
seconds is usually what you want when comparing configurations.

#### Theoretical FPS

`frames rendered` counts `DidRenderFrame` marks of every process (one per composition,
emitted once it is painted and before the frame is handed over) and
`theoretical FPS` divides that by the analyzed duration. `analyze --explain` prints
what that does and does not say, and how it relates to the display refresh rate.

Note that `vblanks per LayerTreeHostRenderingUpdate` (and its `more than 1 per update`
count) only covers the `LayerTreeHostRenderingUpdate` mark. A rendering update that fits
inside one vblank interval can still miss frames, because compositing happens after it.
Use the frame cycle for the complete per-frame budget.

#### Frame cycle

A frame cycle is one turn of the engine's rendering loop, split into the phases the
frame's time went into. `analyze --explain` prints what a cycle is, which mark
delimits each phase and how to read every number of the section.

#### Reading the mark statistics

Three marks measure something other than what their name suggests:

- `StyleRecalc` is an umbrella mark: `RenderTreeBuild`, the `CompositingUpdate` that
  follows a style change and, where style resolution interleaves layout, the layout
  marks all run inside it, so its duration is not style resolution alone.
- `CompositingUpdate` is emitted wherever the compositing layers are updated, after a
  style change, after layout and on scrolling, so its samples mix runs of very
  different lengths and its percentiles are not centered on a typical value.
- `PaintTile` runs on the painting threads, so summing it over a frame gives aggregate
  work rather than elapsed time. Its `#pixels` statistic is the dirty area taken from
  the mark's `dirty region <x>x<y>+<width>+<height>` message.

Percentiles (P25, P75 and P99) are interpolated between the samples, so they lie
between the reported minimum and maximum, and are left out below two samples.
`analyze --explain` prints the long form of all of this in the report itself, next to
the table it applies to.

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

Run tests:

```
Tools/Scripts/test-webkitpy webkitsysprof
```
