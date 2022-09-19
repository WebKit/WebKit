#!/usr/bin/env vpython3

# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Calls process_perf_results.py with a python 3 interpreter."""

import sys
import subprocess


# TODO(crbug.com/webrtc/13835): Delete this file and use
# process_perf_results.py instead.
def main():
  cmd = sys.argv[0].replace('_py2', '')
  print('Calling "%s" with py3 in case this script was called with py2.' % cmd)
  return subprocess.call(['vpython3', cmd] + sys.argv[1:])


if __name__ == '__main__':
  sys.exit(main())
