#! /usr/bin/env vpython3
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# run_perf_test.py:
#   Runs ANGLE perf tests using some statistical averaging.

import argparse
import fnmatch
import importlib
import io
import json
import logging
import time
import os
import pathlib
import re
import subprocess
import sys

PY_UTILS = str(pathlib.Path(__file__).resolve().parent / 'py_utils')
if PY_UTILS not in sys.path:
    os.stat(PY_UTILS) and sys.path.insert(0, PY_UTILS)
import android_helper
import angle_path_util
import angle_test_util

angle_path_util.AddDepsDirToPath('testing/scripts')
import common

angle_path_util.AddDepsDirToPath('third_party/catapult/tracing')
from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value import merge_histograms

ANGLE_PERFTESTS = 'angle_perftests'
DEFAULT_LOG = 'info'
DEFAULT_SAMPLES = 4
DEFAULT_TRIALS = 3
DEFAULT_MAX_ERRORS = 3
DEFAULT_WARMUP_LOOPS = 2
DEFAULT_CALIBRATION_TIME = 2

# Test expectations
FAIL = 'FAIL'
PASS = 'PASS'
SKIP = 'SKIP'

EXIT_FAILURE = 1
EXIT_SUCCESS = 0


def _filter_tests(tests, pattern):
    return [test for test in tests if fnmatch.fnmatch(test, pattern)]


def _shard_tests(tests, shard_count, shard_index):
    return [tests[index] for index in range(shard_index, len(tests), shard_count)]


def _get_results_from_output(output, result):
    m = re.search(r'Running (\d+) tests', output)
    if m and int(m.group(1)) > 1:
        raise Exception('Found more than one test result in output')

    # Results are reported in the format:
    # name_backend.result: story= value units.
    pattern = r'\.' + result + r':.*= ([0-9.]+)'
    logging.debug('Searching for %s in output' % pattern)
    m = re.findall(pattern, output)
    if not m:
        logging.warning('Did not find the result "%s" in the test output:\n%s' % (result, output))
        return None

    return [float(value) for value in m]


def _get_tests_from_output(output):
    out_lines = output.split('\n')
    start = out_lines.index('Tests list:')
    end = out_lines.index('End tests list.')
    return out_lines[start + 1:end]


def _truncated_list(data, n):
    """Compute a truncated list, n is truncation size"""
    if len(data) < n * 2:
        raise ValueError('list not large enough to truncate')
    return sorted(data)[n:-n]


def _mean(data):
    """Return the sample arithmetic mean of data."""
    n = len(data)
    if n < 1:
        raise ValueError('mean requires at least one data point')
    return float(sum(data)) / float(n)  # in Python 2 use sum(data)/float(n)


def _sum_of_square_deviations(data, c):
    """Return sum of square deviations of sequence data."""
    ss = sum((float(x) - c)**2 for x in data)
    return ss


def _coefficient_of_variation(data):
    """Calculates the population coefficient of variation."""
    n = len(data)
    if n < 2:
        raise ValueError('variance requires at least two data points')
    c = _mean(data)
    ss = _sum_of_square_deviations(data, c)
    pvar = ss / n  # the population variance
    stddev = (pvar**0.5)  # population standard deviation
    return stddev / c


def _save_extra_output_files(args, results, histograms):
    isolated_out_dir = os.path.dirname(args.isolated_script_test_output)
    if not os.path.isdir(isolated_out_dir):
        return
    benchmark_path = os.path.join(isolated_out_dir, args.test_suite)
    if not os.path.isdir(benchmark_path):
        os.makedirs(benchmark_path)
    test_output_path = os.path.join(benchmark_path, 'test_results.json')
    results.save_to_json_file(test_output_path)
    perf_output_path = os.path.join(benchmark_path, 'perf_results.json')
    logging.info('Saving perf histograms to %s.' % perf_output_path)
    with open(perf_output_path, 'w') as out_file:
        out_file.write(json.dumps(histograms.AsDicts(), indent=2))


class Results:

    def __init__(self):
        self._results = {
            'tests': {},
            'interrupted': False,
            'seconds_since_epoch': time.time(),
            'path_delimiter': '.',
            'version': 3,
            'num_failures_by_type': {
                FAIL: 0,
                PASS: 0,
                SKIP: 0,
            },
        }
        self._test_results = {}

    def has_failures(self):
        return self._results['num_failures_by_type'][FAIL] > 0

    def has_result(self, test):
        return test in self._test_results

    def result_skip(self, test):
        self._test_results[test] = {'expected': SKIP, 'actual': SKIP}
        self._results['num_failures_by_type'][SKIP] += 1

    def result_pass(self, test):
        self._test_results[test] = {'expected': PASS, 'actual': PASS}
        self._results['num_failures_by_type'][PASS] += 1

    def result_fail(self, test):
        self._test_results[test] = {'expected': PASS, 'actual': FAIL, 'is_unexpected': True}
        self._results['num_failures_by_type'][FAIL] += 1

    def save_to_output_file(self, test_suite, fname):
        self._update_results(test_suite)
        with open(fname, 'w') as out_file:
            out_file.write(json.dumps(self._results, indent=2))

    def save_to_json_file(self, fname):
        logging.info('Saving test results to %s.' % fname)
        with open(fname, 'w') as out_file:
            out_file.write(json.dumps(self._results, indent=2))

    def _update_results(self, test_suite):
        if self._test_results:
            self._results['tests'][test_suite] = self._test_results
            self._test_results = {}


def _read_histogram(histogram_file_path):
    with open(histogram_file_path) as histogram_file:
        histogram = histogram_set.HistogramSet()
        histogram.ImportDicts(json.load(histogram_file))
        return histogram


def _merge_into_one_histogram(test_histogram_set):
    with common.temporary_file() as merge_histogram_path:
        logging.info('Writing merged histograms to %s.' % merge_histogram_path)
        with open(merge_histogram_path, 'w') as merge_histogram_file:
            json.dump(test_histogram_set.AsDicts(), merge_histogram_file)
            merge_histogram_file.close()
        merged_dicts = merge_histograms.MergeHistograms(merge_histogram_path, groupby=['name'])
        merged_histogram = histogram_set.HistogramSet()
        merged_histogram.ImportDicts(merged_dicts)
        return merged_histogram


def _wall_times_stats(wall_times):
    if len(wall_times) > 7:
        truncation_n = len(wall_times) >> 3
        logging.debug('Truncation: Removing the %d highest and lowest times from wall_times.' %
                      truncation_n)
        wall_times = _truncated_list(wall_times, truncation_n)

    if len(wall_times) > 1:
        return ('truncated mean wall_time = %.2f, cov = %.2f%%' %
                (_mean(wall_times), _coefficient_of_variation(wall_times) * 100.0))

    return None


def _run_test_suite(args, cmd_args, env):
    android_test_runner_args = [
        '--extract-test-list-from-filter',
        '--enable-device-cache',
        '--skip-clear-data',
        '--use-existing-test-data',
    ]
    return angle_test_util.RunTestSuite(
        args.test_suite,
        cmd_args,
        env,
        runner_args=android_test_runner_args,
        use_xvfb=args.xvfb,
        show_test_stdout=args.show_test_stdout)


def _run_calibration(args, common_args, env):
    exit_code, calibrate_output, json_results = _run_test_suite(
        args, common_args + [
            '--calibration',
            '--warmup-loops',
            str(args.warmup_loops),
        ], env)
    if exit_code != EXIT_SUCCESS:
        raise RuntimeError('%s failed. Output:\n%s' % (args.test_suite, calibrate_output))
    if SKIP in json_results['num_failures_by_type']:
        return SKIP, None

    steps_per_trial = _get_results_from_output(calibrate_output, 'steps_to_run')
    if not steps_per_trial:
        return FAIL, None

    assert (len(steps_per_trial) == 1)
    return PASS, int(steps_per_trial[0])


def _run_perf(args, common_args, env, steps_per_trial):
    run_args = common_args + [
        '--steps-per-trial',
        str(steps_per_trial),
        '--trials',
        str(args.trials_per_sample),
    ]

    if args.smoke_test_mode:
        run_args += ['--no-warmup']
    else:
        run_args += ['--warmup-loops', str(args.warmup_loops)]

    if args.perf_counters:
        run_args += ['--perf-counters', args.perf_counters]

    with common.temporary_file() as histogram_file_path:
        run_args += ['--isolated-script-test-perf-output=%s' % histogram_file_path]

        exit_code, output, json_results = _run_test_suite(args, run_args, env)
        if exit_code != EXIT_SUCCESS:
            raise RuntimeError('%s failed. Output:\n%s' % (args.test_suite, output))
        if SKIP in json_results['num_failures_by_type']:
            return SKIP, None, None

        sample_wall_times = _get_results_from_output(output, 'wall_time')
        if sample_wall_times:
            sample_histogram = _read_histogram(histogram_file_path)
            return PASS, sample_wall_times, sample_histogram

    return FAIL, None, None


class _MaxErrorsException(Exception):
    pass


def _skipped_or_glmark2(test, test_status):
    if test_status == SKIP:
        logging.info('Test skipped by suite: %s' % test)
        return True

    # GLMark2Benchmark logs .fps/.score instead of our perf metrics.
    if test.startswith('GLMark2Benchmark.Run/'):
        logging.info('GLMark2Benchmark missing metrics (as expected, skipping): %s' % test)
        return True

    return False


def _run_tests(tests, args, extra_flags, env):
    results = Results()
    histograms = histogram_set.HistogramSet()
    total_errors = 0
    prepared_traces = set()

    for test_index in range(len(tests)):
        if total_errors >= args.max_errors:
            raise _MaxErrorsException()

        test = tests[test_index]

        if angle_test_util.IsAndroid():
            trace = android_helper.GetTraceFromTestName(test)
            if trace and trace not in prepared_traces:
                android_helper.PrepareRestrictedTraces([trace])
                prepared_traces.add(trace)

        common_args = [
            '--gtest_filter=%s' % test,
            '--verbose',
            '--calibration-time',
            str(args.calibration_time),
        ] + extra_flags

        if args.steps_per_trial:
            steps_per_trial = args.steps_per_trial
        else:
            try:
                test_status, steps_per_trial = _run_calibration(args, common_args, env)
            except RuntimeError as e:
                logging.fatal(e)
                total_errors += 1
                results.result_fail(test)
                continue

            if _skipped_or_glmark2(test, test_status):
                results.result_skip(test)
                continue

            if not steps_per_trial:
                logging.error('Test %s missing steps_per_trial' % test)
                results.result_fail(test)
                continue

        logging.info('Test %d/%d: %s (samples=%d trials_per_sample=%d steps_per_trial=%d)' %
                     (test_index + 1, len(tests), test, args.samples_per_test,
                      args.trials_per_sample, steps_per_trial))

        wall_times = []
        test_histogram_set = histogram_set.HistogramSet()
        for sample in range(args.samples_per_test):
            try:
                test_status, sample_wall_times, sample_histogram = _run_perf(
                    args, common_args, env, steps_per_trial)
            except RuntimeError as e:
                logging.error(e)
                results.result_fail(test)
                total_errors += 1
                break

            if _skipped_or_glmark2(test, test_status):
                results.result_skip(test)
                break

            if not sample_wall_times:
                logging.error('Test %s failed to produce a sample output' % test)
                results.result_fail(test)
                break

            logging.info('Test %d/%d Sample %d/%d wall_times: %s' %
                         (test_index + 1, len(tests), sample + 1, args.samples_per_test,
                          str(sample_wall_times)))

            if len(sample_wall_times) != args.trials_per_sample:
                logging.error('Test %s failed to record some wall_times (expected %d, got %d)' %
                              (test, args.trials_per_sample, len(sample_wall_times)))
                results.result_fail(test)
                break

            wall_times += sample_wall_times
            test_histogram_set.Merge(sample_histogram)

        if not results.has_result(test):
            assert len(wall_times) == (args.samples_per_test * args.trials_per_sample)
            stats = _wall_times_stats(wall_times)
            if stats:
                logging.info('Test %d/%d: %s: %s' % (test_index + 1, len(tests), test, stats))
            histograms.Merge(_merge_into_one_histogram(test_histogram_set))
            results.result_pass(test)

    return results, histograms


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--isolated-script-test-output', type=str)
    parser.add_argument('--isolated-script-test-perf-output', type=str)
    parser.add_argument(
        '-f', '--filter', '--isolated-script-test-filter', type=str, help='Test filter.')
    parser.add_argument('--test-suite', help='Test suite to run.', default=ANGLE_PERFTESTS)
    parser.add_argument('--xvfb', help='Use xvfb.', action='store_true')
    parser.add_argument(
        '--shard-count',
        help='Number of shards for test splitting. Default is 1.',
        type=int,
        default=1)
    parser.add_argument(
        '--shard-index',
        help='Index of the current shard for test splitting. Default is 0.',
        type=int,
        default=0)
    parser.add_argument(
        '-l', '--log', help='Log output level. Default is %s.' % DEFAULT_LOG, default=DEFAULT_LOG)
    parser.add_argument(
        '-s',
        '--samples-per-test',
        help='Number of samples to run per test. Default is %d.' % DEFAULT_SAMPLES,
        type=int,
        default=DEFAULT_SAMPLES)
    parser.add_argument(
        '-t',
        '--trials-per-sample',
        help='Number of trials to run per sample. Default is %d.' % DEFAULT_TRIALS,
        type=int,
        default=DEFAULT_TRIALS)
    parser.add_argument(
        '--steps-per-trial', help='Fixed number of steps to run per trial.', type=int)
    parser.add_argument(
        '--max-errors',
        help='After this many errors, abort the run. Default is %d.' % DEFAULT_MAX_ERRORS,
        type=int,
        default=DEFAULT_MAX_ERRORS)
    parser.add_argument(
        '--smoke-test-mode', help='Do a quick run to validate correctness.', action='store_true')
    parser.add_argument(
        '--warmup-loops',
        help='Number of warmup loops to run in the perf test. Default is %d.' %
        DEFAULT_WARMUP_LOOPS,
        type=int,
        default=DEFAULT_WARMUP_LOOPS)
    parser.add_argument(
        '--calibration-time',
        help='Amount of time to spend each loop in calibration and warmup. Default is %d seconds.'
        % DEFAULT_CALIBRATION_TIME,
        type=int,
        default=DEFAULT_CALIBRATION_TIME)
    parser.add_argument(
        '--show-test-stdout', help='Prints all test stdout during execution.', action='store_true')
    parser.add_argument(
        '--perf-counters', help='Colon-separated list of extra perf counter metrics.')

    args, extra_flags = parser.parse_known_args()

    angle_test_util.SetupLogging(args.log.upper())

    start_time = time.time()

    # Use fast execution for smoke test mode.
    if args.smoke_test_mode:
        args.steps_per_trial = 1
        args.trials_per_sample = 1
        args.samples_per_test = 1

    env = os.environ.copy()

    if angle_test_util.HasGtestShardsAndIndex(env):
        args.shard_count, args.shard_index = angle_test_util.PopGtestShardsAndIndex(env)

    angle_test_util.Initialize(args.test_suite)

    # Get test list
    if angle_test_util.IsAndroid():
        tests = android_helper.ListTests(args.test_suite)
    else:
        exit_code, output, _ = _run_test_suite(args, ['--list-tests', '--verbose'], env)
        if exit_code != EXIT_SUCCESS:
            logging.fatal('Could not find test list from test output:\n%s' % output)
        tests = _get_tests_from_output(output)

    if args.filter:
        tests = _filter_tests(tests, args.filter)

    # Get tests for this shard (if using sharding args)
    tests = _shard_tests(tests, args.shard_count, args.shard_index)

    if not tests:
        logging.error('No tests to run.')
        return EXIT_FAILURE

    if angle_test_util.IsAndroid() and args.test_suite == ANGLE_PERFTESTS:
        android_helper.RunSmokeTest()

    logging.info('Running %d test%s' % (len(tests), 's' if len(tests) > 1 else ' '))

    try:
        results, histograms = _run_tests(tests, args, extra_flags, env)
    except _MaxErrorsException:
        logging.error('Error count exceeded max errors (%d). Aborting.' % args.max_errors)
        return EXIT_FAILURE

    for test in tests:
        assert results.has_result(test)

    if args.isolated_script_test_output:
        results.save_to_output_file(args.test_suite, args.isolated_script_test_output)

        # Uses special output files to match the merge script.
        _save_extra_output_files(args, results, histograms)

    if args.isolated_script_test_perf_output:
        with open(args.isolated_script_test_perf_output, 'w') as out_file:
            out_file.write(json.dumps(histograms.AsDicts(), indent=2))

    end_time = time.time()
    logging.info('Elapsed time: %.2lf seconds.' % (end_time - start_time))

    if results.has_failures():
        return EXIT_FAILURE
    return EXIT_SUCCESS


if __name__ == '__main__':
    sys.exit(main())
