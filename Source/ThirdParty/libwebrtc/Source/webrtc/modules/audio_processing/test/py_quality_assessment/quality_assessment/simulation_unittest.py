# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the simulation module.
"""

import os
import shutil
import sys
import tempfile
import unittest

SRC = os.path.abspath(os.path.join(
    os.path.dirname((__file__)), os.pardir, os.pardir, os.pardir, os.pardir))
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))

import mock
import pydub

from . import audioproc_wrapper
from . import evaluation
from . import signal_processing
from . import simulation


class TestApmModuleSimulator(unittest.TestCase):
  """Unit tests for the ApmModuleSimulator class.
  """

  def setUp(self):
    """Create temporary folder and fake audio track."""
    self._output_path = tempfile.mkdtemp()

    silence = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)
    fake_signal = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
        silence)
    self._fake_audio_track_path = os.path.join(self._output_path, 'fake.wav')
    signal_processing.SignalProcessingUtils.SaveWav(
        self._fake_audio_track_path, fake_signal)

  def tearDown(self):
    """Recursively delete temporary folders."""
    shutil.rmtree(self._output_path)

  def testSimulation(self):
    # Instance dependencies to inject and mock.
    ap_wrapper = audioproc_wrapper.AudioProcWrapper()
    evaluator = evaluation.ApmModuleEvaluator()
    ap_wrapper.Run = mock.MagicMock(name='Run')
    evaluator.Run = mock.MagicMock(name='Run')

    # Instance simulator.
    simulator = simulation.ApmModuleSimulator(
        aechen_ir_database_path='',
        polqa_tool_bin_path=os.path.join(
            os.path.dirname(__file__), 'fake_polqa'),
        ap_wrapper=ap_wrapper,
        evaluator=evaluator)

    # What to simulate.
    config_files = ['apm_configs/default.json']
    input_files = [self._fake_audio_track_path]
    test_data_generators = ['identity', 'white_noise']
    eval_scores = ['audio_level', 'polqa']

    # Run all simulations.
    simulator.Run(
        config_filepaths=config_files,
        input_filepaths=input_files,
        test_data_generator_names=test_data_generators,
        eval_score_names=eval_scores,
        output_dir=self._output_path)

    # Check.
    # TODO(alessiob): Once the TestDataGenerator classes can be configured by
    # the client code (e.g., number of SNR pairs for the white noise teste data
    # gnerator), the exact number of calls to ap_wrapper.Run and evaluator.Run
    # is known; use that with assertEqual.
    min_number_of_simulations = len(config_files) * len(input_files) * len(
        test_data_generators)
    self.assertGreaterEqual(len(ap_wrapper.Run.call_args_list),
                            min_number_of_simulations)
    self.assertGreaterEqual(len(evaluator.Run.call_args_list),
                            min_number_of_simulations)
