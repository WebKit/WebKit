# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Extraction of annotations from audio files.
"""

from __future__ import division
import logging
import os
import shutil
import struct
import subprocess
import sys
import tempfile

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

from . import external_vad
from . import exceptions
from . import signal_processing


class AudioAnnotationsExtractor(object):
  """Extracts annotations from audio files.
  """

  class VadType(object):
    ENERGY_THRESHOLD = 1  # TODO(alessiob): Consider switching to P56 standard.
    WEBRTC_COMMON_AUDIO = 2  # common_audio/vad/include/vad.h
    WEBRTC_APM = 4  # modules/audio_processing/vad/vad.h

    def __init__(self, value):
      if (not isinstance(value, int)) or not 0 <= value <= 7:
        raise exceptions.InitializationException(
            'Invalid vad type: ' + value)
      self._value = value

    def Contains(self, vad_type):
      return self._value | vad_type == self._value

    def __str__(self):
      vads = []
      if self.Contains(self.ENERGY_THRESHOLD):
        vads.append("energy")
      if self.Contains(self.WEBRTC_COMMON_AUDIO):
        vads.append("common_audio")
      if self.Contains(self.WEBRTC_APM):
        vads.append("apm")
      return "VadType({})".format(", ".join(vads))

  _OUTPUT_FILENAME_TEMPLATE = '{}annotations.npz'

  # Level estimation params.
  _ONE_DB_REDUCTION = np.power(10.0, -1.0 / 20.0)
  _LEVEL_FRAME_SIZE_MS = 1.0
  # The time constants in ms indicate the time it takes for the level estimate
  # to go down/up by 1 db if the signal is zero.
  _LEVEL_ATTACK_MS = 5.0
  _LEVEL_DECAY_MS = 20.0

  # VAD params.
  _VAD_THRESHOLD = 1
  _VAD_WEBRTC_PATH = os.path.join(os.path.dirname(
      os.path.abspath(__file__)), os.pardir, os.pardir)
  _VAD_WEBRTC_COMMON_AUDIO_PATH = os.path.join(_VAD_WEBRTC_PATH, 'vad')

  _VAD_WEBRTC_APM_PATH = os.path.join(
      _VAD_WEBRTC_PATH, 'apm_vad')

  def __init__(self, vad_type, external_vads=None):
    self._signal = None
    self._level = None
    self._level_frame_size = None
    self._common_audio_vad = None
    self._energy_vad = None
    self._apm_vad_probs = None
    self._apm_vad_rms = None
    self._vad_frame_size = None
    self._vad_frame_size_ms = None
    self._c_attack = None
    self._c_decay = None

    self._vad_type = self.VadType(vad_type)
    logging.info('VADs used for annotations: ' + str(self._vad_type))

    if external_vads is None:
      external_vads = {}
    self._external_vads = external_vads

    assert len(self._external_vads) == len(external_vads), (
        'The external VAD names must be unique.')
    for vad in external_vads.values():
      if not isinstance(vad, external_vad.ExternalVad):
        raise exceptions.InitializationException(
            'Invalid vad type: ' + str(type(vad)))
      logging.info('External VAD used for annotation: ' +
                   str(vad.name))

    assert os.path.exists(self._VAD_WEBRTC_COMMON_AUDIO_PATH), \
      self._VAD_WEBRTC_COMMON_AUDIO_PATH
    assert os.path.exists(self._VAD_WEBRTC_APM_PATH), \
      self._VAD_WEBRTC_APM_PATH

  @classmethod
  def GetOutputFileNameTemplate(cls):
    return cls._OUTPUT_FILENAME_TEMPLATE

  def GetLevel(self):
    return self._level

  def GetLevelFrameSize(self):
    return self._level_frame_size

  @classmethod
  def GetLevelFrameSizeMs(cls):
    return cls._LEVEL_FRAME_SIZE_MS

  def GetVadOutput(self, vad_type):
    if vad_type == self.VadType.ENERGY_THRESHOLD:
      return self._energy_vad
    elif vad_type == self.VadType.WEBRTC_COMMON_AUDIO:
      return self._common_audio_vad
    elif vad_type == self.VadType.WEBRTC_APM:
      return (self._apm_vad_probs, self._apm_vad_rms)
    else:
      raise exceptions.InitializationException(
            'Invalid vad type: ' + vad_type)

  def GetVadFrameSize(self):
    return self._vad_frame_size

  def GetVadFrameSizeMs(self):
    return self._vad_frame_size_ms

  def Extract(self, filepath):
    # Load signal.
    self._signal = signal_processing.SignalProcessingUtils.LoadWav(filepath)
    if self._signal.channels != 1:
      raise NotImplementedError('Multiple-channel annotations not implemented')

    # Level estimation params.
    self._level_frame_size = int(self._signal.frame_rate / 1000 * (
        self._LEVEL_FRAME_SIZE_MS))
    self._c_attack = 0.0 if self._LEVEL_ATTACK_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_ATTACK_MS))
    self._c_decay = 0.0 if self._LEVEL_DECAY_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_DECAY_MS))

    # Compute level.
    self._LevelEstimation()

    # Ideal VAD output, it requires clean speech with high SNR as input.
    if self._vad_type.Contains(self.VadType.ENERGY_THRESHOLD):
      # Naive VAD based on level thresholding.
      vad_threshold = np.percentile(self._level, self._VAD_THRESHOLD)
      self._energy_vad = np.uint8(self._level > vad_threshold)
      self._vad_frame_size = self._level_frame_size
      self._vad_frame_size_ms = self._LEVEL_FRAME_SIZE_MS
    if self._vad_type.Contains(self.VadType.WEBRTC_COMMON_AUDIO):
      # WebRTC common_audio/ VAD.
      self._RunWebRtcCommonAudioVad(filepath, self._signal.frame_rate)
    if self._vad_type.Contains(self.VadType.WEBRTC_APM):
      # WebRTC modules/audio_processing/ VAD.
      self._RunWebRtcApmVad(filepath)
    for extvad_name in self._external_vads:
      self._external_vads[extvad_name].Run(filepath)

  def Save(self, output_path, annotation_name=""):
    ext_kwargs = {'extvad_conf-' + ext_vad:
                  self._external_vads[ext_vad].GetVadOutput()
                  for ext_vad in self._external_vads}
    np.savez_compressed(
        file=os.path.join(
            output_path,
            self.GetOutputFileNameTemplate().format(annotation_name)),
        level=self._level,
        level_frame_size=self._level_frame_size,
        level_frame_size_ms=self._LEVEL_FRAME_SIZE_MS,
        vad_output=self._common_audio_vad,
        vad_energy_output=self._energy_vad,
        vad_frame_size=self._vad_frame_size,
        vad_frame_size_ms=self._vad_frame_size_ms,
        vad_probs=self._apm_vad_probs,
        vad_rms=self._apm_vad_rms,
        **ext_kwargs
    )

  def _LevelEstimation(self):
    # Read samples.
    samples = signal_processing.SignalProcessingUtils.AudioSegmentToRawData(
        self._signal).astype(np.float32) / 32768.0
    num_frames = len(samples) // self._level_frame_size
    num_samples = num_frames * self._level_frame_size

    # Envelope.
    self._level = np.max(np.reshape(np.abs(samples[:num_samples]), (
        num_frames, self._level_frame_size)), axis=1)
    assert len(self._level) == num_frames

    # Envelope smoothing.
    smooth = lambda curr, prev, k: (1 - k) * curr  + k * prev
    self._level[0] = smooth(self._level[0], 0.0, self._c_attack)
    for i in range(1, num_frames):
      self._level[i] = smooth(
          self._level[i], self._level[i - 1], self._c_attack if (
              self._level[i] > self._level[i - 1]) else self._c_decay)

  def _RunWebRtcCommonAudioVad(self, wav_file_path, sample_rate):
    self._common_audio_vad = None
    self._vad_frame_size = None

    # Create temporary output path.
    tmp_path = tempfile.mkdtemp()
    output_file_path = os.path.join(
        tmp_path, os.path.split(wav_file_path)[1] + '_vad.tmp')

    # Call WebRTC VAD.
    try:
      subprocess.call([
          self._VAD_WEBRTC_COMMON_AUDIO_PATH,
          '-i', wav_file_path,
          '-o', output_file_path
      ], cwd=self._VAD_WEBRTC_PATH)

      # Read bytes.
      with open(output_file_path, 'rb') as f:
        raw_data = f.read()

      # Parse side information.
      self._vad_frame_size_ms = struct.unpack('B', raw_data[0])[0]
      self._vad_frame_size = self._vad_frame_size_ms * sample_rate / 1000
      assert self._vad_frame_size_ms in [10, 20, 30]
      extra_bits = struct.unpack('B', raw_data[-1])[0]
      assert 0 <= extra_bits <= 8

      # Init VAD vector.
      num_bytes = len(raw_data)
      num_frames = 8 * (num_bytes - 2) - extra_bits  # 8 frames for each byte.
      self._common_audio_vad = np.zeros(num_frames, np.uint8)

      # Read VAD decisions.
      for i, byte in enumerate(raw_data[1:-1]):
        byte = struct.unpack('B', byte)[0]
        for j in range(8 if i < num_bytes - 3 else (8 - extra_bits)):
          self._common_audio_vad[i * 8 + j] = int(byte & 1)
          byte = byte >> 1
    except Exception as e:
      logging.error('Error while running the WebRTC VAD (' + e.message + ')')
    finally:
      if os.path.exists(tmp_path):
        shutil.rmtree(tmp_path)

  def _RunWebRtcApmVad(self, wav_file_path):
    # Create temporary output path.
    tmp_path = tempfile.mkdtemp()
    output_file_path_probs = os.path.join(
        tmp_path, os.path.split(wav_file_path)[1] + '_vad_probs.tmp')
    output_file_path_rms = os.path.join(
        tmp_path, os.path.split(wav_file_path)[1] + '_vad_rms.tmp')

    # Call WebRTC VAD.
    try:
      subprocess.call([
          self._VAD_WEBRTC_APM_PATH,
          '-i', wav_file_path,
          '-o_probs', output_file_path_probs,
          '-o_rms', output_file_path_rms
      ], cwd=self._VAD_WEBRTC_PATH)

      # Parse annotations.
      self._apm_vad_probs = np.fromfile(output_file_path_probs, np.double)
      self._apm_vad_rms = np.fromfile(output_file_path_rms, np.double)
      assert len(self._apm_vad_rms) == len(self._apm_vad_probs)

    except Exception as e:
      logging.error('Error while running the WebRTC APM VAD (' +
                    e.message + ')')
    finally:
      if os.path.exists(tmp_path):
        shutil.rmtree(tmp_path)
