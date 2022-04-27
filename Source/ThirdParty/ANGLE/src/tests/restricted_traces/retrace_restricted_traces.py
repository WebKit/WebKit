#! /usr/bin/env python3
#
# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
'''
Script that re-captures the traces in the restricted trace folder. We can
use this to update traces without needing to re-run the app on a device.
'''

import argparse
import fnmatch
import json
import logging
import os
import shutil
import stat
import subprocess
import sys

from gen_restricted_traces import read_json as read_json

DEFAULT_TEST_SUITE = 'angle_perftests'
DEFAULT_TEST_JSON = 'restricted_traces.json'
DEFAULT_LOG_LEVEL = 'info'
DEFAULT_BACKUP_FOLDER = 'retrace-backups'

EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def get_trace_json_path(trace):
    return os.path.join(get_script_dir(), trace, f'{trace}.json')


def load_trace_json(trace):
    json_file_name = get_trace_json_path(trace)
    return read_json(json_file_name)


def get_context(trace):
    """Returns the trace context number."""
    json_data = load_trace_json(trace)
    return str(json_data['WindowSurfaceContextID'])


def get_script_dir():
    return os.path.dirname(sys.argv[0])


def context_header(trace, trace_path):
    context_id = get_context(trace)
    header = '%s_context%s.h' % (trace, context_id)
    return os.path.join(trace_path, header)


def src_trace_path(trace):
    return os.path.join(get_script_dir(), trace)


def get_num_frames(json_data):
    metadata = json_data['TraceMetadata']
    return metadata['FrameEnd'] - metadata['FrameStart'] + 1


def path_contains_header(path):
    for file in os.listdir(path):
        if fnmatch.fnmatch(file, '*.h'):
            return True
    return False


def chmod_directory(directory, perm):
    assert os.path.isdir(directory)
    for file in os.listdir(directory):
        fn = os.path.join(directory, file)
        os.chmod(fn, perm)


def ensure_rmdir(directory):
    if os.path.isdir(directory):
        chmod_directory(directory, stat.S_IWRITE)
        shutil.rmtree(directory)


def copy_trace_folder(old_path, new_path):
    logging.info('%s -> %s' % (old_path, new_path))
    ensure_rmdir(new_path)
    shutil.copytree(old_path, new_path)


def backup_traces(args, traces):
    for trace in fnmatch.filter(traces, args.traces):
        trace_path = src_trace_path(trace)
        trace_backup_path = os.path.join(args.out_path, trace)
        copy_trace_folder(trace_path, trace_backup_path)


def restore_traces(args, traces):
    for trace in fnmatch.filter(traces, args.traces):
        trace_path = src_trace_path(trace)
        trace_backup_path = os.path.join(args.out_path, trace)
        if not os.path.isdir(trace_backup_path):
            logging.error('Trace folder not found at %s' % trace_backup_path)
        else:
            copy_trace_folder(trace_backup_path, trace_path)


def run_autoninja(args):
    autoninja_binary = 'autoninja'
    if os.name == 'nt':
        autoninja_binary += '.bat'

    autoninja_args = [autoninja_binary, '-C', args.gn_path, args.test_suite]
    logging.debug('Calling %s' % ' '.join(autoninja_args))
    subprocess.check_call(autoninja_args)


def run_test_suite(args, trace, max_steps, additional_args, additional_env):
    trace_binary = os.path.join(args.gn_path, args.test_suite)
    if os.name == 'nt':
        trace_binary += '.exe'

    renderer = 'vulkan' if args.no_swiftshader else 'vulkan_swiftshader'
    trace_filter = '--gtest_filter=TracePerfTest.Run/%s_%s' % (renderer, trace)
    run_args = [
        trace_binary,
        trace_filter,
        '--max-steps-performed',
        str(max_steps),
    ] + additional_args
    if not args.no_swiftshader:
        run_args += ['--enable-all-trace-tests']

    env = {**os.environ.copy(), **additional_env}
    env_string = ' '.join(['%s=%s' % item for item in additional_env.items()])
    if env_string:
        env_string += ' '

    logging.info('%s%s' % (env_string, ' '.join(run_args)))
    subprocess.check_call(run_args, env=env)


def upgrade_traces(args, traces):
    run_autoninja(args)

    failures = []

    for trace in fnmatch.filter(traces, args.traces):
        logging.debug('Tracing %s' % trace)

        trace_path = os.path.abspath(os.path.join(args.out_path, trace))
        if not os.path.isdir(trace_path):
            os.makedirs(trace_path)
        elif args.no_overwrite and path_contains_header(trace_path):
            logging.info('Skipping "%s" because the out folder already exists' % trace)
            continue

        json_data = load_trace_json(trace)
        num_frames = get_num_frames(json_data)

        metadata = json_data['TraceMetadata']
        logging.debug('Read metadata: %s' % str(metadata))

        max_steps = min(args.limit, num_frames) if args.limit else num_frames

        # We start tracing from frame 2. --retrace-mode issues a Swap() after Setup() so we can
        # accurately re-trace the MEC.
        additional_env = {
            'ANGLE_CAPTURE_LABEL': trace,
            'ANGLE_CAPTURE_OUT_DIR': trace_path,
            'ANGLE_CAPTURE_FRAME_START': '2',
            'ANGLE_CAPTURE_FRAME_END': str(max_steps + 1),
        }
        if args.validation:
            additional_env['ANGLE_CAPTURE_VALIDATION'] = '1'
            # Also turn on shader output init to ensure we have no undefined values.
            # This feature is also enabled in replay when using --validation.
            additional_env[
                'ANGLE_FEATURE_OVERRIDES_ENABLED'] = 'allocateNonZeroMemory:forceInitShaderVariables'
        if args.validation_expr:
            additional_env['ANGLE_CAPTURE_VALIDATION_EXPR'] = args.validation_expr
        if args.trim:
            additional_env['ANGLE_CAPTURE_TRIM_ENABLED'] = '1'
        if args.no_trim:
            additional_env['ANGLE_CAPTURE_TRIM_ENABLED'] = '0'

        additional_args = ['--retrace-mode']

        try:
            run_test_suite(args, trace, max_steps, additional_args, additional_env)

            json_file = get_trace_json_path(trace)
            if not os.path.exists(json_file):
                logging.error(
                    f'There was a problem tracing "{trace}", could not find json file: {json_file}'
                )
                failures += [trace]
        except:
            logging.exception('There was an exception running "%s":' % trace)
            failures += [trace]

    if failures:
        print('The following traces failed to upgrade:\n')
        print('\n'.join(['  ' + trace for trace in failures]))
        return EXIT_FAILURE

    return EXIT_SUCCESS


def validate_traces(args, traces):
    restore_traces(args, traces)
    run_autoninja(args)

    additional_args = ['--validation']
    additional_env = {
        'ANGLE_FEATURE_OVERRIDES_ENABLED': 'allocateNonZeroMemory:forceInitShaderVariables'
    }

    failures = []

    for trace in fnmatch.filter(traces, args.traces):
        json_data = load_trace_json(trace)
        num_frames = get_num_frames(json_data)
        max_steps = min(args.limit, num_frames) if args.limit else num_frames
        try:
            run_test_suite(args, trace, max_steps, additional_args, additional_env)
        except:
            logging.error('There was a failure running "%s".' % trace)
            failures += [trace]

    if failures:
        print('The following traces failed to validate:\n')
        print('\n'.join(['  ' + trace for trace in failures]))
        return EXIT_FAILURE

    return EXIT_SUCCESS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--log', help='Logging level.', default=DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '--test-suite',
        help='Test Suite. Default is %s' % DEFAULT_TEST_SUITE,
        default=DEFAULT_TEST_SUITE)
    parser.add_argument(
        '--no-swiftshader',
        help='Trace against native Vulkan.',
        action='store_true',
        default=False)

    subparsers = parser.add_subparsers(dest='command', required=True, help='Command to run.')

    backup_parser = subparsers.add_parser(
        'backup', help='Copies trace contents into a saved folder.')
    backup_parser.add_argument(
        'traces', help='Traces to back up. Supports fnmatch expressions.', default='*')
    backup_parser.add_argument(
        '-o',
        '--out-path',
        '--backup-path',
        help='Destination folder. Default is "%s".' % DEFAULT_BACKUP_FOLDER,
        default=DEFAULT_BACKUP_FOLDER)

    restore_parser = subparsers.add_parser(
        'restore', help='Copies traces from a saved folder to the trace folder.')
    restore_parser.add_argument(
        '-o',
        '--out-path',
        '--backup-path',
        help='Path the traces were saved. Default is "%s".' % DEFAULT_BACKUP_FOLDER,
        default=DEFAULT_BACKUP_FOLDER)
    restore_parser.add_argument(
        'traces', help='Traces to restore. Supports fnmatch expressions.', default='*')

    upgrade_parser = subparsers.add_parser(
        'upgrade', help='Re-trace existing traces, upgrading the format.')
    upgrade_parser.add_argument('gn_path', help='GN build path')
    upgrade_parser.add_argument('out_path', help='Output directory')
    upgrade_parser.add_argument(
        '-f', '--traces', '--filter', help='Trace filter. Defaults to all.', default='*')
    upgrade_parser.add_argument(
        '-n',
        '--no-overwrite',
        help='Skip traces which already exist in the out directory.',
        action='store_true')
    upgrade_parser.add_argument(
        '--validation', help='Enable state serialization validation calls.', action='store_true')
    upgrade_parser.add_argument(
        '--validation-expr',
        help='Validation expression, used to add more validation checkpoints.')
    upgrade_parser.add_argument(
        '--limit',
        '--frame-limit',
        type=int,
        help='Limits the number of captured frames to produce a shorter trace than the original.')
    upgrade_parser.add_argument(
        '--trim', action='store_true', help='Enables trace trimming. Breaks replay validation.')
    upgrade_parser.add_argument(
        '--no-trim', action='store_true', help='Disables trace trimming. Useful for validation.')
    upgrade_parser.set_defaults(trim=True)

    validate_parser = subparsers.add_parser(
        'validate', help='Runs the an updated test suite with validation enabled.')
    validate_parser.add_argument('gn_path', help='GN build path')
    validate_parser.add_argument('out_path', help='Path to the upgraded trace folder.')
    validate_parser.add_argument(
        'traces', help='Traces to validate. Supports fnmatch expressions.', default='*')
    validate_parser.add_argument(
        '--limit', '--frame-limit', type=int, help='Limits the number of tested frames.')

    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    # Load trace names
    with open(os.path.join(get_script_dir(), DEFAULT_TEST_JSON)) as f:
        traces = json.loads(f.read())

    traces = [trace.split(' ')[0] for trace in traces['traces']]

    if args.command == 'backup':
        return backup_traces(args, traces)
    elif args.command == 'restore':
        return restore_traces(args, traces)
    elif args.command == 'upgrade':
        return upgrade_traces(args, traces)
    elif args.command == 'validate':
        return validate_traces(args, traces)
    else:
        logging.fatal('Unknown command: %s' % args.command)
        return EXIT_FAILURE


if __name__ == '__main__':
    sys.exit(main())
