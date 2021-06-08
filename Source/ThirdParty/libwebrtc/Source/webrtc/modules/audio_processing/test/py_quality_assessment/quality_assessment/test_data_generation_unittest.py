# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the test_data_generation module.
"""

import os
import shutil
import tempfile
import unittest

import numpy as np
import scipy.io

from . import test_data_generation
from . import test_data_generation_factory
from . import signal_processing


class TestTestDataGenerators(unittest.TestCase):
  """Unit tests for the test_data_generation module.
  """

  def setUp(self):
    """Create temporary folders."""
    self._base_output_path = tempfile.mkdtemp()
    self._test_data_cache_path = tempfile.mkdtemp()
    self._fake_air_db_path = tempfile.mkdtemp()

    # Fake AIR DB impulse responses.
    # TODO(alessiob): ReverberationTestDataGenerator will change to allow custom
    # impulse responses. When changed, the coupling below between
    # impulse_response_mat_file_names and
    # ReverberationTestDataGenerator._IMPULSE_RESPONSES can be removed.
    impulse_response_mat_file_names = [
        'air_binaural_lecture_0_0_1.mat',
        'air_binaural_booth_0_0_1.mat',
    ]
    for impulse_response_mat_file_name in impulse_response_mat_file_names:
      data = {'h_air': np.random.rand(1, 1000).astype('<f8')}
      scipy.io.savemat(os.path.join(
          self._fake_air_db_path, impulse_response_mat_file_name), data)

  def tearDown(self):
    """Recursively delete temporary folders."""
    shutil.rmtree(self._base_output_path)
    shutil.rmtree(self._test_data_cache_path)
    shutil.rmtree(self._fake_air_db_path)

  def testTestDataGenerators(self):
    # Preliminary check.
    self.assertTrue(os.path.exists(self._base_output_path))
    self.assertTrue(os.path.exists(self._test_data_cache_path))

    # Check that there is at least one registered test data generator.
    registered_classes = (
        test_data_generation.TestDataGenerator.REGISTERED_CLASSES)
    self.assertIsInstance(registered_classes, dict)
    self.assertGreater(len(registered_classes), 0)

    # Instance generators factory.
    generators_factory = test_data_generation_factory.TestDataGeneratorFactory(
        aechen_ir_database_path=self._fake_air_db_path,
        noise_tracks_path=test_data_generation.  \
                          AdditiveNoiseTestDataGenerator.  \
                          DEFAULT_NOISE_TRACKS_PATH,
        copy_with_identity=False)
    generators_factory.SetOutputDirectoryPrefix('datagen-')

    # Use a simple input file as clean input signal.
    input_signal_filepath = os.path.join(
        os.getcwd(), 'probing_signals', 'tone-880.wav')
    self.assertTrue(os.path.exists(input_signal_filepath))

    # Load input signal.
    input_signal = signal_processing.SignalProcessingUtils.LoadWav(
        input_signal_filepath)

    # Try each registered test data generator.
    for generator_name in registered_classes:
      # Instance test data generator.
      generator = generators_factory.GetInstance(
          registered_classes[generator_name])

      # Generate the noisy input - reference pairs.
      generator.Generate(
          input_signal_filepath=input_signal_filepath,
          test_data_cache_path=self._test_data_cache_path,
          base_output_path=self._base_output_path)

      # Perform checks.
      self._CheckGeneratedPairsListSizes(generator)
      self._CheckGeneratedPairsSignalDurations(generator, input_signal)
      self._CheckGeneratedPairsOutputPaths(generator)

  def testTestidentityDataGenerator(self):
    # Preliminary check.
    self.assertTrue(os.path.exists(self._base_output_path))
    self.assertTrue(os.path.exists(self._test_data_cache_path))

    # Use a simple input file as clean input signal.
    input_signal_filepath = os.path.join(
        os.getcwd(), 'probing_signals', 'tone-880.wav')
    self.assertTrue(os.path.exists(input_signal_filepath))

    def GetNoiseReferenceFilePaths(identity_generator):
      noisy_signal_filepaths = identity_generator.noisy_signal_filepaths
      reference_signal_filepaths = identity_generator.reference_signal_filepaths
      assert noisy_signal_filepaths.keys() == reference_signal_filepaths.keys()
      assert len(noisy_signal_filepaths.keys()) == 1
      key = noisy_signal_filepaths.keys()[0]
      return noisy_signal_filepaths[key], reference_signal_filepaths[key]

    # Test the |copy_with_identity| flag.
    for copy_with_identity in [False, True]:
      # Instance the generator through the factory.
      factory = test_data_generation_factory.TestDataGeneratorFactory(
        aechen_ir_database_path='', noise_tracks_path='',
        copy_with_identity=copy_with_identity)
      factory.SetOutputDirectoryPrefix('datagen-')
      generator = factory.GetInstance(
          test_data_generation.IdentityTestDataGenerator)
      # Check |copy_with_identity| is set correctly.
      self.assertEqual(copy_with_identity, generator.copy_with_identity)

      # Generate test data and extract the paths to the noise and the reference
      # files.
      generator.Generate(
          input_signal_filepath=input_signal_filepath,
          test_data_cache_path=self._test_data_cache_path,
          base_output_path=self._base_output_path)
      noisy_signal_filepath, reference_signal_filepath = (
          GetNoiseReferenceFilePaths(generator))

      # Check that a copy is made if and only if |copy_with_identity| is True.
      if copy_with_identity:
        self.assertNotEqual(noisy_signal_filepath, input_signal_filepath)
        self.assertNotEqual(reference_signal_filepath, input_signal_filepath)
      else:
        self.assertEqual(noisy_signal_filepath, input_signal_filepath)
        self.assertEqual(reference_signal_filepath, input_signal_filepath)

  def _CheckGeneratedPairsListSizes(self, generator):
    config_names = generator.config_names
    number_of_pairs = len(config_names)
    self.assertEqual(number_of_pairs,
                     len(generator.noisy_signal_filepaths))
    self.assertEqual(number_of_pairs,
                     len(generator.apm_output_paths))
    self.assertEqual(number_of_pairs,
                     len(generator.reference_signal_filepaths))

  def _CheckGeneratedPairsSignalDurations(
      self, generator, input_signal):
    """Checks duration of the generated signals.

    Checks that the noisy input and the reference tracks are audio files
    with duration equal to or greater than that of the input signal.

    Args:
      generator: TestDataGenerator instance.
      input_signal: AudioSegment instance.
    """
    input_signal_length = (
        signal_processing.SignalProcessingUtils.CountSamples(input_signal))

    # Iterate over the noisy signal - reference pairs.
    for config_name in generator.config_names:
      # Load the noisy input file.
      noisy_signal_filepath = generator.noisy_signal_filepaths[
          config_name]
      noisy_signal = signal_processing.SignalProcessingUtils.LoadWav(
          noisy_signal_filepath)

      # Check noisy input signal length.
      noisy_signal_length = (
          signal_processing.SignalProcessingUtils.CountSamples(noisy_signal))
      self.assertGreaterEqual(noisy_signal_length, input_signal_length)

      # Load the reference file.
      reference_signal_filepath = generator.reference_signal_filepaths[
          config_name]
      reference_signal = signal_processing.SignalProcessingUtils.LoadWav(
          reference_signal_filepath)

      # Check noisy input signal length.
      reference_signal_length = (
          signal_processing.SignalProcessingUtils.CountSamples(
              reference_signal))
      self.assertGreaterEqual(reference_signal_length, input_signal_length)

  def _CheckGeneratedPairsOutputPaths(self, generator):
    """Checks that the output path created by the generator exists.

    Args:
      generator: TestDataGenerator instance.
    """
    # Iterate over the noisy signal - reference pairs.
    for config_name in generator.config_names:
      output_path = generator.apm_output_paths[config_name]
      self.assertTrue(os.path.exists(output_path))
