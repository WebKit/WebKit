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
import re
import subprocess
import sys

from gen_restricted_traces import get_context as get_context

DEFAULT_TEST_SUITE = 'angle_perftests'
DEFAULT_TEST_JSON = 'restricted_traces.json'
DEFAULT_LOG_LEVEL = 'info'

# We preserve select metadata in the trace header that can't be re-captured properly.
# Currently this is just the set of default framebuffer surface config bits.
METADATA_KEYWORDS = ['kDefaultFramebuffer']


def src_trace_path(trace):
    script_dir = os.path.dirname(sys.argv[0])
    return os.path.join(script_dir, trace)


def context_header(trace, trace_path):
    context_id = get_context(trace_path)
    # TODO(jmadill): Remove after retrace. http://anglebug.com/5133
    for try_path_expr in ['%s_capture_context%s.h', '%s_context%s.h']:
        header = try_path_expr % (trace, context_id)
        try_path = os.path.join(trace_path, header)
        if os.path.isfile(try_path):
            return try_path
    logging.fatal('Could not find context header for %s' % trace)
    return None


def get_num_frames(trace):
    trace_path = src_trace_path(trace)

    lo = 99999999
    hi = 0

    for file in os.listdir(trace_path):
        match = re.match(r'.+_context\d_frame(\d+)\.cpp', file)
        if match:
            frame = int(match.group(1))
            if frame < lo:
                lo = frame
            if frame > hi:
                hi = frame

    return hi - lo + 1


def get_trace_metadata(trace):
    trace_path = src_trace_path(trace)
    header_file = context_header(trace, trace_path)
    metadata = []
    with open(header_file, 'rt') as f:
        for line in f.readlines():
            for keyword in METADATA_KEYWORDS:
                if keyword in line:
                    metadata += [line]
    return metadata


def replace_metadata(header_file, metadata):
    lines = []
    replaced = False
    with open(header_file, 'rt') as f:
        for line in f.readlines():
            found_keyword = False
            for keyword in METADATA_KEYWORDS:
                if keyword in line:
                    found_keyword = True
                    break

            if found_keyword:
                if not replaced:
                    replaced = True
                    lines += metadata
            else:
                lines += [line]

    with open(header_file, 'wt') as f:
        f.writelines(lines)


def path_contains_header(path):
    for file in os.listdir(path):
        if fnmatch.fnmatch(file, '*.h'):
            return True
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('gn_path', help='GN build path')
    parser.add_argument('out_path', help='Output directory')
    parser.add_argument('-f', '--filter', help='Trace filter. Defaults to all.', default='*')
    parser.add_argument('-l', '--log', help='Logging level.', default=DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '--no-swiftshader',
        help='Trace against native Vulkan.',
        action='store_true',
        default=False)
    parser.add_argument(
        '-n',
        '--no-overwrite',
        help='Skip traces which already exist in the out directory.',
        action='store_true')
    parser.add_argument(
        '--validation', help='Enable state serialization validation calls.', action='store_true')
    parser.add_argument(
        '--validation-expr',
        help='Validation expression, used to add more validation checkpoints.')
    parser.add_argument(
        '--limit',
        '--frame-limit',
        type=int,
        help='Limits the number of captured frames to produce a shorter trace than the original.')
    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    script_dir = os.path.dirname(sys.argv[0])

    # Load trace names
    with open(os.path.join(script_dir, DEFAULT_TEST_JSON)) as f:
        traces = json.loads(f.read())

    traces = [trace.split(' ')[0] for trace in traces['traces']]

    binary = os.path.join(args.gn_path, DEFAULT_TEST_SUITE)
    if os.name == 'nt':
        binary += '.exe'

    failures = []

    for trace in fnmatch.filter(traces, args.filter):
        logging.debug('Tracing %s' % trace)

        trace_path = os.path.abspath(os.path.join(args.out_path, trace))
        if not os.path.isdir(trace_path):
            os.makedirs(trace_path)
        elif args.no_overwrite and path_contains_header(trace_path):
            logging.info('Skipping "%s" because the out folder already exists' % trace)
            continue

        num_frames = get_num_frames(trace)
        metadata = get_trace_metadata(trace)

        logging.debug('Read metadata: %s' % str(metadata))

        max_steps = min(args.limit, num_frames) if args.limit else num_frames

        # We start tracing from frame 2. --retrace-mode issues a Swap() after Setup() so we can
        # accurately re-trace the MEC.
        additional_env = {
            'ANGLE_CAPTURE_LABEL': trace,
            'ANGLE_CAPTURE_OUT_DIR': trace_path,
            'ANGLE_CAPTURE_FRAME_START': '2',
            'ANGLE_CAPTURE_FRAME_END': str(num_frames + 1),
        }
        if args.validation:
            additional_env['ANGLE_CAPTURE_VALIDATION'] = '1'
            # Also turn on shader output init to ensure we have no undefined values.
            # This feature is also enabled in replay when using --validation.
            additional_env['ANGLE_FEATURE_OVERRIDES_ENABLED'] = 'forceInitShaderOutputVariables'
        if args.validation_expr:
            additional_env['ANGLE_CAPTURE_VALIDATION_EXPR'] = args.validation_expr

        env = {**os.environ.copy(), **additional_env}

        renderer = 'vulkan' if args.no_swiftshader else 'vulkan_swiftshader'

        trace_filter = '--gtest_filter=TracePerfTest.Run/%s_%s' % (renderer, trace)
        run_args = [
            binary,
            trace_filter,
            '--retrace-mode',
            '--max-steps-performed',
            str(max_steps),
            '--enable-all-trace-tests',
        ]

        print('Capturing "%s" (%d frames)...' % (trace, num_frames))
        logging.debug('Running "%s" with environment: %s' %
                      (' '.join(run_args), str(additional_env)))
        try:
            subprocess.check_call(run_args, env=env)

            header_file = context_header(trace, trace_path)

            if not os.path.exists(header_file):
                logging.error('There was a problem tracing "%s", could not find header file: %s' %
                              (trace, header_file))
                failures += [trace]
            else:
                replace_metadata(header_file, metadata)
        except:
            logging.exception('There was an exception running "%s":' % trace)
            failures += [trace]

    if failures:
        print('The following traces failed to re-trace:\n')
        print('\n'.join(['  ' + trace for trace in failures]))
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
