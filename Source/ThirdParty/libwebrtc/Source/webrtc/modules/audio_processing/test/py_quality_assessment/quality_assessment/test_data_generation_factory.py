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

from . import test_data_generation


class TestDataGeneratorFactory(object):
  """Factory class used to create test data generators.

  Usage: Create a factory passing parameters to the ctor with which the
  generators will be produced.
  """

  def __init__(self, aechen_ir_database_path):
    self._aechen_ir_database_path = aechen_ir_database_path

  def GetInstance(self, test_data_generators_class):
    """Creates an TestDataGenerator instance given a class object.

    Args:
      test_data_generators_class: TestDataGenerator class object (not an
                                  instance).
    """
    logging.debug('factory producing %s', test_data_generators_class)
    if test_data_generators_class == (
        test_data_generation.ReverberationTestDataGenerator):
      return test_data_generation.ReverberationTestDataGenerator(
          self._aechen_ir_database_path)
    else:
      # By default, no arguments in the constructor.
      return test_data_generators_class()
