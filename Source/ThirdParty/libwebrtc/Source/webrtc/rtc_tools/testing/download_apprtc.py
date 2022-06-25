#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Downloads prebuilt AppRTC and Go from WebRTC storage and unpacks it.

Requires that depot_tools is installed and in the PATH.

It downloads compressed files in the directory where the script lives.
This is because the precondition is that the script lives in the same
directory of the .sha1 files.
"""

import os
import sys

import utils

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _GetGoArchivePathForPlatform():
    archive_extension = 'zip' if utils.GetPlatform() == 'win' else 'tar.gz'
    return os.path.join(utils.GetPlatform(), 'go.%s' % archive_extension)


def main(argv):
    if len(argv) > 2:
        return 'Usage: %s [output_dir]' % argv[0]

    output_dir = os.path.abspath(argv[1]) if len(argv) > 1 else None

    apprtc_zip_path = os.path.join(SCRIPT_DIR, 'prebuilt_apprtc.zip')
    if os.path.isfile(apprtc_zip_path + '.sha1'):
        utils.DownloadFilesFromGoogleStorage(SCRIPT_DIR, auto_platform=False)

    if output_dir is not None:
        utils.RemoveDirectory(os.path.join(output_dir, 'apprtc'))
        utils.UnpackArchiveTo(apprtc_zip_path, output_dir)

    golang_path = os.path.join(SCRIPT_DIR, 'golang')
    golang_zip_path = os.path.join(golang_path, _GetGoArchivePathForPlatform())
    if os.path.isfile(golang_zip_path + '.sha1'):
        utils.DownloadFilesFromGoogleStorage(golang_path)

    if output_dir is not None:
        utils.RemoveDirectory(os.path.join(output_dir, 'go'))
        utils.UnpackArchiveTo(golang_zip_path, output_dir)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
