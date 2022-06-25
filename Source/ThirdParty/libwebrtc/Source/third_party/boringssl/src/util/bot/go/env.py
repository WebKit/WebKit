#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Modified from go/env.py in Chromium infrastructure's repository to patch out
# everything but the core toolchain.
#
# https://chromium.googlesource.com/infra/infra/

"""Used to wrap a command:

$ ./env.py go version
"""

assert __name__ == '__main__'

import os
import subprocess
import sys

# /path/to/util/bot
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Platform depended suffix for executable files.
EXE_SFX = '.exe' if sys.platform == 'win32' else ''

def get_go_environ(goroot):
  """Returns a copy of os.environ with added GOROOT and PATH variables."""
  env = os.environ.copy()
  env['GOROOT'] = goroot
  gobin = os.path.join(goroot, 'bin')
  path = env['PATH'].split(os.pathsep)
  if gobin not in path:
    env['PATH'] = os.pathsep.join([gobin] + path)
  return env

def find_executable(name, goroot):
  """Returns full path to an executable in GOROOT."""
  basename = name
  if EXE_SFX and basename.endswith(EXE_SFX):
    basename = basename[:-len(EXE_SFX)]
  full_path = os.path.join(goroot, 'bin', basename + EXE_SFX)
  if os.path.exists(full_path):
    return full_path
  return name

# TODO(davidben): Now that we use CIPD to fetch Go, this script does not do
# much. Switch to setting up GOROOT and PATH in the recipe?
goroot = os.path.join(ROOT, 'golang')
new = get_go_environ(goroot)

exe = sys.argv[1]
if exe == 'python':
  exe = sys.executable
else:
  # Help Windows to find the executable in new PATH, do it only when
  # executable is referenced by name (and not by path).
  if os.sep not in exe:
    exe = find_executable(exe, goroot)
sys.exit(subprocess.call([exe] + sys.argv[2:], env=new))
