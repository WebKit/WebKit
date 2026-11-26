import argparse
import json
from unittest import mock

from .helpers import SysprofTestCase, instant, mark

from webkitsysprof import boot
from webkitsysprof.utils import UsageError, msec_to_nsec

UI, WEB = "WebKit (UI)", "WebKit (Web)"
UI_PID, WEB_PID = 10, 20
URL = "http://app.local/"


def ui(name, begin, end=None, message=""):
    return mark(name, begin, begin if end is None else end, UI, UI_PID, message)


def web(name, begin, end=None, message="", pid=WEB_PID):
    return mark(name, begin, begin if end is None else end, WEB, pid, message)


def timing(message, msec, pid=WEB_PID):
    return instant("NavigationAndPaintTiming", msec, message, pid=pid, group=WEB)


def loading(url, msec, pid=WEB_PID):
    return timing(f"Loading {url} | isMainFrame: 1", msec, pid=pid)


def boot_marks():
    """A boot shaped like a WPE app's, t0 at 100 ms into the capture."""
    return [
        ui("InitJITPageReservation", 100),
        ui("ProcessLaunch", 110, 150),
        ui("ProcessLaunch", 120, 151),
        web("ProcessInitialize", 200, 201),
        web("InitializeWebProcess", 202, 300),
        # The initial about:blank document comes first.
        loading("about:blank", 305),
        timing("domInteractive", 306),
        timing("domContentLoadedEventEnd", 307),
        timing("loadEventEnd", 308),
        web("MainResourceLoad", 310, 600),
        loading(URL, 320),
        web("ParseHTML", 321, 330),
        web("ParseHTML", 331, 335),
        timing("domInteractive", 336),
        timing("firstContentfulPaint", 400),
        web("DidRenderFrame", 405),
        ui("WaylandFramePresented", 404),
        ui("WaylandFramePresented", 420),
        web("ExecuteScriptElement", 500, 900, "executing module script"),
        timing("domContentLoadedEventEnd", 901),
        timing("loadEventEnd", 905),
    ]


def capture(marks, end_msec=2000, processes=None):
    return {
        "document": {"timespan": [0, msec_to_nsec(end_msec)]},
        "marks": marks,
        "processes": processes or {},
    }


def milestone_times(report):
    return {
        milestone.key: (
            None if milestone.time is None else milestone.time / 1_000_000 - 100
        )
        for milestone in report.milestones
    }


class BootReportTest(SysprofTestCase):
    def test_milestones_of_the_main_document_skip_about_blank(self):
        report = boot.build_report(capture(boot_marks()), [])

        self.assertEqual(report.web_pid, WEB_PID)
        self.assertEqual(report.url, URL)
        self.assertEqual(
            milestone_times(report),
            {
                "M0": 0,
                "M1": 100,
                "M2": 200,
                "M3": 210,
                "M4": 220,
                "M5": 235,
                "M6": 236,
                "M7": 300,
                "M8": 320,
                "M9": 400,
                "M10": 800,
                "M11": 801,
                "M12": 805,
            },
        )
        self.assertEqual(report.warnings, [])

    def test_phases_are_consecutive_milestones_in_time_order(self):
        report = boot.build_report(capture(boot_marks()), [])

        labels = [phase.label for phase in report.phases]
        self.assertEqual(labels[0], "M0 → M1")
        self.assertEqual(labels[-1], "M11 → M12")
        for before, after in zip(report.phases, report.phases[1:]):
            self.assertIs(before.end, after.begin)
            self.assertGreaterEqual(before.duration, 0)

    def test_without_ui_marks_there_is_no_t0(self):
        marks = [mark for mark in boot_marks() if mark["group"] != UI]

        with self.assertRaises(boot.BootAnalysisError):
            boot.build_report(capture(marks), [])

    def test_t0_falls_back_to_the_first_process_launch(self):
        marks = [
            mark for mark in boot_marks() if mark["name"] != "InitJITPageReservation"
        ]

        report = boot.build_report(capture(marks), [])

        self.assertEqual(report.t0, msec_to_nsec(110))
        self.assertIsNone(report.milestones[0].time)

    def test_a_capture_without_the_app_document_is_rejected(self):
        marks = [mark for mark in boot_marks() if URL not in mark["message"]]

        with self.assertRaises(boot.BootAnalysisError):
            boot.build_report(capture(marks), [])

    def test_iframe_milestones_are_flagged(self):
        marks = boot_marks() + [timing("domInteractive", 340)]

        report = boot.build_report(capture(marks), [])

        self.assertEqual(milestone_times(report)["M6"], 236)
        self.assertTrue(any("domInteractive" in w for w in report.warnings))

    def test_the_document_ends_at_the_next_main_frame_load(self):
        marks = [mark for mark in boot_marks() if mark["message"] != "loadEventEnd"] + [
            loading("http://elsewhere.local/", 902),
            timing("loadEventEnd", 903),
        ]

        report = boot.build_report(capture(marks), [])

        self.assertIsNone(milestone_times(report)["M12"])
        self.assertTrue(any(w.startswith("truncated") for w in report.warnings))

    def test_order_violations_are_flagged(self):
        marks = boot_marks() + [timing("domContentLoadedEventEnd", 850)]
        marks.remove(timing("domContentLoadedEventEnd", 901))

        report = boot.build_report(capture(marks), [])

        self.assertTrue(any(w.startswith("inconsistent") for w in report.warnings))

    def test_milestone_rows_carry_the_phase_each_one_ends(self):
        marks = [mark for mark in boot_marks() if mark["name"] != "ParseHTML"]
        report = boot.build_report(capture(marks), [])

        rows = boot._milestone_rows(report)

        keys = [row[0] for row in rows]
        self.assertEqual(keys[:3], ["M0", "M1", "M2"])
        # Missing milestones close the table.
        self.assertEqual(keys[-1], "M5")
        self.assertEqual(rows[-1][2:5], ["-", "-", "-"])
        self.assertEqual(rows[0][2:5], ["0.0", "-", "-"])
        # M1 at 100 ms, M2 at 200 ms of a boot of 805 ms.
        self.assertEqual(rows[2][2:5], ["200.0", "100.0", "12.4%"])
        shares = [float(row[4].rstrip("%")) for row in rows if row[4] != "-"]
        self.assertAlmostEqual(sum(shares), 100.0, delta=0.1)

    def test_module_evaluation_ends_with_the_last_module_script_before_dcl(self):
        marks = boot_marks() + [
            web("ExecuteScriptElement", 900.2, 900.6, "executing module script"),
            # An async module script running after DOMContentLoaded is no part of it.
            web("ExecuteScriptElement", 950, 960, "executing module script"),
        ]

        report = boot.build_report(capture(marks), [])

        times = milestone_times(report)
        self.assertEqual(times["M9"], 400)
        self.assertAlmostEqual(times["M10"], 800.6)
        self.assertIn("(of 3 module scripts)", report.milestones[10].source)

    def test_module_scripts_of_a_later_document_are_not_the_boot(self):
        marks = [
            mark for mark in boot_marks() if mark["name"] != "ExecuteScriptElement"
        ] + [
            loading("http://elsewhere.local/", 910),
            web("ExecuteScriptElement", 920, 930, "executing module script"),
        ]

        times = milestone_times(boot.build_report(capture(marks), []))

        self.assertIsNone(times["M9"])
        self.assertIsNone(times["M10"])

    def test_a_frame_of_a_later_document_is_no_fcp_on_screen(self):
        marks = [mark for mark in boot_marks() if mark["name"] != "DidRenderFrame"] + [
            loading("http://elsewhere.local/", 402),
            web("DidRenderFrame", 405),
        ]

        times = milestone_times(boot.build_report(capture(marks), []))

        self.assertEqual(times["M7"], 300)
        self.assertIsNone(times["M8"])

    def test_process_names_drop_any_directory(self):
        for cmdline in [
            "/usr/libexec/WPEWebProcess 7 9",
            "./WPEWebProcess",
            "bin/WPEWebProcess",
        ]:
            self.assertEqual(boot._process_name(cmdline), "WPEWebProcess")

    def test_webkit_processes_come_from_mark_groups_and_names(self):
        marks = boot_marks() + [web("InitJITPageReservation", 100, pid=21)]
        processes = {
            UI_PID: "/usr/bin/wpe-bare-app --headless",
            WEB_PID: "WPEWebProcess",
            # The network process emits no marks.
            30: "WPENetworkProce",
            40: "weston",
        }

        report = boot.build_report(capture(marks, processes=processes), [])

        self.assertEqual(
            report.processes,
            [
                (UI_PID, "wpe-bare-app", "UI"),
                (WEB_PID, "WPEWebProcess", "Web, boot"),
                (21, "?", "Web"),
                (30, "WPENetworkProce", "Network"),
            ],
        )


class UserMilestonesTest(SysprofTestCase):
    def test_the_first_mark_with_exactly_that_message_is_taken(self):
        marks = boot_marks() + [
            web("WebKitPerformanceMark", 700, message="app-ready-ish"),
            web("WebKitPerformanceMark", 750, message="app-ready"),
            web("WebKitPerformanceMark", 760, message="app-ready"),
            web("WebKitPerformance", 600, 650, message="init"),
        ]
        requests = boot.parse_user_milestones(
            "WebKitPerformanceMark:app-ready,WebKitPerformance:init,Missing:x"
        )

        report = boot.build_report(capture(marks), requests)

        # In time order, not as given, those not found last.
        init, ready, missing = report.user_milestones
        self.assertEqual(ready.time, msec_to_nsec(750))
        self.assertEqual(init.time, msec_to_nsec(600))
        self.assertIsNone(missing.time)

    def test_user_milestones_are_no_phases(self):
        marks = boot_marks() + [web("WebKitPerformanceMark", 750, message="ready")]
        requests = boot.parse_user_milestones("WebKitPerformanceMark:ready")

        with_user = boot.build_report(capture(marks), requests)
        without = boot.build_report(capture(marks), [])

        self.assertEqual(
            [phase.label for phase in with_user.phases],
            [phase.label for phase in without.phases],
        )

    def test_a_mark_past_the_capture_is_flagged(self):
        marks = boot_marks() + [web("WebKitPerformanceMark", 5000, message="late")]

        report = boot.build_report(
            capture(marks), boot.parse_user_milestones("WebKitPerformanceMark:late")
        )

        self.assertIn(
            "user-defined milestone WebKitPerformanceMark:late is after the capture"
            " ends",
            report.warnings,
        )

    def test_the_table_gives_the_time_from_the_previous_one_in_time(self):
        marks = boot_marks() + [
            web("WebKitPerformanceMark", 300, message="a"),
            web("WebKitPerformanceMark", 450, message="b"),
        ]
        args = argparse.Namespace(
            capture_file="boot.syscap",
            format="text",
            user_defined_milestones=(
                "WebKitPerformanceMark:b,Missing:x,WebKitPerformanceMark:a"
            ),
        )

        with mock.patch.object(boot, "parse", return_value=capture(marks)):
            boot.analyze_boot(args)

        table = self.stdout().split("User-defined milestones:\n", 1)[1].splitlines()
        cells = [[cell.strip() for cell in line.split("|")[1:-1]] for line in table]
        self.assertEqual(
            cells[0], ["mark:message", "from 1st mark [ms]", "from previous [ms]"]
        )
        # The 1st mark is at 100 ms. Rows go in time order, not as given, and a
        # milestone not found comes last.
        self.assertEqual(
            cells[2:5],
            [
                ["WebKitPerformanceMark:a", "200.0", "-"],
                ["WebKitPerformanceMark:b", "350.0", "150.0"],
                ["Missing:x", "-", "-"],
            ],
        )

    def test_parsing(self):
        self.assertEqual(boot.parse_user_milestones(None), [])
        self.assertEqual(
            boot.parse_user_milestones("A:x: y,B:"), [("A", "x: y"), ("B", "")]
        )
        self.assertEqual(
            boot.parse_user_milestones("A:x,B:y,A:x"), [("A", "x"), ("B", "y")]
        )
        for wrong in ["A", ":x", "A:x,,B:y"]:
            with self.assertRaises(UsageError):
                boot.parse_user_milestones(wrong)


class AnalyzeBootCommandTest(SysprofTestCase):
    def args(self, **kwargs):
        defaults = {
            "capture_file": "boot.syscap",
            "format": "text",
            "user_defined_milestones": None,
        }
        defaults.update(kwargs)
        return argparse.Namespace(**defaults)

    def test_json_holds_the_three_sections(self):
        marks = [mark for mark in boot_marks() if mark["name"] != "ParseHTML"]
        marks.append(web("WebKitPerformanceMark", 450, message="ready"))
        data = capture(marks, processes={WEB_PID: "WPEWebProcess"})
        with mock.patch.object(boot, "parse", return_value=data):
            boot.analyze_boot(
                self.args(
                    format="json",
                    user_defined_milestones="WebKitPerformanceMark:ready,Missing:x",
                )
            )

        report = json.loads(self.stdout())
        self.assertEqual(
            list(report), ["processes", "milestones", "user_defined_milestones"]
        )
        self.assertIn(
            {"pid": WEB_PID, "name": "WPEWebProcess", "role": "Web, boot"},
            report["processes"],
        )
        milestones = report["milestones"]
        # Keyed by title in time order, the missing one last.
        self.assertEqual(
            list(milestones)[:3],
            [
                "WebKit starts in the app",
                "Web process alive",
                "Web process initialized",
            ],
        )
        self.assertEqual(list(milestones)[-1], "HTML parsed")
        self.assertEqual(
            milestones["WebKit starts in the app"],
            {"from_1st_mark": 0.0, "from_previous": None},
        )
        self.assertEqual(
            milestones["Web process initialized"],
            {"from_1st_mark": 200.0, "from_previous": 100.0},
        )
        self.assertEqual(
            milestones["HTML parsed"], {"from_1st_mark": None, "from_previous": None}
        )
        self.assertEqual(
            report["user_defined_milestones"],
            {
                "WebKitPerformanceMark:ready": {
                    "from_1st_mark": 350.0,
                    "from_previous": None,
                },
                "Missing:x": {"from_1st_mark": None, "from_previous": None},
            },
        )

    def test_json_and_text_give_the_same_times(self):
        report = boot.build_report(capture(boot_marks()), [])

        json_times = [
            entry["from_1st_mark"]
            for entry in boot.report_to_json(report)["milestones"].values()
        ]
        text_times = [float(row[2]) for row in boot._milestone_rows(report)]

        self.assertEqual(json_times, text_times)

    def test_only_marks_are_read(self):
        with mock.patch.object(
            boot, "parse", return_value=capture(boot_marks())
        ) as parse:
            boot.analyze_boot(
                self.args(
                    user_defined_milestones="ExecuteScriptElement:executing module script"
                )
            )

        parse.assert_called_once_with(
            "boot.syscap", marks=True, counters=False, processes=True
        )
        stdout = self.stdout()
        self.assertTrue(stdout.startswith("Processes:"))
        self.assertIn("Milestones:", stdout)
        self.assertNotIn("Phases:", stdout)
        self.assertIn("from 1st mark", stdout)
        self.assertNotIn("in capture", stdout)
        self.assertIn("User-defined milestones:", stdout)
