# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""TestDataGenerator factory class.
"""

import logging

from . import exceptions
from . import test_data_generation


class TestDataGeneratorFactory(object):
  """Factory class used to create test data generators.

  Usage: Create a factory passing parameters to the ctor with which the
  generators will be produced.
  """

  def __init__(self, aechen_ir_database_path, noise_tracks_path,
               copy_with_identity):
    """Ctor.

    Args:
      aechen_ir_database_path: Path to the Aechen Impulse Response database.
      noise_tracks_path: Path to the noise tracks to add.
      copy_with_identity: Flag indicating whether the identity generator has to
                          make copies of the clean speech input files.
    """
    self._output_directory_prefix = None
    self._aechen_ir_database_path = aechen_ir_database_path
    self._noise_tracks_path = noise_tracks_path
    self._copy_with_identity = copy_with_identity

  def SetOutputDirectoryPrefix(self, prefix):
    self._output_directory_prefix = prefix

  def GetInstance(self, test_data_generators_class):
    """Creates an TestDataGenerator instance given a class object.

    Args:
      test_data_generators_class: TestDataGenerator class object (not an
                                  instance).

    Returns:
      TestDataGenerator instance.
    """
    if self._output_directory_prefix is None:
      raise exceptions.InitializationException(
          'The output directory prefix for test data generators is not set')
    logging.debug('factory producing %s', test_data_generators_class)

    if test_data_generators_class == (
        test_data_generation.IdentityTestDataGenerator):
      return test_data_generation.IdentityTestDataGenerator(
          self._output_directory_prefix, self._copy_with_identity)
    elif test_data_generators_class == (
        test_data_generation.ReverberationTestDataGenerator):
      return test_data_generation.ReverberationTestDataGenerator(
          self._output_directory_prefix, self._aechen_ir_database_path)
    elif test_data_generators_class == (
        test_data_generation.AdditiveNoiseTestDataGenerator):
      return test_data_generation.AdditiveNoiseTestDataGenerator(
          self._output_directory_prefix, self._noise_tracks_path)
    else:
      return test_data_generators_class(self._output_directory_prefix)
