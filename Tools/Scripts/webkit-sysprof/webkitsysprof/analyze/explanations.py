"""What `analyze --explain` and `timeline --explain` print, kept apart from the
analysis itself.

TEXT_* constants back the text format: the paragraphs printed alongside the
report or the timeline, the long form of what the README keeps short.

JSON_* constants back the json format: `JSON_EXPLAINED_REPORT` is a tree shaped
like the JSON report, with every value replaced by a sentence of prose. It
describes the report format, not any capture, so it holds no figure from one.
"""

from typing import Any, Dict


from ..compositions import (
    AWAITING_FRAME_COMPLETE,
    CLASSES,
    BLOCKED_ON_TILES,
    COMPOSITING,
    COMPOSITOR_CLASS,
    GAP,
    IDLE,
    NO_REASON_CLASS,
    SCHEDULED,
    UNKNOWN,
    STATES,
    UNRECORDED_CLASS,
    UPDATE_CLASS,
)
from ..updates import (
    ANOMALY,
    APPROXIMATE,
    BEHIND_PREVIOUS_FRAME,
    COMPOSITION,
    OTHER,
    SCHEDULED_PHASE,
    EXACT,
    PAIRING_REASONS,
    TILE_WAIT,
    UPDATE,
    UPDATE_PHASES,
)

# --- Text format -------------------------------------------------------


TEXT_REFERENCE_INTERVAL = """\
  Frame intervals are measured against the refresh interval, taken from
  WaylandFramePresented, else the median DisplayLinkUpdate interval, else
  the median frame interval. Source here: {source}.
    - refresh interval: how often the display can show a new frame.
    - discarded frames: WaylandFrameDiscarded marks, frames the compositor
      sent that the display never showed.
    - vblanks: DisplayLinkUpdate marks. The display link only runs while
      the main thread observes it, so vblanks do not count refreshes."""

TEXT_COMPOSITIONS = """\
  A composition is a RenderLayerTree ending in a DidRenderFrame, which
  names its reasons. Each one is a composited frame, numbered from 1 over
  the whole capture, as timeline shows them. Pending reasons all go to the
  next composition, except RenderingUpdate, held back while its tiles are
  painted.
    - RenderingUpdate: the main thread asked to show a rendering update.
    - Animation: while an accelerated animation runs, each frame's paint
      requests the next, so it composites every refresh.
    - VideoFrame: a video produced a frame.
    - AsyncScrolling: the scrolling thread moved a layer.
  How to read the lines:
    - frame interval: the time from one DidRenderFrame to the next in the
      same process.
    - active frame interval: a frame interval in which the compositor or
      the page worked on the next frame all along. Idle while a
      LayerTreeHostRenderingUpdate runs, or while one is scheduled and
      waits for the main thread, awaited the page, and up to a refresh of
      idle before then awaited the display link, so these count as
      active, as does blocked on tiles. Other idle or unknown time makes
      it inactive, except a stall, awaiting FrameComplete over ten
      refreshes, and a composition timer late by over a refresh
      (suspended?), as both are jank. Inactive intervals only say the page
      had nothing to show.
    - average frame rate: compositions divided by the analyzed duration,
      idle included.
    - typical frame rate: 1000 divided by the median active frame
      interval in ms, and how many refreshes that frame interval spans.
    - missed refreshes: round(interval / refresh interval) - 1 for an
      active frame interval, the refreshes that showed no new frame.
    - long frame intervals: active frame intervals that missed at least
      one refresh, and the missed refreshes they add up to.
    - compositor time: the share of each compositor state, see below.
    - class: update-driven (has RenderingUpdate, so it shows main thread
      work), compositor-driven (other reasons only, the main thread was not
      involved), no reason (RenderingUpdate held back, nothing else
      pending), unrecorded (capture predates reasons).
    - by reasons: one row per combination of reasons since costs differ,
      Animation alone skips FlushCompositingState. Duration is the
      RenderLayerTree, flush FlushCompositingState, upload
      SkiaBackingStoreTileUpdate and paint PaintToGLContext, in ms.
    - by reason: counts a composition once per reason it names.
    - aborted: a RenderLayerTree without DidRenderFrame, when suspended,
      invalidated or with an empty viewport."""

TEXT_COMPOSITOR_STATES = """\
  Compositor time replays ThreadedCompositor's state machine. Each
  ScheduleComposition and FrameComplete reports the state, reasons and
  tile wait, checked against the replay. A mismatch means the marks
  contradict the state machine.
    - compositing: inside a RenderLayerTree.
    - awaiting FrameComplete: the display round trip, paced by the
      display. Normally most of the time.
    - scheduled: the zero-delay composition timer is armed.
    - blocked on tiles: only a rendering update is pending, and its tiles
      are still being painted.
    - idle: nothing wants a frame.
    - unknown: before the first message, after a mismatch, abort or
      invalidation, while suspended (no mark), scheduled over a refresh
      (suspended?), or awaiting FrameComplete over ten refreshes (stalled).
  Old captures lack state messages: only compositing is known, and gaps
  over three refreshes count as idle."""

TEXT_RENDERING_UPDATES = """\
  A rendering update is a LayerTreeHostRenderingUpdate, the main thread
  turning DOM changes into a new layer tree. Updates are numbered from 1
  over the whole capture, as timeline shows them.
    - asked for a composition: the update ran a
      RequestCompositionForRenderingUpdate, so it had something to show.
    - composited: paired with the frame that showed it. It pairs with the
      first unclaimed update composition whose DidRenderFrame follows the
      request and whose RenderLayerTree began before the next request.
      Otherwise it is uncomposited, or pending if no update came after it.
    - update interval: the time from one update begin to the next. The
      update rate is how often content changed, not a frame rate.
    - update duration in refresh intervals: how many refreshes one update
      took. Above 1, it could not fit between two refreshes.
    - update-to-frame latency: from update begin to the DidRenderFrame of
      its frame, split into phases that add up to it:
        - update: until the request.
        - tile wait: until its tiles are painted. Compositions meanwhile
          hold RenderingUpdate back and count as deferred for tiles.
        - behind previous frame: an earlier frame and its round trip held
          the compositor (vsync throttling).
        - scheduled: the composition timer was armed.
        - other: unexplained by the compositor states.
        - composition: RenderLayerTree begin, or the request if later, to
          DidRenderFrame.
  A pairing is exact, approximate (an old capture, using the update end as
  request and any composition as frame) or an anomaly (the marks
  contradict what LayerTreeHost and ThreadedCompositor allow)."""

TEXT_MARK_STATISTICS = """\
  StyleRecalc wraps RenderTreeBuild, the CompositingUpdate after a style
  change, and interleaved layout. Subtract them to get style resolution.
  CompositingUpdate runs after style changes, layout and scrolling, so its
  percentiles mix very different runs. PaintTile runs on the painting
  threads, so its sum is aggregate work, not elapsed time. Percentiles
  interpolate between samples, stay within min and max, and read - below
  two samples."""

TEXT_TIMELINE = """\
  How to read the timeline:
    - row: a slice of time in one process. Its label gives the row number
      and the capture time it begins at, in ms as -t takes it. The ruler
      counts ms from the row begin.
    - [refresh] ticks: one refresh interval apart from the row begin, not
      vblanks, which only tick while the main thread observes them.
    - frame lane: each composited frame (a RenderLayerTree ending in a
      DidRenderFrame), numbered from 1 over the whole capture as --frames
      takes them, at the cell of its DidRenderFrame.
    - frame interval: the time from one frame to the next in the process.
      It is active when the compositor or the page worked on the next
      frame all along, see analyze -e. Idle during a rendering update or
      while one waits for the main thread, idle under a refresh before,
      blocked on tiles, a stall and a late composition timer (suspended?)
      all count as active.
    - missed refreshes: round(interval / refresh interval) - 1 for an
      active frame interval. A late frame missed at least one, and its
      number is red, or followed by * without color.
    - main lane: the main thread, by the innermost mark in each cell.
    - comp lane: the compositor thread, compositions and the states of
      the compositor between them (analyze -e explains the states).
    - update N at T ms: rendering update N, numbered from 1 over the
      capture, began at capture time T. A row lists the updates that began
      in it or reached the screen in it.
    - frame M after L ms: the update was shown by frame M, L ms after it
      began (its update-to-frame latency). A pairing that is not exact
      says why: approximate (old_capture, an old capture without requests)
      or an anomaly (rlt_before_request, invariant_breach,
      unexpected_composition: the marks contradict what LayerTreeHost and
      ThreadedCompositor allow).
    - never composited: no frame showed the update. Not composited before
      the capture ended: the capture stopped first. Asked for no frame: the
      update ran no RequestCompositionForRenderingUpdate.
    - --worst frame-interval: rows around the longest active frame
      intervals, longest first. --worst update-latency: rows around the
      updates with the longest update-to-frame latency."""

# How the report names a composition class, keyed as compositions.classify() does.
CLASS_LABELS = {
    UPDATE_CLASS: "update-driven",
    COMPOSITOR_CLASS: "compositor-driven",
    NO_REASON_CLASS: "no reason",
    UNRECORDED_CLASS: "unrecorded",
}

# How the report names a compositor state segment.
STATE_LABELS = {
    COMPOSITING: "compositing",
    AWAITING_FRAME_COMPLETE: "awaiting FrameComplete (display round trip)",
    SCHEDULED: "scheduled",
    BLOCKED_ON_TILES: "blocked on tiles",
    IDLE: "idle",
    UNKNOWN: "unknown",
    GAP: "gap",
}

# How the report names a phase of the update-to-frame latency.
PHASE_LABELS = {
    UPDATE: "update",
    TILE_WAIT: "tile wait",
    BEHIND_PREVIOUS_FRAME: "behind previous frame (vsync-throttled)",
    SCHEDULED_PHASE: "scheduled",
    OTHER: "other",
    COMPOSITION: "composition",
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
    "WaitForCompositionCompletion": ("Waiting for an in-flight composition to finish"),
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

# What a composition state is, keyed as compositions.STATES names them.
JSON_STATE_DESCRIPTIONS = {
    COMPOSITING: "inside a RenderLayerTree",
    AWAITING_FRAME_COMPLETE: "the display round trip after a frame",
    SCHEDULED: "the zero-delay composition timer is armed",
    BLOCKED_ON_TILES: (
        "only a rendering update is pending, and its tiles are still being painted"
    ),
    IDLE: "nothing wants a frame",
    UNKNOWN: (
        "the state cannot be told (before the first message, after a mismatch,"
        " abort or invalidation, suspended, or stalled)"
    ),
    GAP: "between compositions of an old capture, under three refreshes",
}

# What a phase of the update-to-frame latency covers, keyed as UPDATE_PHASES.
JSON_PHASE_DESCRIPTIONS = {
    UPDATE: "From the update begin to its RequestCompositionForRenderingUpdate",
    TILE_WAIT: "Waiting for the update's tiles to be painted",
    BEHIND_PREVIOUS_FRAME: (
        "An earlier frame and its round trip held the compositor (vsync throttling)"
    ),
    SCHEDULED_PHASE: "The composition timer was armed",
    OTHER: "Not explained by the compositor states",
    COMPOSITION: (
        "From the frame's RenderLayerTree begin, or the request if later, to"
        " its DidRenderFrame"
    ),
}


def _json_statistic(what: str, unit: str = "") -> str:
    """What a statistics dict (n/min/max/mean/stddev/median/percentiles)
    summarizes, without expanding those keys one by one."""
    return "{}{}.".format(what, " [{}]".format(unit) if unit else "")


def _json_per_state(what: str) -> Dict[str, str]:
    return {
        state: "{} {}, {}.".format(what, state, JSON_STATE_DESCRIPTIONS[state])
        for state in STATES
    }


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
    "vblanks": "Number of DisplayLinkUpdate marks in the analyzed timespan. The"
    " display link only runs while the main thread observes it, so this does"
    " not count refreshes.",
    "vblank_interval_statistics": _json_statistic(
        "Time between two consecutive DisplayLinkUpdate marks", "ms"
    ),
    "reference_interval": "The refresh interval frame intervals are measured"
    " against [ms]. Null when none is known.",
    "reference_interval_source": "Where the refresh interval comes from:"
    ' "WaylandFramePresented", else "DisplayLinkUpdate" (their median'
    ' interval), else "frame intervals" (the median frame interval).',
    "reference_interval_marks": "Number of marks the refresh interval is"
    " taken from.",
    "discarded_frames": "Number of WaylandFrameDiscarded marks, frames the"
    " display never showed.",
}

_JSON_COMPOSITION_PROCESS: Dict[str, Any] = {
    "compositions": "Compositions of this process in the analyzed timespan.",
    "aborted": "RenderLayerTree marks without a DidRenderFrame (suspended,"
    " invalidated or with an empty viewport).",
    "typical_frame_rate": "1000 divided by the median active frame interval,"
    " in frames per second. Null without an active frame interval.",
    "frames_per_refresh": "Refresh intervals the median active frame interval"
    " spans. Null without either.",
    "average_frame_rate": "Compositions divided by the analyzed duration, idle"
    " time included, in frames per second.",
    "active_interval_statistics": _json_statistic(
        "Active frame intervals, from one DidRenderFrame to the next with no"
        " idle or unknown time in between, except idle while a rendering update"
        " runs or waits for the main thread or up to a refresh before, stalls"
        " and late timers",
        "ms",
    ),
    "long_intervals": "Active frame intervals that missed at least one refresh.",
    "missed_refreshes": "Sum over the active frame intervals of"
    " round(interval / refresh interval) - 1.",
    "state_time": _json_per_state("Time [ms] in state"),
    "state_shares": _json_per_state("Share of the analyzed duration in state"),
    "state_longest": _json_per_state("Longest stretch [ms] in state"),
    "unknown_by_detail": "Time [ms] in the unknown state, keyed by why it is"
    ' unknown ("before first message", "mismatch", "suspended",'
    ' "suspended?", "aborted", "stalled", "invalidated").',
    "idle_stretches": {
        "n": "Number of idle stretches.",
        "total": "Their total time [ms].",
        "max": "The longest [ms]. Null without any.",
    },
    "replay_messages": "ScheduleComposition and FrameComplete marks the replay"
    " of ThreadedCompositor's state machine read.",
    "replay_mismatches": "Messages that contradicted the replay.",
    "replay_mismatches_by_kind": "The mismatches, keyed by the message and"
    " field that contradicted the replay.",
    "suspended_stretches": "Stretches in which the compositor was suspended.",
    "probably_suspended_stretches": "Stretches in which the composition timer"
    " was armed for over a refresh (suspended?).",
    "stalled": "Stretches awaiting FrameComplete for over ten refreshes.",
    "idle_fallback": "True when the capture has no state messages, so only"
    " compositing is known and gaps over three refreshes count as idle.",
}

_JSON_COMPOSITIONS_SECTION: Dict[str, Any] = {
    "compositions": "Number of compositions (a RenderLayerTree ending in a"
    " DidRenderFrame) in the analyzed timespan.",
    "capture_compositions": "Number of compositions in the whole capture.",
    "processes": "Number of processes that composited.",
    "by_combination": [
        {
            "class": "The class of the combination ({}).".format(
                ", ".join('"{}"'.format(klass) for klass in CLASSES)
            ),
            "reasons": "The reasons DidRenderFrame names, in canonical order.",
            "combination": 'The reasons joined by " + ".',
            "n": "Number of compositions with exactly these reasons.",
            "share": "Their share of all compositions.",
            "duration_statistics": _json_statistic("RenderLayerTree", "ms"),
            "flush_statistics": _json_statistic("FlushCompositingState", "ms"),
            "upload_statistics": _json_statistic(
                "SkiaBackingStoreTileUpdate within the composition", "ms"
            ),
            "paint_statistics": _json_statistic("PaintToGLContext", "ms"),
        }
    ],
    "by_reason": "Compositions keyed by reason, counting a composition once"
    " for each reason it names.",
    "by_class": 'Compositions keyed by class: "update" (names'
    ' RenderingUpdate), "compositor" (other reasons only), "no_reason"'
    ' (RenderingUpdate held back, nothing else pending), "unrecorded"'
    " (capture predates reasons).",
    "per_process": {"<pid>": _JSON_COMPOSITION_PROCESS},
}

_JSON_RENDERING_UPDATE_PROCESS: Dict[str, Any] = {
    "updates": "LayerTreeHostRenderingUpdate marks of this process.",
    "asked": "Updates that ran a RequestCompositionForRenderingUpdate.",
    "composited": "Updates paired with the frame that showed them.",
    "uncomposited": "Updates that asked for a frame no composition showed.",
    "pending": "Updates that asked for a frame the capture ended before.",
    "pairing": {
        EXACT: "Pairings the marks fully support.",
        APPROXIMATE: "Pairings from an old capture, using the update end as"
        " request and any composition as frame.",
        ANOMALY: "Pairings the marks contradict.",
        "reasons": "Pairings that are not exact, keyed by why ({}).".format(
            ", ".join('"{}"'.format(reason) for reason in PAIRING_REASONS)
        ),
    },
    "update_interval_statistics": _json_statistic(
        "Time from one update begin to the next", "ms"
    ),
    "update_rate": "Updates per second, how often the content changed, not a"
    " frame rate.",
    "duration_statistics": _json_statistic("Duration of an update", "ms"),
    "refresh_intervals_statistics": _json_statistic(
        "Duration of an update in refresh intervals"
    ),
    "latency_statistics": _json_statistic(
        "Update-to-frame latency, from the update begin to the DidRenderFrame"
        " that showed it",
        "ms",
    ),
    "phase_statistics": {
        phase: _json_statistic(JSON_PHASE_DESCRIPTIONS[phase], "ms")
        for phase in UPDATE_PHASES
    },
    "phase_present": {
        phase: "Composited updates whose latency has some of this phase."
        for phase in UPDATE_PHASES
    },
    "deferred_for_tiles": "Compositions that held RenderingUpdate back while"
    " the update's tiles were painted.",
    "deferred_for_tiles_statistics": _json_statistic(
        "Such compositions per composited update"
    ),
}

_JSON_RENDERING_UPDATES_SECTION: Dict[str, Any] = {
    "updates": "Number of rendering updates (LayerTreeHostRenderingUpdate) in"
    " the analyzed timespan.",
    "processes": "Number of processes that ran an update.",
    "old_capture": "True when the capture predates"
    " RequestCompositionForRenderingUpdate, so pairings are approximate.",
    "per_process": {"<pid>": _JSON_RENDERING_UPDATE_PROCESS},
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
            "end": "End of the analyzed window [ms] from the" " start of the capture.",
        },
    },
    "statistics": _JSON_STATISTICS_SECTION,
    "rendering": _JSON_RENDERING_SECTION,
    "compositions": _JSON_COMPOSITIONS_SECTION,
    "rendering_updates": _JSON_RENDERING_UPDATES_SECTION,
}
