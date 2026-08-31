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

### `cycle-analysis`

Draw every frame cycle as a bar, one cell per slice of time, colored by whatever the
engine is doing at that moment. Where `analyze` reports the median cycle, this shows the
individual ones, which is how a cycle stalled by layout is told apart from one stalled
by a long timer or a slow `requestAnimationFrame` callback.

```
webkit-sysprof cycle-analysis CAPTURE_FILE [-t TIMESPAN] [-n MAX_CYCLES]
                              [-r RESOLUTION] [--order first|slowest]
                              [--min-duration MS] [--min-length MS]
                              [--max-cells CELLS] [--no-tile-lane]
                              [--color auto|always|never]
```

- `-n`/`--max-cycles`: how many cycles to draw (default 40).
- `-r`/`--resolution`: milliseconds per cell (default 0.2).
- `--order`: `first` draws the cycles in capture order, `slowest` the worst ones first,
  which is usually what you want when hunting dropped frames.
- `--min-duration`: skip cycles shorter than this, another way to isolate bad frames.
- `--min-length`: shortest bar to draw, in milliseconds (default 16). Bars are padded to
  at least this length so the refresh marker stays visible, and the padding is drawn
  blank because it is not part of the cycle.
- `--max-cells`: a cycle longer than this many cells is truncated and marked with `»`
  (default 400). Only the cycle counts: a short bar whose padding did not fit is drawn
  whole and is not marked.
- `--no-tile-lane`: drop the `tiles` lane.
- `--color`: `auto` colorizes only when writing to a terminal. Without color each cell
  becomes a letter instead of a colored block, and the legend maps both.

```
$ webkit-sysprof cycle-analysis capture.syscap -t 30000-30200 -n 2 -r 0.5
                          0   2   4   6   8   10  12  14  16  18  2
                          ┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼─┴─┼ [ms]
#1        20.40 ms  main ▕uuRRRRRRSSSSBBLL........UUUffPPPP│PPPPwww
                   tiles ▕.................2344432.........│.......

#2        20.40 ms  main ▕uuRRRRRRSSSSBBLL........UUUffPPPP│PPPPwww
                   tiles ▕.................2344432.........│.......
```

Reading the first bar: the rendering update opens (`u`), then `requestAnimationFrame`
callbacks (`R`), style resolution (`S`), render tree build (`B`) and layout (`L`),
an idle stretch during which the lower lane shows two to four tiles painting in
parallel, then the tile upload (`U`), the flush of compositing state (`f`), the paint
(`P`) and the wait for the composition to complete (`w`). This cycle crosses the refresh
marker, so it cost a frame.

The `main` lane holds the marks of the process that rendered the cycle, which for all
but the timer marks is its main thread. `EventLoopRun`, `WebCoreTimerExecution` and
`WebCoreThreadTimers` are emitted by any thread running a run loop, and a capture names
the process a mark came from but not the thread, so those three cannot be told apart
from main-thread work. The innermost mark covering a cell wins, so nested marks are
drawn over their parents and the bar reads as the real call structure rather than as
flat top-level phases.

The `tiles` lane is **not** the main thread and does not show an activity. It counts how
many tiles are being rasterized at that instant on the painting threads, so `3` means
three `PaintTile` marks overlap there. Painting is asynchronous, which is why this lane
comes alive where the `main` lane goes idle: the main thread has handed the tiles off
and is waiting for them before it can composite. It answers what the main lane cannot,
namely whether an idle stretch is the engine waiting on parallel work or doing nothing.

A vertical line crosses all bars at each multiple of the refresh interval. A cycle that
reaches past the first line took longer than one display refresh, so it cost a frame. The
line replaces the cell it is drawn in, so where a refresh interval is only a few cells
wide, the header says so and no line is drawn rather than most of the lane being markers.

A cycle only ever runs between two rendering updates of one process, so where more than
one rendered, each bar names the process it belongs to and is drawn from that process's
marks alone.

The legend lists the activities present, the average time each took per drawn cycle, and
the marks it is drawn from. The average covers the whole of every cycle drawn, including
whatever part of a truncated one did not fit in its bar. Several activities aggregate more than one mark, so that
list is what says exactly what a color covers. It also makes clear that `idle` is the
absence of any mark rather than a mark of its own.


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
