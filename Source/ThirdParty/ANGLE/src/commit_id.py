#!/usr/bin/env python
#  Copyright 2018 The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.

# Generate commit.h with git commit hash.
#

import subprocess as sp
import sys
import os

usage = """\
Usage: commit_id.py check                - check if git is present
       commit_id.py gen <file_to_write>  - generate commit.h"""


def grab_output(command, cwd):
    return sp.Popen(command, stdout=sp.PIPE, shell=True, cwd=cwd).communicate()[0].strip()


if len(sys.argv) < 2:
    sys.exit(usage)

operation = sys.argv[1]

# Set the root of ANGLE's repo as the working directory
cwd = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..')

git_dir_exists = os.path.exists(os.path.join(cwd, '.git', 'HEAD'))

if operation == 'check':
    if git_dir_exists:
        print("1")
    else:
        print("0")
    sys.exit(0)

if len(sys.argv) < 3 or operation != 'gen':
    sys.exit(usage)

output_file = sys.argv[2]
commit_id_size = 12
commit_id = 'unknown hash'
commit_date = 'unknown date'
enable_binary_loading = False

if git_dir_exists:
    try:
        commit_id = grab_output('git rev-parse --short=%d HEAD' % commit_id_size, cwd)
        commit_date = grab_output('git show -s --format=%ci HEAD', cwd)
        enable_binary_loading = True
    except:
        pass

hfile = open(output_file, 'w')

hfile.write('#define ANGLE_COMMIT_HASH "%s"\n' % commit_id)
hfile.write('#define ANGLE_COMMIT_HASH_SIZE %d\n' % commit_id_size)
hfile.write('#define ANGLE_COMMIT_DATE "%s"\n' % commit_date)

if not enable_binary_loading:
    hfile.write('#define ANGLE_DISABLE_PROGRAM_BINARY_LOAD\n')

hfile.close()
