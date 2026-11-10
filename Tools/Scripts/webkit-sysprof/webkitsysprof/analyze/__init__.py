import argparse
import logging
import re
import collections
import json
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

from ..parser import parse
from . import explanations
from .explanations import CLASS_LABELS, PHASE_LABELS, STATE_LABELS
from ..compositions import (
    CLASSES,
    COMPOSITOR_CLASS,
    DISCARDED_MARK,
    GAP_IDLE_REFRESHES,
    IDLE,
    NO_REASON_CLASS,
    STALLED,
    STALLED_REFRESHES,
    STATES,
    MAYBE_SUSPENDED,
    SUSPENDED,
    UNKNOWN,
    UNRECORDED_CLASS,
    UPDATE_CLASS,
    CompositionModel,
    reason_order,
    calculate_compositions,
    clip_segments,
    in_window,
)
from ..updates import (
    ANOMALY,
    APPROXIMATE,
    BEHIND_PREVIOUS_FRAME,
    COMPOSITED,
    EXACT,
    OTHER,
    PENDING,
    TILE_WAIT,
    UNCOMPOSITED,
    UPDATE_PHASES,
    calculate_rendering_updates,
    is_old_capture,
)
from ..utils import (
    marks_by_process,
    check_timespan_holds_data,
    msec_to_sec,
    nsec_to_msec,
    nsec_to_sec,
    parse_timespan_argument,
    display_refreshes,
    intervals_between_marks,
    sample_statistics,
    sysprof_data_with_marks_by_name,
    trim_marks_by_name_to_timespan,
)

# (label, statistics key, percentile or None) per table column. The median column
# reads the exact "median" key, not the interpolated 50th percentile.
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

# Derived from the columns so computed and printed percentiles cannot drift apart.
# The 50th is kept for JSON report consumers.
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

# PaintTile geometry, e.g. "dirty region 768x512+256+56": <x>x<y>+<width>+<height>.
DIRTY_REGION_RE = re.compile(r"(\d+)x(\d+)\+(\d+)\+(\d+)")
TASKS_RE = re.compile(r"^tasks:\s*(\d+)")
SUBTREES_RE = re.compile(r"^subtrees:\s*(\d+)")
TILES_RE = re.compile(r"^dirty tiles:\s*(\d+)")


def _counted(message: str, pattern: "re.Pattern[str]") -> Optional[int]:
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
    if args.format == "json" and args.explain:
        # Json format explanations are capture-independent, so print right away.
        print(json.dumps(explanations.JSON_EXPLAINED_REPORT))
        return
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    capture_begin, capture_end = parsed_data["document"]["timespan"]
    check_timespan_holds_data(capture_begin, capture_end, timespan_begin, timespan_end)
    untrimmed_data = sysprof_data_with_marks_by_name(parsed_data)
    data = trim_marks_by_name_to_timespan(untrimmed_data, timespan_begin, timespan_end)
    logging.info("Preparing report...")
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
    # Built from the untrimmed capture: trimming drops marks crossing the window edge,
    # the replay needs every message, and an edge update may find its frame beyond it.
    model = calculate_compositions(untrimmed_sysprof_data)
    updates = calculate_rendering_updates(untrimmed_sysprof_data, model)
    return {
        "document": document,
        "statistics": mark_statistics,
        "rendering": _prepare_rendering_report(
            sysprof_data, display_refreshes(sysprof_data), model
        ),
        "compositions": _prepare_composition_report(
            model, timespan_begin, timespan_end
        ),
        "rendering_updates": _prepare_rendering_update_report(
            untrimmed_sysprof_data, model, updates, timespan_begin, timespan_end
        ),
    }


def _prepare_rendering_report(
    sysprof_data: Dict[str, Any],
    vblanks: Sequence[Dict[str, Any]],
    model: CompositionModel,
) -> Dict[str, Any]:
    return {
        "vblanks": len(vblanks),
        "vblank_interval_statistics": _calculate_statistics(
            intervals_between_marks(vblanks)
        ),
        "reference_interval": model.reference,
        "reference_interval_source": model.reference_source,
        "reference_interval_marks": model.reference_marks,
        "discarded_frames": len(sysprof_data["marks"].get(DISCARDED_MARK, [])),
    }


def _duration_sec(timespan_begin: int, timespan_end: int) -> Optional[float]:
    duration = nsec_to_sec(timespan_end - timespan_begin)
    return duration if duration > 0 else None


def _prepare_composition_report(
    model: CompositionModel, timespan_begin: int, timespan_end: int
) -> Dict[str, Any]:
    compositions = in_window(
        model.compositions, timespan_begin, timespan_end, "frame_nsec"
    )
    # Every process with a compositor timeline, so idle windows still report states.
    pids = sorted(
        {composition["pid"] for composition in model.compositions}
        | set(model.aborted_by_pid)
        | set(model.replay.segments_by_pid)
    )
    compositions_by_pid = marks_by_process(compositions)
    by_pid = len(pids) > 1
    groups: Dict[Tuple[Any, ...], List[Dict[str, Any]]] = {}
    for composition in compositions:
        key = (composition["pid"] if by_pid else None, composition["combination"])
        groups.setdefault(key, []).append(composition)
    rows = []
    for (pid, combination), group in groups.items():
        row = {
            "class": group[0]["class"],
            "reasons": group[0]["reasons"],
            "combination": combination,
            "n": len(group),
            # Of the row's process where rows are per process.
            "share": len(group)
            / (len(compositions_by_pid[pid]) if by_pid else len(compositions)),
        }
        for key in ("duration", "flush", "upload", "paint"):
            row[key + "_statistics"] = _calculate_statistics(
                [composition[key] for composition in group]
            )
        if by_pid:
            row["pid"] = pid
        rows.append(row)
    rows.sort(
        key=lambda row: (
            row.get("pid") or 0,
            CLASSES.index(row["class"]),
            -row["n"],
            row["combination"],
        )
    )
    by_reason: Dict[str, int] = collections.Counter(
        reason
        for composition in compositions
        for reason in composition["reasons"] or []
    )
    by_class = collections.Counter(composition["class"] for composition in compositions)
    return {
        "compositions": len(compositions),
        "capture_compositions": len(model.compositions),
        "processes": len(pids),
        "by_combination": rows,
        "by_reason": dict(by_reason),
        "by_class": {klass: by_class.get(klass, 0) for klass in CLASSES},
        # Keyed by str, as JSON does, so the report round-trips.
        "per_process": {
            str(pid): _composition_process_report(
                pid,
                compositions_by_pid.get(pid, []),
                model,
                timespan_begin,
                timespan_end,
            )
            for pid in pids
        },
    }


def _composition_process_report(
    pid: int,
    compositions: Sequence[Dict[str, Any]],
    model: CompositionModel,
    timespan_begin: int,
    timespan_end: int,
) -> Dict[str, Any]:
    active = [c["interval"] for c in compositions if c["active"]]
    interval_statistics = _calculate_statistics(active)
    median = interval_statistics.get("median")
    duration = _duration_sec(timespan_begin, timespan_end)
    long_intervals = [c for c in compositions if c["active"] and c["missed_refreshes"]]
    segments = clip_segments(
        model.replay.segments_by_pid.get(pid, []), timespan_begin, timespan_end
    )
    state_time = {state: 0.0 for state in STATES}
    state_longest = {state: 0.0 for state in STATES}
    unknown_by_detail: Dict[str, float] = {}
    for begin, end, state, detail in segments:
        length = nsec_to_msec(end - begin)
        state_time[state] += length
        state_longest[state] = max(state_longest[state], length)
        if state == UNKNOWN:
            key = detail or "unknown"
            unknown_by_detail[key] = unknown_by_detail.get(key, 0.0) + length
    total = sum(state_time.values())
    idle = [
        nsec_to_msec(end - begin) for begin, end, state, _ in segments if state == IDLE
    ]
    mismatches = model.replay.mismatches_by_pid.get(pid, {})
    return {
        "compositions": len(compositions),
        "aborted": len(
            [
                begin
                for begin in model.aborted_by_pid.get(pid, [])
                if timespan_begin <= begin < timespan_end
            ]
        ),
        "typical_frame_rate": 1 / msec_to_sec(median) if median else None,
        "frames_per_refresh": (
            model.reference / median if median and model.reference else None
        ),
        "average_frame_rate": len(compositions) / duration if duration else None,
        "active_interval_statistics": interval_statistics,
        "long_intervals": len(long_intervals),
        "missed_refreshes": sum(c["missed_refreshes"] for c in long_intervals),
        "state_time": state_time,
        "state_shares": {
            state: (time / total if total else None)
            for state, time in state_time.items()
        },
        "state_longest": state_longest,
        "unknown_by_detail": unknown_by_detail,
        "idle_stretches": {
            "n": len(idle),
            "total": sum(idle),
            "max": max(idle) if idle else None,
        },
        "replay_messages": model.replay.messages_by_pid.get(pid, 0),
        "replay_mismatches": sum(mismatches.values()),
        "replay_mismatches_by_kind": mismatches,
        "suspended_stretches": sum(
            1 for segment in segments if segment[3] == SUSPENDED
        ),
        # Scheduled for longer than a refresh, which a suspend leaving no mark explains.
        "probably_suspended_stretches": sum(
            1 for segment in segments if segment[3] == MAYBE_SUSPENDED
        ),
        "stalled": sum(1 for segment in segments if segment[3] == STALLED),
        "idle_fallback": model.replay.fallback_by_pid.get(pid, False),
    }


def _prepare_rendering_update_report(
    sysprof_data: Dict[str, Any],
    model: CompositionModel,
    updates: Sequence[Dict[str, Any]],
    timespan_begin: int,
    timespan_end: int,
) -> Dict[str, Any]:
    in_timespan = in_window(updates, timespan_begin, timespan_end, "begin_nsec")
    updates_by_pid = marks_by_process(in_timespan)
    pids = list(updates_by_pid)
    duration = _duration_sec(timespan_begin, timespan_end)
    per_process = {}
    for pid, process_updates in updates_by_pid.items():
        asked = [u for u in process_updates if u["asked"]]
        composited = [u for u in asked if u["outcome"] == COMPOSITED]
        statuses = collections.Counter(u["pairing"]["status"] for u in composited)
        reasons = collections.Counter(
            reason for u in composited for reason in u["pairing"]["reasons"]
        )
        begins = [u["begin_nsec"] for u in process_updates]
        per_process[str(pid)] = {
            "updates": len(process_updates),
            "asked": len(asked),
            "composited": len(composited),
            "uncomposited": sum(1 for u in asked if u["outcome"] == UNCOMPOSITED),
            "pending": sum(1 for u in asked if u["outcome"] == PENDING),
            "pairing": {
                status: statuses.get(status, 0)
                for status in (EXACT, APPROXIMATE, ANOMALY)
            }
            | {"reasons": dict(reasons)},
            "update_interval_statistics": _calculate_statistics(
                [nsec_to_msec(b - a) for a, b in zip(begins, begins[1:])]
            ),
            "update_rate": len(process_updates) / duration if duration else None,
            "duration_statistics": _calculate_statistics(
                [u["duration"] for u in process_updates]
            ),
            "refresh_intervals_statistics": _calculate_statistics(
                [
                    u["refresh_intervals"]
                    for u in process_updates
                    if u["refresh_intervals"] is not None
                ]
            ),
            "latency_statistics": _calculate_statistics(
                [
                    nsec_to_msec(
                        model.compositions[u["frame"]]["frame_nsec"] - u["begin_nsec"]
                    )
                    for u in composited
                ]
            ),
            "phase_statistics": {
                phase: _calculate_statistics([u["phases"][phase] for u in composited])
                for phase in UPDATE_PHASES
            },
            "phase_present": {
                phase: sum(1 for u in composited if u["phases"][phase] > 0)
                for phase in UPDATE_PHASES
            },
            "deferred_for_tiles": sum(u["deferred_for_tiles"] for u in composited),
            "deferred_for_tiles_statistics": _calculate_statistics(
                [u["deferred_for_tiles"] for u in composited]
            ),
        }
    return {
        "updates": len(in_timespan),
        "processes": len(pids),
        "old_capture": is_old_capture(sysprof_data, model),
        "per_process": per_process,
    }


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

    _render_rendering_numbers(report, explain)
    _render_composition_numbers(report, explain)
    _render_rendering_update_numbers(report, explain)

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
        print(explanations.TEXT_MARK_STATISTICS)

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
    return msec_to_sec(
        report["document"]["timespan"]["end"] - report["document"]["timespan"]["begin"]
    )


def _render_rendering_numbers(report: Dict[str, Any], explain: bool) -> None:
    rendering = report["rendering"]
    print()
    print("Rendering:")
    print(
        "- refresh interval: {}, discarded frames: {}".format(
            _reference_text(rendering), rendering["discarded_frames"]
        )
    )
    print(
        "- vblanks: {} DisplayLinkUpdate marks (the display link only runs while"
        " the main thread observes it)".format(rendering["vblanks"])
    )
    vblank_interval_statistics = _statistics_to_strings(
        rendering["vblank_interval_statistics"]
    )
    print(
        "- vblank intervals: (min: {}, median: {}, max: {})".format(
            vblank_interval_statistics["min"],
            vblank_interval_statistics["median"],
            vblank_interval_statistics["max"],
        )
    )
    if explain:
        print()
        print(_reference_explanation(rendering))


def _reference_text(rendering: Dict[str, Any]) -> str:
    interval = rendering["reference_interval"]
    if interval is None:
        return "unknown (no presentation feedback, display link or frame intervals)"
    source = rendering["reference_interval_source"]
    what = (
        "the median frame interval, no refresh was traced"
        if source == "frame intervals"
        else "from {} {} marks".format(rendering["reference_interval_marks"], source)
    )
    return "{:.3f} ms ({:.2f} Hz, {})".format(interval, 1 / msec_to_sec(interval), what)


def _reference_explanation(rendering: Dict[str, Any]) -> str:
    return explanations.TEXT_REFERENCE_INTERVAL.format(
        source=rendering["reference_interval_source"] or "nothing"
    )


def _process_prefix(pid: Any, processes: int) -> str:
    return "[{}] ".format(pid) if processes > 1 else ""


def _percent(share: Optional[float]) -> str:
    return "-" if share is None else "{:.1f}%".format(share * 100)


def _spread(statistics: Dict[str, Any], number: str = "{:.4f}") -> str:
    strings = _statistics_to_strings(statistics, number)
    return "(min: {}, {}, max: {})".format(
        strings["min"], ", ".join(_percentile_strings(strings)), strings["max"]
    )


def _frame_rate_text(process: Dict[str, Any], reference: Optional[float]) -> str:
    rate = process["typical_frame_rate"]
    if rate is None:
        return "-"
    median = process["active_interval_statistics"]["median"]
    text = "{:.1f} fps".format(rate)
    per_refresh = process["frames_per_refresh"]
    if per_refresh:
        refreshes = 1 / per_refresh
        if abs(refreshes - round(refreshes)) < 0.05 and round(refreshes) >= 1:
            whole = round(refreshes)
            text += (
                ", one frame per refresh"
                if whole == 1
                else (", one frame per {} refreshes".format(whole))
            )
        else:
            text += ", one frame per {:.2f} refreshes".format(refreshes)
    text += " (median active frame interval {:.3f} ms".format(median)
    if reference:
        text += ", refresh {:.3f} ms".format(reference)
    return text + ")"


def _compositor_time_parts(process: Dict[str, Any]) -> List[str]:
    """One "state: share (details)" per compositor state, longest first."""
    shares = process["state_shares"]
    parts = []
    for state in sorted(
        (state for state in STATES if process["state_time"][state] > 0),
        key=lambda state: -process["state_time"][state],
    ):
        part = "{}: {}".format(STATE_LABELS[state], _percent(shares[state]))
        if state == IDLE:
            stretches = process["idle_stretches"]
            part += " ({} stretch{}, longest {:.1f} ms)".format(
                stretches["n"],
                "" if stretches["n"] == 1 else "es",
                stretches["max"],
            )
        elif state == UNKNOWN:
            part += " ({})".format(
                ", ".join(
                    "{} {:.1f} ms".format(detail, time)
                    for detail, time in sorted(
                        process["unknown_by_detail"].items(), key=lambda item: -item[1]
                    )
                )
            )
        parts.append(part)
    return parts


def _render_composition_numbers(report: Dict[str, Any], explain: bool) -> None:
    compositions = report["compositions"]
    reference = report["rendering"]["reference_interval"]
    processes = compositions["processes"]
    print()
    print("Compositions:")
    print("- compositions: {}".format(compositions["compositions"]))
    if not compositions["per_process"]:
        print("- no compositions here: the capture holds no RenderLayerTree marks")
        if explain:
            print()
            print(explanations.TEXT_COMPOSITIONS)
        return
    if not compositions["capture_compositions"]:
        print("- no RenderLayerTree of the capture holds a DidRenderFrame")
    for pid, process in compositions["per_process"].items():
        prefix = "- " + _process_prefix(pid, processes)
        if processes > 1:
            print("{}compositions: {}".format(prefix, process["compositions"]))
        if process["aborted"]:
            print("{}aborted: {}".format(prefix, process["aborted"]))
        average = process["average_frame_rate"]
        print(
            "{}average frame rate: {} (over the analyzed duration)".format(
                prefix, "-" if average is None else "{:.2f} fps".format(average)
            )
        )
        print(
            "{}typical frame rate: {}".format(
                prefix, _frame_rate_text(process, reference)
            )
        )
        print(
            "{}active frame interval: {} [ms]".format(
                prefix, _spread(process["active_interval_statistics"])
            )
        )
        active = process["active_interval_statistics"].get("n", 0)
        if reference:
            print(
                "{}long frame intervals: {} of {} active, {} missed refresh{}".format(
                    prefix,
                    process["long_intervals"],
                    active,
                    process["missed_refreshes"],
                    "" if process["missed_refreshes"] == 1 else "es",
                )
            )
        parts = _compositor_time_parts(process)
        print("{}compositor time:{}".format(prefix, "" if parts else " -"))
        for part in parts:
            print("  - {}".format(part))
        if process["idle_fallback"]:
            print(
                "{}compositor states: no ScheduleComposition or FrameComplete state"
                " messages, so a gap of more than {} refreshes between compositions"
                " counts as idle".format(prefix, GAP_IDLE_REFRESHES)
            )
        if process["replay_mismatches"]:
            print(
                # The replay runs over the whole capture, whatever the window.
                "{}replay mismatches: {} of {} messages in the whole capture".format(
                    prefix, process["replay_mismatches"], process["replay_messages"]
                )
            )
        if process["suspended_stretches"] or process["probably_suspended_stretches"]:
            print(
                "{}suspended stretches: {}, probably suspended: {}".format(
                    prefix,
                    process["suspended_stretches"],
                    process["probably_suspended_stretches"],
                )
            )
        if process["stalled"]:
            print(
                "{}stalled: {} (awaiting FrameComplete for more than {} refreshes)".format(
                    prefix, process["stalled"], STALLED_REFRESHES
                )
            )

    if compositions["compositions"]:
        print("- by reasons:")
        _render_combination_table(compositions)
        by_class = compositions["by_class"]
        print(
            "  update-driven: {}, compositor-driven: {}".format(
                by_class[UPDATE_CLASS], by_class[COMPOSITOR_CLASS]
            )
        )
        if by_class[NO_REASON_CLASS]:
            print("- no reason (unexpected): {}".format(by_class[NO_REASON_CLASS]))
        if by_class[UNRECORDED_CLASS]:
            print("- unrecorded: {}".format(by_class[UNRECORDED_CLASS]))
        by_reason = compositions["by_reason"]
        if by_reason:
            print(
                "- by reason (a composition with several reasons counts toward"
                " each): {}".format(
                    ", ".join(
                        "{} {}".format(reason, by_reason[reason])
                        for reason in sorted(by_reason, key=reason_order)
                    )
                )
            )
    if explain:
        print()
        print(explanations.TEXT_COMPOSITIONS)
        print()
        print(explanations.TEXT_COMPOSITOR_STATES)


def _render_combination_table(compositions: Dict[str, Any]) -> None:
    by_pid = compositions["processes"] > 1
    header = (["pid"] if by_pid else []) + [
        "class",
        "reasons",
        "#",
        "share",
        "duration median",
        "P99",
        "flush median",
        "upload median",
        "paint median",
    ]
    rows = [header]
    for row in compositions["by_combination"]:
        duration = _statistics_to_strings(row["duration_statistics"], "{:.3f}")
        rows.append(
            ([str(row["pid"])] if by_pid else [])
            + [
                CLASS_LABELS[row["class"]],
                row["combination"],
                str(row["n"]),
                "{:.2f}%".format(row["share"] * 100),
                duration["median"],
                duration["percentiles"][99],
            ]
            + [
                _statistics_to_strings(row[key + "_statistics"], "{:.3f}")["median"]
                for key in ("flush", "upload", "paint")
            ]
        )
    _render_statistics_rows_to_stdout(
        rows, indent="  ", left_columns=3 if by_pid else 2
    )


def _render_rendering_update_numbers(report: Dict[str, Any], explain: bool) -> None:
    updates = report["rendering_updates"]
    processes = updates["processes"]
    print()
    print("Rendering updates:")
    if not updates["per_process"]:
        print("- updates: 0")
        if explain:
            print()
            print(explanations.TEXT_RENDERING_UPDATES)
        return
    if updates["old_capture"]:
        print(
            "- pairing approximate (old capture): no RequestCompositionForRenderingUpdate"
            " marks, so every update counts as asking when it ends"
        )
    for pid, process in updates["per_process"].items():
        prefix = "- " + _process_prefix(pid, processes)
        print(
            "{}updates: {}, asked for a composition: {}, composited: {}".format(
                prefix, process["updates"], process["asked"], process["composited"]
            )
        )
        if process["uncomposited"]:
            print("{}uncomposited: {}".format(prefix, process["uncomposited"]))
        if process["pending"]:
            print("{}pending at capture end: {}".format(prefix, process["pending"]))
        pairing = process["pairing"]
        if pairing[APPROXIMATE] or pairing[ANOMALY]:
            print(
                "{}pairing: {} approximate, {} anomalies ({})".format(
                    prefix,
                    pairing[APPROXIMATE],
                    pairing[ANOMALY],
                    ", ".join(
                        "{} {}".format(reason.replace("_", " "), n)
                        for reason, n in sorted(pairing["reasons"].items())
                    ),
                )
            )
        rate = process["update_rate"]
        print(
            "{}update interval: {} [ms], update rate {} (how often the page produced"
            " new content, not a frame rate)".format(
                prefix,
                _spread(process["update_interval_statistics"]),
                "-" if rate is None else "{:.2f} /s".format(rate),
            )
        )
        print(
            "{}update duration in refresh intervals: {}".format(
                prefix, _spread(process["refresh_intervals_statistics"], "{:.4g}")
            )
        )
        if not process["composited"]:
            continue
        print(
            "{}update-to-frame latency of the {} composited updates: {} [ms]".format(
                prefix, process["composited"], _spread(process["latency_statistics"])
            )
        )
        phases = process["phase_statistics"]
        present = process["phase_present"]
        for phase in UPDATE_PHASES:
            if phase == OTHER and not present[OTHER]:
                continue
            line = "  - {}: {:.3f} ms median".format(
                PHASE_LABELS[phase], phases[phase]["median"]
            )
            if phase in (TILE_WAIT, BEHIND_PREVIOUS_FRAME, OTHER):
                line += ", in {} of {} updates".format(
                    present[phase], process["composited"]
                )
            if phase == TILE_WAIT:
                line += ", compositions deferred for tiles: {}".format(
                    process["deferred_for_tiles"]
                )
            print(line)
    if explain:
        print()
        print(explanations.TEXT_RENDERING_UPDATES)


def _percentile_strings(strings: Dict[str, Any]) -> List[str]:
    """Percentile strings in order, with the median in place of P50.

    Takes the output of _statistics_to_strings().
    """
    return [
        (
            "median: {}".format(strings["median"])
            if percentile == PERCENTILE_SHOWN_AS_MEDIAN
            else "P{}: {}".format(percentile, strings["percentiles"][percentile])
        )
        for percentile in RELEVANT_PERCENTILES
    ]


def _render_statistics_rows_to_stdout(
    statistics_rows: List[List[str]], indent: str = "", left_columns: int = 0
) -> None:
    """Print a table whose first row is the header and first columns left-aligned."""
    column_max_sizes = [
        max([len(row[col]) for row in statistics_rows])
        for col in range(len(statistics_rows[0]))
    ]
    format_str = " | ".join(
        "{{:{}{}}}".format("<" if col < left_columns else ">", cms)
        for col, cms in enumerate(column_max_sizes)
    )
    for i, statistics_row in enumerate(statistics_rows):
        print(indent + "|", format_str.format(*statistics_row), "|")
        if i == 0:
            print(indent + "|", format_str.format(*"-" * len(statistics_row)), "|")


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
    """Format a statistic's numbers as strings, `-` where missing."""

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
