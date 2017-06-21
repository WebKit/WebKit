# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the eval_scores module.
"""

import os
import shutil
import tempfile
import unittest

import pydub

from . import data_access
from . import eval_scores
from . import eval_scores_factory
from . import signal_processing


class TestEvalScores(unittest.TestCase):
  """Unit tests for the eval_scores module.
  """

  def setUp(self):
    """Create temporary output folder and two audio track files."""
    self._output_path = tempfile.mkdtemp()

    # Create fake reference and tested (i.e., APM output) audio track files.
    silence = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)
    fake_reference_signal = (
        signal_processing.SignalProcessingUtils.GenerateWhiteNoise(silence))
    fake_tested_signal = (
        signal_processing.SignalProcessingUtils.GenerateWhiteNoise(silence))

    # Save fake audio tracks.
    self._fake_reference_signal_filepath = os.path.join(
        self._output_path, 'fake_ref.wav')
    signal_processing.SignalProcessingUtils.SaveWav(
        self._fake_reference_signal_filepath, fake_reference_signal)
    self._fake_tested_signal_filepath = os.path.join(
        self._output_path, 'fake_test.wav')
    signal_processing.SignalProcessingUtils.SaveWav(
        self._fake_tested_signal_filepath, fake_tested_signal)

  def tearDown(self):
    """Recursively delete temporary folder."""
    shutil.rmtree(self._output_path)

  def testRegisteredClasses(self):
    # Preliminary check.
    self.assertTrue(os.path.exists(self._output_path))

    # Check that there is at least one registered evaluation score worker.
    registered_classes = eval_scores.EvaluationScore.REGISTERED_CLASSES
    self.assertIsInstance(registered_classes, dict)
    self.assertGreater(len(registered_classes), 0)

    # Instance evaluation score workers factory with fake dependencies.
    eval_score_workers_factory = (
        eval_scores_factory.EvaluationScoreWorkerFactory(
            polqa_tool_bin_path=os.path.join(
                os.path.dirname(os.path.abspath(__file__)), 'fake_polqa')))

    # Try each registered evaluation score worker.
    for eval_score_name in registered_classes:
      # Instance evaluation score worker.
      eval_score_worker = eval_score_workers_factory.GetInstance(
          registered_classes[eval_score_name])

      # Set reference and test, then run.
      eval_score_worker.SetReferenceSignalFilepath(
          self._fake_reference_signal_filepath)
      eval_score_worker.SetTestedSignalFilepath(
          self._fake_tested_signal_filepath)
      eval_score_worker.Run(self._output_path)

      # Check output.
      score = data_access.ScoreFile.Load(eval_score_worker.output_filepath)
      self.assertTrue(isinstance(score, float))
