#!/usr/bin/python2
#
# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# trigger.py:
#   Helper script for triggering GPU tests on swarming.

import argparse
import hashlib
import os
import subprocess
import sys


def parse_args():
    parser = argparse.ArgumentParser(os.path.basename(sys.argv[0]))
    parser.add_argument('gn_path', help='path to GN. (e.g. out/Release)')
    parser.add_argument('test', help='test name. (e.g. angle_end2end_tests)')
    parser.add_argument('os_dim', help='OS dimension. (e.g. Windows-10)')
    parser.add_argument('gpu_dim', help='GPU dimension. (e.g. intel-hd-630-win10-stable)')
    parser.add_argument('-s', '--shards', default=1, help='number of shards', type=int)
    parser.add_argument('-p', '--pool', default='Chrome-GPU', help='swarming pool')
    return parser.parse_known_args()


# Taken from:
# https://chromium.googlesource.com/chromium/src/tools/mb/+/2192df66cd0ed214bcfbfd387ad0c5c8c0a21eb1/mb.py#586
def add_base_software(swarming_args):
    # HACK(iannucci): These packages SHOULD NOT BE HERE.
    # Remove method once Swarming Pool Task Templates are implemented.
    # crbug.com/812428

    # Add in required base software. This should be kept in sync with the
    # `chromium_swarming` recipe module in build.git. All references to
    # `swarming_module` below are purely due to this.
    cipd_packages = [
        ('infra/python/cpython/${platform}', 'version:2.7.15.chromium14'),
        ('infra/tools/luci/logdog/butler/${platform}',
         'git_revision:e1abc57be62d198b5c2f487bfb2fa2d2eb0e867c'),
        ('infra/tools/luci/vpython-native/${platform}',
         'git_revision:e317c7d2c17d4c3460ee37524dfce4e1dee4306a'),
        ('infra/tools/luci/vpython/${platform}',
         'git_revision:e317c7d2c17d4c3460ee37524dfce4e1dee4306a'),
    ]

    for pkg, vers in cipd_packages:
        swarming_args.append('--cipd-package=.swarming_module:%s=%s' % (pkg, vers))

    # Add packages to $PATH
    swarming_args.extend([
        '--env-prefix',
        'PATH=.swarming_module',
        '--env-prefix',
        'PATH=.swarming_module/bin',
    ])

    # Add cache directives for vpython.
    vpython_cache_path = '.swarming_module_cache/vpython'
    swarming_args.extend([
        '--named-cache',
        'swarming_module_cache_vpython=' + vpython_cache_path,
        '--env-prefix',
        'VPYTHON_VIRTUALENV_ROOT=' + vpython_cache_path,
    ])


def main():
    args, unknown = parse_args()
    path = args.gn_path.replace('\\', '/')
    out_gn_path = '//' + path
    out_file_path = os.path.join(*path.split('/'))

    mb_script_path = os.path.join('tools', 'mb', 'mb.py')
    subprocess.call(['python', mb_script_path, 'isolate', out_gn_path, args.test])

    isolate_cmd_path = os.path.join('tools', 'luci-go', 'isolate')
    isolate_file = os.path.join(out_file_path, '%s.isolate' % args.test)
    isolated_file = os.path.join(out_file_path, '%s.isolated' % args.test)

    isolate_args = [
        isolate_cmd_path, 'archive', '-I', 'https://isolateserver.appspot.com', '-i', isolate_file,
        '-s', isolated_file
    ]
    subprocess.check_call(isolate_args)
    with open(isolated_file, 'rb') as f:
        sha = hashlib.sha1(f.read()).hexdigest()

    print('Got an isolated SHA of %s' % sha)
    swarming_script_path = os.path.join('tools', 'luci-go', 'swarming')

    swarming_args = [
        swarming_script_path, 'trigger', '-S', 'chromium-swarm.appspot.com', '-I',
        'https://isolateserver.appspot.com', '-d', 'os=' + args.os_dim, '-d', 'pool=' + args.pool,
        '-d', 'gpu=' + args.gpu_dim, '-s', sha
    ]

    add_base_software(swarming_args)

    for i in range(args.shards):
        shard_args = swarming_args[:]
        shard_args.extend([
            '--env',
            'GTEST_TOTAL_SHARDS=%d' % args.shards,
            '--env',
            'GTEST_SHARD_INDEX=%d' % i,
        ])
        if unknown:
            shard_args += ["--"] + unknown

        print(' '.join(shard_args))
        subprocess.call(shard_args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
