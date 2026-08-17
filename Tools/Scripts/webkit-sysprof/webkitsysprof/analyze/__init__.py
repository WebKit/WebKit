import argparse
import logging
import re
import statistics
import collections
import json
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

from ..parser import parse
from ..utils import (
    nsec_to_msec,
    nsec_to_sec,
    msec_to_sec,
    parse_timespan_argument,
    trim_sysprof_data_to_timespan,
)

RELEVANT_PERCENTILES = [25, 50, 75, 99]

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
        "tasks": int(data["message"].split()[1]),
    },
    "PerformSubtreesLayout": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "subtrees": int(data["message"].split()[1]) if data["message"] != "" else None,
    },
    "UpdateTiles": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "tiles": int(data["message"].split()[2]) if data["message"] != "" else None,
    },
    "PaintTile": lambda data: STATISTICAL_DATA_EXTRACTORS["_"](data)
    | {
        "dirty_pixels": _dirty_region_pixels(data["message"]),
    },
}


def analyze(args: argparse.Namespace) -> None:
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    trimmed_data = trim_sysprof_data_to_timespan(
        parsed_data, timespan_begin, timespan_end
    )
    data = _sysprof_data_to_high_level_representation(trimmed_data)
    logging.info("Preparing report...")
    report = _prepare_report(data, timespan_begin, timespan_end)

    logging.info("Rendering report...")
    if args.format == "text":
        _render_text_report(report)
    elif args.format == "json":
        print(json.dumps(report))
    else:
        raise NotImplementedError


def _sysprof_data_to_high_level_representation(
    sysprof_data: Dict[str, Any],
) -> Dict[str, Any]:
    high_level_representation: Dict[str, Any] = {
        "document": sysprof_data["document"],
        "all_marks": sysprof_data["marks"],
        "marks": {},
    }
    high_level_representation["document"]["timespan"] = {
        "begin": sysprof_data["document"]["timespan"][0],
        "end": sysprof_data["document"]["timespan"][1],
    }
    for mark in sysprof_data["marks"]:
        high_level_representation["marks"].setdefault(mark["name"], []).append(mark)
    for mark_name in high_level_representation["marks"]:
        high_level_representation["marks"][mark_name].sort(key=lambda m: m["end_time"])
    return high_level_representation


def _prepare_report(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Dict[str, Any]:
    # TODO: Frame time.
    # TODO: Dropped frames.
    # TODO: Interaction latency.
    # TODO: System metrics.
    report = {
        "document": sysprof_data["document"],
        "statistics": {},
        "rendering": _prepare_rendering_report(sysprof_data),
    }

    statistics_input_data = _calculate_statistics_input_data(
        MARKS_RELEVANT_FOR_STATISTICS, sysprof_data
    )
    statistics = statistics_input_data
    for mark_name in statistics:
        for input_name in statistics[mark_name]:
            statistics[mark_name][input_name] = _calculate_statistics(
                statistics[mark_name][input_name]
            )
    report["statistics"].update(statistics)
    report["document"]["timespan"]["begin"] = nsec_to_msec(
        report["document"]["timespan"]["begin"]
    )
    report["document"]["timespan"]["end"] = nsec_to_msec(
        report["document"]["timespan"]["end"]
    )

    return report


def _prepare_rendering_report(sysprof_data: Dict[str, Any]) -> Dict[str, Any]:
    document_duration = nsec_to_sec(
        sysprof_data["document"]["timespan"]["end"]
        - sysprof_data["document"]["timespan"]["begin"]
    )
    vblanks = sysprof_data["marks"].get("DisplayLinkUpdate", [])
    vblanks_per_rendering_update = _calculate_vblanks_per_rendering_update(sysprof_data)
    did_render_frames = sysprof_data["marks"].get("DidRenderFrame", [])
    report = {
        "vblanks": len(vblanks),
        "vblank_interval_statistics": _calculate_statistics(
            [
                nsec_to_msec(vblanks[i]["end_time"] - vblanks[i - 1]["end_time"])
                for i in range(1, len(vblanks))
            ]
        ),
        "frames_rendered": len(did_render_frames),
        "theoretical_fps": (
            len(did_render_frames) / document_duration if document_duration != 0 else 0
        ),
        "frame_rendering_reasons": dict(
            collections.Counter(
                [mark["message"].split(":")[1].strip() for mark in did_render_frames]
            )
        ),
        "frame_compositions_per_vblank_statistics": _calculate_statistics(
            _calculate_frame_compositions_per_vblank(sysprof_data)
        ),
        "vblanks_per_rendering_update": {
            "n_greater_than_1": len(
                [1 for vpru in vblanks_per_rendering_update if vpru > 1]
            ),
            "statistics": _calculate_statistics(vblanks_per_rendering_update),
        },
    }
    return report


def _calculate_frame_compositions_per_vblank(sysprof_data: Dict[str, Any]) -> List[int]:
    vblanks = sysprof_data["marks"].get("DisplayLinkUpdate", [])
    if len(vblanks) == 0:
        return []
    compositing_finishes = sysprof_data["marks"].get("DidRenderFrame", [])
    frame_compositions_per_vblank = [0]
    vblanks_iterator = 0
    for compositing_finish in compositing_finishes:
        while (
            vblanks_iterator < len(vblanks)
            and vblanks[vblanks_iterator]["end_time"] <= compositing_finish["end_time"]
        ):
            vblanks_iterator += 1
            if vblanks_iterator < len(vblanks):
                frame_compositions_per_vblank.append(0)
        if vblanks_iterator == 0:
            continue
        frame_compositions_per_vblank[vblanks_iterator - 1] += 1

    return frame_compositions_per_vblank


def _calculate_vblanks_per_rendering_update(sysprof_data: Dict[str, Any]) -> List[int]:
    vblanks = sysprof_data["marks"].get("DisplayLinkUpdate", [])
    if len(vblanks) == 0:
        return []
    rendering_updates = sysprof_data["marks"]["LayerTreeHostRenderingUpdate"]
    vblanks_iterator = 0
    vblanks_per_rendering_update = []
    for rendering_update in rendering_updates:
        rendering_update_begin = (
            rendering_update["end_time"] - rendering_update["duration"]
        )
        while (
            vblanks_iterator < len(vblanks)
            and vblanks[vblanks_iterator]["end_time"] <= rendering_update_begin
        ):
            vblanks_iterator += 1
        if (
            vblanks_iterator >= len(vblanks)
            or vblanks[vblanks_iterator]["end_time"] <= rendering_update_begin
        ):
            break
        if vblanks_iterator == 0:
            continue
        vblanks_during_rendering_update = 0
        while (
            vblanks_iterator < len(vblanks)
            and vblanks[vblanks_iterator]["end_time"] <= rendering_update["end_time"]
        ):
            vblanks_iterator += 1
            vblanks_during_rendering_update += 1
        vblanks_per_rendering_update.append(
            1 + max(vblanks_during_rendering_update - 1, 0)
        )
    return vblanks_per_rendering_update


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


def _calculate_statistics(values: Sequence[Union[int, float]]) -> Dict[str, Any]:
    if values == []:
        return {}
    return {
        "n": len(values),
        "min": min(values),
        "max": max(values),
        "mean": statistics.mean(values),
        "stddev": statistics.stdev(values) if len(values) > 1 else 0,
        "median": statistics.median(values),
        "percentiles": {
            k: v
            for k, v in dict(
                zip(range(1, 100), statistics.quantiles(values, n=100))
            ).items()
            if k in RELEVANT_PERCENTILES
        },
    }


def _render_text_report(report: Dict[str, Any]) -> None:
    print(
        "Timespan: {:.4f} - {:.4f} [s]".format(
            abs(msec_to_sec(report["document"]["timespan"]["begin"])),
            abs(msec_to_sec(report["document"]["timespan"]["end"])),
        )
    )
    print(
        "Duration: {:.4f} [s]".format(
            abs(
                msec_to_sec(
                    report["document"]["timespan"]["end"]
                    - report["document"]["timespan"]["begin"]
                )
            )
        )
    )

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
        report["rendering"]["vblanks_per_rendering_update"]["statistics"]
    )
    print("- vblanks per LayerTreeHostRenderingUpdate: ", end="")
    print(
        "(min: {}, median: {}, P75: {}, P99: {}, max: {})".format(
            vblanks_per_rendering_update_statistics["min"],
            vblanks_per_rendering_update_statistics["median"],
            vblanks_per_rendering_update_statistics["percentiles"][75],
            vblanks_per_rendering_update_statistics["percentiles"][99],
            vblanks_per_rendering_update_statistics["max"],
        )
    )
    print(
        "  - more than 1 per update: {}".format(
            report["rendering"]["vblanks_per_rendering_update"]["n_greater_than_1"]
        )
    )
    print(f"- frames rendered: {report['rendering']['frames_rendered']}")
    print(f"- theoretical FPS: {report['rendering']['theoretical_fps']:.2f}")
    print("- frame rendering reasons:")
    for reason in sorted(report["rendering"]["frame_rendering_reasons"].keys()):
        print(
            "  - {}: {}".format(
                reason, report["rendering"]["frame_rendering_reasons"][reason]
            )
        )
    frame_compositions_per_vblank_statistics = _statistics_to_strings(
        report["rendering"]["frame_compositions_per_vblank_statistics"]
    )
    print(
        "- frame compositions per vblank: (min: {}, median: {}, max: {})".format(
            frame_compositions_per_vblank_statistics["min"],
            frame_compositions_per_vblank_statistics["median"],
            frame_compositions_per_vblank_statistics["max"],
        )
    )

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
    print()
    print(
        "  StyleRecalc is an umbrella mark: it nests RenderTreeBuild,\n"
        "  PerformSubtreesLayout and CompositingUpdate, so its duration is not\n"
        "  style resolution alone. Subtract the nested marks to get that.\n"
        "  CompositingUpdate runs twice per rendering update, once inside\n"
        "  StyleRecalc and once after it, and the second one is much shorter, so\n"
        "  its percentiles are bimodal rather than centered on a typical value.\n"
        "  PaintTile runs on the painting threads, so summing it across a frame\n"
        "  yields aggregate work rather than elapsed time."
    )

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


def _statistics_to_strings(statistics: Dict[str, Any]) -> Dict[str, Any]:
    def format_statistic(
        data: Dict[str, Any],
        key: str,
        default: str = "-",
        subkey: Optional[int] = None,
    ) -> str:
        if key not in data:
            return default
        number = data[key]
        if subkey is not None and subkey in number:
            number = data[key][subkey]
        if key == "n":
            return str(number)
        return "{:.4f}".format(number)

    return {
        key: format_statistic(statistics, key, "0" if key == "n" else "-")
        for key in ["n", "min", "max", "mean", "stddev", "median"]
    } | {
        "percentiles": {
            percentile: format_statistic(statistics, "percentiles", "-", percentile)
            for percentile in RELEVANT_PERCENTILES
        }
    }
