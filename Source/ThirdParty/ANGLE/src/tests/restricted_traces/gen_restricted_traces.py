#!/usr/bin/python3
#
# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# gen_restricted_traces.py:
#   Generates integration code for the restricted trace tests.

import getpass
import glob
import fnmatch
import re
import json
import os
import sys

CIPD_TRACE_PREFIX = 'angle/traces'
EXPERIMENTAL_CIPD_PREFIX = 'experimental/google.com/%s/angle/traces'
DEPS_PATH = '../../../DEPS'
DEPS_START = '# === ANGLE Restricted Trace Generated Code Start ==='
DEPS_END = '# === ANGLE Restricted Trace Generated Code End ==='
DEPS_TEMPLATE = """\
  'src/tests/restricted_traces/{trace}': {{
      'packages': [
        {{
            'package': '{trace_prefix}/{trace}',
            'version': 'version:{version}',
        }},
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  }},
"""


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


def write_json(json_file, data):
    with open(json_file, 'w') as map_file:
        json.dump(data, map_file, indent=4, sort_keys=True)


def update_deps(trace_pairs):
    # Generate substitution string
    replacement = ""
    for (trace, version) in trace_pairs:
        if 'x' in version:
            version = version.strip('x')
            trace_prefix = EXPERIMENTAL_CIPD_PREFIX % getpass.getuser()
        else:
            trace_prefix = CIPD_TRACE_PREFIX
        sub = {'trace': trace, 'version': version, 'trace_prefix': trace_prefix}
        replacement += DEPS_TEMPLATE.format(**sub)

    # Update DEPS to download CIPD dependencies
    new_deps = ""
    with open(DEPS_PATH) as f:
        in_deps = False
        for line in f:
            if in_deps:
                if DEPS_END in line:
                    new_deps += replacement
                    new_deps += line
                    in_deps = False
            else:
                if DEPS_START in line:
                    new_deps += line
                    in_deps = True
                else:
                    new_deps += line
        f.close()

    with open(DEPS_PATH, 'w') as f:
        f.write(new_deps)
        f.close()

    return True


def main():
    json_file = 'restricted_traces.json'

    json_data = read_json(json_file)
    if 'traces' not in json_data:
        print('Trace data missing traces key.')
        return 1
    trace_pairs = [trace.split(' ') for trace in json_data['traces']]
    traces = [trace_pair[0] for trace_pair in trace_pairs]

    # auto_script parameters.
    if len(sys.argv) > 1:
        inputs = [json_file]

        # Note: we do not include DEPS in the list of outputs to simplify the integration.
        # Otherwise we'd continually need to regenerate on any roll. We include .gitignore
        # in the outputs so we have a placeholder value.
        outputs = ['.gitignore']

        if sys.argv[1] == 'inputs':
            print(','.join(inputs))
        elif sys.argv[1] == 'outputs':
            print(','.join(outputs))
        else:
            print('Invalid script parameters.')
            return 1
        return 0

    if not update_deps(trace_pairs):
        print('DEPS file update failed')
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
