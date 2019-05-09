#!/usr/bin/python2
#
# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# update_glslang_binary.py:
#   Helper script to update the version of glslang in cloud storage.
#   This glslang is used to precompile Vulkan shaders. This script builds
#   glslang and uploads it to the bucket for Windows or Linux. It
#   currently only works on Windows and Linux. It also will update the
#   hashes stored in the tree. For more info see README.md in this folder.

import os
import platform
import re
import shutil
import subprocess
import sys


gn_args = """is_clang = true
is_debug = false
angle_enable_vulkan = true"""


is_windows = platform.system() == 'Windows'
is_linux  = platform.system() == 'Linux'


def find_file_in_path(filename):
    """ Finds |filename| by searching the environment paths """
    path_delimiter = ';' if is_windows else ':'
    for env_path in os.environ['PATH'].split(path_delimiter):
        full_path = os.path.join(env_path, filename)
        if os.path.isfile(full_path):
            return full_path
    raise Exception('Cannot find %s in environment' % filename)


def main():
    if not is_windows and not is_linux:
        print('Script must be run on Linux or Windows.')
        return 1

    # Step 1: Generate an output directory
    out_dir = os.path.join('out', 'glslang_release')

    if not os.path.isdir(out_dir):
        os.mkdir(out_dir)

    args_gn = os.path.join(out_dir, 'args.gn')
    if not os.path.isfile(args_gn):
        with open(args_gn, 'w') as f:
            f.write(gn_args)
            f.close()

    gn_exe = 'gn'
    if is_windows:
        gn_exe += '.bat'

    # Step 2: Generate the ninja build files in the output directory
    if subprocess.call([gn_exe, 'gen', out_dir]) != 0:
        print('Error calling gn')
        return 2

    # Step 3: Compile glslang_validator
    if subprocess.call(['ninja', '-C', out_dir, 'glslang_validator']) != 0:
        print('Error calling ninja')
        return 3

    # Step 4: Copy glslang_validator to the tools/glslang directory
    glslang_exe = 'glslang_validator'
    if is_windows:
        glslang_exe += '.exe'

    glslang_src = os.path.join(out_dir, glslang_exe)
    glslang_dst = os.path.join(sys.path[0], glslang_exe)

    shutil.copy(glslang_src, glslang_dst)

    # Step 5: Delete the build directory
    shutil.rmtree(out_dir)

    # Step 6: Upload to cloud storage
    upload_script = find_file_in_path('upload_to_google_storage.py')
    upload_args = ['python', upload_script, '-b', 'angle-glslang-validator', glslang_dst]
    if subprocess.call(upload_args) != 0:
        print('Error upload to cloud storage')
        return 4

    # Step 7: Stage new SHA to git
    git_exe = 'git'
    if is_windows:
        git_exe += '.bat'
    git_exe = find_file_in_path(git_exe)

    if subprocess.call([git_exe, 'add', glslang_dst + '.sha1']) != 0:
        print('Error running git add')
        return 5

    print('')
    print('The updated SHA has been staged for commit. Please commit and upload.')
    print('Suggested commit message:')
    print('----------------------------')
    print('')
    print('Update glslang_validator binary for %s.' % platform.system())
    print('')
    print('This binary was updated using %s.' % os.path.basename(__file__))
    print('Please see instructions in tools/glslang/README.md.')
    print('')
    print('Bug: None')

    return 0


if __name__ == '__main__':
    sys.exit(main())
