"""Frame cycle reconstruction for the `analyze` command.

What a cycle is, what its phases mean and how to read the numbers taken from them
is what `analyze --explain` prints, from webkitsysprof.analyze.explanations, which
is where that is written down rather than here.
"""

import bisect
import itertools
from typing import Any, Dict, List, Optional, Sequence, Tuple

from ..utils import (
    MarkIndex,
    frame_rendering_reason,
    mark_begin,
    marks_by_process,
    merged_spans,
    nsec_to_msec,
)

# Compositing of a cycle is the time any of these marks was running within it, up to
# the end of the composition that presented its frame, whether the mark began there
# or in an earlier cycle it overran. All four are read,
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
# The mark a frame cycle begins with, and so also the one a capture must hold two
# of before there is any cycle to speak of.
RENDERING_UPDATE_MARK = "LayerTreeHostRenderingUpdate"
# A composition is one RenderLayerTree mark, and it presented a frame when it emitted
# a DidRenderFrame mark, whose message names the reasons it composited for. The one a
# rendering update asked for names RenderingUpdate among them.
COMPOSITION_MARK = "RenderLayerTree"
FRAME_MARK = "DidRenderFrame"
RENDERING_UPDATE_REASON = "RenderingUpdate"

# A composition that presented a frame: its begin and end, and whether a rendering
# update asked for it.
FrameComposition = Tuple[int, int, bool]

PHASES = [
    "rendering_update",
    "waiting_for_compositing",
    "compositing",
    "idle",
]
RENDERING_UPDATE, WAITING, COMPOSITING, IDLE = PHASES


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
) -> Dict[int, List[MarkIndex]]:
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
        pid: [MarkIndex(marks.get(mark_name, [])) for mark_name in COMPOSITING_MARKS]
        for pid, marks in by_pid.items()
    }


def _frame_compositions_by_process(
    sysprof_data: Dict[str, Any], timespan_end: Optional[int]
) -> Dict[int, List[FrameComposition]]:
    """The compositions of the capture that presented a frame, by process, in order.

    A process whose compositions emitted no DidRenderFrame mark is left out, since
    which of them presented the frame of a cycle cannot be told there.
    """
    frames_by_pid = marks_by_process(sysprof_data["marks"].get(FRAME_MARK, []))
    compositions = _marks_beginning_within(
        sysprof_data["marks"].get(COMPOSITION_MARK, []), None, timespan_end
    )
    by_pid: Dict[int, List[FrameComposition]] = {}
    for pid, marks in marks_by_process(compositions).items():
        frames = sorted(frames_by_pid.get(pid, []), key=lambda frame: frame["end_time"])
        frame_times = [frame["end_time"] for frame in frames]
        found: List[FrameComposition] = []
        for composition in sorted(marks, key=mark_begin):
            begin, end = mark_begin(composition), composition["end_time"]
            first = bisect.bisect_left(frame_times, begin)
            last = bisect.bisect_right(frame_times, end)
            if first < last:
                asked_for = any(
                    RENDERING_UPDATE_REASON
                    in frame_rendering_reason(frames[index]["message"]).split(", ")
                    for index in range(first, last)
                )
                found.append((begin, end, asked_for))
        if found:
            by_pid[pid] = found
    return by_pid


def _frame_composition(
    compositions: Sequence[FrameComposition],
    begins: Sequence[int],
    cycle_begin: int,
    update_end: int,
    cycle_end: int,
) -> Optional[FrameComposition]:
    """The composition that presented the frame of a cycle, None where none did.

    That is the composition its rendering update asked for. The update asks from
    inside, so the composition can begin before the update mark ends, but never
    before the cycle does: the next rendering update waits for it. An update that
    had nothing to commit asks for none, and the first composition to begin after
    it is then the one showing the page as that update left it. Any other
    composition is the compositor's own, for a compositor-side animation, a video
    frame or asynchronous scrolling, and is not part of the frame.
    """
    first_after_update = None
    for index in range(bisect.bisect_left(begins, cycle_begin), len(compositions)):
        composition = compositions[index]
        begin, _, asked_for = composition
        if begin >= cycle_end:
            break
        if asked_for:
            return composition
        if first_after_update is None and begin >= update_end:
            first_after_update = composition
    return first_after_update


def _compositing_spans(
    indices: Sequence[MarkIndex], begin: int, end: int
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
    indices: Sequence[MarkIndex], cycle_begin: int, cycle_end: int
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
    as `composition_overrun` rather than as a fifth phase. So are the compositions
    that began after the one presenting the frame of the cycle, which are part of
    its idle time: `compositions_after_frame` counts them and
    `compositing_after_frame` is their time within the cycle, both None where no
    composition can be told to have presented the frame. A cycle nothing
    composited in has all of these set to None, so callers can count it without
    letting it skew the per-phase statistics.

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
        sysprof_data["marks"].get(RENDERING_UPDATE_MARK, []),
        timespan_begin,
        timespan_end,
        including_end=True,
    )
    compositing = _compositing_indices_by_process(
        sysprof_data, timespan_begin, timespan_end
    )
    frame_compositions = _frame_compositions_by_process(sysprof_data, timespan_end)
    cycles: List[Dict[str, Any]] = []
    for pid, updates in marks_by_process(all_updates).items():
        cycles += _cycles_of_one_process(
            pid,
            sorted(updates, key=mark_begin),
            compositing.get(pid, []),
            frame_compositions.get(pid, []),
        )
    return sorted(cycles, key=lambda cycle: cycle["begin_nsec"])


def _cycles_of_one_process(
    pid: int,
    updates: Sequence[Dict[str, Any]],
    indices: Sequence[MarkIndex],
    frame_compositions: Sequence[FrameComposition],
) -> List[Dict[str, Any]]:
    """The cycles between the given rendering updates, all of one process.

    `indices` holds that same process's compositing marks, one index per name, and
    `frame_compositions` its compositions that presented a frame, in order.
    """
    frame_composition_begins = [begin for begin, _, _ in frame_compositions]
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
            "compositions_after_frame": None,
            "compositing_after_frame": None,
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
        # Compositing is over once the frame of the cycle is: what the compositor
        # does after that, until the next rendering update, is not part of this
        # frame. Where no composition can be told to have presented it, all of the
        # compositing within the cycle counts.
        frame = _frame_composition(
            frame_compositions,
            frame_composition_begins,
            cycle_begin,
            update_end,
            cycle_end,
        )
        compositing_end = min(frame[1], cycle_end) if frame else cycle_end
        spans = [
            (max(span_begin, update_end), min(span_end, compositing_end))
            for span_begin, span_end in within
            if span_end > update_end and span_begin < compositing_end
        ]
        own_end = (
            frame[1] if frame else _own_compositing_end(indices, cycle_begin, cycle_end)
        )

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
        if frame:
            # Reported beside the phases rather than as one, like the overrun: within
            # the cycle they are part of its idle time. What ran beside the rendering
            # update is part of that phase instead, and is left out.
            first = bisect.bisect_left(frame_composition_begins, frame[1])
            last = bisect.bisect_left(frame_composition_begins, cycle_end)
            after = [
                (max(begin, update_end), min(end, cycle_end))
                for begin, end, _ in frame_compositions[first:last]
                if end > update_end
            ]
            cycle["compositions_after_frame"] = len(after)
            cycle["compositing_after_frame"] = nsec_to_msec(
                sum(end - begin for begin, end in after)
            )

    return cycles
