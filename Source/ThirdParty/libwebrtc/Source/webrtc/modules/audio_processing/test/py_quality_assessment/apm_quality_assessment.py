#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Perform APM module quality assessment on one or more input files using one or
   more audioproc_f configuration files and one or more test data generators.

Usage: apm_quality_assessment.py -i audio1.wav [audio2.wav ...]
                                 -c cfg1.json [cfg2.json ...]
                                 -n white [echo ...]
                                 -e audio_level [polqa ...]
                                 -o /path/to/output
"""

import argparse
import logging
import os
import sys

import quality_assessment.audioproc_wrapper as audioproc_wrapper
import quality_assessment.eval_scores as eval_scores
import quality_assessment.evaluation as evaluation
import quality_assessment.test_data_generation as test_data_generation
import quality_assessment.simulation as simulation

_TEST_DATA_GENERATOR_CLASSES = (
    test_data_generation.TestDataGenerator.REGISTERED_CLASSES)
_TEST_DATA_GENERATORS_NAMES = _TEST_DATA_GENERATOR_CLASSES.keys()
_EVAL_SCORE_WORKER_CLASSES = eval_scores.EvaluationScore.REGISTERED_CLASSES
_EVAL_SCORE_WORKER_NAMES = _EVAL_SCORE_WORKER_CLASSES.keys()

_DEFAULT_CONFIG_FILE = 'apm_configs/default.json'

_POLQA_BIN_NAME = 'PolqaOem64'


def _InstanceArgumentsParser():
  """Arguments parser factory.
  """
  parser = argparse.ArgumentParser(description=(
      'Perform APM module quality assessment on one or more input files using '
      'one or more audioproc_f configuration files and one or more '
      'test data generators.'))

  parser.add_argument('-c', '--config_files', nargs='+', required=False,
                      help=('path to the configuration files defining the '
                            'arguments with which the audioproc_f tool is '
                            'called'),
                      default=[_DEFAULT_CONFIG_FILE])

  parser.add_argument('-i', '--input_files', nargs='+', required=True,
                      help='path to the input wav files (one or more)')

  parser.add_argument('-t', '--test_data_generators', nargs='+', required=False,
                      help='custom list of test data generators to use',
                      choices=_TEST_DATA_GENERATORS_NAMES,
                      default=_TEST_DATA_GENERATORS_NAMES)

  parser.add_argument('-e', '--eval_scores', nargs='+', required=False,
                      help='custom list of evaluation scores to use',
                      choices=_EVAL_SCORE_WORKER_NAMES,
                      default=_EVAL_SCORE_WORKER_NAMES)

  parser.add_argument('-o', '--output_dir', required=False,
                      help=('base path to the output directory in which the '
                            'output wav files and the evaluation outcomes '
                            'are saved'),
                      default='output')

  parser.add_argument('--polqa_path', required=True,
                      help='path to the POLQA tool')

  parser.add_argument('--air_db_path', required=True,
                      help='path to the Aechen IR database')

  return parser


def main():
  # TODO(alessiob): level = logging.INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)

  parser = _InstanceArgumentsParser()
  args = parser.parse_args()

  simulator = simulation.ApmModuleSimulator(
      aechen_ir_database_path=args.air_db_path,
      polqa_tool_bin_path=os.path.join(args.polqa_path, _POLQA_BIN_NAME),
      ap_wrapper=audioproc_wrapper.AudioProcWrapper(),
      evaluator=evaluation.ApmModuleEvaluator())
  simulator.Run(
      config_filepaths=args.config_files,
      input_filepaths=args.input_files,
      test_data_generator_names=args.test_data_generators,
      eval_score_names=args.eval_scores,
      output_dir=args.output_dir)

  sys.exit(0)


if __name__ == '__main__':
  main()
