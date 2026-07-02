# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import collections
import unittest

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner


class MockRunner:
    def __init__(self, plan):
        self._plan = plan


class ConstructSubtestURLTest(unittest.TestCase):

    def _load_plan(self, name):
        _, plan = BenchmarkRunner._load_plan_data(name)
        return plan

    def _construct(self, plan, subtests):
        runner = MockRunner(plan)
        return BenchmarkRunner._construct_subtest_url(runner, subtests)

    # Edge cases

    def test_no_subtests_returns_empty(self):
        plan = self._load_plan('speedometer3.0')
        self.assertEqual(self._construct(plan, None), '')

    def test_empty_subtests_returns_empty(self):
        plan = self._load_plan('speedometer3.0')
        self.assertEqual(self._construct(plan, {}), '')

    def test_no_format_in_plan_returns_empty(self):
        plan = {'timeout': 600}
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs'])])
        self.assertEqual(self._construct(plan, subtests), '')

    # Speedometer-style: comma-separated suite names

    def test_speedometer_single_subtest(self):
        plan = self._load_plan('speedometer3.0')
        subtests = collections.OrderedDict([('TodoMVC-React', [''])])
        self.assertEqual(self._construct(plan, subtests), 'TodoMVC-React,')

    def test_speedometer_multiple_subtests(self):
        plan = self._load_plan('speedometer3.0')
        subtests = collections.OrderedDict([('TodoMVC-React', ['']), ('TodoMVC-Vue', [''])])
        self.assertEqual(self._construct(plan, subtests), 'TodoMVC-React,TodoMVC-Vue,')

    # MotionMark-style: URL parameters with suite-name and test-name

    def test_motionmark_single_subtest(self):
        plan = self._load_plan('motionmark1.3.1')
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&suite-name=MotionMark&test-name=CanvasArcs')

    def test_motionmark_multiple_subtests(self):
        plan = self._load_plan('motionmark1.3.1')
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs', 'Leaves'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&suite-name=MotionMark&test-name=CanvasArcs&suite-name=MotionMark&test-name=Leaves')

    # JetStream-style: leading & with test parameter

    def test_jetstream_single_subtest(self):
        plan = self._load_plan('jetstream3')
        subtests = collections.OrderedDict([('default', ['HashSet-String'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&test=HashSet-String')

    def test_jetstream_multiple_subtests(self):
        plan = self._load_plan('jetstream3')
        subtests = collections.OrderedDict([('default', ['HashSet-String', 'bomb-workers'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&test=HashSet-String&test=bomb-workers')
