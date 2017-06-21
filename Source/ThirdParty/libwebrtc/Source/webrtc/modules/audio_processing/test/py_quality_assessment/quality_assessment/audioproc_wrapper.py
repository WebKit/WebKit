# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Class implementing a wrapper for audioproc_f.
"""

import cProfile
import logging
import os
import subprocess

from . import data_access


class AudioProcWrapper(object):
  """Wrapper for audioproc_f.
  """

  OUTPUT_FILENAME = 'output.wav'
  _AUDIOPROC_F_BIN_PATH = os.path.abspath('../audioproc_f')

  def __init__(self):
    self._config = None
    self._input_signal_filepath = None
    self._output_signal_filepath = None

    # Profiler instance to measure audioproc_f running time.
    self._profiler = cProfile.Profile()

  @property
  def output_filepath(self):
    return self._output_signal_filepath

  def Run(self, config_filepath, input_filepath, output_path):
    """Run audioproc_f.

    Args:
      config_filepath: path to the configuration file specifing the arguments
                       for audioproc_f.
      input_filepath: path to the audio track input file.
      output_path: path of the audio track output file.
    """
    # Init.
    self._input_signal_filepath = input_filepath
    self._output_signal_filepath = os.path.join(
        output_path, self.OUTPUT_FILENAME)
    profiling_stats_filepath = os.path.join(output_path, 'profiling.stats')

    # Skip if the output has already been generated.
    if os.path.exists(self._output_signal_filepath) and os.path.exists(
        profiling_stats_filepath):
      return

    # Load configuration.
    self._config = data_access.AudioProcConfigFile.Load(config_filepath)

    # Set remaining parametrs.
    self._config['-i'] = self._input_signal_filepath
    self._config['-o'] = self._output_signal_filepath

    # Build arguments list.
    args = [self._AUDIOPROC_F_BIN_PATH]
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
