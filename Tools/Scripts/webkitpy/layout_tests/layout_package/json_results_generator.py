# Copyright (C) 2010 Google Inc. All rights reserved.
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

import json
import logging

# A JSON results generator for generic tests.
# FIXME: move this code out of the layout_package directory.

_log = logging.getLogger(__name__)

_JSON_PREFIX = "ADD_RESULTS("
_JSON_SUFFIX = ");"
_JSON_PREFIX_B = b"ADD_RESULTS("
_JSON_SUFFIX_B = b");"


def has_json_wrapper(string):
    if isinstance(string, bytes):
        return string.startswith(_JSON_PREFIX_B) and string.endswith(_JSON_SUFFIX_B)
    else:
        return string.startswith(_JSON_PREFIX) and string.endswith(_JSON_SUFFIX)


def strip_json_wrapper(json_content):
    # FIXME: Kill this code once the server returns json instead of jsonp.
    if has_json_wrapper(json_content):
        return json_content[len(_JSON_PREFIX):len(json_content) - len(_JSON_SUFFIX)]
    return json_content


def load_json(filesystem, file_path):
    content = filesystem.read_text_file(file_path)
    content = strip_json_wrapper(content)
    return json.loads(content)


def write_json(filesystem, json_object, file_path, callback=None):
    # Specify separators in order to get compact encoding.
    json_string = json.dumps(json_object, separators=(',', ':'))
    if callback:
        json_string = callback + "(" + json_string + ");"
    filesystem.write_text_file(file_path, json_string)


def add_path_to_trie(path, value, trie):
    """Inserts a single flat directory path and associated value into a directory trie structure."""
    if "/" not in path:
        trie[path] = value
        return

    directory, slash, rest = path.partition("/")
    if directory not in trie:
        trie[directory] = {}
    add_path_to_trie(rest, value, trie[directory])


def _add_perf_metric_for_test(path, time, tests, depth, depth_limit):
    """
    Aggregate test time to result for a given test at a specified depth_limit.
    """
    if "/" not in path:
        tests["tests"][path] = {
            "metrics": {
                "Time": {
                    "current": [time],
                }}}
        return

    directory, slash, rest = path.partition("/")
    if depth == depth_limit:
        if directory not in tests["tests"]:
            tests["tests"][directory] = {
                "metrics": {
                    "Time": {
                        "current": [time],
                    }}}
        else:
            tests["tests"][directory]["metrics"]["Time"]["current"][0] += time
        return
    else:
        if directory not in tests["tests"]:
            tests["tests"][directory] = {
                "metrics": {
                    "Time": ["Total", "Arithmetic"],
                },
                "tests": {}
            }
        _add_perf_metric_for_test(rest, time, tests["tests"][directory], depth + 1, depth_limit)


def perf_metrics_for_test(run_time, individual_test_timings):
    """
    Output two performace metrics
    1. run time, which is how much time consumed by the layout tests script
    2. run time of first-level and second-level of test directories
    """
    total_run_time = 0

    for test_result in individual_test_timings:
        total_run_time += int(1000 * test_result.test_run_time)

    perf_metric = {
        "layout_tests": {
            "metrics": {
                "Time": ["Total", "Arithmetic"],
            },
            "tests": {}
        },
        "layout_tests_run_time": {
            "metrics": {
                "Time": {"current": [run_time]},
            }}}
    for test_result in individual_test_timings:
        test = test_result.test_name
        # for now, we only send two levels of directories
        _add_perf_metric_for_test(test, int(1000 * test_result.test_run_time), perf_metric["layout_tests"], 1, 2)
    return perf_metric
