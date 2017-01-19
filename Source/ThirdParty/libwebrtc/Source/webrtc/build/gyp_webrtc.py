#!/usr/bin/env python
#
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is used to run GYP for WebRTC. It contains selected parts of the
# main function from the src/build/gyp_chromium.py file while other parts are
# reused to minimize code duplication.

import argparse
import gc
import glob
import os
import shlex
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
checkout_root = os.path.abspath(os.path.join(script_dir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(checkout_root, 'build'))
import gyp_chromium
import gyp_helper
import vs_toolchain

sys.path.insert(0, os.path.join(checkout_root, 'tools', 'gyp', 'pylib'))
import gyp


def GetSupplementalFiles():
  """Returns a list of the supplemental files.

  A supplemental file is included in all GYP sources. Such files can be used to
  override default values.
  """
  # Can't use the one in gyp_chromium since the directory location of the root
  # is different.
  return glob.glob(os.path.join(checkout_root, '*', 'supplement.gypi'))


def GetOutputDirectory():
  """Returns the output directory that GYP will use."""

  # Handle command line generator flags.
  parser = argparse.ArgumentParser()
  parser.add_argument('-G', dest='genflags', default=[], action='append')
  genflags = parser.parse_known_args()[0].genflags

  # Handle generator flags from the environment.
  genflags += shlex.split(os.environ.get('GYP_GENERATOR_FLAGS', ''))

  needle = 'output_dir='
  for item in genflags:
    if item.startswith(needle):
      return item[len(needle):]

  return 'out'


def additional_include_files(supplemental_files, args=None):
  """
  Returns a list of additional (.gypi) files to include, without duplicating
  ones that are already specified on the command line. The list of supplemental
  include files is passed in as an argument.
  """
  # Determine the include files specified on the command line.
  # This doesn't cover all the different option formats you can use,
  # but it's mainly intended to avoid duplicating flags on the automatic
  # makefile regeneration which only uses this format.
  specified_includes = set()
  args = args or []
  for arg in args:
    if arg.startswith('-I') and len(arg) > 2:
      specified_includes.add(os.path.realpath(arg[2:]))
  result = []
  def AddInclude(path):
    if os.path.realpath(path) not in specified_includes:
      result.append(path)
  if os.environ.get('GYP_INCLUDE_FIRST') != None:
    AddInclude(os.path.join(checkout_root, os.environ.get('GYP_INCLUDE_FIRST')))
  # Always include Chromium's common.gypi, which we now have a copy of.
  AddInclude(os.path.join(script_dir, 'chromium_common.gypi'))
  # Optionally add supplemental .gypi files if present.
  for supplement in supplemental_files:
    AddInclude(supplement)
  if os.environ.get('GYP_INCLUDE_LAST') != None:
    AddInclude(os.path.join(checkout_root, os.environ.get('GYP_INCLUDE_LAST')))
  return result


def main():
  # Disabling garbage collection saves about 5% processing time. Since this is a
  # short-lived process it's not a problem.
  gc.disable()

  args = sys.argv[1:]

  if int(os.environ.get('GYP_CHROMIUM_NO_ACTION', 0)):
    print 'Skipping gyp_webrtc.py due to GYP_CHROMIUM_NO_ACTION env var.'
    sys.exit(0)

  if 'SKIP_WEBRTC_GYP_ENV' not in os.environ:
    # Update the environment based on webrtc.gyp_env.
    gyp_env_path = os.path.join(os.path.dirname(checkout_root),
                                'webrtc.gyp_env')
    gyp_helper.apply_gyp_environment_from_file(gyp_env_path)

  # This could give false positives since it doesn't actually do real option
  # parsing.  Oh well.
  gyp_file_specified = False
  for arg in args:
    if arg.endswith('.gyp'):
      gyp_file_specified = True
      break

  # If we didn't get a file, assume 'all.gyp' in the root of the checkout.
  if not gyp_file_specified:
    # Because of a bug in gyp, simply adding the abspath to all.gyp doesn't
    # work, but chdir'ing and adding the relative path does. Spooky :/
    os.chdir(checkout_root)
    args.append('all.gyp')

  # There shouldn't be a circular dependency relationship between .gyp files,
  args.append('--no-circular-check')

  # Default to ninja unless GYP_GENERATORS is set.
  if not os.environ.get('GYP_GENERATORS'):
    os.environ['GYP_GENERATORS'] = 'ninja'

  # Enable check for missing sources in GYP files on Windows.
  if sys.platform.startswith('win'):
    gyp_generator_flags = os.getenv('GYP_GENERATOR_FLAGS', '')
    if not 'msvs_error_on_missing_sources' in gyp_generator_flags:
      os.environ['GYP_GENERATOR_FLAGS'] = (
          gyp_generator_flags + ' msvs_error_on_missing_sources=1')

  vs2013_runtime_dll_dirs = None
  if int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')):
    vs2013_runtime_dll_dirs = vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()

  # Enforce gyp syntax checking. This adds about 20% execution time.
  args.append('--check')

  supplemental_includes = GetSupplementalFiles()
  gyp_vars = gyp_chromium.GetGypVars(supplemental_includes)

  # Automatically turn on crosscompile support for platforms that need it.
  if all(('ninja' in os.environ.get('GYP_GENERATORS', ''),
          gyp_vars.get('OS') in ['android', 'ios'],
          'GYP_CROSSCOMPILE' not in os.environ)):
    os.environ['GYP_CROSSCOMPILE'] = '1'

  args.extend(['-I' + i for i in additional_include_files(supplemental_includes,
                                                          args)])

  # Set the gyp depth variable to the root of the checkout.
  args.append('--depth=' + os.path.relpath(checkout_root))

  print 'Updating projects from gyp files...'
  sys.stdout.flush()

  # Off we go...
  gyp_rc = gyp.main(args)

  if vs2013_runtime_dll_dirs:
    # pylint: disable=unpacking-non-sequence
    x64_runtime, x86_runtime = vs2013_runtime_dll_dirs
    vs_toolchain.CopyVsRuntimeDlls(
        os.path.join(checkout_root, GetOutputDirectory()),
        (x86_runtime, x64_runtime))

  sys.exit(gyp_rc)


if __name__ == '__main__':
  sys.exit(main())

