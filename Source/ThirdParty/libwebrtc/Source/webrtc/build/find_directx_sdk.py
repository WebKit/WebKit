#!/usr/bin/env python
#
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Searches for DirectX SDK installation and prints full path to it."""

import os
import subprocess
import sys


def main():
  sys.stdout.write(FindDirectXInstallation())
  return 0


def FindDirectXInstallation():
  """Try to find an installation location for the DirectX SDK. Check for the
  standard environment variable, and if that doesn't exist, try to find
  via the registry. Returns empty string if not found in either location."""

  dxsdk_dir = os.environ.get('DXSDK_DIR')
  if dxsdk_dir:
    return dxsdk_dir

  # Setup params to pass to and attempt to launch reg.exe.
  cmd = ['reg.exe', 'query', r'HKLM\Software\Microsoft\DirectX', '/s']
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  for line in p.communicate()[0].splitlines():
    if 'InstallPath' in line:
      return line.split('    ')[3] + "\\"

  return ''


if __name__ == '__main__':
  sys.exit(main())
