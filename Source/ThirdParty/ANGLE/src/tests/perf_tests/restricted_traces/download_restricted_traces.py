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


def main():
    if (len(sys.argv) != 2):
        print('Missing restricted traces directory')
        return 1

    trace_dir = sys.argv[1]

    # issue download command
    cmd = [
        'download_from_google_storage',
        '--directory',
        '--recursive',
        '--extract',
        '--bucket',
        'chrome-angle-capture-binaries',
        trace_dir,
    ]

    os.system(" ".join(cmd))

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
