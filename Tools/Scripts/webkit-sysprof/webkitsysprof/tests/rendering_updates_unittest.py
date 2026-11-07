from .helpers import (
    SysprofTestCase,
    approx,
    composition,
    frame_complete,
    mark,
    rendering_update,
    requested,
    scheduled,
    sysprof_data,
)

from webkitsysprof import analyze
from webkitsysprof.compositions import calculate_compositions
from webkitsysprof.updates import calculate_rendering_updates
from webkitsysprof.utils import msec_to_nsec

RU, ANIMATION = "RenderingUpdate", "Animation"


def updates_of(marks, end_msec=1000):
    data = sysprof_data(marks, 0, end_msec)
    model = calculate_compositions(data)
    return model, calculate_rendering_updates(data, model)


def asking(begin, end, request_at, state="Idle", waiting=False, pid=1, **kwargs):
    """A rendering update asking for a composition at `request_at`."""
    return [
        rendering_update(begin, end, pid=pid),
        scheduled(request_at, state, waiting=waiting, reasons=(RU,), pid=pid, **kwargs),
        requested(request_at + 0.01, pid=pid),
    ]


def phases(update):
    return {phase: approx(value, abs=1e-6) for phase, value in update["phases"].items()}


class RenderingUpdatesTest(SysprofTestCase):
    def test_the_phases_add_up_to_the_latency(self):
        model, (update,) = updates_of(
            asking(0, 10, 5)
            + composition(5.1, 6, (RU,))
            + [frame_complete(15, "InProgress")]
        )
        self.assertEqual(update["outcome"], "composited")
        self.assertEqual(update["pairing"], {"status": "exact", "reasons": []})
        # The frame lands before the update ends, which is no reason not to pair.
        self.assertEqual(
            phases(update),
            {
                "update": 5,
                "tile_wait": 0,
                "behind_previous_frame": 0,
                "scheduled": 0.1,
                "other": 0,
                "composition": 0.45,
            },
        )
        frame = model.compositions[update["frame"]]["frame_nsec"]
        self.assertEqual(
            sum(update["phases"].values()),
            approx((frame - update["begin_nsec"]) / 1e6),
        )

    def test_a_frame_whose_composition_began_after_the_next_update_is_an_anomaly(self):
        _, updates = updates_of(
            asking(0, 10, 9)
            + [rendering_update(15, 16)]
            + composition(20, 21, (RU,))
            + [frame_complete(30, "InProgress")]
        )
        self.assertEqual(
            updates[0]["pairing"],
            {"status": "anomaly", "reasons": ["invariant_breach"]},
        )
        # The next update may begin just before the frame, as DidComposite is
        # dispatched at TC:626-627, before DidRenderFrame at :629.
        _, updates = updates_of(
            asking(0, 10, 9)
            + composition(9.1, 12, (RU,), frame_at=11.9)
            + [rendering_update(11.8, 11.85), frame_complete(20, "InProgress")]
        )
        self.assertEqual(updates[0]["pairing"]["status"], "exact")

    def test_compositions_during_a_tile_wait_are_deferred_for_tiles(self):
        _, (update,) = updates_of(
            asking(0, 10, 9, waiting=True)
            + [scheduled(12, "Scheduled", waiting=True, reasons=(RU, ANIMATION))]
            + composition(12.1, 13, (ANIMATION,))
            + [scheduled(12.5, "InProgress", waiting=True, reasons=(RU, ANIMATION))]
            + [
                frame_complete(
                    20,
                    "ScheduledWhileInProgress",
                    waiting=True,
                    reasons=(RU, ANIMATION),
                )
            ]
            + composition(20.1, 21, (ANIMATION,))
            + [frame_complete(30, "InProgress", waiting=True, reasons=(RU,))]
            + [scheduled(35, "Scheduled", reasons=(RU,))]
            + composition(35.1, 36, (RU,))
            + [frame_complete(45, "InProgress")]
        )
        self.assertEqual(update["pairing"]["status"], "exact")
        self.assertEqual(update["deferred_for_tiles"], 2)
        # The compositor was scheduled and compositing meanwhile, but the update
        # waited for its tiles all along.
        self.assertEqual(
            phases(update),
            {
                "update": 9,
                "tile_wait": 26,
                "behind_previous_frame": 0,
                "scheduled": 0.1,
                "other": 0,
                "composition": 0.45,
            },
        )

    def test_an_update_arriving_mid_frame_waits_behind_it(self):
        _, (update,) = updates_of(
            [scheduled(99.5, "Idle", reasons=(ANIMATION,))]
            + composition(100, 101, (ANIMATION,))
            + [scheduled(100.3, "InProgress", reasons=(ANIMATION,))]
            + [rendering_update(95, 100.45)]
            + [
                scheduled(100.4, "ScheduledWhileInProgress", reasons=(RU, ANIMATION)),
                requested(100.41),
                frame_complete(
                    115.5, "ScheduledWhileInProgress", reasons=(RU, ANIMATION)
                ),
            ]
            + composition(116, 117, (RU, ANIMATION))
            + [frame_complete(131.5, "InProgress")]
        )
        self.assertEqual(update["pairing"]["status"], "exact")
        self.assertEqual(
            phases(update),
            {
                "update": 5.4,
                "tile_wait": 0,
                "behind_previous_frame": 15.1,
                "scheduled": 0.5,
                "other": 0,
                "composition": 0.5,
            },
        )

    def test_a_composition_that_should_have_taken_the_update_is_an_anomaly(self):
        _, (update,) = updates_of(
            asking(0, 10, 9)
            + composition(9.1, 9.5, (ANIMATION,))
            + [frame_complete(15, "InProgress"), scheduled(19, "Idle", reasons=(RU,))]
            + composition(20, 21, (RU,))
            + [frame_complete(30, "InProgress")]
        )
        self.assertEqual(
            update["pairing"],
            {"status": "anomaly", "reasons": ["unexpected_composition"]},
        )

    def test_a_composition_that_began_before_the_request_took_it_under_the_lock(self):
        marks = [scheduled(5, "Idle", reasons=(ANIMATION,))]
        marks += composition(9, 10, (RU, ANIMATION), frame_at=9.6)
        marks += [
            rendering_update(0, 9.3),
            requested(9.21),
            frame_complete(20, "InProgress"),
        ]
        _, (update,) = updates_of(
            marks + [scheduled(9.2, "Scheduled", timer=True, reasons=(RU, ANIMATION))]
        )
        self.assertEqual(update["pairing"], {"status": "exact", "reasons": []})
        self.assertTrue(update["rlt_before_request"])
        # Clamped, so the phases still add up to the latency.
        self.assertEqual(update["phases"]["update"], approx(9.2))
        self.assertEqual(update["phases"]["composition"], approx(0.4))
        self.assertEqual(sum(update["phases"].values()), approx(9.6))

        # The lock only blocks a composition the timer started, which the request
        # SC reports as Scheduled.
        _, (update,) = updates_of(
            marks + [scheduled(9.2, "InProgress", reasons=(RU, ANIMATION))]
        )
        self.assertEqual(
            update["pairing"], {"status": "anomaly", "reasons": ["rlt_before_request"]}
        )

    def test_an_update_that_did_not_ask_gets_no_frame(self):
        # The capture holds Request marks, so it is no old capture.
        _, (update, _) = updates_of(
            [rendering_update(0, 10)] + composition(12, 13, (RU,)) + asking(50, 60, 59)
        )
        self.assertFalse(update["asked"])
        self.assertIsNone(update["frame"])
        self.assertIsNone(update["outcome"])

    def test_an_update_whose_composition_aborted_was_never_composited(self):
        _, updates = updates_of(
            asking(0, 10, 9)
            + [mark("RenderLayerTree", 9.1, 9.5)]
            + asking(20, 30, 29)
            + composition(29.1, 30, (RU,))
        )
        self.assertEqual(
            [u["outcome"] for u in updates], ["uncomposited", "composited"]
        )
        _, (update,) = updates_of(asking(0, 10, 9))
        self.assertEqual(update["outcome"], "pending")
        # A later update, asking or not, shows the capture went on.
        _, (update, _) = updates_of(asking(0, 10, 9) + [rendering_update(50, 60)])
        self.assertEqual(update["outcome"], "uncomposited")

    def test_an_old_capture_pairs_every_update_with_the_next_composition(self):
        _, (update,) = updates_of([rendering_update(0, 10)] + composition(12, 13, None))
        self.assertTrue(update["asked"])
        self.assertEqual(update["request_nsec"], msec_to_nsec(10))
        self.assertEqual(
            update["pairing"], {"status": "approximate", "reasons": ["old_capture"]}
        )
        # State messages are newer than Request marks, so even with nothing
        # composited the capture is no old one.
        _, (update,) = updates_of(
            [rendering_update(0, 10), scheduled(9, "Idle", reasons=(ANIMATION,))]
            + [mark("RenderLayerTree", 9.1, 9.5)]
        )
        self.assertFalse(update["asked"])

    def test_updates_pair_with_compositions_of_their_own_process(self):
        model, updates = updates_of(
            asking(0, 10, 9, pid=1)
            + asking(0, 10, 9.5, pid=2)
            + composition(9.6, 10.5, (RU,), pid=2)
            + composition(9.1, 11, (RU,), pid=1)
        )
        for update in updates:
            self.assertEqual(model.compositions[update["frame"]]["pid"], update["pid"])

    def test_a_frame_past_the_window_is_still_found(self):
        data = sysprof_data(
            asking(0, 10, 9) + composition(9.1, 30, (RU,), frame_at=29), 0, 1000
        )
        model = calculate_compositions(data)
        report = analyze._prepare_rendering_update_report(
            data,
            model,
            calculate_rendering_updates(data, model),
            0,
            msec_to_nsec(20),
        )
        self.assertEqual(report["per_process"]["1"]["composited"], 1)

    def test_a_compositor_request_after_the_request_does_not_move_it(self):
        # A compositor-thread Animation request between the request SC and the
        # Request mark carries RenderingUpdate too.
        _, (update,) = updates_of(
            [rendering_update(0, 10), scheduled(9, "Idle", reasons=(RU,))]
            + [scheduled(9.005, "Scheduled", timer=True, reasons=(RU, ANIMATION))]
            + [requested(9.01)]
            + composition(9.1, 10, (RU, ANIMATION))
        )
        self.assertEqual(update["request_nsec"], msec_to_nsec(9))
