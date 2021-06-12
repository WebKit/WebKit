#!/usr/bin/env python3
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to download prebuilt clang binaries."""

from __future__ import division
from __future__ import print_function

import os
import shutil
import subprocess
import stat
import sys
import tarfile
import tempfile
import time

try:
  # Python 3.0 or later
  from urllib.error import HTTPError, URLError
  from urllib.request import urlopen
except ImportError:
  from urllib2 import urlopen, HTTPError, URLError


# CLANG_REVISION and CLANG_SUB_REVISION determine the build of clang
# to use. These should be synced with tools/clang/scripts/update.py in
# Chromium.
CLANG_REVISION = 'llvmorg-13-init-794-g83e2710e'
CLANG_SUB_REVISION = 1

PACKAGE_VERSION = '%s-%s' % (CLANG_REVISION, CLANG_SUB_REVISION)

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
LLVM_BUILD_DIR = os.path.join(THIS_DIR, 'llvm-build')
STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')

# URL for pre-built binaries.
CDS_URL = os.environ.get('CDS_CLANG_BUCKET_OVERRIDE',
    'https://commondatastorage.googleapis.com/chromium-browser-clang')


def DownloadUrl(url, output_file):
  """Download url into output_file."""
  CHUNK_SIZE = 4096
  TOTAL_DOTS = 10
  num_retries = 3
  retry_wait_s = 5  # Doubled at each retry.

  while True:
    try:
      sys.stdout.write('Downloading %s ' % url)
      sys.stdout.flush()
      response = urlopen(url)
      total_size = int(response.headers.get('Content-Length').strip())
      bytes_done = 0
      dots_printed = 0
      while True:
        chunk = response.read(CHUNK_SIZE)
        if not chunk:
          break
        output_file.write(chunk)
        bytes_done += len(chunk)
        num_dots = TOTAL_DOTS * bytes_done // total_size
        sys.stdout.write('.' * (num_dots - dots_printed))
        sys.stdout.flush()
        dots_printed = num_dots
      if bytes_done != total_size:
        raise URLError("only got %d of %d bytes" % (bytes_done, total_size))
      print(' Done.')
      return
    except URLError as e:
      sys.stdout.write('\n')
      print(e)
      if num_retries == 0 or isinstance(e, HTTPError) and e.code == 404:
        raise e
      num_retries -= 1
      print('Retrying in %d s ...' % retry_wait_s)
      time.sleep(retry_wait_s)
      retry_wait_s *= 2


def EnsureDirExists(path):
  if not os.path.exists(path):
    print("Creating directory %s" % path)
    os.makedirs(path)


def DownloadAndUnpack(url, output_dir):
  with tempfile.TemporaryFile() as f:
    DownloadUrl(url, f)
    f.seek(0)
    EnsureDirExists(output_dir)
    tarfile.open(mode='r:gz', fileobj=f).extractall(path=output_dir)


def ReadStampFile(path=STAMP_FILE):
  """Return the contents of the stamp file, or '' if it doesn't exist."""
  try:
    with open(path, 'r') as f:
      return f.read().rstrip()
  except IOError:
    return ''


def WriteStampFile(s, path=STAMP_FILE):
  """Write s to the stamp file."""
  EnsureDirExists(os.path.dirname(path))
  with open(path, 'w') as f:
    f.write(s)
    f.write('\n')


def RmTree(dir):
  """Delete dir."""
  def ChmodAndRetry(func, path, _):
    # Subversion can leave read-only files around.
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      return func(path)
    raise

  shutil.rmtree(dir, onerror=ChmodAndRetry)


def CopyFile(src, dst):
  """Copy a file from src to dst."""
  print("Copying %s to %s" % (src, dst))
  shutil.copy(src, dst)


def UpdateClang():
  cds_file = "clang-%s.tgz" %  PACKAGE_VERSION
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    cds_full_url = CDS_URL + '/Win/' + cds_file
  elif sys.platform.startswith('linux'):
    cds_full_url = CDS_URL + '/Linux_x64/' + cds_file
  else:
    return 0

  print('Updating Clang to %s...' % PACKAGE_VERSION)

  if ReadStampFile() == PACKAGE_VERSION:
    print('Clang is already up to date.')
    return 0

  # Reset the stamp file in case the build is unsuccessful.
  WriteStampFile('')

  print('Downloading prebuilt clang')
  if os.path.exists(LLVM_BUILD_DIR):
    RmTree(LLVM_BUILD_DIR)
  try:
    DownloadAndUnpack(cds_full_url, LLVM_BUILD_DIR)
    print('clang %s unpacked' % PACKAGE_VERSION)
    WriteStampFile(PACKAGE_VERSION)
    return 0
  except URLError:
    print('Failed to download prebuilt clang %s' % cds_file)
    print('Exiting.')
    return 1


def main():
  return UpdateClang()


if __name__ == '__main__':
  sys.exit(main())
