import collections
import logging
import re
import statistics
from typing import (
    Any,
    Dict,
    Iterable,
    List,
    Literal,
    Optional,
    Sequence,
    Tuple,
    Union,
)

NSEC_PER_MSEC = 1_000_000
NSEC_PER_SEC = 1_000_000_000
MSEC_PER_SEC = 1_000


def nsec_to_msec(nsec: Union[int, float]) -> float:
    return nsec / NSEC_PER_MSEC


def nsec_to_sec(nsec: Union[int, float]) -> float:
    return nsec / NSEC_PER_SEC


def msec_to_sec(msec: Union[int, float]) -> float:
    return msec / MSEC_PER_SEC


def msec_to_nsec(msec: int) -> int:
    return msec * NSEC_PER_MSEC


# A timespan bound is a plain number of milliseconds, nothing int() would also take.
# Ended with \Z rather than $, which would let a trailing newline through.
MILLISECONDS_RE = re.compile(r"^\d+\Z")


class UsageError(ValueError):
    """The user asked for something the tool cannot do, e.g. a backwards timespan.

    Told apart from the other ValueErrors so that the command line can report it as
    a usage error while a broken capture still raises where it broke.
    """


def mark_begin(mark: Dict[str, Any]) -> int:
    """Marks carry an end time and a duration, so a begin time has to be derived."""
    return int(mark["end_time"] - mark["duration"])


def parse_timespan_argument(arg: str) -> Tuple[Optional[int], Optional[int]]:
    """Parse "<begin>-<end>" in milliseconds, either side of which may be empty.

    A single bound without a dash is the begin, e.g. "500" is "from 500 ms on".
    """

    def parse_bound(value: str) -> Optional[int]:
        if value == "":
            return None
        # Digits and nothing else: int() would take " 5", "+5" and "1_000" too, and
        # a timespan the tool reads as something other than what was typed is what
        # rejecting a malformed one is meant to prevent.
        if not MILLISECONDS_RE.match(value):
            raise UsageError(f"timespan bound is no number of milliseconds: {value}")
        return int(value)

    if arg == "":
        raise UsageError("timespan is empty")
    values = arg.split("-")
    if len(values) > 2:
        raise UsageError(f'timespan is no "<begin>-<end>" in milliseconds: {arg}')
    timespan_begin = parse_bound(values[0])
    timespan_end = parse_bound(values[1]) if len(values) == 2 else None

    if timespan_begin is not None and timespan_end is not None:
        if timespan_begin > timespan_end:
            raise UsageError(f"timespan begins after it ends: {arg}")
        # A window of no length encloses no data and is no duration to divide by,
        # so every rate taken over it is unknown. Rejected rather than reported as
        # a row of zeroes, which reads as a measurement that was made.
        if timespan_begin == timespan_end:
            raise UsageError(f"timespan is of no length: {arg}")

    return (
        msec_to_nsec(timespan_begin) if timespan_begin is not None else None,
        msec_to_nsec(timespan_end) if timespan_end is not None else None,
    )


def sysprof_data_with_marks_by_name(sysprof_data: Dict[str, Any]) -> Dict[str, Any]:
    """Reshape parsed data so that marks are grouped by name.

    The groups keep the order the marks were parsed in, so the callers that walk
    them in time order sort them the way they need. The input is left untouched.
    """
    document = dict(sysprof_data["document"])
    document["timespan"] = {
        "begin": sysprof_data["document"]["timespan"][0],
        "end": sysprof_data["document"]["timespan"][1],
    }
    marks_by_name: Dict[str, List[Dict[str, Any]]] = collections.defaultdict(list)
    for mark in sysprof_data["marks"]:
        marks_by_name[mark["name"]].append(mark)
    # Handed out as a plain dict, so that a name the capture never held is a
    # KeyError for the callers that subscript it rather than an empty list.
    return {"document": document, "marks": dict(marks_by_name)}


def check_timespan_holds_data(
    capture_begin: int,
    capture_end: int,
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> None:
    """Reject a timespan the caller asked for that the capture cannot answer."""
    if capture_begin >= capture_end:
        # The capture holds no range for a window to miss, so there is nothing the
        # caller could have asked for that would be right, and blaming the window
        # for what the capture lacks would point at the wrong thing. Say so, so
        # that the empty report below carries its reason with it.
        logging.warning(
            "The capture spans no time (begins at %d, ends at %d), so the report "
            "has no rate to give.",
            capture_begin,
            capture_end,
        )
        return
    if timespan_begin is not None and timespan_begin >= capture_end:
        raise UsageError(
            "timespan begins at or after the capture ends, so it holds no data"
        )
    if timespan_end is not None and timespan_end <= capture_begin:
        raise UsageError(
            "timespan ends at or before the capture begins, so it holds no data"
        )


def trim_marks_by_name_to_timespan(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Dict[str, Any]:
    """Narrow data grouped by sysprof_data_with_marks_by_name() to a timespan.

    Marks that cross a boundary are dropped, since only part of them happened
    within the window. The input is left untouched, so its callers keep the
    untrimmed data, which the frame cycles need.
    """
    # Narrowed even where no bound was given, since the capture holds marks that
    # begin before its own timespan does, and the frame cycles are cut to that
    # timespan either way.
    timespan = dict(sysprof_data["document"]["timespan"])
    # Clamped to the capture, so that a window reaching past it does not stretch
    # the analyzed duration that every rate is divided by.
    if timespan_begin is not None:
        timespan["begin"] = max(timespan_begin, timespan["begin"])
    if timespan_end is not None:
        timespan["end"] = min(timespan_end, timespan["end"])

    def mark_in_timespan(mark: Dict[str, Any]) -> bool:
        return (
            mark_begin(mark) >= timespan["begin"]
            and mark["end_time"] <= timespan["end"]
        )

    trimmed_document = dict(sysprof_data["document"])
    trimmed_document["timespan"] = timespan
    return {
        "document": trimmed_document,
        "marks": {
            mark_name: [mark for mark in marks if mark_in_timespan(mark)]
            for mark_name, marks in sysprof_data["marks"].items()
        },
    }


def merged_spans(spans: Iterable[Tuple[int, int]]) -> List[Tuple[int, int]]:
    """The [begin, end) spans merged where they overlap or touch, in order.

    Empty spans are dropped, so that a mark clipped to nothing does not become a
    span of no length between two real ones.
    """
    merged: List[Tuple[int, int]] = []
    for begin, end in sorted(spans):
        if end <= begin:
            continue
        if merged and begin <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((begin, end))
    return merged


def sample_statistics(
    values: Sequence[Union[int, float]], wanted: Sequence[int] = ()
) -> Dict[str, Any]:
    if not values:
        return {}
    return {
        "n": len(values),
        "min": min(values),
        "max": max(values),
        "mean": statistics.mean(values),
        "stddev": statistics.stdev(values) if len(values) > 1 else 0,
        # Both of these sort what they are given, so handing them a sorted sequence
        # would add a sort rather than save the two they do.
        "median": statistics.median(values),
        "percentiles": percentiles(values, wanted),
    }


def percentiles(
    values: Sequence[Union[int, float]],
    wanted: Sequence[int],
    method: Literal["inclusive", "exclusive"] = "inclusive",
) -> Dict[str, float]:
    """Percentiles interpolated between the data points, empty below two of them.

    Keyed by the percentile as a string, the shape json.dumps() turns an int key
    into anyway, so that a report read back from JSON is the report written.

    The inclusive method stays between the smallest and the largest sample, while
    the exclusive one extrapolates past them: on few samples it reports a P25 below
    the minimum, a P99 above the maximum and negative durations. Reporting the
    samples wants the former, estimating the spread of what they were drawn from
    wants the latter. Both need two data points, and below that quantiles() raises
    before Python 3.13.
    """
    for percentile in wanted:
        if not 1 <= percentile <= 99:
            raise ValueError(f"no percentile between 1 and 99: {percentile}")
    # Before the sort quantiles() does, which nothing would read.
    if not wanted:
        return {}
    if len(values) < 2:
        return {}
    quantiles: List[float] = statistics.quantiles(values, n=100, method=method)
    return {str(percentile): quantiles[percentile - 1] for percentile in wanted}


def mark_pid(mark: Dict[str, Any]) -> int:
    """The process a mark was emitted by, as the capture records it."""
    return int(mark.get("pid", 0))


def marks_by_process(
    marks: Sequence[Dict[str, Any]],
) -> Dict[int, List[Dict[str, Any]]]:
    """The marks grouped by the process they came from, in process order."""
    by_pid: Dict[int, List[Dict[str, Any]]] = collections.defaultdict(list)
    for mark in marks:
        by_pid[mark_pid(mark)].append(mark)
    return dict(sorted(by_pid.items()))


def marks_in_time_order(
    sysprof_data: Dict[str, Any], mark_name: str
) -> List[Dict[str, Any]]:
    """The marks of that name, ordered by when they ended.

    The grouping keeps the marks in parse order, so every walk that depends on
    time order sorts them here.
    """
    return sorted(
        sysprof_data["marks"].get(mark_name, []), key=lambda mark: mark["end_time"]
    )


def intervals_between_marks(marks: Sequence[Dict[str, Any]]) -> List[float]:
    """Times between the ends of consecutive marks, in milliseconds.

    Takes the marks in time order, as marks_in_time_order() returns them.
    """
    return [
        nsec_to_msec(marks[i]["end_time"] - marks[i - 1]["end_time"])
        for i in range(1, len(marks))
    ]


def display_refreshes(sysprof_data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """The DisplayLinkUpdate marks of the capture, in time order.

    All of them: the display links all live in the UI process, one per display, so
    the marks of a capture are the refreshes of one process either way. Two displays
    driven at once are merged, which no mark tells apart, and so is a link that
    stopped and started again: the gap it leaves counts as one very long interval
    between two refreshes.
    """
    return marks_in_time_order(sysprof_data, "DisplayLinkUpdate")


def median_vblank_interval(refreshes: Sequence[Dict[str, Any]]) -> Optional[float]:
    """Median time between display refreshes in milliseconds, None if unknown.

    Takes the refreshes as display_refreshes() returns them.
    """
    intervals = intervals_between_marks(refreshes)
    return statistics.median(intervals) if intervals else None
