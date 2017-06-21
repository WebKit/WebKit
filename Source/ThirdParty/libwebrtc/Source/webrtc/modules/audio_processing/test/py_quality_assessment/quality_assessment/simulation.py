# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""APM module simulator.
"""

import logging
import os

from . import data_access
from . import eval_scores
from . import eval_scores_factory
from . import test_data_generation
from . import test_data_generation_factory


class ApmModuleSimulator(object):
  """APM module simulator class.
  """

  _TEST_DATA_GENERATOR_CLASSES = (
      test_data_generation.TestDataGenerator.REGISTERED_CLASSES)
  _EVAL_SCORE_WORKER_CLASSES = eval_scores.EvaluationScore.REGISTERED_CLASSES

  def __init__(self, aechen_ir_database_path, polqa_tool_bin_path,
               ap_wrapper, evaluator):
    # Init.
    self._audioproc_wrapper = ap_wrapper
    self._evaluator = evaluator

    # Instance factory objects.
    self._test_data_generator_factory = (
        test_data_generation_factory.TestDataGeneratorFactory(
            aechen_ir_database_path=aechen_ir_database_path))
    self._evaluation_score_factory = (
        eval_scores_factory.EvaluationScoreWorkerFactory(
            polqa_tool_bin_path=polqa_tool_bin_path))

    # Properties for each run.
    self._base_output_path = None
    self._test_data_generators = None
    self._evaluation_score_workers = None
    self._config_filepaths = None
    self._input_filepaths = None

  def Run(self, config_filepaths, input_filepaths, test_data_generator_names,
          eval_score_names, output_dir):
    """Runs the APM simulation.

    Initializes paths and required instances, then runs all the simulations.

    Args:
      config_filepaths: set of APM configuration files to test.
      input_filepaths: set of input audio track files to test.
      test_data_generator_names: set of test data generator names to test.
      eval_score_names: set of evaluation score names to test.
      output_dir: base path to the output directory for wav files and outcomes.
    """
    self._base_output_path = os.path.abspath(output_dir)

    # Instance test data generators.
    self._test_data_generators = [self._test_data_generator_factory.GetInstance(
        test_data_generators_class=(
            self._TEST_DATA_GENERATOR_CLASSES[name])) for name in (
                test_data_generator_names)]

    # Instance evaluation score workers.
    self._evaluation_score_workers = [
        self._evaluation_score_factory.GetInstance(
            evaluation_score_class=self._EVAL_SCORE_WORKER_CLASSES[name]) for (
                name) in eval_score_names]

    # Set APM configuration file paths.
    self._config_filepaths = self._CreatePathsCollection(config_filepaths)

    # Set probing signal file paths.
    self._input_filepaths = self._CreatePathsCollection(input_filepaths)

    self._SimulateAll()

  def _SimulateAll(self):
    """Runs all the simulations.

    Iterates over the combinations of APM configurations, probing signals, and
    test data generators.
    """
    # Try different APM config files.
    for config_name in self._config_filepaths:
      config_filepath = self._config_filepaths[config_name]

      # Try different probing signal files.
      for input_name in self._input_filepaths:
        input_filepath = self._input_filepaths[input_name]

        # Try different test data generators.
        for test_data_generators in self._test_data_generators:
          logging.info('config: <%s>, input: <%s>, noise: <%s>',
                       config_name, input_name, test_data_generators.NAME)

          # Output path for the input-noise pairs. It is used to cache the noisy
          # copies of the probing signals (shared across some simulations).
          input_noise_cache_path = os.path.join(
              self._base_output_path,
              '_cache',
              'input_{}-noise_{}'.format(input_name, test_data_generators.NAME))
          data_access.MakeDirectory(input_noise_cache_path)
          logging.debug('input-noise cache path: <%s>', input_noise_cache_path)

          # Full output path.
          output_path = os.path.join(
              self._base_output_path,
              'cfg-{}'.format(config_name),
              'input-{}'.format(input_name),
              'gen-{}'.format(test_data_generators.NAME))
          data_access.MakeDirectory(output_path)
          logging.debug('output path: <%s>', output_path)

          self._Simulate(test_data_generators, input_filepath,
                         input_noise_cache_path, output_path, config_filepath)

  def _Simulate(self, test_data_generators, input_filepath,
                input_noise_cache_path, output_path, config_filepath):
    """Runs a single set of simulation.

    Simulates a given combination of APM configuration, probing signal, and
    test data generator. It iterates over the test data generator
    internal configurations.

    Args:
      test_data_generators: TestDataGenerator instance.
      input_filepath: input audio track file to test.
      input_noise_cache_path: path for the noisy audio track files.
      output_path: base output path for the test data generator.
      config_filepath: APM configuration file to test.
    """
    # Generate pairs of noisy input and reference signal files.
    test_data_generators.Generate(
        input_signal_filepath=input_filepath,
        input_noise_cache_path=input_noise_cache_path,
        base_output_path=output_path)

    # For each test data pair, simulate a call and evaluate.
    for config_name in test_data_generators.config_names:
      logging.info(' - test data generator config: <%s>', config_name)

      # APM input and output signal paths.
      noisy_signal_filepath = test_data_generators.noisy_signal_filepaths[
          config_name]
      evaluation_output_path = test_data_generators.apm_output_paths[
          config_name]

      # Simulate a call using the audio processing module.
      self._audioproc_wrapper.Run(
          config_filepath=config_filepath,
          input_filepath=noisy_signal_filepath,
          output_path=evaluation_output_path)

      # Reference signal path for the evaluation step.
      reference_signal_filepath = (
          test_data_generators.reference_signal_filepaths[
              config_name])

      # Evaluate.
      self._evaluator.Run(
          evaluation_score_workers=self._evaluation_score_workers,
          apm_output_filepath=self._audioproc_wrapper.output_filepath,
          reference_input_filepath=reference_signal_filepath,
          output_path=evaluation_output_path)

  @classmethod
  def _CreatePathsCollection(cls, filepaths):
    """Creates a collection of file paths.

    Given a list of file paths, makes a collection with one item for each file
    path. The value is absolute path, the key is the file name without
    extenstion.

    Args:
      filepaths: list of file paths.

    Returns:
      A dict.
    """
    filepaths_collection = {}
    for filepath in filepaths:
      name = os.path.splitext(os.path.split(filepath)[1])[0]
      filepaths_collection[name] = os.path.abspath(filepath)
    return filepaths_collection
