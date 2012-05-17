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


import logging
import math
import re

from webkitpy.layout_tests.port.driver import DriverInput


_log = logging.getLogger(__name__)


class PerfTest(object):
    def __init__(self, test_name, path_or_url):
        self._test_name = test_name
        self._path_or_url = path_or_url

    def test_name(self):
        return self._test_name

    def path_or_url(self):
        return self._path_or_url

    def run(self, driver, timeout_ms):
        output = driver.run_test(DriverInput(self.path_or_url(), timeout_ms, None, False))
        if self.run_failed(output):
            return None
        return self.parse_output(output)

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
        re.compile(r'^\d+(.\d+)?(\s*(runs\/s|ms))?$'),
        # Following are for handle existing test like Dromaeo
        re.compile(re.escape("""main frame - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->-->" - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->/<!--frame0-->-->" - has 1 onunload handler(s)"""))]

    _statistics_keys = ['avg', 'median', 'stdev', 'min', 'max']

    def _should_ignore_line_in_parser_test_result(self, line):
        if not line:
            return True
        for regex in self._lines_to_ignore_in_parser_result:
            if regex.search(line):
                return True
        return False

    def parse_output(self, output):
        got_a_result = False
        test_failed = False
        results = {}
        score_regex = re.compile(r'^(?P<key>' + r'|'.join(self._statistics_keys) + r')\s+(?P<value>[0-9\.]+)\s*(?P<unit>.*)')
        description_regex = re.compile(r'^Description: (?P<description>.*)$', re.IGNORECASE)
        description_string = ""
        unit = "ms"

        for line in re.split('\n', output.text):
            description = description_regex.match(line)
            if description:
                description_string = description.group('description')
                continue

            score = score_regex.match(line)
            if score:
                results[score.group('key')] = float(score.group('value'))
                if score.group('unit'):
                    unit = score.group('unit')
                continue

            if not self._should_ignore_line_in_parser_test_result(line):
                test_failed = True
                _log.error(line)

        if test_failed or set(self._statistics_keys) != set(results.keys()):
            return None

        results['description'] = description_string
        results['unit'] = unit

        test_name = re.sub(r'\.\w+$', '', self._test_name)
        self.output_statistics(test_name, results)

        return {test_name: results}

    def output_statistics(self, test_name, results):
        unit = results['unit']
        if results['description']:
            _log.info('DESCRIPTION: %s' % results['description'])
        _log.info('RESULT %s= %s %s' % (test_name.replace('/', ': '), results['avg'], unit))
        _log.info(', '.join(['%s= %s %s' % (key, results[key], unit) for key in self._statistics_keys[1:]]))


class ChromiumStylePerfTest(PerfTest):
    _chromium_style_result_regex = re.compile(r'^RESULT\s+(?P<name>[^=]+)\s*=\s+(?P<value>\d+(\.\d+)?)\s*(?P<unit>\w+)$')

    def __init__(self, test_name, path_or_url):
        super(ChromiumStylePerfTest, self).__init__(test_name, path_or_url)

    def parse_output(self, output):
        test_failed = False
        got_a_result = False
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
        results['description'] = ''
        return results if results and not test_failed else None


class PageLoadingPerfTest(PerfTest):
    def __init__(self, test_name, path_or_url):
        super(PageLoadingPerfTest, self).__init__(test_name, path_or_url)

    def run(self, driver, timeout_ms):
        test_times = []

        for i in range(0, 20):
            output = driver.run_test(DriverInput(self.path_or_url(), timeout_ms, None, False))
            if self.run_failed(output):
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
            'description': '',
            'unit': 'ms'}
        self.output_statistics(self.test_name(), results)
        return {self.test_name(): results}


class PerfTestFactory(object):

    _pattern_map = [
        (re.compile('^inspector/'), ChromiumStylePerfTest),
        (re.compile('^PageLoad/'), PageLoadingPerfTest),
    ]

    @classmethod
    def create_perf_test(cls, test_name, path):
        for (pattern, test_class) in cls._pattern_map:
            if pattern.match(test_name):
                return test_class(test_name, path)
        return PerfTest(test_name, path)
