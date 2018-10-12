#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""WebRTC iOS FAT libraries build script.
Each architecture is compiled separately before being merged together.
By default, the library is created in out_ios_libs/. (Change with -o.)
"""

import argparse
import distutils.dir_util
import logging
import os
import shutil
import subprocess
import sys


os.environ['PATH'] = '/usr/libexec' + os.pathsep + os.environ['PATH']

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools

SDK_OUTPUT_DIR = os.path.join(SRC_DIR, 'out_ios_libs')
SDK_FRAMEWORK_NAME = 'WebRTC.framework'

DEFAULT_ARCHS = ENABLED_ARCHS = ['arm64', 'arm', 'x64', 'x86']
IOS_DEPLOYMENT_TARGET = '9.0'
LIBVPX_BUILD_VP9 = False

sys.path.append(os.path.join(SCRIPT_DIR, '..', 'libs'))
from generate_licenses import LicenseBuilder


def _ParseArgs():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--build_config', default='release',
      choices=['debug', 'release'],
      help='The build config. Can be "debug" or "release". '
           'Defaults to "release".')
  parser.add_argument('--arch', nargs='+', default=DEFAULT_ARCHS,
      choices=ENABLED_ARCHS,
      help='Architectures to build. Defaults to %(default)s.')
  parser.add_argument('-c', '--clean', action='store_true', default=False,
      help='Removes the previously generated build output, if any.')
  parser.add_argument('-p', '--purify', action='store_true', default=False,
      help='Purifies the previously generated build output by '
           'removing the temporary results used when (re)building.')
  parser.add_argument('-o', '--output-dir', default=SDK_OUTPUT_DIR,
      help='Specifies a directory to output the build artifacts to. '
           'If specified together with -c, deletes the dir.')
  parser.add_argument('-r', '--revision', type=int, default=0,
      help='Specifies a revision number to embed if building the framework.')
  parser.add_argument('-e', '--bitcode', action='store_true', default=False,
      help='Compile with bitcode.')
  parser.add_argument('--verbose', action='store_true', default=False,
      help='Debug logging.')
  parser.add_argument('--use-goma', action='store_true', default=False,
      help='Use goma to build.')
  parser.add_argument('--extra-gn-args', default=[], nargs='*',
      help='Additional GN args to be used during Ninja generation.')

  return parser.parse_args()


def _RunCommand(cmd):
  logging.debug('Running: %r', cmd)
  subprocess.check_call(cmd, cwd=SRC_DIR)


def _CleanArtifacts(output_dir):
  if os.path.isdir(output_dir):
    logging.info('Deleting %s', output_dir)
    shutil.rmtree(output_dir)


def _CleanTemporary(output_dir, architectures):
  if os.path.isdir(output_dir):
    logging.info('Removing temporary build files.')
    for arch in architectures:
      arch_lib_path = os.path.join(output_dir, arch + '_libs')
      if os.path.isdir(arch_lib_path):
        shutil.rmtree(arch_lib_path)


def BuildWebRTC(output_dir, target_arch, flavor, gn_target_name,
                ios_deployment_target, libvpx_build_vp9, use_bitcode,
                use_goma, extra_gn_args):
  output_dir = os.path.join(output_dir, target_arch + '_libs')
  gn_args = ['target_os="ios"', 'ios_enable_code_signing=false',
             'use_xcode_clang=true', 'is_component_build=false']

  # Add flavor option.
  if flavor == 'debug':
    gn_args.append('is_debug=true')
  elif flavor == 'release':
    gn_args.append('is_debug=false')
  else:
    raise ValueError('Unexpected flavor type: %s' % flavor)

  gn_args.append('target_cpu="%s"' % target_arch)

  gn_args.append('ios_deployment_target="%s"' % ios_deployment_target)

  gn_args.append('rtc_libvpx_build_vp9=' +
                 ('true' if libvpx_build_vp9 else 'false'))

  gn_args.append('enable_ios_bitcode=' +
                 ('true' if use_bitcode else 'false'))
  gn_args.append('use_goma=' + ('true' if use_goma else 'false'))

  args_string = ' '.join(gn_args + extra_gn_args)
  logging.info('Building WebRTC with args: %s', args_string)

  cmd = [
    sys.executable,
    os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py'),
    'gen',
    output_dir,
    '--args=' + args_string,
  ]
  _RunCommand(cmd)
  logging.info('Building target: %s', gn_target_name)

  cmd = [
    os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'ninja'),
    '-C',
    output_dir,
    gn_target_name,
  ]
  if use_goma:
    cmd.extend(['-j', '200'])
  _RunCommand(cmd)

def main():
  args = _ParseArgs()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  if args.clean:
    _CleanArtifacts(args.output_dir)
    return 0

  architectures = list(args.arch)
  gn_args = args.extra_gn_args

  if args.purify:
    _CleanTemporary(args.output_dir, architectures)
    return 0

  gn_target_name = 'framework_objc'
  if not args.bitcode:
    gn_args.append('enable_dsyms=true')
  gn_args.append('enable_stripping=true')


  # Build all architectures.
  for arch in architectures:
    BuildWebRTC(args.output_dir, arch, args.build_config, gn_target_name,
                IOS_DEPLOYMENT_TARGET, LIBVPX_BUILD_VP9, args.bitcode,
                args.use_goma, gn_args)

  # Create FAT archive.
  lib_paths = [os.path.join(args.output_dir, arch + '_libs')
                for arch in architectures]

  # Combine the slices.
  dylib_path = os.path.join(SDK_FRAMEWORK_NAME, 'WebRTC')
  # Dylibs will be combined, all other files are the same across archs.
  # Use distutils instead of shutil to support merging folders.
  distutils.dir_util.copy_tree(
      os.path.join(lib_paths[0], SDK_FRAMEWORK_NAME),
      os.path.join(args.output_dir, SDK_FRAMEWORK_NAME))
  logging.info('Merging framework slices.')
  dylib_paths = [os.path.join(path, dylib_path) for path in lib_paths]
  out_dylib_path = os.path.join(args.output_dir, dylib_path)
  try:
    os.remove(out_dylib_path)
  except OSError:
    pass
  cmd = ['lipo'] + dylib_paths + ['-create', '-output', out_dylib_path]
  _RunCommand(cmd)

  # Merge the dSYM slices.
  lib_dsym_dir_path = os.path.join(lib_paths[0], 'WebRTC.dSYM')
  if os.path.isdir(lib_dsym_dir_path):
    distutils.dir_util.copy_tree(lib_dsym_dir_path,
                                  os.path.join(args.output_dir, 'WebRTC.dSYM'))
    logging.info('Merging dSYM slices.')
    dsym_path = os.path.join('WebRTC.dSYM', 'Contents', 'Resources', 'DWARF',
                              'WebRTC')
    lib_dsym_paths = [os.path.join(path, dsym_path) for path in lib_paths]
    out_dsym_path = os.path.join(args.output_dir, dsym_path)
    try:
      os.remove(out_dsym_path)
    except OSError:
      pass
    cmd = ['lipo'] + lib_dsym_paths + ['-create', '-output', out_dsym_path]
    _RunCommand(cmd)

    # Generate the license file.
    ninja_dirs = [os.path.join(args.output_dir, arch + '_libs')
                  for arch in architectures]
    gn_target_full_name = '//sdk:' + gn_target_name
    builder = LicenseBuilder(ninja_dirs, [gn_target_full_name])
    builder.GenerateLicenseText(
        os.path.join(args.output_dir, SDK_FRAMEWORK_NAME))


    # Modify the version number.
    # Format should be <Branch cut MXX>.<Hotfix #>.<Rev #>.
    # e.g. 55.0.14986 means branch cut 55, no hotfixes, and revision 14986.
    infoplist_path = os.path.join(args.output_dir, SDK_FRAMEWORK_NAME,
                                  'Info.plist')
    cmd = ['PlistBuddy', '-c',
           'Print :CFBundleShortVersionString', infoplist_path]
    major_minor = subprocess.check_output(cmd).strip()
    version_number = '%s.%s' % (major_minor, args.revision)
    logging.info('Substituting revision number: %s', version_number)
    cmd = ['PlistBuddy', '-c',
           'Set :CFBundleVersion ' + version_number, infoplist_path]
    _RunCommand(cmd)
    _RunCommand(['plutil', '-convert', 'binary1', infoplist_path])

  logging.info('Done.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
