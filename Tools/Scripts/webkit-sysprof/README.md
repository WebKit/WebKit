# webkit-sysprof

A command-line tool for processing [Sysprof](https://gitlab.gnome.org/GNOME/sysprof)
`.syscap` capture files recorded from WebKit (GTK/WPE ports). It extracts marks
(timeline events) and counters (time-series metrics) from a capture and lets you dump,
summarize, analyze (rendering/frame-timing statistics), draw a timeline of the main
thread and the compositor, or plot delta-time histograms for them.

## Installation

```
pip install -e .
```

This installs the `webkit-sysprof` command.

## Commands

### `dump`

Dump marks, counters, or collapsed stack traces from a capture as CSV or JSON.

```
webkit-sysprof dump [--marks|--counters|--collapsed-stacktraces] [-f csv|json] CAPTURE_FILE
```

`--marks` is the default when no flag is given. `-f`/`--format`: `csv` (default) or
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

#### `--collapsed-stacktraces`

Dump the capture's stack samples in the folded-stack format that
[FlameGraph](https://github.com/brendangregg/FlameGraph)'s `flamegraph.pl` reads
directly, and that matches what its own `stackcollapse-perf.pl` produces from `perf
script` output: one line per distinct stack, semicolon-separated from the process down
to the innermost frame, followed by a space and how many samples had exactly that
stack. `-f json` gives the same rows as a JSON array of `{"stack": ..., "count": ...}`
objects instead.

```
$ webkit-sysprof dump --collapsed-stacktraces capture.syscap
WPEWebProcess;my_thread_main;WebCore::TextureMapperLayer::paint;memcpy+0x30 3
```

The output in the above format may be very useful for AI agents to digest the profiling
data gathered by sysprof.

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

Compute rendering statistics (refresh interval, vblanks), a breakdown of the
compositions and the compositor's time, the rendering updates and how long each took to
reach a frame, and duration/count statistics for a fixed set of marks (styling,
layout, rendering, compositing and tiles).

```
webkit-sysprof analyze CAPTURE_FILE [-f text|json] [-t TIMESPAN] [-e]
```

- `-f`/`--format`: `text` (default) or `json`.
- `-t`/`--timespan`: restrict the analysis to a window in milliseconds relative to the
  capture start, e.g. `0-5000`. Either side may be omitted (`500-` or `500` mean "from
  500ms to the end", `-500` means "from the start to 500ms"). Defaults to the whole
  capture (`-`), and is clamped to it.
- `-e`/`--explain`: interleave the text report with paragraphs explaining how to read
  it, in particular what the composition classes, the compositor states and the update
  phases do and do not measure. With `-f json` it prints instead a document shaped like
  the JSON report, each value replaced by what it means.

```
$ webkit-sysprof analyze capture.syscap
...
Compositions:
- compositions: 17708
- average frame rate: 59.13 fps (over the analyzed duration)
- typical frame rate: 60.0 fps, one frame per refresh (median active frame interval 16.665 ms, refresh 16.667 ms)
- active frame interval: (min: 10.9340, P25: 16.6051, median: 16.6650, P75: 16.7270, P99: 17.0330, max: 118.6193) [ms]
- long frame intervals: 4 of 17701 active, 11 missed refreshes
- compositor time:
  - awaiting FrameComplete (display round trip): 93.6%
  - compositing: 4.9%
  - idle: 1.1% (8 stretches, longest 1480.6 ms)
  - unknown: 0.1% (before first message 397.0 ms)
  - scheduled: 0.1%
  - blocked on tiles: 0.0%
- by reasons:
  | class             | reasons                     |     # |  share | duration median |     P99 | flush median | upload median | paint median |
  | -                 | -                           |     - |      - |               - |       - |            - |             - |            - |
  | update-driven     | RenderingUpdate + Animation |    15 |  0.08% |           3.162 |   5.803 |        0.485 |         0.306 |        1.779 |
  | update-driven     | RenderingUpdate             |     9 |  0.05% |          26.555 | 174.149 |       17.510 |        16.431 |        4.876 |
  | compositor-driven | Animation                   | 17684 | 99.86% |           0.786 |   1.046 |        0.005 |         0.000 |        0.593 |
  update-driven: 24, compositor-driven: 17684
- by reason (a composition with several reasons counts toward each): RenderingUpdate 24, Animation 17699

Rendering updates:
- updates: 80, asked for a composition: 24, composited: 24
- update interval: (min: 16.5279, ..., max: 9883.7273) [ms], update rate 0.27 /s (how often the page produced new content, not a frame rate)
- update duration in refresh intervals: (min: 0.009135, ..., max: 22.75)
- update-to-frame latency of the 24 composited updates: (min: 11.2550, ..., max: 500.8069) [ms]
  - update: 31.089 ms median
  - tile wait: 1.248 ms median, in 21 of 24 updates, compositions deferred for tiles: 2
  - behind previous frame (vsync-throttled): 0.682 ms median, in 15 of 24 updates
  - scheduled: 0.035 ms median
  - composition: 3.564 ms median
...

$ webkit-sysprof analyze -f json -t 0-2000 capture.syscap
{"document": {...}, "statistics": {...}, "rendering": {...}, "compositions": {...}, "rendering_updates": {...}}
```

Restricting the timespan matters more than it looks: page load dominates the tail of
most metrics, so a whole-capture run largely measures startup. Skipping the first
seconds is usually what you want when comparing configurations.

The JSON report used to carry a `frame_cycle` section, and `rendering` carried
`frames_rendered`, `theoretical_fps`, `frame_rendering_reasons`,
`frame_compositions_per_vblank_statistics` and `vblanks_per_rendering_update`. They are
gone, not renamed: the `compositions` and `rendering_updates` sections replace them.

#### Reference interval

Frame intervals are measured against the refresh interval, taken from the
`refreshInterval` of `WaylandFramePresented` where the capture holds it, else from the
`DisplayLinkUpdate` marks, else from the median frame interval itself. The report names
the source. The display link only runs while the main thread observes it, so the vblank
count is no count of refreshes.

#### Compositions

A composition is one `RenderLayerTree` that ends in a `DidRenderFrame`, whose message
names the reasons it was composited for: `RenderingUpdate`, `VideoFrame`, `Animation`
and `AsyncScrolling`, in any combination. A composition is update-driven when
`RenderingUpdate` is among them and compositor-driven otherwise, and every combination
of reasons is its own row. The typical frame rate is taken from the median active frame
interval, which leaves out the intervals in which neither the compositor nor the page
worked on the next frame. Idle time counts as such a lull unless a
`LayerTreeHostRenderingUpdate` ran then or was scheduled and waiting for the main
thread, or it lasted under a refresh before one (the wait for the display link). Unknown time counts as a lull too, except stalls and late
timers. Blocked on tiles never does, since new content was held back.

#### Compositor states

The compositor time comes from a replay of `ThreadedCompositor`'s state machine built
from its own marks. Each `ScheduleComposition` and `FrameComplete` message reports the
state, which the replay checks before adopting it, so a change to the compositor shows
up as `replay mismatches` rather than as wrong numbers. `awaiting FrameComplete` is the
display round trip, and normally takes most of the time.

#### Rendering updates

A rendering update is one `LayerTreeHostRenderingUpdate`. The update rate is how often
the page produced new content, not a frame rate. An update that asked for a composition
(`RequestCompositionForRenderingUpdate`) is paired with the first update-driven
composition after its request, and its latency splits into the update itself, the wait
for its tiles, the wait behind an earlier frame, the scheduled time and the composition.
`analyze --explain` prints the long form of all four sections.

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

### `timeline`

Draw what the main thread and the compositor did over time, one cell per slice of
time and one row per `--row-length` of each process. Where `analyze` reports medians,
this shows the individual frames, which is how an update stalled by layout is told
apart from one stalled by its tiles or by the display.

```
webkit-sysprof timeline CAPTURE_FILE [-t TIMESPAN | --frames FIRST-LAST]
                        [--worst frame-interval|update-latency]
                        [--row-length MS] [-n MAX_ROWS] [-r RESOLUTION]
                        [--max-cells CELLS] [--no-tile-lane]
                        [--color auto|always|never] [-e]
```

- `-t`/`--timespan`: the window to draw, as in `analyze`.
- `--frames`: the window from frame `FIRST` to frame `LAST`, or one row around a single
  frame. Frames are numbered from 1 over the whole capture, the same numbers the
  `frame` lane shows.
- `--worst`: draw a row around each of the worst events of the window instead, worst
  first. `frame-interval` takes the longest active frame intervals (see `analyze
  --explain`), `update-latency` the rendering updates slowest to reach the screen. An
  event inside a row already drawn is left out, and the line above each row says what
  the event was.
- `--row-length`: milliseconds per row (default six refresh intervals). A `--worst` row
  is longer where its event is.
- `-n`/`--max-rows`: how many rows to draw (default 40).
- `-r`/`--resolution`: milliseconds per cell (default 1, coarser where a `--worst` row
  would not fit in `--max-cells`).
- `--max-cells`: a row longer than this many cells is truncated and marked with `»`
  (default 400).
- `--no-tile-lane`: drop the `tiles` lane.
- `--color`: `auto` colorizes only when writing to a terminal. Without color each cell
  becomes a letter instead of a colored block, and the legend maps both.
- `-e`/`--explain`: after the timeline, explain its terms: rows, frame numbers, active
  frame intervals, missed refreshes, update lines and their pairings.

```
$ webkit-sysprof timeline capture.syscap -t 4640-4700 --row-length 60 -n 1
                           0    5    10   15   20   25   30   35   40   45   50   55
                           ┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴ [ms]
                                            ^               ^                ^          [refresh]
#1     @  4640.0 ms frame ▕         21                22              23
                     main ▕LLCCCRRuuuFFDDTtttttttttttttttttttBBBLLLLBBLLLLLLLLLSBBLLLLL
                     comp ▕========a!a==============UU#w=============a!a=============aa
                    tiles ▕..............224...........................................
                           3 frames: Animation 2, RenderingUpdate + Animation 1
                           update 14 at 4615.8 ms: frame 22 after 52.092 ms
                           update 15 at 4674.3 ms: frame 26 after 60.787 ms
```

Reading the row: an Animation composition (`a!a`) and its display round trip (`=`)
take the compositor while the main thread finishes a rendering update (`C`, `R`, `F`,
`D`, `T`), which reaches the screen as frame 22: its tile upload (`U`), its frame (`#`)
and the wait for the composition to complete (`w`). The main thread then starts the
next update (`B`, `L`), whose frame, 26, lies past the row.

The `frame` lane numbers each frame (DidRenderFrame) from its cell on. Where a number
does not fit before the next frame, `|` stands in for it. A frame of a rendering update
is drawn in yellow, and one that came a refresh or more late while the compositor was
active in red, or with a `*` without color.

The `main` lane holds the main-thread marks of the process. The `EventLoopRun`,
`WebCoreTimerExecution` and `WebCoreThreadTimers` marks are emitted by any thread
running a run loop, and a capture names the process a mark came from but not the
thread, so those three cannot be told apart from main-thread work. The innermost mark
covering a cell wins, so nested marks are drawn over their parents.

The `comp` lane is the compositor thread. An update-driven composition is drawn from
its `FlushCompositingState`, `SkiaBackingStoreTileUpdate`, `PaintToGLContext` and
`WaitForCompositionCompletion` marks. A compositor-driven one is a single muted glyph by
its leading reason (`a` Animation, `v` VideoFrame, `~` AsyncScrolling, in a brighter
shade where other reasons came along), `n` names no reason and `?` did not record one.
Its frame is `!`, or `#` for the frame of a rendering update. Between compositions the
lane shows the replayed compositor state: `=` awaiting FrameComplete, `-` scheduled,
`:` blocked on tiles, `.` idle and `x` unknown.

The `tiles` lane is **not** the main thread and does not show an activity. It counts how
many tiles are being rasterized at that instant on the painting threads, so `3` means
three `PaintTile` marks overlap there. It answers what the main lane cannot, namely
whether an idle stretch is the engine waiting on parallel work or doing nothing.

Under the lanes, a line counts the frames of the row by their reasons, and one line per
rendering update that began in the row or reached the screen in it says when it began,
in capture time as `-t` takes it, and which frame it reached the screen with.

```
$ webkit-sysprof timeline capture.syscap --worst frame-interval --row-length 60 -n 1 --no-tile-lane
                           0    5    10   15   20   25   30   35   40   45   50   55
                           ┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴┼┴┴┴┴ [ms]
                                            ^               ^                ^          [refresh]
frame 10 came 30.718 ms after frame 9, 1 missed refresh
#1     @  4416.1 ms frame ▕              9                              10*
                     main ▕tttttttttttttttttttttttttttttttttttttSBBBLLLBLLLLSBLLLLLLCCC
                     comp ▕UUUUUUUUUUPPPP#w================aaaaaaaaaaaaa!aaaaaaaa======
                           2 frames: Animation 1, RenderingUpdate 1
                           update 10 at 4373.1 ms: frame 9 after 57.636 ms
                           update 11 at 4453.5 ms: frame 12 after 50.414 ms
```

Here the longest active frame interval of the capture: frame 10, an Animation
composition, took over 20 ms to composite, so it missed a refresh.

The `^` ticks under the ruler step one refresh interval from the row begin. They are
not phase-aligned with the display's vblanks, so they say how many refreshes a stretch
lasted, not which one a frame made. Where a refresh interval is only a few cells wide,
the header says so and no tick is drawn.

A row only ever holds one process, so where more than one rendered, each row names the
process it belongs to and is drawn from that process's marks alone.

The legend lists the activities present, the average time each took per drawn row, and
the marks it is drawn from. The average covers the whole of every row drawn, including
whatever part of a truncated one did not fit in its bar. Several activities aggregate
more than one mark, so that list is what says exactly what a color covers. It also
makes clear that `idle` is the absence of any mark rather than a mark of its own.

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
