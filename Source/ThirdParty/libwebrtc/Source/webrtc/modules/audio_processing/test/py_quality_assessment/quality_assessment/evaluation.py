# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import logging

class ApmModuleEvaluator(object):

  def __init__(self):
    pass

  @classmethod
  def run(cls, evaluation_score_workers, apm_output_filepath,
          reference_input_filepath, output_path):
    # Init.
    scores = {}

    for evaluation_score_worker in evaluation_score_workers:
      logging.info('   computing <%s> score', evaluation_score_worker.NAME)
      evaluation_score_worker.set_reference_signal_filepath(
          reference_input_filepath)
      evaluation_score_worker.set_tested_signal_filepath(
          apm_output_filepath)

      evaluation_score_worker.run(output_path)
      scores[evaluation_score_worker.NAME] = evaluation_score_worker.score

    return scores
