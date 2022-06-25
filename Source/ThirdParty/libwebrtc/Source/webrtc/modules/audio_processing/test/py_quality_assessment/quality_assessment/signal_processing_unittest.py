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

    def testMixSignalNoiseDifferentLengths(self):
        # Test signals.
        shorter = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
            pydub.AudioSegment.silent(duration=1000, frame_rate=8000))
        longer = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
            pydub.AudioSegment.silent(duration=2000, frame_rate=8000))

        # When the signal is shorter than the noise, the mix length always equals
        # that of the signal regardless of whether padding is applied.
        # No noise padding, length of signal less than that of noise.
        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=shorter,
            noise=longer,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.
            NO_PADDING)
        self.assertEqual(len(shorter), len(mix))
        # With noise padding, length of signal less than that of noise.
        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=shorter,
            noise=longer,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.
            ZERO_PADDING)
        self.assertEqual(len(shorter), len(mix))

        # When the signal is longer than the noise, the mix length depends on
        # whether padding is applied.
        # No noise padding, length of signal greater than that of noise.
        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=longer,
            noise=shorter,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.
            NO_PADDING)
        self.assertEqual(len(shorter), len(mix))
        # With noise padding, length of signal greater than that of noise.
        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=longer,
            noise=shorter,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.
            ZERO_PADDING)
        self.assertEqual(len(longer), len(mix))

    def testMixSignalNoisePaddingTypes(self):
        # Test signals.
        shorter = signal_processing.SignalProcessingUtils.GenerateWhiteNoise(
            pydub.AudioSegment.silent(duration=1000, frame_rate=8000))
        longer = signal_processing.SignalProcessingUtils.GeneratePureTone(
            pydub.AudioSegment.silent(duration=2000, frame_rate=8000), 440.0)

        # Zero padding: expect pure tone only in 1-2s.
        mix_zero_pad = signal_processing.SignalProcessingUtils.MixSignals(
            signal=longer,
            noise=shorter,
            target_snr=-6,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.
            ZERO_PADDING)

        # Loop: expect pure tone plus noise in 1-2s.
        mix_loop = signal_processing.SignalProcessingUtils.MixSignals(
            signal=longer,
            noise=shorter,
            target_snr=-6,
            pad_noise=signal_processing.SignalProcessingUtils.MixPadding.LOOP)

        def Energy(signal):
            samples = signal_processing.SignalProcessingUtils.AudioSegmentToRawData(
                signal).astype(np.float32)
            return np.sum(samples * samples)

        e_mix_zero_pad = Energy(mix_zero_pad[-1000:])
        e_mix_loop = Energy(mix_loop[-1000:])
        self.assertLess(0, e_mix_zero_pad)
        self.assertLess(e_mix_zero_pad, e_mix_loop)

    def testMixSignalSnr(self):
        # Test signals.
        tone_low = signal_processing.SignalProcessingUtils.GeneratePureTone(
            pydub.AudioSegment.silent(duration=64, frame_rate=8000), 250.0)
        tone_high = signal_processing.SignalProcessingUtils.GeneratePureTone(
            pydub.AudioSegment.silent(duration=64, frame_rate=8000), 3000.0)

        def ToneAmplitudes(mix):
            """Returns the amplitude of the coefficients #16 and #192, which
         correspond to the tones at 250 and 3k Hz respectively."""
            mix_fft = np.absolute(
                signal_processing.SignalProcessingUtils.Fft(mix))
            return mix_fft[16], mix_fft[192]

        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=tone_low, noise=tone_high, target_snr=-6)
        ampl_low, ampl_high = ToneAmplitudes(mix)
        self.assertLess(ampl_low, ampl_high)

        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=tone_high, noise=tone_low, target_snr=-6)
        ampl_low, ampl_high = ToneAmplitudes(mix)
        self.assertLess(ampl_high, ampl_low)

        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=tone_low, noise=tone_high, target_snr=6)
        ampl_low, ampl_high = ToneAmplitudes(mix)
        self.assertLess(ampl_high, ampl_low)

        mix = signal_processing.SignalProcessingUtils.MixSignals(
            signal=tone_high, noise=tone_low, target_snr=6)
        ampl_low, ampl_high = ToneAmplitudes(mix)
        self.assertLess(ampl_low, ampl_high)
