#!/usr/bin/env vpython3
# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""
This script is a wrapper that loads "pipewire" library.
"""

import os
import subprocess
import sys

_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
_SRC_DIR = os.path.dirname(_SCRIPT_DIR)


def _GetPipeWireDir():
  pipewire_dir = os.path.join(_SRC_DIR, 'third_party', 'pipewire',
                              'linux-amd64')

  if not os.path.isdir(pipewire_dir):
    pipewire_dir = None

  return pipewire_dir


def _ConfigurePipeWirePaths(path):
  library_dir = os.path.join(path, 'lib64')
  pipewire_binary_dir = os.path.join(path, 'bin')
  pipewire_config_prefix = os.path.join(path, 'share', 'pipewire')
  pipewire_module_dir = os.path.join(library_dir, 'pipewire-0.3')
  spa_plugin_dir = os.path.join(library_dir, 'spa-0.2')
  media_session_config_dir = os.path.join(pipewire_config_prefix,
                                          'media-session.d')

  env_vars = os.environ
  env_vars['LD_LIBRARY_PATH'] = library_dir
  env_vars['PIPEWIRE_CONFIG_PREFIX'] = pipewire_config_prefix
  env_vars['PIPEWIRE_MODULE_DIR'] = pipewire_module_dir
  env_vars['SPA_PLUGIN_DIR'] = spa_plugin_dir
  env_vars['MEDIA_SESSION_CONFIG_DIR'] = media_session_config_dir
  env_vars['PIPEWIRE_RUNTIME_DIR'] = '/tmp'
  env_vars['PATH'] = env_vars['PATH'] + ':' + pipewire_binary_dir


def main():
  pipewire_dir = _GetPipeWireDir()

  if pipewire_dir is None:
    print('configure-pipewire: Couldn\'t find directory %s' % pipewire_dir)
    return 1

  _ConfigurePipeWirePaths(pipewire_dir)

  pipewire_process = subprocess.Popen(["pipewire"], stdout=None)
  pipewire_media_session_process = subprocess.Popen(["pipewire-media-session"],
                                                    stdout=None)

  return_value = subprocess.call(sys.argv[1:])

  pipewire_media_session_process.terminate()
  pipewire_process.terminate()

  return return_value


if __name__ == '__main__':
  sys.exit(main())
