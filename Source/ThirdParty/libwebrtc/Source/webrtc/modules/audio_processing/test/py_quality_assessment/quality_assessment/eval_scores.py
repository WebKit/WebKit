# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Evaluation score abstract class and implementations.
"""

import logging
import os
import re
import subprocess

from . import data_access
from . import exceptions
from . import signal_processing


class EvaluationScore(object):

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self):
    self._reference_signal = None
    self._reference_signal_filepath = None
    self._tested_signal = None
    self._tested_signal_filepath = None
    self._output_filepath = None
    self._score = None

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

  def SetReferenceSignalFilepath(self, filepath):
    """ Sets the path to the audio track used as reference signal.

    Args:
      filepath: path to the reference audio track.
    """
    self._reference_signal_filepath = filepath

  def SetTestedSignalFilepath(self, filepath):
    """ Sets the path to the audio track used as test signal.

    Args:
      filepath: path to the test audio track.
    """
    self._tested_signal_filepath = filepath

  def Run(self, output_path):
    """Extracts the score for the set test data pair.

    Args:
      output_path: path to the directory where the output is written.
    """
    self._output_filepath = os.path.join(output_path, 'score-{}.txt'.format(
        self.NAME))
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
class AudioLevelScore(EvaluationScore):
  """Audio level score.

  Defined as the difference between the average audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level'

  def __init__(self):
    EvaluationScore.__init__(self)

  def _Run(self, output_path):
    self._LoadReferenceSignal()
    self._LoadTestedSignal()
    self._score = self._tested_signal.dBFS - self._reference_signal.dBFS
    self._SaveScore()


@EvaluationScore.RegisterClass
class PolqaScore(EvaluationScore):
  """POLQA score.

  See http://www.polqa.info/.

  Unit: MOS
  Ideal: 4.5
  Worst case: 1.0
  """

  NAME = 'polqa'

  def __init__(self, polqa_bin_filepath):
    EvaluationScore.__init__(self)

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
