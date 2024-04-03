#!/usr/bin/env python
#
# Copyright 2019 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Create the asset."""


import argparse
import os
import subprocess
import sys

FILE_DIR = os.path.dirname(os.path.abspath(__file__))
INFRA_BOTS_DIR = os.path.realpath(os.path.join(FILE_DIR, os.pardir, os.pardir))
sys.path.insert(0, INFRA_BOTS_DIR)
import utils


# Remember to also update the go asset when this is updated.
GO_URL = "https://go.dev/dl/go1.18.windows-amd64.zip"


def create_asset(target_dir):
  """Create the asset."""
  with utils.tmp_dir():
    cwd = os.getcwd()
    zipfile = os.path.join(cwd, 'go.zip')
    subprocess.check_call(["wget", '-O', zipfile, GO_URL])
    subprocess.check_call(["unzip", zipfile, "-d", target_dir])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--target_dir', '-t', required=True)
  args = parser.parse_args()
  create_asset(args.target_dir)


if __name__ == '__main__':
  main()
