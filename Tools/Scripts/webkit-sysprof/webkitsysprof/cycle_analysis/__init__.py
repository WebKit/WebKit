"""Per-cycle ASCII visualization of what the engine is doing inside every frame."""

import argparse
import logging
import math
import sys
from typing import Any, Dict, Iterable, List, Optional, Sequence, Set, Tuple

from ..cycles import RENDERING_UPDATE_MARK, calculate_frame_cycles
from ..parser import parse
from ..utils import (
    MarkIndex,
    UsageError,
    check_timespan_holds_data,
    display_refreshes,
    mark_begin,
    marks_by_process,
    median_vblank_interval,
    msec_to_nsec,
    parse_timespan_argument,
    sysprof_data_with_marks_by_name,
)

# Every mark that can occupy a cell of the main-thread lane, mapped to the activity
# it represents. Marks not listed here are ignored, and cells that no mark covers
# are drawn as idle. Nested marks win over their parents because the lane is
# painted longest-first, so the innermost mark is what ends up visible.
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
    "SkiaBackingStoreTileUpdate": "upload",
    "LayerTreeHostRenderingUpdate": "update",
    "RenderingUpdate": "update",
    "RenderLayerTree": "composite",
    "FlushCompositingState": "flush",
    "PaintToGLContext": "paint",
    "WaitForCompositionCompletion": "wait",
    "EventLoopRun": "timers",
    "WebCoreTimerExecution": "timers",
    "WebCoreThreadTimers": "timers",
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
# The legend is the styles themselves, in the order they are written down above,
# so an activity added to one cannot go missing from the other.
LEGEND_ORDER = list(ACTIVITY_STYLES)

# Concurrently painting tiles, shown as a brightening green ramp in the second
# lane. A single ramp rather than a set of hues, because the value is a count and
# reads best as an intensity. One list of (color, glyph, label), because all three
# are read by the same level and parallel lists of them drift apart.
TILE_LANE_LEVELS: List[Tuple[int, str, str]] = [
    (236, ".", "none"),
    (28, "1", "1"),
    (34, "2", "2"),
    (40, "3", "3"),
    (46, "4", "4"),
    (82, "+", "5 or more"),
]

# Drawn where a cycle is shorter than the minimum bar length, so that the refresh
# marker stays visible. Blank in either mode because it is not part of the cycle:
# a block here would read as more idle time inside it.
BEYOND_GLYPH = " "
BLOCK = "█"
VBLANK_GLYPH = "│"
VBLANK_COLOR = 231
TRUNCATION_GLYPH = "»"
MAIN_LANE_LABEL = "main"
TILE_LANE_LABEL = "tiles"
# The row label is "<cycle> <lane> ", and the ruler is indented to match it, so the
# two are derived from these rather than from a width written down twice. The cycle
# field is a minimum, widened to whatever the labels of the drawn cycles need.
CYCLE_FIELD_WIDTH = 18
LANE_FIELD_WIDTH = 5

# Milliseconds per cell, and the shortest bar to draw. One cell per 0.2 ms keeps a
# typical cycle around a hundred columns wide, and padding every bar to at least one
# refresh interval keeps the refresh marker visible on short cycles.
DEFAULT_RESOLUTION = 0.2
DEFAULT_MIN_LENGTH = 16.0

# The refresh marker replaces the cell it is drawn in, so an interval of very few
# cells would overwrite the lane it is meant to divide rather than mark it. Below
# this many cells per interval no marker is drawn at all.
MIN_VBLANK_CELLS = 4
# A cell cannot be shorter than the nanosecond the capture times in.
MIN_RESOLUTION = 0.000001


def analyze_frame_cycle(args: argparse.Namespace) -> None:
    _check_drawing_options(args)
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    capture_begin, capture_end = parsed_data["document"]["timespan"]
    check_timespan_holds_data(capture_begin, capture_end, timespan_begin, timespan_end)
    # Untrimmed, like the frame cycle section of `analyze`: the window bounds the
    # cycles, and a mark straddling its edge still covers cells of the cycle it
    # runs in.
    data = sysprof_data_with_marks_by_name(parsed_data)

    logging.info("Reconstructing frame cycles...")
    draw_frame_cycles(data, args, timespan_begin, timespan_end)


def draw_frame_cycles(
    data: Dict[str, Any],
    args: argparse.Namespace,
    timespan_begin: Optional[int] = None,
    timespan_end: Optional[int] = None,
) -> None:
    """Draw the cycles of data already reshaped by sysprof_data_with_marks_by_name().

    Split from the subcommand so that what a capture on disk is drawn with is what a
    capture built in a test is drawn with.
    """
    cycles = calculate_frame_cycles(data, timespan_begin, timespan_end)
    if not cycles:
        print(_nothing_to_draw(data, timespan_begin, timespan_end))
        return

    vblank_interval = median_vblank_interval(display_refreshes(data)) or 0.0
    selected = _select_cycles(cycles, args)
    if not selected:
        print("No frame cycles match the selection.")
        return

    logging.info("Drawing %d of %d cycles...", len(selected), len(cycles))
    renderer = _Renderer(data, args, vblank_interval, _renderers_of(cycles))
    renderer.render(selected, len(cycles))


def _nothing_to_draw(
    data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> str:
    """Why there was nothing to draw, which the capture and the window differ on.

    A capture whose cycles the selected window cuts through is not a capture without
    rendering updates, and saying it is sends the reader looking for marks that are
    there already.
    """
    updates = len(data["marks"].get(RENDERING_UPDATE_MARK, []))
    windowed = timespan_begin is not None or timespan_end is not None
    if windowed and updates >= 2:
        return (
            "No frame cycles found in the selected timespan, though the capture holds"
            " {} rendering updates: widen --timespan.".format(updates)
        )
    return "No frame cycles found (needs at least two rendering updates)."


def _check_drawing_options(args: argparse.Namespace) -> None:
    """Reject the option values that draw nothing, or nothing that means anything."""
    # Before the comparisons below, which a NaN passes by being neither too small
    # nor too large, only to reach the arithmetic drawing it and raise there.
    for option, value in (
        ("--resolution", args.resolution),
        ("--min-duration", args.min_duration),
        ("--min-length", args.min_length),
    ):
        if not math.isfinite(value):
            raise UsageError(
                "{} is a number of milliseconds, so it must be finite".format(option)
            )
    if args.resolution <= 0:
        raise UsageError("a cell is of no length: --resolution must be more than 0")
    if args.resolution < MIN_RESOLUTION:
        raise UsageError(
            "a cell is shorter than the nanosecond a capture is timed in:"
            " --resolution must be at least {}".format(MIN_RESOLUTION)
        )
    if args.max_cycles < 1:
        raise UsageError("--max-cycles is the number of cycles to draw, so at least 1")
    if args.max_cells < 1:
        raise UsageError("--max-cells is the length of a bar in cells, so at least 1")
    if args.min_duration < 0:
        raise UsageError("--min-duration is a duration, so it cannot be negative")
    if args.min_length < 0:
        raise UsageError("--min-length is a length, so it cannot be negative")


def _renderers_of(cycles: Sequence[Dict[str, Any]]) -> Set[int]:
    """The processes the cycles came from, which the bars are labelled by."""
    return {cycle["pid"] for cycle in cycles}


def _select_cycles(
    cycles: List[Dict[str, Any]], args: argparse.Namespace
) -> List[Tuple[int, Dict[str, Any]]]:
    numbered = [
        (index, cycle)
        for index, cycle in enumerate(cycles, 1)
        if cycle["duration"] >= args.min_duration
    ]
    if args.order == "slowest":
        numbered.sort(key=lambda item: -item[1]["duration"])
    return numbered[: args.max_cycles]


class _Renderer:
    def __init__(
        self,
        data: Dict[str, Any],
        args: argparse.Namespace,
        vblank_interval: float,
        pids: Set[int],
    ) -> None:
        self.args = args
        self.vblank_interval = vblank_interval
        self.pids = pids
        self.resolution = int(round(msec_to_nsec(1) * args.resolution))
        # How many cells one refresh interval covers, and whether that is enough of
        # them to mark without drawing over the lane, which is what every reader of
        # this has to check first.
        self.vblank_cells = int(round(vblank_interval / args.resolution))
        self.marks_vblanks = self.vblank_cells >= MIN_VBLANK_CELLS
        self.color = args.color == "always" or (
            args.color == "auto" and sys.stdout.isatty()
        )
        # Indexed per process: two renderers render independently of one another, so
        # the marks of one say nothing about a cycle of the other.
        self.activity_marks = _collect_marks(data, MAIN_THREAD_ACTIVITIES)
        self.tile_marks = _collect_marks(data, ["PaintTile"])
        self.cycle_field = CYCLE_FIELD_WIDTH
        self.label_width = self.cycle_field + LANE_FIELD_WIDTH + 2

    def render(self, selected: List[Tuple[int, Dict[str, Any]]], total: int) -> None:
        self._size_labels(selected)
        minimum_length = self.args.min_length
        if self.marks_vblanks:
            # One cell past the interval rather than exactly one interval, or the
            # first marker falls one cell beyond the end of the bar it is meant to
            # cross, which is the whole reason for padding a short cycle.
            minimum_length = max(
                minimum_length, (self.vblank_cells + 1) * self.args.resolution
            )
        longest = max(cycle["duration"] for _, cycle in selected)
        cells = self._cell_count(max(minimum_length, longest))

        self._render_header(total, len(selected), cells)
        totals: Dict[str, float] = {}
        for position, (number, cycle) in enumerate(selected):
            # A cycle drawn as two lanes needs a blank line before the next one,
            # otherwise neighbouring cycles read as one four-lane block.
            if position and self.args.tile_lane:
                print()
            self._render_cycle(number, cycle, cells, minimum_length, totals)
        self._render_footer(totals, len(selected))

    def _size_labels(self, selected: List[Tuple[int, Dict[str, Any]]]) -> None:
        """Widen the label field to whatever the labels of these cycles need.

        A label wider than its field pushes the bar beside it out of the column the
        ruler and every other bar are drawn in, since formatting to a width pads a
        shorter label but does not cut a longer one.
        """
        self.cycle_field = max(
            [CYCLE_FIELD_WIDTH]
            + [len(self._cycle_label(number, cycle)) for number, cycle in selected]
        )
        self.label_width = self.cycle_field + LANE_FIELD_WIDTH + 2

    def _cycle_label(self, number: int, cycle: Dict[str, Any]) -> str:
        # Two renderers render independently of one another, so a bar says which one
        # it belongs to where the capture holds more than one.
        process = _process_suffix(cycle["pid"]) if len(self.pids) > 1 else ""
        return "#{:<6d}{:>8.2f} ms{}".format(number, cycle["duration"], process)

    def _cells_spanning(self, milliseconds: float) -> int:
        """The cells that many milliseconds covers, at least one.

        Rounded up rather than to the nearest, so the last part-filled cell of a
        cycle is still drawn. It reaches past the cycle, which paints nothing there
        because every window painted from is clamped to the cycle itself.
        """
        # Rounded before the ceiling, or a duration that is a whole number of cells
        # in exact arithmetic gains a cell from the error of dividing by 0.2.
        return max(1, math.ceil(round(milliseconds / self.args.resolution, 9)))

    def _cell_count(self, milliseconds: float) -> int:
        """As many cells as fit in a bar, which is what the cell limit limits."""
        return min(self._cells_spanning(milliseconds), self.args.max_cells)

    def _render_header(self, total: int, shown: int, cells: int) -> None:
        print(
            "Frame cycles: {} in the analyzed window, {} shown ({}), "
            "{:.4f} ms per cell.".format(
                total,
                shown,
                "slowest first" if self.args.order == "slowest" else "in order",
                self.args.resolution,
            )
        )
        if self.marks_vblanks:
            print(
                "Refresh interval: {:.4f} ms. A cycle reaching past the first {}"
                " missed that refresh, which showed the previous frame"
                " again.".format(self.vblank_interval, VBLANK_GLYPH)
            )
        elif self.vblank_interval > 0:
            print(
                "Refresh interval: {:.4f} ms, fewer than {} cells, so no refresh"
                " marker is drawn.".format(self.vblank_interval, MIN_VBLANK_CELLS)
            )
        else:
            print(
                "Refresh interval: unknown, since the capture holds no display"
                " refreshes, so no refresh marker is drawn."
            )
        print()
        print(self._ruler(cells))

    def _ruler(self, cells: int) -> str:
        cells_per_msec = 1 / self.args.resolution
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
            for offset, character in enumerate(str(millisecond)):
                if position + offset < cells:
                    labels[position + offset] = character
        # Indented past the label field and the border the cells begin after, so a
        # tick sits over the cell whose time it names rather than over the one
        # before it.
        indent = " " * (self.label_width + 1)
        return "{}{}\n{}{} [ms]".format(indent, "".join(labels), indent, "".join(ticks))

    def _ruler_step(self, cells: int, cells_per_msec: float, span: int) -> int:
        """Milliseconds between two labelled ticks.

        Wide enough that two labels cannot run into one another, which a step
        chosen for a count of labels alone is not: at one cell per millisecond it
        writes "10" and "11" into the same three cells. Sparse enough, too, that
        the ruler does not become a solid row of ticks.
        """
        width = len(str(span)) + 1
        magnitude = 1
        while True:
            for factor in (1, 2, 5):
                step = magnitude * factor
                spacing = step * cells_per_msec
                if spacing >= width and cells / spacing <= 20:
                    return step
            magnitude *= 10

    def _render_cycle(
        self,
        number: int,
        cycle: Dict[str, Any],
        cells: int,
        minimum_length: float,
        totals: Dict[str, float],
    ) -> None:
        # The whole cycle, not only the part of it that fits in a bar: the legend
        # averages what the engine did per cycle, and counting the drawn cells alone
        # would leave the tail of every truncated cycle out of it silently.
        cycle_cells = self._cells_spanning(cycle["duration"])
        activities = self._paint_activities(cycle, cycle_cells)
        for activity in activities:
            totals[activity] = totals.get(activity, 0) + self.args.resolution

        drawn_cells = self._cell_count(max(cycle["duration"], minimum_length))
        # Only the cycle being cut short is a truncation. The padding of a short bar
        # is not part of the cycle, so a bar whose padding did not fit has lost
        # nothing of what it draws.
        painted_cells = min(cycle_cells, drawn_cells)
        truncated = cycle_cells > drawn_cells
        print(
            _row_label(
                self._cycle_label(number, cycle),
                MAIN_LANE_LABEL,
                self.cycle_field,
            )
            + self._lane(activities[:painted_cells], drawn_cells, cells, truncated)
        )
        if not self.args.tile_lane:
            return
        print(
            _row_label("", TILE_LANE_LABEL, self.cycle_field)
            + self._tile_lane(cycle, painted_cells, drawn_cells, cells, truncated)
        )

    def _lane(
        self, activities: List[str], drawn_cells: int, cells: int, truncated: bool
    ) -> str:
        glyphs: List[Tuple[str, Optional[int]]] = []
        for index in range(drawn_cells):
            if self._is_vblank_boundary(index):
                glyphs.append((VBLANK_GLYPH, VBLANK_COLOR))
            elif index >= len(activities):
                glyphs.append((BEYOND_GLYPH, None))
            else:
                color, glyph, _ = ACTIVITY_STYLES[activities[index]]
                glyphs.append((BLOCK if self.color else glyph, color))
        return self._close_lane(glyphs, drawn_cells, cells, truncated)

    def _tile_lane(
        self,
        cycle: Dict[str, Any],
        cycle_cells: int,
        drawn_cells: int,
        cells: int,
        truncated: bool,
    ) -> str:
        concurrency = self._paint_tile_concurrency(cycle, cycle_cells)
        glyphs: List[Tuple[str, Optional[int]]] = []
        for index in range(drawn_cells):
            if self._is_vblank_boundary(index):
                glyphs.append((VBLANK_GLYPH, VBLANK_COLOR))
            elif index >= cycle_cells:
                glyphs.append((BEYOND_GLYPH, None))
            else:
                color, glyph, _ = TILE_LANE_LEVELS[
                    min(concurrency[index], len(TILE_LANE_LEVELS) - 1)
                ]
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
        """The cells as one string, with a colour escape only where it changes.

        A pair of escapes around every cell is six times the bytes of the cells
        themselves, and a bar is mostly runs of one colour.
        """
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

    def _is_vblank_boundary(self, index: int) -> bool:
        # Below a cell per interval there is no cell to mark, and dividing by that
        # count would be dividing by zero. A few cells per interval is no better:
        # the markers would be most of the lane.
        if not self.marks_vblanks:
            return False
        return index > 0 and index % self.vblank_cells == 0

    def _paint_activities(self, cycle: Dict[str, Any], cells: int) -> List[str]:
        lane = ["idle"] * cells
        marks = _marks_overlapping(
            self.activity_marks, cycle["pid"], cycle["begin_nsec"], cycle["end_nsec"]
        )
        # Longest first, so nested marks overwrite their parents.
        for mark in sorted(marks, key=lambda m: -m["duration"]):
            activity = MAIN_THREAD_ACTIVITIES[mark["name"]]
            for index in self._cell_range(cycle, mark, cells):
                lane[index] = activity
        return lane

    def _paint_tile_concurrency(self, cycle: Dict[str, Any], cells: int) -> List[int]:
        lane = [0] * cells
        marks = _marks_overlapping(
            self.tile_marks, cycle["pid"], cycle["begin_nsec"], cycle["end_nsec"]
        )
        for mark in marks:
            for index in self._cell_range(cycle, mark, cells):
                lane[index] += 1
        return lane

    def _cell_range(
        self, cycle: Dict[str, Any], mark: Dict[str, Any], cells: int
    ) -> range:
        """The cells of the bar the mark covers, clamped to the cycle it is drawn in.

        Clamped to the cycle rather than to the cells: the last cell of a bar reaches
        past the end of a cycle that is not a whole number of cells long, and the
        rendering update the next cycle begins with would be drawn into it.
        """
        origin = cycle["begin_nsec"]
        begin = max(mark_begin(mark), origin)
        end = min(mark["end_time"], cycle["end_nsec"])
        if end <= begin:
            return range(0)
        first = int((begin - origin) // self.resolution)
        last = int((end - origin - 1) // self.resolution)
        return range(max(0, first), min(cells - 1, last) + 1)

    def _colored(self, text: str, color: int) -> str:
        if not self.color:
            return text
        return "\x1b[38;5;{}m{}\x1b[0m".format(color, text)

    def _render_footer(self, totals: Dict[str, float], cycles: int) -> None:
        print()
        print(
            "Legend: time per cycle averaged over the drawn cycles, and the marks"
            " each\ncolor is drawn from."
        )
        width = max(len(ACTIVITY_STYLES[activity][2]) for activity in totals)
        for activity in LEGEND_ORDER:
            color, glyph, description = ACTIVITY_STYLES[activity]
            if activity not in totals:
                continue
            print(
                "  {} {:<{}} {:>7.3f} ms  {}".format(
                    self._colored(BLOCK * 2 if self.color else glyph * 2, color),
                    description,
                    width,
                    totals[activity] / cycles,
                    _marks_of_activity(activity),
                )
            )
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


def _marks_of_activity(activity: str) -> str:
    """The marks an activity aggregates, in the order they nest."""
    names = [
        name for name, mapped in MAIN_THREAD_ACTIVITIES.items() if mapped == activity
    ]
    return ", ".join(names) if names else "no mark covers these cells"


def _process_suffix(pid: int) -> str:
    return " [{}]".format(pid)


def _row_label(cycle: str, lane: str, cycle_field: int) -> str:
    return "{:<{}} {:>{}} ".format(cycle, cycle_field, lane, LANE_FIELD_WIDTH)


def _collect_marks(
    data: Dict[str, Any], mark_names: Iterable[str]
) -> Dict[int, MarkIndex]:
    """The marks of those names, indexed per process.

    Two renderers render independently of one another, so the marks of one say
    nothing about a cycle of the other and must not be painted into its bar.
    """
    marks = [
        mark
        for name in mark_names
        for mark in data["marks"].get(name, [])
        if mark["duration"] > 0
    ]
    return {
        pid: MarkIndex(process_marks)
        for pid, process_marks in marks_by_process(marks).items()
    }


def _marks_overlapping(
    indices: Dict[int, MarkIndex], pid: int, begin: int, end: int
) -> List[Dict[str, Any]]:
    index = indices.get(pid)
    return index.marks_overlapping(begin, end) if index is not None else []
