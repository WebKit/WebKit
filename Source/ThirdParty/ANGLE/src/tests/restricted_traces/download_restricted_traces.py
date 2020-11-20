#!/usr/bin/python3
#
# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# download_restricted_traces.py:
#   download restricted traces and update their timestamp for build

import fnmatch
import json
import os
import subprocess
import sys


def reject_duplicate_keys(pairs):
    found_keys = {}
    for key, value in pairs:
        if key in found_keys:
            raise ValueError("duplicate key: %r" % (key,))
        else:
            found_keys[key] = value
    return found_keys


def read_json(json_file):
    with open(json_file) as map_file:
        return json.loads(map_file.read(), object_pairs_hook=reject_duplicate_keys)


def run_command(command):
    env = os.environ.copy()
    env['PYTHONUNBUFFERED'] = '1'
    with subprocess.Popen(
            command, stdout=subprocess.PIPE, universal_newlines=True, env=env) as proc:
        while proc.poll() is None:
            out = proc.stdout.read(1)
            sys.stdout.write(out)
            sys.stdout.flush()
        if proc.returncode:
            raise subprocess.CalledProcessError(proc.returncode, cmd)


def main():
    if (len(sys.argv) != 2):
        print('Missing restricted traces directory')
        return 1

    trace_dir = sys.argv[1]

    # issue download command
    download_script = 'download_from_google_storage'
    if os.name == 'nt':
        download_script += '.bat'
    cmd = [
        download_script,
        '--directory',
        '--recursive',
        '--extract',
        '--num_threads=4',
        '--bucket',
        'chrome-angle-capture-binaries',
        trace_dir,
    ]

    run_command(cmd)

    json_file = os.path.join(trace_dir, 'restricted_traces.json')

    json_data = read_json(json_file)
    if 'traces' not in json_data:
        print('Trace data missing traces key.')
        return 1

    traces = json_data['traces']

    for trace in traces:
        for dirname, dirnames, filenames in os.walk(os.path.join(trace_dir, trace)):
            # update timestamp on directory
            os.utime(dirname, None)

            # update timestamp on the files
            for filename in filenames:
                target = os.path.join(trace_dir, trace, filename)
                os.utime(target, None)

    return 0


if __name__ == '__main__':
    sys.exit(main())
