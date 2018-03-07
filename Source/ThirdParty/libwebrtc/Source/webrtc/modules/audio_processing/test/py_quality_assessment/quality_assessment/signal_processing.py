# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Signal processing utility module.
"""

import array
import enum
import logging
import os
import sys

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

try:
  import pydub
  import pydub.generators
except ImportError:
  logging.critical('Cannot import the third-party Python package pydub')
  sys.exit(1)

try:
  import scipy.signal
  import scipy.fftpack
except ImportError:
  logging.critical('Cannot import the third-party Python package scipy')
  sys.exit(1)

from . import exceptions


class SignalProcessingUtils(object):
  """Collection of signal processing utilities.
  """

  @enum.unique
  class MixPadding(enum.Enum):
    NO_PADDING = 0
    ZERO_PADDING = 1
    LOOP = 2

  def __init__(self):
    pass

  @classmethod
  def LoadWav(cls, filepath, channels=1):
    """Loads wav file.

    Args:
      filepath: path to the wav audio track file to load.
      channels: number of channels (downmixing to mono by default).

    Returns:
      AudioSegment instance.
    """
    if not os.path.exists(filepath):
      logging.error('cannot find the <%s> audio track file', filepath)
      raise exceptions.FileNotFoundError()
    return pydub.AudioSegment.from_file(
        filepath, format='wav', channels=channels)

  @classmethod
  def SaveWav(cls, output_filepath, signal):
    """Saves wav file.

    Args:
      output_filepath: path to the wav audio track file to save.
      signal: AudioSegment instance.
    """
    return signal.export(output_filepath, format='wav')

  @classmethod
  def CountSamples(cls, signal):
    """Number of samples per channel.

    Args:
      signal: AudioSegment instance.

    Returns:
      An integer.
    """
    number_of_samples = len(signal.get_array_of_samples())
    assert signal.channels > 0
    assert number_of_samples % signal.channels == 0
    return number_of_samples / signal.channels

  @classmethod
  def GenerateSilence(cls, duration=1000, sample_rate=48000):
    """Generates silence.

    This method can also be used to create a template AudioSegment instance.
    A template can then be used with other Generate*() methods accepting an
    AudioSegment instance as argument.

    Args:
      duration: duration in ms.
      sample_rate: sample rate.

    Returns:
      AudioSegment instance.
    """
    return pydub.AudioSegment.silent(duration, sample_rate)

  @classmethod
  def GeneratePureTone(cls, template, frequency=440.0):
    """Generates a pure tone.

    The pure tone is generated with the same duration and in the same format of
    the given template signal.

    Args:
      template: AudioSegment instance.
      frequency: Frequency of the pure tone in Hz.

    Return:
      AudioSegment instance.
    """
    if frequency > template.frame_rate >> 1:
      raise exceptions.SignalProcessingException('Invalid frequency')

    generator = pydub.generators.Sine(
        sample_rate=template.frame_rate,
        bit_depth=template.sample_width * 8,
        freq=frequency)

    return generator.to_audio_segment(
        duration=len(template),
        volume=0.0)

  @classmethod
  def GenerateWhiteNoise(cls, template):
    """Generates white noise.

    The white noise is generated with the same duration and in the same format
    of the given template signal.

    Args:
      template: AudioSegment instance.

    Return:
      AudioSegment instance.
    """
    generator = pydub.generators.WhiteNoise(
        sample_rate=template.frame_rate,
        bit_depth=template.sample_width * 8)
    return generator.to_audio_segment(
        duration=len(template),
        volume=0.0)

  @classmethod
  def AudioSegmentToRawData(cls, signal):
    samples = signal.get_array_of_samples()
    if samples.typecode != 'h':
      raise exceptions.SignalProcessingException('Unsupported samples type')
    return np.array(signal.get_array_of_samples(), np.int16)

  @classmethod
  def Fft(cls, signal, normalize=True):
    if signal.channels != 1:
      raise NotImplementedError('multiple-channel FFT not implemented')
    x = cls.AudioSegmentToRawData(signal).astype(np.float32)
    if normalize:
      x /= max(abs(np.max(x)), 1.0)
    y = scipy.fftpack.fft(x)
    return y[:len(y) / 2]

  @classmethod
  def DetectHardClipping(cls, signal, threshold=2):
    """Detects hard clipping.

    Hard clipping is simply detected by counting samples that touch either the
    lower or upper bound too many times in a row (according to |threshold|).
    The presence of a single sequence of samples meeting such property is enough
    to label the signal as hard clipped.

    Args:
      signal: AudioSegment instance.
      threshold: minimum number of samples at full-scale in a row.

    Returns:
      True if hard clipping is detect, False otherwise.
    """
    if signal.channels != 1:
      raise NotImplementedError('multiple-channel clipping not implemented')
    if signal.sample_width != 2:  # Note that signal.sample_width is in bytes.
      raise exceptions.SignalProcessingException(
          'hard-clipping detection only supported for 16 bit samples')
    samples = cls.AudioSegmentToRawData(signal)

    # Detect adjacent clipped samples.
    samples_type_info = np.iinfo(samples.dtype)
    mask_min = samples == samples_type_info.min
    mask_max = samples == samples_type_info.max

    def HasLongSequence(vector, min_legth=threshold):
      """Returns True if there are one or more long sequences of True flags."""
      seq_length = 0
      for b in vector:
        seq_length = seq_length + 1 if b else 0
        if seq_length >= min_legth:
          return True
      return False

    return HasLongSequence(mask_min) or HasLongSequence(mask_max)

  @classmethod
  def ApplyImpulseResponse(cls, signal, impulse_response):
    """Applies an impulse response to a signal.

    Args:
      signal: AudioSegment instance.
      impulse_response: list or numpy vector of float values.

    Returns:
      AudioSegment instance.
    """
    # Get samples.
    assert signal.channels == 1, (
        'multiple-channel recordings not supported')
    samples = signal.get_array_of_samples()

    # Convolve.
    logging.info('applying %d order impulse response to a signal lasting %d ms',
                 len(impulse_response), len(signal))
    convolved_samples = scipy.signal.fftconvolve(
        in1=samples,
        in2=impulse_response,
        mode='full').astype(np.int16)
    logging.info('convolution computed')

    # Cast.
    convolved_samples = array.array(signal.array_type, convolved_samples)

    # Verify.
    logging.debug('signal length: %d samples', len(samples))
    logging.debug('convolved signal length: %d samples', len(convolved_samples))
    assert len(convolved_samples) > len(samples)

    # Generate convolved signal AudioSegment instance.
    convolved_signal = pydub.AudioSegment(
        data=convolved_samples,
        metadata={
            'sample_width': signal.sample_width,
            'frame_rate': signal.frame_rate,
            'frame_width': signal.frame_width,
            'channels': signal.channels,
        })
    assert len(convolved_signal) > len(signal)

    return convolved_signal

  @classmethod
  def Normalize(cls, signal):
    """Normalizes a signal.

    Args:
      signal: AudioSegment instance.

    Returns:
      An AudioSegment instance.
    """
    return signal.apply_gain(-signal.max_dBFS)

  @classmethod
  def Copy(cls, signal):
    """Makes a copy os a signal.

    Args:
      signal: AudioSegment instance.

    Returns:
      An AudioSegment instance.
    """
    return pydub.AudioSegment(
        data=signal.get_array_of_samples(),
        metadata={
            'sample_width': signal.sample_width,
            'frame_rate': signal.frame_rate,
            'frame_width': signal.frame_width,
            'channels': signal.channels,
        })

  @classmethod
  def MixSignals(cls, signal, noise, target_snr=0.0,
                 pad_noise=MixPadding.NO_PADDING):
    """Mixes |signal| and |noise| with a target SNR.

    Mix |signal| and |noise| with a desired SNR by scaling |noise|.
    If the target SNR is +/- infinite, a copy of signal/noise is returned.
    If |signal| is shorter than |noise|, the length of the mix equals that of
    |signal|. Otherwise, the mix length depends on whether padding is applied.
    When padding is not applied, that is |pad_noise| is set to NO_PADDING
    (default), the mix length equals that of |noise| - i.e., |signal| is
    truncated. Otherwise, |noise| is extended and the resulting mix has the same
    length of |signal|.

    Args:
      signal: AudioSegment instance (signal).
      noise: AudioSegment instance (noise).
      target_snr: float, numpy.Inf or -numpy.Inf (dB).
      pad_noise: SignalProcessingUtils.MixPadding, default: NO_PADDING.

    Returns:
      An AudioSegment instance.
    """
    # Handle infinite target SNR.
    if target_snr == -np.Inf:
      # Return a copy of noise.
      logging.warning('SNR = -Inf, returning noise')
      return cls.Copy(noise)
    elif target_snr == np.Inf:
      # Return a copy of signal.
      logging.warning('SNR = +Inf, returning signal')
      return cls.Copy(signal)

    # Check signal and noise power.
    signal_power = float(signal.dBFS)
    noise_power = float(noise.dBFS)
    if signal_power == -np.Inf:
      logging.error('signal has -Inf power, cannot mix')
      raise exceptions.SignalProcessingException(
          'cannot mix a signal with -Inf power')
    if noise_power == -np.Inf:
      logging.error('noise has -Inf power, cannot mix')
      raise exceptions.SignalProcessingException(
          'cannot mix a signal with -Inf power')

    # Mix.
    gain_db = signal_power - noise_power - target_snr
    signal_duration = len(signal)
    noise_duration = len(noise)
    if signal_duration <= noise_duration:
      # Ignore |pad_noise|, |noise| is truncated if longer that |signal|, the
      # mix will have the same length of |signal|.
      return signal.overlay(noise.apply_gain(gain_db))
    elif pad_noise == cls.MixPadding.NO_PADDING:
      # |signal| is longer than |noise|, but no padding is applied to |noise|.
      # Truncate |signal|.
      return noise.overlay(signal, gain_during_overlay=gain_db)
    elif pad_noise == cls.MixPadding.ZERO_PADDING:
      # TODO(alessiob): Check that this works as expected.
      return signal.overlay(noise.apply_gain(gain_db))
    elif pad_noise == cls.MixPadding.LOOP:
      # |signal| is longer than |noise|, extend |noise| by looping.
      return signal.overlay(noise.apply_gain(gain_db), loop=True)
    else:
      raise exceptions.SignalProcessingException('invalid padding type')
