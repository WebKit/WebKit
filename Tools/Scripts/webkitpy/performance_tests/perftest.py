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


import re


class PerfTest(object):
    def __init__(self, test_name, dirname, path_or_url):
        self._test_name = test_name
        self._dirname = dirname
        self._path_or_url = path_or_url

    def test_name(self):
        return self._test_name

    def dirname(self):
        return self._dirname

    def path_or_url(self):
        return self._path_or_url

    _lines_to_ignore_in_parser_result = [
        re.compile(r'^Running \d+ times$'),
        re.compile(r'^Ignoring warm-up '),
        re.compile(r'^Info:'),
        re.compile(r'^\d+(.\d+)?$'),
        # Following are for handle existing test like Dromaeo
        re.compile(re.escape("""main frame - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->-->" - has 1 onunload handler(s)""")),
        re.compile(re.escape("""frame "<!--framePath //<!--frame0-->/<!--frame0-->-->" - has 1 onunload handler(s)"""))]

    def _should_ignore_line_in_parser_test_result(self, line):
        if not line:
            return True
        for regex in self._lines_to_ignore_in_parser_result:
            if regex.search(line):
                return True
        return False

    def parse_output(self, output, printer, buildbot_output):
        got_a_result = False
        test_failed = False
        results = {}
        keys = ['avg', 'median', 'stdev', 'min', 'max']
        score_regex = re.compile(r'^(?P<key>' + r'|'.join(keys) + r')\s+(?P<value>[0-9\.]+)\s*(?P<unit>.*)')
        unit = "ms"

        for line in re.split('\n', output.text):
            score = score_regex.match(line)
            if score:
                results[score.group('key')] = float(score.group('value'))
                if score.group('unit'):
                    unit = score.group('unit')
                continue

            if not self._should_ignore_line_in_parser_test_result(line):
                test_failed = True
                printer.write("%s" % line)

        if test_failed or set(keys) != set(results.keys()):
            return None

        results['unit'] = unit

        test_name = re.sub(r'\.\w+$', '', self._test_name)
        buildbot_output.write('RESULT %s= %s %s\n' % (test_name.replace('/', ': '), results['avg'], unit))
        buildbot_output.write(', '.join(['%s= %s %s' % (key, results[key], unit) for key in keys[1:]]) + '\n')

        return {test_name: results}


class ChromiumStylePerfTest(PerfTest):
    _chromium_style_result_regex = re.compile(r'^RESULT\s+(?P<name>[^=]+)\s*=\s+(?P<value>\d+(\.\d+)?)\s*(?P<unit>\w+)$')

    def __init__(self, test_name, dirname, path_or_url):
        super(ChromiumStylePerfTest, self).__init__(test_name, dirname, path_or_url)

    def parse_output(self, output, printer, buildbot_output):
        test_failed = False
        got_a_result = False
        results = {}
        for line in re.split('\n', output.text):
            resultLine = ChromiumStylePerfTest._chromium_style_result_regex.match(line)
            if resultLine:
                # FIXME: Store the unit
                results[self.test_name() + ':' + resultLine.group('name').replace(' ', '')] = float(resultLine.group('value'))
                buildbot_output.write("%s\n" % line)
            elif not len(line) == 0:
                test_failed = True
                printer.write("%s" % line)
        return results if results and not test_failed else None
