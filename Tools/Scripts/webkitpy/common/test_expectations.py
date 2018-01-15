# Copyright (C) 2018 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import os


class TestExpectations(object):

    def __init__(self, port_name, expectations_file, build_type='Release'):
        self._port_name = port_name
        self._build_type = build_type
        if os.path.isfile(expectations_file):
            with open(expectations_file, 'r') as fd:
                self._expectations = json.load(fd)
        else:
            self._expectations = {}

    def _port_name_for_expected(self, expected):
        if self._port_name in expected:
            return self._port_name

        name_with_build = self._port_name + '@' + self._build_type
        if name_with_build in expected:
            return name_with_build

        if 'all' in expected:
            return 'all'

        name_with_build = 'all@' + self._build_type
        if name_with_build in expected:
            return name_with_build

        return None

    def _expected_value(self, expected, value, default):
        port_name = self._port_name_for_expected(expected)
        if port_name is None:
            return default

        port_expected = expected[port_name]
        if value in port_expected:
            return port_expected[value]

        return default

    def skipped_tests(self):
        skipped = []
        for test in self._expectations:
            if 'expected' not in self._expectations[test]:
                continue

            expected = self._expectations[test]['expected']
            if 'SKIP' in self._expected_value(expected, 'status', []):
                skipped.append(test)
        return skipped

    def skipped_subtests(self, test):
        skipped = []
        if test not in self._expectations:
            return skipped

        test_expectation = self._expectations[test]
        if 'subtests' not in test_expectation:
            return skipped

        subtests = test_expectation['subtests']
        for subtest in subtests:
            if 'SKIP' in self._expected_value(subtests[subtest]['expected'], 'status', []):
                skipped.append(subtest)
        return skipped

    def _expectation_value(self, test, subtest, value, default):
        retval = default
        if test not in self._expectations:
            return retval

        test_expectation = self._expectations[test]
        if 'expected' in test_expectation:
            retval = self._expected_value(test_expectation['expected'], value, retval)

        if subtest is None or 'subtests' not in test_expectation:
            return retval

        subtests = test_expectation['subtests']
        if subtest not in subtests:
            return retval

        return self._expected_value(subtests[subtest]['expected'], value, retval)

    def is_slow(self, test, subtest=None):
        return self._expectation_value(test, subtest, 'slow', False)

    def get_expectation(self, test, subtest=None):
        return self._expectation_value(test, subtest, 'status', ['PASS'])
