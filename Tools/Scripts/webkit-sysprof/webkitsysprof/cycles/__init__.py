"""Frame cycle reconstruction for the `analyze` command.

What a cycle is, what its phases mean and how to read the numbers taken from them
is what `analyze --explain` prints, from webkitsysprof.analyze.explanations, which
is where that is written down rather than here.
"""

import bisect
import itertools
from typing import Any, Dict, List, Optional, Sequence, Tuple

from ..utils import mark_begin, marks_by_process, merged_spans, nsec_to_msec

# Compositing of a cycle is the time any of these marks was running within it,
# whether the mark began there or in an earlier cycle it overran. All four are read,
# and as time rather than by name, because which of them a cycle carries varies:
# renderLayerTree() opens its trace scope before the checks that can return early, so
# a cycle can hold a RenderLayerTree mark with no flush or paint inside it. Their
# spans are merged, so listing a mark that RenderLayerTree already encloses adds no
# time of its own.
COMPOSITING_MARKS = [
    "RenderLayerTree",
    "FlushCompositingState",
    "PaintToGLContext",
    "WaitForCompositionCompletion",
]
PHASES = [
    "rendering_update",
    "waiting_for_compositing",
    "compositing",
    "idle",
]
RENDERING_UPDATE, WAITING, COMPOSITING, IDLE = PHASES


class _MarkIndex:
    """Marks of a single name, ordered by begin time for lookup within a cycle."""

    def __init__(self, marks: Sequence[Dict[str, Any]]) -> None:
        self._marks = sorted(marks, key=mark_begin)
        self._begins = [mark_begin(mark) for mark in self._marks]
        # The latest end among the first i marks. Every mark still running at a
        # given time covers that time onwards, so their union is one span reaching
        # to the latest of their ends, and that is all this has to answer.
        self._ends_until: List[int] = list(
            itertools.accumulate((mark["end_time"] for mark in self._marks), max)
        )

    def spans_within(self, begin: int, end: int) -> List[Tuple[int, int]]:
        """The [begin, end) spans of time a mark of this name was running.

        Unmerged and in begin order. What began before the window and had not ended
        by then counts, since a composition of an earlier cycle is what a cycle
        waiting on the compositor is running, and all of those together reach from
        the start of the window to the latest of their ends.
        """
        first = bisect.bisect_left(self._begins, begin)
        spans = []
        still_running = self._ends_until[first - 1] if first else None
        if still_running is not None and still_running > begin:
            spans.append((begin, min(still_running, end)))
        for index in range(first, len(self._marks)):
            if self._begins[index] >= end:
                break
            spans.append(
                (self._begins[index], min(self._marks[index]["end_time"], end))
            )
        return spans

    def last_end_of_marks_beginning_within(self, begin: int, end: int) -> Optional[int]:
        """End of the last mark beginning within [begin, end), None if there is none.

        The last end, not the first: marks nest, so the first one to end is not the
        one compositing finished with. Indexing by end time instead has the mirror
        problem, of returning a short mark of the next cycle when an enclosing mark
        overruns this one.
        """
        first = bisect.bisect_left(self._begins, begin)
        last = bisect.bisect_left(self._begins, end)
        if first >= last:
            return None
        return max(self._marks[index]["end_time"] for index in range(first, last))


def _marks_beginning_within(
    marks: Sequence[Dict[str, Any]],
    begin: Optional[int],
    end: Optional[int],
    including_end: bool = False,
) -> List[Dict[str, Any]]:
    """The marks that begin within the window, which are the only ones read."""

    def within(mark: Dict[str, Any]) -> bool:
        if begin is not None and mark_begin(mark) < begin:
            return False
        if end is None:
            return True
        return mark_begin(mark) <= end if including_end else mark_begin(mark) < end

    return [mark for mark in marks if within(mark)]


def _compositing_indices_by_process(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int],
    timespan_end: Optional[int],
) -> Dict[int, List[_MarkIndex]]:
    """The compositing marks of the capture, grouped by process and by name."""
    by_pid: Dict[int, Dict[str, List[Dict[str, Any]]]] = {}
    for mark_name in COMPOSITING_MARKS:
        marks = [
            mark
            for mark in _marks_beginning_within(
                sysprof_data["marks"].get(mark_name, []), None, timespan_end
            )
            if timespan_begin is None or mark["end_time"] >= timespan_begin
        ]
        for pid, process_marks in marks_by_process(marks).items():
            by_pid.setdefault(pid, {})[mark_name] = process_marks
    return {
        pid: [_MarkIndex(marks.get(mark_name, [])) for mark_name in COMPOSITING_MARKS]
        for pid, marks in by_pid.items()
    }


def _compositing_spans(
    indices: Sequence[_MarkIndex], begin: int, end: int
) -> List[Tuple[int, int]]:
    """The spans of [begin, end) that were spent compositing, merged and in order.

    Merged rather than taken as one span from the first mark to the last, because a
    cycle can composite twice: the tail of a composition it inherited and then one
    of its own, with the wait for the painting threads in between. Reading that as
    one span would report the wait as compositing.
    """
    return merged_spans(
        span for index in indices for span in index.spans_within(begin, end)
    )


def _own_compositing_end(
    indices: Sequence[_MarkIndex], cycle_begin: int, cycle_end: int
) -> Optional[int]:
    """When the composition of this cycle was over, None where it has none.

    Only the marks that began within the cycle count, so this is the cycle's own
    composition rather than one it inherited. It may end after the cycle does: when
    compositing overruns, the next rendering update is dispatched from within the
    composition wait. It is usually the RenderLayerTree mark compositing began with,
    since that encloses the others and runs on through the wait for the GPU to
    finish the frame, which costs frame time as well.
    """
    ends = [
        index.last_end_of_marks_beginning_within(cycle_begin, cycle_end)
        for index in indices
    ]
    ended = [end for end in ends if end is not None]
    return max(ended) if ended else None


def calculate_frame_cycles(
    sysprof_data: Dict[str, Any],
    timespan_begin: Optional[int] = None,
    timespan_end: Optional[int] = None,
) -> List[Dict[str, Any]]:
    """Split the timeline into frame cycles and break each cycle into its phases.

    Each cycle carries its boundaries in nanoseconds, its duration and phases in
    milliseconds, and the process it belongs to. The phases sum to the cycle
    duration exactly, so compositing running past the cycle is reported beside them
    as `composition_overrun` rather than as a fifth phase. A cycle nothing
    composited in has both set to None, so callers can count it without letting it
    skew the per-phase statistics.

    A cycle runs between two rendering updates of one process, which the marks name.
    `timespan_begin`/`timespan_end` keep the cycles
    lying entirely within that window, in nanoseconds, the way trimming keeps the
    marks lying entirely within it. Filtering here rather than trimming the marks
    first keeps the compositing marks of a cycle at the window edge, which trimming
    drops for reaching past it. `analyze --explain` reads all of this out.
    """
    # A cycle ends where the next update begins, and the whole cycle has to fit in
    # the window, so an update beginning on the very edge still bounds one.
    all_updates = _marks_beginning_within(
        sysprof_data["marks"].get("LayerTreeHostRenderingUpdate", []),
        timespan_begin,
        timespan_end,
        including_end=True,
    )
    compositing = _compositing_indices_by_process(
        sysprof_data, timespan_begin, timespan_end
    )
    cycles: List[Dict[str, Any]] = []
    for pid, updates in marks_by_process(all_updates).items():
        cycles += _cycles_of_one_process(
            pid, sorted(updates, key=mark_begin), compositing.get(pid, [])
        )
    return sorted(cycles, key=lambda cycle: cycle["begin_nsec"])


def _cycles_of_one_process(
    pid: int, updates: Sequence[Dict[str, Any]], indices: Sequence[_MarkIndex]
) -> List[Dict[str, Any]]:
    """The cycles between the given rendering updates, all of one process.

    `indices` holds that same process's compositing marks, one index per name.
    """
    cycles: List[Dict[str, Any]] = []
    for update, next_update in zip(updates, itertools.islice(updates, 1, None)):
        cycle_begin = mark_begin(update)
        cycle_end = mark_begin(next_update)
        cycle: Dict[str, Any] = {
            "pid": pid,
            "begin_nsec": cycle_begin,
            "end_nsec": cycle_end,
            "duration": nsec_to_msec(cycle_end - cycle_begin),
            "phases": None,
            "composition_overrun": None,
        }
        cycles.append(cycle)

        # LayerTreeHost::updateRendering() asserts it is not re-entered, so an
        # update cannot outlast its own cycle. The phases cover the cycle, so one
        # that does anyway is cut off rather than trusted.
        update_end = min(update["end_time"], cycle_end)
        # What was traced as compositing within the cycle, whether it began there
        # or in an earlier one. A composition running entirely beside the rendering
        # update still says the cycle composited, so it is what decides that, while
        # the phases below cover the time after the update.
        within = _compositing_spans(indices, cycle_begin, cycle_end)
        if not within:
            # Nothing about compositing can be said of this cycle.
            continue
        spans = [
            (max(span_begin, update_end), span_end)
            for span_begin, span_end in within
            if span_end > update_end
        ]
        own_end = _own_compositing_end(indices, cycle_begin, cycle_end)

        compositing = sum(span_end - span_begin for span_begin, span_end in spans)
        # Idle runs from the last thing traced until the next update begins, and
        # what is neither the update, compositing nor idle was spent waiting.
        idle = cycle_end - (spans[-1][1] if spans else update_end)
        cycle["phases"] = {
            RENDERING_UPDATE: nsec_to_msec(update_end - cycle_begin),
            WAITING: nsec_to_msec(cycle_end - update_end - compositing - idle),
            COMPOSITING: nsec_to_msec(compositing),
            IDLE: nsec_to_msec(idle),
        }
        # How far the compositing this cycle began ran past its end. A composition
        # of an earlier cycle running through this one is that cycle's, not this
        # one's, so what began here is what counts.
        cycle["composition_overrun"] = (
            nsec_to_msec(max(own_end - cycle_end, 0)) if own_end is not None else 0.0
        )

    return cycles
