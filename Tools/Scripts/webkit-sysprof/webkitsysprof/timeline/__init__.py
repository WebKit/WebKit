"""Swimlanes of what the main thread and the compositor did over time.

Each row is a slice of time with a lane per thread role, the frames numbered over
the whole capture, and the rendering updates that reached the screen in it.
"""

import argparse
import bisect
import collections
import logging
import math
import sys
from typing import Any, Dict, Iterable, List, Optional, Sequence, Set, Tuple

from ..compositions import (
    AWAITING_FRAME_COMPLETE,
    BLOCKED_ON_TILES,
    COMPOSITING,
    COMPOSITOR_CLASS,
    GAP,
    IDLE,
    NO_REASON_CLASS,
    SCHEDULED,
    UNKNOWN,
    UNRECORDED_CLASS,
    UPDATE_CLASS,
    CompositionModel,
    calculate_compositions,
    indices_by_pid,
    segments_overlapping,
)
from ..analyze import explanations
from ..parser import parse
from ..updates import (
    COMPOSITED,
    EXACT,
    PENDING,
    RENDERING_UPDATE_MARK,
    UNCOMPOSITED,
    calculate_rendering_updates,
)
from ..utils import (
    clamped_timespan,
    MarkIndex,
    UsageError,
    check_timespan_holds_data,
    mark_begin,
    mark_index_by_process,
    marks_by_process,
    msec_to_nsec,
    nsec_to_msec,
    parse_timespan_argument,
    sysprof_data_with_marks_by_name,
)

# Marks drawn in the main lane, by activity. Unlisted marks are ignored, uncovered
# cells are idle. The lane is painted longest-first, so the innermost mark shows.
MAIN_THREAD_ACTIVITIES = {
    "RAFCallback": "js",
    "StyleRecalc": "style",
    "RenderTreeBuild": "build",
    "PerformSubtreesLayout": "layout",
    "PerformLayout": "layout",
    "RenderTreeLayout": "layout",
    "CompositingUpdate": "compositing_update",
    "FinalizeRenderingUpdate": "finalize",
    "UpdateTiles": "tiles",
    "UpdateTile": "tiles",
    "RecordTile": "record",
    "LayerTreeHostRenderingUpdate": "update",
    "RenderingUpdate": "update",
    "EventLoopRun": "timers",
    "WebCoreTimerExecution": "timers",
    "WebCoreThreadTimers": "timers",
}

# Compositor-thread marks an update-driven composition is drawn from in the comp lane.
UPDATE_FRAME_ACTIVITIES = {
    "RenderLayerTree": "composite",
    "FlushCompositingState": "flush",
    "PaintToGLContext": "paint",
    "WaitForCompositionCompletion": "wait",
    "SkiaBackingStoreTileUpdate": "upload",
}

# activity -> (256-color code, plain-text glyph, legend label)
ACTIVITY_STYLES: Dict[str, Tuple[int, str, str]] = {
    "js": (214, "R", "requestAnimationFrame callbacks"),
    "style": (170, "S", "style resolution"),
    "build": (97, "B", "render tree build"),
    "layout": (39, "L", "layout"),
    "compositing_update": (44, "C", "compositing update"),
    "finalize": (108, "F", "finalize rendering update"),
    "record": (131, "D", "display list recording"),
    "tiles": (143, "T", "tile updates"),
    "upload": (172, "U", "tile upload to GPU"),
    "update": (245, "u", "rendering update, other"),
    "wait": (240, "w", "waiting for composition"),
    "composite": (29, "c", "compositing, other"),
    "flush": (220, "f", "flush compositing state"),
    "paint": (41, "P", "paint to GL context"),
    "timers": (139, "t", "timers and event loop"),
    "idle": (236, ".", "idle"),
}
# Derived from the styles so an added activity cannot go missing from the legend.
LEGEND_ORDER = list(ACTIVITY_STYLES)

# A compositor-driven composition is one muted glyph by its leading reason, brighter
# with further reasons. None of these glyphs is used by the main lane.
COMPOSITOR_GLYPHS = {"Animation": "a", "VideoFrame": "v", "AsyncScrolling": "~"}
COMPOSITOR_STYLES: Dict[str, Tuple[int, str, str]] = {
    "a": (66, "a", "Animation composition"),
    "a+": (109, "a", "Animation with other compositor reasons"),
    "v": (67, "v", "VideoFrame composition"),
    "v+": (110, "v", "VideoFrame with other compositor reasons"),
    "~": (68, "~", "AsyncScrolling composition"),
    "~+": (111, "~", "AsyncScrolling with other compositor reasons"),
    "o": (95, "o", "composition for another reason"),
    "o+": (138, "o", "composition for other reasons"),
    "no_reason": (124, "n", "composition naming no reason (unexpected)"),
    "unrecorded": (241, "?", "composition with unrecorded reasons"),
    "frame": (231, "!", "frame (DidRenderFrame)"),
    "update_frame": (226, "#", "frame of a rendering update"),
}
# Frames are instants: drawn in a cell but taking no time from the lane.
FRAME_KEYS = {"frame", "update_frame"}
# Compositor state between compositions, from the state replay.
STATE_STYLES: Dict[str, Tuple[int, str, str]] = {
    AWAITING_FRAME_COMPLETE: (238, "=", "awaiting FrameComplete (display round trip)"),
    IDLE: (236, ".", "compositor idle"),
    BLOCKED_ON_TILES: (130, ":", "blocked on tiles"),
    SCHEDULED: (250, "-", "composition scheduled"),
    UNKNOWN: (196, "x", "compositor state unknown"),
    GAP: (237, "_", "between compositions, state not traced"),
}
COMP_STYLES: Dict[str, Tuple[int, str, str]] = (
    {
        key: ACTIVITY_STYLES[key]
        for key in ("composite", "flush", "upload", "paint", "wait")
    }
    | COMPOSITOR_STYLES
    | {"state:" + state: style for state, style in STATE_STYLES.items()}
)
COMP_LEGEND_ORDER = list(COMP_STYLES)

# Frame numbers in the frame lane: a frame of the compositor, of a rendering update,
# and one that came a refresh or more late while the compositor was active.
FRAME_NUMBER_COLORS = {"frame": 250, "update_frame": 226, "late": 196}
# Marks a late frame without color, and a frame whose number did not fit.
LATE_SUFFIX = "*"
UNLABELLED_FRAME = "|"

# Concurrently painting tiles as a green intensity ramp, since the value is a count.
TILE_LANE_LEVELS: List[Tuple[int, str, str]] = [
    (236, ".", "none"),
    (28, "1", "1"),
    (34, "2", "2"),
    (40, "3", "3"),
    (46, "4", "4"),
    (82, "+", "5 or more"),
]

BLOCK = "█"
REFRESH_TICK = "^"
TRUNCATION_GLYPH = "»"
FRAME_LANE_LABEL = "frame"
MAIN_LANE_LABEL = "main"
COMP_LANE_LABEL = "comp"
TILE_LANE_LABEL = "tiles"
# Row label is "<row> <lane> " and the ruler indent matches it. The row field is a
# minimum, widened to fit the drawn labels.
ROW_FIELD_WIDTH = 18
LANE_FIELD_WIDTH = 5

DEFAULT_RESOLUTION = 1.0
# Default row length, in refresh intervals or in ms when none is known.
DEFAULT_ROW_REFRESHES = 6
DEFAULT_ROW_LENGTH = 100.0
DEFAULT_MAX_ROWS = 40

# What --worst ranks the rows by.
FRAME_INTERVAL = "frame-interval"
UPDATE_LATENCY = "update-latency"
# A worst event longer than a row, or a --frames window, gets this share of a row on
# each side.
WORST_MARGIN = 0.1

# Below this many cells per refresh interval, ticks would merge, so none are drawn.
MIN_VBLANK_CELLS = 4
# Captures are timed in nanoseconds.
MIN_RESOLUTION = 0.000001
# Rows are all built before --max-rows picks some.
MAX_TIME_ROWS = 1000000
# Cells a row's legend totals are counted in at most, coarser where it is long.
MAX_ROW_CELLS = 100000


def timeline(args: argparse.Namespace) -> None:
    _check_drawing_options(args)
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    capture_begin, capture_end = parsed_data["document"]["timespan"]
    check_timespan_holds_data(capture_begin, capture_end, timespan_begin, timespan_end)
    # Untrimmed like `analyze`: a mark straddling the window edge still covers cells.
    data = sysprof_data_with_marks_by_name(parsed_data)

    logging.info("Reconstructing compositions and rendering updates...")
    draw_timeline(data, args, timespan_begin, timespan_end)


def draw_timeline(
    data: Dict[str, Any],
    args: argparse.Namespace,
    timespan_begin: Optional[int] = None,
    timespan_end: Optional[int] = None,
) -> None:
    """Draw data already reshaped by sysprof_data_with_marks_by_name().

    Split from the subcommand so tests draw through the same path.
    """
    model = calculate_compositions(data)
    updates = calculate_rendering_updates(data, model)
    pids = _drawn_pids(data, model)
    if not pids:
        print(
            "Nothing to draw: the capture holds no compositor marks (RenderLayerTree,"
            " ScheduleComposition, FrameComplete) and no {} marks.".format(
                RENDERING_UPDATE_MARK
            )
        )
        return
    row_length = _row_length(args, model)
    frames = getattr(args, "frames", None)
    if frames is not None:
        timespan_begin, timespan_end = _frames_window(data, model, frames, row_length)
    window = clamped_timespan(data, timespan_begin, timespan_end)
    worst = getattr(args, "worst", None)
    if worst:
        rows = _worst_rows(model, updates, worst, row_length, window)
    else:
        rows = _time_rows(pids, model, row_length, window)
    if not rows:
        print(_nothing_in_window(worst))
        return

    selected = list(enumerate(rows, 1))[: args.max_rows]
    resolution = getattr(args, "resolution", None)
    if resolution is None:
        resolution = _fitting_resolution(
            max(row["duration"] for _, row in selected), args.max_cells
        )
    logging.info("Drawing %d of %d rows...", len(selected), len(rows))
    renderer = _Renderer(data, args, model, updates, row_length, resolution, worst)
    renderer.render(selected, len(rows))
    if getattr(args, "explain", False):
        print()
        print(explanations.TEXT_TIMELINE)


def _fitting_resolution(milliseconds: float, max_cells: int) -> float:
    """The default resolution, or the finest 1-2-5 step coarser than it that fits a
    row of `milliseconds` in `max_cells`, for a worst event longer than a row.
    """
    magnitude = DEFAULT_RESOLUTION
    while True:
        for factor in (1, 2, 5):
            if milliseconds / (magnitude * factor) <= max_cells:
                return magnitude * factor
        magnitude *= 10


def _drawn_pids(data: Dict[str, Any], model: CompositionModel) -> List[int]:
    """Processes with compositor marks or rendering updates of their own."""
    pids = {c["pid"] for c in model.compositions}
    pids |= {pid for pid, segments in model.replay.segments_by_pid.items() if segments}
    pids |= set(marks_by_process(data["marks"].get(RENDERING_UPDATE_MARK, [])))
    return sorted(pids)


def _cell_nsec(resolution: float) -> int:
    """A cell of --resolution milliseconds, in the whole nanoseconds it indexes."""
    return int(round(msec_to_nsec(1) * resolution))


def _row_length(args: argparse.Namespace, model: CompositionModel) -> float:
    length = getattr(args, "row_length", None)
    if length is not None:
        return float(length)
    if model.reference:
        return DEFAULT_ROW_REFRESHES * model.reference
    return DEFAULT_ROW_LENGTH


def frame_number(index: int) -> int:
    """Frames are numbered from 1 in frame order over the whole capture."""
    return index + 1


def parse_frames_argument(arg: str) -> Tuple[int, Optional[int]]:
    """Parse "<first>-<last>" or a single frame number."""
    values = arg.split("-")
    if len(values) > 2 or not all(value.isdigit() for value in values):
        raise UsageError(
            'frames is no "<first>-<last>" or single frame number: {}'.format(arg)
        )
    first = int(values[0])
    last = int(values[1]) if len(values) == 2 else None
    if first < 1 or (last is not None and last < first):
        raise UsageError(
            "frames are numbered from 1, and the last cannot come before the"
            " first: {}".format(arg)
        )
    return first, last


def _frames_window(
    data: Dict[str, Any], model: CompositionModel, arg: str, row_length: float
) -> Tuple[int, int]:
    """The window --frames selects: from the first frame's composition to the last
    frame with a margin either side, so its number fits, or a row centered on a
    single frame.
    """
    first, last = parse_frames_argument(arg)
    compositions = model.compositions
    if not compositions:
        raise UsageError("--frames needs frames, but the capture holds none")
    if (last or first) > len(compositions):
        raise UsageError(
            "--frames {} reaches past the last frame, {}".format(arg, len(compositions))
        )
    capture_begin, capture_end = clamped_timespan(data, None, None)
    if last is None:
        frame = compositions[first - 1]["frame_nsec"]
        half = int(round(msec_to_nsec(1) * row_length / 2))
        begin = max(capture_begin, frame - half)
        return begin, min(capture_end, begin + 2 * half)
    margin = int(round(msec_to_nsec(1) * row_length * WORST_MARGIN))
    begin = compositions[first - 1]["begin_nsec"] - margin
    end = compositions[last - 1]["frame_nsec"] + margin
    return max(capture_begin, begin), min(capture_end, end)


def _time_rows(
    pids: Sequence[int],
    model: CompositionModel,
    row_length: float,
    window: Tuple[int, int],
) -> List[Dict[str, Any]]:
    """Rows of `row_length` ms across the window for every drawn process."""
    begin, end = window
    step = int(round(msec_to_nsec(1) * row_length))
    count = len(pids) * -(-max(0, end - begin) // step)
    if count > MAX_TIME_ROWS:
        raise UsageError(
            "rows of {} ms make {} rows, more than {}: pass a longer"
            " --row-length or narrow --timespan".format(
                row_length, count, MAX_TIME_ROWS
            )
        )
    rows: List[Dict[str, Any]] = []
    for pid in pids:
        row_begin = begin
        while row_begin < end:
            row_end = min(row_begin + step, end)
            rows.append(_row(pid, row_begin, row_end))
            row_begin = row_end
    return sorted(rows, key=lambda row: (row["begin_nsec"], row["pid"]))


def _row(pid: int, begin: int, end: int, note: Optional[str] = None) -> Dict[str, Any]:
    return {
        "pid": pid,
        "begin_nsec": begin,
        "end_nsec": end,
        "duration": nsec_to_msec(end - begin),
        "note": note,
    }


def _worst_rows(
    model: CompositionModel,
    updates: Sequence[Dict[str, Any]],
    worst: str,
    row_length: float,
    window: Tuple[int, int],
) -> List[Dict[str, Any]]:
    """A row around each of the worst events in the window, worst first.

    An event overlapping a row already chosen is drawn by that row.
    """
    events = (
        _frame_interval_events(model, window)
        if worst == FRAME_INTERVAL
        else _update_latency_events(model, updates, window)
    )
    events.sort(key=lambda event: -event["length"])
    length = msec_to_nsec(1) * row_length
    rows: List[Dict[str, Any]] = []
    taken: Dict[int, List[Tuple[int, int]]] = {}
    for event in events:
        pid, begin, end = event["pid"], event["begin_nsec"], event["end_nsec"]
        if any(b < end and begin < e for b, e in taken.get(pid, [])):
            continue
        span = end - begin
        margin = (length - span) / 2 if span <= length else length * WORST_MARGIN
        row_begin = max(window[0], int(begin - margin))
        row_end = min(window[1], max(row_begin + int(length), int(end + margin)))
        taken.setdefault(pid, []).append((row_begin, row_end))
        rows.append(_row(pid, row_begin, row_end, event["note"]))
    return rows


def _frame_interval_events(
    model: CompositionModel, window: Tuple[int, int]
) -> List[Dict[str, Any]]:
    events = []
    for pid, indices in indices_by_pid(model.compositions).items():
        for previous, index in zip(indices, indices[1:]):
            composition = model.compositions[index]
            frame = composition["frame_nsec"]
            if not composition["active"] or not window[0] <= frame < window[1]:
                continue
            missed = composition["missed_refreshes"]
            events.append(
                {
                    "pid": pid,
                    "begin_nsec": model.compositions[previous]["frame_nsec"],
                    "end_nsec": frame,
                    "length": composition["interval"],
                    "note": "frame {} came {:.3f} ms after frame {}{}".format(
                        frame_number(index),
                        composition["interval"],
                        frame_number(previous),
                        _missed_text(missed),
                    ),
                }
            )
    return events


def _missed_text(missed: Optional[int]) -> str:
    if not missed:
        return ""
    return ", {} missed refresh{}".format(missed, "" if missed == 1 else "es")


def _update_latency_events(
    model: CompositionModel,
    updates: Sequence[Dict[str, Any]],
    window: Tuple[int, int],
) -> List[Dict[str, Any]]:
    events = []
    for number, update in enumerate(updates, 1):
        if update["frame"] is None or not window[0] <= update["begin_nsec"] < window[1]:
            continue
        frame = model.compositions[update["frame"]]["frame_nsec"]
        latency = nsec_to_msec(frame - update["begin_nsec"])
        events.append(
            {
                "pid": update["pid"],
                "begin_nsec": update["begin_nsec"],
                "end_nsec": frame,
                "length": latency,
                "note": "update {} reached the screen with frame {} after"
                " {:.3f} ms".format(number, frame_number(update["frame"]), latency),
            }
        )
    return events


def _nothing_in_window(worst: Optional[str]) -> str:
    if worst == FRAME_INTERVAL:
        return "No active frame intervals in the analyzed window."
    if worst == UPDATE_LATENCY:
        return "No composited rendering updates in the analyzed window."
    return "No rows: the analyzed window spans no time."


def _check_finite(option: str, value: float) -> None:
    if not math.isfinite(value):
        raise UsageError(
            "{} is a number of milliseconds, so it must be finite".format(option)
        )


def _check_length(option: str, value: float, what: str) -> None:
    """Reject a length of milliseconds shorter than a nanosecond, `what` naming it."""
    _check_finite(option, value)
    if value <= 0:
        raise UsageError(
            "{} is of no length: {} must be more than 0".format(what, option)
        )
    if value < MIN_RESOLUTION:
        raise UsageError(
            "{} is shorter than the nanosecond a capture is timed in:"
            " {} must be at least {}".format(what, option, MIN_RESOLUTION)
        )


def _check_drawing_options(args: argparse.Namespace) -> None:
    resolution = getattr(args, "resolution", None)
    if resolution is not None:
        _check_length("--resolution", resolution, "a cell")
    row_length = getattr(args, "row_length", None)
    if row_length is not None:
        _check_length("--row-length", row_length, "a row")
    if args.max_rows < 1:
        raise UsageError("--max-rows is the number of rows to draw, so at least 1")
    if args.max_cells < 1:
        raise UsageError("--max-cells is the length of a bar in cells, so at least 1")
    frames = getattr(args, "frames", None)
    if frames is not None:
        parse_frames_argument(frames)
        if getattr(args, "timespan", "-") != "-":
            raise UsageError("--frames selects the window, so it takes no --timespan")


def _renderers_of(rows: Sequence[Dict[str, Any]]) -> Set[int]:
    return {row["pid"] for row in rows}


class _Renderer:
    def __init__(
        self,
        data: Dict[str, Any],
        args: argparse.Namespace,
        model: CompositionModel,
        updates: Sequence[Dict[str, Any]],
        row_length: float,
        resolution: float = DEFAULT_RESOLUTION,
        worst: Optional[str] = None,
    ) -> None:
        self.args = args
        self.model = model
        self.updates = updates
        self.row_length = row_length
        self.worst = worst
        # Cells are whole nanoseconds, and every length is measured in them.
        self.resolution = _cell_nsec(resolution)
        self.cell_msec = nsec_to_msec(self.resolution)
        self.reference = model.reference
        self.reference_cells = (
            self.reference / self.cell_msec if self.reference else 0.0
        )
        self.marks_refreshes = self.reference_cells >= MIN_VBLANK_CELLS
        self.color = args.color == "always" or (
            args.color == "auto" and sys.stdout.isatty()
        )
        self.activity_marks = _collect_marks(data, MAIN_THREAD_ACTIVITIES)
        self.frame_marks = _collect_marks(data, UPDATE_FRAME_ACTIVITIES)
        # Only the tiles lane reads them.
        self.tile_marks = _collect_marks(data, ["PaintTile"]) if args.tile_lane else {}
        # A process without compositor marks of its own gets no comp lane.
        self.timeline_pids = {c["pid"] for c in model.compositions} | {
            pid for pid, segments in model.replay.segments_by_pid.items() if segments
        }
        self.frame_indices_by_pid = indices_by_pid(model.compositions)
        # (index, composition) pairs by begin.
        self.compositions_by_pid = {
            pid: MarkIndex(
                [(index, model.compositions[index]) for index in indices],
                begin=lambda item: item[1]["begin_nsec"],
                end=lambda item: item[1]["end_nsec"],
            )
            for pid, indices in self.frame_indices_by_pid.items()
        }
        self.frames_by_pid = {
            pid: [model.compositions[index]["frame_nsec"] for index in indices]
            for pid, indices in self.frame_indices_by_pid.items()
        }
        self.segment_begins = {
            pid: [segment[0] for segment in segments]
            for pid, segments in model.replay.segments_by_pid.items()
        }
        # The update each frame is the frame of, by composition index.
        self.update_of_frame = {
            update["frame"]: number
            for number, update in enumerate(updates, 1)
            if update["frame"] is not None
        }
        self.pids: Set[int] = set()
        self.row_field = ROW_FIELD_WIDTH

    @property
    def label_width(self) -> int:
        return self.row_field + LANE_FIELD_WIDTH + 2

    def render(self, selected: List[Tuple[int, Dict[str, Any]]], total: int) -> None:
        self.pids = _renderers_of([row for _, row in selected])
        self._size_labels(selected)
        minimum_length = 0.0
        if self.marks_refreshes:
            # One cell past the interval, or the first tick falls beyond the bar.
            minimum_length = (math.ceil(self.reference_cells) + 1) * self.cell_msec
        longest = max(row["duration"] for _, row in selected)
        cells = self._cell_count(max(minimum_length, longest))

        self._render_header(total, len(selected), cells)
        totals: Dict[str, float] = {}
        comp_totals: Dict[str, float] = {}
        frames: Dict[str, int] = {}
        for position, (number, row) in enumerate(selected):
            if position:
                print()
            self._render_row(number, row, cells, totals, comp_totals, frames)
        comp_rows = sum(1 for _, row in selected if row["pid"] in self.timeline_pids)
        self._render_footer(totals, comp_totals, frames, len(selected), comp_rows)

    def _size_labels(self, selected: List[Tuple[int, Dict[str, Any]]]) -> None:
        """Widen the label field so no label pushes its bar out of the ruler column."""
        self.row_field = max(
            [ROW_FIELD_WIDTH]
            + [len(self._row_label(number, row)) for number, row in selected]
        )

    def _row_label(self, number: int, row: Dict[str, Any]) -> str:
        process = _process_suffix(row["pid"]) if len(self.pids) > 1 else ""
        return "#{:<6d}@{:>8.1f} ms{}".format(
            number, nsec_to_msec(row["begin_nsec"]), process
        )

    def _cells_spanning(self, milliseconds: float) -> int:
        """Cells covering `milliseconds`, rounded up so a partial last cell is drawn."""
        # Round before the ceiling, or float error from dividing by 0.2 adds a cell.
        return max(1, math.ceil(round(milliseconds / self.cell_msec, 9)))

    def _cell_count(self, milliseconds: float) -> int:
        return min(self._cells_spanning(milliseconds), self.args.max_cells)

    def _render_header(self, total: int, shown: int, cells: int) -> None:
        if self.worst == FRAME_INTERVAL:
            print(
                "Timeline: rows around the longest active frame intervals of the"
                " analyzed window, worst first, {} of {} shown, {:.4f} ms per"
                " cell.".format(shown, total, self.cell_msec)
            )
        elif self.worst == UPDATE_LATENCY:
            print(
                "Timeline: rows around the rendering updates of the analyzed window"
                " slowest to reach the screen, worst first, {} of {} shown, {:.4f} ms"
                " per cell.".format(shown, total, self.cell_msec)
            )
        else:
            print(
                "Timeline: rows of {:.3f} ms across the analyzed window, {} of {}"
                " shown, {:.4f} ms per cell.".format(
                    self.row_length, shown, total, self.cell_msec
                )
            )
        if self.marks_refreshes:
            print(
                "Refresh interval: {:.4f} ms (from {}). The {} ticks under the ruler"
                " step one refresh interval from the row begin, they are not"
                " vblanks.".format(
                    self.reference, self.model.reference_source, REFRESH_TICK
                )
            )
        elif self.reference:
            print(
                "Refresh interval: {:.4f} ms, fewer than {} cells, so no refresh"
                " ticks are drawn.".format(self.reference, MIN_VBLANK_CELLS)
            )
        else:
            print(
                "Refresh interval: unknown, since the capture holds no presentation"
                " feedback, display refreshes or frame intervals, so no refresh ticks"
                " are drawn."
            )
        print()
        print(self._ruler(cells))

    def _ruler(self, cells: int) -> str:
        cells_per_msec = 1 / self.cell_msec
        span = max(1, int(cells / cells_per_msec))
        step = self._ruler_step(cells, cells_per_msec, span)
        labels = [" "] * cells
        ticks = ["─"] * cells
        for millisecond in range(0, span + 1, max(1, step // 5)):
            position = int(round(millisecond * cells_per_msec))
            if position >= cells:
                break
            ticks[position] = "┼" if millisecond % step == 0 else "┴"
            if millisecond % step:
                continue
            # The last label may run past the row, as nothing follows it.
            for offset, character in enumerate(str(millisecond)):
                if position + offset < len(labels):
                    labels[position + offset] = character
                else:
                    labels.append(character)
        # Indent past the label and the lane border so ticks sit over their cells.
        indent = " " * (self.label_width + 1)
        ruler = "{}{}\n{}{} [ms]".format(
            indent, "".join(labels), indent, "".join(ticks)
        )
        if self.marks_refreshes:
            refreshes = [" "] * cells
            for position in self._refresh_positions(cells):
                refreshes[position] = REFRESH_TICK
            ruler += "\n{}{} [refresh]".format(indent, "".join(refreshes))
        return ruler

    def _refresh_positions(self, cells: int) -> List[int]:
        positions = []
        k = 1
        while True:
            position = int(round(k * self.reference_cells))
            if position >= cells:
                return positions
            positions.append(position)
            k += 1

    def _ruler_step(self, cells: int, cells_per_msec: float, span: int) -> int:
        """Milliseconds between labelled ticks, so labels never collide."""
        width = len(str(span)) + 1
        magnitude = 1
        while True:
            for factor in (1, 2, 5):
                step = magnitude * factor
                spacing = step * cells_per_msec
                if spacing >= width and cells / spacing <= 20:
                    return step
            magnitude *= 10

    def _render_row(
        self,
        number: int,
        row: Dict[str, Any],
        cells: int,
        totals: Dict[str, float],
        comp_totals: Dict[str, float],
        frames: Dict[str, int],
    ) -> None:
        row_cells = self._cells_spanning(row["duration"])
        comp_lane = row["pid"] in self.timeline_pids
        drawn_cells = self._cell_count(row["duration"])
        painted_cells = drawn_cells
        truncated = row_cells > drawn_cells
        # Totals count the whole row, not only the drawn cells, so truncation does
        # not skew the legend averages. A long row is counted in coarser cells, so
        # painting it costs at most MAX_ROW_CELLS.
        duration_nsec = row["end_nsec"] - row["begin_nsec"]
        total_cell = max(self.resolution, -(-duration_nsec // MAX_ROW_CELLS))
        total_cells = -(-duration_nsec // total_cell) or 1
        fine = total_cell == self.resolution

        if row["note"]:
            print(row["note"])
        label = self._row_label(number, row)
        if comp_lane:
            print(
                _lane_prefix(label, FRAME_LANE_LABEL, self.row_field)
                + self._frame_lane(row, painted_cells, cells, truncated)
            )
            label = ""
        activities = self._paint_activities(row, row_cells if fine else painted_cells)
        self._count(
            totals,
            (
                activities
                if fine
                else self._paint_activities(row, total_cells, total_cell)
            ),
            row["duration"],
            total_cell,
        )
        print(
            _lane_prefix(label, MAIN_LANE_LABEL, self.row_field)
            + self._lane(
                activities[:painted_cells],
                ACTIVITY_STYLES,
                drawn_cells,
                cells,
                truncated,
            )
        )
        row_frames: List[str] = []
        if comp_lane:
            comp, row_frames, frame_cells = self._paint_comp(
                row, row_cells if fine else painted_cells
            )
            # Frames are instants, drawn over the lane but taking no time from it.
            self._count(
                comp_totals,
                comp if fine else self._paint_comp(row, total_cells, total_cell)[0],
                row["duration"],
                total_cell,
            )
            for cell, key in frame_cells:
                # A frame past the drawn cells is not drawn, but listed.
                if cell < len(comp):
                    comp[cell] = key
                comp_totals.setdefault(key, 0.0)
            for combination in row_frames:
                frames[combination] = frames.get(combination, 0) + 1
            print(
                _lane_prefix("", COMP_LANE_LABEL, self.row_field)
                + self._lane(
                    comp[:painted_cells], COMP_STYLES, drawn_cells, cells, truncated
                )
            )
        if self.args.tile_lane:
            print(
                _lane_prefix("", TILE_LANE_LABEL, self.row_field)
                + self._tile_lane(row, painted_cells, drawn_cells, cells, truncated)
            )
        indent = " " * (self.label_width + 1)
        if comp_lane:
            print(indent + _frames_line(row_frames))
        for line in self._update_lines(row):
            print(indent + line)

    def _frame_lane(
        self, row: Dict[str, Any], painted_cells: int, cells: int, truncated: bool
    ) -> str:
        """Each frame's number from its cell on, where it fits before the next one,
        and a bare tick where it does not.
        """
        glyphs: List[Tuple[str, Optional[int]]] = [(" ", None)] * painted_cells
        pid, begin, end = row["pid"], row["begin_nsec"], row["end_nsec"]
        frames = self.frames_by_pid.get(pid, [])
        indices = self.frame_indices_by_pid.get(pid, [])
        first = bisect.bisect_left(frames, begin)
        last = bisect.bisect_left(frames, end)
        cursor = 0
        for position in range(first, last):
            index = indices[position]
            composition = self.model.compositions[index]
            cell = int((frames[position] - begin) // self.resolution)
            if cell >= painted_cells:
                break
            late = composition["active"] and composition["missed_refreshes"]
            kind = (
                "late"
                if late
                else "update_frame" if index in self.update_of_frame else "frame"
            )
            label = str(frame_number(index))
            if late and not self.color:
                label += LATE_SUFFIX
            # The next frame's cell, so a label does not hide it.
            limit = painted_cells
            if position + 1 < last:
                limit = min(
                    limit, int((frames[position + 1] - begin) // self.resolution)
                )
            if cell >= cursor and cell + len(label) <= limit:
                for offset, character in enumerate(label):
                    glyphs[cell + offset] = (character, FRAME_NUMBER_COLORS[kind])
                cursor = cell + len(label) + 1
            elif cell >= cursor or glyphs[cell][0] == " ":
                glyphs[cell] = (UNLABELLED_FRAME, FRAME_NUMBER_COLORS[kind])
                cursor = max(cursor, cell + 1)
        return self._close_lane(glyphs, painted_cells, cells, truncated)

    def _update_lines(self, row: Dict[str, Any]) -> List[str]:
        """The rendering updates beginning in the row or reaching the screen in it."""
        lines = []
        begin, end = row["begin_nsec"], row["end_nsec"]
        for number, update in enumerate(self.updates, 1):
            if update["pid"] != row["pid"] or update["begin_nsec"] >= end:
                continue
            frame = (
                self.model.compositions[update["frame"]]["frame_nsec"]
                if update["frame"] is not None
                else None
            )
            began_in_row = update["begin_nsec"] >= begin
            if not began_in_row and (frame is None or not begin <= frame < end):
                continue
            at = "at {:.1f} ms".format(nsec_to_msec(update["begin_nsec"]))
            lines.append(
                "update {} {}: {}".format(number, at, self._update_outcome(update))
            )
        return lines

    def _update_outcome(self, update: Dict[str, Any]) -> str:
        if update["outcome"] == COMPOSITED:
            frame = self.model.compositions[update["frame"]]["frame_nsec"]
            pairing = update["pairing"]
            text = "frame {} after {:.3f} ms".format(
                frame_number(update["frame"]),
                nsec_to_msec(frame - update["begin_nsec"]),
            )
            if pairing["status"] != EXACT:
                text += " ({}: {})".format(
                    pairing["status"], ", ".join(pairing["reasons"])
                )
            return text
        if update["outcome"] == UNCOMPOSITED:
            return "never composited"
        if update["outcome"] == PENDING:
            return "not composited before the capture ended"
        return "asked for no frame"

    def _count(
        self,
        totals: Dict[str, float],
        keys: List[str],
        duration: float,
        cell_nsec: Optional[int] = None,
    ) -> None:
        """Add each cell's time to its key's total, the partial last cell in part."""
        cell_msec = nsec_to_msec(cell_nsec) if cell_nsec else self.cell_msec
        for key in keys:
            totals[key] = totals.get(key, 0) + cell_msec
        if keys:
            totals[keys[-1]] -= len(keys) * cell_msec - duration

    def _lane(
        self,
        keys: List[str],
        styles: Dict[str, Tuple[int, str, str]],
        drawn_cells: int,
        cells: int,
        truncated: bool,
    ) -> str:
        glyphs: List[Tuple[str, Optional[int]]] = []
        for key in keys[:drawn_cells]:
            color, glyph, _ = styles[key]
            glyphs.append((BLOCK if self.color else glyph, color))
        return self._close_lane(glyphs, drawn_cells, cells, truncated)

    def _tile_lane(
        self,
        row: Dict[str, Any],
        painted_cells: int,
        drawn_cells: int,
        cells: int,
        truncated: bool,
    ) -> str:
        concurrency = self._paint_tile_concurrency(row, painted_cells)
        glyphs: List[Tuple[str, Optional[int]]] = []
        for count in concurrency[:drawn_cells]:
            color, glyph, _ = TILE_LANE_LEVELS[min(count, len(TILE_LANE_LEVELS) - 1)]
            glyphs.append((BLOCK if self.color else glyph, color))
        return self._close_lane(glyphs, drawn_cells, cells, truncated)

    def _close_lane(
        self,
        glyphs: List[Tuple[str, Optional[int]]],
        drawn_cells: int,
        cells: int,
        truncated: bool,
    ) -> str:
        padding = " " * max(0, cells - drawn_cells)
        return "▕{}{}{}".format(
            self._painted(glyphs), TRUNCATION_GLYPH if truncated else "", padding
        )

    def _painted(self, glyphs: Sequence[Tuple[str, Optional[int]]]) -> str:
        """Join the cells, emitting a color escape only where the color changes."""
        if not self.color:
            return "".join(glyph for glyph, _ in glyphs)
        painted = []
        current: Optional[int] = None
        for glyph, color in glyphs:
            if color != current:
                painted.append(
                    "\x1b[0m" if color is None else "\x1b[38;5;{}m".format(color)
                )
                current = color
            painted.append(glyph)
        if current is not None:
            painted.append("\x1b[0m")
        return "".join(painted)

    def _paint_activities(
        self, row: Dict[str, Any], cells: int, cell_nsec: Optional[int] = None
    ) -> List[str]:
        lane = ["idle"] * cells
        marks = _marks_overlapping(
            self.activity_marks, row["pid"], row["begin_nsec"], row["end_nsec"]
        )
        # Longest first, so nested marks overwrite their parents.
        for mark in sorted(marks, key=lambda m: -m["duration"]):
            self._fill(
                lane,
                row,
                mark_begin(mark),
                mark["end_time"],
                MAIN_THREAD_ACTIVITIES[mark["name"]],
                cell_nsec,
            )
        return lane

    def _paint_comp(
        self, row: Dict[str, Any], cells: int, cell_nsec: Optional[int] = None
    ) -> Tuple[List[str], List[str], List[Tuple[int, str]]]:
        """The comp lane of a row, the combinations of its frames, and their cells.

        Painted in passes: compositor states, compositions, then frame cells.
        """
        pid, begin, end = row["pid"], row["begin_nsec"], row["end_nsec"]
        lane = ["state:" + UNKNOWN] * cells
        segments = self.model.replay.segments_by_pid.get(pid, [])
        begins = self.segment_begins.get(pid, [])
        for segment_begin, segment_end, state, _ in segments_overlapping(
            segments, begins, begin, end
        ):
            key = "composite" if state == COMPOSITING else "state:" + state
            self._fill(lane, row, segment_begin, segment_end, key, cell_nsec)

        row_frames = []
        frame_cells = []
        for index, composition in self._compositions_overlapping(pid, begin, end):
            klass = composition["class"]
            if klass == UPDATE_CLASS:
                self._fill(
                    lane,
                    row,
                    composition["begin_nsec"],
                    composition["end_nsec"],
                    "composite",
                    cell_nsec,
                )
                marks = _marks_overlapping(
                    self.frame_marks,
                    pid,
                    max(begin, composition["begin_nsec"]),
                    min(end, composition["end_nsec"]),
                )
                for mark in sorted(marks, key=lambda m: -m["duration"]):
                    if mark["name"] == "RenderLayerTree":
                        continue
                    self._fill(
                        lane,
                        row,
                        max(mark_begin(mark), composition["begin_nsec"]),
                        min(mark["end_time"], composition["end_nsec"]),
                        UPDATE_FRAME_ACTIVITIES[mark["name"]],
                        cell_nsec,
                    )
            else:
                self._fill(
                    lane,
                    row,
                    composition["begin_nsec"],
                    composition["end_nsec"],
                    _compositor_key(composition),
                    cell_nsec,
                )
            if begin <= composition["frame_nsec"] < end:
                row_frames.append(composition["combination"])
                frame_cells.append((index, composition["frame_nsec"]))

        return (
            lane,
            row_frames,
            [
                (
                    # Not clamped to the cells: a frame past them is not drawn.
                    int((frame - begin) // (cell_nsec or self.resolution)),
                    "update_frame" if index in self.update_of_frame else "frame",
                )
                for index, frame in frame_cells
            ],
        )

    def _compositions_overlapping(
        self, pid: int, begin: int, end: int
    ) -> List[Tuple[int, Dict[str, Any]]]:
        index = self.compositions_by_pid.get(pid)
        return index.marks_overlapping(begin, end) if index else []

    def _paint_tile_concurrency(self, row: Dict[str, Any], cells: int) -> List[int]:
        lane = [0] * cells
        marks = _marks_overlapping(
            self.tile_marks, row["pid"], row["begin_nsec"], row["end_nsec"]
        )
        for mark in marks:
            for index in self._cell_range(
                row, mark_begin(mark), mark["end_time"], cells
            ):
                lane[index] += 1
        return lane

    def _fill(
        self,
        lane: List[str],
        row: Dict[str, Any],
        begin: int,
        end: int,
        key: str,
        cell_nsec: Optional[int],
    ) -> None:
        """Paint `key` over the cells of the lane [begin, end) covers."""
        for index in self._cell_range(row, begin, end, len(lane), cell_nsec):
            lane[index] = key

    def _cell_range(
        self,
        row: Dict[str, Any],
        begin: int,
        end: int,
        cells: int,
        cell_nsec: Optional[int] = None,
    ) -> range:
        """The cells [begin, end) covers, clamped to the row.

        Clamped to the row, not the cells, since the last cell can reach past the row
        end and would otherwise draw the next row's update.
        """
        origin = row["begin_nsec"]
        begin = max(begin, origin)
        end = min(end, row["end_nsec"])
        if end <= begin:
            return range(0)
        cell = cell_nsec or self.resolution
        first = int((begin - origin) // cell)
        last = int((end - origin - 1) // cell)
        return range(max(0, first), min(cells - 1, last) + 1)

    def _colored(self, text: str, color: int) -> str:
        if not self.color:
            return text
        return "\x1b[38;5;{}m{}\x1b[0m".format(color, text)

    def _render_footer(
        self,
        totals: Dict[str, float],
        comp_totals: Dict[str, float],
        frames: Dict[str, int],
        rows: int,
        comp_rows: int,
    ) -> None:
        print()
        print(
            "Legend: time per row averaged over the drawn rows, and the marks"
            " each\ncolor is drawn from."
        )
        width = max(
            [len(ACTIVITY_STYLES[activity][2]) for activity in totals]
            + [len(COMP_STYLES[key][2]) for key in comp_totals]
        )
        for activity in LEGEND_ORDER:
            color, glyph, description = ACTIVITY_STYLES[activity]
            if activity not in totals:
                continue
            print(
                "  {} {:<{}} {:>7.3f} ms  {}".format(
                    self._colored(BLOCK * 2 if self.color else glyph * 2, color),
                    description,
                    width,
                    totals[activity] / rows,
                    _marks_of_activity(activity),
                )
            )
        # Only where a drawn row had a comp lane.
        if comp_totals:
            print()
            print(
                "The {} lane numbers each frame (DidRenderFrame) from its cell on,"
                " counted\nover the whole capture, as --frames takes them. {} marks a"
                " frame whose\nnumber did not fit.".format(
                    FRAME_LANE_LABEL, UNLABELLED_FRAME
                )
            )
            kinds = [
                ("frame", "frame"),
                ("update_frame", "frame of a rendering update"),
                ("late", "late frame, its active frame interval missed a refresh"),
            ]
            for kind, description in kinds if self.color else kinds[::2]:
                sample = "12" + (
                    LATE_SUFFIX if kind == "late" and not self.color else ""
                )
                print(
                    "  {} {}".format(
                        self._colored(sample, FRAME_NUMBER_COLORS[kind]), description
                    )
                )
            print()
            print(
                "The {} lane is the compositor thread. An update-driven composition"
                " is drawn\nfrom its marks, a compositor-driven one as one glyph by"
                " its leading reason,\nand the time between compositions as the"
                " state the replay of the compositor\nreports.".format(COMP_LANE_LABEL)
            )
            for key in COMP_LEGEND_ORDER:
                if key not in comp_totals:
                    continue
                color, glyph, description = COMP_STYLES[key]
                time = (
                    "instant"
                    if key in FRAME_KEYS
                    else "{:>7.3f} ms".format(comp_totals[key] / comp_rows)
                )
                print(
                    "  {} {:<{}} {}".format(
                        self._colored(BLOCK * 2 if self.color else glyph * 2, color),
                        description,
                        width,
                        time,
                    )
                )
            drawn = [combination for combination, n in frames.items() for _ in range(n)]
            print("  in the drawn rows, {}".format(_frames_line(drawn)))
        if self.args.tile_lane:
            print()
            print(
                "The {} lane is not the main thread. It counts how many tiles are"
                " being\nrasterized at that moment on the painting threads, which is"
                " what the main\nthread is waiting for while its own lane sits"
                " idle.".format(TILE_LANE_LABEL)
            )
            print(
                "  "
                + "   ".join(
                    "{} {}".format(
                        self._colored(BLOCK * 2 if self.color else glyph * 2, color),
                        label,
                    )
                    for color, glyph, label in TILE_LANE_LEVELS
                )
            )


def _compositor_key(composition: Dict[str, Any]) -> str:
    """The comp lane key of a composition that is not update-driven."""
    klass = composition["class"]
    if klass == NO_REASON_CLASS:
        return "no_reason"
    if klass == UNRECORDED_CLASS:
        return "unrecorded"
    assert klass == COMPOSITOR_CLASS
    reasons = composition["reasons"]
    glyph = COMPOSITOR_GLYPHS.get(reasons[0], "o")
    return glyph + ("+" if len(reasons) > 1 else "")


def _frames_line(combinations: Sequence[str]) -> str:
    """Frame count by reason combination, most frequent first."""
    if not combinations:
        return "no frames"
    counts = collections.Counter(combinations)
    return "{} frame{}: {}".format(
        len(combinations),
        "" if len(combinations) == 1 else "s",
        ", ".join(
            "{} {}".format(combination, n)
            for combination, n in sorted(
                counts.items(), key=lambda item: (-item[1], item[0])
            )
        ),
    )


def _marks_of_activity(activity: str) -> str:
    names = [
        name for name, mapped in MAIN_THREAD_ACTIVITIES.items() if mapped == activity
    ]
    return ", ".join(names) if names else "no mark covers these cells"


def _process_suffix(pid: int) -> str:
    return " [{}]".format(pid)


def _lane_prefix(row: str, lane: str, row_field: int) -> str:
    return "{:<{}} {:>{}} ".format(row, row_field, lane, LANE_FIELD_WIDTH)


def _collect_marks(
    data: Dict[str, Any], mark_names: Iterable[str]
) -> Dict[int, MarkIndex]:
    """Marks of those names, indexed per process so renderers never mix."""
    marks = [
        mark
        for name in mark_names
        for mark in data["marks"].get(name, [])
        if mark["duration"] > 0
    ]
    return mark_index_by_process(marks)


def _marks_overlapping(
    indices: Dict[int, MarkIndex], pid: int, begin: int, end: int
) -> List[Dict[str, Any]]:
    index = indices.get(pid)
    return index.marks_overlapping(begin, end) if index is not None else []
