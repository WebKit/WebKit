#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Perform APM module quality assessment on one or more input files using one or
   more APM simulator configuration files and one or more test data generators.

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
import quality_assessment.echo_path_simulation as echo_path_simulation
import quality_assessment.eval_scores as eval_scores
import quality_assessment.evaluation as evaluation
import quality_assessment.eval_scores_factory as eval_scores_factory
import quality_assessment.external_vad as external_vad
import quality_assessment.test_data_generation as test_data_generation
import quality_assessment.test_data_generation_factory as  \
    test_data_generation_factory
import quality_assessment.simulation as simulation

_ECHO_PATH_SIMULATOR_NAMES = (
    echo_path_simulation.EchoPathSimulator.REGISTERED_CLASSES)
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
      'one or more APM simulator configuration files and one or more '
      'test data generators.'))

  parser.add_argument('-c', '--config_files', nargs='+', required=False,
                      help=('path to the configuration files defining the '
                            'arguments with which the APM simulator tool is '
                            'called'),
                      default=[_DEFAULT_CONFIG_FILE])

  parser.add_argument('-i', '--capture_input_files', nargs='+', required=True,
                      help='path to the capture input wav files (one or more)')

  parser.add_argument('-r', '--render_input_files', nargs='+', required=False,
                      help=('path to the render input wav files; either '
                            'omitted or one file for each file in '
                            '--capture_input_files (files will be paired by '
                            'index)'), default=None)

  parser.add_argument('-p', '--echo_path_simulator', required=False,
                      help=('custom echo path simulator name; required if '
                            '--render_input_files is specified'),
                      choices=_ECHO_PATH_SIMULATOR_NAMES,
                      default=echo_path_simulation.NoEchoPathSimulator.NAME)

  parser.add_argument('-t', '--test_data_generators', nargs='+', required=False,
                      help='custom list of test data generators to use',
                      choices=_TEST_DATA_GENERATORS_NAMES,
                      default=_TEST_DATA_GENERATORS_NAMES)

  parser.add_argument('--additive_noise_tracks_path', required=False,
                      help='path to the wav files for the additive',
                      default=test_data_generation.  \
                              AdditiveNoiseTestDataGenerator.  \
                              DEFAULT_NOISE_TRACKS_PATH)

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

  parser.add_argument('--apm_sim_path', required=False,
                      help='path to the APM simulator tool',
                      default=audioproc_wrapper.  \
                              AudioProcWrapper.  \
                              DEFAULT_APM_SIMULATOR_BIN_PATH)

  parser.add_argument('--copy_with_identity_generator', required=False,
                      help=('If true, the identity test data generator makes a '
                            'copy of the clean speech input file.'),
                      default=False)

  parser.add_argument('--external_vad_paths', nargs='+', required=False,
                      help=('Paths to external VAD programs. Each must take'
                            '\'-i <wav file> -o <output>\' inputs'), default=[])

  parser.add_argument('--external_vad_names', nargs='+', required=False,
                      help=('Keys to the vad paths. Must be different and '
                            'as many as the paths.'), default=[])

  return parser


def _ValidateArguments(args, parser):
  if args.capture_input_files and args.render_input_files and (
      len(args.capture_input_files) != len(args.render_input_files)):
    parser.error('--render_input_files and --capture_input_files must be lists '
                 'having the same length')
    sys.exit(1)

  if args.render_input_files and not args.echo_path_simulator:
    parser.error('when --render_input_files is set, --echo_path_simulator is '
                 'also required')
    sys.exit(1)

  if len(args.external_vad_names) != len(args.external_vad_paths):
    parser.error('If provided, --external_vad_paths and '
                 '--external_vad_names must '
                 'have the same number of arguments.')
    sys.exit(1)


def main():
  # TODO(alessiob): level = logging.INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)
  parser = _InstanceArgumentsParser()
  args = parser.parse_args()
  _ValidateArguments(args, parser)

  simulator = simulation.ApmModuleSimulator(
      test_data_generator_factory=(
          test_data_generation_factory.TestDataGeneratorFactory(
              aechen_ir_database_path=args.air_db_path,
              noise_tracks_path=args.additive_noise_tracks_path,
              copy_with_identity=args.copy_with_identity_generator)),
      evaluation_score_factory=eval_scores_factory.EvaluationScoreWorkerFactory(
          polqa_tool_bin_path=os.path.join(args.polqa_path, _POLQA_BIN_NAME)),
      ap_wrapper=audioproc_wrapper.AudioProcWrapper(args.apm_sim_path),
      evaluator=evaluation.ApmModuleEvaluator(),
      external_vads=external_vad.ExternalVad.ConstructVadDict(
          args.external_vad_paths, args.external_vad_names))
  simulator.Run(
      config_filepaths=args.config_files,
      capture_input_filepaths=args.capture_input_files,
      render_input_filepaths=args.render_input_files,
      echo_path_simulator_name=args.echo_path_simulator,
      test_data_generator_names=args.test_data_generators,
      eval_score_names=args.eval_scores,
      output_dir=args.output_dir)
  sys.exit(0)


if __name__ == '__main__':
  main()
