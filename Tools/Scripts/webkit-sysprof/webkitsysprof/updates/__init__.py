"""Rendering updates and the composition each reached the screen with.

A LayerTreeHostRenderingUpdate holding a RequestCompositionForRenderingUpdate is
paired with the first update-class composition after its request, and the time
between is split into phases from the compositor state timeline.
"""

import bisect
from typing import Any, Dict, List, Optional, Sequence, Tuple

from ..compositions import (
    AWAITING_FRAME_COMPLETE,
    COMPOSITING,
    RENDERING_UPDATE_MARK,
    SCHEDULED,
    UPDATE_CLASS,
    CompositionModel,
    Segment,
    indices_by_pid,
    segments_overlapping,
)
from ..utils import MarkIndex, mark_begin, marks_by_process, nsec_to_msec

REQUEST_MARK = "RequestCompositionForRenderingUpdate"

UPDATE_PHASES = [
    "update",
    "tile_wait",
    "behind_previous_frame",
    "scheduled",
    "other",
    "composition",
]
UPDATE, TILE_WAIT, BEHIND_PREVIOUS_FRAME, SCHEDULED_PHASE, OTHER, COMPOSITION = (
    UPDATE_PHASES
)

COMPOSITED = "composited"
UNCOMPOSITED = "uncomposited"
PENDING = "pending"

EXACT = "exact"
APPROXIMATE = "approximate"
ANOMALY = "anomaly"

RLT_BEFORE_REQUEST = "rlt_before_request"
OLD_CAPTURE = "old_capture"
INVARIANT_BREACH = "invariant_breach"
UNEXPECTED_COMPOSITION = "unexpected_composition"
PAIRING_REASONS = [
    RLT_BEFORE_REQUEST,
    OLD_CAPTURE,
    INVARIANT_BREACH,
    UNEXPECTED_COMPOSITION,
]

_BEHIND_STATES = {COMPOSITING, AWAITING_FRAME_COMPLETE}


def calculate_rendering_updates(
    data: Dict[str, Any], model: CompositionModel
) -> List[Dict[str, Any]]:
    """Every rendering update of untrimmed data, paired with its composition.

    Paired over the whole capture so an update at a window edge still finds its
    frame. Callers filter on `begin_nsec`.
    """
    marks = data["marks"]
    old_capture = is_old_capture(data, model)
    requests_by_pid = {
        pid: sorted(mark_begin(mark) for mark in group)
        for pid, group in marks_by_process(marks.get(REQUEST_MARK, [])).items()
    }
    compositions_by_pid = indices_by_pid(model.compositions)

    updates: List[Dict[str, Any]] = []
    for pid, group in marks_by_process(marks.get(RENDERING_UPDATE_MARK, [])).items():
        updates += _updates_of_one_process(
            pid,
            sorted(group, key=mark_begin),
            requests_by_pid.get(pid, []),
            compositions_by_pid.get(pid, []),
            model,
            old_capture,
        )
    return sorted(updates, key=lambda update: update["begin_nsec"])


def is_old_capture(data: Dict[str, Any], model: CompositionModel) -> bool:
    """Whether the capture predates the Request marks.

    They landed with the composition reasons (558de671b9bb), and the state
    messages after them (d7da21e4b2df), so a capture holding either is not old
    even if no update asked.
    """
    if data["marks"].get(REQUEST_MARK) or any(model.replay.messages_by_pid.values()):
        return False
    return not any(c["reasons"] is not None for c in model.compositions)


def _request(
    update: Dict[str, Any],
    requests: Sequence[int],
    request_scs: Sequence[Tuple[int, str]],
    request_sc_times: Sequence[int],
) -> Tuple[bool, Optional[int], Optional[str]]:
    """Whether the update asked, when its request SC was, and what it reported.

    The request SC is where RenderingUpdate joined the reasons, per the replay.
    The nearest SC before the Request mark may be a compositor-thread Animation
    request that also carries RenderingUpdate. Without a replay the Request mark
    stands in.
    """
    begin, end = mark_begin(update), update["end_time"]
    i = bisect.bisect_left(requests, begin)
    if i >= len(requests) or requests[i] > end:
        return False, None, None
    request_mark = requests[i]
    j = bisect.bisect_right(request_sc_times, request_mark) - 1
    if j >= 0 and request_sc_times[j] >= begin:
        return True, request_sc_times[j], request_scs[j][1]
    return True, request_mark, None


def _updates_of_one_process(
    pid: int,
    update_marks: Sequence[Dict[str, Any]],
    requests: Sequence[int],
    composition_indices: Sequence[int],
    model: CompositionModel,
    old_capture: bool,
) -> List[Dict[str, Any]]:
    replay = model.replay
    request_scs = replay.request_scs_by_pid.get(pid, [])
    request_sc_times = [t for t, _ in request_scs]
    approximate: List[str] = []
    if old_capture:
        approximate.append(OLD_CAPTURE)

    updates: List[Dict[str, Any]] = []
    for mark in update_marks:
        begin, end = mark_begin(mark), mark["end_time"]
        if old_capture:
            asked, request, request_state = True, end, None
        else:
            asked, request, request_state = _request(
                mark, requests, request_scs, request_sc_times
            )
        updates.append(
            {
                "pid": pid,
                "begin_nsec": begin,
                "end_nsec": end,
                "duration": nsec_to_msec(end - begin),
                "refresh_intervals": (
                    nsec_to_msec(end - begin) / model.reference
                    if model.reference
                    else None
                ),
                "asked": asked,
                "request_nsec": request,
                "request_state": request_state,
                "frame": None,
                "outcome": None,
                "phases": None,
                "deferred_for_tiles": None,
                "pairing": None,
                # The frame's RLT began first and took the update under the lock.
                "rlt_before_request": False,
            }
        )

    compositions = model.compositions
    # Old captures name no reasons, so any composition can be the frame.
    eligible = [
        index
        for index in composition_indices
        if old_capture or compositions[index]["class"] == UPDATE_CLASS
    ]
    eligible_frames = [compositions[index]["frame_nsec"] for index in eligible]
    begins = [compositions[index]["begin_nsec"] for index in composition_indices]
    segments = replay.segments_by_pid.get(pid, [])
    segment_begins = [segment[0] for segment in segments]
    tile_waits = MarkIndex(
        replay.tile_waits_by_pid.get(pid, []),
        begin=lambda span: span[0],
        end=lambda span: span[1],
    )
    claimed = set()

    # With the position of each, for the update after it.
    asking = [(i, update) for i, update in enumerate(updates) if update["asked"]]
    for position, (i, update) in enumerate(asking):
        request = update["request_nsec"]
        next_request = (
            asking[position + 1][1]["request_nsec"]
            if position + 1 < len(asking)
            else None
        )
        frame = None
        for k in range(bisect.bisect_right(eligible_frames, request), len(eligible)):
            if eligible[k] in claimed:
                continue
            candidate = compositions[eligible[k]]
            # An RLT begun after the next request is the next update's frame.
            if next_request is None or candidate["begin_nsec"] < next_request:
                frame = eligible[k]
            break
        if frame is None:
            # LayerTreeHost runs no update before DidComposite (LTH:154-157), so
            # any update after it shows the capture went on.
            update["outcome"] = UNCOMPOSITED if i + 1 < len(updates) else PENDING
            continue
        claimed.add(frame)
        update["frame"] = frame
        update["outcome"] = COMPOSITED
        _attribute(
            update,
            compositions[frame],
            updates[i + 1]["begin_nsec"] if i + 1 < len(updates) else None,
            begins,
            segments,
            segment_begins,
            tile_waits,
            approximate,
        )
    return updates


def _attribute(
    update: Dict[str, Any],
    composition: Dict[str, Any],
    next_begin: Optional[int],
    begins: Sequence[int],
    segments: Sequence[Segment],
    segment_begins: Sequence[int],
    tile_waits: MarkIndex,
    approximate: Sequence[str],
) -> None:
    """Split the update-to-frame latency into its phases, and check the pairing."""
    begin, request = update["begin_nsec"], update["request_nsec"]
    rlt_begin, frame = composition["begin_nsec"], composition["frame_nsec"]
    reasons = list(approximate)
    anomaly = False

    if rlt_begin < request:
        # The RLT began, then blocked on the lock the request held (TC:557, 566).
        # Only an RLT the timer started does, which the request SC reports as
        # Scheduled.
        update["rlt_before_request"] = True
        if update["request_state"] not in (None, "Scheduled"):
            reasons.append(RLT_BEFORE_REQUEST)
            anomaly = True

    # LayerTreeHost defers the next update until DidComposite (LTH:154-157), so
    # the frame's RLT must begin before the next update.
    if next_begin is not None and rlt_begin >= next_begin:
        reasons.append(INVARIANT_BREACH)
        anomaly = True

    phases = {phase: 0.0 for phase in UPDATE_PHASES}
    phases[UPDATE] = nsec_to_msec(max(0, request - begin))
    phases[COMPOSITION] = nsec_to_msec(frame - max(request, rlt_begin))
    deferred = 0
    if rlt_begin > request:
        for a, b, state, _ in _segments_within(
            segments, segment_begins, request, rlt_begin
        ):
            waiting = _covered(tile_waits, a, b)
            phases[TILE_WAIT] += nsec_to_msec(waiting)
            rest = nsec_to_msec(b - a - waiting)
            if state in _BEHIND_STATES:
                phases[BEHIND_PREVIOUS_FRAME] += rest
            elif state == SCHEDULED:
                phases[SCHEDULED_PHASE] += rest
            else:
                phases[OTHER] += rest
        # Compositions begun in between, the frame excluded. In a tile wait
        # RenderingUpdate was held back from them (TC:577-580), elsewhere they
        # should have taken it.
        first = bisect.bisect_left(begins, request)
        for k in range(first, bisect.bisect_left(begins, rlt_begin)):
            if tile_waits.marks_overlapping(begins[k], begins[k] + 1):
                deferred += 1
            elif UNEXPECTED_COMPOSITION not in reasons:
                reasons.append(UNEXPECTED_COMPOSITION)
                anomaly = True

    update["phases"] = phases
    update["deferred_for_tiles"] = deferred
    status = ANOMALY if anomaly else APPROXIMATE if approximate else EXACT
    update["pairing"] = {"status": status, "reasons": reasons}


def _segments_within(
    segments: Sequence[Segment], begins: Sequence[int], begin: int, end: int
) -> List[Segment]:
    """The segments clipped to [begin, end), with what no segment covers as other."""
    result: List[Segment] = []
    cursor = begin
    for segment in segments_overlapping(segments, begins, begin, end):
        if segment[0] > cursor:
            result.append((cursor, segment[0], "", None))
        result.append(segment)
        cursor = segment[1]
    if cursor < end:
        result.append((cursor, end, "", None))
    return result


def _covered(tile_waits: MarkIndex, begin: int, end: int) -> int:
    """Nanoseconds of [begin, end) the disjoint tile waits cover."""
    return sum(b - a for a, b in tile_waits.spans_within(begin, end))
