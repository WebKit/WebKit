#! /usr/bin/env vpython3
#
# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# restricted_trace_gold_tests.py:
#   Uses Skia Gold (https://skia.org/dev/testing/skiagold) to run pixel tests with ANGLE traces.
#
#   Requires vpython to run standalone. Run with --help for usage instructions.

import argparse
import contextlib
import fnmatch
import json
import logging
import os
import platform
import re
import shutil
import sys
import tempfile
import time
import traceback

# Add //src/testing into sys.path for importing xvfb and test_env, and
# //src/testing/scripts for importing common.
d = os.path.dirname
THIS_DIR = d(os.path.abspath(__file__))
sys.path.insert(0, d(THIS_DIR))

from skia_gold import angle_skia_gold_properties
from skia_gold import angle_skia_gold_session_manager

ANGLE_SRC_DIR = d(d(d(THIS_DIR)))
sys.path.insert(0, os.path.join(ANGLE_SRC_DIR, 'testing'))
sys.path.insert(0, os.path.join(ANGLE_SRC_DIR, 'testing', 'scripts'))
# Handle the Chromium-relative directory as well. As long as one directory
# is valid, Python is happy.
CHROMIUM_SRC_DIR = d(d(ANGLE_SRC_DIR))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'testing'))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'testing', 'scripts'))

import common
import test_env
import xvfb


def IsWindows():
    return sys.platform == 'cygwin' or sys.platform.startswith('win')


DEFAULT_TEST_SUITE = 'angle_perftests'
DEFAULT_TEST_PREFIX = 'TracePerfTest.Run/vulkan_'
SWIFTSHADER_TEST_PREFIX = 'TracePerfTest.Run/vulkan_swiftshader_'
DEFAULT_SCREENSHOT_PREFIX = 'angle_vulkan_'
SWIFTSHADER_SCREENSHOT_PREFIX = 'angle_vulkan_swiftshader_'
DEFAULT_BATCH_SIZE = 5
DEFAULT_LOG = 'info'

# Filters out stuff like: " I   72.572s run_tests_on_device(96071FFAZ00096) "
ANDROID_LOGGING_PREFIX = r'I +\d+.\d+s \w+\(\w+\)  '
ANDROID_BEGIN_SYSTEM_INFO = '>>ScopedMainEntryLogger'

# Test expectations
FAIL = 'FAIL'
PASS = 'PASS'
SKIP = 'SKIP'


@contextlib.contextmanager
def temporary_dir(prefix=''):
    path = tempfile.mkdtemp(prefix=prefix)
    try:
        yield path
    finally:
        logging.info("Removing temporary directory: %s" % path)
        shutil.rmtree(path)


def add_skia_gold_args(parser):
    group = parser.add_argument_group('Skia Gold Arguments')
    group.add_argument('--git-revision', help='Revision being tested.', default=None)
    group.add_argument(
        '--gerrit-issue', help='For Skia Gold integration. Gerrit issue ID.', default='')
    group.add_argument(
        '--gerrit-patchset',
        help='For Skia Gold integration. Gerrit patch set number.',
        default='')
    group.add_argument(
        '--buildbucket-id', help='For Skia Gold integration. Buildbucket build ID.', default='')
    group.add_argument(
        '--bypass-skia-gold-functionality',
        action='store_true',
        default=False,
        help='Bypass all interaction with Skia Gold, effectively disabling the '
        'image comparison portion of any tests that use Gold. Only meant to '
        'be used in case a Gold outage occurs and cannot be fixed quickly.')
    local_group = group.add_mutually_exclusive_group()
    local_group.add_argument(
        '--local-pixel-tests',
        action='store_true',
        default=None,
        help='Specifies to run the test harness in local run mode or not. When '
        'run in local mode, uploading to Gold is disabled and links to '
        'help with local debugging are output. Running in local mode also '
        'implies --no-luci-auth. If both this and --no-local-pixel-tests are '
        'left unset, the test harness will attempt to detect whether it is '
        'running on a workstation or not and set this option accordingly.')
    local_group.add_argument(
        '--no-local-pixel-tests',
        action='store_false',
        dest='local_pixel_tests',
        help='Specifies to run the test harness in non-local (bot) mode. When '
        'run in this mode, data is actually uploaded to Gold and triage links '
        'arge generated. If both this and --local-pixel-tests are left unset, '
        'the test harness will attempt to detect whether it is running on a '
        'workstation or not and set this option accordingly.')
    group.add_argument(
        '--no-luci-auth',
        action='store_true',
        default=False,
        help='Don\'t use the service account provided by LUCI for '
        'authentication for Skia Gold, instead relying on gsutil to be '
        'pre-authenticated. Meant for testing locally instead of on the bots.')


def run_wrapper(args, cmd, env, stdoutfile=None):
    if args.xvfb:
        return xvfb.run_executable(cmd, env, stdoutfile=stdoutfile)
    else:
        return test_env.run_command_with_output(cmd, env=env, stdoutfile=stdoutfile)


def to_hex(num):
    return hex(int(num))


def to_hex_or_none(num):
    return 'None' if num == None else to_hex(num)


def to_non_empty_string_or_none(val):
    return 'None' if val == '' else str(val)


def to_non_empty_string_or_none_dict(d, key):
    return 'None' if not key in d else to_non_empty_string_or_none(d[key])


def get_binary_name(binary):
    if IsWindows():
        return '.\\%s.exe' % binary
    else:
        return './%s' % binary


def get_skia_gold_keys(args, env):
    """Get all the JSON metadata that will be passed to golctl."""
    # All values need to be strings, otherwise goldctl fails.

    # Only call this method one time
    if hasattr(get_skia_gold_keys, 'called') and get_skia_gold_keys.called:
        logging.exception('get_skia_gold_keys may only be called once')
    get_skia_gold_keys.called = True

    class Filter:

        def __init__(self):
            self.accepting_lines = True
            self.done_accepting_lines = False
            self.android_prefix = re.compile(ANDROID_LOGGING_PREFIX)
            self.lines = []
            self.is_android = False

        def append(self, line):
            if self.done_accepting_lines:
                return
            if 'Additional test environment' in line or 'android/test_runner.py' in line:
                self.accepting_lines = False
                self.is_android = True
            if ANDROID_BEGIN_SYSTEM_INFO in line:
                self.accepting_lines = True
                return
            if not self.accepting_lines:
                return

            if self.is_android:
                line = self.android_prefix.sub('', line)

            if line[0] == '}':
                self.done_accepting_lines = True

            self.lines.append(line)

        def get(self):
            return self.lines

    with common.temporary_file() as tempfile_path:
        binary = get_binary_name('angle_system_info_test')
        sysinfo_args = [binary, '--vulkan', '-v']
        if args.swiftshader:
            sysinfo_args.append('--swiftshader')
        if run_wrapper(args, sysinfo_args, env, tempfile_path):
            raise Exception('Error getting system info.')

        filter = Filter()

        with open(tempfile_path) as f:
            for line in f:
                filter.append(line)

        str = ''.join(filter.get())
        logging.info(str)
        json_data = json.loads(str)

    if len(json_data.get('gpus', [])) == 0 or not 'activeGPUIndex' in json_data:
        raise Exception('Error getting system info.')

    active_gpu = json_data['gpus'][json_data['activeGPUIndex']]

    angle_keys = {
        'vendor_id': to_hex_or_none(active_gpu['vendorId']),
        'device_id': to_hex_or_none(active_gpu['deviceId']),
        'model_name': to_non_empty_string_or_none_dict(active_gpu, 'machineModelVersion'),
        'manufacturer_name': to_non_empty_string_or_none_dict(active_gpu, 'machineManufacturer'),
        'os': to_non_empty_string_or_none(platform.system()),
        'os_version': to_non_empty_string_or_none(platform.version()),
        'driver_version': to_non_empty_string_or_none_dict(active_gpu, 'driverVersion'),
        'driver_vendor': to_non_empty_string_or_none_dict(active_gpu, 'driverVendor'),
    }

    return angle_keys


def output_diff_local_files(gold_session, image_name):
    """Logs the local diff image files from the given SkiaGoldSession.

  Args:
    gold_session: A skia_gold_session.SkiaGoldSession instance to pull files
        from.
    image_name: A string containing the name of the image/test that was
        compared.
  """
    given_file = gold_session.GetGivenImageLink(image_name)
    closest_file = gold_session.GetClosestImageLink(image_name)
    diff_file = gold_session.GetDiffImageLink(image_name)
    failure_message = 'Unable to retrieve link'
    logging.error('Generated image: %s', given_file or failure_message)
    logging.error('Closest image: %s', closest_file or failure_message)
    logging.error('Diff image: %s', diff_file or failure_message)


def upload_test_result_to_skia_gold(args, gold_session_manager, gold_session, gold_properties,
                                    screenshot_dir, image_name, artifacts):
    """Compares the given image using Skia Gold and uploads the result.

    No uploading is done if the test is being run in local run mode. Compares
    the given screenshot to baselines provided by Gold, raising an Exception if
    a match is not found.

    Args:
      args: Command line options.
      gold_session_manager: Skia Gold session manager.
      gold_session: Skia Gold session.
      gold_properties: Skia Gold properties.
      screenshot_dir: directory where the test stores screenshots.
      image_name: the name of the image being checked.
      artifacts: dictionary of JSON artifacts to pass to the result merger.
    """

    use_luci = not (gold_properties.local_pixel_tests or gold_properties.no_luci_auth)

    # Note: this would be better done by iterating the screenshot directory.
    prefix = SWIFTSHADER_SCREENSHOT_PREFIX if args.swiftshader else DEFAULT_SCREENSHOT_PREFIX
    png_file_name = os.path.join(screenshot_dir, prefix + image_name + '.png')

    if not os.path.isfile(png_file_name):
        logging.info('Screenshot not found, test skipped.')
        return SKIP

    status, error = gold_session.RunComparison(
        name=image_name, png_file=png_file_name, use_luci=use_luci)

    artifact_name = os.path.basename(png_file_name)
    artifacts[artifact_name] = [artifact_name]

    if not status:
        return PASS

    status_codes = gold_session_manager.GetSessionClass().StatusCodes
    if status == status_codes.AUTH_FAILURE:
        logging.error('Gold authentication failed with output %s', error)
    elif status == status_codes.INIT_FAILURE:
        logging.error('Gold initialization failed with output %s', error)
    elif status == status_codes.COMPARISON_FAILURE_REMOTE:
        _, triage_link = gold_session.GetTriageLinks(image_name)
        if not triage_link:
            logging.error('Failed to get triage link for %s, raw output: %s', image_name, error)
            logging.error('Reason for no triage link: %s',
                          gold_session.GetTriageLinkOmissionReason(image_name))
        if gold_properties.IsTryjobRun():
            # Pick "show all results" so we can see the tryjob images by default.
            triage_link += '&master=true'
            artifacts['triage_link_for_entire_cl'] = [triage_link]
        else:
            artifacts['gold_triage_link'] = [triage_link]
    elif status == status_codes.COMPARISON_FAILURE_LOCAL:
        logging.error('Local comparison failed. Local diff files:')
        output_diff_local_files(gold_session, image_name)
    elif status == status_codes.LOCAL_DIFF_FAILURE:
        logging.error(
            'Local comparison failed and an error occurred during diff '
            'generation: %s', error)
        # There might be some files, so try outputting them.
        logging.error('Local diff files:')
        output_diff_local_files(gold_session, image_name)
    else:
        logging.error('Given unhandled SkiaGoldSession StatusCode %s with error %s', status, error)

    return FAIL


def _get_batches(traces, batch_size):
    for i in range(0, len(traces), batch_size):
        yield traces[i:i + batch_size]


def _get_gtest_filter_for_batch(args, batch):
    prefix = SWIFTSHADER_TEST_PREFIX if args.swiftshader else DEFAULT_TEST_PREFIX
    expanded = ['%s%s' % (prefix, trace) for trace in batch]
    return '--gtest_filter=%s' % ':'.join(expanded)


def _run_tests(args, tests, extra_flags, env, screenshot_dir, results, test_results):
    keys = get_skia_gold_keys(args, env)

    with temporary_dir('angle_skia_gold_') as skia_gold_temp_dir:
        gold_properties = angle_skia_gold_properties.ANGLESkiaGoldProperties(args)
        gold_session_manager = angle_skia_gold_session_manager.ANGLESkiaGoldSessionManager(
            skia_gold_temp_dir, gold_properties)
        gold_session = gold_session_manager.GetSkiaGoldSession(keys)

        traces = [trace.split(' ')[0] for trace in tests]

        if args.isolated_script_test_filter:
            filtered = []
            for trace in traces:
                # Apply test filter if present.
                full_name = 'angle_restricted_trace_gold_tests.%s' % trace
                if not fnmatch.fnmatch(full_name, args.isolated_script_test_filter):
                    logging.info('Skipping test %s because it does not match filter %s' %
                                 (full_name, args.isolated_script_test_filter))
                else:
                    filtered += [trace]
            traces = filtered

        batches = _get_batches(traces, args.batch_size)

        for batch in batches:
            for iteration in range(0, args.flaky_retries + 1):
                with common.temporary_file() as tempfile_path:
                    # This is how we signal early exit
                    if not batch:
                        logging.debug('All tests in batch completed.')
                        break
                    if iteration > 0:
                        logging.info('Test run failed, running retry #%d...' % iteration)

                    gtest_filter = _get_gtest_filter_for_batch(args, batch)
                    cmd = [
                        args.test_suite,
                        gtest_filter,
                        '--render-test-output-dir=%s' % screenshot_dir,
                        '--one-frame-only',
                        '--verbose-logging',
                        '--enable-all-trace-tests',
                    ] + extra_flags
                    batch_result = PASS if run_wrapper(args, cmd, env,
                                                       tempfile_path) == 0 else FAIL

                    next_batch = []
                    for trace in batch:
                        artifacts = {}

                        if batch_result == PASS:
                            logging.debug('upload test result: %s' % trace)
                            result = upload_test_result_to_skia_gold(args, gold_session_manager,
                                                                     gold_session, gold_properties,
                                                                     screenshot_dir, trace,
                                                                     artifacts)
                        else:
                            result = batch_result

                        expected_result = SKIP if result == SKIP else PASS
                        test_results[trace] = {'expected': expected_result, 'actual': result}
                        if len(artifacts) > 0:
                            test_results[trace]['artifacts'] = artifacts
                        if result == FAIL:
                            next_batch.append(trace)
                    batch = next_batch

        # These properties are recorded after iteration to ensure they only happen once.
        for _, trace_results in test_results.items():
            result = trace_results['actual']
            results['num_failures_by_type'][result] += 1
            if result == FAIL:
                trace_results['is_unexpected'] = True

        return results['num_failures_by_type'][FAIL] == 0


def _shard_tests(tests, shard_count, shard_index):
    return [tests[index] for index in range(shard_index, len(tests), shard_count)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--isolated-script-test-output', type=str)
    parser.add_argument('--isolated-script-test-perf-output', type=str)
    parser.add_argument('-f', '--isolated-script-test-filter', '--filter', type=str)
    parser.add_argument('--test-suite', help='Test suite to run.', default=DEFAULT_TEST_SUITE)
    parser.add_argument('--render-test-output-dir', help='Directory to store screenshots')
    parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')
    parser.add_argument(
        '--flaky-retries', help='Number of times to retry failed tests.', type=int, default=0)
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
        '--batch-size',
        help='Number of tests to run in a group. Default: %d' % DEFAULT_BATCH_SIZE,
        type=int,
        default=DEFAULT_BATCH_SIZE)
    parser.add_argument(
        '-l', '--log', help='Log output level. Default is %s.' % DEFAULT_LOG, default=DEFAULT_LOG)
    parser.add_argument('--swiftshader', help='Test with SwiftShader.', action='store_true')

    add_skia_gold_args(parser)

    args, extra_flags = parser.parse_known_args()
    logging.basicConfig(level=args.log.upper())

    env = os.environ.copy()

    if 'GTEST_TOTAL_SHARDS' in env and int(env['GTEST_TOTAL_SHARDS']) != 1:
        if 'GTEST_SHARD_INDEX' not in env:
            logging.error('Sharding params must be specified together.')
            sys.exit(1)
        args.shard_count = int(env.pop('GTEST_TOTAL_SHARDS'))
        args.shard_index = int(env.pop('GTEST_SHARD_INDEX'))

    results = {
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

    test_results = {}

    rc = 0

    try:
        if IsWindows():
            args.test_suite = '.\\%s.exe' % args.test_suite
        else:
            args.test_suite = './%s' % args.test_suite

        # read test set
        json_name = os.path.join(ANGLE_SRC_DIR, 'src', 'tests', 'restricted_traces',
                                 'restricted_traces.json')
        with open(json_name) as fp:
            tests = json.load(fp)

        # Split tests according to sharding
        sharded_tests = _shard_tests(tests['traces'], args.shard_count, args.shard_index)

        if args.render_test_output_dir:
            if not _run_tests(args, sharded_tests, extra_flags, env, args.render_test_output_dir,
                              results, test_results):
                rc = 1
        elif 'ISOLATED_OUTDIR' in env:
            if not _run_tests(args, sharded_tests, extra_flags, env, env['ISOLATED_OUTDIR'],
                              results, test_results):
                rc = 1
        else:
            with temporary_dir('angle_trace_') as temp_dir:
                if not _run_tests(args, sharded_tests, extra_flags, env, temp_dir, results,
                                  test_results):
                    rc = 1

    except Exception:
        traceback.print_exc()
        results['interrupted'] = True
        rc = 1

    if test_results:
        results['tests']['angle_restricted_trace_gold_tests'] = test_results

    if args.isolated_script_test_output:
        with open(args.isolated_script_test_output, 'w') as out_file:
            out_file.write(json.dumps(results, indent=2))

    if args.isolated_script_test_perf_output:
        with open(args.isolated_script_test_perf_output, 'w') as out_file:
            out_file.write(json.dumps({}))

    return rc


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
