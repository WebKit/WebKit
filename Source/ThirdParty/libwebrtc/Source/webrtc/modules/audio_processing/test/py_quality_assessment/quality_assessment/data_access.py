# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Data access utility functions and classes.
"""

import json
import os


def MakeDirectory(path):
  """Makes a directory recursively without rising exceptions if existing.

  Args:
    path: path to the directory to be created.
  """
  if os.path.exists(path):
    return
  os.makedirs(path)


class Metadata(object):
  """Data access class to save and load metadata.
  """

  def __init__(self):
    pass

  _AUDIO_TEST_DATA_FILENAME = 'audio_test_data.txt'

  @classmethod
  def LoadAudioTestDataPaths(cls, metadata_path):
    """Loads the input and the reference audio track paths.

    Args:
      metadata_path: path to the directory containing the metadata file.

    Returns:
      Tuple with the paths to the input and output audio tracks.
    """
    metadata_filepath = os.path.join(
        metadata_path, cls._AUDIO_TEST_DATA_FILENAME)
    with open(metadata_filepath) as f:
      audio_in_filepath = f.readline().strip()
      audio_ref_filepath = f.readline().strip()
    return audio_in_filepath, audio_ref_filepath

  @classmethod
  def SaveAudioTestDataPaths(cls, output_path, audio_in_filepath,
                              audio_ref_filepath):
    """Saves the input and the reference audio track paths.

    Args:
      output_path: path to the directory containing the metadata file.
      audio_in_filepath: path to the input audio track file.
      audio_ref_filepath: path to the reference audio track file.
    """
    output_filepath = os.path.join(output_path, cls._AUDIO_TEST_DATA_FILENAME)
    with open(output_filepath, 'w') as f:
      f.write('{}\n{}\n'.format(audio_in_filepath, audio_ref_filepath))


class AudioProcConfigFile(object):
  """Data access to load/save audioproc_f argument lists.

  The arguments stored in the config files are used to control the APM flags.
  """

  def __init__(self):
    pass

  @classmethod
  def Load(cls, filepath):
    """Loads a configuration file for audioproc_f.

    Args:
      filepath: path to the configuration file.

    Returns:
      A dict containing the configuration.
    """
    with open(filepath) as f:
      return json.load(f)

  @classmethod
  def Save(cls, filepath, config):
    """Saves a configuration file for audioproc_f.

    Args:
      filepath: path to the configuration file.
      config: a dict containing the configuration.
    """
    with open(filepath, 'w') as f:
      json.dump(config, f)


class ScoreFile(object):
  """Data access class to save and load float scalar scores.
  """

  def __init__(self):
    pass

  @classmethod
  def Load(cls, filepath):
    """Loads a score from file.

    Args:
      filepath: path to the score file.

    Returns:
      A float encoding the score.
    """
    with open(filepath) as f:
      return float(f.readline().strip())

  @classmethod
  def Save(cls, filepath, score):
    """Saves a score into a file.

    Args:
      filepath: path to the score file.
      score: float encoding the score.
    """
    with open(filepath, 'w') as f:
      f.write('{0:f}\n'.format(score))
