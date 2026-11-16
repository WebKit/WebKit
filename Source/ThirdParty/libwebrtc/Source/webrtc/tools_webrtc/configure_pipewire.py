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

PipeWire is built from source (third_party/pipewire).
WirePlumber is taken from the CIPD package at
third_party/wireplumber/linux-amd64.
"""

import os
import subprocess
import sys


def _get_out_dir():
    if len(sys.argv) > 1:
        candidate = os.path.dirname(os.path.abspath(sys.argv[1]))
        if os.path.isdir(candidate):
            return candidate
    return os.getcwd()


def _get_wireplumber_dir():
    script_dir = os.path.dirname(os.path.realpath(__file__))
    src_dir = os.path.dirname(script_dir)
    return os.path.join(src_dir, 'third_party', 'wireplumber', 'linux-amd64')


def main():
    out_dir = _get_out_dir()
    wireplumber_dir = _get_wireplumber_dir()

    if not os.path.isdir(wireplumber_dir):
        print('configure-pipewire: WirePlumber directory not found: %s' %
              wireplumber_dir)
        return 1

    wireplumber_lib_dir = os.path.join(wireplumber_dir, 'lib64')

    env = os.environ
    # dlopen looks for the versioned soname libpipewire-0.3.so.0; GN produces
    # only libpipewire-0.3.so, so create the symlink if it doesn't exist yet.
    versioned = os.path.join(out_dir, 'libpipewire-0.3.so.0')
    if not os.path.exists(versioned):
        os.symlink(os.path.join(out_dir, 'libpipewire-0.3.so'), versioned)

    env['LD_LIBRARY_PATH'] = (out_dir + ':' +
                              os.path.join(out_dir, 'pipewire-0.3') + ':' +
                              wireplumber_lib_dir)
    env['PIPEWIRE_MODULE_DIR'] = os.path.join(out_dir, 'pipewire-0.3')
    env['SPA_PLUGIN_DIR'] = os.path.join(out_dir, 'spa-0.2')
    env['PIPEWIRE_CONFIG_DIR'] = os.path.join(out_dir, 'pipewire-conf')
    env['PIPEWIRE_RUNTIME_DIR'] = '/tmp'
    env['PATH'] = (out_dir + ':' + os.path.join(wireplumber_dir, 'bin') + ':' +
                   env['PATH'])
    env['WIREPLUMBER_CONFIG_DIR'] = os.path.join(wireplumber_dir, 'share',
                                                 'wireplumber')
    env['WIREPLUMBER_DATA_DIR'] = os.path.join(wireplumber_dir, 'share',
                                               'wireplumber')
    env['WIREPLUMBER_MODULE_DIR'] = os.path.join(wireplumber_lib_dir,
                                                 'wireplumber-0.5')

    pipewire_process = subprocess.Popen([os.path.join(out_dir, 'pipewire')])
    wireplumber_process = subprocess.Popen(
        [os.path.join(wireplumber_dir, 'bin', 'wireplumber')])

    return_value = subprocess.call(sys.argv[1:])

    wireplumber_process.terminate()
    pipewire_process.terminate()

    return return_value


if __name__ == '__main__':
    sys.exit(main())
