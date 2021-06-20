# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Unit tests for the echo path simulation module.
"""

import shutil
import os
import tempfile
import unittest

import pydub

from . import echo_path_simulation
from . import echo_path_simulation_factory
from . import signal_processing


class TestEchoPathSimulators(unittest.TestCase):
    """Unit tests for the eval_scores module.
  """

    def setUp(self):
        """Creates temporary data."""
        self._tmp_path = tempfile.mkdtemp()

        # Create and save white noise.
        silence = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)
        white_noise = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
            silence)
        self._audio_track_num_samples = (
            signal_processing.SignalProcessingUtils.CountSamples(white_noise))
        self._audio_track_filepath = os.path.join(self._tmp_path,
                                                  'white_noise.wav')
        signal_processing.SignalProcessingUtils.SaveWav(
            self._audio_track_filepath, white_noise)

        # Make a copy the white noise audio track file; it will be used by
        # echo_path_simulation.RecordedEchoPathSimulator.
        shutil.copy(self._audio_track_filepath,
                    os.path.join(self._tmp_path, 'white_noise_echo.wav'))

    def tearDown(self):
        """Recursively deletes temporary folders."""
        shutil.rmtree(self._tmp_path)

    def testRegisteredClasses(self):
        # Check that there is at least one registered echo path simulator.
        registered_classes = (
            echo_path_simulation.EchoPathSimulator.REGISTERED_CLASSES)
        self.assertIsInstance(registered_classes, dict)
        self.assertGreater(len(registered_classes), 0)

        # Instance factory.
        factory = echo_path_simulation_factory.EchoPathSimulatorFactory()

        # Try each registered echo path simulator.
        for echo_path_simulator_name in registered_classes:
            simulator = factory.GetInstance(
                echo_path_simulator_class=registered_classes[
                    echo_path_simulator_name],
                render_input_filepath=self._audio_track_filepath)

            echo_filepath = simulator.Simulate(self._tmp_path)
            if echo_filepath is None:
                self.assertEqual(echo_path_simulation.NoEchoPathSimulator.NAME,
                                 echo_path_simulator_name)
                # No other tests in this case.
                continue

            # Check that the echo audio track file exists and its length is greater or
            # equal to that of the render audio track.
            self.assertTrue(os.path.exists(echo_filepath))
            echo = signal_processing.SignalProcessingUtils.LoadWav(
                echo_filepath)
            self.assertGreaterEqual(
                signal_processing.SignalProcessingUtils.CountSamples(echo),
                self._audio_track_num_samples)
