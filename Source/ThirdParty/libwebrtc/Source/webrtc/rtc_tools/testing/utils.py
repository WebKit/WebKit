#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Utilities for all our deps-management stuff."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile


def RunSubprocessWithRetry(cmd):
    """Invokes the subprocess and backs off exponentially on fail."""
    for i in range(5):
        try:
            subprocess.check_call(cmd)
            return
        except subprocess.CalledProcessError as exception:
            backoff = pow(2, i)
            print('Got %s, retrying in %d seconds...' % (exception, backoff))
            time.sleep(backoff)

    print('Giving up.')
    raise exception


def DownloadFilesFromGoogleStorage(path, auto_platform=True):
    print('Downloading files in %s...' % path)

    extension = 'bat' if 'win32' in sys.platform else 'py'
    cmd = [
        'download_from_google_storage.%s' % extension,
        '--bucket=chromium-webrtc-resources', '--directory', path
    ]
    if auto_platform:
        cmd += ['--auto_platform', '--recursive']
    subprocess.check_call(cmd)


# Code partially copied from
# https://cs.chromium.org#chromium/build/scripts/common/chromium_utils.py
def RemoveDirectory(*path):
    """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
    file_path = os.path.join(*path)
    print('Deleting `{}`.'.format(file_path))
    if not os.path.exists(file_path):
        print('`{}` does not exist.'.format(file_path))
        return

    if sys.platform == 'win32':
        # Give up and use cmd.exe's rd command.
        file_path = os.path.normcase(file_path)
        for _ in range(3):
            print('RemoveDirectory running %s' %
                  (' '.join(['cmd.exe', '/c', 'rd', '/q', '/s', file_path])))
            if not subprocess.call(
                ['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
                break
            print('  Failed')
            time.sleep(3)
        return
    else:
        shutil.rmtree(file_path, ignore_errors=True)


def UnpackArchiveTo(archive_path, output_dir):
    extension = os.path.splitext(archive_path)[1]
    if extension == '.zip':
        _UnzipArchiveTo(archive_path, output_dir)
    else:
        _UntarArchiveTo(archive_path, output_dir)


def _UnzipArchiveTo(archive_path, output_dir):
    print('Unzipping {} in {}.'.format(archive_path, output_dir))
    zip_file = zipfile.ZipFile(archive_path)
    try:
        zip_file.extractall(output_dir)
    finally:
        zip_file.close()


def _UntarArchiveTo(archive_path, output_dir):
    print('Untarring {} in {}.'.format(archive_path, output_dir))
    tar_file = tarfile.open(archive_path, 'r:gz')
    try:
        tar_file.extractall(output_dir)
    finally:
        tar_file.close()


def GetPlatform():
    if sys.platform.startswith('win'):
        return 'win'
    if sys.platform.startswith('linux'):
        return 'linux'
    if sys.platform.startswith('darwin'):
        return 'mac'
    raise Exception("Can't run on platform %s." % sys.platform)


def GetExecutableExtension():
    return '.exe' if GetPlatform() == 'win' else ''
