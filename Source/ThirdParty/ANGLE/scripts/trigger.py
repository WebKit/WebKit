#!/usr/bin/python2
#
# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# trigger.py:
#   Helper script for triggering GPU tests on swarming.

import argparse
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
    parser.add_argument('extra_args', help='extra test command line arguments', nargs='*')
    return parser.parse_args()


def main():
    args = parse_args()
    path = args.gn_path.replace('\\', '/')
    out_gn_path = '//' + path
    out_file_path = os.path.join(*path.split('/'))

    mb_script_path = os.path.join('tools', 'mb', 'mb.py')
    subprocess.call(['python', mb_script_path, 'isolate', out_gn_path, args.test])

    isolate_script_path = os.path.join('tools', 'swarming_client', 'isolate.py')
    isolate_file = os.path.join(out_file_path, '%s.isolate' % args.test)
    isolated_file = os.path.join(out_file_path, '%s.isolated' % args.test)

    isolate_args = [
        'python', isolate_script_path, 'archive',
        '-I', 'https://isolateserver.appspot.com',
        '-i', isolate_file,
        '-s', isolated_file]
    stdout = subprocess.check_output(isolate_args)
    sha = stdout[:40]

    print('Got an isolated SHA of %s' % sha)
    swarming_script_path = os.path.join('tools', 'swarming_client', 'swarming.py')

    swarmings_args = [
        'python', swarming_script_path, 'trigger',
        '-S', 'chromium-swarm.appspot.com',
        '-I', 'isolateserver.appspot.com',
        '-d', 'os', args.os_dim,
        '-d', 'pool', args.pool,
        '-d', 'gpu', args.gpu_dim,
        '--shards=%d' % args.shards,
        '-s', sha]

    if args.extra_args:
        swarmings_args += ['--'] + args.extra_args

    print(' '.join(swarmings_args))
    subprocess.call(swarmings_args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
