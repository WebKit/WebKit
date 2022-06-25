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
        fake_reference_signal = (signal_processing.SignalProcessingUtils.
                                 GenerateWhiteNoise(silence))
        fake_tested_signal = (signal_processing.SignalProcessingUtils.
                              GenerateWhiteNoise(silence))

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
        # Evaluation score names to exclude (tested separately).
        exceptions = ['thd', 'echo_metric']

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
                    os.path.dirname(os.path.abspath(__file__)), 'fake_polqa'),
                echo_metric_tool_bin_path=None))
        eval_score_workers_factory.SetScoreFilenamePrefix('scores-')

        # Try each registered evaluation score worker.
        for eval_score_name in registered_classes:
            if eval_score_name in exceptions:
                continue

            # Instance evaluation score worker.
            eval_score_worker = eval_score_workers_factory.GetInstance(
                registered_classes[eval_score_name])

            # Set fake input metadata and reference and test file paths, then run.
            eval_score_worker.SetReferenceSignalFilepath(
                self._fake_reference_signal_filepath)
            eval_score_worker.SetTestedSignalFilepath(
                self._fake_tested_signal_filepath)
            eval_score_worker.Run(self._output_path)

            # Check output.
            score = data_access.ScoreFile.Load(
                eval_score_worker.output_filepath)
            self.assertTrue(isinstance(score, float))

    def testTotalHarmonicDistorsionScore(self):
        # Init.
        pure_tone_freq = 5000.0
        eval_score_worker = eval_scores.TotalHarmonicDistorsionScore('scores-')
        eval_score_worker.SetInputSignalMetadata({
            'signal':
            'pure_tone',
            'frequency':
            pure_tone_freq,
            'test_data_gen_name':
            'identity',
            'test_data_gen_config':
            'default',
        })
        template = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)

        # Create 3 test signals: pure tone, pure tone + white noise, white noise
        # only.
        pure_tone = signal_processing.SignalProcessingUtils.GeneratePureTone(
            template, pure_tone_freq)
        white_noise = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
            template)
        noisy_tone = signal_processing.SignalProcessingUtils.MixSignals(
            pure_tone, white_noise)

        # Compute scores for increasingly distorted pure tone signals.
        scores = [None, None, None]
        for index, tested_signal in enumerate(
            [pure_tone, noisy_tone, white_noise]):
            # Save signal.
            tmp_filepath = os.path.join(self._output_path, 'tmp_thd.wav')
            signal_processing.SignalProcessingUtils.SaveWav(
                tmp_filepath, tested_signal)

            # Compute score.
            eval_score_worker.SetTestedSignalFilepath(tmp_filepath)
            eval_score_worker.Run(self._output_path)
            scores[index] = eval_score_worker.score

            # Remove output file to avoid caching.
            os.remove(eval_score_worker.output_filepath)

        # Validate scores (lowest score with a pure tone).
        self.assertTrue(all([scores[i + 1] > scores[i] for i in range(2)]))
