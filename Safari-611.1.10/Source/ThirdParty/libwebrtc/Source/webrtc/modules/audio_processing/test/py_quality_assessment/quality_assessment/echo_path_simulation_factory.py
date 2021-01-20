# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Echo path simulation factory module.
"""

import numpy as np

from . import echo_path_simulation


class EchoPathSimulatorFactory(object):

  # TODO(alessiob): Replace 20 ms delay (at 48 kHz sample rate) with a more
  # realistic impulse response.
  _LINEAR_ECHO_IMPULSE_RESPONSE = np.array([0.0]*(20 * 48) + [0.15])

  def __init__(self):
    pass

  @classmethod
  def GetInstance(cls, echo_path_simulator_class, render_input_filepath):
    """Creates an EchoPathSimulator instance given a class object.

    Args:
      echo_path_simulator_class: EchoPathSimulator class object (not an
                                 instance).
      render_input_filepath: Path to the render audio track file.

    Returns:
      An EchoPathSimulator instance.
    """
    assert render_input_filepath is not None or (
        echo_path_simulator_class == echo_path_simulation.NoEchoPathSimulator)

    if echo_path_simulator_class == echo_path_simulation.NoEchoPathSimulator:
      return echo_path_simulation.NoEchoPathSimulator()
    elif echo_path_simulator_class == (
        echo_path_simulation.LinearEchoPathSimulator):
      return echo_path_simulation.LinearEchoPathSimulator(
          render_input_filepath, cls._LINEAR_ECHO_IMPULSE_RESPONSE)
    else:
      return echo_path_simulator_class(render_input_filepath)
