# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Evaluator of the APM module.
"""

import logging


class ApmModuleEvaluator(object):
  """APM evaluator class.
  """

  def __init__(self):
    pass

  @classmethod
  def Run(cls, evaluation_score_workers, apm_input_metadata,
          apm_output_filepath, reference_input_filepath, output_path):
    """Runs the evaluation.

    Iterates over the given evaluation score workers.

    Args:
      evaluation_score_workers: list of EvaluationScore instances.
      apm_input_metadata: dictionary with metadata of the APM input.
      apm_output_filepath: path to the audio track file with the APM output.
      reference_input_filepath: path to the reference audio track file.
      output_path: output path.

    Returns:
      A dict of evaluation score name and score pairs.
    """
    # Init.
    scores = {}

    for evaluation_score_worker in evaluation_score_workers:
      logging.info('   computing <%s> score', evaluation_score_worker.NAME)
      evaluation_score_worker.SetInputSignalMetadata(apm_input_metadata)
      evaluation_score_worker.SetReferenceSignalFilepath(
          reference_input_filepath)
      evaluation_score_worker.SetTestedSignalFilepath(
          apm_output_filepath)

      evaluation_score_worker.Run(output_path)
      scores[evaluation_score_worker.NAME] = evaluation_score_worker.score

    return scores
