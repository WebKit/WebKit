"""The paragraphs `analyze --explain` prints, kept apart from the analysis itself.

The long form of what the report means. The README keeps the same caveats short.
"""

THEORETICAL_FPS = """\
  Theoretical FPS is the number of DidRenderFrame marks, one per
  composition, divided by the analyzed duration:
  {frames} / {duration:.4f} s = {fps}. It is not clamped to the refresh
  rate, and it counts every process, so two renderers compositing once
  per refresh read as two.
{refresh_rate}
  Matching it means completing one frame per vblank interval. The frame
  cycle below shows where a frame's time goes."""

UNKNOWN_THEORETICAL_FPS = """\
  Theoretical FPS is the number of DidRenderFrame marks, one per
  composition, divided by the analyzed duration. It is unknown here: the
  analyzed timespan has no length to divide the {frames} of them by.
{refresh_rate}"""

REFRESH_RATE = """\
  That rate is {rate:.2f} Hz here, from the median vblank interval of
  {interval:.4f} ms."""

REFRESH_RATE_WITHOUT_INTERVAL = """\
  That rate is unknown here: the display link refreshed {vblanks} time{plural},
  too few for an interval between two refreshes."""

REFRESH_RATE_ZERO_INTERVAL = """\
  That rate is unknown here: the median interval between two of the {vblanks}
  refreshes is zero."""

FRAME_CYCLE = """\
  A frame cycle starts when a LayerTreeHostRenderingUpdate begins and
  ends when the next one begins, so consecutive cycles tile a rendering
  period without gaps and the frame rate within it is 1 / cycle
  duration. The phases are:
    - rendering update: the LayerTreeHostRenderingUpdate mark itself,
      i.e. requestAnimationFrame callbacks, style recalc, layout and the
      compositing update.
    - compositing: the time after the rendering update spent in
      RenderLayerTree, FlushCompositingState, PaintToGLContext or
      WaitForCompositionCompletion. They are merged as spans rather than
      added up, since they nest and a cycle can composite twice with a
      wait in between. On GTK and WPE the wait for the GPU and the buffer
      handover after it both happen inside RenderLayerTree, so getting the
      frame out is counted here too.
    - waiting for compositing: what is left of the cycle once the other
      phases are taken off it, mostly spent waiting for the painting
      threads to finish their tiles.
    - idle: from the end of compositing until the next rendering update
      begins.
  Each phase is its own median over the cycles it could be read from, so
  the shares need not total 100%.
  The loop is vblank-driven, but it does not wait for a refresh it has
  already missed: a cycle that runs past its vblank deadline schedules
  the next rendering update as soon as compositing is far enough along
  rather than at the following refresh, so a cycle longer than one vblank
  interval means frames cannot be presented on every vblank. Compositing
  that began in one cycle can then still be running when the next
  rendering update opens the following one, and since the phases add up
  to the cycle exactly, that part of it past the cycle end is reported on
  its own as composition overrunning the cycle rather than as a fifth
  phase.
  Implied FPS is the rate of the median cycle, while theoretical FPS
  averages over the whole analyzed duration, so the two differ by the
  time no cycle spans, which coverage reports, plus the time the cycles
  spent idle. A capture that renders for a stretch and then sits still,
  rather than rendering evenly throughout, keeps a high coverage, since
  the pause is itself one long cycle, and a high idle share along with
  it.
  A cycle reaching past either end of the analyzed timespan is left out
  of it. Between two rendering periods the engine idles, and that gap is
  reported as one long, nearly all-idle cycle rather than dropped.
  A cycle only ever runs between two rendering updates of one process.
  Where more than one rendered, their cycles are reported together and
  the count above says so. The refreshes come from the UI process and
  name no display, so a capture of two displays refreshing at once halves
  the interval between them and doubles the rate derived from it."""


MARK_STATISTICS = """\
  StyleRecalc is an umbrella mark: RenderTreeBuild, the CompositingUpdate
  that follows a style change, and layout where style resolution
  interleaves it all run inside it, so its duration is not style
  resolution alone. Subtract the nested marks to get that.
  CompositingUpdate is emitted after a style change, after layout and on
  scrolling, and those runs are of very different lengths, so its
  percentiles mix them rather than centering on a typical value.
  PaintTile runs on the painting threads, so summing it across a frame
  yields aggregate work rather than elapsed time.
  Percentiles are interpolated between the samples, so they lie between
  the minimum and the maximum, and read - below two samples."""


# What the report says of a frame whose message named no reason for compositing it,
# keyed by the token _frame_rendering_reason() buckets it under.
UNNAMED_FRAME_RENDERING_REASONS = {
    "_none": "none given",
}

# What the report says of a median cycle it cannot be given in vblank intervals,
# keyed by the token _cycle_in_vblank_intervals() reports it with.
UNKNOWN_VBLANK_INTERVALS_PER_CYCLE = {
    "no_cycles": "there are no cycles",
    "no_vblank_interval": "no vblank interval known",
    "zero_vblank_interval": "the vblank interval is zero",
    "zero_length_cycle": "the median cycle is of zero length",
}
