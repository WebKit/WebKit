#!/usr/bin/python3
#
# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# trigger.py:
#   Helper script for triggering GPU tests on swarming.

import argparse
import json
import hashlib
import logging
import os
import re
import subprocess
import sys

# This is the same as the trybots.
DEFAULT_TASK_PRIORITY = 30
DEFAULT_POOL = 'chromium.tests.gpu'
DEFAULT_LOG_LEVEL = 'info'
DEFAULT_REALM = 'chromium:try'
GOLD_SERVICE_ACCOUNT = 'chrome-gpu-gold@chops-service-accounts.iam.gserviceaccount.com'


def parse_args():
    parser = argparse.ArgumentParser(os.path.basename(sys.argv[0]))
    parser.add_argument('gn_path', help='path to GN. (e.g. out/Release)')
    parser.add_argument('test', help='test name. (e.g. angle_end2end_tests)')
    parser.add_argument('os_dim', help='OS dimension. (e.g. Windows-10)')
    parser.add_argument('-s', '--shards', default=1, help='number of shards', type=int)
    parser.add_argument(
        '-p', '--pool', default=DEFAULT_POOL, help='swarming pool, default is %s.' % DEFAULT_POOL)
    parser.add_argument('-g', '--gpu', help='GPU dimension. (e.g. intel-hd-630-win10-stable)')
    parser.add_argument('-t', '--device-type', help='Android device type (e.g. bullhead)')
    parser.add_argument('-o', '--device-os', help='Android OS.')
    parser.add_argument(
        '-l',
        '--log',
        default=DEFAULT_LOG_LEVEL,
        help='Log level. Default is %s.' % DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '--gold', action='store_true', help='Use swarming arguments for Gold tests.')
    parser.add_argument(
        '--priority',
        help='Task priority. Default is %s. Use judiciously.' % DEFAULT_TASK_PRIORITY,
        default=DEFAULT_TASK_PRIORITY)
    parser.add_argument(
        '-e',
        '--env',
        action='append',
        default=[],
        help='Environment variables. Can be specified multiple times.')

    return parser.parse_known_args()


def invoke_mb(args):
    mb_script_path = os.path.join('tools', 'mb', 'mb.py')
    mb_args = [sys.executable, mb_script_path] + args

    # Attempt to detect standalone vs chromium component build.
    is_standalone = not os.path.isdir(os.path.join('third_party', 'angle'))

    if is_standalone:
        logging.info('Standalone mode detected.')
        mb_args += ['-i', os.path.join('infra', 'specs', 'gn_isolate_map.pyl')]

    logging.info('Invoking mb: %s' % ' '.join(mb_args))
    return subprocess.check_output(mb_args)


def main():
    args, unknown = parse_args()

    logging.basicConfig(level=args.log.upper())

    path = args.gn_path.replace('\\', '/')
    out_gn_path = '//' + path
    out_file_path = os.path.join(*path.split('/'))

    get_command_output = invoke_mb(['get-swarming-command', out_gn_path, args.test, '--as-list'])
    swarming_cmd = json.loads(get_command_output)
    logging.info('Swarming command: %s' % ' '.join(swarming_cmd))

    invoke_mb(['isolate', out_gn_path, args.test])

    isolate_cmd_path = os.path.join('tools', 'luci-go', 'isolate')
    isolate_file = os.path.join(out_file_path, '%s.isolate' % args.test)
    archive_file = os.path.join(out_file_path, '%s.archive.json' % args.test)

    isolate_args = [
        isolate_cmd_path, 'archive', '-i', isolate_file, '-cas-instance', 'chromium-swarm',
        '-dump-json', archive_file
    ]
    logging.info('Invoking isolate: %s' % ' '.join(isolate_args))
    subprocess.check_call(isolate_args)
    with open(archive_file) as f:
        digest = json.load(f).get(args.test)

    logging.info('Got an CAS digest %s' % digest)
    swarming_script_path = os.path.join('tools', 'luci-go', 'swarming')

    swarming_args = [
        swarming_script_path, 'trigger', '-S', 'chromium-swarm.appspot.com', '-d',
        'os=' + args.os_dim, '-d', 'pool=' + args.pool, '-digest', digest
    ]

    # Set priority. Don't abuse this!
    swarming_args += ['-priority', str(args.priority), '-realm', DEFAULT_REALM]

    # Define a user tag.
    try:
        whoami = subprocess.check_output(['whoami'])
        # Strip extra stuff (e.g. on Windows we are 'hostname\username')
        whoami = re.sub(r'\w+[^\w]', '', whoami.strip())
        swarming_args += ['-user', whoami]
    except:
        pass

    if args.gpu:
        swarming_args += ['-d', 'gpu=' + args.gpu]

    if args.device_type:
        swarming_args += ['-d', 'device_type=' + args.device_type]

    if args.device_os:
        swarming_args += ['-d', 'device_os=' + args.device_os]

    cmd_args = ['-relative-cwd', args.gn_path, '--']

    if args.gold:
        swarming_args += ['-service-account', GOLD_SERVICE_ACCOUNT]
        cmd_args += ['luci-auth', 'context', '--']

    for env in args.env:
        swarming_args += ['-env', env]

    cmd_args += swarming_cmd

    if unknown:
        cmd_args += unknown

    if args.shards > 1:
        for i in range(args.shards):
            shard_args = swarming_args[:]
            shard_args.extend([
                '--env',
                'GTEST_TOTAL_SHARDS=%d' % args.shards,
                '--env',
                'GTEST_SHARD_INDEX=%d' % i,
            ])

            shard_args += cmd_args

            logging.info('Invoking swarming: %s' % ' '.join(shard_args))
            subprocess.call(shard_args)
    else:
        swarming_args += cmd_args
        logging.info('Invoking swarming: %s' % ' '.join(swarming_args))
        subprocess.call(swarming_args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
