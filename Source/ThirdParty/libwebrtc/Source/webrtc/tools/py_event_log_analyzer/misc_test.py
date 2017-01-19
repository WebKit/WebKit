#!/usr/bin/env python
#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Run the tests with

      python misc_test.py
or
      python3 misc_test.py
"""

from __future__ import division
import random
import unittest

import misc


class TestMisc(unittest.TestCase):
  def testUnwrapMod3(self):
    data = [0, 1, 2, 0, -1, -2, -3, -4]
    unwrapped_3 = misc.unwrap(data, 3)
    self.assertEqual([0, 1, 2, 3, 2, 1, 0, -1], unwrapped_3)

  def testUnwrapMod4(self):
    data = [0, 1, 2, 0, -1, -2, -3, -4]
    unwrapped_4 = misc.unwrap(data, 4)
    self.assertEqual([0, 1, 2, 0, -1, -2, -3, -4], unwrapped_4)

  def testDataShouldNotChangeAfterUnwrap(self):
    data = [0, 1, 2, 0, -1, -2, -3, -4]
    _ = misc.unwrap(data, 4)

    self.assertEqual([0, 1, 2, 0, -1, -2, -3, -4], data)

  def testRandomlyMultiplesOfModAdded(self):
    # `unwrap` definition says only multiples of mod are added.
    random_data = [random.randint(0, 9) for _ in range(100)]

    for mod in range(1, 100):
      random_data_unwrapped_mod = misc.unwrap(random_data, mod)

      for (old_a, a) in zip(random_data, random_data_unwrapped_mod):
        self.assertEqual((old_a - a) % mod, 0)

  def testRandomlyAgainstInequalityDefinition(self):
    # Data has to satisfy -mod/2 <= difference < mod/2 for every
    # difference between consecutive values after unwrap.
    random_data = [random.randint(0, 9) for _ in range(100)]

    for mod in range(1, 100):
      random_data_unwrapped_mod = misc.unwrap(random_data, mod)

      for (a, b) in zip(random_data_unwrapped_mod,
                        random_data_unwrapped_mod[1:]):
        self.assertTrue(-mod / 2 <= b - a < mod / 2)

  def testRandomlyDataShouldNotChangeAfterUnwrap(self):
    random_data = [random.randint(0, 9) for _ in range(100)]
    random_data_copy = random_data[:]
    for mod in range(1, 100):
      _ = misc.unwrap(random_data, mod)

      self.assertEqual(random_data, random_data_copy)

if __name__ == "__main__":
  unittest.main()
