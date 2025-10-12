#!/usr/bin/env python3
#  Copyright 2018 The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.

# Generate commit.h with git commit hash.
#

import pathlib
import subprocess as sp
import sys
import os

usage = """\
Usage: commit_id.py position               - print commit position
       commit_id.py gen <output> [depfile] - generate commit.h"""


def grab_output(command, cwd):
    return sp.Popen(
        command, stdout=sp.PIPE, shell=True, cwd=cwd).communicate()[0].strip().decode('utf-8')


def get_git_dir(cwd):
    return grab_output('git rev-parse --git-dir', cwd)


def get_git_common_dir(cwd):
    return grab_output('git rev-parse --git-common-dir', cwd)


def get_commit_position(cwd):
    return grab_output('git rev-list HEAD --count', cwd)


def does_git_dir_exist(cwd):
    ret = os.path.exists(os.path.join(cwd, '.git', 'HEAD'))
    # .git may be a file with a gitdir directive pointing elsewhere.
    if not ret and os.path.exists(os.path.join(cwd, '.git')):
        ret = 'true' == grab_output('git rev-parse --is-inside-work-tree', cwd)
    return ret


def unpack_ref(ref_file, ref_file_full_path, packed_refs_full_path):

    with open(packed_refs_full_path) as fin:
        refs = fin.read().strip().split('\n')

    # Strip comments
    refs = [ref.split(' ') for ref in refs if ref.strip()[0] != '#']

    # Parse lines (which are in the format <hash> <ref_file>) and find the input file
    refs = [git_hash for (git_hash, file_path) in refs if file_path == ref_file]

    assert (len(refs) == 1)
    git_hash = refs[0]

    with open(ref_file_full_path, 'w') as fout:
        fout.write(git_hash + '\n')


# Get the files that GN action target angle_commit_id depends on.
# If any of these files changed, the build system should rerun the commit_id.py
# script to regenerate the angle_commit.h
# Case 1: git config extensions.refStorage == file (or empty)
# Case 1.1: .git/HEAD contains the hash.
#           Return .git/HEAD
# Case 1.2: .git/HEAD contains non-hash: e.g. refs/heads/<branch_name>
#           Return .git/HEAD
#           Return .git/refs/heads/<branch_name>
# Case 2: git config extensions.refStorage == reftable
# In this case, HEAD will just store refs/heads/.invalid. If any reference is
# updated, .git/reftable/table.list will be updated.
#           Return .git/reftable/table.list
def get_git_inputs_and_maybe_unpack_ref(cwd):
    # check git extensions.refStorage type
    gitRefStorageType = grab_output('git config --get extensions.refStorage', cwd)
    isRefTableStorage = gitRefStorageType and gitRefStorageType == "reftable"
    git_dir = os.path.normpath(os.path.join(cwd, get_git_dir(cwd)))
    head_file = os.path.join(git_dir, 'HEAD')
    ret = []
    # commit id should depend on angle's HEAD revision only if
    # extensions.refStorage = file.
    # If extensions.refStorage = reftable, .git/HEAD will always contains
    # "refs/heads/.invalid", and we can't rely on it as an indicator of whether
    # to rerun this script.
    if not isRefTableStorage:
        ret.append(head_file)
    git_common_dir = os.path.normpath(os.path.join(cwd, get_git_common_dir(cwd)))
    result = pathlib.Path(head_file).read_text().split()
    if result[0] == "ref:":
        # if the extensions.refStorage is reftable, add .git/reftable/tables.list as an input to gn action target
        if isRefTableStorage:
            ret.append(os.path.join(git_common_dir, 'reftable', 'tables.list'))
        else:
            # if the extensions.refStorage is file, add loose ref file pointed by HEAD
            ref_file = result[1]
            ref_file_full_path = os.path.join(git_common_dir, ref_file)

            if not os.path.exists(ref_file_full_path):
                packed_refs_full_path = os.path.join(git_common_dir, 'packed-refs')
                unpack_ref(ref_file, ref_file_full_path, packed_refs_full_path)
            ret.append(os.path.join(git_common_dir, ref_file))
    return ret


if len(sys.argv) < 2:
    sys.exit(usage)

operation = sys.argv[1]

# Set the root of ANGLE's repo as the working directory
aosp_angle_path = os.path.join(os.path.dirname('.'), 'external', 'angle')
aosp = os.path.exists(aosp_angle_path)
cwd = aosp_angle_path if aosp else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
git_dir_exists = does_git_dir_exist(cwd)

if operation == 'position':
    if git_dir_exists:
        print(get_commit_position(cwd))
    else:
        print("0")
    sys.exit(0)

if len(sys.argv) < 3 or operation != 'gen':
    sys.exit(usage)

output_file = sys.argv[2]
depfile = sys.argv[3] if len(sys.argv) == 4 else None
commit_id_size = 12
commit_date = 'unknown date'
commit_position = '0'

# If the ANGLE_UPSTREAM_HASH environment variable is set, use it as
# commit_id. commit_date will be 'unknown date' and commit_position will be 0
# in this case. See details in roll_aosp.sh where commit_id.py is invoked.
commit_id = os.environ.get('ANGLE_UPSTREAM_HASH')
# If ANGLE_UPSTREAM_HASH environment variable is not set, use the git command
# to get the git hash, when .git is available
if git_dir_exists and not commit_id:
    try:
        commit_id = grab_output('git rev-parse --short=%d HEAD' % commit_id_size, cwd)
        commit_date = grab_output('git show -s --format=%ci HEAD', cwd) or commit_date
        commit_position = get_commit_position(cwd) or commit_position
    except:
        pass

with open(output_file, 'w') as hfile:
    hfile.write('#define ANGLE_COMMIT_HASH "%s"\n' % (commit_id or "unknown hash"))
    hfile.write('#define ANGLE_COMMIT_HASH_SIZE %d\n' % commit_id_size)
    hfile.write('#define ANGLE_COMMIT_DATE "%s"\n' % commit_date)
    hfile.write('#define ANGLE_COMMIT_POSITION %s\n' % commit_position)

if depfile:
    inputs = []
    if git_dir_exists:
        inputs = get_git_inputs_and_maybe_unpack_ref(cwd)
    with open(depfile, 'w') as f:
        f.write(output_file)
        f.write(': ')
        f.write(' '.join(os.path.relpath(p) for p in inputs))
        f.write('\n')
