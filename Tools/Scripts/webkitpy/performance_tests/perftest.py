#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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


class PerfTest(object):
    def __init__(self, port, test_name, path_or_url):
        self._port = port
        self._test_name = test_name
        self._path_or_url = path_or_url

    def test_name(self):
        return self._test_name

    def path_or_url(self):
        return self._path_or_url

    def prepare(self, time_out_ms):
        return True

    def run(self, driver, time_out_ms):
        output = self.run_single(driver, self.path_or_url(), time_out_ms)
        if self.run_failed(output):
            return None
        return self.parse_output(output)

    def run_single(self, driver, path_or_url, time_out_ms, should_run_pixel_test=False):
        return driver.run_test(DriverInput(path_or_url, time_out_ms, image_hash=None, should_run_pixel_test=should_run_pixel_test))

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
        re.compile(re.escape("""Blocked access to external URL http://www.whatwg.org/specs/web-apps/current-work/"""))]

    def _should_ignore_line_in_parser_test_result(self, line):
        if not line:
            return True
        for regex in self._lines_to_ignore_in_parser_result:
            if regex.search(line):
                return True
        return False

    _description_regex = re.compile(r'^Description: (?P<description>.*)$', re.IGNORECASE)
    _result_classes = ['Time', 'JS Heap', 'Malloc']
    _result_class_regex = re.compile(r'^(?P<resultclass>' + r'|'.join(_result_classes) + '):')
    _statistics_keys = ['avg', 'median', 'stdev', 'min', 'max', 'unit']
    _score_regex = re.compile(r'^(?P<key>' + r'|'.join(_statistics_keys) + r')\s+(?P<value>[0-9\.]+)\s*(?P<unit>.*)')

    def parse_output(self, output):
        test_failed = False
        results = {}
        ordered_results_keys = []
        test_name = re.sub(r'\.\w+$', '', self._test_name)
        description_string = ""
        result_class = ""
        for line in re.split('\n', output.text):
            description = self._description_regex.match(line)
            if description:
                description_string = description.group('description')
                continue

            result_class_match = self._result_class_regex.match(line)
            if result_class_match:
                result_class = result_class_match.group('resultclass')
                continue

            score = self._score_regex.match(line)
            if score:
                key = score.group('key')
                value = float(score.group('value'))
                unit = score.group('unit')
                name = test_name
                if result_class != 'Time':
                    name += ':' + result_class.replace(' ', '')
                if name not in ordered_results_keys:
                    ordered_results_keys.append(name)
                results.setdefault(name, {})
                results[name]['unit'] = unit
                results[name][key] = value
                continue

            if not self._should_ignore_line_in_parser_test_result(line):
                test_failed = True
                _log.error(line)

        if test_failed or set(self._statistics_keys) != set(results[test_name].keys()):
            return None

        for result_name in ordered_results_keys:
            if result_name == test_name:
                self.output_statistics(result_name, results[result_name], description_string)
            else:
                self.output_statistics(result_name, results[result_name])
        return results

    def output_statistics(self, test_name, results, description_string=None):
        unit = results['unit']
        if description_string:
            _log.info('DESCRIPTION: %s' % description_string)
        _log.info('RESULT %s= %s %s' % (test_name.replace(':', ': ').replace('/', ': '), results['avg'], unit))
        _log.info(', '.join(['%s= %s %s' % (key, results[key], unit) for key in self._statistics_keys[1:5]]))


class ChromiumStylePerfTest(PerfTest):
    _chromium_style_result_regex = re.compile(r'^RESULT\s+(?P<name>[^=]+)\s*=\s+(?P<value>\d+(\.\d+)?)\s*(?P<unit>\w+)$')

    def __init__(self, port, test_name, path_or_url):
        super(ChromiumStylePerfTest, self).__init__(port, test_name, path_or_url)

    def parse_output(self, output):
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


class PageLoadingPerfTest(PerfTest):
    def __init__(self, port, test_name, path_or_url):
        super(PageLoadingPerfTest, self).__init__(port, test_name, path_or_url)

    def run(self, driver, time_out_ms):
        test_times = []

        for i in range(0, 20):
            output = self.run_single(driver, self.path_or_url(), time_out_ms)
            if not output or self.run_failed(output):
                return None
            if i == 0:
                continue
            test_times.append(output.test_time * 1000)

        test_times = sorted(test_times)

        # Compute the mean and variance using a numerically stable algorithm.
        squareSum = 0
        mean = 0
        valueSum = sum(test_times)
        for i, time in enumerate(test_times):
            delta = time - mean
            sweep = i + 1.0
            mean += delta / sweep
            squareSum += delta * delta * (i / sweep)

        middle = int(len(test_times) / 2)
        results = {'avg': mean,
            'min': min(test_times),
            'max': max(test_times),
            'median': test_times[middle] if len(test_times) % 2 else (test_times[middle - 1] + test_times[middle]) / 2,
            'stdev': math.sqrt(squareSum),
            'unit': 'ms'}
        self.output_statistics(self.test_name(), results, '')
        return {self.test_name(): results}


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


class ReplayPerfTest(PageLoadingPerfTest):
    def __init__(self, port, test_name, path_or_url):
        super(ReplayPerfTest, self).__init__(port, test_name, path_or_url)

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
        path_without_ext = filesystem.splitext(self.path_or_url())[0]

        self._archive_path = filesystem.join(path_without_ext + '.wpr')
        self._expected_image_path = filesystem.join(path_without_ext + '-expected.png')
        self._url = filesystem.read_text_file(self.path_or_url()).split('\n')[0]

        if filesystem.isfile(self._archive_path) and filesystem.isfile(self._expected_image_path):
            _log.info("Replay ready for %s" % self._archive_path)
            return True

        _log.info("Preparing replay for %s" % self.test_name())

        driver = self._port.create_driver(worker_number=1, no_timeout=True)
        try:
            output = self.run_single(driver, self._archive_path, time_out_ms, record=True)
        finally:
            driver.stop()

        if not output or not filesystem.isfile(self._archive_path):
            _log.error("Failed to prepare a replay for %s" % self.test_name())
            return False

        _log.info("Prepared replay for %s" % self.test_name())

        return True

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

            super(ReplayPerfTest, self).run_single(driver, "about:blank", time_out_ms)
            _log.debug("Loading the page")

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
        (re.compile(r'^PageLoad/'), PageLoadingPerfTest),
        (re.compile(r'(.+)\.replay$'), ReplayPerfTest),
    ]

    @classmethod
    def create_perf_test(cls, port, test_name, path):
        for (pattern, test_class) in cls._pattern_map:
            if pattern.match(test_name):
                return test_class(port, test_name, path)
        return PerfTest(port, test_name, path)
