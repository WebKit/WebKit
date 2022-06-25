# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

from __future__ import division

import logging
import os
import subprocess
import shutil
import sys
import tempfile

try:
    import numpy as np
except ImportError:
    logging.critical('Cannot import the third-party Python package numpy')
    sys.exit(1)

from . import signal_processing


class ExternalVad(object):
    def __init__(self, path_to_binary, name):
        """Args:
       path_to_binary: path to binary that accepts '-i <wav>', '-o
          <float probabilities>'. There must be one float value per
          10ms audio
       name: a name to identify the external VAD. Used for saving
          the output as extvad_output-<name>.
    """
        self._path_to_binary = path_to_binary
        self.name = name
        assert os.path.exists(self._path_to_binary), (self._path_to_binary)
        self._vad_output = None

    def Run(self, wav_file_path):
        _signal = signal_processing.SignalProcessingUtils.LoadWav(
            wav_file_path)
        if _signal.channels != 1:
            raise NotImplementedError('Multiple-channel'
                                      ' annotations not implemented')
        if _signal.frame_rate != 48000:
            raise NotImplementedError('Frame rates '
                                      'other than 48000 not implemented')

        tmp_path = tempfile.mkdtemp()
        try:
            output_file_path = os.path.join(tmp_path, self.name + '_vad.tmp')
            subprocess.call([
                self._path_to_binary, '-i', wav_file_path, '-o',
                output_file_path
            ])
            self._vad_output = np.fromfile(output_file_path, np.float32)
        except Exception as e:
            logging.error('Error while running the ' + self.name + ' VAD (' +
                          e.message + ')')
        finally:
            if os.path.exists(tmp_path):
                shutil.rmtree(tmp_path)

    def GetVadOutput(self):
        assert self._vad_output is not None
        return self._vad_output

    @classmethod
    def ConstructVadDict(cls, vad_paths, vad_names):
        external_vads = {}
        for path, name in zip(vad_paths, vad_names):
            external_vads[name] = ExternalVad(path, name)
        return external_vads
