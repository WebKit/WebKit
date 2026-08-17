from typing import Any, Dict, Optional, Tuple, Union

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


def parse_timespan_argument(arg: str) -> Tuple[Optional[int], Optional[int]]:
    timespan_begin, timespan_end = None, None
    values = arg.split("-")

    if len(values) > 0:
        try:
            timespan_begin = int(values[0])
        except ValueError:
            pass
    if len(values) > 1:
        try:
            timespan_end = int(values[1])
        except ValueError:
            pass

    return (
        msec_to_nsec(timespan_begin) if timespan_begin is not None else None,
        msec_to_nsec(timespan_end) if timespan_end is not None else None,
    )


def trim_sysprof_data_to_timespan(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Dict[str, Any]:
    trimmed_document = sysprof_data["document"]

    if timespan_begin is not None:
        trimmed_document["timespan"][0] = timespan_begin
    if timespan_end is not None:
        trimmed_document["timespan"][1] = timespan_end

    def mark_in_timespan(mark: Dict[str, Any]) -> bool:
        return (
            (mark["end_time"] - mark["duration"]) >= trimmed_document["timespan"][0]
        ) and mark["end_time"] <= trimmed_document["timespan"][1]

    return {
        "document": trimmed_document,
        "marks": [mark for mark in sysprof_data["marks"] if mark_in_timespan(mark)],
    }
