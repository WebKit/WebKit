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


def _get_pipewire_dir():
    script_dir = os.path.dirname(os.path.realpath(__file__))
    src_dir = os.path.dirname(script_dir)

    pipewire_dir = os.path.join(src_dir, 'third_party', 'pipewire',
                                'linux-amd64')

    return pipewire_dir


def _configure_pipewire_paths(path):
    library_dir = os.path.join(path, 'lib64')
    pipewire_binary_dir = os.path.join(path, 'bin')
    pipewire_config_prefix = os.path.join(path, 'share', 'pipewire')
    pipewire_module_dir = os.path.join(library_dir, 'pipewire-0.3')
    spa_plugin_dir = os.path.join(library_dir, 'spa-0.2')
    wireplumber_config_dir = os.path.join(path, 'share', 'wireplumber')
    wireplumber_data_dir = os.path.join(path, 'share', 'wireplumber')
    wireplumber_module_dir = os.path.join(library_dir, 'wireplumber-0.5')

    env_vars = os.environ
    env_vars['LD_LIBRARY_PATH'] = library_dir
    env_vars['PIPEWIRE_CONFIG_PREFIX'] = pipewire_config_prefix
    env_vars['PIPEWIRE_MODULE_DIR'] = pipewire_module_dir
    env_vars['SPA_PLUGIN_DIR'] = spa_plugin_dir
    env_vars['PIPEWIRE_RUNTIME_DIR'] = '/tmp'
    env_vars['PATH'] = env_vars['PATH'] + ':' + pipewire_binary_dir
    env_vars['WIREPLUMBER_CONFIG_DIR'] = wireplumber_config_dir
    env_vars['WIREPLUMBER_DATA_DIR'] = wireplumber_data_dir
    env_vars['WIREPLUMBER_MODULE_DIR'] = wireplumber_module_dir


def main():
    pipewire_dir = _get_pipewire_dir()

    if not os.path.isdir(pipewire_dir):
        print('configure-pipewire: Couldn\'t find directory %s' % pipewire_dir)
        return 1

    _configure_pipewire_paths(pipewire_dir)

    pipewire_process = subprocess.Popen(["pipewire"], stdout=None)
    wireplumber_process = subprocess.Popen(["wireplumber"], stdout=None)

    return_value = subprocess.call(sys.argv[1:])

    wireplumber_process.terminate()
    pipewire_process.terminate()

    return return_value


if __name__ == '__main__':
    sys.exit(main())
