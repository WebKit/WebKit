#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Finds the APM configuration that maximizes a provided metric by
parsing the output generated apm_quality_assessment.py.
"""

from __future__ import division

import collections
import logging
import os

import quality_assessment.data_access as data_access
import quality_assessment.collect_data as collect_data

def _InstanceArgumentsParser():
  """Arguments parser factory. Extends the arguments from 'collect_data'
  with a few extra for selecting what parameters to optimize for.
  """
  parser = collect_data.InstanceArgumentsParser()
  parser.description = (
      'Rudimentary optimization of a function over different parameter'
      'combinations.')

  parser.add_argument('-n', '--config_dir', required=False,
                      help=('path to the folder with the configuration files'),
                      default='apm_configs')

  parser.add_argument('-p', '--params', required=True, nargs='+',
                      help=('parameters to parse from the config files in'
                            'config_dir'))

  parser.add_argument('-z', '--params_not_to_optimize', required=False,
                      nargs='+', default=[],
                      help=('parameters from `params` not to be optimized for'))

  return parser


def _ConfigurationAndScores(data_frame, params,
                            params_not_to_optimize, config_dir):
  """Returns a list of all configurations and scores.

  Args:
    data_frame: A pandas data frame with the scores and config name
                returned by _FindScores.
    params: The parameter names to parse from configs the config
            directory

    params_not_to_optimize: The parameter names which shouldn't affect
                            the optimal parameter
                            selection. E.g., fixed settings and not
                            tunable parameters.

    config_dir: Path to folder with config files.

  Returns:
    Dictionary of the form
    {param_combination: [{params: {param1: value1, ...},
                          scores: {score1: value1, ...}}]}.

    The key `param_combination` runs over all parameter combinations
    of the parameters in `params` and not in
    `params_not_to_optimize`. A corresponding value is a list of all
    param combinations for params in `params_not_to_optimize` and
    their scores.
  """
  results = collections.defaultdict(list)
  config_names = data_frame['apm_config'].drop_duplicates().values.tolist()
  score_names = data_frame['eval_score_name'].drop_duplicates().values.tolist()

  # Normalize the scores
  normalization_constants = {}
  for score_name in score_names:
    scores = data_frame[data_frame.eval_score_name == score_name].score
    normalization_constants[score_name] = max(scores)

  params_to_optimize = [p for p in params if p not in params_not_to_optimize]
  param_combination = collections.namedtuple("ParamCombination",
                                            params_to_optimize)

  for config_name in config_names:
    config_json = data_access.AudioProcConfigFile.Load(
        os.path.join(config_dir, config_name + ".json"))
    scores = {}
    data_cell = data_frame[data_frame.apm_config == config_name]
    for score_name in score_names:
      data_cell_scores = data_cell[data_cell.eval_score_name ==
                                   score_name].score
      scores[score_name] = sum(data_cell_scores) / len(data_cell_scores)
      scores[score_name] /= normalization_constants[score_name]

    result = {'scores': scores, 'params': {}}
    config_optimize_params = {}
    for param in params:
      if param in params_to_optimize:
        config_optimize_params[param] = config_json['-' + param]
      else:
        result['params'][param] = config_json['-' + param]

    current_param_combination = param_combination(
        **config_optimize_params)
    results[current_param_combination].append(result)
  return results


def _FindOptimalParameter(configs_and_scores, score_weighting):
  """Finds the config producing the maximal score.

  Args:
    configs_and_scores: structure of the form returned by
                        _ConfigurationAndScores

    score_weighting: a function to weight together all score values of
                     the form [{params: {param1: value1, ...}, scores:
                                {score1: value1, ...}}] into a numeric
                     value
  Returns:
    the config that has the largest values of |score_weighting| applied
    to its scores.
  """

  min_score = float('+inf')
  best_params = None
  for config in configs_and_scores:
    scores_and_params = configs_and_scores[config]
    current_score = score_weighting(scores_and_params)
    if current_score < min_score:
      min_score = current_score
      best_params = config
      logging.debug("Score: %f", current_score)
      logging.debug("Config: %s", str(config))
  return best_params


def _ExampleWeighting(scores_and_configs):
  """Example argument to `_FindOptimalParameter`
  Args:
    scores_and_configs: a list of configs and scores, in the form
                        described in _FindOptimalParameter
  Returns:
    numeric value, the sum of all scores
  """
  res = 0
  for score_config in scores_and_configs:
    res += sum(score_config['scores'].values())
  return res


def main():
  # Init.
  # TODO(alessiob): INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)
  parser = _InstanceArgumentsParser()
  args = parser.parse_args()

  # Get the scores.
  src_path = collect_data.ConstructSrcPath(args)
  logging.debug('Src path <%s>', src_path)
  scores_data_frame = collect_data.FindScores(src_path, args)
  all_scores = _ConfigurationAndScores(scores_data_frame,
                                       args.params,
                                       args.params_not_to_optimize,
                                       args.config_dir)

  opt_param = _FindOptimalParameter(all_scores, _ExampleWeighting)

  logging.info('Optimal parameter combination: <%s>', opt_param)
  logging.info('It\'s score values: <%s>', all_scores[opt_param])

if __name__ == "__main__":
  main()
