# Copyright (C) 2012 Google Inc. All rights reserved.
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

# Import for auto-install
if sys.platform not in ('cygwin', 'win32'):
    # FIXME: webpagereplay doesn't work on win32. See https://bugs.webkit.org/show_bug.cgi?id=88279.
    import webkitpy.thirdparty.autoinstalled.webpagereplay.replay

from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.port.driver import DriverInput
from webkitpy.layout_tests.port.driver import DriverOutput


_log = logging.getLogger(__name__)


class PerfTestMetric(object):
    def __init__(self, metric, unit=None, iterations=None):
        # FIXME: Fix runner.js to report correct metric names
        self._iterations = iterations or []
        self._unit = unit or self.metric_to_unit(metric)
        self._metric = self.time_unit_to_metric(self._unit) if metric == 'Time' else metric

    def metric(self):
        return self._metric

    def has_values(self):
        return bool(self._iterations)

    # FIXME: We don't need to support this anymore. Make outputs more human friendly.
    def legacy_chromium_bot_compatible_test_name(self, test_name_with_extension):
        test_name = re.sub(r'\.\w+$', '', test_name_with_extension)
        return test_name + ':' + self._metric

    def append(self, value):
        self._iterations.append(value)

    def iteration_values(self):
        return self._iterations

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
    def __init__(self, port, test_name, test_path):
        self._port = port
        self._test_name = test_name
        self._test_path = test_path
        self._description = None

    def test_name(self):
        return self._test_name

    def test_path(self):
        return self._test_path

    def description(self):
        return self._description

    def prepare(self, time_out_ms):
        return True

    def _create_driver(self):
        return self._port.create_driver(worker_number=0, no_timeout=True)

    def run(self, time_out_ms):
        driver = self._create_driver()
        try:
            metrics = self._run_with_driver(driver, time_out_ms)
        finally:
            driver.stop()

        if not metrics:
            return metrics

        should_log = not self._port.get_option('profile')
        if should_log and self._description:
            _log.info('DESCRIPTION: %s' % self._description)

        results = {}
        for metric in metrics:
            legacy_test_name = metric.legacy_chromium_bot_compatible_test_name(self.test_name())
            results[legacy_test_name] = metric.iteration_values()
            if should_log:
                self.log_statistics(legacy_test_name.replace(':', ': ').replace('/', ': '),
                    metric.iteration_values(), metric.unit())

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
    _metrics_regex = re.compile(r'^(?P<metric>Time|Malloc|JS Heap):')
    _statistics_keys = ['avg', 'median', 'stdev', 'min', 'max', 'unit', 'values']
    _score_regex = re.compile(r'^(?P<key>' + r'|'.join(_statistics_keys) + r')\s+(?P<value>([0-9\.]+(,\s+)?)+)\s*(?P<unit>.*)')

    def _run_with_driver(self, driver, time_out_ms):
        output = self.run_single(driver, self.test_path(), time_out_ms)
        self._filter_output(output)
        if self.run_failed(output):
            return None

        current_metric = None
        results = []
        for line in re.split('\n', output.text):
            if not line:
                continue

            description_match = self._description_regex.match(line)
            metric_match = self._metrics_regex.match(line)
            score = self._score_regex.match(line)

            if description_match:
                self._description = description_match.group('description')
            elif metric_match:
                current_metric = metric_match.group('metric').replace(' ', '')
            elif score:
                key = score.group('key')
                if key == 'values' and results != None:
                    values = [float(number) for number in score.group('value').split(', ')]
                    results.append(PerfTestMetric(current_metric, score.group('unit'), values))
            else:
                results = None
                _log.error('ERROR: ' + line)

        return results

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

    _lines_to_ignore_in_stderr = [
        re.compile(r'^Unknown option:'),
        re.compile(r'^\[WARNING:proxy_service.cc'),
        re.compile(r'^\[INFO:'),
    ]

    _lines_to_ignore_in_parser_result = [
        re.compile(r'^Running \d+ times$'),
        re.compile(r'^Ignoring warm-up '),
        re.compile(r'^Info:'),
        re.compile(r'^\d+(.\d+)?(\s*(runs\/s|ms|fps))?$'),
        # Following are for handle existing test like Dromaeo
        re.compile(re.escape("""main frame - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->-->" - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->/<!--frame0-->-->" - has 1 onunload handler(s)""")),
        # Following is for html5.html
        re.compile(re.escape("""Blocked access to external URL http://www.whatwg.org/specs/web-apps/current-work/""")),
        re.compile(r"CONSOLE MESSAGE: (line \d+: )?Blocked script execution in '[A-Za-z0-9\-\.:]+' because the document's frame is sandboxed and the 'allow-scripts' permission is not set."),
        # Dromaeo reports values for subtests. Ignore them for now.
        re.compile(r'(?P<name>.+): \[(?P<values>(\d+(.\d+)?,\s+)*\d+(.\d+)?)\]'),
    ]

    def _filter_output(self, output):
        if output.error:
            output.error = '\n'.join([line for line in re.split('\n', output.error) if not self._should_ignore_line(self._lines_to_ignore_in_stderr, line)])
        if output.text:
            output.text = '\n'.join([line for line in re.split('\n', output.text) if not self._should_ignore_line(self._lines_to_ignore_in_parser_result, line)])


class ChromiumStylePerfTest(PerfTest):
    _chromium_style_result_regex = re.compile(r'^RESULT\s+(?P<name>[^=]+)\s*=\s+(?P<value>\d+(\.\d+)?)\s*(?P<unit>\w+)$')

    def __init__(self, port, test_name, test_path):
        super(ChromiumStylePerfTest, self).__init__(port, test_name, test_path)

    def run(self, time_out_ms):
        driver = self._create_driver()
        try:
            output = self.run_single(driver, self.test_path(), time_out_ms)
        finally:
            driver.stop()

        self._filter_output(output)
        if self.run_failed(output):
            return None

        return self.parse_and_log_output(output)

    def parse_and_log_output(self, output):
        test_failed = False
        results = {}
        for line in re.split('\n', output.text):
            resultLine = ChromiumStylePerfTest._chromium_style_result_regex.match(line)
            if resultLine:
                # FIXME: Store the unit
                results[self.test_name() + ':' + resultLine.group('name').replace(' ', '')] = float(resultLine.group('value'))
                _log.info(line)
            elif not len(line) == 0:
                test_failed = True
                _log.error(line)
        return results if results and not test_failed else None


class ReplayServer(object):
    def __init__(self, archive, record):
        self._process = None

        # FIXME: Should error if local proxy isn't set to forward requests to localhost:8080 and localhost:8443

        replay_path = webkitpy.thirdparty.autoinstalled.webpagereplay.replay.__file__
        args = ['python', replay_path, '--no-dns_forwarding', '--port', '8080', '--ssl_port', '8443', '--use_closest_match', '--log_level', 'warning']
        if record:
            args.append('--record')
        args.append(archive)

        self._process = subprocess.Popen(args)

    def wait_until_ready(self):
        for i in range(0, 3):
            try:
                connection = socket.create_connection(('localhost', '8080'), timeout=1)
                connection.close()
                return True
            except socket.error:
                time.sleep(1)
                continue
        return False

    def stop(self):
        if self._process:
            self._process.send_signal(signal.SIGINT)
            self._process.wait()
        self._process = None

    def __del__(self):
        self.stop()


class ReplayPerfTest(PerfTest):
    _FORCE_GC_FILE = 'resources/force-gc.html'

    def __init__(self, port, test_name, test_path):
        super(ReplayPerfTest, self).__init__(port, test_name, test_path)
        self.force_gc_test = self._port.host.filesystem.join(self._port.perf_tests_dir(), self._FORCE_GC_FILE)

    def _start_replay_server(self, archive, record):
        try:
            return ReplayServer(archive, record)
        except OSError as error:
            if error.errno == errno.ENOENT:
                _log.error("Replay tests require web-page-replay.")
            else:
                raise error

    def prepare(self, time_out_ms):
        filesystem = self._port.host.filesystem
        path_without_ext = filesystem.splitext(self.test_path())[0]

        self._archive_path = filesystem.join(path_without_ext + '.wpr')
        self._expected_image_path = filesystem.join(path_without_ext + '-expected.png')
        self._url = filesystem.read_text_file(self.test_path()).split('\n')[0]

        if filesystem.isfile(self._archive_path) and filesystem.isfile(self._expected_image_path):
            _log.info("Replay ready for %s" % self._archive_path)
            return True

        _log.info("Preparing replay for %s" % self.test_name())

        driver = self._port.create_driver(worker_number=0, no_timeout=True)
        try:
            output = self.run_single(driver, self._archive_path, time_out_ms, record=True)
        finally:
            driver.stop()

        if not output or not filesystem.isfile(self._archive_path):
            _log.error("Failed to prepare a replay for %s" % self.test_name())
            return False

        _log.info("Prepared replay for %s" % self.test_name())

        return True

    def _run_with_driver(self, driver, time_out_ms):
        times = PerfTestMetric('Time')
        malloc = PerfTestMetric('Malloc')
        js_heap = PerfTestMetric('JSHeap')

        for i in range(0, 20):
            output = self.run_single(driver, self.test_path(), time_out_ms)
            if not output or self.run_failed(output):
                return None
            if i == 0:
                continue

            times.append(output.test_time * 1000)
            if not output.measurements:
                continue

            for metric, result in output.measurements.items():
                assert metric == 'Malloc' or metric == 'JSHeap'
                if metric == 'Malloc':
                    malloc.append(result)
                else:
                    js_heap.append(result)

        return filter(lambda metric: metric.has_values(), [times, malloc, js_heap])

    def run_single(self, driver, url, time_out_ms, record=False):
        server = self._start_replay_server(self._archive_path, record)
        if not server:
            _log.error("Web page replay didn't start.")
            return None

        try:
            _log.debug("Waiting for Web page replay to start.")
            if not server.wait_until_ready():
                _log.error("Web page replay didn't start.")
                return None

            _log.debug("Web page replay started. Loading the page.")
            # Force GC to prevent pageload noise. See https://bugs.webkit.org/show_bug.cgi?id=98203
            super(ReplayPerfTest, self).run_single(driver, self.force_gc_test, time_out_ms, False)
            output = super(ReplayPerfTest, self).run_single(driver, self._url, time_out_ms, should_run_pixel_test=True)
            if self.run_failed(output):
                return None

            if not output.image:
                _log.error("Loading the page did not generate image results")
                _log.error(output.text)
                return None

            filesystem = self._port.host.filesystem
            dirname = filesystem.dirname(self._archive_path)
            filename = filesystem.split(self._archive_path)[1]
            writer = TestResultWriter(filesystem, self._port, dirname, filename)
            if record:
                writer.write_image_files(actual_image=None, expected_image=output.image)
            else:
                writer.write_image_files(actual_image=output.image, expected_image=None)

            return output
        finally:
            server.stop()


class PerfTestFactory(object):

    _pattern_map = [
        (re.compile(r'^inspector/'), ChromiumStylePerfTest),
        (re.compile(r'(.+)\.replay$'), ReplayPerfTest),
    ]

    @classmethod
    def create_perf_test(cls, port, test_name, path):
        for (pattern, test_class) in cls._pattern_map:
            if pattern.match(test_name):
                return test_class(port, test_name, path)
        return PerfTest(port, test_name, path)
