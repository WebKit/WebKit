#!/usr/bin/env vpython3

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""
This script builds a GN executable targeting the host machine.

It is useful, for example, for mobile devices performance testing where
it makes sense to build WebRTC for a mobile platform (e.g. Android) but
part of the test is performed on the host machine (e.g. running an
executable to analyze a video downloaded from a device).

The script has only one (mandatory) option: --executable_name, which is
the output name of the GN executable. For example, if you have the
following executable in your out folder:

  out/Debug/random_exec

You will be able to compile the same executable targeting your host machine
by running:

  $ vpython3 tools_webrtc/executable_host_build.py --executable_name random_exec

The generated executable will have the same name as the input executable with
suffix '_host'.

This script should not be used standalone but from GN, through an action:

  action("random_exec_host") {
    script = "//tools_webrtc/executable_host_build.py"
    outputs = [
      "${root_out_dir}/random_exec_host",
    ]
    args = [
      "--executable_name",
      "random_exec",
    ]
  }

The executable for the host machine will be generated in the GN out directory
(e.g. out/Debug in the previous example).
"""

from contextlib import contextmanager

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir))
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools


def _ParseArgs():
  desc = 'Generates a GN executable targeting the host machine.'
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument('--executable_name',
                      required=True,
                      help='Name of the executable to build')
  args = parser.parse_args()
  return args


@contextmanager
def HostBuildDir():
  temp_dir = tempfile.mkdtemp()
  try:
    yield temp_dir
  finally:
    shutil.rmtree(temp_dir)


def _RunCommand(argv, cwd=SRC_DIR, **kwargs):
  with open(os.devnull, 'w') as devnull:
    subprocess.check_call(argv, cwd=cwd, stdout=devnull, **kwargs)


def DepotToolPath(*args):
  return os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, *args)


if __name__ == '__main__':
  ARGS = _ParseArgs()
  EXECUTABLE_TO_BUILD = ARGS.executable_name
  EXECUTABLE_FINAL_NAME = ARGS.executable_name + '_host'
  with HostBuildDir() as build_dir:
    _RunCommand([sys.executable, DepotToolPath('gn.py'), 'gen', build_dir])
    _RunCommand([DepotToolPath('ninja'), '-C', build_dir, EXECUTABLE_TO_BUILD])
    shutil.copy(os.path.join(build_dir, EXECUTABLE_TO_BUILD),
                EXECUTABLE_FINAL_NAME)
