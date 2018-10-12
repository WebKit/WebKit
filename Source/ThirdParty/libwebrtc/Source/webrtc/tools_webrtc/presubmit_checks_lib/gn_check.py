# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import re
import shutil
import subprocess
import sys
import tempfile


def FindSrcDirPath():
  """Returns the abs path to the src/ dir of the project."""
  src_dir = os.path.dirname(os.path.abspath(__file__))
  while os.path.basename(src_dir) != 'src':
    src_dir = os.path.normpath(os.path.join(src_dir, os.pardir))
  return src_dir


SRC_DIR = FindSrcDirPath()
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools


# GN_ERROR_RE matches the summary of an error output by `gn check`.
# Matches "ERROR" and following lines until it sees an empty line or a line
# containing just underscores.
GN_ERROR_RE = re.compile(r'^ERROR .+(?:\n.*[^_\n].*$)+', re.MULTILINE)


def RunGnCheck(root_dir=None):
  """Runs `gn gen --check` with default args to detect mismatches between
  #includes and dependencies in the BUILD.gn files, as well as general build
  errors.

  Returns a list of error summary strings.
  """
  out_dir = tempfile.mkdtemp('gn')
  try:
    command = [
      sys.executable,
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py'),
      'gen',
      '--check',
      out_dir,
    ]
    subprocess.check_output(command, cwd=root_dir)
  except subprocess.CalledProcessError as err:
    return GN_ERROR_RE.findall(err.output)
  else:
    return []
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)
