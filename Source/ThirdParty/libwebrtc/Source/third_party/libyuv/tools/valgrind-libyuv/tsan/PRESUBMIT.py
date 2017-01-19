#!/usr/bin/env python
# Copyright (c) 2012 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import re
import sys

"""
Copied from Chrome's src/tools/valgrind/tsan/PRESUBMIT.py

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def CheckChange(input_api, output_api):
  """Checks the TSan suppressions files for bad suppressions."""

  # Add the path to the Chrome valgrind dir to the import path:
  tools_vg_path = os.path.join(input_api.PresubmitLocalPath(), '..', '..',
                               'valgrind')
  sys.path.append(tools_vg_path)
  import suppressions

  return suppressions.PresubmitCheck(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

def GetPreferredTrySlaves():
  # We don't have any tsan slaves yet, so there's no use for this method.
  # When we have, the slave name(s) should be put into this list.
  return []
