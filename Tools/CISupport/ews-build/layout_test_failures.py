# Copyright (C) 2019 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json


class LayoutTestFailures(object):
    _JSON_PREFIX = "ADD_RESULTS("
    _JSON_SUFFIX = ");"

    def __init__(self, failing_tests, flaky_tests, did_exceed_test_failure_limit):
        self.failing_tests = failing_tests
        self.flaky_tests = flaky_tests
        self.did_exceed_test_failure_limit = did_exceed_test_failure_limit

    @classmethod
    def _strip_json_wrapper(cls, json_content):
        if json_content.startswith(cls._JSON_PREFIX) and json_content.endswith(cls._JSON_SUFFIX):
            return json_content[len(cls._JSON_PREFIX):-len(cls._JSON_SUFFIX)]
        return json_content

    @classmethod
    def results_from_string(cls, string):
        if not string:
            return None

        content_string = cls._strip_json_wrapper(string.strip())
        # Workaround for https://github.com/buildbot/buildbot/issues/4906
        content_string = ''.join(content_string.splitlines())
        try:
            json_dict = json.loads(content_string)
        except json.JSONDecodeError:
            return None

        failing_tests = []
        flaky_tests = []

        def get_failing_tests(test, result):
            if result.get('report') in ['REGRESSION', 'MISSING']:
                failing_tests.append(test)
            elif result.get('report') in ['FLAKY']:
                flaky_tests.append(test)

        cls.parse_full_results_json(json_dict['tests'], get_failing_tests)
        return cls(failing_tests, flaky_tests, json_dict.get('interrupted', False))

    @classmethod
    def parse_full_results_json(cls, tree, handler, prefix=''):
        for key in tree:
            new_prefix = (prefix + '/' + key) if prefix else key
            if 'actual' not in tree[key]:
                cls.parse_full_results_json(tree[key], handler, new_prefix)
            else:
                handler(new_prefix, tree[key])
