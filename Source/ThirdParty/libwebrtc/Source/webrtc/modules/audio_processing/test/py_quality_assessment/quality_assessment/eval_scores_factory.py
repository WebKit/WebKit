# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""EvaluationScore factory class.
"""

import logging

from . import exceptions
from . import eval_scores


class EvaluationScoreWorkerFactory(object):
  """Factory class used to instantiate evaluation score workers.

  The ctor gets the parametrs that are used to instatiate the evaluation score
  workers.
  """

  def __init__(self, polqa_tool_bin_path):
    self._score_filename_prefix = None
    self._polqa_tool_bin_path = polqa_tool_bin_path

  def SetScoreFilenamePrefix(self, prefix):
    self._score_filename_prefix = prefix

  def GetInstance(self, evaluation_score_class):
    """Creates an EvaluationScore instance given a class object.

    Args:
      evaluation_score_class: EvaluationScore class object (not an instance).

    Returns:
      An EvaluationScore instance.
    """
    if self._score_filename_prefix is None:
      raise exceptions.InitializationException(
          'The score file name prefix for evaluation score workers is not set')
    logging.debug(
        'factory producing a %s evaluation score', evaluation_score_class)

    if evaluation_score_class == eval_scores.PolqaScore:
      return eval_scores.PolqaScore(
          self._score_filename_prefix, self._polqa_tool_bin_path)
    else:
      return evaluation_score_class(self._score_filename_prefix)
