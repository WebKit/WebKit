#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Export the scores computed by the apm_quality_assessment.py script into an
   HTML file.
"""

import argparse
import collections
import logging
import glob
import os
import re
import sys

import quality_assessment.audioproc_wrapper as audioproc_wrapper
import quality_assessment.data_access as data_access
import quality_assessment.export as export

# Regular expressions used to derive score descriptors from file paths.
RE_CONFIG_NAME = re.compile(r'cfg-(.+)')
RE_INPUT_NAME = re.compile(r'input-(.+)')
RE_TEST_DATA_GEN_NAME = re.compile(r'gen-(.+)')
RE_SCORE_NAME = re.compile(r'score-(.+)\.txt')


def _InstanceArgumentsParser():
  """Arguments parser factory.
  """
  parser = argparse.ArgumentParser(description=(
      'Exports pre-computed APM module quality assessment results into HTML '
      'tables.'))

  parser.add_argument('-o', '--output_dir', required=True,
                      help=('the same base path used with the '
                            'apm_quality_assessment tool'))

  parser.add_argument('-f', '--filename_suffix',
                      help=('suffix of the exported file'))

  parser.add_argument('-c', '--config_names', type=re.compile,
                      help=('regular expression to filter the APM configuration'
                            ' names'))

  parser.add_argument('-i', '--input_names', type=re.compile,
                      help=('regular expression to filter the probing signal '
                            'names'))

  parser.add_argument('-t', '--test_data_generators', type=re.compile,
                      help=('regular expression to filter the test data '
                            'generator names'))

  parser.add_argument('-e', '--eval_scores', type=re.compile,
                      help=('regular expression to filter the evaluation score '
                            'names'))

  return parser


def _GetScoreDescriptors(score_filepath):
  """Extracts a score descriptor from the given score file path.

  Args:
    score_filepath: path to the score file.

  Returns:
    A tuple of strings (APM configuration name, input audio track name,
    test data generator name, test data generator parameters name,
    evaluation score name).
  """
  (config_name, input_name, test_data_gen_name, test_data_gen_params,
      score_name) = score_filepath.split(os.sep)[-5:]
  config_name = RE_CONFIG_NAME.match(config_name).groups(0)[0]
  input_name = RE_INPUT_NAME.match(input_name).groups(0)[0]
  test_data_gen_name = RE_TEST_DATA_GEN_NAME.match(
      test_data_gen_name).groups(0)[0]
  score_name = RE_SCORE_NAME.match(score_name).groups(0)[0]
  return (config_name, input_name, test_data_gen_name, test_data_gen_params,
          score_name)


def _ExcludeScore(config_name, input_name, test_data_gen_name, score_name,
                  args):
  """Decides whether excluding a score.

  Given a score descriptor, encoded in config_name, input_name,
  test_data_gen_name and score_name, use the corresponding regular expressions
  to determine if the score should be excluded.

  Args:
    config_name: APM configuration name.
    input_name: input audio track name.
    test_data_gen_name: test data generator name.
    score_name: evaluation score name.
    args: parsed arguments.

  Returns:
    A boolean.
  """
  value_regexpr_pairs = [
      (config_name, args.config_names),
      (input_name, args.input_names),
      (test_data_gen_name, args.test_data_generators),
      (score_name, args.eval_scores),
  ]

  # Score accepted if each value matches the corresponding regular expression.
  for value, regexpr in value_regexpr_pairs:
    if regexpr is None:
      continue
    if not regexpr.match(value):
      return True

  return False


def _BuildOutputFilename(filename_suffix):
  """Builds the filename for the exported file.

  Args:
    filename_suffix: suffix for the output file name.

  Returns:
    A string.
  """
  if filename_suffix is None:
    return 'results.html'
  return 'results-{}.html'.format(filename_suffix)


def main():
  # Init.
  logging.basicConfig(level=logging.DEBUG)  # TODO(alessio): INFO once debugged.
  parser = _InstanceArgumentsParser()
  nested_dict = lambda: collections.defaultdict(nested_dict)
  scores = nested_dict()  # Organize the scores in a nested dictionary.

  # Parse command line arguments.
  args = parser.parse_args()

  # Find score files in the output path.
  src_path = os.path.join(
      args.output_dir, 'cfg-*', 'input-*', 'gen-*', '*', 'score-*.txt')
  logging.debug(src_path)
  for score_filepath in glob.iglob(src_path):
    # Extract score descriptors from the path.
    (config_name, input_name, test_data_gen_name, test_data_gen_params,
        score_name) = _GetScoreDescriptors(score_filepath)

    # Ignore the score if required.
    if _ExcludeScore(
        config_name, input_name, test_data_gen_name, score_name, args):
      logging.info('ignored score: %s %s %s %s',
                   config_name, input_name, test_data_gen_name, score_name)
      continue

    # Get metadata.
    score_path, _ = os.path.split(score_filepath)
    audio_in_filepath, audio_ref_filepath = (
        data_access.Metadata.LoadAudioTestDataPaths(score_path))
    audio_out_filepath = os.path.abspath(os.path.join(
        score_path, audioproc_wrapper.AudioProcWrapper.OUTPUT_FILENAME))

    # Add the score to the nested dictionary.
    scores[score_name][config_name][input_name][test_data_gen_name][
        test_data_gen_params] = {
            'score': data_access.ScoreFile.Load(score_filepath),
            'audio_in_filepath': audio_in_filepath,
            'audio_out_filepath': audio_out_filepath,
            'audio_ref_filepath': audio_ref_filepath,
    }

  # Export.
  output_filepath = os.path.join(args.output_dir, _BuildOutputFilename(
      args.filename_suffix))
  exporter = export.HtmlExport(output_filepath)
  exporter.Export(scores)

  logging.info('output file successfully written in %s', output_filepath)
  sys.exit(0)


if __name__ == '__main__':
  main()
