#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Perform APM module quality assessment on one or more input files using one or
   more audioproc_f configuration files and one or more noise generators.

Usage: apm_quality_assessment.py -i audio1.wav [audio2.wav ...]
                                 -c cfg1.json [cfg2.json ...]
                                 -n white [echo ...]
                                 -e audio_level [polqa ...]
                                 -o /path/to/output
"""

import argparse
import logging
import sys

import quality_assessment.eval_scores as eval_scores
import quality_assessment.noise_generation as noise_generation
import quality_assessment.simulation as simulation

_NOISE_GENERATOR_CLASSES = noise_generation.NoiseGenerator.REGISTERED_CLASSES
_NOISE_GENERATORS_NAMES = _NOISE_GENERATOR_CLASSES.keys()
_EVAL_SCORE_WORKER_CLASSES = eval_scores.EvaluationScore.REGISTERED_CLASSES
_EVAL_SCORE_WORKER_NAMES = _EVAL_SCORE_WORKER_CLASSES.keys()

_DEFAULT_CONFIG_FILE = 'apm_configs/default.json'

def _instance_arguments_parser():
  parser = argparse.ArgumentParser(description=(
      'Perform APM module quality assessment on one or more input files using '
      'one or more audioproc_f configuration files and one or more noise '
      'generators.'))

  parser.add_argument('-c', '--config_files', nargs='+', required=False,
                      help=('path to the configuration files defining the '
                            'arguments with which the audioproc_f tool is '
                            'called'),
                      default=[_DEFAULT_CONFIG_FILE])

  parser.add_argument('-i', '--input_files', nargs='+', required=True,
                      help='path to the input wav files (one or more)')

  parser.add_argument('-n', '--noise_generators', nargs='+', required=False,
                      help='custom list of noise generators to use',
                      choices=_NOISE_GENERATORS_NAMES,
                      default=_NOISE_GENERATORS_NAMES)

  parser.add_argument('-e', '--eval_scores', nargs='+', required=False,
                      help='custom list of evaluation scores to use',
                      choices=_EVAL_SCORE_WORKER_NAMES,
                      default=_EVAL_SCORE_WORKER_NAMES)

  parser.add_argument('-o', '--output_dir', required=False,
                      help=('base path to the output directory in which the '
                            'output wav files and the evaluation outcomes '
                            'are saved'),
                      default='output')

  return parser


def main():
  # TODO(alessiob): level = logging.INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)

  parser = _instance_arguments_parser()
  args = parser.parse_args()

  simulator = simulation.ApmModuleSimulator()
  simulator.run(
      config_filepaths=args.config_files,
      input_filepaths=args.input_files,
      noise_generator_names=args.noise_generators,
      eval_score_names=args.eval_scores,
      output_dir=args.output_dir)

  sys.exit(0)


if __name__ == '__main__':
  main()
