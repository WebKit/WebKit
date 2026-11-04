import collections
import json
from pathlib import Path

from .helpers import (
    SysprofTestCase,
    approx,
    composition,
    frame_complete,
    instant,
    mark,
    presented,
    rendering_update,
    scheduled,
    steady_animation,
    sysprof_data,
)

from webkitsysprof import analyze
from webkitsysprof.compositions import (
    CompositorReplay,
    calculate_compositions,
    classify,
    parse_reasons,
    parse_state_message,
    reference_interval,
)
from webkitsysprof.parser import parse
from webkitsysprof.updates import calculate_rendering_updates
from webkitsysprof.utils import msec_to_nsec, sysprof_data_with_marks_by_name

SAMPLE_CAPTURE_FILE = str(Path(__file__).parent / "assets" / "sample.syscap")
# 700 ms of a real capture: an animation, an idle stretch, rendering updates whose
# tiles were still being painted, and updates arriving mid-frame.
EXCERPT_FILE = Path(__file__).parent / "assets" / "composition-excerpt.json"
RU, ANIMATION = "RenderingUpdate", "Animation"


def model_of(marks, end_msec=1000):
    return calculate_compositions(sysprof_data(marks, 0, end_msec))


def states(model, pid=1):
    """The state timeline in milliseconds, as (begin, end, state, detail)."""
    return [
        (round(begin / 1e6, 3), round(end / 1e6, 3), state, detail)
        for begin, end, state, detail in model.replay.segments_by_pid[pid]
    ]


def stretches(model, detail, pid=1):
    return sum(
        1 for segment in model.replay.segments_by_pid[pid] if segment[3] == detail
    )


class ReasonsTest(SysprofTestCase):
    def test_reasons_are_parsed_into_canonical_order(self):
        self.assertEqual(parse_reasons("reasons: Animation"), (ANIMATION,))
        self.assertEqual(
            parse_reasons("reasons: Animation, RenderingUpdate"), (RU, ANIMATION)
        )
        self.assertEqual(
            parse_reasons("reasons: Animation,VideoFrame"), ("VideoFrame", ANIMATION)
        )
        # An unknown reason is kept, after the known ones.
        self.assertEqual(
            parse_reasons("reasons: Scrolling, Animation"), (ANIMATION, "Scrolling")
        )
        # endMark appends " | ..." to a message, which is no reason.
        self.assertEqual(parse_reasons("reasons: Animation | extra"), (ANIMATION,))

    def test_an_empty_list_is_no_reason_and_no_list_is_unrecorded(self):
        self.assertEqual(parse_reasons("reasons: "), ())
        self.assertIsNone(parse_reasons(""))
        self.assertIsNone(parse_reasons("compositionResponseID 5"))
        self.assertEqual(classify(()), "no_reason")
        self.assertEqual(classify(None), "unrecorded")
        self.assertEqual(classify((RU,)), "update")
        self.assertEqual(classify((RU, ANIMATION)), "update")
        self.assertEqual(classify((ANIMATION,)), "compositor")

    def test_a_state_message_is_parsed_apart_from_its_reasons(self):
        message = scheduled(0, "Idle", waiting=True, reasons=(RU, ANIMATION))["message"]
        parsed = parse_state_message(message)
        self.assertEqual(parsed.reasons, {RU, ANIMATION})
        self.assertEqual(parsed.state, "Idle")
        self.assertTrue(parsed.waiting)
        self.assertFalse(parsed.timer)
        # Read as a DidRenderFrame message, the state would become a reason, and
        # a test for RenderingUpdate alone could never hold.
        self.assertIn("state: Idle", parse_reasons(message))
        parsed = parse_state_message(frame_complete(0, "InProgress")["message"])
        self.assertEqual(parsed.reasons, set())
        self.assertIsNone(parsed.timer)


class MatchingTest(SysprofTestCase):
    def test_a_composition_is_the_render_layer_tree_holding_the_frame(self):
        model = model_of(composition(10, 12, (RU, ANIMATION)))
        (c,) = model.compositions
        self.assertEqual(c["begin_nsec"], msec_to_nsec(10))
        self.assertEqual(c["frame_nsec"], msec_to_nsec(11))
        self.assertEqual(c["duration"], approx(2.0))
        self.assertEqual(c["flush"], approx(0.5))
        self.assertEqual(c["paint"], approx(0.5))
        self.assertEqual(c["class"], "update")
        self.assertEqual(c["combination"], "RenderingUpdate + Animation")

    def test_a_composition_shorter_than_a_millisecond_is_matched(self):
        self.assertEqual(len(model_of(composition(10, 10.4)).compositions), 1)

    def test_compositions_keep_to_their_process(self):
        model = model_of(composition(10, 12, pid=1) + composition(10.5, 11.5, pid=2))
        self.assertEqual(sorted(c["pid"] for c in model.compositions), [1, 2])

    def test_a_render_layer_tree_without_a_frame_is_aborted(self):
        model = model_of([mark("RenderLayerTree", 3, 5)])
        self.assertEqual(model.compositions, [])
        self.assertEqual(model.aborted_by_pid, {1: [msec_to_nsec(3)]})

    def test_tile_uploads_count_where_they_overlap_the_composition(self):
        model = model_of(
            composition(10, 20)
            + [
                mark("SkiaBackingStoreTileUpdate", 9, 12),
                mark("SkiaBackingStoreTileUpdate", 11, 13),
                mark("SkiaBackingStoreTileUpdate", 30, 31),
            ]
        )
        self.assertEqual(model.compositions[0]["upload"], approx(3.0))


class ReplayTest(SysprofTestCase):
    def assertNoMismatches(self, model, pid=1):
        self.assertEqual(model.replay.mismatches_by_pid[pid], {})

    def test_a_steady_animation_alternates_compositing_and_the_round_trip(self):
        model = model_of(steady_animation(100, 3))
        self.assertNoMismatches(model)
        self.assertEqual(
            states(model),
            [
                (0, 99.5, "unknown", "before first message"),
                (99.5, 100, "scheduled", None),
                (100, 101, "compositing", None),
                (101, 115.5, "awaiting_frame_complete", None),
                (115.5, 116, "scheduled", None),
                (116, 117, "compositing", None),
                (117, 131.5, "awaiting_frame_complete", None),
                (131.5, 132, "scheduled", None),
                (132, 133, "compositing", None),
                (133, 147.5, "awaiting_frame_complete", None),
                (147.5, 1000, "idle", None),
            ],
        )
        self.assertEqual(model.replay.messages_by_pid[1], 6)

    def test_idle_lasts_from_the_frame_complete_to_the_next_request(self):
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [frame_complete(20, "InProgress")]
            + [scheduled(60, "Idle", reasons=(ANIMATION,))]
            + composition(60.1, 61)
            + [frame_complete(70, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((20, 60, "idle", None), states(model))
        self.assertIn((60, 60.1, "scheduled", None), states(model))
        # The interval holds idle time, so it says nothing about how fast frames
        # came while they were wanted.
        self.assertFalse(model.compositions[1]["active"])

    def test_a_rendering_update_waiting_for_tiles_blocks_the_compositor(self):
        model = model_of(
            # The request while tiles are pending moves Idle to Scheduled without
            # a timer (TC:688-691).
            [scheduled(10, "Idle", waiting=True, reasons=(RU,))]
            # An Animation request then starts the timer while waiting goes on.
            + [scheduled(12, "Scheduled", waiting=True, reasons=(RU, ANIMATION))]
            + composition(12.1, 13, (ANIMATION,))
            # RenderingUpdate was held back (TC:576-581), so the FrameComplete
            # returns to Scheduled without a timer (TC:720-721).
            + [frame_complete(20, "InProgress", waiting=True, reasons=(RU,))]
            # The tiles are done, which starts the timer (TC:281-291).
            + [scheduled(30, "Scheduled", reasons=(RU,))]
            + composition(30.1, 31, (RU,))
            + [frame_complete(40, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertEqual(
            states(model),
            [
                (0, 10, "unknown", "before first message"),
                (10, 12, "blocked_on_tiles", None),
                (12, 12.1, "scheduled", None),
                (12.1, 13, "compositing", None),
                (13, 20, "awaiting_frame_complete", None),
                (20, 30, "blocked_on_tiles", None),
                (30, 30.1, "scheduled", None),
                (30.1, 31, "compositing", None),
                (31, 40, "awaiting_frame_complete", None),
                (40, 1000, "idle", None),
            ],
        )
        self.assertEqual(
            model.replay.tile_waits_by_pid[1], [(msec_to_nsec(10), msec_to_nsec(30))]
        )
        self.assertEqual(
            model.replay.request_scs_by_pid[1], [(msec_to_nsec(10), "Idle")]
        )

    def test_only_a_rendering_update_waiting_for_tiles_starts_no_timer(self):
        # TC:726-728: ScheduledWhileInProgress returns to Scheduled, but starts no
        # timer for a rendering update that waits for its tiles.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11, (ANIMATION,))
            + [scheduled(10.5, "InProgress", waiting=True, reasons=(RU,))]
            + [
                frame_complete(
                    20, "ScheduledWhileInProgress", waiting=True, reasons=(RU,)
                )
            ]
            + [scheduled(25, "Scheduled", reasons=(RU,))]
            + composition(25.1, 26, (RU,))
            + [frame_complete(35, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((20, 25, "blocked_on_tiles", None), states(model))

    def test_a_no_op_frame_complete_against_the_replay_is_a_mismatch(self):
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [frame_complete(20, "Idle")]
        )
        self.assertEqual(model.replay.mismatches_by_pid[1], {"FrameComplete state": 1})
        self.assertIn((20, 1000, "idle", None), states(model))
        # Resynced to Scheduled, the timer is unknown, which is no suspension.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [frame_complete(20, "Scheduled")]
        )
        self.assertEqual(states(model)[-1], (11, 1000, "unknown", "mismatch"))
        # A composition after it resyncs the replay.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [frame_complete(20, "Scheduled")]
            + composition(30, 31)
            + [frame_complete(40, "InProgress")]
        )
        self.assertIn((31, 40, "awaiting_frame_complete", None), states(model))

    def test_a_no_op_frame_complete_corrects_the_tile_wait(self):
        model = model_of(
            [scheduled(10, "Idle", waiting=True, reasons=(RU,))]
            + [frame_complete(20, "Scheduled", reasons=(RU,))]
        )
        self.assertEqual(
            model.replay.mismatches_by_pid[1], {"FrameComplete waiting for tiles": 1}
        )
        self.assertEqual(
            model.replay.tile_waits_by_pid[1], [(msec_to_nsec(10), msec_to_nsec(20))]
        )

    def test_a_no_op_frame_complete_tells_the_state_the_replay_lost(self):
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 20, 30)]
            + [frame_complete(40, "Idle")]
        )
        self.assertNoMismatches(model)
        self.assertEqual(states(model)[-1], (40, 1000, "idle", None))
        # Scheduled leaves the timer unknown, for the reason it was unknown before.
        model = model_of([frame_complete(40, "Scheduled")])
        self.assertNoMismatches(model)
        self.assertEqual(states(model), [(0, 1000, "unknown", "before first message")])

    def test_a_running_timer_ends_a_suspension(self):
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [scheduled(50, "Scheduled", timer=False, reasons=(ANIMATION,))]
            + [scheduled(70, "Scheduled", timer=True, reasons=(ANIMATION,))]
            + composition(80, 81)
            + [frame_complete(90, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((70, 80, "scheduled", None), states(model))
        # Also where an aborted RLT left the checks off.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [scheduled(20, "Scheduled", timer=False, reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 30, 31)]
            + [scheduled(40, "Scheduled", timer=True, reasons=(ANIMATION,))]
            + composition(45, 46)
            + [frame_complete(55, "InProgress")]
        )
        self.assertIn((40, 45, "scheduled", None), states(model))

    def test_a_timer_that_stopped_without_a_mark_is_a_suspension(self):
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            # suspend() stopped the timer (TC:244), which no mark says.
            + [scheduled(12, "Scheduled", reasons=(ANIMATION,))]
            + composition(30, 31, (ANIMATION,))
            + [frame_complete(40, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertEqual(
            states(model)[1:4],
            [
                (10, 30, "unknown", "suspended"),
                (30, 31, "compositing", None),
                (31, 40, "awaiting_frame_complete", None),
            ],
        )
        self.assertEqual(stretches(model, "suspended"), 1)
        # A composition without a timer was resumed without a mark.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11, (ANIMATION,))
            + [scheduled(10.5, "InProgress", waiting=True, reasons=(RU,))]
            + [
                frame_complete(
                    20, "ScheduledWhileInProgress", waiting=True, reasons=(RU,)
                )
            ]
            + composition(30, 31, (ANIMATION,))
            + [frame_complete(40, "InProgress", reasons=(RU,), waiting=True)]
        )
        self.assertNoMismatches(model)
        self.assertIn((20, 30, "unknown", "suspended"), states(model))

    def test_an_aborted_composition_is_resolved_by_the_next_message(self):
        # Returned after the transition (TC:603): the message reports InProgress.
        after = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 10.1, 11)]
            + [scheduled(15, "InProgress", reasons=(ANIMATION,))]
        )
        self.assertNoMismatches(after)
        self.assertEqual(
            states(after)[3:5],
            [
                (11, 15, "unknown", "aborted"),
                (15, 1000, "awaiting_frame_complete", None),
            ],
        )
        # Returned before it (TC:572): the message reports Scheduled.
        before = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 10.1, 11)]
            + [scheduled(15, "Scheduled", reasons=(ANIMATION,))]
            + composition(15.1, 16)
            + [frame_complete(25, "InProgress")]
        )
        self.assertNoMismatches(before)
        self.assertIn((11, 15, "unknown", "aborted"), states(before))
        self.assertIn((15, 15.1, "scheduled", None), states(before))
        # A no-op FrameComplete reporting Scheduled says so too, so the reasons
        # were not taken.
        model = model_of(
            [scheduled(10, "Idle", reasons=(RU,))]
            + [mark("RenderLayerTree", 10.1, 11)]
            + [frame_complete(12, "Scheduled", reasons=(RU,))]
            + [scheduled(15, "InProgress", reasons=(RU, ANIMATION))]
        )
        self.assertEqual(
            model.replay.request_scs_by_pid[1], [(msec_to_nsec(10), "Idle")]
        )

    def test_a_frame_complete_leaving_only_a_waiting_update_arms_no_timer(self):
        # isOnlyRenderingUpdatePendingAndWaitingForTiles() at TC:726.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [scheduled(12, "InProgress", reasons=(RU,), waiting=True)]
            + [
                frame_complete(
                    20, "ScheduledWhileInProgress", reasons=(RU,), waiting=True
                )
            ]
            + [scheduled(30, "Scheduled", reasons=(RU,))]
        )
        self.assertNoMismatches(model)
        self.assertIn((20, 30, "blocked_on_tiles", None), states(model))
        # containsOnly() holds for no reason too.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [scheduled(12, "InProgress", reasons=(RU,), waiting=True)]
            + [frame_complete(20, "ScheduledWhileInProgress", reasons=(), waiting=True)]
            + [scheduled(30, "Scheduled", reasons=(RU,))]
        )
        self.assertNoMismatches(model)
        self.assertIn((20, 30, "blocked_on_tiles", None), states(model))

    def test_an_aborted_composition_without_a_timer_is_no_resume(self):
        model = model_of(
            [scheduled(10, "Idle", waiting=True, reasons=(RU,))]
            + [mark("RenderLayerTree", 30, 31)]
            + [scheduled(40, "Scheduled", reasons=(RU,))]
        )
        self.assertNoMismatches(model)
        self.assertIn((31, 40, "unknown", "aborted"), states(model))
        self.assertEqual(stretches(model, "suspended"), 0)

    def test_events_at_one_instant_apply_in_order(self):
        # The frame lands on the end of its RenderLayerTree, and the FrameComplete
        # on the same instant. The frame is still the composition's.
        model = model_of(
            [scheduled(5, "Idle", reasons=(ANIMATION,))]
            + composition(10, 11, frame_at=11)
            + [frame_complete(11, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertEqual(len(model.compositions), 1)
        self.assertEqual(
            states(model)[1:],
            [
                (5, 10, "scheduled", None),
                (10, 11, "compositing", None),
                (11, 1000, "idle", None),
            ],
        )

    def test_marks_before_the_capture_timespan_are_replayed(self):
        model = model_of(
            [scheduled(-2, "Idle", reasons=(ANIMATION,))]
            + composition(-1, 3)
            + [frame_complete(10, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((-1, 3, "compositing", None), states(model))
        # Also without state messages.
        model = model_of(composition(-5, -3) + composition(20, 21))
        self.assertEqual(states(model)[0], (-5, -3, "compositing", None))

    def test_an_rlt_beginning_as_the_previous_one_ends_is_compositing(self):
        model = model_of(
            [scheduled(5, "Idle", reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 10, 20)]
            + [scheduled(15, "Scheduled", reasons=(ANIMATION,))]
            + composition(20, 30)
            + [frame_complete(40, "InProgress")]
        )
        # Merged with the aborted RLT before it, with no gap.
        self.assertIn((10, 30, "compositing", None), states(model))

    def test_a_frame_interval_across_a_stall_is_active(self):
        model = model_of(
            [presented(1), scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [scheduled(12, "InProgress", reasons=(ANIMATION,))]
            + [frame_complete(300, "ScheduledWhileInProgress", reasons=(ANIMATION,))]
            + composition(300.1, 301)
            + [frame_complete(310, "InProgress")]
        )
        self.assertIn((11, 300, "unknown", "stalled"), states(model))
        stalled = model.compositions[1]
        self.assertTrue(stalled["active"])
        self.assertGreater(stalled["missed_refreshes"], 10)

    def test_a_late_composition_timer_is_jank_not_inactivity(self):
        # The zero-delay timer fired three refreshes late, maybe a busy run loop.
        model = model_of(
            [presented(1), scheduled(99.5, "Idle", reasons=(ANIMATION,))]
            + composition(100, 101)
            + [scheduled(100.3, "InProgress", reasons=(ANIMATION,))]
            + [frame_complete(115.5, "ScheduledWhileInProgress", reasons=(ANIMATION,))]
            + composition(166, 167)
            + [frame_complete(180, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((115.5, 166, "unknown", "suspended?"), states(model))
        late = model.compositions[1]
        self.assertTrue(late["active"])
        self.assertEqual(late["missed_refreshes"], 3)

    def test_messages_do_not_split_a_stretch_below_its_threshold(self):
        # Every message starts a segment, so the thresholds must see whole stretches.
        model = model_of(
            [presented(1), scheduled(10, "Idle", reasons=(ANIMATION,))]
            + [scheduled(20, "Scheduled", timer=True, reasons=(ANIMATION,))]
            + composition(30, 31)
            + [scheduled(40, "InProgress", reasons=(ANIMATION,))]
            + [
                scheduled(40 + 16 * k, "ScheduledWhileInProgress", reasons=(ANIMATION,))
                for k in range(1, 15)
            ]
            + [frame_complete(300, "ScheduledWhileInProgress", reasons=(ANIMATION,))]
            + composition(300.1, 301)
            + [frame_complete(310, "InProgress")]
        )
        self.assertNoMismatches(model)
        self.assertIn((10, 30, "unknown", "suspended?"), states(model))
        self.assertIn((31, 300, "unknown", "stalled"), states(model))
        self.assertEqual(stretches(model, "stalled"), 1)

    def test_a_request_before_the_lock_is_applied_to_the_state_before_it(self):
        # The RenderLayerTree began (TC:557), and the main thread asked for a
        # composition before the compositor took the lock (TC:566).
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11, (RU, ANIMATION), frame_at=10.6)
            + [
                scheduled(10.2, "Scheduled", timer=True, reasons=(RU, ANIMATION)),
                frame_complete(20, "InProgress"),
            ]
        )
        self.assertNoMismatches(model)
        self.assertEqual(
            model.replay.request_scs_by_pid[1], [(msec_to_nsec(10.2), "Scheduled")]
        )

    def test_the_waiting_flag_only_flips_the_ways_the_compositor_flips_it(self):
        # Set without a rendering update asking for it.
        model = model_of(
            [scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11)
            + [scheduled(10.5, "InProgress", waiting=True, reasons=(ANIMATION,))]
        )
        self.assertEqual(
            model.replay.mismatches_by_pid[1],
            {"ScheduleComposition waiting for tiles": 1},
        )
        # Still set, then reported clear by a FrameComplete, which clears nothing.
        model = model_of(
            [scheduled(10, "Idle", waiting=True, reasons=(RU,))]
            + [scheduled(12, "Scheduled", waiting=True, reasons=(RU, ANIMATION))]
            + composition(12.1, 13, (ANIMATION,))
            + [frame_complete(20, "InProgress", reasons=(RU,))]
        )
        self.assertEqual(
            model.replay.mismatches_by_pid[1], {"FrameComplete waiting for tiles": 1}
        )

    def test_frame_completes_that_change_nothing_agree_with_the_replay(self):
        model = model_of(
            [frame_complete(5, "Idle")]
            + steady_animation(100, 2)
            + [frame_complete(140, "Idle")]
        )
        self.assertNoMismatches(model)
        # Checked against the replay, so counted as messages.
        self.assertEqual(model.replay.messages_by_pid[1], 6)

    def test_an_invalidated_compositor_can_be_followed_by_a_new_one(self):
        model = model_of(
            steady_animation(100, 2)
            + [frame_complete(140, "Invalidated")]
            + steady_animation(200, 2)
        )
        self.assertNoMismatches(model)
        self.assertIn((140, 199.5, "unknown", "invalidated"), states(model))

    def test_a_frame_applies_only_the_composition_it_ends(self):
        # The next RLT begins on the nanosecond the previous one's frame lands.
        replay = CompositorReplay(0, None)
        replay.rlt_begin(10, rlt_id=1, has_frame=True)
        replay.rlt_end(11, has_frame=True)
        replay.rlt_begin(11, rlt_id=2, has_frame=True)
        replay.frame(rlt_id=1)
        self.assertEqual(replay.pending, "pending")
        replay.frame(rlt_id=2)
        self.assertIsNone(replay.pending)

    def test_a_message_contradicting_the_replay_is_a_mismatch(self):
        model = model_of(
            steady_animation(100, 3) + [scheduled(115.7, "Idle", reasons=(ANIMATION,))]
        )
        self.assertEqual(
            model.replay.mismatches_by_pid[1], {"ScheduleComposition state": 1}
        )
        self.assertIn((115.5, 115.7, "unknown", "mismatch"), states(model))

    def test_without_state_messages_the_gaps_between_compositions_are_read(self):
        marks = [presented(1)] + composition(10, 11) + composition(100, 101)
        marks += composition(110, 111)
        marks.append(instant("FrameComplete", 105))
        model = model_of(marks)
        self.assertTrue(model.replay.fallback_by_pid[1])
        self.assertIn((11, 100, "idle", "gap"), states(model))
        self.assertIn((101, 110, "gap", None), states(model))


class IntervalsTest(SysprofTestCase):
    def test_an_active_interval_counts_the_refreshes_it_missed(self):
        model = model_of([presented(1)] + steady_animation(100, 3, period=50))
        intervals = [c["interval"] for c in model.compositions if c["active"]]
        self.assertEqual(intervals, [approx(50.0), approx(50.0)])
        self.assertEqual(
            [c["missed_refreshes"] for c in model.compositions[1:]], [2, 2]
        )

    def _main_thread_bound(self, update_begin):
        """Two rendering update frames, the compositor idle from 20 ms until the
        second update asks at 50 ms."""
        return model_of(
            [presented(1), scheduled(10, "Idle", reasons=(RU,))]
            + composition(10.1, 11, (RU,))
            + [frame_complete(20, "InProgress")]
            + [rendering_update(update_begin, 50)]
            + [scheduled(50, "Idle", reasons=(RU,))]
            + composition(50.1, 51, (RU,))
            + [frame_complete(60, "InProgress")]
        )

    def test_idle_while_the_main_thread_updates_is_active(self):
        model = self._main_thread_bound(update_begin=15)
        self.assertIn((20, 50, "idle", None), states(model))
        slow = model.compositions[1]
        self.assertTrue(slow["active"])
        self.assertEqual(slow["missed_refreshes"], 1)

    def test_idle_under_a_refresh_before_an_update_awaits_the_display_link(self):
        self.assertTrue(
            self._main_thread_bound(update_begin=30).compositions[1]["active"]
        )

    def test_idle_over_a_refresh_before_an_update_is_inactive(self):
        self.assertFalse(
            self._main_thread_bound(update_begin=40).compositions[1]["active"]
        )

    def test_idle_while_a_scheduled_update_waits_for_the_main_thread_is_active(self):
        # The page scheduled an update at 22 ms, but a timer task held the main
        # thread until 45 ms, so the update could only start then.
        model = model_of(
            [presented(1), scheduled(10, "Idle", reasons=(RU,))]
            + composition(10.1, 11, (RU,))
            + [frame_complete(20, "InProgress")]
            + [instant("LayerTreeHostScheduleRenderingUpdate", 22)]
            + [rendering_update(45, 50)]
            + [scheduled(50, "Idle", reasons=(RU,))]
            + composition(50.1, 51, (RU,))
            + [frame_complete(60, "InProgress")]
        )
        self.assertTrue(model.compositions[1]["active"])

    def test_a_schedule_before_the_previous_update_ended_covers_no_idle(self):
        model = model_of(
            [presented(1), instant("LayerTreeHostScheduleRenderingUpdate", 2)]
            + [rendering_update(5, 9), scheduled(9, "Idle", reasons=(RU,))]
            + composition(10.1, 11, (RU,))
            + [frame_complete(20, "InProgress")]
            + [rendering_update(45, 50)]
            + [scheduled(50, "Idle", reasons=(RU,))]
            + composition(50.1, 51, (RU,))
            + [frame_complete(60, "InProgress")]
        )
        self.assertFalse(model.compositions[1]["active"])

    def test_an_interval_blocked_on_tiles_is_active(self):
        model = model_of(
            [presented(1), scheduled(10, "Idle", reasons=(ANIMATION,))]
            + composition(10.1, 11, (ANIMATION,))
            + [scheduled(10.5, "InProgress", waiting=True, reasons=(RU,))]
            + [
                frame_complete(
                    20, "ScheduledWhileInProgress", waiting=True, reasons=(RU,)
                )
            ]
            + [scheduled(45, "Scheduled", reasons=(RU,))]
            + composition(45.1, 46, (RU,))
            + [frame_complete(55, "InProgress")]
        )
        self.assertIn((20, 45, "blocked_on_tiles", None), states(model))
        self.assertTrue(model.compositions[1]["active"])

    def test_a_window_starting_mid_capture_keeps_its_first_interval(self):
        model = model_of(steady_animation(100, 5))
        report = analyze._prepare_composition_report(
            model, msec_to_nsec(120), msec_to_nsec(1000)
        )
        self.assertEqual(report["compositions"], 3)
        self.assertEqual(
            report["per_process"]["1"]["active_interval_statistics"]["n"], 3
        )

    def test_the_reference_interval_prefers_what_the_display_reports(self):
        vblanks = [
            mark("DisplayLinkUpdate", msec, msec, "WebKit (UI)", pid=0)
            for msec in (0, 8, 16, 24)
        ]
        frames = composition(10, 11) + composition(30, 31) + composition(50, 51)
        both = sysprof_data(vblanks + frames + [presented(5, 16.667)])
        self.assertEqual(
            reference_interval(both, calculate_compositions(both).compositions)[:2],
            (16.667, "WaylandFramePresented"),
        )
        # A refresh interval of zero is no refresh interval.
        zero = sysprof_data(vblanks + frames + [presented(5, 0)])
        self.assertEqual(
            reference_interval(zero, calculate_compositions(zero).compositions)[:2],
            (approx(8.0), "DisplayLinkUpdate"),
        )
        neither = sysprof_data(frames)
        self.assertEqual(
            reference_interval(neither, calculate_compositions(neither).compositions)[
                :2
            ],
            (approx(20.0), "frame intervals"),
        )


class RealCaptureTest(SysprofTestCase):
    def test_the_sample_capture_replays_without_a_mismatch(self):
        data = sysprof_data_with_marks_by_name(
            parse(SAMPLE_CAPTURE_FILE, marks=True, counters=False)
        )
        model = calculate_compositions(data)
        self.assertEqual(len(model.compositions), 2)
        for pid, mismatches in model.replay.mismatches_by_pid.items():
            self.assertEqual(mismatches, {}, pid)
        self.assertEqual(model.replay.messages_by_pid[2501], 6)

    def test_a_real_excerpt_replays_without_a_mismatch(self):
        excerpt = json.loads(EXCERPT_FILE.read_text())
        data = sysprof_data_with_marks_by_name(
            {"document": {"timespan": excerpt["timespan"]}, "marks": excerpt["marks"]}
        )
        model = calculate_compositions(data)
        pid = 242584

        self.assertEqual(model.replay.mismatches_by_pid, {pid: {}})
        self.assertEqual(model.replay.messages_by_pid[pid], 85)
        self.assertEqual(
            collections.Counter(c["combination"] for c in model.compositions),
            {"Animation": 25, "RenderingUpdate + Animation": 8, "RenderingUpdate": 1},
        )
        self.assertIn(
            (66.1, 108.3, "idle", None),
            [
                (round(begin / 1e6, 1), round(end / 1e6, 1), state, detail)
                for begin, end, state, detail in model.replay.segments_by_pid[pid]
            ],
        )
        updates = [u for u in calculate_rendering_updates(data, model) if u["asked"]]
        self.assertEqual(len(updates), 9)
        self.assertEqual(
            {(u["outcome"], u["pairing"]["status"]) for u in updates},
            {("composited", "exact")},
        )
        self.assertEqual(sum(u["deferred_for_tiles"] for u in updates), 2)
        self.assertEqual(sum(1 for u in updates if u["phases"]["tile_wait"] > 0), 8)
