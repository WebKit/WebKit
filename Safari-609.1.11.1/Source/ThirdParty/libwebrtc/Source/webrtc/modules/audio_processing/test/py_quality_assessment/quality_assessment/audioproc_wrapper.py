# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Class implementing a wrapper for APM simulators.
"""

import cProfile
import logging
import os
import subprocess

from . import data_access
from . import exceptions


class AudioProcWrapper(object):
  """Wrapper for APM simulators.
  """

  DEFAULT_APM_SIMULATOR_BIN_PATH = os.path.abspath(os.path.join(
      os.pardir, 'audioproc_f'))
  OUTPUT_FILENAME = 'output.wav'

  def __init__(self, simulator_bin_path):
    """Ctor.

    Args:
      simulator_bin_path: path to the APM simulator binary.
    """
    self._simulator_bin_path = simulator_bin_path
    self._config = None
    self._output_signal_filepath = None

    # Profiler instance to measure running time.
    self._profiler = cProfile.Profile()

  @property
  def output_filepath(self):
    return self._output_signal_filepath

  def Run(self, config_filepath, capture_input_filepath, output_path,
          render_input_filepath=None):
    """Runs APM simulator.

    Args:
      config_filepath: path to the configuration file specifying the arguments
                       for the APM simulator.
      capture_input_filepath: path to the capture audio track input file (aka
                              forward or near-end).
      output_path: path of the audio track output file.
      render_input_filepath: path to the render audio track input file (aka
                             reverse or far-end).
    """
    # Init.
    self._output_signal_filepath = os.path.join(
        output_path, self.OUTPUT_FILENAME)
    profiling_stats_filepath = os.path.join(output_path, 'profiling.stats')

    # Skip if the output has already been generated.
    if os.path.exists(self._output_signal_filepath) and os.path.exists(
        profiling_stats_filepath):
      return

    # Load configuration.
    self._config = data_access.AudioProcConfigFile.Load(config_filepath)

    # Set remaining parameters.
    if not os.path.exists(capture_input_filepath):
      raise exceptions.FileNotFoundError('cannot find capture input file')
    self._config['-i'] = capture_input_filepath
    self._config['-o'] = self._output_signal_filepath
    if render_input_filepath is not None:
      if not os.path.exists(render_input_filepath):
        raise exceptions.FileNotFoundError('cannot find render input file')
      self._config['-ri'] = render_input_filepath

    # Build arguments list.
    args = [self._simulator_bin_path]
    for param_name in self._config:
      args.append(param_name)
      if self._config[param_name] is not None:
        args.append(str(self._config[param_name]))
    logging.debug(' '.join(args))

    # Run.
    self._profiler.enable()
    subprocess.call(args)
    self._profiler.disable()

    # Save profiling stats.
    self._profiler.dump_stats(profiling_stats_filepath)
