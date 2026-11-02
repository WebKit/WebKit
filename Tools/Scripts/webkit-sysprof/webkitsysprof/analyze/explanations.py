"""What `analyze --explain` prints, kept apart from the analysis itself.

TEXT_* constants back the text format: paragraphs of prose `_render_text_report`
prints alongside the report; the long form of what it means, where the README
keeps the same caveats short.

JSON_* constants back the json format: `JSON_EXPLAINED_REPORT` is a tree shaped
like the JSON report, with every value replaced by a sentence of prose. It is a
fixed, capture-independent document describing the report format itself, not
any particular capture, so it holds no figure taken from one.
"""

from typing import Any, Dict

# --- Text format -------------------------------------------------------

TEXT_THEORETICAL_FPS = """\
  Theoretical FPS is the number of DidRenderFrame marks, one per
  composition, divided by the analyzed duration:
  {frames} / {duration:.4f} s = {fps}. It is not clamped to the refresh
  rate, and it counts every process, so two renderers compositing once
  per refresh read as two.
{refresh_rate}
  Matching it means completing one frame per vblank interval. The frame
  cycle below shows where a frame's time goes."""

TEXT_UNKNOWN_THEORETICAL_FPS = """\
  Theoretical FPS is the number of DidRenderFrame marks, one per
  composition, divided by the analyzed duration. It is unknown here: the
  analyzed timespan has no length to divide the {frames} of them by.
{refresh_rate}"""

TEXT_REFRESH_RATE = """\
  That rate is {rate:.2f} Hz here, from the median vblank interval of
  {interval:.4f} ms."""

TEXT_REFRESH_RATE_WITHOUT_INTERVAL = """\
  That rate is unknown here: the display link refreshed {vblanks} time{plural},
  too few for an interval between two refreshes."""

TEXT_REFRESH_RATE_ZERO_INTERVAL = """\
  That rate is unknown here: the median interval between two of the {vblanks}
  refreshes is zero."""

TEXT_FRAME_CYCLE = """\
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


TEXT_MARK_STATISTICS = """\
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
TEXT_UNNAMED_FRAME_RENDERING_REASONS = {
    "_none": "none given",
}

# What the report says of a median cycle it cannot be given in vblank intervals,
# keyed by the token _cycle_in_vblank_intervals() reports it with. Shared by
# the text report, which names just the one token that applied, and the JSON
# explanation below, which lists every possible one since it is
# capture-independent and cannot know which applied.
UNKNOWN_VBLANK_INTERVALS_PER_CYCLE = {
    "no_cycles": "there are no cycles",
    "no_vblank_interval": "no vblank interval known",
    "zero_vblank_interval": "the vblank interval is zero",
    "zero_length_cycle": "the median cycle is of zero length",
}


# --- JSON format ---------------------------------------------------------
#
# Backs JSON_EXPLAINED_REPORT below: a report-shaped tree where every value is
# replaced by a sentence of prose, for tools that read the JSON report (e.g. a
# dashboard) and want the explanation delivered the same way the data is.

# What each mark relevant to the "Statistics (durations)"/"(other)" tables
# in the text report measures, keyed by mark name. Its keys are the marks the
# "statistics" section of JSON_EXPLAINED_REPORT covers, and must track
# MARKS_RELEVANT_FOR_STATISTICS in analyze/__init__.py.
JSON_MARK_DESCRIPTIONS = {
    "EventLoopRun": (
        "Iteration of the WebContent process's run loop"
        " (JS callbacks, timers, IPC dispatch)"
    ),
    "RAFCallback": "requestAnimationFrame callbacks execution phase of a frame",
    "LayerTreeHostRenderingUpdate": (
        "Rendering update phase of a frame: requestAnimationFrame callbacks,"
        " style recalc, layout and the compositing update"
    ),
    "FinalizeRenderingUpdate": (
        "Finalizing a rendering update once the layer tree has been updated"
    ),
    "PerformLayout": "Laying out the single render (sub)tree",
    "PerformSubtreesLayout": "Laying out one or more render tree subtrees",
    "StyleRecalc": (
        "Style recalculation; an umbrella mark that also contains"
        " RenderTreeBuild, the CompositingUpdate that follows a style change,"
        " and any layout that interleaves it, so its duration is not style"
        " resolution alone"
    ),
    "RenderTreeBuild": "Building/updating the render tree after style resolution",
    "CompositingUpdate": (
        "Updating the compositing layer tree; emitted after a style change,"
        " after layout, or on scrolling, so its percentiles mix runs of very"
        " different lengths rather than centering on a typical value"
    ),
    "FlushCompositingState": (
        "Flushing the computed compositing state to the layer tree host"
    ),
    "RenderLayerTree": (
        "Rendering the layer tree into a composited frame; on GTK/WPE this"
        " also covers waiting for the GPU and the buffer handover after it"
    ),
    "PaintToGLContext": "Painting composited content into the GL context",
    "WaitForCompositionCompletion": (
        "Waiting for an in-flight composition to finish"
    ),
    "RecordTile": "Recording a tile's paint commands",
    "UpdateTiles": "Updating a batch of dirty tiles",
    "PaintTile": "Painting a single tile",
    "UpdateTile": "Updating a single tile's backing store",
    "SkiaBackingStoreTileUpdate": "Updating a tile's Skia backing store",
}

# The extra, non-duration metric some marks report, keyed by (mark, data key).
JSON_EXTRA_METRIC_DESCRIPTIONS = {
    ("EventLoopRun", "tasks"): (
        "Tasks (IPC messages, timers, etc.) before run loop iteration start"
    ),
    ("PerformSubtreesLayout", "subtrees"): "Subtrees laid out",
    ("UpdateTiles", "tiles"): "Dirty tiles updated",
    ("PaintTile", "dirty_pixels"): (
        "Area, in pixels, of the tile's repainted dirty region"
    ),
}

# The four frame cycle phases, keyed as PHASES (webkitsysprof.cycles) names them.
JSON_PHASE_DESCRIPTIONS = {
    "rendering_update": (
        "The LayerTreeHostRenderingUpdate mark itself: requestAnimationFrame"
        " callbacks, style recalc, layout and the compositing update"
    ),
    "compositing": (
        "Time after the rendering update spent in RenderLayerTree,"
        " FlushCompositingState, PaintToGLContext or"
        " WaitForCompositionCompletion, merged as spans rather than summed"
        " since they can nest"
    ),
    "waiting_for_compositing": (
        "What is left of the cycle once the other phases are taken off it,"
        " mostly spent waiting for the painting threads to finish their tiles"
    ),
    "idle": "From the end of compositing until the next rendering update begins",
}


def _json_statistic(what: str, unit: str = "") -> str:
    """What a statistics dict (n/min/max/mean/stddev/median/percentiles)
    summarizes, without expanding those keys one by one."""
    return "{}{}.".format(what, " [{}]".format(unit) if unit else "")


_JSON_STATISTICS_SECTION: Dict[str, Any] = {
    mark_name: {
        "duration": _json_statistic(description, "ms"),
        **{
            data_key: _json_statistic(extra_description)
            for (mark, data_key), extra_description in (
                JSON_EXTRA_METRIC_DESCRIPTIONS.items()
            )
            if mark == mark_name
        },
    }
    for mark_name, description in JSON_MARK_DESCRIPTIONS.items()
}

_JSON_RENDERING_SECTION: Dict[str, Any] = {
    "vblanks": "Number of display refreshes (vblanks) observed in the"
    " analyzed timespan.",
    "vblank_interval_statistics": _json_statistic(
        "Time between two consecutive vblanks, i.e. the refresh interval", "ms"
    ),
    "frames_rendered": "Number of DidRenderFrame marks, i.e. completed"
    " compositions, in the analyzed timespan.",
    "theoretical_fps": "DidRenderFrame marks divided by the analyzed"
    " duration, in frames per second. Not clamped to the refresh rate, and"
    " counts every process, so two renderers compositing once per refresh"
    " read as two. Null when the analyzed duration has no length to divide"
    " the marks by.",
    "frame_rendering_reasons": "Number of composited frames, keyed by the"
    " reason the DidRenderFrame mark's message gives for compositing (or"
    " \"_none\" when it gives none).",
    "frame_compositions_per_vblank_statistics": _json_statistic(
        "Frame compositions finished within each interval between two"
        " consecutive vblanks"
    ),
    "vblanks_per_rendering_update": {
        "n_greater_than_1": "Number of rendering updates that spanned more"
        " than one vblank, i.e. missed a refresh deadline.",
        "statistics": _json_statistic(
            "Display refreshes spanned by each LayerTreeHostRenderingUpdate"
        ),
    },
}

_JSON_FRAME_CYCLE_SECTION: Dict[str, Any] = {
    "cycles": "Number of frame cycles found (one LayerTreeHostRenderingUpdate"
    " to the next) in the analyzed timespan.",
    "processes": "Number of distinct processes that rendered a cycle.",
    "duration_statistics": _json_statistic("Duration of a frame cycle", "ms"),
    "implied_fps": "1000 divided by the median cycle duration, in frames per"
    " second. Null where there are no cycles.",
    "coverage": "Share of the analyzed duration the cycles' spans cover,"
    " merged rather than summed so overlapping cycles of different"
    " processes are not double-counted. Null when the analyzed duration has"
    " no length to divide by.",
    "capture_vblank_interval": "Median vblank interval over the whole"
    " capture, not just the analyzed window [ms]. Null when it"
    " cannot be computed.",
    "vblank_intervals_per_cycle": "Median cycle duration expressed in"
    " vblank intervals (capture_vblank_interval). Null when it cannot be"
    " computed; vblank_intervals_per_cycle_unknown then names why.",
    "vblank_intervals_per_cycle_unknown": "Why vblank_intervals_per_cycle"
    " could not be computed ({}); null when it could.".format(
        ", ".join(
            "\"{}\" ({})".format(token, reason)
            for token, reason in UNKNOWN_VBLANK_INTERVALS_PER_CYCLE.items()
        )
    ),
    "phase_statistics": {
        phase: _json_statistic(description, "ms")
        for phase, description in JSON_PHASE_DESCRIPTIONS.items()
    },
    "resolved_duration_statistics": _json_statistic(
        "Duration of cycles whose phases could be resolved, i.e. the ones"
        " the phase shares are taken from",
        "ms",
    ),
    "overrunning_composition_statistics": _json_statistic(
        "Among resolved cycles, how far compositing ran past the end of its"
        " cycle",
        "ms",
    ),
}

# The JSON report's shape, with every value replaced by what it means. A
# constant: it does not depend on, and is not built from, any capture.
JSON_EXPLAINED_REPORT: Dict[str, Any] = {
    "document": {
        "title": "Name of the analyzed capture file.",
        "subtitle": "When the capture was recorded.",
        "timespan": {
            "begin": "Start of the analyzed window [ms] from"
            " the start of the capture.",
            "end": "End of the analyzed window [ms] from the"
            " start of the capture.",
        },
    },
    "statistics": _JSON_STATISTICS_SECTION,
    "rendering": _JSON_RENDERING_SECTION,
    "frame_cycle": _JSON_FRAME_CYCLE_SECTION,
}
