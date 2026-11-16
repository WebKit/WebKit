"""Compositions and the compositor state timeline.

A composition is a RenderLayerTree mark holding a DidRenderFrame, which names its
reasons. The timeline replays ThreadedCompositor's state machine from its marks,
checked against the state each ScheduleComposition and FrameComplete reports.
"""

import bisect
import collections
import re
import statistics
from typing import Any, Dict, Iterable, List, NamedTuple, Optional, Sequence, Set, Tuple

from ..utils import (
    MarkIndex,
    display_refreshes,
    mark_index_by_process,
    mark_begin,
    mark_pid,
    marks_by_process,
    median_vblank_interval,
    merged_spans,
    msec_to_nsec,
    nsec_to_msec,
)

COMPOSITION_MARK = "RenderLayerTree"
RENDERING_UPDATE_MARK = "LayerTreeHostRenderingUpdate"
SCHEDULE_RENDERING_UPDATE_MARK = "LayerTreeHostScheduleRenderingUpdate"
FRAME_MARK = "DidRenderFrame"
FLUSH_MARK = "FlushCompositingState"
PAINT_MARK = "PaintToGLContext"
UPLOAD_MARK = "SkiaBackingStoreTileUpdate"
SCHEDULE_MARK = "ScheduleComposition"
FRAME_COMPLETE_MARK = "FrameComplete"
PRESENTED_MARK = "WaylandFramePresented"
DISCARDED_MARK = "WaylandFrameDiscarded"

# CoordinatedCompositionReason.h, in the order the enum declares them.
CANONICAL_REASONS = ("RenderingUpdate", "VideoFrame", "Animation", "AsyncScrolling")
RENDERING_UPDATE_REASON = "RenderingUpdate"

# The class of a composition, from the reasons its DidRenderFrame names.
UPDATE_CLASS = "update"
COMPOSITOR_CLASS = "compositor"
NO_REASON_CLASS = "no_reason"
UNRECORDED_CLASS = "unrecorded"
CLASSES = [UPDATE_CLASS, COMPOSITOR_CLASS, NO_REASON_CLASS, UNRECORDED_CLASS]

# The segments of the compositor state timeline.
COMPOSITING = "compositing"
AWAITING_FRAME_COMPLETE = "awaiting_frame_complete"
SCHEDULED = "scheduled"
BLOCKED_ON_TILES = "blocked_on_tiles"
IDLE = "idle"
UNKNOWN = "unknown"
GAP = "gap"
STATES = [
    COMPOSITING,
    AWAITING_FRAME_COMPLETE,
    SCHEDULED,
    BLOCKED_ON_TILES,
    IDLE,
    UNKNOWN,
    GAP,
]
# A frame interval holding unknown time does not measure frame rate. Idle time
# does not either, unless the main thread was producing the next update then.
# Blocked on tiles always held new content back, so it is jank.
INACTIVE_STATES = {IDLE, UNKNOWN}

# The details an unknown segment carries.
BEFORE_FIRST_MESSAGE = "before first message"
MISMATCH = "mismatch"
SUSPENDED = "suspended"
MAYBE_SUSPENDED = "suspended?"
ABORTED = "aborted"
STALLED = "stalled"
INVALIDATED = "invalidated"

# endMark appends " | ..." to a message, which is not a reason.
REASONS_RE = re.compile(r"^reasons:([^|\n]*)")
# ScheduleComposition (TC:686) and FrameComplete (TC:712) messages. Not for
# parse_reasons(), which would take "state: ..." for a reason.
STATE_MESSAGE_RE = re.compile(
    r"^reasons: ([^,]*(?:, [A-Z]\w+)*), state: (\w+), waiting for tiles: (\w+)"
    r"(?:, render timer active: (\w+))?"
)
REFRESH_INTERVAL_RE = re.compile(r"refreshInterval=([\d.]+) ms")

# A scheduled segment ends when a zero-delay timer fires, so one longer than a
# refresh means a suspend() that left no mark.
MAYBE_SUSPENDED_REFRESHES = 1
# A display round trip takes about one refresh. Ten without a FrameComplete means
# the frame was never sent or the compositor wedged.
STALLED_REFRESHES = 10
# Without state messages, a longer gap between compositions is taken for idle.
GAP_IDLE_REFRESHES = 3
# Idle shorter than a refresh before a rendering update is the wait for the
# display link to start it, so not a lull.
DISPLAY_LINK_WAIT_REFRESHES = 1

IN_PROGRESS_STATES = ("InProgress", "ScheduledWhileInProgress")


def parse_reasons(message: str) -> Optional[Tuple[str, ...]]:
    """The reasons a DidRenderFrame names, in canonical order.

    None if it names none (WebKit before reasons were traced), an
    empty tuple if the list is empty.
    """
    match = REASONS_RE.match(message.strip())
    if match is None:
        return None
    return tuple(sorted(_reason_set(match.group(1)), key=reason_order))


def _reason_set(listed: str) -> Set[str]:
    """The reasons of a comma-separated list."""
    return {token.strip() for token in listed.split(",")} - {""}


def reason_order(reason: str) -> Tuple[int, str]:
    if reason in CANONICAL_REASONS:
        return (CANONICAL_REASONS.index(reason), "")
    return (len(CANONICAL_REASONS), reason)


def classify(reasons: Optional[Sequence[str]]) -> str:
    if reasons is None:
        return UNRECORDED_CLASS
    if not reasons:
        return NO_REASON_CLASS
    if RENDERING_UPDATE_REASON in reasons:
        return UPDATE_CLASS
    return COMPOSITOR_CLASS


def combination_label(reasons: Optional[Sequence[str]]) -> str:
    if reasons is None:
        return "unrecorded"
    if not reasons:
        return "no reason"
    return " + ".join(reasons)


class StateMessage(NamedTuple):
    reasons: Set[str]
    state: str
    waiting: bool
    timer: Optional[bool]


def parse_state_message(message: str) -> Optional[StateMessage]:
    match = STATE_MESSAGE_RE.match(message.strip())
    if match is None:
        return None
    reasons = _reason_set(match.group(1))
    timer = None if match.group(4) is None else match.group(4) == "yes"
    return StateMessage(reasons, match.group(2), match.group(3) == "yes", timer)


def _spans_within(index: MarkIndex, begin: int, end: int) -> float:
    """Milliseconds of [begin, end) the indexed marks ran in, overlaps merged."""
    return nsec_to_msec(
        sum(b - a for a, b in merged_spans(index.spans_within(begin, end)))
    )


def match_compositions(
    data: Dict[str, Any],
) -> Tuple[List[Dict[str, Any]], Dict[int, List[int]]]:
    """The compositions in frame order, and per pid the begin times of aborted
    RLTs (no DidRenderFrame inside). One ThreadedCompositor per process, so
    RLTs do not nest.
    """
    marks = data["marks"]
    frames_by_pid = marks_by_process(marks.get(FRAME_MARK, []))
    rlts_by_pid = marks_by_process(marks.get(COMPOSITION_MARK, []))
    flush = mark_index_by_process(marks.get(FLUSH_MARK, []))
    paint = mark_index_by_process(marks.get(PAINT_MARK, []))
    upload = mark_index_by_process(marks.get(UPLOAD_MARK, []))
    empty = MarkIndex([])

    compositions: List[Dict[str, Any]] = []
    aborted: Dict[int, List[int]] = {}
    for pid in sorted(set(frames_by_pid) | set(rlts_by_pid)):
        rlts = rlts_by_pid.get(pid, [])
        index = MarkIndex(rlts)
        claimed: Set[int] = set()
        for frame_mark in sorted(frames_by_pid.get(pid, []), key=mark_begin):
            frame = mark_begin(frame_mark)
            rlt = next(
                (
                    rlt
                    for rlt in index.marks_overlapping(frame - 1, frame + 1)
                    if mark_begin(rlt) <= frame <= rlt["end_time"]
                    and id(rlt) not in claimed
                ),
                None,
            )
            if rlt is None:
                continue
            claimed.add(id(rlt))
            begin, end = mark_begin(rlt), rlt["end_time"]
            reasons = parse_reasons(frame_mark["message"])
            compositions.append(
                {
                    "pid": pid,
                    "begin_nsec": begin,
                    "frame_nsec": frame,
                    "end_nsec": end,
                    "duration": nsec_to_msec(end - begin),
                    "flush": _spans_within(flush.get(pid, empty), begin, end),
                    "paint": _spans_within(paint.get(pid, empty), begin, end),
                    "upload": _spans_within(upload.get(pid, empty), begin, end),
                    "reasons": list(reasons) if reasons is not None else None,
                    "class": classify(reasons),
                    "combination": combination_label(reasons),
                    # Which RLT mark, for replay(). Spans can coincide.
                    "rlt_id": id(rlt),
                }
            )
        unmatched = sorted(mark_begin(rlt) for rlt in rlts if id(rlt) not in claimed)
        if unmatched:
            aborted[pid] = unmatched
    compositions.sort(key=lambda composition: composition["frame_nsec"])
    return compositions, aborted


def reference_interval(
    data: Dict[str, Any], compositions: Sequence[Dict[str, Any]]
) -> Tuple[Optional[float], Optional[str], int]:
    """The refresh interval in ms, its source, and how many marks gave it.

    From untrimmed data. Prefers WaylandFramePresented, then DisplayLinkUpdate
    (runs only while the main thread observes it), then the frame intervals.
    """
    presented = [
        float(match.group(1))
        for match in (
            REFRESH_INTERVAL_RE.search(mark["message"])
            for mark in data["marks"].get(PRESENTED_MARK, [])
        )
        if match is not None and float(match.group(1)) > 0
    ]
    if presented:
        return statistics.median(presented), PRESENTED_MARK, len(presented)
    refreshes = display_refreshes(data)
    vblank = median_vblank_interval(refreshes)
    if vblank:
        return vblank, "DisplayLinkUpdate", len(refreshes)
    intervals = [
        nsec_to_msec(b["frame_nsec"] - a["frame_nsec"])
        for group in marks_by_process(compositions).values()
        for a, b in zip(group, group[1:])
    ]
    intervals = [interval for interval in intervals if interval > 0]
    if intervals:
        return statistics.median(intervals), "frame intervals", len(intervals)
    return None, None, 0


Segment = Tuple[int, int, str, Optional[str]]


class CompositorReplay:
    """ThreadedCompositor's state machine, replayed from the marks of one process.

    Mirrors renderLayerTree() (TC:566-586), scheduleUpdateLocked() (TC:686-704),
    frameComplete() (TC:712-729) and the suspend()/resume() guards (TC:244, 257).
    Each message reports the state before its transition, which the replay checks
    and then adopts, so it resyncs at every message and counts mismatches.
    """

    def __init__(self, begin: int, reference_nsec: Optional[float]) -> None:
        self.state: Optional[str] = None
        self.timer: Optional[bool] = None
        self.waiting = False
        self.reasons: Set[str] = set()
        # "pending": an RLT with a frame began and its locked transition is still
        # to come. "tentative": an aborted RLT began and the next message tells
        # whether it returned before or after the transition.
        self.pending: Optional[str] = None
        self.in_rlt = False
        # The RLT inside, so a frame of an earlier RLT cannot apply this one.
        self.rlt_id: Optional[int] = None
        self.suspended = False
        self.after_abort = False
        # Why the timer is unknown after a no-op FrameComplete reported Scheduled.
        self.desynced: Optional[str] = None
        self.label: Tuple[str, Optional[str]] = (UNKNOWN, BEFORE_FIRST_MESSAGE)
        self.since = begin
        self.segments: List[Segment] = []
        self.message_index = 0
        self.reference_nsec = reference_nsec
        self.messages = 0
        self.mismatches: collections.Counter = collections.Counter()
        self.request_scs: List[Tuple[int, str]] = []
        self.waiting_since: Optional[int] = None
        self.tile_waits: List[Tuple[int, int]] = []

    def _enter(self, label: Tuple[str, Optional[str]], t: int) -> None:
        if t > self.since:
            self.segments.append((self.since, t) + self.label)
        if t >= self.since:
            self.label, self.since = label, t

    def _relabel_since_message(self, label: Tuple[str, Optional[str]]) -> None:
        for i in range(self.message_index, len(self.segments)):
            if self.segments[i][2] != COMPOSITING:
                self.segments[i] = self.segments[i][:2] + label
        if self.label[0] != COMPOSITING:
            self.label = label

    def _between_label(self) -> Tuple[str, Optional[str]]:
        if self.desynced:
            return (UNKNOWN, self.desynced)
        if self.suspended:
            return (UNKNOWN, SUSPENDED)
        if self.after_abort:
            return (UNKNOWN, ABORTED)
        if self.state is None:
            return (UNKNOWN, BEFORE_FIRST_MESSAGE)
        if self.state == "Invalidated":
            return (UNKNOWN, INVALIDATED)
        if self.state == "Idle":
            return (IDLE, None)
        if self.state == "Scheduled":
            if self.timer:
                return (SCHEDULED, None)
            if self.waiting:
                return (BLOCKED_ON_TILES, None)
            return (UNKNOWN, SUSPENDED)
        if self.state in IN_PROGRESS_STATES:
            return (AWAITING_FRAME_COMPLETE, None)
        return (UNKNOWN, MISMATCH)

    def _mismatch(self, what: str) -> None:
        self.mismatches[what] += 1
        if not self.in_rlt:
            self.label = (UNKNOWN, MISMATCH)

    def _set_waiting(self, waiting: bool, t: int) -> None:
        if waiting and self.waiting_since is None:
            self.waiting_since = t
        elif not waiting and self.waiting_since is not None:
            self.tile_waits.append((self.waiting_since, t))
            self.waiting_since = None
        self.waiting = waiting

    # The transition under the lock, TC:566-586.

    def _take_reasons(self) -> None:
        self.state, self.timer = "InProgress", False
        # TC:576-581: a rendering update waiting for tiles stays pending.
        held = RENDERING_UPDATE_REASON in self.reasons and self.waiting
        self.reasons = {RENDERING_UPDATE_REASON} if held else set()

    def _apply_rlt_transition(self) -> None:
        if self.pending != "pending":
            return
        self.pending = None
        if self.state is None or self.state == "Invalidated" or self.desynced:
            pass
        elif self.suspended:
            # resume() restarted the timer without a mark.
            self.suspended = False
        elif self.state != "Scheduled":
            self._mismatch("composition while not Scheduled")
        elif not self.timer:
            # resume() (TC:257) started the timer without a mark, so the stretch
            # since the last message was suspended.
            self._relabel_since_message((UNKNOWN, SUSPENDED))
        # The composition took the lock, so the replay knows the state again.
        self.desynced = None
        self._take_reasons()

    def _resolve_pending(self, reported_state: str) -> None:
        if self.pending == "pending":
            if reported_state in IN_PROGRESS_STATES:
                self._apply_rlt_transition()
        elif self.pending == "tentative":
            self.pending = None
            # Returned after TC:586 (TC:590, 603), so the transition happened.
            if reported_state in IN_PROGRESS_STATES:
                self._take_reasons()

    def rlt_begin(self, t: int, rlt_id: int, has_frame: bool) -> None:
        self.in_rlt = True
        self.rlt_id = rlt_id
        self.pending = "pending" if has_frame else "tentative"
        self._enter((COMPOSITING, None), t)

    def frame(self, rlt_id: int) -> None:
        if rlt_id == self.rlt_id:
            self._apply_rlt_transition()

    def rlt_end(self, t: int, has_frame: bool) -> None:
        self.in_rlt = False
        if has_frame:
            self._apply_rlt_transition()
        else:
            self.after_abort = True
        self._enter(self._between_label(), t)

    def schedule(self, t: int, message: StateMessage) -> None:
        self.messages += 1
        self._resolve_pending(message.state)
        if RENDERING_UPDATE_REASON in message.reasons and (
            RENDERING_UPDATE_REASON not in self.reasons
        ):
            self.request_scs.append((t, message.state))
        # Only an unsuspended compositor starts the timer (TC:692, 696), whether
        # or not the checks apply.
        resumed = self.suspended and bool(message.timer)
        if resumed:
            self.suspended = False
        checks = self._checks_apply()
        if checks and self.state != message.state:
            # One mismatch per message: what else it reports follows from it.
            self._mismatch("ScheduleComposition state")
        elif checks:
            if self.suspended or resumed:
                # A suspended timer neither stops nor mismatches.
                pass
            elif (
                self.state == "Scheduled"
                and message.state == "Scheduled"
                and self.timer
                and message.timer is False
            ):
                # suspend() stopped the timer at TC:244 without a mark.
                self.suspended = True
                self._relabel_since_message((UNKNOWN, SUSPENDED))
            elif message.timer is not None and self.timer != message.timer:
                self._mismatch("ScheduleComposition timer")
            # Only a request (TC:655) sets the flag and only finished tiles
            # (TC:290) clear it.
            if message.waiting != self.waiting:
                legal = (
                    message.waiting and RENDERING_UPDATE_REASON in message.reasons
                ) or (self.waiting and not message.waiting)
                if not legal:
                    self._mismatch("ScheduleComposition waiting for tiles")
        self.after_abort, self.desynced = False, None
        self.reasons = set(message.reasons)
        self.state = message.state
        if message.timer is not None:
            self.timer = message.timer
        self._set_waiting(message.waiting, t)
        if message.state == "Idle":
            self.state = "Scheduled"
            self.timer = not message.waiting and not self.suspended
        elif message.state == "Scheduled":
            self.timer = bool(self.timer) or not self.suspended
        elif message.state == "InProgress":
            self.state = "ScheduledWhileInProgress"
        self._after_message(t)

    def frame_complete(self, t: int, message: StateMessage) -> None:
        if message.state not in IN_PROGRESS_STATES:
            # No-op FrameCompletes change nothing (TC:715-717), but tell whether an
            # aborted RLT returned before its transition.
            self.messages += 1
            self._resolve_pending(message.state)
            if message.state == "Invalidated":
                self.state = "Invalidated"
                self._after_message(t)
            elif not self._checks_apply() or self.state != message.state:
                # Adopt the state where the replay does not know it, and resync
                # where it contradicts the replay.
                # Unknown before, or a contradiction now.
                unknown = self._between_label()[1]
                if self._checks_apply():
                    self._mismatch("FrameComplete state")
                    unknown = MISMATCH
                self.after_abort = False
                # Idle tells all, Scheduled not whether the timer runs.
                self.desynced = unknown if message.state == "Scheduled" else None
                if message.state == "Idle":
                    self.timer = False
                self.state = message.state
                self.reasons = set(message.reasons)
                self._set_waiting(message.waiting, t)
                self._after_message(t)
            elif self.waiting != message.waiting:
                # The state agrees, the tile wait not: finished tiles clear it with
                # a ScheduleComposition (TC:289-290), which the replay lacks.
                self._mismatch("FrameComplete waiting for tiles")
                self._set_waiting(message.waiting, t)
                self._after_message(t)
            return
        self.messages += 1
        self._resolve_pending(message.state)
        if self._checks_apply():
            if self.state != message.state:
                self._mismatch("FrameComplete state")
            elif self.waiting != message.waiting:
                self._mismatch("FrameComplete waiting for tiles")
        self.after_abort, self.desynced = False, None
        self.reasons = set(message.reasons)
        self._set_waiting(message.waiting, t)
        if message.state == "InProgress":
            held = RENDERING_UPDATE_REASON in message.reasons and message.waiting
            self.state = "Scheduled" if held else "Idle"
            self.timer = False
        else:
            self.state = "Scheduled"
            only_update_waiting = (
                # OptionSet::containsOnly(), true of no reason too.
                set(message.reasons) <= {RENDERING_UPDATE_REASON}
                and message.waiting
            )
            self.timer = not only_update_waiting and not self.suspended
        self._after_message(t)

    def _checks_apply(self) -> bool:
        # Nothing to check before the first message, after an invalidation (a new
        # compositor may follow) or after an aborted composition, whose outcome
        # only the message tells.
        return (
            self.state is not None
            and self.state != "Invalidated"
            and not self.after_abort
            and not self.desynced
        )

    def _after_message(self, t: int) -> None:
        if not self.in_rlt:
            self._enter(self._between_label(), t)
        self.message_index = len(self.segments)

    def finish(self, end: int) -> None:
        self._enter((UNKNOWN, None), max(end, self.since))
        if self.waiting_since is not None:
            self.tile_waits.append((self.waiting_since, max(end, self.waiting_since)))
            self.waiting_since = None
        # Every message starts a segment, so merge before judging lengths.
        self.segments = merge_segments(self.segments)
        if self.reference_nsec:
            self._relabel_long_segments()
            self.segments = merge_segments(self.segments)

    def _relabel_long_segments(self) -> None:
        assert self.reference_nsec
        for i, (begin, end, state, detail) in enumerate(self.segments):
            length = end - begin
            if state == SCHEDULED and length > (
                MAYBE_SUSPENDED_REFRESHES * self.reference_nsec
            ):
                self.segments[i] = (begin, end, UNKNOWN, MAYBE_SUSPENDED)
            elif state == AWAITING_FRAME_COMPLETE and length > (
                STALLED_REFRESHES * self.reference_nsec
            ):
                self.segments[i] = (begin, end, UNKNOWN, STALLED)


class ReplayResult(NamedTuple):
    segments_by_pid: Dict[int, List[Segment]]
    tile_waits_by_pid: Dict[int, List[Tuple[int, int]]]
    request_scs_by_pid: Dict[int, List[Tuple[int, str]]]
    mismatches_by_pid: Dict[int, Dict[str, int]]
    messages_by_pid: Dict[int, int]
    fallback_by_pid: Dict[int, bool]


# Order of events at one timestamp. An RLT ending as the next begins ends first.
# Messages come under the lock after the RLT.
_EARLIER_RLT_END, _RLT_BEGIN, _FRAME, _RLT_END, _MESSAGE = range(5)


def replay(
    data: Dict[str, Any],
    compositions: Sequence[Dict[str, Any]],
    reference: Optional[float],
) -> ReplayResult:
    """Replay the compositor of every process across the whole capture."""
    marks = data["marks"]
    begin = data["document"]["timespan"]["begin"]
    end = data["document"]["timespan"]["end"]
    reference_nsec = msec_to_nsec(1) * reference if reference else None
    compositions_by_pid = marks_by_process(compositions)
    frames_by_pid: Dict[int, List[Tuple[int, int]]] = {
        pid: sorted((c["frame_nsec"], c["rlt_id"]) for c in group)
        for pid, group in compositions_by_pid.items()
    }
    # The RLTs match_compositions() gave a frame, so both agree on what aborted.
    composited_by_pid: Dict[int, Set[int]] = {
        pid: {c["rlt_id"] for c in group} for pid, group in compositions_by_pid.items()
    }
    rlts_by_pid = marks_by_process(marks.get(COMPOSITION_MARK, []))
    messages_by_pid: Dict[int, List[Tuple[int, str, StateMessage]]] = {}
    for kind, name in (("sc", SCHEDULE_MARK), ("fc", FRAME_COMPLETE_MARK)):
        for mark in marks.get(name, []):
            message = parse_state_message(mark["message"])
            if message is not None:
                messages_by_pid.setdefault(mark_pid(mark), []).append(
                    (mark_begin(mark), kind, message)
                )

    result = ReplayResult({}, {}, {}, {}, {}, {})
    for pid in sorted(set(rlts_by_pid) | set(messages_by_pid)):
        rlts = rlts_by_pid.get(pid, [])
        frames = frames_by_pid.get(pid, [])
        composited = composited_by_pid.get(pid, set())
        if pid not in messages_by_pid:
            result.segments_by_pid[pid] = _fallback_segments(
                rlts, begin, end, reference_nsec
            )
            result.fallback_by_pid[pid] = True
            result.tile_waits_by_pid[pid] = []
            result.request_scs_by_pid[pid] = []
            result.mismatches_by_pid[pid] = {}
            result.messages_by_pid[pid] = 0
            continue

        events: List[Tuple[int, int, int, Any]] = []
        for rlt in rlts:
            rlt_begin, rlt_end = mark_begin(rlt), rlt["end_time"]
            has_frame = id(rlt) in composited
            events.append((rlt_begin, _RLT_BEGIN, len(events), (id(rlt), has_frame)))
            order = _EARLIER_RLT_END if rlt_begin < rlt_end else _RLT_END
            events.append((rlt_end, order, len(events), has_frame))
        for frame, rlt_id in frames:
            events.append((frame, _FRAME, len(events), rlt_id))
        for t, kind, message in messages_by_pid[pid]:
            events.append((t, _MESSAGE, len(events), (kind, message)))
        events.sort(key=lambda event: event[:3])

        # Marks may begin before the capture's timespan.
        first = min(begin, events[0][0]) if events else begin
        compositor = CompositorReplay(first, reference_nsec)
        for t, order, _, payload in events:
            if order == _RLT_BEGIN:
                compositor.rlt_begin(t, *payload)
            elif order == _FRAME:
                compositor.frame(payload)
            elif order in (_EARLIER_RLT_END, _RLT_END):
                compositor.rlt_end(t, payload)
            elif payload[0] == "sc":
                compositor.schedule(t, payload[1])
            else:
                compositor.frame_complete(t, payload[1])
        compositor.finish(end)

        result.segments_by_pid[pid] = compositor.segments
        result.fallback_by_pid[pid] = False
        result.tile_waits_by_pid[pid] = compositor.tile_waits
        result.request_scs_by_pid[pid] = compositor.request_scs
        result.mismatches_by_pid[pid] = dict(compositor.mismatches)
        result.messages_by_pid[pid] = compositor.messages
    return result


def merge_segments(segments: Iterable[Segment]) -> List[Segment]:
    """The segments with touching neighbours of one state and detail joined."""
    merged: List[Segment] = []
    for segment in segments:
        if merged and merged[-1][1] == segment[0] and merged[-1][2:] == segment[2:]:
            merged[-1] = (merged[-1][0], segment[1]) + segment[2:]
        else:
            merged.append(segment)
    return merged


def _fallback_segments(
    rlts: Sequence[Dict[str, Any]],
    begin: int,
    end: int,
    reference_nsec: Optional[float],
) -> List[Segment]:
    """Compositing and the gaps between, for a process without state messages."""
    segments: List[Segment] = []
    spans = merged_spans((mark_begin(rlt), rlt["end_time"]) for rlt in rlts)
    # RLTs may begin before the capture's timespan, as in replay().
    cursor = min([begin] + [span[0] for span in spans[:1]])
    for rlt_begin, rlt_end in spans:
        if rlt_begin > cursor:
            segments.append(_gap(cursor, rlt_begin, reference_nsec))
        segments.append((max(rlt_begin, cursor), rlt_end, COMPOSITING, None))
        cursor = max(cursor, rlt_end)
    if end > cursor:
        segments.append(_gap(cursor, end, reference_nsec))
    return segments


def _gap(begin: int, end: int, reference_nsec: Optional[float]) -> Segment:
    if reference_nsec and end - begin > GAP_IDLE_REFRESHES * reference_nsec:
        return (begin, end, IDLE, GAP)
    return (begin, end, GAP, None)


def annotate_intervals(
    compositions: Sequence[Dict[str, Any]],
    replay_result: ReplayResult,
    reference: Optional[float],
    page_busy_by_pid: Dict[int, MarkIndex],
) -> None:
    """Set each composition's frame interval and whether it was active, meaning
    the compositor or the page worked on the next frame all along. Idle while
    the page is busy (see page_busy_spans()) awaited the page, and up to a
    refresh of idle before then awaited the display link. Other idle and
    unknown time is inactive, except stalls and late timers (suspended?),
    which are jank.
    """
    tolerance = (
        msec_to_nsec(DISPLAY_LINK_WAIT_REFRESHES * reference) if reference else 0
    )
    empty = MarkIndex([])
    for pid, group in marks_by_process(compositions).items():
        segments = replay_result.segments_by_pid.get(pid, [])
        segment_begins = [segment[0] for segment in segments]
        page_busy = page_busy_by_pid.get(pid, empty)
        previous: Optional[int] = None
        for composition in group:
            frame = composition["frame_nsec"]
            composition["interval"] = None
            composition["active"] = False
            composition["missed_refreshes"] = None
            if previous is not None:
                interval = nsec_to_msec(frame - previous)
                composition["interval"] = interval
                active = _is_active(
                    segments_overlapping(segments, segment_begins, previous, frame),
                    page_busy,
                    tolerance,
                )
                composition["active"] = active
                if active and reference:
                    composition["missed_refreshes"] = max(
                        0, round(interval / reference) - 1
                    )
            previous = frame


def _is_active(
    segments: Sequence[Segment], page_busy: MarkIndex, tolerance: float
) -> bool:
    idle_while_page_idle = 0
    for begin, end, state, detail in segments:
        if state not in INACTIVE_STATES or detail in (STALLED, MAYBE_SUSPENDED):
            continue
        if state != IDLE:
            return False
        busy = sum(b - a for a, b in merged_spans(page_busy.spans_within(begin, end)))
        idle_while_page_idle += end - begin - busy
    if not idle_while_page_idle:
        return True
    # Only a busy page in the interval makes the idle before it a wait.
    first, last = segments[0][0], segments[-1][1]
    return idle_while_page_idle < tolerance and bool(
        page_busy.marks_overlapping(first, last)
    )


def page_busy_spans(data: Dict[str, Any]) -> Dict[int, MarkIndex]:
    """Per pid, the (begin, end) spans in which the page worked on new content:
    each LayerTreeHostRenderingUpdate, and the wait before it from the first
    LayerTreeHostScheduleRenderingUpdate since the previous one ended. That
    wait is the main thread busy with other tasks, or the DidComposite that
    lets a schedule made while waiting for the renderer go on.
    """
    marks = data["marks"]
    schedules_by_pid = {
        pid: sorted(mark_begin(mark) for mark in group)
        for pid, group in marks_by_process(
            marks.get(SCHEDULE_RENDERING_UPDATE_MARK, [])
        ).items()
    }
    spans_by_pid: Dict[int, MarkIndex] = {}
    for pid, group in marks_by_process(marks.get(RENDERING_UPDATE_MARK, [])).items():
        schedules = schedules_by_pid.get(pid, [])
        spans: List[Tuple[int, int]] = []
        previous_end: Optional[int] = None
        for update in sorted(group, key=mark_begin):
            begin, end = mark_begin(update), update["end_time"]
            first = (
                bisect.bisect_right(schedules, previous_end)
                if previous_end is not None
                else 0
            )
            if first < len(schedules) and schedules[first] < begin:
                spans.append((schedules[first], begin))
            spans.append((begin, end))
            previous_end = end
        spans_by_pid[pid] = MarkIndex(
            spans, begin=lambda span: span[0], end=lambda span: span[1]
        )
    return spans_by_pid


class CompositionModel(NamedTuple):
    compositions: List[Dict[str, Any]]
    aborted_by_pid: Dict[int, List[int]]
    reference: Optional[float]
    reference_source: Optional[str]
    reference_marks: int
    replay: ReplayResult


def calculate_compositions(data: Dict[str, Any]) -> CompositionModel:
    """The compositions and the compositor state timeline of untrimmed data."""
    compositions, aborted = match_compositions(data)
    reference, source, reference_marks = reference_interval(data, compositions)
    replay_result = replay(data, compositions, reference)
    annotate_intervals(compositions, replay_result, reference, page_busy_spans(data))
    return CompositionModel(
        compositions,
        aborted,
        reference,
        source,
        reference_marks,
        replay_result,
    )


def indices_by_pid(compositions: Sequence[Dict[str, Any]]) -> Dict[int, List[int]]:
    """The indices into `compositions` of each process, in frame order."""
    indices: Dict[int, List[int]] = {}
    for index, composition in enumerate(compositions):
        indices.setdefault(composition["pid"], []).append(index)
    return indices


def in_window(
    items: Iterable[Dict[str, Any]],
    begin: Optional[int],
    end: Optional[int],
    key: str,
) -> List[Dict[str, Any]]:
    """The items whose `key` time lies in [begin, end)."""
    return [
        item
        for item in items
        if (begin is None or item[key] >= begin) and (end is None or item[key] < end)
    ]


def segments_overlapping(
    segments: Sequence[Segment], begins: Sequence[int], begin: int, end: int
) -> List[Segment]:
    """The sorted, disjoint segments clipped to [begin, end). `begins` are theirs."""
    first = max(0, bisect.bisect_right(begins, begin) - 1)
    last = bisect.bisect_left(begins, end)
    return clip_segments(segments[first:last], begin, end)


def clip_segments(
    segments: Iterable[Segment], begin: Optional[int], end: Optional[int]
) -> List[Segment]:
    clipped = []
    for segment_begin, segment_end, state, detail in segments:
        if begin is not None:
            segment_begin = max(segment_begin, begin)
        if end is not None:
            segment_end = min(segment_end, end)
        if segment_end > segment_begin:
            clipped.append((segment_begin, segment_end, state, detail))
    return clipped
