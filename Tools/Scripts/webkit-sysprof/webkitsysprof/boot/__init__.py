"""Boot milestones and phases of a WPE application, from WebKit's sysprof marks.

Each engine milestone is one specific mark, or a fixed rule over marks: no
thresholds and no guessing when boot has settled. The phases are the intervals
between consecutive milestones found in the capture, so they add up to the whole
boot. See the `analyze-boot` subcommand.
"""

import argparse
import collections
import json
import re
import sys
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

from ..parser import parse
from ..utils import UsageError, mark_begin, mark_pid, nsec_to_msec

UI_GROUP = "WebKit (UI)"
WEB_GROUP = "WebKit (Web)"

_GROUP_ROLE_RE = re.compile(r"^WebKit \((?P<role>.+)\)$")
# Process names, also as the kernel's comm cut to 15 characters, which sysprof may
# record instead of the command line.
_PROCESS_NAME_ROLES = {
    "WPEWebProcess": "Web",
    "WPENetworkProcess": "Network",
    "WPENetworkProce": "Network",
    "WPEGPUProcess": "GPU",
}

_LOADING_RE = re.compile(r"^Loading (?P<url>.*) \| isMainFrame: 1$")
_MODULE_SCRIPT_PREFIX = "executing module script"


@dataclass
class Milestone:
    key: str
    title: str
    time: Optional[int] = None
    source: str = ""
    # Why the milestone is missing, or what makes it doubtful.
    note: str = ""

    @property
    def found(self) -> bool:
        return self.time is not None


@dataclass
class UserMilestone:
    name: str
    message: str
    time: Optional[int] = None


@dataclass
class Phase:
    begin: Milestone
    end: Milestone

    @property
    def begin_time(self) -> int:
        assert self.begin.time is not None
        return self.begin.time

    @property
    def end_time(self) -> int:
        assert self.end.time is not None
        return self.end.time

    @property
    def duration(self) -> int:
        return self.end_time - self.begin_time

    @property
    def label(self) -> str:
        return f"{self.begin.key} → {self.end.key}"


@dataclass
class BootReport:
    capture_end: int
    t0: int
    t0_source: str
    ui_pid: Optional[int]
    web_pid: Optional[int]
    url: Optional[str]
    milestones: List[Milestone]
    phases: List[Phase]
    user_milestones: List[UserMilestone]
    # (pid, name, role) of the WebKit processes, by pid.
    processes: List[Tuple[int, str, str]] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


class BootAnalysisError(Exception):
    """The capture lacks what any boot analysis needs."""


def analyze_boot(args: argparse.Namespace) -> None:
    user_requests = parse_user_milestones(args.user_defined_milestones)
    data = parse(args.capture_file, marks=True, counters=False, processes=True)
    try:
        report = build_report(data, user_requests)
    except BootAnalysisError as error:
        sys.exit(f"webkit-sysprof analyze-boot: {error}")

    if args.format == "json":
        print(json.dumps(report_to_json(report)))
    else:
        print_report(report)


def parse_user_milestones(arg: Optional[str]) -> List[Tuple[str, str]]:
    """Parse "Mark:Message,Mark:Message" into (mark name, message) pairs.

    The message is everything after the first colon and may be empty. A pair given
    again is dropped, as it would report the same mark twice.
    """
    if not arg:
        return []
    requests: List[Tuple[str, str]] = []
    for entry in arg.split(","):
        name, colon, message = entry.partition(":")
        if not colon or not name:
            raise UsageError(
                f'user-defined milestone is no "MarkName:Message": {entry!r}'
            )
        if (name, message) not in requests:
            requests.append((name, message))
    return requests


def build_report(
    data: Dict[str, Any], user_requests: Sequence[Tuple[str, str]]
) -> BootReport:
    marks = sorted(data["marks"], key=lambda mark: (mark_begin(mark), mark["end_time"]))
    capture_end = data["document"]["timespan"][1]
    warnings: List[str] = []

    ui_marks = [mark for mark in marks if mark["group"] == UI_GROUP]
    web_marks = [mark for mark in marks if mark["group"] == WEB_GROUP]

    m = {
        key: Milestone(key, title)
        for key, title in [
            ("M0", "WebKit starts in the app"),
            ("M1", "Web process alive"),
            ("M2", "Web process initialized"),
            ("M3", "Navigation starts"),
            ("M4", "Main document loading"),
            ("M5", "HTML parsed"),
            ("M6", "DOM interactive"),
            ("M7", "First contentful paint"),
            ("M8", "First contentful paint on screen"),
            ("M9", "Module graph evaluation begins"),
            ("M10", "Module graph evaluation ends"),
            ("M11", "DOMContentLoaded"),
            ("M12", "Load event done"),
        ]
    }

    # M0 and t0.
    ui_jit = _first(ui_marks, lambda mark: mark["name"] == "InitJITPageReservation")
    ui_pid = mark_pid(ui_jit) if ui_jit else None
    if ui_jit:
        _set(m["M0"], mark_begin(ui_jit), ui_jit)
        t0, t0_source = mark_begin(ui_jit), "InitJITPageReservation (UI process)"
    else:
        m["M0"].note = "no InitJITPageReservation in the UI process"
        launch = _first(ui_marks, lambda mark: mark["name"] == "ProcessLaunch")
        if launch is None:
            raise BootAnalysisError(
                "no UI-process InitJITPageReservation nor ProcessLaunch mark to"
                " measure the boot from"
            )
        ui_pid = mark_pid(launch)
        t0, t0_source = mark_begin(launch), "first ProcessLaunch (UI process)"
    if ui_pid is not None:
        ui_marks = [mark for mark in ui_marks if mark_pid(mark) == ui_pid]

    # The main document and the boot web process.
    loading_marks = [
        (mark, match.group("url"))
        for mark in web_marks
        if mark["name"] == "NavigationAndPaintTiming"
        and (match := _LOADING_RE.match(mark["message"]))
    ]
    app_loadings = [(mark, url) for mark, url in loading_marks if url != "about:blank"]
    if not app_loadings:
        raise BootAnalysisError(
            "no NavigationAndPaintTiming 'Loading <url> | isMainFrame: 1' mark of a"
            " document other than about:blank"
        )
    loading, url = app_loadings[0]
    web_pid = mark_pid(loading)
    same_url = [mark for mark, other in app_loadings if other == url]
    if len(same_url) > 1:
        warnings.append(
            f"ambiguous: {len(same_url)} main-frame loads of {url}, at "
            + ", ".join(_ms(mark_begin(mark) - t0) for mark in same_url)
            + " ms. The first one is used"
        )
    boot_marks = [mark for mark in web_marks if mark_pid(mark) == web_pid]

    # NavigationAndPaintTiming carries no document identity. The main document's
    # milestones are those following its Loading mark, up to the next main-frame
    # Loading mark of the process.
    window_begin = mark_begin(loading)
    later_loadings = [
        mark_begin(mark)
        for mark, _url in loading_marks
        if mark_pid(mark) == web_pid and mark_begin(mark) > window_begin
    ]
    window_end = min(later_loadings, default=capture_end + 1)
    document_marks = [
        mark
        for mark in boot_marks
        if mark["name"] == "NavigationAndPaintTiming"
        and window_begin <= mark_begin(mark) < window_end
    ]
    by_message: Dict[str, List[Dict[str, Any]]] = collections.defaultdict(list)
    for mark in document_marks:
        by_message[mark["message"]].append(mark)
    for message in [
        "domInteractive",
        "domContentLoadedEventBegin",
        "domContentLoadedEventEnd",
        "loadEventBegin",
        "loadEventEnd",
    ]:
        if len(by_message[message]) > 1:
            warnings.append(
                f"ambiguous: {len(by_message[message])} {message} marks after the"
                " main document's Loading mark, probably from iframes. The first one"
                " is used"
            )

    def document_milestone(key: str, message: str, last: bool = False) -> None:
        found = by_message.get(message)
        if not found:
            m[key].note = f"no NavigationAndPaintTiming '{message}' of the document"
            return
        mark = found[-1] if last else found[0]
        _set(m[key], mark_begin(mark), mark, message)

    # M1, M2: the web process comes up.
    initialize = _first(boot_marks, lambda mark: mark["name"] == "ProcessInitialize")
    if initialize:
        _set(m["M1"], mark_begin(initialize), initialize)
    else:
        m["M1"].note = "no ProcessInitialize in the boot web process"
    initialized = _first(
        boot_marks, lambda mark: mark["name"] == "InitializeWebProcess"
    )
    if initialized:
        _set(m["M2"], initialized["end_time"], initialized, "end")
    else:
        m["M2"].note = "no InitializeWebProcess in the boot web process"

    # M3, M4: navigation.
    navigation = _first(
        boot_marks,
        lambda mark: mark["name"] == "MainResourceLoad"
        and (m["M2"].time is None or mark_begin(mark) >= m["M2"].time),
    )
    if navigation:
        _set(m["M3"], mark_begin(navigation), navigation, "begin")
    else:
        m["M3"].note = "no MainResourceLoad in the boot web process"
    _set(m["M4"], mark_begin(loading), loading, loading["message"])

    # M5-M8, M11, M12: document milestones.
    document_milestone("M6", "domInteractive")
    document_milestone("M7", "firstContentfulPaint")
    document_milestone("M11", "domContentLoadedEventEnd")
    document_milestone("M12", "loadEventEnd")

    if m["M6"].time is not None:
        dom_interactive = m["M6"].time
        parsed = [
            mark
            for mark in boot_marks
            if mark["name"] == "ParseHTML"
            and mark_begin(mark) >= window_begin
            and mark["end_time"] <= dom_interactive
        ]
        if parsed:
            last_parse = max(parsed, key=lambda mark: mark["end_time"])
            _set(m["M5"], last_parse["end_time"], last_parse, "end")
        else:
            m["M5"].note = "no ParseHTML between the Loading mark and domInteractive"
    else:
        m["M5"].note = "needs M6"

    _find_frame_on_screen(m["M8"], m["M7"], boot_marks, ui_marks, window_end)

    # M9, M10: from the first module script the document runs to the end of the
    # last one run before DOMContentLoaded.
    module_scripts = [
        mark
        for mark in boot_marks
        if mark["name"] == "ExecuteScriptElement"
        and mark["message"].startswith(_MODULE_SCRIPT_PREFIX)
        and window_begin <= mark_begin(mark) < window_end
    ]
    if module_scripts:
        first_script = module_scripts[0]
        _set(m["M9"], mark_begin(first_script), first_script, "begin")
        dom_content_loaded = m["M11"].time
        before_dcl = [
            mark
            for mark in module_scripts
            if dom_content_loaded is None or mark["end_time"] <= dom_content_loaded
        ]
        last_script = max(
            before_dcl or [first_script], key=lambda mark: mark["end_time"]
        )
        detail = "end"
        if len(module_scripts) > 1:
            detail += f" (of {len(module_scripts)} module scripts)"
        _set(m["M10"], last_script["end_time"], last_script, detail)
    else:
        for key in ("M9", "M10"):
            m[key].note = "no module script executed by the document"

    boot_end = m["M12"].time
    milestones = list(m.values())
    _check_order(m, warnings)
    if boot_end is None:
        warnings.append(
            "truncated: no loadEventEnd of the main document, the capture may end"
            " before the boot does"
        )

    bounding = sorted(
        (milestone for milestone in milestones if milestone.found),
        key=lambda milestone: milestone.time or 0,
    )
    phases = [Phase(begin, end) for begin, end in zip(bounding, bounding[1:])]

    user_milestones = [
        _find_user_milestone(marks, name, message, t0, capture_end, warnings)
        for name, message in user_requests
    ]
    # In time order, those not found last.
    user_milestones.sort(
        key=lambda milestone: (milestone.time is None, milestone.time or 0)
    )

    return BootReport(
        processes=_webkit_processes(marks, data.get("processes", {}), ui_pid, web_pid),
        capture_end=capture_end,
        t0=t0,
        t0_source=t0_source,
        ui_pid=ui_pid,
        web_pid=web_pid,
        url=url,
        milestones=milestones,
        phases=phases,
        user_milestones=user_milestones,
        warnings=warnings,
    )


def _process_name(cmdline: str) -> str:
    return cmdline.split(" ", 1)[0].rsplit("/", 1)[-1]


def _webkit_processes(
    marks: Sequence[Dict[str, Any]],
    processes: Dict[int, str],
    ui_pid: Optional[int],
    web_pid: int,
) -> List[Tuple[int, str, str]]:
    """The processes that emitted WebKit marks, named by their mark group, and
    those named like WPE's auxiliary processes, which may emit none (the network
    process does not).
    """
    roles: Dict[int, str] = {}
    for mark in marks:
        match = _GROUP_ROLE_RE.match(mark["group"])
        if match:
            roles.setdefault(mark_pid(mark), match.group("role"))
    for pid, cmdline in processes.items():
        role = _PROCESS_NAME_ROLES.get(_process_name(cmdline))
        if role:
            roles.setdefault(pid, role)

    rows = []
    for pid, role in sorted(roles.items()):
        if pid == web_pid:
            role += ", boot"
        elif pid == ui_pid and role != "UI":
            role += ", UI"
        rows.append((pid, _process_name(processes.get(pid, "")) or "?", role))
    return rows


def _first(
    marks: Sequence[Dict[str, Any]], predicate: Callable[[Dict[str, Any]], bool]
) -> Optional[Dict[str, Any]]:
    """The first mark, by begin time, matching the predicate."""
    return next((mark for mark in marks if predicate(mark)), None)


def _set(
    milestone: Milestone, time: int, mark: Dict[str, Any], detail: str = ""
) -> None:
    milestone.time = time
    milestone.source = f"{mark['name']} (pid {mark_pid(mark)})" + (
        f" {detail}" if detail else ""
    )


def _find_frame_on_screen(
    on_screen: Milestone,
    first_paint: Milestone,
    boot_marks: Sequence[Dict[str, Any]],
    ui_marks: Sequence[Dict[str, Any]],
    window_end: int,
) -> None:
    """The first frame presented after the first frame the document rendered
    after FCP, before the next main-frame navigation.
    """
    if first_paint.time is None:
        on_screen.note = "needs M7"
        return
    fcp = first_paint.time
    rendered = _first(
        boot_marks,
        lambda mark: mark["name"] == "DidRenderFrame"
        and fcp <= mark_begin(mark) < window_end,
    )
    if rendered is None:
        on_screen.note = "no DidRenderFrame of the document after its FCP"
        return
    rendered_at = mark_begin(rendered)
    presented = _first(
        ui_marks,
        lambda mark: mark["name"] == "WaylandFramePresented"
        and mark_begin(mark) >= rendered_at,
    )
    if presented is None:
        on_screen.note = "no WaylandFramePresented after the frame holding it"
        return
    _set(on_screen, mark_begin(presented), presented)


# Milestones that must come in this order when found.
_ORDERS = [
    ["M0", "M1", "M2", "M3", "M4", "M5", "M6", "M11", "M12"],
    ["M4", "M9", "M10", "M11"],
    ["M4", "M7", "M8"],
]


def _check_order(m: Dict[str, Milestone], warnings: List[str]) -> None:
    for order in _ORDERS:
        found = [m[key] for key in order if m[key].found]
        for before, after in zip(found, found[1:]):
            assert before.time is not None and after.time is not None
            if before.time > after.time:
                warnings.append(
                    f"inconsistent: {before.key} ({before.title}) comes after"
                    f" {after.key} ({after.title})"
                )


def _find_user_milestone(
    marks: Sequence[Dict[str, Any]],
    name: str,
    message: str,
    t0: int,
    capture_end: int,
    warnings: List[str],
) -> UserMilestone:
    milestone = UserMilestone(name, message)
    mark = _first(
        marks, lambda mark: mark["name"] == name and mark["message"] == message
    )
    if mark is None:
        return milestone
    milestone.time = mark_begin(mark)
    if milestone.time < t0:
        warnings.append(
            f"user-defined milestone {name}:{message} is before the 1st mark"
        )
    elif milestone.time > capture_end:
        warnings.append(
            f"user-defined milestone {name}:{message} is after the capture ends"
        )
    return milestone


def _ms(nsec: int) -> str:
    return f"{nsec_to_msec(nsec):.1f}"


def _table(
    headers: Sequence[str], rows: Sequence[Sequence[str]], right: Sequence[int]
) -> List[str]:
    """A markdown-like table, with the columns of `right` right-aligned."""
    widths = [
        (
            max(len(headers[column]), *(len(row[column]) for row in rows))
            if rows
            else len(headers[column])
        )
        for column in range(len(headers))
    ]

    def line(cells: Sequence[str]) -> str:
        aligned = [
            cell.rjust(width) if column in right else cell.ljust(width)
            for column, (cell, width) in enumerate(zip(cells, widths))
        ]
        return "  | " + " | ".join(aligned) + " |"

    return [line(headers), line(["-" * width for width in widths])] + [
        line(row) for row in rows
    ]


def _ordered_milestones(report: BootReport) -> List[Tuple[Milestone, Optional[int]]]:
    """The milestones in time order, each with the duration of the phase it ends.

    The previous milestone is the one before. A lone milestone found, bounding no
    phase, follows, then the missing ones.
    """
    ordered: List[Tuple[Milestone, Optional[int]]] = []
    if report.phases:
        ordered.append((report.phases[0].begin, None))
        ordered += [(phase.end, phase.duration) for phase in report.phases]
    listed = {milestone.key for milestone, _ in ordered}
    found = [m for m in report.milestones if m.found and m.key not in listed]
    ordered += [(m, None) for m in sorted(found, key=lambda m: m.time or 0)]
    ordered += [(m, None) for m in report.milestones if not m.found]
    return ordered


def _ordered_user_milestones(
    report: BootReport,
) -> List[Tuple[UserMilestone, Optional[int]]]:
    """The user-defined milestones, in time order already, each with the time from
    the previous one found.
    """
    ordered: List[Tuple[UserMilestone, Optional[int]]] = []
    previous: Optional[int] = None
    for milestone in report.user_milestones:
        if milestone.time is None:
            ordered.append((milestone, None))
            continue
        ordered.append(
            (milestone, None if previous is None else milestone.time - previous)
        )
        previous = milestone.time
    return ordered


def _milestone_rows(report: BootReport) -> List[List[str]]:
    t0 = report.t0
    total = (
        report.phases[-1].end_time - report.phases[0].begin_time if report.phases else 0
    )
    rows = []
    for milestone, previous in _ordered_milestones(report):
        if milestone.time is None:
            rows.append(
                [
                    milestone.key,
                    milestone.title,
                    "-",
                    "-",
                    "-",
                    "missing: " + milestone.note,
                ]
            )
            continue
        share = "-"
        if previous is not None and total:
            share = f"{100.0 * previous / total:.1f}%"
        rows.append(
            [
                milestone.key,
                milestone.title,
                _ms(milestone.time - t0),
                "-" if previous is None else _ms(previous),
                share,
                milestone.source,
            ]
        )
    return rows


def _msec_or_none(nsec: Optional[int]) -> Optional[float]:
    return None if nsec is None else nsec_to_msec(nsec)


def report_to_json(report: BootReport) -> Dict[str, Any]:
    """The report as printed, times in milliseconds, missing ones null.

    Milestones are keyed by their title, user-defined ones by "MARK:MESSAGE", both
    in time order.
    """
    t0 = report.t0
    return {
        "processes": [
            {"pid": pid, "name": name, "role": role}
            for pid, name, role in report.processes
        ],
        "milestones": {
            milestone.title: {
                "from_1st_mark": _msec_or_none(
                    None if milestone.time is None else milestone.time - t0
                ),
                "from_previous": _msec_or_none(previous),
            }
            for milestone, previous in _ordered_milestones(report)
        },
        "user_defined_milestones": {
            f"{milestone.name}:{milestone.message}": {
                "from_1st_mark": _msec_or_none(
                    None if milestone.time is None else milestone.time - t0
                ),
                "from_previous": _msec_or_none(previous),
            }
            for milestone, previous in _ordered_user_milestones(report)
        },
    }


def print_report(report: BootReport) -> None:
    t0 = report.t0
    print("Processes:")
    for line in _table(
        ["pid", "process", "role"],
        [[str(pid), name, role] for pid, name, role in report.processes],
        right=[0],
    ):
        print(line)
    print()

    print("Milestones:")
    for line in _table(
        [
            "#",
            "milestone",
            "from 1st mark [ms]",
            "from previous [ms]",
            "share",
            "source",
        ],
        _milestone_rows(report),
        right=[2, 3, 4],
    ):
        print(line)

    if report.user_milestones:
        print()
        print("User-defined milestones:")
        rows = []
        for user_milestone, previous in _ordered_user_milestones(report):
            name = f"{user_milestone.name}:{user_milestone.message}"
            if user_milestone.time is None:
                rows.append([name, "-", "-"])
                continue
            rows.append(
                [
                    name,
                    _ms(user_milestone.time - t0),
                    "-" if previous is None else _ms(previous),
                ]
            )
        for line in _table(
            ["mark:message", "from 1st mark [ms]", "from previous [ms]"],
            rows,
            right=[1, 2],
        ):
            print(line)

    if report.warnings:
        print()
        print("Warnings:")
        for warning in report.warnings:
            print(f"- {warning}")
