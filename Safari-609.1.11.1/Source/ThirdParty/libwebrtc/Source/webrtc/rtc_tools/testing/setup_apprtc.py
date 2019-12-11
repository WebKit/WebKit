#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script sets up AppRTC and its dependencies.

Requires that depot_tools is installed and in the PATH.

It will put the result under <output_dir>/collider.
"""

import os
import sys

import utils


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def main(argv):
  if len(argv) == 1:
    return 'Usage %s <output_dir>' % argv[0]

  output_dir = os.path.abspath(argv[1])

  download_apprtc_path = os.path.join(SCRIPT_DIR, 'download_apprtc.py')
  utils.RunSubprocessWithRetry([sys.executable, download_apprtc_path,
                                output_dir])

  build_apprtc_path = os.path.join(SCRIPT_DIR, 'build_apprtc.py')
  apprtc_dir = os.path.join(output_dir, 'apprtc')
  go_dir = os.path.join(output_dir, 'go')
  collider_dir = os.path.join(output_dir, 'collider')
  utils.RunSubprocessWithRetry([sys.executable, build_apprtc_path,
                                apprtc_dir, go_dir, collider_dir])


if __name__ == '__main__':
  sys.exit(main(sys.argv))
