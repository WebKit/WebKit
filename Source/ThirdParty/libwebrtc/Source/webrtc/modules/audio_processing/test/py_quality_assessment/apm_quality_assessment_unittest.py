# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Unit tests for the apm_quality_assessment module.
"""

import sys
import unittest

import mock

import apm_quality_assessment


class TestSimulationScript(unittest.TestCase):
    """Unit tests for the apm_quality_assessment module.
  """

    def testMain(self):
        # Exit with error code if no arguments are passed.
        with self.assertRaises(SystemExit) as cm, mock.patch.object(
                sys, 'argv', ['apm_quality_assessment.py']):
            apm_quality_assessment.main()
        self.assertGreater(cm.exception.code, 0)
