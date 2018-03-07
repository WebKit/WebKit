# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Input mixer module.
"""

import logging
import os

from . import exceptions
from . import signal_processing


class ApmInputMixer(object):
  """Class to mix a set of audio segments down to the APM input."""

  _HARD_CLIPPING_LOG_MSG = 'hard clipping detected in the  mixed signal'

  def __init__(self):
    pass

  @classmethod
  def HardClippingLogMessage(cls):
    """Returns the log message used when hard clipping is detected in the mix.

    This method is mainly intended to be used by the unit tests.
    """
    return cls._HARD_CLIPPING_LOG_MSG

  @classmethod
  def Mix(cls, output_path, capture_input_filepath, echo_filepath):
    """Mixes capture and echo.

    Creates the overall capture input for APM by mixing the "echo-free" capture
    signal with the echo signal (e.g., echo simulated via the
    echo_path_simulation module).

    The echo signal cannot be shorter than the capture signal and the generated
    mix will have the same duration of the capture signal. The latter property
    is enforced in order to let the input of APM and the reference signal
    created by TestDataGenerator have the same length (required for the
    evaluation step).

    Hard-clipping may occur in the mix; a warning is raised when this happens.

    If |echo_filepath| is None, nothing is done and |capture_input_filepath| is
    returned.

    Args:
      speech: AudioSegment instance.
      echo_path: AudioSegment instance or None.

    Returns:
      Path to the mix audio track file.
    """
    if echo_filepath is None:
      return capture_input_filepath

    # Build the mix output file name as a function of the echo file name.
    # This ensures that if the internal parameters of the echo path simulator
    # change, no erroneous cache hit occurs.
    echo_file_name, _ = os.path.splitext(os.path.split(echo_filepath)[1])
    capture_input_file_name, _ = os.path.splitext(
        os.path.split(capture_input_filepath)[1])
    mix_filepath = os.path.join(output_path, 'mix_capture_{}_{}.wav'.format(
        capture_input_file_name, echo_file_name))

    # Create the mix if not done yet.
    mix = None
    if not os.path.exists(mix_filepath):
      echo_free_capture = signal_processing.SignalProcessingUtils.LoadWav(
          capture_input_filepath)
      echo = signal_processing.SignalProcessingUtils.LoadWav(echo_filepath)

      if signal_processing.SignalProcessingUtils.CountSamples(echo) < (
          signal_processing.SignalProcessingUtils.CountSamples(
              echo_free_capture)):
        raise exceptions.InputMixerException(
            'echo cannot be shorter than capture')

      mix = echo_free_capture.overlay(echo)
      signal_processing.SignalProcessingUtils.SaveWav(mix_filepath, mix)

    # Check if hard clipping occurs.
    if mix is None:
      mix = signal_processing.SignalProcessingUtils.LoadWav(mix_filepath)
    if signal_processing.SignalProcessingUtils.DetectHardClipping(mix):
      logging.warning(cls._HARD_CLIPPING_LOG_MSG)

    return mix_filepath
