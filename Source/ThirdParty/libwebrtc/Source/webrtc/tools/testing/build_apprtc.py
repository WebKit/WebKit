#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Builds the AppRTC collider using the golang toolchain.

The golang toolchain is downloaded by download_apprtc.py. We use that here
to build the AppRTC collider server.

This script needs to know the path to the 'src' directory in apprtc, the
root directory of 'go' and the output_dir.
"""

import os
import shutil
import subprocess
import sys

import utils


USAGE_STR = "Usage: {} <apprtc_src_dir> <go_dir> <output_dir>"


def main(argv):
  if len(argv) != 4:
    return USAGE_STR.format(argv[0])

  apprtc_dir = os.path.abspath(argv[1])
  go_root_dir = os.path.abspath(argv[2])
  golang_workspace = os.path.abspath(argv[3])

  utils.RemoveDirectory(golang_workspace)

  golang_workspace_src = os.path.join(golang_workspace, 'src')

  collider_dir = os.path.join(apprtc_dir, 'collider')
  shutil.copytree(collider_dir, golang_workspace_src)

  golang_binary = 'go%s' % ('.exe' if utils.GetPlatform() == 'win' else '')
  golang_path = os.path.join(go_root_dir, 'bin', golang_binary)

  golang_env = os.environ.copy()
  golang_env['GOROOT'] = go_root_dir
  golang_env['GOPATH'] = golang_workspace
  collider_exec = os.path.join(golang_workspace, 'collidermain')
  subprocess.check_call([golang_path, 'build', '-o', collider_exec,
                         'collidermain'], env=golang_env)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
