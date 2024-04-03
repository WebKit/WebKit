#!/usr/bin/env python
#
# Copyright 2016 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Create the asset."""


from __future__ import print_function
import argparse
import fileinput
import os
import shutil
import subprocess
import sys


def create_asset(target_dir):
  """Create the asset."""

  print("Installing some cross-compiling packages. You may be asked for your sudo password")
  subprocess.check_call([
    "sudo","apt-get","install",
    "libstdc++-10-dev-armhf-cross",
    "libgcc-10-dev-armhf-cross",
    "binutils-arm-linux-gnueabihf"
  ])

  # shutil complains if the target directory exists already.
  shutil.rmtree(target_dir)
  shutil.copytree('/usr/arm-linux-gnueabihf', target_dir)
  shutil.copytree('/usr/lib/gcc-cross/arm-linux-gnueabihf/10',
                  os.path.join(target_dir, 'gcc-cross'))

  # Libs needed to link. These were found by trial-and-error.
  shutil.copy('/usr/lib/x86_64-linux-gnu/libbfd-2.37-armhf.so',
              os.path.join(target_dir, 'lib'))
  shutil.copy('/usr/lib/x86_64-linux-gnu/libopcodes-2.37-armhf.so',
              os.path.join(target_dir, 'lib'))
  shutil.copy('/usr/lib/x86_64-linux-gnu/libctf-armhf.so.0',
              os.path.join(target_dir, 'lib'))

  # The file paths in libc.so start off as absolute file
  # paths (e.g. /usr/arm-linux-gnueabihf/lib/libpthread.so.0), which won't
  # work on the bots. We use fileinput to replace just those lines (which
  # start with GROUP). fileinput redirects stdout, so printing here actually
  # writes to the file.
  bad_libc = os.path.join(target_dir, "lib", "libc.so")
  for line in fileinput.input(bad_libc, inplace=True):
    if line.startswith("GROUP"):
      print("GROUP ( libc.so.6 libc_nonshared.a "
             "AS_NEEDED ( ld-linux-armhf.so.3 ) )")
    else:
      print(line)


def main():
  if 'linux' not in sys.platform:
    print('This script only runs on Linux.', file=sys.stderr)
    sys.exit(1)
  parser = argparse.ArgumentParser()
  parser.add_argument('--target_dir', '-t', required=True)
  args = parser.parse_args()
  create_asset(args.target_dir)


if __name__ == '__main__':
  main()
