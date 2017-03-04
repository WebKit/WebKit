# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import logging
import os

from .data_access import ScoreFile

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
  def register_class(cls, class_to_register):
    """
    Decorator to automatically register the classes that extend EvaluationScore.
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register

  @property
  def output_filepath(self):
    return self._output_filepath

  @property
  def score(self):
    return self._score

  def set_reference_signal_filepath(self, filepath):
    """
    Set the path to the audio track used as reference signal.
    """
    self._reference_signal_filepath = filepath

  def set_tested_signal_filepath(self, filepath):
    """
    Set the path to the audio track used as test signal.
    """
    self._tested_signal_filepath = filepath

  def _load_reference_signal(self):
    assert self._reference_signal_filepath is not None
    # TODO(alessio): load signal.
    self._reference_signal = None

  def _load_tested_signal(self):
    assert self._tested_signal_filepath is not None
    # TODO(alessio): load signal.
    self._tested_signal = None

  def run(self, output_path):
    self._output_filepath = os.path.join(output_path, 'score-{}.txt'.format(
        self.NAME))
    try:
      # If the score has already been computed, load.
      self._load_score()
      logging.debug('score found and loaded')
    except IOError:
      # Compute the score.
      logging.debug('score not found, compute')
      self._run(output_path)

  def _run(self, output_path):
    # Abstract method.
    raise NotImplementedError()

  def _load_score(self):
    return ScoreFile.load(self._output_filepath)

  def _save_score(self):
    return ScoreFile.save(self._output_filepath, self._score)


@EvaluationScore.register_class
class AudioLevelScore(EvaluationScore):
  """
  Compute the difference between the average audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level'

  def __init__(self):
    EvaluationScore.__init__(self)

  def _run(self, output_path):
    # TODO(alessio): implement.
    self._score = 0.0
    self._save_score()


@EvaluationScore.register_class
class PolqaScore(EvaluationScore):
  """
  Compute the POLQA score.

  Unit: MOS
  Ideal: 4.5
  Worst case: 1.0
  """

  NAME = 'polqa'

  def __init__(self):
    EvaluationScore.__init__(self)

  def _run(self, output_path):
    # TODO(alessio): implement.
    self._score = 0.0
    self._save_score()
