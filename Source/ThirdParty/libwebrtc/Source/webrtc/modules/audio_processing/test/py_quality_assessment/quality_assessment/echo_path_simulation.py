# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Echo path simulation module.
"""

import hashlib
import os

from . import signal_processing


class EchoPathSimulator(object):
    """Abstract class for the echo path simulators.

  In general, an echo path simulator is a function of the render signal and
  simulates the propagation of the latter into the microphone (e.g., due to
  mechanical or electrical paths).
  """

    NAME = None
    REGISTERED_CLASSES = {}

    def __init__(self):
        pass

    def Simulate(self, output_path):
        """Creates the echo signal and stores it in an audio file (abstract method).

    Args:
      output_path: Path in which any output can be saved.

    Returns:
      Path to the generated audio track file or None if no echo is present.
    """
        raise NotImplementedError()

    @classmethod
    def RegisterClass(cls, class_to_register):
        """Registers an EchoPathSimulator implementation.

    Decorator to automatically register the classes that extend
    EchoPathSimulator.
    Example usage:

    @EchoPathSimulator.RegisterClass
    class NoEchoPathSimulator(EchoPathSimulator):
      pass
    """
        cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register
        return class_to_register


@EchoPathSimulator.RegisterClass
class NoEchoPathSimulator(EchoPathSimulator):
    """Simulates absence of echo."""

    NAME = 'noecho'

    def __init__(self):
        EchoPathSimulator.__init__(self)

    def Simulate(self, output_path):
        return None


@EchoPathSimulator.RegisterClass
class LinearEchoPathSimulator(EchoPathSimulator):
    """Simulates linear echo path.

  This class applies a given impulse response to the render input and then it
  sums the signal to the capture input signal.
  """

    NAME = 'linear'

    def __init__(self, render_input_filepath, impulse_response):
        """
    Args:
      render_input_filepath: Render audio track file.
      impulse_response: list or numpy vector of float values.
    """
        EchoPathSimulator.__init__(self)
        self._render_input_filepath = render_input_filepath
        self._impulse_response = impulse_response

    def Simulate(self, output_path):
        """Simulates linear echo path."""
        # Form the file name with a hash of the impulse response.
        impulse_response_hash = hashlib.sha256(
            str(self._impulse_response).encode('utf-8', 'ignore')).hexdigest()
        echo_filepath = os.path.join(
            output_path, 'linear_echo_{}.wav'.format(impulse_response_hash))

        # If the simulated echo audio track file does not exists, create it.
        if not os.path.exists(echo_filepath):
            render = signal_processing.SignalProcessingUtils.LoadWav(
                self._render_input_filepath)
            echo = signal_processing.SignalProcessingUtils.ApplyImpulseResponse(
                render, self._impulse_response)
            signal_processing.SignalProcessingUtils.SaveWav(
                echo_filepath, echo)

        return echo_filepath


@EchoPathSimulator.RegisterClass
class RecordedEchoPathSimulator(EchoPathSimulator):
    """Uses recorded echo.

  This class uses the clean capture input file name to build the file name of
  the corresponding recording containing echo (a predefined suffix is used).
  Such a file is expected to be already existing.
  """

    NAME = 'recorded'

    _FILE_NAME_SUFFIX = '_echo'

    def __init__(self, render_input_filepath):
        EchoPathSimulator.__init__(self)
        self._render_input_filepath = render_input_filepath

    def Simulate(self, output_path):
        """Uses recorded echo path."""
        path, file_name_ext = os.path.split(self._render_input_filepath)
        file_name, file_ext = os.path.splitext(file_name_ext)
        echo_filepath = os.path.join(
            path, '{}{}{}'.format(file_name, self._FILE_NAME_SUFFIX, file_ext))
        assert os.path.exists(echo_filepath), (
            'cannot find the echo audio track file {}'.format(echo_filepath))
        return echo_filepath
