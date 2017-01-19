#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import subprocess
import sys


def run_ant_build_command(path_to_ant_build_file):
  """Tries to build the passed build file with ant."""
  ant_executable = 'ant'
  if sys.platform == 'win32':
    if os.getenv('ANT_HOME'):
      ant_executable = os.path.join(os.getenv('ANT_HOME'), 'bin', 'ant.bat')
    else:
      ant_executable = 'ant.bat'
  cmd = [ant_executable, '-buildfile', path_to_ant_build_file]
  try:
    process = subprocess.Popen(cmd, stdout=sys.stdout, stderr=sys.stderr)
    process.wait()
    if process.returncode != 0:
      print >> sys.stderr, 'Failed to execute: %s' % ' '.join(cmd)
    return process.returncode
  except subprocess.CalledProcessError as e:
    print >> sys.stderr, 'Failed to execute: %s.\nCause: %s' % (' '.join(cmd),
                                                                e)
    return -1

def _main():
  core_build = os.path.join('third_party', 'zxing', 'core', 'build.xml')
  run_ant_build_command(core_build)

  javase_build = os.path.join('third_party', 'zxing', 'javase', 'build.xml')
  return run_ant_build_command(javase_build)


if __name__ == '__main__':
  sys.exit(_main())
