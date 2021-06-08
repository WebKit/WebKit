#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Shows boxplots of given score for different values of selected
parameters. Can be used to compare scores by audioproc_f flag.

Usage: apm_quality_assessment_boxplot.py -o /path/to/output
                                         -v polqa
                                         -n /path/to/dir/with/apm_configs
                                         -z audioproc_f_arg1 [arg2 ...]

Arguments --config_names, --render_names, --echo_simulator_names,
--test_data_generators, --eval_scores can be used to filter the data
used for plotting.
"""

import collections
import logging
import matplotlib.pyplot as plt
import os

import quality_assessment.data_access as data_access
import quality_assessment.collect_data as collect_data


def InstanceArgumentsParser():
  """Arguments parser factory.
  """
  parser = collect_data.InstanceArgumentsParser()
  parser.description = (
      'Shows boxplot of given score for different values of selected'
      'parameters. Can be used to compare scores by audioproc_f flag')

  parser.add_argument('-v', '--eval_score', required=True,
                      help=('Score name for constructing boxplots'))

  parser.add_argument('-n', '--config_dir', required=False,
                      help=('path to the folder with the configuration files'),
                      default='apm_configs')

  parser.add_argument('-z', '--params_to_plot', required=True,
                      nargs='+', help=('audioproc_f parameter values'
                      'by which to group scores (no leading dash)'))

  return parser


def FilterScoresByParams(data_frame, filter_params, score_name, config_dir):
  """Filters data on the values of one or more parameters.

  Args:
    data_frame: pandas.DataFrame of all used input data.

    filter_params: each config of the input data is assumed to have
      exactly one parameter from `filter_params` defined. Every value
      of the parameters in `filter_params` is a key in the returned
      dict; the associated value is all cells of the data with that
      value of the parameter.

    score_name: Name of score which value is boxplotted. Currently cannot do
      more than one value.

    config_dir: path to dir with APM configs.

  Returns: dictionary, key is a param value, result is all scores for
    that param value (see `filter_params` for explanation).
  """
  results = collections.defaultdict(dict)
  config_names = data_frame['apm_config'].drop_duplicates().values.tolist()

  for config_name in config_names:
    config_json = data_access.AudioProcConfigFile.Load(
        os.path.join(config_dir, config_name + '.json'))
    data_with_config = data_frame[data_frame.apm_config == config_name]
    data_cell_scores = data_with_config[data_with_config.eval_score_name ==
                                        score_name]

    # Exactly one of |params_to_plot| must match:
    (matching_param, ) = [x for x in filter_params if '-' + x in config_json]

    # Add scores for every track to the result.
    for capture_name in data_cell_scores.capture:
      result_score = float(data_cell_scores[data_cell_scores.capture ==
                                            capture_name].score)
      config_dict = results[config_json['-' + matching_param]]
      if capture_name not in config_dict:
        config_dict[capture_name] = {}

      config_dict[capture_name][matching_param] = result_score

  return results


def _FlattenToScoresList(config_param_score_dict):
  """Extracts a list of scores from input data structure.

  Args:
    config_param_score_dict: of the form {'capture_name':
    {'param_name' : score_value,.. } ..}

  Returns: Plain list of all score value present in input data
    structure
  """
  result = []
  for capture_name in config_param_score_dict:
    result += list(config_param_score_dict[capture_name].values())
  return result


def main():
  # Init.
  # TODO(alessiob): INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)
  parser = InstanceArgumentsParser()
  args = parser.parse_args()

  # Get the scores.
  src_path = collect_data.ConstructSrcPath(args)
  logging.debug(src_path)
  scores_data_frame = collect_data.FindScores(src_path, args)

  # Filter the data by `args.params_to_plot`
  scores_filtered = FilterScoresByParams(scores_data_frame,
                                         args.params_to_plot,
                                         args.eval_score,
                                         args.config_dir)

  data_list = sorted(scores_filtered.items())
  data_values = [_FlattenToScoresList(x) for (_, x) in data_list]
  data_labels = [x for (x, _) in data_list]

  _, axes = plt.subplots(nrows=1, ncols=1, figsize=(6, 6))
  axes.boxplot(data_values, labels=data_labels)
  axes.set_ylabel(args.eval_score)
  axes.set_xlabel('/'.join(args.params_to_plot))
  plt.show()


if __name__ == "__main__":
  main()
