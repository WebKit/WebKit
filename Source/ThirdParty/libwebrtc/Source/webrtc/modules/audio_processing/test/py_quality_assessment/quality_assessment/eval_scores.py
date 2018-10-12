# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Evaluation score abstract class and implementations.
"""

from __future__ import division
import logging
import os
import re
import subprocess
import sys

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

from . import data_access
from . import exceptions
from . import signal_processing


class EvaluationScore(object):

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self, score_filename_prefix):
    self._score_filename_prefix = score_filename_prefix
    self._input_signal_metadata = None
    self._reference_signal = None
    self._reference_signal_filepath = None
    self._tested_signal = None
    self._tested_signal_filepath = None
    self._output_filepath = None
    self._score = None
    self._render_signal_filepath = None

  @classmethod
  def RegisterClass(cls, class_to_register):
    """Registers an EvaluationScore implementation.

    Decorator to automatically register the classes that extend EvaluationScore.
    Example usage:

    @EvaluationScore.RegisterClass
    class AudioLevelScore(EvaluationScore):
      pass
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register
    return class_to_register

  @property
  def output_filepath(self):
    return self._output_filepath

  @property
  def score(self):
    return self._score

  def SetInputSignalMetadata(self, metadata):
    """Sets input signal metadata.

    Args:
      metadata: dict instance.
    """
    self._input_signal_metadata = metadata

  def SetReferenceSignalFilepath(self, filepath):
    """Sets the path to the audio track used as reference signal.

    Args:
      filepath: path to the reference audio track.
    """
    self._reference_signal_filepath = filepath

  def SetTestedSignalFilepath(self, filepath):
    """Sets the path to the audio track used as test signal.

    Args:
      filepath: path to the test audio track.
    """
    self._tested_signal_filepath = filepath

  def SetRenderSignalFilepath(self, filepath):
    """Sets the path to the audio track used as render signal.

    Args:
      filepath: path to the test audio track.
    """
    self._render_signal_filepath = filepath

  def Run(self, output_path):
    """Extracts the score for the set test data pair.

    Args:
      output_path: path to the directory where the output is written.
    """
    self._output_filepath = os.path.join(
        output_path, self._score_filename_prefix + self.NAME + '.txt')
    try:
      # If the score has already been computed, load.
      self._LoadScore()
      logging.debug('score found and loaded')
    except IOError:
      # Compute the score.
      logging.debug('score not found, compute')
      self._Run(output_path)

  def _Run(self, output_path):
    # Abstract method.
    raise NotImplementedError()

  def _LoadReferenceSignal(self):
    assert self._reference_signal_filepath is not None
    self._reference_signal = signal_processing.SignalProcessingUtils.LoadWav(
        self._reference_signal_filepath)

  def _LoadTestedSignal(self):
    assert self._tested_signal_filepath is not None
    self._tested_signal = signal_processing.SignalProcessingUtils.LoadWav(
        self._tested_signal_filepath)


  def _LoadScore(self):
    return data_access.ScoreFile.Load(self._output_filepath)

  def _SaveScore(self):
    return data_access.ScoreFile.Save(self._output_filepath, self._score)


@EvaluationScore.RegisterClass
class AudioLevelPeakScore(EvaluationScore):
  """Peak audio level score.

  Defined as the difference between the peak audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level_peak'

  def __init__(self, score_filename_prefix):
    EvaluationScore.__init__(self, score_filename_prefix)

  def _Run(self, output_path):
    self._LoadReferenceSignal()
    self._LoadTestedSignal()
    self._score = self._tested_signal.dBFS - self._reference_signal.dBFS
    self._SaveScore()


@EvaluationScore.RegisterClass
class MeanAudioLevelScore(EvaluationScore):
  """Mean audio level score.

  Defined as the difference between the mean audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level_mean'

  def __init__(self, score_filename_prefix):
    EvaluationScore.__init__(self, score_filename_prefix)

  def _Run(self, output_path):
    self._LoadReferenceSignal()
    self._LoadTestedSignal()

    dbfs_diffs_sum = 0.0
    seconds = min(len(self._tested_signal), len(self._reference_signal)) // 1000
    for t in range(seconds):
      t0 = t * seconds
      t1 = t0 + seconds
      dbfs_diffs_sum += (
        self._tested_signal[t0:t1].dBFS - self._reference_signal[t0:t1].dBFS)
    self._score = dbfs_diffs_sum / float(seconds)
    self._SaveScore()


@EvaluationScore.RegisterClass
class EchoMetric(EvaluationScore):
  """Echo score.

  Proportion of detected echo.

  Unit: ratio
  Ideal: 0
  Worst case: 1
  """

  NAME = 'echo_metric'

  def __init__(self, score_filename_prefix, echo_detector_bin_filepath):
    EvaluationScore.__init__(self, score_filename_prefix)

    # POLQA binary file path.
    self._echo_detector_bin_filepath = echo_detector_bin_filepath
    if not os.path.exists(self._echo_detector_bin_filepath):
      logging.error('cannot find EchoMetric tool binary file')
      raise exceptions.FileNotFoundError()

    self._echo_detector_bin_path, _ = os.path.split(
        self._echo_detector_bin_filepath)

  def _Run(self, output_path):
    echo_detector_out_filepath = os.path.join(output_path, 'echo_detector.out')
    if os.path.exists(echo_detector_out_filepath):
      os.unlink(echo_detector_out_filepath)

    logging.debug("Render signal filepath: %s", self._render_signal_filepath)
    if not os.path.exists(self._render_signal_filepath):
      logging.error("Render input required for evaluating the echo metric.")

    args = [
        self._echo_detector_bin_filepath,
        '--output_file', echo_detector_out_filepath,
        '--',
        '-i', self._tested_signal_filepath,
        '-ri', self._render_signal_filepath
    ]
    logging.debug(' '.join(args))
    subprocess.call(args, cwd=self._echo_detector_bin_path)

    # Parse Echo detector tool output and extract the score.
    self._score = self._ParseOutputFile(echo_detector_out_filepath)
    self._SaveScore()

  @classmethod
  def _ParseOutputFile(cls, echo_metric_file_path):
    """
    Parses the POLQA tool output formatted as a table ('-t' option).

    Args:
      polqa_out_filepath: path to the POLQA tool output file.

    Returns:
      The score as a number in [0, 1].
    """
    with open(echo_metric_file_path) as f:
      return float(f.read())

@EvaluationScore.RegisterClass
class PolqaScore(EvaluationScore):
  """POLQA score.

  See http://www.polqa.info/.

  Unit: MOS
  Ideal: 4.5
  Worst case: 1.0
  """

  NAME = 'polqa'

  def __init__(self, score_filename_prefix, polqa_bin_filepath):
    EvaluationScore.__init__(self, score_filename_prefix)

    # POLQA binary file path.
    self._polqa_bin_filepath = polqa_bin_filepath
    if not os.path.exists(self._polqa_bin_filepath):
      logging.error('cannot find POLQA tool binary file')
      raise exceptions.FileNotFoundError()

    # Path to the POLQA directory with binary and license files.
    self._polqa_tool_path, _ = os.path.split(self._polqa_bin_filepath)

  def _Run(self, output_path):
    polqa_out_filepath = os.path.join(output_path, 'polqa.out')
    if os.path.exists(polqa_out_filepath):
      os.unlink(polqa_out_filepath)

    args = [
        self._polqa_bin_filepath, '-t', '-q', '-Overwrite',
        '-Ref', self._reference_signal_filepath,
        '-Test', self._tested_signal_filepath,
        '-LC', 'NB',
        '-Out', polqa_out_filepath,
    ]
    logging.debug(' '.join(args))
    subprocess.call(args, cwd=self._polqa_tool_path)

    # Parse POLQA tool output and extract the score.
    polqa_output = self._ParseOutputFile(polqa_out_filepath)
    self._score = float(polqa_output['PolqaScore'])

    self._SaveScore()

  @classmethod
  def _ParseOutputFile(cls, polqa_out_filepath):
    """
    Parses the POLQA tool output formatted as a table ('-t' option).

    Args:
      polqa_out_filepath: path to the POLQA tool output file.

    Returns:
      A dict.
    """
    data = []
    with open(polqa_out_filepath) as f:
      for line in f:
        line = line.strip()
        if len(line) == 0 or line.startswith('*'):
          # Ignore comments.
          continue
        # Read fields.
        data.append(re.split(r'\t+', line))

    # Two rows expected (header and values).
    assert len(data) == 2, 'Cannot parse POLQA output'
    number_of_fields = len(data[0])
    assert number_of_fields == len(data[1])

    # Build and return a dictionary with field names (header) as keys and the
    # corresponding field values as values.
    return {data[0][index]: data[1][index] for index in range(number_of_fields)}


@EvaluationScore.RegisterClass
class TotalHarmonicDistorsionScore(EvaluationScore):
  """Total harmonic distorsion plus noise score.

  Total harmonic distorsion plus noise score.
  See "https://en.wikipedia.org/wiki/Total_harmonic_distortion#THD.2BN".

  Unit: -.
  Ideal: 0.
  Worst case: +inf
  """

  NAME = 'thd'

  def __init__(self, score_filename_prefix):
    EvaluationScore.__init__(self, score_filename_prefix)
    self._input_frequency = None

  def _Run(self, output_path):
    # TODO(aleloi): Integrate changes made locally.
    self._CheckInputSignal()

    self._LoadTestedSignal()
    if self._tested_signal.channels != 1:
      raise exceptions.EvaluationScoreException(
          'unsupported number of channels')
    samples = signal_processing.SignalProcessingUtils.AudioSegmentToRawData(
        self._tested_signal)

    # Init.
    num_samples = len(samples)
    duration = len(self._tested_signal) / 1000.0
    scaling = 2.0 / num_samples
    max_freq = self._tested_signal.frame_rate / 2
    f0_freq = float(self._input_frequency)
    t = np.linspace(0, duration, num_samples)

    # Analyze harmonics.
    b_terms = []
    n = 1
    while f0_freq * n < max_freq:
      x_n = np.sum(samples * np.sin(2.0 * np.pi * n * f0_freq * t)) * scaling
      y_n = np.sum(samples * np.cos(2.0 * np.pi * n * f0_freq * t)) * scaling
      b_terms.append(np.sqrt(x_n**2 + y_n**2))
      n += 1

    output_without_fundamental = samples - b_terms[0] * np.sin(
        2.0 * np.pi * f0_freq * t)
    distortion_and_noise = np.sqrt(np.sum(
        output_without_fundamental**2) * np.pi * scaling)

    # TODO(alessiob): Fix or remove if not needed.
    # thd = np.sqrt(np.sum(b_terms[1:]**2)) / b_terms[0]

    # TODO(alessiob): Check the range of |thd_plus_noise| and update the class
    # docstring above if accordingly.
    thd_plus_noise = distortion_and_noise / b_terms[0]

    self._score = thd_plus_noise
    self._SaveScore()

  def _CheckInputSignal(self):
    # Check input signal and get properties.
    try:
      if self._input_signal_metadata['signal'] != 'pure_tone':
        raise exceptions.EvaluationScoreException(
            'The THD score requires a pure tone as input signal')
      self._input_frequency = self._input_signal_metadata['frequency']
      if self._input_signal_metadata['test_data_gen_name'] != 'identity' or (
          self._input_signal_metadata['test_data_gen_config'] != 'default'):
        raise exceptions.EvaluationScoreException(
            'The THD score cannot be used with any test data generator other '
            'than "identity"')
    except TypeError:
      raise exceptions.EvaluationScoreException(
          'The THD score requires an input signal with associated metadata')
    except KeyError:
      raise exceptions.EvaluationScoreException(
          'Invalid input signal metadata to compute the THD score')
