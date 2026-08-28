"""What the tests build captures from, and read what a command printed with.

A synthetic mark is shaped like what the parser produces: it carries no begin
time, so everything derives one from `end_time` and `duration`, and it names the
process that emitted it by pid as well as by kind.
"""

import builtins
import unittest

from webkitcorepy import OutputCapture

from webkitsysprof.utils import msec_to_nsec


def mark(name, begin_msec, end_msec, group="WebKit (Web)", pid=1):
    return {
        "group": group,
        "pid": pid,
        "name": name,
        "message": "",
        "duration": msec_to_nsec(end_msec - begin_msec),
        "end_time": msec_to_nsec(end_msec),
    }


def sysprof_data(marks, begin_msec=0, end_msec=1000):
    marks_by_name = {}
    for a_mark in marks:
        marks_by_name.setdefault(a_mark["name"], []).append(a_mark)
    return {
        "document": {
            "timespan": {
                "begin": msec_to_nsec(begin_msec),
                "end": msec_to_nsec(end_msec),
            }
        },
        "marks": marks_by_name,
    }


class approx:
    """A number equal to another within a tolerance, relative unless `abs` says so.

    Durations are divided and summed before they are compared, so comparing them
    exactly would test the rounding of binary floating point rather than the
    analysis. Works inside a list or a dict too, since a float compared against one
    of these defers to it.
    """

    def __init__(self, value, rel=1e-6, abs=None):
        self.value = value
        self.tolerance = abs if abs is not None else rel * max(builtins.abs(value), 1.0)

    def __eq__(self, other):
        return builtins.abs(other - self.value) <= self.tolerance

    def __repr__(self):
        return f"~{self.value!r}"


class SysprofTestCase(unittest.TestCase):
    """A test reading back what the command under it printed.

    OutputCapture keeps stdout and the root logger to itself, so that a command
    configuring logging does not outlive the test that ran it.
    """

    def setUp(self):
        capture = OutputCapture()
        capture.__enter__()
        self.addCleanup(capture.__exit__, None, None, None)
        self._capture = capture

    def stdout(self):
        return self._capture.stdout.getvalue()
