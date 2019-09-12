#!/usr/bin/python2
#
# Copyright 2017 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# msvs_projects.py:
#   A helper utility that generates Visual Studio projects for each of
#   the available directories in 'out', and then runs another helper
#   utility that merges these projects into one solution.

import sys, os, subprocess

# Change this to target another VS version.
target_ide = 'vs2017'
solution_name = 'ANGLE'

script_dir = os.path.dirname(sys.argv[0])

# Set the CWD to the root ANGLE folder.
os.chdir(os.path.join(script_dir, '..'))

out_dir = 'out'

# Generate the VS solutions for any valid directory.
def generate_projects(dirname):
    args = ['gn.bat', 'gen', dirname, '--ide=' + target_ide, '--sln=' + solution_name]
    print('Running "' + ' '.join(args) + '"')
    subprocess.call(args)

for potential_dir in os.listdir(out_dir):
    path = os.path.join(out_dir, potential_dir)
    build_ninja_d = os.path.join(path, 'build.ninja.d')
    if os.path.exists(build_ninja_d):
        generate_projects(path)

# Run the helper utility that merges the projects.
args = ['python', os.path.join('build', 'win', 'gn_meta_sln.py')]
print('Running "' + ' '.join(args) + '"')
subprocess.call(args)
