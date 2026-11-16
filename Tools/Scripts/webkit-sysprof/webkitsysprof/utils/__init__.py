import bisect
import collections
import itertools
import logging
import re
import statistics
from typing import (
    Any,
    Callable,
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


# Plain milliseconds only. \Z because $ lets a trailing newline through.
MILLISECONDS_RE = re.compile(r"^\d+\Z")


class UsageError(ValueError):
    """A request the tool cannot serve, e.g. a backwards timespan.

    Separate from other ValueErrors so the CLI reports it as a usage error while a
    broken capture still raises where it broke.
    """


def mark_begin(mark: Dict[str, Any]) -> int:
    """Marks carry only an end time and a duration."""
    return int(mark["end_time"] - mark["duration"])


def parse_timespan_argument(arg: str) -> Tuple[Optional[int], Optional[int]]:
    """Parse "<begin>-<end>" in milliseconds, either side of which may be empty.

    A single bound without a dash is the begin, e.g. "500" is "from 500 ms on".
    """

    def parse_bound(value: str) -> Optional[int]:
        if value == "":
            return None
        # int() would also take " 5", "+5" and "1_000".
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
        # No duration to divide rates by. Rejected rather than reported as zeroes
        # that look measured.
        if timespan_begin == timespan_end:
            raise UsageError(f"timespan is of no length: {arg}")

    return (
        msec_to_nsec(timespan_begin) if timespan_begin is not None else None,
        msec_to_nsec(timespan_end) if timespan_end is not None else None,
    )


def sysprof_data_with_marks_by_name(sysprof_data: Dict[str, Any]) -> Dict[str, Any]:
    """Group the marks by name, in parse order. The input is left untouched."""
    document = dict(sysprof_data["document"])
    document["timespan"] = {
        "begin": sysprof_data["document"]["timespan"][0],
        "end": sysprof_data["document"]["timespan"][1],
    }
    marks_by_name: Dict[str, List[Dict[str, Any]]] = collections.defaultdict(list)
    for mark in sysprof_data["marks"]:
        marks_by_name[mark["name"]].append(mark)
    # A plain dict, so subscripting a name the capture lacks is a KeyError.
    return {"document": document, "marks": dict(marks_by_name)}


def check_timespan_holds_data(
    capture_begin: int,
    capture_end: int,
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> None:
    """Reject a timespan the caller asked for that the capture cannot answer."""
    if capture_begin >= capture_end:
        # Not the window's fault, so warn to explain the empty report.
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


def clamped_timespan(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Tuple[int, int]:
    """The --timespan window clamped to the capture's timespan.

    So a window past the capture does not stretch the rate duration.
    """
    begin = sysprof_data["document"]["timespan"]["begin"]
    end = sysprof_data["document"]["timespan"]["end"]
    if timespan_begin is not None:
        begin = max(begin, timespan_begin)
    if timespan_end is not None:
        end = min(end, timespan_end)
    return begin, end


def trim_marks_by_name_to_timespan(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Dict[str, Any]:
    """Narrow data from sysprof_data_with_marks_by_name() to a timespan.

    Marks crossing a boundary are dropped. The input is left untouched, since
    compositions and rendering updates need the untrimmed data.
    """
    # Narrowed even without bounds, as the capture holds marks that begin before
    # its own timespan.
    begin, end = clamped_timespan(sysprof_data, timespan_begin, timespan_end)
    timespan = {"begin": begin, "end": end}

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
    """The [begin, end) spans merged where they overlap or touch, empties dropped."""
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
        # Both sort internally, so presorting would not save anything.
        "median": statistics.median(values),
        "percentiles": percentiles(values, wanted),
    }


def percentiles(
    values: Sequence[Union[int, float]],
    wanted: Sequence[int],
    method: Literal["inclusive", "exclusive"] = "inclusive",
) -> Dict[str, float]:
    """Interpolated percentiles, empty below two samples (quantiles() raises
    there before Python 3.13). Keyed by str so the report round-trips JSON.

    Inclusive stays within the sample range, exclusive extrapolates past it and
    can report values below the minimum or above the maximum on few samples.
    """
    for percentile in wanted:
        if not 1 <= percentile <= 99:
            raise ValueError(f"no percentile between 1 and 99: {percentile}")
    if not wanted:
        return {}
    if len(values) < 2:
        return {}
    quantiles: List[float] = statistics.quantiles(values, n=100, method=method)
    return {str(percentile): quantiles[percentile - 1] for percentile in wanted}


def mark_pid(mark: Dict[str, Any]) -> int:
    return int(mark.get("pid", 0))


def marks_by_process(
    marks: Iterable[Dict[str, Any]],
) -> Dict[int, List[Dict[str, Any]]]:
    """The marks, or anything with a pid, grouped by process in process order."""
    by_pid: Dict[int, List[Dict[str, Any]]] = collections.defaultdict(list)
    for mark in marks:
        by_pid[mark_pid(mark)].append(mark)
    return dict(sorted(by_pid.items()))


class MarkIndex:
    """Marks of one or several names ordered by begin time, for window lookups.

    `begin` and `end` read the times of other items, such as compositions.
    """

    def __init__(
        self,
        marks: Sequence[Any],
        begin: Callable[[Any], int] = mark_begin,
        end: Callable[[Any], int] = lambda mark: mark["end_time"],
    ) -> None:
        self._marks = sorted(marks, key=begin)
        self._end = end
        self._begins = [begin(mark) for mark in self._marks]
        # Latest end among the first i marks. Marks still running at a time form
        # one span from it to their latest end.
        self._ends_until: List[int] = list(
            itertools.accumulate((end(mark) for mark in self._marks), max)
        )

    def spans_within(self, begin: int, end: int) -> List[Tuple[int, int]]:
        """Unmerged [begin, end) spans where a mark runs, in begin order.

        Marks begun before the window and still running form one leading span.
        """
        first = bisect.bisect_left(self._begins, begin)
        spans = []
        still_running = self._ends_until[first - 1] if first else None
        if still_running is not None and still_running > begin:
            spans.append((begin, min(still_running, end)))
        for index in range(first, len(self._marks)):
            if self._begins[index] >= end:
                break
            spans.append((self._begins[index], min(self._end(self._marks[index]), end)))
        return spans

    def marks_overlapping(self, begin: int, end: int) -> List[Any]:
        """The marks running in [begin, end), in begin order.

        Walks back only while an earlier mark can still reach the window.
        """
        overlapping = []
        for index in reversed(range(bisect.bisect_left(self._begins, end))):
            if self._ends_until[index] <= begin:
                break
            if self._end(self._marks[index]) > begin:
                overlapping.append(self._marks[index])
        overlapping.reverse()
        return overlapping


def mark_index_by_process(marks: Sequence[Dict[str, Any]]) -> Dict[int, MarkIndex]:
    """A MarkIndex of the marks of each process."""
    return {pid: MarkIndex(group) for pid, group in marks_by_process(marks).items()}


def marks_in_time_order(
    sysprof_data: Dict[str, Any], mark_name: str
) -> List[Dict[str, Any]]:
    """The marks of that name, ordered by end time."""
    return sorted(
        sysprof_data["marks"].get(mark_name, []), key=lambda mark: mark["end_time"]
    )


def intervals_between_marks(marks: Sequence[Dict[str, Any]]) -> List[float]:
    """Milliseconds between the ends of consecutive marks, given in time order."""
    return [
        nsec_to_msec(marks[i]["end_time"] - marks[i - 1]["end_time"])
        for i in range(1, len(marks))
    ]


def display_refreshes(sysprof_data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """All DisplayLinkUpdate marks in time order.

    The links all live in the UI process. Two displays at once merge
    indistinguishably, and a stopped link leaves one very long interval.
    """
    return marks_in_time_order(sysprof_data, "DisplayLinkUpdate")


def median_vblank_interval(refreshes: Sequence[Dict[str, Any]]) -> Optional[float]:
    """Median ms between display_refreshes(), None if unknown."""
    intervals = intervals_between_marks(refreshes)
    return statistics.median(intervals) if intervals else None
