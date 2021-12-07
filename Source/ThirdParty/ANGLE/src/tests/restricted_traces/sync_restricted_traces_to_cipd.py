#!/usr/bin/python3
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# sync_restricted_traces_to_cipd.py:
#   Ensures the restricted traces are uploaded to CIPD. Versions are encoded in
#   restricted_traces.json. Requires access to the CIPD path to work.

import argparse
import getpass
import fnmatch
import logging
import json
import os
import platform
import subprocess
import sys

CIPD_PREFIX = 'angle/traces'
EXPERIMENTAL_CIPD_PREFIX = 'experimental/google.com/%s/angle/traces'
LOG_LEVEL = 'info'
JSON_PATH = 'restricted_traces.json'
SCRIPT_DIR = os.path.dirname(sys.argv[0])


def cipd(*args):
    logging.debug('running cipd with args: %s', ' '.join(args))
    exe = 'cipd.bat' if platform.system() == 'Windows' else 'cipd'
    completed = subprocess.run([exe] + list(args), stderr=subprocess.STDOUT)
    if completed.stdout:
        logging.debug('cipd stdout:\n%s' % completed.stdout)
    return completed.returncode


def main(args):
    with open(os.path.join(SCRIPT_DIR, JSON_PATH)) as f:
        traces = json.loads(f.read())

    for trace_info in traces['traces']:
        trace, trace_version = trace_info.split(' ')

        if args.filter and not fnmatch.fnmatch(trace, args.filter):
            logging.debug('Skipping %s because it does not match the test filter.' % trace)
            continue

        if 'x' in trace_version:
            trace_prefix = EXPERIMENTAL_CIPD_PREFIX % getpass.getuser()
            trace_version = trace_version.strip('x')
        else:
            trace_prefix = CIPD_PREFIX

        trace_name = '%s/%s' % (trace_prefix, trace)
        # Determine if this version exists
        if cipd('describe', trace_name, '-version', 'version:%s' % trace_version) == 0:
            logging.info('%s version %s already present' % (trace, trace_version))
            continue

        logging.info('%s version %s missing. calling create.' % (trace, trace_version))
        trace_folder = os.path.join(SCRIPT_DIR, trace)
        if cipd('create', '-name', trace_name, '-in', trace_folder, '-tag', 'version:%s' %
                trace_version, '-log-level', args.log.lower(), '-install-mode', 'copy') != 0:
            logging.error('%s version %s create failed' % (trace, trace_version))
            return 1

    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-p', '--prefix', help='CIPD Prefix. Default: %s' % CIPD_PREFIX, default=CIPD_PREFIX)
    parser.add_argument(
        '-l', '--log', help='Logging level. Default: %s' % LOG_LEVEL, default=LOG_LEVEL)
    parser.add_argument(
        '-f', '--filter', help='Only sync specified tests. Supports fnmatch expressions.')
    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    sys.exit(main(args))
