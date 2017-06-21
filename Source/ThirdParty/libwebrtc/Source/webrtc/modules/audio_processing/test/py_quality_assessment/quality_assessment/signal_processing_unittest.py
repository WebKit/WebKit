# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the signal_processing module.
"""

import unittest

import numpy as np
import pydub

from . import exceptions
from . import signal_processing


class TestSignalProcessing(unittest.TestCase):
  """Unit tests for the signal_processing module.
  """

  def testMixSignals(self):
    # Generate a template signal with which white noise can be generated.
    silence = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)

    # Generate two distinct AudioSegment instances with 1 second of white noise.
    signal = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
        silence)
    noise = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
        silence)

    # Extract samples.
    signal_samples = signal.get_array_of_samples()
    noise_samples = noise.get_array_of_samples()

    # Test target SNR -Inf (noise expected).
    mix_neg_inf = signal_processing.SignalProcessingUtils.MixSignals(
        signal, noise, -np.Inf)
    self.assertTrue(len(noise), len(mix_neg_inf))  # Check duration.
    mix_neg_inf_samples = mix_neg_inf.get_array_of_samples()
    self.assertTrue(  # Check samples.
        all([x == y for x, y in zip(noise_samples, mix_neg_inf_samples)]))

    # Test target SNR 0.0 (different data expected).
    mix_0 = signal_processing.SignalProcessingUtils.MixSignals(
        signal, noise, 0.0)
    self.assertTrue(len(signal), len(mix_0))  # Check duration.
    self.assertTrue(len(noise), len(mix_0))
    mix_0_samples = mix_0.get_array_of_samples()
    self.assertTrue(
        any([x != y for x, y in zip(signal_samples, mix_0_samples)]))
    self.assertTrue(
        any([x != y for x, y in zip(noise_samples, mix_0_samples)]))

    # Test target SNR +Inf (signal expected).
    mix_pos_inf = signal_processing.SignalProcessingUtils.MixSignals(
        signal, noise, np.Inf)
    self.assertTrue(len(signal), len(mix_pos_inf))  # Check duration.
    mix_pos_inf_samples = mix_pos_inf.get_array_of_samples()
    self.assertTrue(  # Check samples.
        all([x == y for x, y in zip(signal_samples, mix_pos_inf_samples)]))

  def testMixSignalsMinInfPower(self):
    silence = pydub.AudioSegment.silent(duration=1000, frame_rate=48000)
    signal = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
        silence)

    with self.assertRaises(exceptions.SignalProcessingException):
      _ = signal_processing.SignalProcessingUtils.MixSignals(
          signal, silence, 0.0)

    with self.assertRaises(exceptions.SignalProcessingException):
      _ = signal_processing.SignalProcessingUtils.MixSignals(
          silence, signal, 0.0)
