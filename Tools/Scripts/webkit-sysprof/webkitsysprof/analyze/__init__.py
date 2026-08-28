import argparse
import bisect
import logging
import re
import collections
import json
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

from ..parser import parse
from . import explanations
from ..cycles import PHASES, calculate_frame_cycles
from ..utils import (
    UsageError,
    check_timespan_holds_data,
    mark_begin,
    median_vblank_interval,
    msec_to_sec,
    nsec_to_msec,
    nsec_to_sec,
    parse_timespan_argument,
    merged_spans,
    display_refreshes,
    intervals_between_marks,
    sample_statistics,
    sysprof_data_with_marks_by_name,
    trim_marks_by_name_to_timespan,
)

# Columns of the statistics tables, as (label, statistics key, percentile) triples.
# The percentile is None for statistics that are not percentiles. The median is taken
# from the "median" key rather than from the 50th percentile: both describe the same
# quantity, but the former is exact while the latter is interpolated.
STATISTICS_COLUMNS: List[Tuple[str, str, Optional[int]]] = [
    ("#", "n", None),
    ("mean", "mean", None),
    ("min", "min", None),
    ("P25", "percentiles", 25),
    ("median", "median", None),
    ("P75", "percentiles", 75),
    ("P99", "percentiles", 99),
    ("max", "max", None),
]

# Derived from the columns, so the percentiles computed and the ones printed cannot
# drift apart. The 50th is not a column - the median column prints the exact value -
# but stays for the JSON report, whose consumers already read it.
PERCENTILE_SHOWN_AS_MEDIAN = 50
RELEVANT_PERCENTILES = sorted(
    {percentile for _, _, percentile in STATISTICS_COLUMNS if percentile is not None}
    | {PERCENTILE_SHOWN_AS_MEDIAN}
)

MARKS_RELEVANT_FOR_STATISTICS = [
    "EventLoopRun",
    "RAFCallback",
    "LayerTreeHostRenderingUpdate",
    "FinalizeRenderingUpdate",
    "PerformLayout",
    "PerformSubtreesLayout",
    "StyleRecalc",
    "RenderTreeBuild",
    "CompositingUpdate",
    "FlushCompositingState",
    "RenderLayerTree",
    "PaintToGLContext",
    "WaitForCompositionCompletion",
    "RecordTile",
    "UpdateTiles",
    "PaintTile",
    "UpdateTile",
    "SkiaBackingStoreTileUpdate",
]

# PaintTile messages look like "Skia/CPU threaded, dirty region 768x512+256+56",
# where the geometry is "<x>x<y>+<width>+<height>".
DIRTY_REGION_RE = re.compile(r"(\d+)x(\d+)\+(\d+)\+(\d+)")
REASONS_RE = re.compile(r"^reasons:([^|\n]*)")
TASKS_RE = re.compile(r"^tasks:\s*(\d+)")
SUBTREES_RE = re.compile(r"^subtrees:\s*(\d+)")
TILES_RE = re.compile(r"^dirty tiles:\s*(\d+)")


def _frame_rendering_reason(message: str) -> str:
    """Why a frame was composited, as the DidRenderFrame message tells it."""
    match = REASONS_RE.match(message.strip())
    return (match.group(1).strip() if match is not None else "") or "_none"


def _counted(message: str, pattern: "re.Pattern[str]") -> Optional[int]:
    """The number the pattern names in a mark message, None where it does not."""
    match = pattern.match(message.strip())
    return int(match.group(1)) if match is not None else None


def _dirty_region_pixels(message: str) -> Optional[int]:
    match = DIRTY_REGION_RE.search(message)
    if match is None:
        return None
    return int(match.group(3)) * int(match.group(4))


STATISTICAL_DATA_EXTRACTORS: Dict[str, Callable[[Dict[str, Any]], Dict[str, Any]]] = {
    "_": lambda data: {
        "duration": nsec_to_msec(data["duration"]),
    },
    "EventLoopRun": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "tasks": _counted(data["message"], TASKS_RE),
    },
    "PerformSubtreesLayout": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "subtrees": _counted(data["message"], SUBTREES_RE),
    },
    "UpdateTiles": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "tiles": _counted(data["message"], TILES_RE),
    },
    "PaintTile": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "dirty_pixels": _dirty_region_pixels(data["message"]),
    },
}


def analyze(args: argparse.Namespace) -> None:
    if args.explain and args.format != "text":
        raise UsageError("--explain only applies to the text format")
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    capture_begin, capture_end = parsed_data["document"]["timespan"]
    check_timespan_holds_data(capture_begin, capture_end, timespan_begin, timespan_end)
    untrimmed_data = sysprof_data_with_marks_by_name(parsed_data)
    data = trim_marks_by_name_to_timespan(untrimmed_data, timespan_begin, timespan_end)
    logging.info("Preparing report...")
    # Frame cycles are built from the untrimmed capture and only then cut down to
    # the timespan: trimming drops the marks that cross a timespan boundary, and a
    # cycle at that boundary needs them.
    report = _prepare_report(data, untrimmed_data)

    logging.info("Rendering report...")
    if args.format == "text":
        _render_text_report(report, args.explain)
    elif args.format == "json":
        print(json.dumps(report))
    else:
        raise NotImplementedError


def _calculate_statistics(values: Sequence[Union[int, float]]) -> Dict[str, Any]:
    return sample_statistics(values, RELEVANT_PERCENTILES)


def _prepare_report(
    sysprof_data: Dict[str, Any],
    untrimmed_sysprof_data: Dict[str, Any],
) -> Dict[str, Any]:
    # TODO: Dropped frames.
    # TODO: Interaction latency.
    # TODO: System metrics.
    timespan_begin = sysprof_data["document"]["timespan"]["begin"]
    timespan_end = sysprof_data["document"]["timespan"]["end"]
    document = dict(sysprof_data["document"])
    document["timespan"] = {
        "begin": nsec_to_msec(timespan_begin),
        "end": nsec_to_msec(timespan_end),
    }
    mark_statistics = _calculate_statistics_input_data(
        MARKS_RELEVANT_FOR_STATISTICS, sysprof_data
    )
    for mark_name in mark_statistics:
        for input_name in mark_statistics[mark_name]:
            mark_statistics[mark_name][input_name] = _calculate_statistics(
                mark_statistics[mark_name][input_name]
            )
    report = {
        "document": document,
        "statistics": mark_statistics,
        "rendering": _prepare_rendering_report(
            sysprof_data, display_refreshes(sysprof_data)
        ),
        "frame_cycle": _prepare_frame_cycle_report(
            untrimmed_sysprof_data,
            timespan_begin,
            timespan_end,
            # From the whole capture, not from the window: a narrow window holds
            # too few vblanks, and the cycles come from the whole capture too.
            median_vblank_interval(
                display_refreshes(untrimmed_sysprof_data)
            ),
        ),
    }

    return report


def _prepare_rendering_report(
    sysprof_data: Dict[str, Any], vblanks: Sequence[Dict[str, Any]]
) -> Dict[str, Any]:
    document_duration = nsec_to_sec(
        sysprof_data["document"]["timespan"]["end"]
        - sysprof_data["document"]["timespan"]["begin"]
    )
    intervals = intervals_between_marks(vblanks)
    vblanks_per_rendering_update = _calculate_vblanks_per_rendering_update(
        vblanks, sysprof_data["marks"].get("LayerTreeHostRenderingUpdate", [])
    )
    did_render_frames = sysprof_data["marks"].get("DidRenderFrame", [])
    report = {
        "vblanks": len(vblanks),
        "vblank_interval_statistics": _calculate_statistics(intervals),
        "frames_rendered": len(did_render_frames),
        "theoretical_fps": (
            len(did_render_frames) / document_duration
            if document_duration > 0
            else None
        ),
        "frame_rendering_reasons": collections.Counter(
            [_frame_rendering_reason(mark["message"]) for mark in did_render_frames]
        ),
        "frame_compositions_per_vblank_statistics": _calculate_statistics(
            _calculate_frame_compositions_per_vblank(vblanks, did_render_frames)
        ),
        "vblanks_per_rendering_update": {
            "n_greater_than_1": len(
                [1 for vpru in vblanks_per_rendering_update if vpru > 1]
            ),
            "statistics": _calculate_statistics(vblanks_per_rendering_update),
        },
    }
    return report


def _covered_duration(cycles: Sequence[Dict[str, Any]]) -> float:
    """How much of the timeline the cycles span, in milliseconds.

    Their spans are merged rather than summed, since the cycles of two processes
    rendering at once overlap and would otherwise cover more time than there is.
    """
    return nsec_to_msec(
        sum(
            end - begin
            for begin, end in merged_spans(
                (cycle["begin_nsec"], cycle["end_nsec"]) for cycle in cycles
            )
        )
    )


def _cycle_in_vblank_intervals(
    cycles: int,
    median_duration: float,
    vblank_interval: Optional[float],
) -> Tuple[Optional[float], Optional[str]]:
    if not cycles:
        return None, "no_cycles"
    if vblank_interval is None:
        return None, "no_vblank_interval"
    if not vblank_interval:
        return None, "zero_vblank_interval"
    if not median_duration:
        return None, "zero_length_cycle"
    return median_duration / vblank_interval, None


def _prepare_frame_cycle_report(
    sysprof_data: Dict[str, Any],
    timespan_begin: int,
    timespan_end: int,
    vblank_interval: Optional[float],
) -> Dict[str, Any]:
    cycles = calculate_frame_cycles(sysprof_data, timespan_begin, timespan_end)
    durations = [cycle["duration"] for cycle in cycles]
    resolved = [cycle for cycle in cycles if cycle["phases"] is not None]
    # The phase statistics cover the resolved cycles only, so the duration they are
    # shares of has to cover the same ones. Taken from all cycles instead, the four
    # shares do not sum to 100%.
    resolved_durations = [cycle["duration"] for cycle in resolved]

    duration_statistics = _calculate_statistics(durations)
    median_duration = duration_statistics.get("median", 0.0)
    analyzed_duration = nsec_to_msec(timespan_end - timespan_begin)
    overrunning = [cycle for cycle in resolved if cycle["composition_overrun"] > 0]
    intervals_per_cycle, unknown_reason = _cycle_in_vblank_intervals(
        len(cycles), median_duration, vblank_interval
    )
    return {
        "cycles": len(cycles),
        "processes": len({cycle["pid"] for cycle in cycles}),
        "duration_statistics": duration_statistics,
        "implied_fps": 1 / msec_to_sec(median_duration) if median_duration else None,
        "coverage": (
            _covered_duration(cycles) / analyzed_duration
            if analyzed_duration > 0
            else None
        ),
        "capture_vblank_interval": vblank_interval,
        "vblank_intervals_per_cycle": intervals_per_cycle,
        "vblank_intervals_per_cycle_unknown": unknown_reason,
        "phase_statistics": {
            phase: _calculate_statistics([cycle["phases"][phase] for cycle in resolved])
            for phase in PHASES
        },
        "resolved_duration_statistics": _calculate_statistics(resolved_durations),
        "overrunning_composition_statistics": _calculate_statistics(
            [cycle["composition_overrun"] for cycle in overrunning]
        ),
    }


def _calculate_frame_compositions_per_vblank(
    vblanks: Sequence[Dict[str, Any]], compositing_finishes: Sequence[Dict[str, Any]]
) -> List[int]:
    """Compositions finished within each interval between two consecutive vblanks."""
    if len(vblanks) < 2:
        return []
    vblank_ends = [vblank["end_time"] for vblank in vblanks]
    compositions_per_interval = [0] * (len(vblanks) - 1)
    for compositing_finish in compositing_finishes:
        # Each interval runs from one refresh to the next, the later one included,
        # so a composition ending exactly on a refresh belongs to the interval that
        # ends there.
        interval = bisect.bisect_left(vblank_ends, compositing_finish["end_time"]) - 1
        if not 0 <= interval < len(compositions_per_interval):
            # Composited before the first vblank of the timespan or after the last
            # one, so there is no interval between two refreshes it belongs to.
            continue
        compositions_per_interval[interval] += 1
    return compositions_per_interval


def _calculate_vblanks_per_rendering_update(
    vblanks: Sequence[Dict[str, Any]], rendering_updates: Sequence[Dict[str, Any]]
) -> List[int]:
    """How many display refreshes each rendering update spanned."""
    if not vblanks:
        return []
    vblank_ends = [vblank["end_time"] for vblank in vblanks]
    counts = []
    for rendering_update in rendering_updates:
        begin, end = mark_begin(rendering_update), rendering_update["end_time"]
        if end < vblank_ends[0] or begin > vblank_ends[-1]:
            # Outside the refreshes the timespan holds, so they say nothing about
            # this update, rather than saying it spanned none of them.
            continue
        first = bisect.bisect_right(vblank_ends, begin)
        counts.append(max(bisect.bisect_right(vblank_ends, end) - first, 1))
    return counts


def _calculate_statistics_input_data(
    mark_names: List[str], sysprof_data: Dict[str, Any]
) -> Dict[str, Any]:
    statistics_input_data: Dict[str, Any] = {}

    for mark_name in mark_names:
        statistics_input_data[mark_name] = {}
        if mark_name not in sysprof_data["marks"]:
            continue
        extractor = (
            STATISTICAL_DATA_EXTRACTORS[mark_name]
            if mark_name in STATISTICAL_DATA_EXTRACTORS
            else STATISTICAL_DATA_EXTRACTORS["_"]
        )
        for mark in sysprof_data["marks"][mark_name]:
            for k, v in extractor(mark).items():
                if v is not None:
                    statistics_input_data[mark_name].setdefault(k, []).append(v)

    return statistics_input_data


def _render_text_report(report: Dict[str, Any], explain: bool = False) -> None:
    print(
        "Timespan: {:.4f} - {:.4f} [s]".format(
            msec_to_sec(report["document"]["timespan"]["begin"]),
            msec_to_sec(report["document"]["timespan"]["end"]),
        )
    )
    print("Duration: {:.4f} [s]".format(_analyzed_duration_sec(report)))

    print()
    print("Rendering:")
    print(f"- vblanks: {report['rendering']['vblanks']}")
    vblank_interval_statistics = _statistics_to_strings(
        report["rendering"]["vblank_interval_statistics"]
    )
    print(
        "- vblank intervals: (min: {}, median: {}, max: {})".format(
            vblank_interval_statistics["min"],
            vblank_interval_statistics["median"],
            vblank_interval_statistics["max"],
        )
    )
    vblanks_per_rendering_update_statistics = _statistics_to_strings(
        report["rendering"]["vblanks_per_rendering_update"]["statistics"], "{:.4g}"
    )
    print("- vblanks per LayerTreeHostRenderingUpdate: ", end="")
    print(
        "(min: {}, {}, max: {})".format(
            vblanks_per_rendering_update_statistics["min"],
            ", ".join(_percentile_strings(vblanks_per_rendering_update_statistics)),
            vblanks_per_rendering_update_statistics["max"],
        )
    )
    print(
        "  - more than 1 per update: {}".format(
            report["rendering"]["vblanks_per_rendering_update"]["n_greater_than_1"]
        )
    )
    print(f"- frames rendered: {report['rendering']['frames_rendered']}")
    theoretical_fps = report["rendering"]["theoretical_fps"]
    print(
        "- theoretical FPS: {}".format(
            "{:.2f}".format(theoretical_fps) if theoretical_fps is not None else "-"
        )
    )
    print("- frame rendering reasons:")
    for reason in sorted(report["rendering"]["frame_rendering_reasons"].keys()):
        print(
            "  - {}: {}".format(
                explanations.UNNAMED_FRAME_RENDERING_REASONS.get(reason, reason),
                report["rendering"]["frame_rendering_reasons"][reason],
            )
        )
    frame_compositions_per_vblank_statistics = _statistics_to_strings(
        report["rendering"]["frame_compositions_per_vblank_statistics"], "{:.4g}"
    )
    print(
        "- frame compositions per vblank:"
        " (min: {}, mean: {}, median: {}, max: {})".format(
            frame_compositions_per_vblank_statistics["min"],
            frame_compositions_per_vblank_statistics["mean"],
            frame_compositions_per_vblank_statistics["median"],
            frame_compositions_per_vblank_statistics["max"],
        )
    )
    if explain:
        print()
        print(_theoretical_fps_explanation(report))

    _render_frame_cycle_numbers(report["frame_cycle"])
    if explain:
        print()
        print(explanations.FRAME_CYCLE)

    print()
    print("Statistics (durations):")
    _render_statistics_rows_to_stdout(
        _prepare_statistics_rows(
            [
                ("JS", "EventLoopRun", "ms", "duration"),
                ("Styling", "StyleRecalc", "ms", "duration"),
                ("Styling", "RenderTreeBuild", "ms", "duration"),
                ("Layout", "PerformSubtreesLayout", "ms", "duration"),
                ("Layout", "PerformLayout", "ms", "duration"),
                ("Rendering", "LayerTreeHostRenderingUpdate", "ms", "duration"),
                ("Rendering", "FinalizeRenderingUpdate", "ms", "duration"),
                ("Rendering/JS", "RAFCallback", "ms", "duration"),
                ("Compositing", "CompositingUpdate", "ms", "duration"),
                ("Compositing", "FlushCompositingState", "ms", "duration"),
                ("Compositing", "RenderLayerTree", "ms", "duration"),
                ("Compositing", "PaintToGLContext", "ms", "duration"),
                ("Compositing", "WaitForCompositionCompletion", "ms", "duration"),
                ("Tiles", "RecordTile", "ms", "duration"),
                ("Tiles", "UpdateTiles", "ms", "duration"),
                ("Tiles", "PaintTile", "ms", "duration"),
                ("Tiles", "UpdateTile", "ms", "duration"),
                ("Tiles", "SkiaBackingStoreTileUpdate", "ms", "duration"),
            ],
            report,
        )
    )
    if explain:
        print()
        print(explanations.MARK_STATISTICS)

    print()
    print("Statistics (other):")
    _render_statistics_rows_to_stdout(
        _prepare_statistics_rows(
            [
                ("JS", "EventLoopRun", "#tasks", "tasks"),
                ("Layout", "PerformSubtreesLayout", "#subtrees", "subtrees"),
                ("Tiles", "UpdateTiles", "#tiles", "tiles"),
                ("Tiles", "PaintTile", "#pixels", "dirty_pixels"),
            ],
            report,
        )
    )


def _analyzed_duration_sec(report: Dict[str, Any]) -> float:
    """How long the analyzed timespan is, in seconds."""
    return msec_to_sec(
        report["document"]["timespan"]["end"] - report["document"]["timespan"]["begin"]
    )


def _theoretical_fps_explanation(report: Dict[str, Any]) -> str:
    rendering = report["rendering"]
    if rendering["theoretical_fps"] is None:
        return explanations.UNKNOWN_THEORETICAL_FPS.format(
            frames=rendering["frames_rendered"],
            refresh_rate=_refresh_rate_explanation(report),
        )
    return explanations.THEORETICAL_FPS.format(
        frames=rendering["frames_rendered"],
        duration=_analyzed_duration_sec(report),
        fps="{:.2f}".format(rendering["theoretical_fps"]),
        refresh_rate=_refresh_rate_explanation(report),
    )


def _refresh_rate_explanation(report: Dict[str, Any]) -> str:
    rendering = report["rendering"]
    interval = rendering["vblank_interval_statistics"].get("median")
    vblanks = rendering["vblanks"]
    if interval:
        refresh_rate = explanations.REFRESH_RATE.format(
            rate=1 / msec_to_sec(interval), interval=interval
        )
    elif interval is None:
        refresh_rate = explanations.REFRESH_RATE_WITHOUT_INTERVAL.format(
            vblanks=vblanks, plural="" if vblanks == 1 else "s"
        )
    else:
        refresh_rate = explanations.REFRESH_RATE_ZERO_INTERVAL.format(vblanks=vblanks)
    return refresh_rate


def _percentile_strings(strings: Dict[str, Any]) -> List[str]:
    """The percentiles of a statistic, and its median among them, in order.

    Takes the strings _statistics_to_strings() built, not the statistic itself.
    """
    return [
        (
            "median: {}".format(strings["median"])
            if percentile == PERCENTILE_SHOWN_AS_MEDIAN
            else "P{}: {}".format(percentile, strings["percentiles"][percentile])
        )
        for percentile in RELEVANT_PERCENTILES
    ]


def _analyzed_cycles(frame_cycle: Dict[str, Any]) -> int:
    """How many cycles the phase statistics were taken from."""
    return frame_cycle["resolved_duration_statistics"].get("n", 0)


def _render_frame_cycle_numbers(frame_cycle: Dict[str, Any]) -> None:
    print()
    print("Frame cycle:")
    print(f"- cycles: {frame_cycle['cycles']}")
    if frame_cycle["processes"] > 1:
        print(
            "- rendered by {} processes, whose frames the medians below"
            " describe together".format(frame_cycle["processes"])
        )

    if frame_cycle["cycles"] == 0:
        print(
            "- no cycles here: no two consecutive LayerTreeHostRenderingUpdate"
            " marks of one process begin within the analyzed timespan"
        )
        return

    duration_statistics = _statistics_to_strings(frame_cycle["duration_statistics"])
    print(
        "- cycle duration: (min: {}, {}, max: {}) [ms]".format(
            duration_statistics["min"],
            ", ".join(_percentile_strings(duration_statistics)),
            duration_statistics["max"],
        )
    )
    implied_fps = frame_cycle["implied_fps"]
    print(
        "- implied FPS (1000 / median cycle): {}".format(
            "{:.2f}".format(implied_fps) if implied_fps is not None else "-"
        )
    )
    coverage = frame_cycle["coverage"]
    print(
        "- cycles cover {} of the analyzed duration".format(
            "{:.1f}%".format(coverage * 100) if coverage is not None else "-"
        )
    )
    if frame_cycle["vblank_intervals_per_cycle"] is None:
        print(
            "- median cycle in vblank intervals: - ({})".format(
                explanations.UNKNOWN_VBLANK_INTERVALS_PER_CYCLE[
                    frame_cycle["vblank_intervals_per_cycle_unknown"]
                ]
            )
        )
    else:
        print(
            "- median cycle in vblank intervals: {:.2f}"
            " ({:.4f} ms per vblank, from the whole capture)".format(
                frame_cycle["vblank_intervals_per_cycle"],
                frame_cycle["capture_vblank_interval"],
            )
        )

    # Shares of the median duration of the analyzed cycles, not of all of them,
    # since the phase medians cover the analyzed ones only.
    median_duration = frame_cycle["resolved_duration_statistics"].get("median", 0)
    analyzed = "{} of {} cycles analyzed".format(
        _analyzed_cycles(frame_cycle), frame_cycle["cycles"]
    )
    # Each phase is its own median, and medians do not add up, so the shares below
    # need not total 100%.
    if median_duration:
        analyzed += ", median analyzed cycle {:.4f} ms".format(median_duration)
    print(f"- phases (median, {analyzed}):")
    shares_shown = False
    for phase in PHASES:
        label = phase.replace("_", " ")
        phase_statistics = frame_cycle["phase_statistics"][phase]
        if not phase_statistics:
            print(f"  - {label}: -")
            continue
        share = "-"
        if median_duration:
            share = "{:.1f}%".format(phase_statistics["median"] / median_duration * 100)
            shares_shown = True
        print(
            "  - {}: {:.4f} ms ({} of the cycle)".format(
                label, phase_statistics["median"], share
            )
        )
    if shares_shown:
        print(
            "  (each phase is a median of its own,"
            " so the shares need not total 100%)"
        )
    overrun_statistics = frame_cycle["overrunning_composition_statistics"]
    if not _analyzed_cycles(frame_cycle):
        print("- composition overrunning the cycle: -")
    elif overrun_statistics:
        print(
            "- composition overrunning the cycle: in {} of {} analyzed cycles,"
            " by {:.4f} ms median".format(
                overrun_statistics["n"],
                _analyzed_cycles(frame_cycle),
                overrun_statistics["median"],
            )
        )
    else:
        print(
            "- composition overrunning the cycle: in none of {} analyzed"
            " cycles".format(_analyzed_cycles(frame_cycle))
        )


def _render_statistics_rows_to_stdout(statistics_rows: List[List[str]]) -> None:
    column_max_sizes = [
        max([len(row[col]) for row in statistics_rows])
        for col in range(len(statistics_rows[0]))
    ]
    for i, statistics_row in enumerate(statistics_rows):
        format_str = " | ".join(["{{:>{}}}".format(cms) for cms in column_max_sizes])
        print("|", format_str.format(*statistics_row), "|")
        if i == 0:
            print("|", format_str.format(*"-" * len(statistics_row)), "|")


def _prepare_statistics_rows(
    keys: List[Tuple[str, str, str, str]], report: Dict[str, Any]
) -> List[List[str]]:
    statistics_rows = [
        ["category", "mark", "unit"] + [label for label, _, _ in STATISTICS_COLUMNS],
    ]
    for category, mark, unit, data_key in keys:
        data = {}
        if mark in report["statistics"] and data_key in report["statistics"][mark]:
            data = report["statistics"][mark][data_key]
        statistics_rows.append(_prepare_statistics_row(category, mark, unit, data))
    return statistics_rows


def _prepare_statistics_row(
    category: str, mark: str, unit: str, raw_data: Dict[str, Any]
) -> List[str]:
    data = _statistics_to_strings(raw_data)
    return [category, mark, unit] + [
        data[key] if percentile is None else data["percentiles"][percentile]
        for _, key, percentile in STATISTICS_COLUMNS
    ]


def _statistics_to_strings(
    statistic: Dict[str, Any], number: str = "{:.4f}"
) -> Dict[str, Any]:
    """The numbers of a statistic as strings, `-` where it holds none."""

    def format_statistic(
        data: Dict[str, Any],
        key: str,
        default: str = "-",
        subkey: Optional[str] = None,
    ) -> str:
        if key not in data:
            return default
        value = data[key]
        if subkey is not None:
            if subkey not in value:
                return default
            value = value[subkey]
        if key == "n":
            return str(value)
        return number.format(value)

    return {
        key: format_statistic(statistic, key, "0" if key == "n" else "-")
        for key in ["n", "min", "max", "mean", "stddev", "median"]
    } | {
        "percentiles": {
            percentile: format_statistic(statistic, "percentiles", "-", str(percentile))
            for percentile in RELEVANT_PERCENTILES
        }
    }
