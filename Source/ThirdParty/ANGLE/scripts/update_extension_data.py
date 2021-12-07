#!/usr/bin/python3
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# update_extension_data.py:
#   Downloads and updates auto-generated extension data.

import argparse
import logging
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

EXIT_SUCCESS = 0
EXIT_FAILURE = 1

TEST_SUITE = 'angle_end2end_tests'
BUILDERS = ['angle/ci/android-arm64-rel', 'angle/ci/win-clang-x64-rel', 'angle/ci/linux-clang-rel']
SWARMING_SERVER = 'chromium-swarm.appspot.com'

d = os.path.dirname
THIS_DIR = d(os.path.abspath(__file__))
ANGLE_ROOT_DIR = d(THIS_DIR)

# Host GPUs
INTEL_630 = '8086:5912'
NVIDIA_P400 = '10de:1cb3'
GPUS = [INTEL_630, NVIDIA_P400]
GPU_NAME_MAP = {INTEL_630: 'intel_630', NVIDIA_P400: 'nvidia_p400'}

# OSes
LINUX = 'Linux'
WINDOWS_10 = 'Windows-10'
BOT_OSES = [LINUX, WINDOWS_10]
BOT_OS_NAME_MAP = {LINUX: 'linux', WINDOWS_10: 'win10'}

# Devices
PIXEL_4 = 'flame'
DEVICES_TYPES = [PIXEL_4]
DEVICE_NAME_MAP = {PIXEL_4: 'pixel_4'}

# Device OSes
ANDROID_11 = 'R'
DEVICE_OSES = [ANDROID_11]
DEVICE_OS_NAME_MAP = {ANDROID_11: 'android_11'}

# Result names
INFO_FILES = [
    'GLinfo_ES3_2_Vulkan.json',
    'GLinfo_ES3_1_Vulkan.json',
]

LOG_LEVELS = ['WARNING', 'INFO', 'DEBUG']


def run_and_get_json(args):
    logging.debug(' '.join(args))
    output = subprocess.check_output(args)
    return json.loads(output)


def get_bb():
    return 'bb.bat' if os.name == 'nt' else 'bb'


def run_bb_and_get_output(*args):
    bb_args = [get_bb()] + list(args)
    return subprocess.check_output(bb_args, encoding='utf-8')


def run_bb_and_get_json(*args):
    bb_args = [get_bb()] + list(args) + ['-json']
    return run_and_get_json(bb_args)


def get_swarming():
    swarming_bin = 'swarming.exe' if os.name == 'nt' else 'swarming'
    return os.path.join(ANGLE_ROOT_DIR, 'tools', 'luci-go', swarming_bin)


def run_swarming(*args):
    swarming_args = [get_swarming()] + list(args)
    logging.debug(' '.join(swarming_args))
    subprocess.check_call(swarming_args)


def run_swarming_and_get_json(*args):
    swarming_args = [get_swarming()] + list(args)
    return run_and_get_json(swarming_args)


def name_device(gpu, device_type):
    if gpu:
        return GPU_NAME_MAP[gpu]
    else:
        assert device_type
        return DEVICE_NAME_MAP[device_type]


def name_os(bot_os, device_os):
    if bot_os:
        return BOT_OS_NAME_MAP[bot_os]
    else:
        assert device_os
        return DEVICE_OS_NAME_MAP[device_os]


def get_props_string(gpu, bot_os, device_os, device_type):
    d = {'gpu': gpu, 'os': bot_os, 'device os': device_os, 'device': device_type}
    return ', '.join('%s %s' % (k, v) for (k, v) in d.items() if v)


def collect_task_and_update_json(task_id, gpu, bot_os, device_os, device_type):
    logging.info('Found task with ID: %s, %s' %
                 (task_id, get_props_string(gpu, bot_os, device_os, device_type)))
    target_file_name = '%s_%s.json' % (name_device(gpu, device_type), name_os(bot_os, device_os))
    target_file = os.path.join(THIS_DIR, 'extension_data', target_file_name)
    with tempfile.TemporaryDirectory() as tempdirname:
        run_swarming('collect', '-S', SWARMING_SERVER, '-output-dir=%s' % tempdirname, task_id)
        task_dir = os.path.join(tempdirname, task_id)
        found = False
        for fname in os.listdir(task_dir):
            if fname in INFO_FILES:
                if found:
                    logging.warning('Multiple candidates found for %s' % target_file_name)
                    return
                else:
                    logging.info('%s -> %s' % (fname, target_file))
                    found = True
                    source_file = os.path.join(task_dir, fname)
                    shutil.copy(source_file, target_file)


def get_intersect_or_none(list_a, list_b):
    i = [v for v in list_a if v in list_b]
    assert not i or len(i) == 1
    return i[0] if i else None


def main():
    parser = argparse.ArgumentParser(description='Pulls extension support data from ANGLE CI.')
    parser.add_argument(
        '-v', '--verbose', help='Print additional debugging into.', action='count', default=0)
    args = parser.parse_args()

    if args.verbose >= len(LOG_LEVELS):
        args.verbose = len(LOG_LEVELS) - 1
    logging.basicConfig(level=LOG_LEVELS[args.verbose])

    name_expr = re.compile(r'^' + TEST_SUITE + r' on (.*) on (.*)$')

    for builder in BUILDERS:

        # Step 1: Find the build ID.
        # We list two builds using 'bb ls' and take the second, to ensure the build is finished.
        ls_output = run_bb_and_get_output('ls', builder, '-n', '2', '-id')
        build_id = ls_output.splitlines()[1]
        logging.info('%s: build id %s' % (builder, build_id))

        # Step 2: Get the test suite swarm hashes.
        # 'bb get' returns build properties, including cloud storage identifiers for this test suite.
        get_json = run_bb_and_get_json('get', build_id, '-p')
        test_suite_hash = get_json['output']['properties']['swarm_hashes'][TEST_SUITE]
        logging.info('Found swarm hash: %s' % test_suite_hash)

        # Step 3: Find all tasks using the swarm hashes.
        # 'swarming tasks' can find instances of the test suite that ran on specific systems.
        task_json = run_swarming_and_get_json('tasks', '-tag', 'data:%s' % test_suite_hash, '-S',
                                              SWARMING_SERVER)

        # Step 4: Download the extension data for each configuration we're monitoring.
        # 'swarming collect' downloads test artifacts to a temporary directory.
        for task in task_json:
            gpu = None
            bot_os = None
            device_os = None
            device_type = None
            for bot_dim in task['bot_dimensions']:
                if bot_dim['key'] == 'gpu':
                    logging.debug(bot_dim['value'])
                    gpu = get_intersect_or_none(GPUS, bot_dim['value'])
                if bot_dim['key'] == 'os':
                    logging.debug(bot_dim['value'])
                    bot_os = get_intersect_or_none(BOT_OSES, bot_dim['value'])
                if bot_dim['key'] == 'device_os':
                    logging.debug(bot_dim['value'])
                    device_os = get_intersect_or_none(DEVICE_OSES, bot_dim['value'])
                if bot_dim['key'] == 'device_type':
                    logging.debug(bot_dim['value'])
                    device_type = get_intersect_or_none(DEVICES_TYPES, bot_dim['value'])
            if (gpu or device_type) and (bot_os or device_os):
                collect_task_and_update_json(task['task_id'], gpu, bot_os, device_os, device_type)

    return EXIT_SUCCESS


if __name__ == '__main__':
    sys.exit(main())
