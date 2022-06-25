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

import fileinput
import os
import shutil
import subprocess
import sys

import utils

USAGE_STR = "Usage: {} <apprtc_dir> <go_dir> <output_dir>"


def _ConfigureApprtcServerToDeveloperMode(app_yaml_path):
    for line in fileinput.input(app_yaml_path, inplace=True):
        # We can't click past these in browser-based tests, so disable them.
        line = line.replace('BYPASS_JOIN_CONFIRMATION: false',
                            'BYPASS_JOIN_CONFIRMATION: true')
        sys.stdout.write(line)


def main(argv):
    if len(argv) != 4:
        return USAGE_STR.format(argv[0])

    apprtc_dir = os.path.abspath(argv[1])
    go_root_dir = os.path.abspath(argv[2])
    golang_workspace = os.path.abspath(argv[3])

    app_yaml_path = os.path.join(apprtc_dir, 'out', 'app_engine', 'app.yaml')
    _ConfigureApprtcServerToDeveloperMode(app_yaml_path)

    utils.RemoveDirectory(golang_workspace)

    collider_dir = os.path.join(apprtc_dir, 'src', 'collider')
    shutil.copytree(collider_dir, os.path.join(golang_workspace, 'src'))

    golang_path = os.path.join(go_root_dir, 'bin',
                               'go' + utils.GetExecutableExtension())
    golang_env = os.environ.copy()
    golang_env['GOROOT'] = go_root_dir
    golang_env['GOPATH'] = golang_workspace
    collider_out = os.path.join(
        golang_workspace, 'collidermain' + utils.GetExecutableExtension())
    subprocess.check_call(
        [golang_path, 'build', '-o', collider_out, 'collidermain'],
        env=golang_env)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
