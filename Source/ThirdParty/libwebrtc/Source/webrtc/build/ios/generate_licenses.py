#!/usr/bin/python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Generates license HTML for a prebuilt version of WebRTC for iOS."""

import sys

import argparse
import cgi
import fnmatch
import os
import re
import textwrap


LIB_TO_LICENSES_DICT = {
    'boringssl': ['third_party/boringssl/src/LICENSE'],
    'expat': ['third_party/expat/files/COPYING'],
    'jsoncpp': ['third_party/jsoncpp/LICENSE'],
    'opus': ['third_party/opus/src/COPYING'],
    'protobuf_lite': ['third_party/protobuf/LICENSE'],
    'srtp': ['third_party/libsrtp/srtp/LICENSE'],
    'usrsctplib': ['third_party/usrsctp/LICENSE'],
    'webrtc': ['webrtc/LICENSE', 'webrtc/LICENSE_THIRD_PARTY'],
    'vpx': ['third_party/libvpx/source/libvpx/LICENSE'],
    'yuv': ['third_party/libyuv/LICENSE'],
}

SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                             os.pardir))
TALK_ROOT = os.path.join(CHECKOUT_ROOT, 'talk')
WEBRTC_ROOT = os.path.join(CHECKOUT_ROOT, 'webrtc')


def GetWebRTCGypFilePaths():
  gyp_filepaths = []
  search_roots = [TALK_ROOT, WEBRTC_ROOT]
  for search_root in search_roots:
    for root, _, filenames in os.walk(search_root):
      for filename in fnmatch.filter(filenames, '*.gyp*'):
        gyp_filepaths.append(os.path.join(root, filename))
  return gyp_filepaths


def GetWebRTCTargetNames():
  gyp_filepaths = GetWebRTCGypFilePaths()
  target_names = []
  for gyp_filepath in gyp_filepaths:
    with open(gyp_filepath, 'r') as gyp_file:
      for line in gyp_file:
        match = re.search(r'\'target_name\'.*\'(\w+)\'', line)
        if match:
          target_name = match.group(1)
          target_names.append(target_name)
  return target_names


class LicenseBuilder(object):

  def __init__(self):
    self.webrtc_target_names = GetWebRTCTargetNames()

  def IsWebRTCLib(self, lib_name):
    alternate_lib_name = 'lib' + lib_name
    return (lib_name in self.webrtc_target_names or
            alternate_lib_name in self.webrtc_target_names)

  def GenerateLicenseText(self, static_lib_dir, output_dir):
    # Get a list of libs from the files without their prefix and extension.
    static_libs = []
    for static_lib in os.listdir(static_lib_dir):
      # Skip non libraries.
      if not (static_lib.endswith('.a') and static_lib.startswith('lib')):
        continue
      # Extract library name.
      static_libs.append(static_lib[3:-2])

    # Generate amalgamated list of libraries. Mostly this just collapses the
    # various WebRTC libs names into just 'webrtc'. Will exit with error if a
    # lib is unrecognized.
    license_libs = set()
    for static_lib in static_libs:
      license_lib = 'webrtc' if self.IsWebRTCLib(static_lib) else static_lib
      license_path = LIB_TO_LICENSES_DICT.get(license_lib)
      if license_path is None:
        print 'Missing license path for lib: %s' % license_lib
        return 1
      license_libs.add(license_lib)

    # Put webrtc at the front of the list.
    assert 'webrtc' in license_libs
    license_libs.remove('webrtc')
    license_libs = sorted(license_libs)
    license_libs.insert(0, 'webrtc')

    # Generate HTML.
    output_license_file = open(os.path.join(output_dir, 'LICENSE.html'), 'w+')
    output_license_file.write('<!DOCTYPE html>\n')
    output_license_file.write('<html>\n<head>\n')
    output_license_file.write('<meta charset="UTF-8">\n')
    output_license_file.write('<title>Licenses</title>\n')
    style_tag = textwrap.dedent('''\
    <style>
      body { margin: 0; font-family: sans-serif; }
      pre { background-color: #eeeeee; padding: 1em; white-space: pre-wrap; }
      p { margin: 1em; white-space: nowrap; }
    </style>
    ''')
    output_license_file.write(style_tag)
    output_license_file.write('</head>\n')

    for license_lib in license_libs:
      output_license_file.write('<p>%s<br/></p>\n' % license_lib)
      output_license_file.write('<pre>\n')
      for path in LIB_TO_LICENSES_DICT[license_lib]:
        license_path = os.path.join(CHECKOUT_ROOT, path)
        with open(license_path, 'r') as license_file:
          license_text = cgi.escape(license_file.read(), quote=True)
          output_license_file.write(license_text)
          output_license_file.write('\n')
      output_license_file.write('</pre>\n')

    output_license_file.write('</body>\n')
    output_license_file.write('</html>')
    output_license_file.close()
    return 0


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Generate WebRTC LICENSE.html')
  parser.add_argument('static_lib_dir',
                      help='Directory with built static libraries.')
  parser.add_argument('output_dir',
                      help='Directory to output LICENSE.html to.')
  args = parser.parse_args()
  builder = LicenseBuilder()
  sys.exit(builder.GenerateLicenseText(args.static_lib_dir, args.output_dir))
