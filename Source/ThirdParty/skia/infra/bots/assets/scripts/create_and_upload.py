#!/usr/bin/env python
#
# Copyright 2017 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Create the asset and upload it."""


import argparse
import os
import subprocess
import sys

FILE_DIR = os.path.dirname(os.path.abspath(__file__))
INFRA_BOTS_DIR = os.path.realpath(os.path.join(FILE_DIR, os.pardir, os.pardir))
sys.path.insert(0, INFRA_BOTS_DIR)
import utils


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--gsutil')
  args = parser.parse_args()

  with utils.tmp_dir():
    cwd = os.getcwd()
    create_script = os.path.join(FILE_DIR, 'create.py')
    upload_script = os.path.join(FILE_DIR, 'upload.py')

    try:
      subprocess.check_call(['python', create_script, '-t', cwd])
      cmd = ['python', upload_script, '-t', cwd]
      if args.gsutil:
        cmd.extend(['--gsutil', args.gsutil])
      subprocess.check_call(cmd)
    except subprocess.CalledProcessError:
      # Trap exceptions to avoid printing two stacktraces.
      sys.exit(1)


if __name__ == '__main__':
  main()
