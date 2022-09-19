#!/usr/bin/env python3
#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
"""Run the tests with

      python rtp_analyzer_test.py
or
      python3 rtp_analyzer_test.py
"""

from __future__ import absolute_import
from __future__ import print_function
import collections
import unittest

MISSING_NUMPY = False  # pylint: disable=invalid-name
try:
  import numpy
  import rtp_analyzer
except ImportError:
  MISSING_NUMPY = True

FakePoint = collections.namedtuple("FakePoint",
                                   ["real_send_time_ms", "absdelay"])


class TestDelay(unittest.TestCase):
  def AssertMaskEqual(self, masked_array, data, mask):
    self.assertEqual(list(masked_array.data), data)

    if isinstance(masked_array.mask, numpy.bool_):
      array_mask = masked_array.mask
    else:
      array_mask = list(masked_array.mask)
    self.assertEqual(array_mask, mask)

  def testCalculateDelaySimple(self):
    points = [FakePoint(0, 0), FakePoint(1, 0)]
    mask = rtp_analyzer.CalculateDelay(0, 1, 1, points)
    self.AssertMaskEqual(mask, [0, 0], False)

  def testCalculateDelayMissing(self):
    points = [FakePoint(0, 0), FakePoint(2, 0)]
    mask = rtp_analyzer.CalculateDelay(0, 2, 1, points)
    self.AssertMaskEqual(mask, [0, -1, 0], [False, True, False])

  def testCalculateDelayBorders(self):
    points = [FakePoint(0, 0), FakePoint(2, 0)]
    mask = rtp_analyzer.CalculateDelay(0, 3, 2, points)
    self.AssertMaskEqual(mask, [0, 0, -1], [False, False, True])


if __name__ == "__main__":
  if MISSING_NUMPY:
    print("Missing numpy, skipping test.")
  else:
    unittest.main()
