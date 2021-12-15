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
import itertools
import json
import multiprocessing
import os
import platform
import signal
import subprocess
import sys

CIPD_PREFIX = 'angle/traces'
EXPERIMENTAL_CIPD_PREFIX = 'experimental/google.com/%s/angle/traces'
LOG_LEVEL = 'info'
JSON_PATH = 'restricted_traces.json'
SCRIPT_DIR = os.path.dirname(sys.argv[0])
MAX_THREADS = 8
LONG_TIMEOUT = 100000

EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def cipd(logger, *args):
    logger.debug('running cipd with args: %s', ' '.join(args))
    exe = 'cipd.bat' if platform.system() == 'Windows' else 'cipd'
    try:
        completed = subprocess.run([exe] + list(args), stderr=subprocess.STDOUT)
    except KeyboardInterrupt:
        pass
    if completed.stdout:
        logger.debug('cipd stdout:\n%s' % completed.stdout)
    return completed.returncode


def sync_trace(param):
    args, trace_info = param
    logger = args.logger
    trace, trace_version = trace_info.split(' ')

    if args.filter and not fnmatch.fnmatch(trace, args.filter):
        logger.debug('Skipping %s because it does not match the test filter.' % trace)
        return EXIT_SUCCESS

    if 'x' in trace_version:
        trace_prefix = EXPERIMENTAL_CIPD_PREFIX % getpass.getuser()
        trace_version = trace_version.strip('x')
    else:
        trace_prefix = CIPD_PREFIX

    trace_name = '%s/%s' % (trace_prefix, trace)
    # Determine if this version exists
    if cipd(logger, 'describe', trace_name, '-version', 'version:%s' % trace_version) == 0:
        logger.info('%s version %s already present' % (trace, trace_version))
        return EXIT_SUCCESS

    logger.info('%s version %s missing. calling create.' % (trace, trace_version))
    trace_folder = os.path.join(SCRIPT_DIR, trace)
    if cipd(logger, 'create', '-name', trace_name, '-in', trace_folder, '-tag', 'version:%s' %
            trace_version, '-log-level', args.log.lower(), '-install-mode', 'copy') != 0:
        logger.error('%s version %s create failed' % (trace, trace_version))
        return EXIT_FAILURE
    return EXIT_SUCCESS


def main(args):
    args.logger = multiprocessing.log_to_stderr()
    args.logger.setLevel(level=args.log.upper())

    with open(os.path.join(SCRIPT_DIR, JSON_PATH)) as f:
        traces = json.loads(f.read())

    zipped_args = zip(itertools.repeat(args), traces['traces'])

    if args.threads > 1:
        pool = multiprocessing.Pool(args.threads)
        try:
            retval = pool.map_async(sync_trace, zipped_args).get(LONG_TIMEOUT)
        except KeyboardInterrupt:
            pool.terminate()
        except Exception as e:
            print('got exception: %r, terminating the pool' % (e,))
            pool.terminate()
        pool.join()
    else:
        retval = map(sync_trace, zipped_args)

    return EXIT_FAILURE if EXIT_FAILURE in retval else EXIT_SUCCESS


if __name__ == '__main__':
    max_threads = min(multiprocessing.cpu_count(), MAX_THREADS)
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-p', '--prefix', help='CIPD Prefix. Default: %s' % CIPD_PREFIX, default=CIPD_PREFIX)
    parser.add_argument(
        '-l', '--log', help='Logging level. Default: %s' % LOG_LEVEL, default=LOG_LEVEL)
    parser.add_argument(
        '-f', '--filter', help='Only sync specified tests. Supports fnmatch expressions.')
    parser.add_argument(
        '-t',
        '--threads',
        help='Maxiumum parallel threads. Default: %s' % max_threads,
        default=max_threads)
    args, extra_flags = parser.parse_known_args()

    logging.basicConfig(level=args.log.upper())

    sys.exit(main(args))
