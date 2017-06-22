#!/usr/bin/env python
# Copyright 2016 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This file emits the list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').
"""

import os
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
checkout_root = os.path.abspath(os.path.join(script_dir, os.pardir))
sys.path.insert(0, os.path.join(checkout_root, 'build'))
import landmine_utils


distributor = landmine_utils.distributor
gyp_defines = landmine_utils.gyp_defines
gyp_msvs_version = landmine_utils.gyp_msvs_version
platform = landmine_utils.platform


def print_landmines():
  """
  ALL LANDMINES ARE EMITTED FROM HERE.
  """
  # DO NOT add landmines as part of a regular CL. Landmines are a last-effort
  # bandaid fix if a CL that got landed has a build dependency bug and all bots
  # need to be cleaned up. If you're writing a new CL that causes build
  # dependency problems, fix the dependency problems instead of adding a
  # landmine.
  # See the Chromium version in src/build/get_landmines.py for usage examples.
  print 'Clobber to remove GYP artifacts after switching bots to GN.'
  print 'Another try to remove GYP artifacts after switching bots to GN.'


def main():
  print_landmines()
  return 0


if __name__ == '__main__':
  sys.exit(main())
