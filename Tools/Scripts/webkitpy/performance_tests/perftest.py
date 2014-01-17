# Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
# Copyright (C) 2012, 2013 Google Inc. All rights reserved.
# Copyright (C) 2012 Zoltan Horvath, Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import errno
import logging
import math
import re
import os
import signal
import socket
import subprocess
import sys
import time

from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.port.driver import DriverInput
from webkitpy.port.driver import DriverOutput

DEFAULT_TEST_RUNNER_COUNT = 4

_log = logging.getLogger(__name__)


class PerfTestMetric(object):
    def __init__(self, path, test_file_name, metric, unit=None, aggregator=None, iterations=None):
        # FIXME: Fix runner.js to report correct metric names
        self._iterations = iterations or []
        self._unit = unit or self.metric_to_unit(metric)
        self._aggregator = aggregator
        self._metric = self.time_unit_to_metric(self._unit) if metric == 'Time' else metric
        self._path = path
        self._test_file_name = test_file_name

    def name(self):
        return self._metric

    def aggregator(self):
        return self._aggregator

    def path(self):
        return self._path

    def test_file_name(self):
        return self._test_file_name

    def has_values(self):
        return bool(self._iterations)

    def append_group(self, group_values):
        assert isinstance(group_values, list)
        self._iterations.append(group_values)

    def grouped_iteration_values(self):
        return self._iterations

    def flattened_iteration_values(self):
        return [value for group_values in self._iterations for value in group_values]

    def unit(self):
        return self._unit

    @staticmethod
    def metric_to_unit(metric):
        assert metric in ('Time', 'Malloc', 'JSHeap')
        return 'ms' if metric == 'Time' else 'bytes'

    @staticmethod
    def time_unit_to_metric(unit):
        return {'fps': 'FrameRate', 'runs/s': 'Runs', 'ms': 'Time'}[unit]


class PerfTest(object):

    def __init__(self, port, test_name, test_path, test_runner_count=DEFAULT_TEST_RUNNER_COUNT):
        self._port = port
        self._test_name = test_name
        self._test_path = test_path
        self._description = None
        self._metrics = []
        self._test_runner_count = test_runner_count

    def test_name(self):
        return self._test_name

    def test_name_without_file_extension(self):
        return re.sub(r'\.\w+$', '', self.test_name())

    def test_path(self):
        return self._test_path

    def description(self):
        return self._description

    def prepare(self, time_out_ms):
        return True

    def _create_driver(self):
        return self._port.create_driver(worker_number=0, no_timeout=True)

    def run(self, time_out_ms):
        for _ in xrange(self._test_runner_count):
            driver = self._create_driver()
            try:
                if not self._run_with_driver(driver, time_out_ms):
                    return None
            finally:
                driver.stop()

        should_log = not self._port.get_option('profile')
        if should_log and self._description:
            _log.info('DESCRIPTION: %s' % self._description)

        results = []
        for subtest in self._metrics:
            for metric in subtest['metrics']:
                results.append(metric)
                if should_log and not subtest['name']:
                    legacy_chromium_bot_compatible_name = self.test_name_without_file_extension().replace('/', ': ')
                    self.log_statistics(legacy_chromium_bot_compatible_name + ': ' + metric.name(),
                        metric.flattened_iteration_values(), metric.unit())

        return results

    @staticmethod
    def log_statistics(test_name, values, unit):
        sorted_values = sorted(values)

        # Compute the mean and variance using Knuth's online algorithm (has good numerical stability).
        square_sum = 0
        mean = 0
        for i, time in enumerate(sorted_values):
            delta = time - mean
            sweep = i + 1.0
            mean += delta / sweep
            square_sum += delta * (time - mean)

        middle = int(len(sorted_values) / 2)
        mean = sum(sorted_values) / len(values)
        median = sorted_values[middle] if len(sorted_values) % 2 else (sorted_values[middle - 1] + sorted_values[middle]) / 2
        stdev = math.sqrt(square_sum / (len(sorted_values) - 1)) if len(sorted_values) > 1 else 0

        _log.info('RESULT %s= %s %s' % (test_name, mean, unit))
        _log.info('median= %s %s, stdev= %s %s, min= %s %s, max= %s %s' %
            (median, unit, stdev, unit, sorted_values[0], unit, sorted_values[-1], unit))

    _description_regex = re.compile(r'^Description: (?P<description>.*)$', re.IGNORECASE)
    _metrics_regex = re.compile(r'^(?P<subtest>[A-Za-z0-9\(\[].+?)?:(?P<metric>[A-Z][A-Za-z]+)(:(?P<aggregator>[A-Z][A-Za-z]+))? -> \[(?P<values>(\d+(\.\d+)?)(, \d+(\.\d+)?)+)\] (?P<unit>[a-z/]+)?$')

    def _run_with_driver(self, driver, time_out_ms):
        output = self.run_single(driver, self.test_path(), time_out_ms)
        self._filter_output(output)
        if self.run_failed(output):
            return False

        current_metric = None
        for line in re.split('\n', output.text):
            description_match = self._description_regex.match(line)
            if description_match:
                self._description = description_match.group('description')
                continue

            metric_match = self._metrics_regex.match(line)
            if not metric_match:
                _log.error('ERROR: ' + line)
                return False

            metric = self._ensure_metrics(metric_match.group('metric'), metric_match.group('subtest'), metric_match.group('unit'), metric_match.group('aggregator'))
            metric.append_group(map(lambda value: float(value), metric_match.group('values').split(', ')))

        return True

    def _ensure_metrics(self, metric_name, subtest_name='', unit=None, aggregator=None):
        try:
            subtest = next(subtest for subtest in self._metrics if subtest['name'] == subtest_name)
        except StopIteration:
            subtest = {'name': subtest_name, 'metrics': []}
            self._metrics.append(subtest)

        try:
            return next(metric for metric in subtest['metrics'] if metric.name() == metric_name)
        except StopIteration:
            path = self.test_name_without_file_extension().split('/')
            if subtest_name:
                path += subtest_name.split('/')
            metric = PerfTestMetric(path, self._test_name, metric_name, unit, aggregator)
            subtest['metrics'].append(metric)
            return metric

    def run_single(self, driver, test_path, time_out_ms, should_run_pixel_test=False):
        return driver.run_test(DriverInput(test_path, time_out_ms, image_hash=None, should_run_pixel_test=should_run_pixel_test), stop_when_done=False)

    def run_failed(self, output):
        if output.text == None or output.error:
            pass
        elif output.timeout:
            _log.error('timeout: %s' % self.test_name())
        elif output.crash:
            _log.error('crash: %s' % self.test_name())
        else:
            return False

        if output.error:
            _log.error('error: %s\n%s' % (self.test_name(), output.error))

        return True

    @staticmethod
    def _should_ignore_line(regexps, line):
        if not line:
            return True
        for regexp in regexps:
            if regexp.search(line):
                return True
        return False

    _lines_to_ignore_in_parser_result = [
        # Following are for handle existing test like Dromaeo
        re.compile(re.escape("""main frame - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->-->" - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->/<!--frame0-->-->" - has 1 onunload handler(s)""")),
        # Following is for html5.html
        re.compile(re.escape("""Blocked access to external URL http://www.whatwg.org/specs/web-apps/current-work/""")),
        re.compile(r"CONSOLE MESSAGE: (line \d+: )?Blocked script execution in '[A-Za-z0-9\-\.:]+' because the document's frame is sandboxed and the 'allow-scripts' permission is not set."),
        re.compile(r"CONSOLE MESSAGE: (line \d+: )?Not allowed to load local resource"),
        # DoYouEvenBench
        re.compile(re.escape("CONSOLE MESSAGE: line 140: Miss the info bar? Run TodoMVC from a server to avoid a cross-origin error.")),
        re.compile(re.escape("CONSOLE MESSAGE: line 315: TypeError: Attempted to assign to readonly property.")),
        re.compile(re.escape("CONSOLE MESSAGE: line 339: DEBUG: -------------------------------")),
        re.compile(re.escape("CONSOLE MESSAGE: line 339: DEBUG: Ember.VERSION : 1.0.0-rc.1")),
        re.compile(re.escape("CONSOLE MESSAGE: line 339: DEBUG: Handlebars.VERSION : 1.0.0-rc.3")),
        re.compile(re.escape("CONSOLE MESSAGE: line 339: DEBUG: jQuery.VERSION : 1.9.1")),
        re.compile(re.escape("CONSOLE MESSAGE: line 339: DEPRECATION: Namespaces should not begin with lowercase")),
        re.compile(re.escape("processAllNamespaces@ember.js:4359:36")),
        re.compile(re.escape("CONSOLE MESSAGE: line 124: Booting in DEBUG mode")),
        re.compile(re.escape("CONSOLE MESSAGE: line 125: You can configure event logging with DEBUG.events.logAll()/logNone()/logByName()/logByAction()")),
    ]

    def _filter_output(self, output):
        if output.text:
            output.text = '\n'.join([line for line in re.split('\n', output.text) if not self._should_ignore_line(self._lines_to_ignore_in_parser_result, line)])


class SingleProcessPerfTest(PerfTest):
    def __init__(self, port, test_name, test_path, test_runner_count=1):
        super(SingleProcessPerfTest, self).__init__(port, test_name, test_path, test_runner_count)


class PerfTestFactory(object):

    _pattern_map = [
        (re.compile(r'^Dromaeo/'), SingleProcessPerfTest),
    ]

    @classmethod
    def create_perf_test(cls, port, test_name, path, test_runner_count=DEFAULT_TEST_RUNNER_COUNT):
        for (pattern, test_class) in cls._pattern_map:
            if pattern.match(test_name):
                return test_class(port, test_name, path, test_runner_count)
        return PerfTest(port, test_name, path, test_runner_count)
