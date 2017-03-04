# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import json
import os

def make_directory(path):
  """
  Recursively make a directory without rising exceptions if it already exists.
  """
  if os.path.exists(path):
    return
  os.makedirs(path)


class Metadata(object):
  """
  Data access class to save and load metadata.
  """

  def __init__(self):
    pass

  _AUDIO_IN_REF_FILENAME = 'audio_in_ref.txt'

  @classmethod
  def load_audio_in_ref_paths(cls, metadata_path):
    """
    Metadata loader for input and reference audio track paths.
    """
    metadata_filepath = os.path.join(metadata_path, cls._AUDIO_IN_REF_FILENAME)
    with open(metadata_filepath) as f:
      audio_in_filepath = f.readline().strip()
      audio_ref_filepath = f.readline().strip()
    return audio_in_filepath, audio_ref_filepath

  @classmethod
  def save_audio_in_ref_paths(cls, output_path, audio_in_filepath,
                              audio_ref_filepath):
    """
    Metadata saver for input and reference audio track paths.
    """
    output_filepath = os.path.join(output_path, cls._AUDIO_IN_REF_FILENAME)
    with open(output_filepath, 'w') as f:
      f.write('{}\n{}\n'.format(audio_in_filepath, audio_ref_filepath))


class AudioProcConfigFile(object):
  """
  Data access class to save and load audioproc_f argument lists to control
  the APM flags.
  """

  def __init__(self):
    pass

  @classmethod
  def load(cls, filepath):
    with open(filepath) as f:
      return json.load(f)

  @classmethod
  def save(cls, filepath, config):
    with open(filepath, 'w') as f:
      json.dump(config, f)


class ScoreFile(object):
  """
  Data access class to save and load float scalar scores.
  """

  def __init__(self):
    pass

  @classmethod
  def load(cls, filepath):
    with open(filepath) as f:
      return float(f.readline().strip())

  @classmethod
  def save(cls, filepath, score):
    with open(filepath, 'w') as f:
      f.write('{0:f}\n'.format(score))
