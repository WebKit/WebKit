# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest

from . import eval_scores

class TestEvalScores(unittest.TestCase):

  def test_registered_classes(self):
    # Check that there is at least one registered evaluation score worker.
    classes = eval_scores.EvaluationScore.REGISTERED_CLASSES
    self.assertIsInstance(classes, dict)
    self.assertGreater(len(classes), 0)
