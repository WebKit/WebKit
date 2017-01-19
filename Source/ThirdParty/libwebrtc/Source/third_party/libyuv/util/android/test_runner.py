#!/usr/bin/env python
# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
Runs tests on Android devices.

This script exists to avoid Libyuv being broken by changes in the Chrome Android
test execution toolchain. It also conveniently sets the CHECKOUT_SOURCE_ROOT
environment variable.
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(__file__)
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
CHROMIUM_BUILD_ANDROID_DIR = os.path.join(ROOT_DIR, 'build', 'android')
sys.path.insert(0, CHROMIUM_BUILD_ANDROID_DIR)


import test_runner  # pylint: disable=W0406

def main():
  # Override environment variable to make it possible for the scripts to find
  # the root directory (our symlinking of the Chromium build toolchain would
  # otherwise make them fail to do so).
  os.environ['CHECKOUT_SOURCE_ROOT'] = ROOT_DIR
  return test_runner.main()

if __name__ == '__main__':
  sys.exit(main())
