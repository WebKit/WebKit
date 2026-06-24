#!/usr/bin/env vpython3
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Script to validate test registration using GN metadata."""

import argparse
import json
import os
import subprocess
import sys


def _find_root_path():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    while os.path.basename(root_dir) not in ('src', 'chromium'):
        par_dir = os.path.normpath(os.path.join(root_dir, os.pardir))
        if par_dir == root_dir:
            raise RuntimeError('Could not find the repo root.')
        root_dir = par_dir
    return root_dir


def _dump_dep_file(depfile_path, stamp_path, build_dir, root_dir):
    build_files = []
    for walk_root, _, files in os.walk(root_dir):
        if any(d in walk_root for d in ['.git', 'out', 'third_party']):
            continue
        if 'BUILD.gn' in files:
            build_files.append(
                os.path.relpath(os.path.join(walk_root, 'BUILD.gn'),
                                build_dir))

    with open(depfile_path, 'w', encoding='utf-8') as depfile:
        depfile.write(f"{stamp_path}: {' '.join(build_files)}\n")


def _all_rtc_cc_tests(build_dir, root_dir):
    sys.path.append(os.path.join(root_dir, 'build'))
    import find_depot_tools
    gn_path = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py')

    desc_out = subprocess.check_output(
        [
            sys.executable, gn_path, '--quiet', 'desc', '.', '*', 'metadata',
            '--format=json'
        ],
        cwd=build_dir,
        stderr=subprocess.STDOUT).decode('utf-8')

    json_start = desc_out.find('{')
    if json_start == -1:
        all_metadata = {}
    else:
        all_metadata = json.loads(desc_out[json_start:])

    all_tests = set()
    for label, data in all_metadata.items():
        target_metadata = data.get('metadata', {})
        if 'rtc_cc_test' in target_metadata:
            all_tests.add(label)
    return all_tests


def _rtc_cc_tests_in_test_suites(suite_tests_path):
    with open(suite_tests_path, 'r', encoding='utf-8') as suite_tests_file:
        suite_tests_metadata = json.load(suite_tests_file)

    suite_tests = {
        item['label']
        for item in suite_tests_metadata if 'label' in item
    }
    return suite_tests


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--suite-tests', required=True)
    parser.add_argument('--stamp', required=True)
    parser.add_argument('--build-dir', default='.')
    parser.add_argument(
        '--depfile',
        help='Optional depfile to write. Used to track dependencies for '
        'the build system.')
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    root_dir = _find_root_path()

    if args.depfile:
        _dump_dep_file(args.depfile, args.stamp, build_dir, root_dir)

    try:
        all_tests = _all_rtc_cc_tests(build_dir, root_dir)
    except (subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"Error querying GN metadata: {error}")
        return 1

    try:
        suite_tests = _rtc_cc_tests_in_test_suites(args.suite_tests)
    except (IOError, json.JSONDecodeError) as error:
        print(f"Error reading suite tests file: {error}")
        return 1

    orphans = all_tests - suite_tests

    if orphans:
        print("\nERROR: The following rtc_cc_test targets are reachable from "
              "the root build but NOT registered in any rtc_test_suite:")
        for orphan in sorted(list(orphans)):
            print(f"  {orphan}")
        print(
            "\nThey will not run on CQ! Please add them to an rtc_test_suite "
            "in the root BUILD.gn or a relevant sub-directory.\n")
        return 1

    with open(args.stamp, 'w', encoding='utf-8') as stamp_file:
        stamp_file.write('success')

    return 0


if __name__ == '__main__':
    sys.exit(main())
