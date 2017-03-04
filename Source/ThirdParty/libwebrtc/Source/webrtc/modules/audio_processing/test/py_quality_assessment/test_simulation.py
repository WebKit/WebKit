# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest

import apm_quality_assessment

class TestSimulationScript(unittest.TestCase):

  def test_main(self):
    # Exit with error code if no arguments are passed.
    with self.assertRaises(SystemExit) as cm:
      apm_quality_assessment.main()
    self.assertGreater(cm.exception.code, 0)
