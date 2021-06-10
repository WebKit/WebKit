# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Unit tests for the input mixer module.
"""

import logging
import os
import shutil
import tempfile
import unittest

import mock

from . import exceptions
from . import input_mixer
from . import signal_processing


class TestApmInputMixer(unittest.TestCase):
    """Unit tests for the ApmInputMixer class.
  """

    # Audio track file names created in setUp().
    _FILENAMES = ['capture', 'echo_1', 'echo_2', 'shorter', 'longer']

    # Target peak power level (dBFS) of each audio track file created in setUp().
    # These values are hand-crafted in order to make saturation happen when
    # capture and echo_2 are mixed and the contrary for capture and echo_1.
    # None means that the power is not changed.
    _MAX_PEAK_POWER_LEVELS = [-10.0, -5.0, 0.0, None, None]

    # Audio track file durations in milliseconds.
    _DURATIONS = [1000, 1000, 1000, 800, 1200]

    _SAMPLE_RATE = 48000

    def setUp(self):
        """Creates temporary data."""
        self._tmp_path = tempfile.mkdtemp()

        # Create audio track files.
        self._audio_tracks = {}
        for filename, peak_power, duration in zip(self._FILENAMES,
                                                  self._MAX_PEAK_POWER_LEVELS,
                                                  self._DURATIONS):
            audio_track_filepath = os.path.join(self._tmp_path,
                                                '{}.wav'.format(filename))

            # Create a pure tone with the target peak power level.
            template = signal_processing.SignalProcessingUtils.GenerateSilence(
                duration=duration, sample_rate=self._SAMPLE_RATE)
            signal = signal_processing.SignalProcessingUtils.GeneratePureTone(
                template)
            if peak_power is not None:
                signal = signal.apply_gain(-signal.max_dBFS + peak_power)

            signal_processing.SignalProcessingUtils.SaveWav(
                audio_track_filepath, signal)
            self._audio_tracks[filename] = {
                'filepath':
                audio_track_filepath,
                'num_samples':
                signal_processing.SignalProcessingUtils.CountSamples(signal)
            }

    def tearDown(self):
        """Recursively deletes temporary folders."""
        shutil.rmtree(self._tmp_path)

    def testCheckMixSameDuration(self):
        """Checks the duration when mixing capture and echo with same duration."""
        mix_filepath = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['echo_1']['filepath'])
        self.assertTrue(os.path.exists(mix_filepath))

        mix = signal_processing.SignalProcessingUtils.LoadWav(mix_filepath)
        self.assertEqual(
            self._audio_tracks['capture']['num_samples'],
            signal_processing.SignalProcessingUtils.CountSamples(mix))

    def testRejectShorterEcho(self):
        """Rejects echo signals that are shorter than the capture signal."""
        try:
            _ = input_mixer.ApmInputMixer.Mix(
                self._tmp_path, self._audio_tracks['capture']['filepath'],
                self._audio_tracks['shorter']['filepath'])
            self.fail('no exception raised')
        except exceptions.InputMixerException:
            pass

    def testCheckMixDurationWithLongerEcho(self):
        """Checks the duration when mixing an echo longer than the capture."""
        mix_filepath = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['longer']['filepath'])
        self.assertTrue(os.path.exists(mix_filepath))

        mix = signal_processing.SignalProcessingUtils.LoadWav(mix_filepath)
        self.assertEqual(
            self._audio_tracks['capture']['num_samples'],
            signal_processing.SignalProcessingUtils.CountSamples(mix))

    def testCheckOutputFileNamesConflict(self):
        """Checks that different echo files lead to different output file names."""
        mix1_filepath = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['echo_1']['filepath'])
        self.assertTrue(os.path.exists(mix1_filepath))

        mix2_filepath = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['echo_2']['filepath'])
        self.assertTrue(os.path.exists(mix2_filepath))

        self.assertNotEqual(mix1_filepath, mix2_filepath)

    def testHardClippingLogExpected(self):
        """Checks that hard clipping warning is raised when occurring."""
        logging.warning = mock.MagicMock(name='warning')
        _ = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['echo_2']['filepath'])
        logging.warning.assert_called_once_with(
            input_mixer.ApmInputMixer.HardClippingLogMessage())

    def testHardClippingLogNotExpected(self):
        """Checks that hard clipping warning is not raised when not occurring."""
        logging.warning = mock.MagicMock(name='warning')
        _ = input_mixer.ApmInputMixer.Mix(
            self._tmp_path, self._audio_tracks['capture']['filepath'],
            self._audio_tracks['echo_1']['filepath'])
        self.assertNotIn(
            mock.call(input_mixer.ApmInputMixer.HardClippingLogMessage()),
            logging.warning.call_args_list)
