#!/usr/bin/python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Script for merging generated iOS libraries."""

import sys

import argparse
import os
import re
import subprocess

# Valid arch subdir names.
VALID_ARCHS = ['arm_libs', 'arm64_libs', 'ia32_libs', 'x64_libs']


def MergeLibs(lib_base_dir):
  """Merges generated iOS libraries for different archs.

  Uses libtool to generate FAT archive files for each generated library.

  Args:
    lib_base_dir: directory whose subdirectories are named by architecture and
                  contain the built libraries for that architecture

  Returns:
    Exit code of libtool.
  """
  output_dir_name = 'fat_libs'
  archs = [arch for arch in os.listdir(lib_base_dir)
           if arch in VALID_ARCHS]
  # For each arch, find (library name, libary path) for arch. We will merge
  # all libraries with the same name.
  libs = {}
  for lib_dir in [os.path.join(lib_base_dir, arch) for arch in VALID_ARCHS]:
    if not os.path.exists(lib_dir):
      continue
    for dirpath, _, filenames in os.walk(lib_dir):
      for filename in filenames:
        if not filename.endswith('.a'):
          continue
        entry = libs.get(filename, [])
        entry.append(os.path.join(dirpath, filename))
        libs[filename] = entry
  orphaned_libs = {}
  valid_libs = {}
  for library, paths in libs.items():
    if len(paths) < len(archs):
      orphaned_libs[library] = paths
    else:
      valid_libs[library] = paths
  for library, paths in orphaned_libs.items():
    components = library[:-2].split('_')[:-1]
    found = False
    # Find directly matching parent libs by stripping suffix.
    while components and not found:
      parent_library = '_'.join(components) + '.a'
      if parent_library in valid_libs:
        valid_libs[parent_library].extend(paths)
        found = True
        break
      components = components[:-1]
    # Find next best match by finding parent libs with the same prefix.
    if not found:
      base_prefix = library[:-2].split('_')[0]
      for valid_lib, valid_paths in valid_libs.items():
        if valid_lib[:len(base_prefix)] == base_prefix:
          valid_paths.extend(paths)
          found = True
          break
    assert found

  # Create output directory.
  output_dir_path = os.path.join(lib_base_dir, output_dir_name)
  if not os.path.exists(output_dir_path):
    os.mkdir(output_dir_path)

  # Use this so libtool merged binaries are always the same.
  env = os.environ.copy()
  env['ZERO_AR_DATE'] = '1'

  # Ignore certain errors.
  libtool_re = re.compile(r'^.*libtool:.*file: .* has no symbols$')

  # Merge libraries using libtool.
  libtool_returncode = 0
  for library, paths in valid_libs.items():
    cmd_list = ['libtool', '-static', '-v', '-o',
                os.path.join(output_dir_path, library)] + paths
    libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE, env=env)
    _, err = libtoolout.communicate()
    for line in err.splitlines():
      if not libtool_re.match(line):
        print >>sys.stderr, line
    # Unconditionally touch the output .a file on the command line if present
    # and the command succeeded. A bit hacky.
    libtool_returncode = libtoolout.returncode
    if not libtool_returncode:
      for i in range(len(cmd_list) - 1):
        if cmd_list[i] == '-o' and cmd_list[i+1].endswith('.a'):
          os.utime(cmd_list[i+1], None)
          break
  return libtool_returncode


def Main():
  parser_description = 'Merge WebRTC libraries.'
  parser = argparse.ArgumentParser(description=parser_description)
  parser.add_argument('lib_base_dir',
                      help='Directory with built libraries. ',
                      type=str)
  args = parser.parse_args()
  lib_base_dir = args.lib_base_dir
  MergeLibs(lib_base_dir)

if __name__ == '__main__':
  sys.exit(Main())
