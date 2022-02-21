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
import re
import subprocess
import sys

# Add //src/testing into sys.path for importing xvfb and test_env, and
# //src/testing/scripts for importing common.
d = os.path.dirname
THIS_DIR = d(os.path.abspath(__file__))
ANGLE_DIR = d(d(THIS_DIR))
sys.path.append(os.path.join(ANGLE_DIR, 'testing'))
sys.path.append(os.path.join(ANGLE_DIR, 'testing', 'scripts'))

import common
import test_env
import xvfb

sys.path.append(os.path.join(ANGLE_DIR, 'third_party', 'catapult', 'tracing'))
from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value import merge_histograms

DEFAULT_TEST_SUITE = 'angle_perftests'
DEFAULT_LOG = 'info'
DEFAULT_SAMPLES = 4
DEFAULT_TRIALS = 3
DEFAULT_MAX_ERRORS = 3
DEFAULT_WARMUP_LOOPS = 2
DEFAULT_CALIBRATION_TIME = 2

# Filters out stuff like: " I   72.572s run_tests_on_device(96071FFAZ00096) "
ANDROID_LOGGING_PREFIX = r'I +\d+.\d+s \w+\(\w+\)  '

# Test expectations
FAIL = 'FAIL'
PASS = 'PASS'
SKIP = 'SKIP'

EXIT_FAILURE = 1
EXIT_SUCCESS = 0


def is_windows():
    return sys.platform == 'cygwin' or sys.platform.startswith('win')


def get_binary_name(binary):
    if is_windows():
        return '.\\%s.exe' % binary
    else:
        return './%s' % binary


def _popen(*args, **kwargs):
    assert 'creationflags' not in kwargs
    if sys.platform == 'win32':
        # Necessary for signal handling. See crbug.com/733612#c6.
        kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
    return subprocess.Popen(*args, **kwargs)


def run_command_with_output(argv, stdoutfile, env=None, cwd=None, log=True):
    assert stdoutfile
    with io.open(stdoutfile, 'wb') as writer:
        process = _popen(argv, env=env, cwd=cwd, stdout=writer, stderr=subprocess.STDOUT)
        test_env.forward_signals([process])
        while process.poll() is None:
            # This sleep is needed for signal propagation. See the
            # wait_with_signals() docstring.
            time.sleep(0.1)
        return process.returncode


def _run_and_get_output(args, cmd, env):
    lines = []
    logging.debug(' '.join(cmd))
    with common.temporary_file() as tempfile_path:
        if args.xvfb:
            exit_code = xvfb.run_executable(cmd, env, stdoutfile=tempfile_path)
        else:
            exit_code = run_command_with_output(cmd, env=env, stdoutfile=tempfile_path, log=False)
        with open(tempfile_path) as f:
            for line in f:
                lines.append(line.strip())
    return exit_code, lines


def _filter_tests(tests, pattern):
    return [test for test in tests if fnmatch.fnmatch(test, pattern)]


def _shard_tests(tests, shard_count, shard_index):
    return [tests[index] for index in range(shard_index, len(tests), shard_count)]


def _get_results_from_output(output, result):
    output = '\n'.join(output)
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


def _get_tests_from_output(lines):
    seen_start_of_tests = False
    tests = []
    android_prefix = re.compile(ANDROID_LOGGING_PREFIX)
    logging.debug('Read %d lines from test output.' % len(lines))
    for line in lines:
        line = android_prefix.sub('', line.strip())
        if line == 'Tests list:':
            seen_start_of_tests = True
        elif line == 'End tests list.':
            break
        elif seen_start_of_tests:
            tests.append(line)
    if not seen_start_of_tests:
        raise Exception('Did not find test list in test output!')
    logging.debug('Found %d tests from test output.' % len(tests))
    return tests


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--isolated-script-test-output', type=str)
    parser.add_argument('--isolated-script-test-perf-output', type=str)
    parser.add_argument(
        '-f', '--filter', '--isolated-script-test-filter', type=str, help='Test filter.')
    parser.add_argument('--test-suite', help='Test suite to run.', default=DEFAULT_TEST_SUITE)
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

    args, extra_flags = parser.parse_known_args()

    importlib.reload(logging)
    logging.basicConfig(level=args.log.upper())

    start_time = time.time()

    # Use fast execution for smoke test mode.
    if args.smoke_test_mode:
        args.steps_per_trial = 1
        args.trials_per_sample = 1
        args.samples_per_test = 1

    env = os.environ.copy()

    # Get sharding args
    if 'GTEST_TOTAL_SHARDS' in env and int(env['GTEST_TOTAL_SHARDS']) != 1:
        if 'GTEST_SHARD_INDEX' not in env:
            logging.error('Sharding params must be specified together.')
            sys.exit(1)
        args.shard_count = int(env.pop('GTEST_TOTAL_SHARDS'))
        args.shard_index = int(env.pop('GTEST_SHARD_INDEX'))

    # Get test list
    cmd = [get_binary_name(args.test_suite), '--list-tests', '--verbose']
    exit_code, lines = _run_and_get_output(args, cmd, env)
    if exit_code != EXIT_SUCCESS:
        logging.fatal('Could not find test list from test output:\n%s' % '\n'.join(lines))
    tests = _get_tests_from_output(lines)

    if args.filter:
        tests = _filter_tests(tests, args.filter)

    # Get tests for this shard (if using sharding args)
    tests = _shard_tests(tests, args.shard_count, args.shard_index)

    num_tests = len(tests)
    if num_tests == 0:
        logging.error('No tests to run.')
        return EXIT_FAILURE

    logging.info('Running %d test%s' % (num_tests, 's' if num_tests > 1 else ' '))

    # Run tests
    results = Results()

    histograms = histogram_set.HistogramSet()
    total_errors = 0

    for test_index in range(num_tests):
        test = tests[test_index]
        cmd = [
            get_binary_name(args.test_suite),
            '--gtest_filter=%s' % test,
            '--extract-test-list-from-filter',
            '--enable-device-cache',
            '--skip-clear-data',
            '--use-existing-test-data',
            '--verbose',
            '--calibration-time',
            str(args.calibration_time),
        ]
        if args.steps_per_trial:
            steps_per_trial = args.steps_per_trial
        else:
            cmd_calibrate = cmd + [
                '--calibration',
                '--warmup-loops',
                str(args.warmup_loops),
            ]
            exit_code, calibrate_output = _run_and_get_output(args, cmd_calibrate, env)
            if exit_code != EXIT_SUCCESS:
                logging.fatal('%s failed. Output:\n%s' %
                              (cmd_calibrate[0], '\n'.join(calibrate_output)))
                total_errors += 1
                results.result_fail(test)
                continue
            steps_per_trial = _get_results_from_output(calibrate_output, 'steps_to_run')
            if not steps_per_trial:
                logging.warning('Skipping test %s' % test)
                continue
            assert (len(steps_per_trial) == 1)
            steps_per_trial = int(steps_per_trial[0])
        logging.info('Test %d/%d: %s (samples=%d trials_per_sample=%d steps_per_trial=%d)' %
                     (test_index + 1, num_tests, test, args.samples_per_test,
                      args.trials_per_sample, steps_per_trial))
        wall_times = []
        test_histogram_set = histogram_set.HistogramSet()
        for sample in range(args.samples_per_test):
            if total_errors >= args.max_errors:
                logging.error('Error count exceeded max errors (%d). Aborting.' % args.max_errors)
                return EXIT_FAILURE

            cmd_run = cmd + [
                '--steps-per-trial',
                str(steps_per_trial),
                '--trials',
                str(args.trials_per_sample),
            ]
            if args.smoke_test_mode:
                cmd_run += ['--no-warmup']
            else:
                cmd_run += ['--warmup-loops', str(args.warmup_loops)]
            with common.temporary_file() as histogram_file_path:
                cmd_run += ['--isolated-script-test-perf-output=%s' % histogram_file_path]
                exit_code, output = _run_and_get_output(args, cmd_run, env)
                if exit_code != EXIT_SUCCESS:
                    logging.error('%s failed. Output:\n%s' % (cmd_run[0], '\n'.join(output)))
                    results.result_fail(test)
                    total_errors += 1
                    break

                sample_wall_times = _get_results_from_output(output, 'wall_time')
                if not sample_wall_times:
                    # This can be intentional for skipped tests. They are handled below.
                    logging.warning('Test %s failed to produce a sample output' % test)
                    break
                logging.info('Test %d/%d Sample %d/%d wall_times: %s' %
                             (test_index + 1, num_tests, sample + 1, args.samples_per_test,
                              str(sample_wall_times)))
                wall_times += sample_wall_times
                with open(histogram_file_path) as histogram_file:
                    sample_json = json.load(histogram_file)
                    sample_histogram = histogram_set.HistogramSet()
                    sample_histogram.ImportDicts(sample_json)
                    test_histogram_set.Merge(sample_histogram)

        if not results.has_result(test):
            if not wall_times:
                logging.warning('Skipping test %s. Assuming this is intentional.' % test)
                results.result_skip(test)
            elif len(wall_times) == (args.samples_per_test * args.trials_per_sample):
                if len(wall_times) > 7:
                    truncation_n = len(wall_times) >> 3
                    logging.debug(
                        'Truncation: Removing the %d highest and lowest times from wall_times.' %
                        truncation_n)
                    wall_times = _truncated_list(wall_times, truncation_n)

                if len(wall_times) > 1:
                    logging.info('Test %d/%d: %s: truncated mean wall_time = %.2f, cov = %.2f%%' %
                                 (test_index + 1, num_tests, test, _mean(wall_times),
                                  (_coefficient_of_variation(wall_times) * 100.0)))
                results.result_pass(test)

                # Merge the histogram set into one histogram
                with common.temporary_file() as merge_histogram_path:
                    logging.info('Writing merged histograms to %s.' % merge_histogram_path)
                    with open(merge_histogram_path, 'w') as merge_histogram_file:
                        json.dump(test_histogram_set.AsDicts(), merge_histogram_file)
                        merge_histogram_file.close()
                    merged_dicts = merge_histograms.MergeHistograms(
                        merge_histogram_path, groupby=['name'])
                    merged_histogram = histogram_set.HistogramSet()
                    merged_histogram.ImportDicts(merged_dicts)
                    histograms.Merge(merged_histogram)
            else:
                logging.error('Test %s failed to record some samples' % test)

    if args.isolated_script_test_output:
        results.save_to_output_file(args.test_suite, args.isolated_script_test_output)

        # Uses special output files to match the merge script.
        _save_extra_output_files(args, results, histograms)

    if args.isolated_script_test_perf_output:
        with open(args.isolated_script_test_perf_output, 'w') as out_file:
            out_file.write(json.dumps(histograms.AsDicts(), indent=2))

    end_time = time.time()
    logging.info('Elapsed time: %.2lf seconds.' % (end_time - start_time))

    return EXIT_SUCCESS


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())
