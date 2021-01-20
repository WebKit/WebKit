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
    header = '%s_capture_context%s.h' % (trace, context_id)
    return os.path.join(trace_path, header)


def get_num_frames(trace):

    trace_path = src_trace_path(trace)

    lo = 99999999
    hi = 0

    for file in os.listdir(trace_path):
        match = re.match(r'.+_capture_context\d_frame(\d+)\.cpp', file)
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
    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    script_dir = os.path.dirname(sys.argv[0])

    # Load trace names
    with open(os.path.join(script_dir, DEFAULT_TEST_JSON)) as f:
        traces = json.loads(f.read())

    traces = traces['traces']

    binary = os.path.join(args.gn_path, DEFAULT_TEST_SUITE)
    if os.name == 'nt':
        binary += '.exe'

    failures = []

    for trace in fnmatch.filter(traces, args.filter):
        logging.debug('Tracing %s' % trace)

        trace_path = os.path.abspath(os.path.join(args.out_path, trace))
        if not os.path.isdir(trace_path):
            os.makedirs(trace_path)

        num_frames = get_num_frames(trace)
        metadata = get_trace_metadata(trace)

        logging.debug('Read metadata: %s' % str(metadata))

        env = os.environ.copy()
        env['ANGLE_CAPTURE_OUT_DIR'] = trace_path
        env['ANGLE_CAPTURE_LABEL'] = trace
        env['ANGLE_CAPTURE_TRIGGER'] = str(num_frames)

        renderer = 'vulkan' if args.no_swiftshader else 'vulkan_swiftshader'

        trace_filter = '--gtest_filter=TracePerfTest.Run/%s_%s' % (renderer, trace)
        run_args = [
            binary,
            trace_filter,
            '--no-warmup',
            '--trials',
            '1',
            '--start-trace-after-setup',
            '--steps',
            str(num_frames),
            '--enable-all-trace-tests',
        ]

        print('Capturing %s (%d frames)...' % (trace, num_frames))
        logging.debug('Running %s with capture environment' % ' '.join(run_args))
        subprocess.check_call(run_args, env=env)

        header_file = context_header(trace, trace_path)

        if not os.path.exists(header_file):
            logging.warning('There was a problem tracing %s, could not find header file' % trace)
            failures += [trace]
        else:
            replace_metadata(header_file, metadata)

    if failures:
        print('The following traces failed to re-trace:\n')
        print('\n'.join(['  ' + trace for trace in failures]))

    return 0


if __name__ == '__main__':
    sys.exit(main())
