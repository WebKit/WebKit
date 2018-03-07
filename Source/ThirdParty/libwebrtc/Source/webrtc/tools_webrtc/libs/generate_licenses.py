#!/usr/bin/env python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Generates license markdown for a prebuilt version of WebRTC."""

import sys

import argparse
import cgi
import json
import logging
import os
import re
import subprocess


LIB_TO_LICENSES_DICT = {
    'android_tools': ['third_party/android_tools/LICENSE'],
    'auto': ['third_party/auto/src/LICENSE.txt'],
    'boringssl': ['third_party/boringssl/src/LICENSE'],
    'errorprone': ['third_party/errorprone/LICENSE'],
    'expat': ['third_party/expat/files/COPYING'],
    'guava': ['third_party/guava/LICENSE'],
    'ijar': ['third_party/ijar/LICENSE'],
    'jsoncpp': ['third_party/jsoncpp/LICENSE'],
    'libc++': ['buildtools/third_party/libc++/trunk/LICENSE.TXT'],
    'libc++abi': ['buildtools/third_party/libc++abi/trunk/LICENSE.TXT'],
    'libevent': ['base/third_party/libevent/LICENSE'],
    'libjpeg_turbo': ['third_party/libjpeg_turbo/LICENSE.md'],
    'libsrtp': ['third_party/libsrtp/LICENSE'],
    'libvpx': ['third_party/libvpx/source/libvpx/LICENSE'],
    'libyuv': ['third_party/libyuv/LICENSE'],
    'openmax_dl': ['third_party/openmax_dl/LICENSE'],
    'opus': ['third_party/opus/src/COPYING'],
    'protobuf': ['third_party/protobuf/LICENSE'],
    'usrsctp': ['third_party/usrsctp/LICENSE'],
    'webrtc': ['LICENSE', 'LICENSE_THIRD_PARTY'],
    'zlib': ['LICENSE', 'third_party/zlib/LICENSE'],

    # Compile time dependencies, no license needed:
    'yasm': [],
}

SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.append(os.path.join(CHECKOUT_ROOT, 'build'))
import find_depot_tools

THIRD_PARTY_LIB_REGEX = r'^.*/third_party/([\w+]+).*$'

class LicenseBuilder(object):

  def __init__(self, buildfile_dirs, targets):
    self.buildfile_dirs = buildfile_dirs
    self.targets = targets

  @staticmethod
  def _ParseLibrary(dep):
    """
    Returns a regex match containing library name after third_party

    Input one of:
    //a/b/third_party/libname:c
    //a/b/third_party/libname:c(//d/e/f:g)
    //a/b/third_party/libname/c:d(//e/f/g:h)

    Outputs match with libname in group 1 or None if this is not a third_party
    dependency.
    """
    return re.match(THIRD_PARTY_LIB_REGEX, dep)

  @staticmethod
  def _RunGN(buildfile_dir, target):
    cmd = [
      sys.executable,
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py'),
      'desc',
      '--all',
      '--format=json',
      os.path.abspath(buildfile_dir),
      target,
    ]
    logging.debug("Running: %r", cmd)
    output_json = subprocess.check_output(cmd, cwd=CHECKOUT_ROOT)
    logging.debug("Output: %s", output_json)
    return output_json

  @staticmethod
  def _GetThirdPartyLibraries(buildfile_dir, target):
    output = json.loads(LicenseBuilder._RunGN(buildfile_dir, target))
    libraries = set()
    for target in output.values():
      third_party_matches = (
          LicenseBuilder._ParseLibrary(dep) for dep in target['deps'])
      libraries |= set(match.group(1) for match in third_party_matches if match)
    return libraries

  def GenerateLicenseText(self, output_dir):
    # Get a list of third_party libs from gn. For fat libraries we must consider
    # all architectures, hence the multiple buildfile directories.
    third_party_libs = set()
    for buildfile in self.buildfile_dirs:
      for target in self.targets:
        third_party_libs |= LicenseBuilder._GetThirdPartyLibraries(
            buildfile, target)
    assert len(third_party_libs) > 0

    missing_licenses = third_party_libs - set(LIB_TO_LICENSES_DICT.keys())
    if missing_licenses:
      error_msg = 'Missing licenses: %s' % ', '.join(missing_licenses)
      logging.error(error_msg)
      raise Exception(error_msg)

    # Put webrtc at the front of the list.
    license_libs = sorted(third_party_libs)
    license_libs.insert(0, 'webrtc')

    logging.info("List of licenses: %s", ', '.join(license_libs))

    # Generate markdown.
    output_license_file = open(os.path.join(output_dir, 'LICENSE.md'), 'w+')
    for license_lib in license_libs:
      if len(LIB_TO_LICENSES_DICT[license_lib]) == 0:
        logging.info("Skipping compile time dependency: %s", license_lib)
        continue # Compile time dependency

      output_license_file.write('# %s\n' % license_lib)
      output_license_file.write('```\n')
      for path in LIB_TO_LICENSES_DICT[license_lib]:
        license_path = os.path.join(CHECKOUT_ROOT, path)
        with open(license_path, 'r') as license_file:
          license_text = cgi.escape(license_file.read(), quote=True)
          output_license_file.write(license_text)
          output_license_file.write('\n')
      output_license_file.write('```\n\n')

    output_license_file.close()


def main():
  parser = argparse.ArgumentParser(description='Generate WebRTC LICENSE.md')
  parser.add_argument('--verbose', action='store_true', default=False,
                      help='Debug logging.')
  parser.add_argument('--target', required=True, action='append', default=[],
                      help='Name of the GN target to generate a license for')
  parser.add_argument('output_dir',
                      help='Directory to output LICENSE.md to.')
  parser.add_argument('buildfile_dirs', nargs="+",
                      help='Directories containing gn generated ninja files')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  builder = LicenseBuilder(args.buildfile_dirs, args.target)
  builder.GenerateLicenseText(args.output_dir)


if __name__ == '__main__':
  sys.exit(main())
