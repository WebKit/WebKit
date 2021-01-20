# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Imports a filtered subset of the scores and configurations computed
by apm_quality_assessment.py into a pandas data frame.
"""

import argparse
import glob
import logging
import os
import re
import sys

try:
  import pandas as pd
except ImportError:
  logging.critical('Cannot import the third-party Python package pandas')
  sys.exit(1)

from . import data_access as data_access
from . import simulation as sim

# Compiled regular expressions used to extract score descriptors.
RE_CONFIG_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixApmConfig() + r'(.+)')
RE_CAPTURE_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixCapture() + r'(.+)')
RE_RENDER_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixRender() + r'(.+)')
RE_ECHO_SIM_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixEchoSimulator() + r'(.+)')
RE_TEST_DATA_GEN_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixTestDataGenerator() + r'(.+)')
RE_TEST_DATA_GEN_PARAMS = re.compile(
    sim.ApmModuleSimulator.GetPrefixTestDataGeneratorParameters() + r'(.+)')
RE_SCORE_NAME = re.compile(
    sim.ApmModuleSimulator.GetPrefixScore() + r'(.+)(\..+)')


def InstanceArgumentsParser():
  """Arguments parser factory.
  """
  parser = argparse.ArgumentParser(description=(
      'Override this description in a user script by changing'
      ' `parser.description` of the returned parser.'))

  parser.add_argument('-o', '--output_dir', required=True,
                      help=('the same base path used with the '
                            'apm_quality_assessment tool'))

  parser.add_argument('-c', '--config_names', type=re.compile,
                      help=('regular expression to filter the APM configuration'
                            ' names'))

  parser.add_argument('-i', '--capture_names', type=re.compile,
                      help=('regular expression to filter the capture signal '
                            'names'))

  parser.add_argument('-r', '--render_names', type=re.compile,
                      help=('regular expression to filter the render signal '
                            'names'))

  parser.add_argument('-e', '--echo_simulator_names', type=re.compile,
                      help=('regular expression to filter the echo simulator '
                            'names'))

  parser.add_argument('-t', '--test_data_generators', type=re.compile,
                      help=('regular expression to filter the test data '
                            'generator names'))

  parser.add_argument('-s', '--eval_scores', type=re.compile,
                      help=('regular expression to filter the evaluation score '
                            'names'))

  return parser


def _GetScoreDescriptors(score_filepath):
  """Extracts a score descriptor from the given score file path.

  Args:
    score_filepath: path to the score file.

  Returns:
    A tuple of strings (APM configuration name, capture audio track name,
    render audio track name, echo simulator name, test data generator name,
    test data generator parameters as string, evaluation score name).
  """
  fields = score_filepath.split(os.sep)[-7:]
  extract_name = lambda index, reg_expr: (
      reg_expr.match(fields[index]).groups(0)[0])
  return (
      extract_name(0, RE_CONFIG_NAME),
      extract_name(1, RE_CAPTURE_NAME),
      extract_name(2, RE_RENDER_NAME),
      extract_name(3, RE_ECHO_SIM_NAME),
      extract_name(4, RE_TEST_DATA_GEN_NAME),
      extract_name(5, RE_TEST_DATA_GEN_PARAMS),
      extract_name(6, RE_SCORE_NAME),
  )


def _ExcludeScore(config_name, capture_name, render_name, echo_simulator_name,
                  test_data_gen_name, score_name, args):
  """Decides whether excluding a score.

  A set of optional regular expressions in args is used to determine if the
  score should be excluded (depending on its |*_name| descriptors).

  Args:
    config_name: APM configuration name.
    capture_name: capture audio track name.
    render_name: render audio track name.
    echo_simulator_name: echo simulator name.
    test_data_gen_name: test data generator name.
    score_name: evaluation score name.
    args: parsed arguments.

  Returns:
    A boolean.
  """
  value_regexpr_pairs = [
      (config_name, args.config_names),
      (capture_name, args.capture_names),
      (render_name, args.render_names),
      (echo_simulator_name, args.echo_simulator_names),
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


def FindScores(src_path, args):
  """Given a search path, find scores and return a DataFrame object.

  Args:
    src_path: Search path pattern.
    args: parsed arguments.

  Returns:
    A DataFrame object.
  """
  # Get scores.
  scores = []
  for score_filepath in glob.iglob(src_path):
    # Extract score descriptor fields from the path.
    (config_name,
     capture_name,
     render_name,
     echo_simulator_name,
     test_data_gen_name,
     test_data_gen_params,
     score_name) = _GetScoreDescriptors(score_filepath)

    # Ignore the score if required.
    if _ExcludeScore(
        config_name,
        capture_name,
        render_name,
        echo_simulator_name,
        test_data_gen_name,
        score_name,
        args):
      logging.info(
          'ignored score: %s %s %s %s %s %s',
          config_name,
          capture_name,
          render_name,
          echo_simulator_name,
          test_data_gen_name,
          score_name)
      continue

    # Read metadata and score.
    metadata = data_access.Metadata.LoadAudioTestDataPaths(
        os.path.split(score_filepath)[0])
    score = data_access.ScoreFile.Load(score_filepath)

    # Add a score with its descriptor fields.
    scores.append((
        metadata['clean_capture_input_filepath'],
        metadata['echo_free_capture_filepath'],
        metadata['echo_filepath'],
        metadata['render_filepath'],
        metadata['capture_filepath'],
        metadata['apm_output_filepath'],
        metadata['apm_reference_filepath'],
        config_name,
        capture_name,
        render_name,
        echo_simulator_name,
        test_data_gen_name,
        test_data_gen_params,
        score_name,
        score,
    ))

  return pd.DataFrame(
      data=scores,
      columns=(
          'clean_capture_input_filepath',
          'echo_free_capture_filepath',
          'echo_filepath',
          'render_filepath',
          'capture_filepath',
          'apm_output_filepath',
          'apm_reference_filepath',
          'apm_config',
          'capture',
          'render',
          'echo_simulator',
          'test_data_gen',
          'test_data_gen_params',
          'eval_score_name',
          'score',
      ))


def ConstructSrcPath(args):
  return os.path.join(
      args.output_dir,
      sim.ApmModuleSimulator.GetPrefixApmConfig() + '*',
      sim.ApmModuleSimulator.GetPrefixCapture() + '*',
      sim.ApmModuleSimulator.GetPrefixRender() + '*',
      sim.ApmModuleSimulator.GetPrefixEchoSimulator() + '*',
      sim.ApmModuleSimulator.GetPrefixTestDataGenerator() + '*',
      sim.ApmModuleSimulator.GetPrefixTestDataGeneratorParameters() + '*',
      sim.ApmModuleSimulator.GetPrefixScore() + '*')
