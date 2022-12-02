#! /usr/bin/env python3
#
# Copyright 2022 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# run_angle_android_test.py:
#   Runs ANGLE tests using android_helper wrapper. Example:
#     (cd out/Android; ../../src/tests/run_angle_android_test.py \
#       angle_trace_tests \
#       --filter='TraceTest.words_with_friends_2' \
#       --no-warmup --steps-per-trial 1000 --trials 1)

import argparse
import fnmatch
import logging
import os
import pathlib
import sys

PY_UTILS = str(pathlib.Path(__file__).resolve().parent / 'py_utils')
if PY_UTILS not in sys.path:
    os.stat(PY_UTILS) and sys.path.insert(0, PY_UTILS)
import android_helper
import angle_test_util


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'suite', help='Test suite to run.', choices=['angle_perftests', 'angle_end2end_tests'])
    parser.add_argument(
        '-f',
        '--filter',
        '--isolated-script-test-filter',
        '--gtest_filter',
        type=str,
        help='Test filter.')
    parser.add_argument('--list-tests', help='List tests.', action='store_true')
    parser.add_argument('-l', '--log', help='Logging level.', default='info')

    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    android_helper.Initialize(args.suite)
    assert android_helper.IsAndroid()

    rc, output, _ = android_helper.RunTests(
        args.suite, ['--list-tests', '--verbose'] + extra_flags, log_output=False)
    if rc != 0:
        logging.fatal('Could not find test list from test output:\n%s' % output)
        return rc

    tests = angle_test_util.GetTestsFromOutput(output)
    if args.filter:
        tests = [test for test in tests if fnmatch.fnmatch(test, args.filter)]

    if args.list_tests:
        for test in tests:
            print(test)
        return 0

    if args.suite == 'angle_perftests':
        traces = set(android_helper.GetTraceFromTestName(test) for test in tests)
        android_helper.PrepareRestrictedTraces(traces)

    flags = ['--gtest_filter=' + args.filter] if args.filter else []
    return android_helper.RunTests(args.suite, flags + extra_flags)[0]


if __name__ == '__main__':
    sys.exit(main())
