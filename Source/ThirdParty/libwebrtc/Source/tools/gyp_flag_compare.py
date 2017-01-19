#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Given the output of -t commands from a ninja build for a gyp and GN generated
build, report on differences between the command lines.


When invoked from the command line, this script assumes that the GN and GYP
targets have been generated in the specified folders. It is meant to be used as
follows:
  $ python tools/gyp_flag_compare.py gyp_dir gn_dir target

When the GN and GYP target names differ, it should be called invoked as follows:
  $ python tools/gyp_flag_compare.py gyp_dir gn_dir gyp_target gn_target

When all targets want to be compared, it should be called without a target name,
i.e.:
  $ python tools/gyp_flag_compare.py gyp_dir gn_dir

This script can also be used interactively. Then ConfigureBuild can optionally
be used to generate ninja files with GYP and GN.
Here's an example setup. Note that the current working directory must be the
project root:
  $ PYTHONPATH=tools python
  >>> import sys
  >>> import pprint
  >>> sys.displayhook = pprint.pprint
  >>> import gyp_flag_compare as fc
  >>> fc.ConfigureBuild(['gyp_define=1', 'define=2'], ['gn_arg=1', 'arg=2'])
  >>> modules_unittests = fc.Comparison('modules_unittests')

The above starts interactive Python, sets up the output to be pretty-printed
(useful for making lists, dicts, and sets readable), configures the build with
GN arguments and GYP defines, and then generates a comparison for that build
configuration for the "modules_unittests" target.

After that, the |modules_unittests| object can be used to investigate
differences in the build.

To configure an official build, use this configuration. Disabling NaCl produces
a more meaningful comparison, as certain files need to get compiled twice
for the IRT build, which uses different flags:
  >>> fc.ConfigureBuild(
          ['disable_nacl=1', 'buildtype=Official', 'branding=Chrome'],
          ['enable_nacl=false', 'is_official_build=true',
           'is_chrome_branded=true'])
"""


import os
import shlex
import subprocess
import sys

# Must be in src/.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(BASE_DIR)

_DEFAULT_GN_DIR = 'out/gn'
_DEFAULT_GYP_DIR = 'out/Release'


def FilterChromium(filename):
  """Replaces 'chromium/src/' by '' in the filename."""
  return filename.replace('chromium/src/', '')


def ConfigureBuild(gyp_args=None, gn_args=None, gn_dir=_DEFAULT_GN_DIR):
  """Generates gn and gyp targets with the given arguments."""
  gyp_args = gyp_args or []
  gn_args = gn_args or []

  print >> sys.stderr, 'Regenerating GN in %s...' % gn_dir
  # Currently only Release, non-component.
  Run('gn gen %s --args="is_debug=false is_component_build=false %s"' % \
      (gn_dir, ' '.join(gn_args)))

  os.environ.pop('GYP_DEFINES', None)
  # Remove environment variables required by gn but conflicting with GYP.
  # Relevant if Windows toolchain isn't provided by depot_tools.
  os.environ.pop('GYP_MSVS_OVERRIDE_PATH', None)
  os.environ.pop('WINDOWSSDKDIR', None)

  gyp_defines = ''
  if len(gyp_args) > 0:
    gyp_defines = '-D' + ' -D'.join(gyp_args)

  print >> sys.stderr, 'Regenerating GYP in %s...' % _DEFAULT_GYP_DIR
  Run('python webrtc/build/gyp_webrtc.py -Gconfig=Release %s' % gyp_defines)


def Counts(dict_of_list):
  """Given a dictionary whose value are lists, returns a dictionary whose values
  are the length of the list. This can be used to summarize a dictionary.
  """
  return {k: len(v) for k, v in dict_of_list.iteritems()}


def CountsByDirname(dict_of_list):
  """Given a list of files, returns a dict of dirname to counts in that dir."""
  r = {}
  for path in dict_of_list:
    dirname = os.path.dirname(path)
    r.setdefault(dirname, 0)
    r[dirname] += 1
  return r


class Comparison(object):
  """A comparison of the currently-configured build for a target."""

  def __init__(self, gyp_target="", gn_target=None, gyp_dir=_DEFAULT_GYP_DIR,
               gn_dir=_DEFAULT_GN_DIR):
    """Creates a comparison of a GN and GYP target. If the target names differ
    between the two build systems, then two names may be passed.
    """
    if gn_target is None:
      gn_target = gyp_target

    self._gyp_target = gyp_target
    self._gn_target = gn_target

    self._gyp_dir = gyp_dir
    self._gn_dir = gn_dir

    self._skipped = []

    self._total_diffs = 0

    self._missing_gyp_flags = {}
    self._missing_gn_flags = {}

    self._missing_gyp_files = {}
    self._missing_gn_files = {}

    self._CompareFiles()

  @property
  def gyp_files(self):
    """Returns the set of files that are in the GYP target."""
    return set(self._gyp_flags.keys())

  @property
  def gn_files(self):
    """Returns the set of files that are in the GN target."""
    return set(self._gn_flags.keys())

  @property
  def skipped(self):
    """Returns the list of compiler commands that were not processed during the
    comparison.
    """
    return self._skipped

  @property
  def total_differences(self):
    """Returns the total number of differences detected."""
    return self._total_diffs

  @property
  def missing_in_gyp(self):
    """Differences that are only in GYP build but not in GN, indexed by the
    difference."""
    return self._missing_gyp_flags

  @property
  def missing_in_gn(self):
    """Differences that are only in the GN build but not in GYP, indexed by
    the difference."""
    return self._missing_gn_flags

  @property
  def missing_in_gyp_by_file(self):
    """Differences that are only in the GYP build but not in GN, indexed by
    file.
    """
    return self._missing_gyp_files

  @property
  def missing_in_gn_by_file(self):
    """Differences that are only in the GYP build but not in GN, indexed by
    file.
    """
    return self._missing_gn_files

  def _CompareFiles(self):
    """Performs the actual target comparison."""
    if sys.platform == 'win32':
      # On Windows flags are stored in .rsp files which are created by building.
      print >> sys.stderr, 'Building in %s...' % self._gn_dir
      Run('ninja -C %s -d keeprsp %s' % (self._gn_dir, self._gn_target))
      print >> sys.stderr, 'Building in %s...' % self._gyp_dir
      Run('ninja -C %s -d keeprsp %s' % (self._gyp_dir, self._gn_target))

    gn = Run('ninja -C %s -t commands %s' % (self._gn_dir, self._gn_target))
    gyp = Run('ninja -C %s -t commands %s' % (self._gyp_dir, self._gyp_target))

    self._gn_flags = self._GetFlags(gn.splitlines(),
                                    os.path.join(os.getcwd(), self._gn_dir))
    self._gyp_flags = self._GetFlags(gyp.splitlines(),
                                     os.path.join(os.getcwd(), self._gyp_dir))

    self._gn_flags = dict((FilterChromium(filename), value)
                          for filename, value in self._gn_flags.iteritems())
    self._gyp_flags = dict((FilterChromium(filename), value)
                           for filename, value in self._gyp_flags.iteritems())

    all_files = sorted(self.gn_files & self.gyp_files)
    for filename in all_files:
      gyp_flags = self._gyp_flags[filename]
      gn_flags = self._gn_flags[filename]
      self._CompareLists(filename, gyp_flags, gn_flags, 'dash_f')
      self._CompareLists(filename, gyp_flags, gn_flags, 'defines',
          # These defines are not used by WebRTC
          dont_care_gyp=[
            '-DENABLE_WEBVR',
            '-DUSE_EXTERNAL_POPUP_MENU',
            '-DUSE_LIBJPEG_TURBO=1',
            '-DUSE_MINIKIN_HYPHENATION=1',
            '-DV8_USE_EXTERNAL_STARTUP_DATA',
            '-DCR_CLANG_REVISION=280106-1',
            '-DUSE_LIBPCI=1'
          ],
          dont_care_gn=[
            '-DUSE_EXTERNAL_POPUP_MENU=1'
          ])
      self._CompareLists(filename, gyp_flags, gn_flags, 'include_dirs')
      self._CompareLists(filename, gyp_flags, gn_flags, 'warnings',
          # More conservative warnings in GN we consider to be OK.
          dont_care_gyp=[
            '/wd4091',  # 'keyword' : ignored on left of 'type' when no variable
                        # is declared.
            '/wd4456',  # Declaration hides previous local declaration.
            '/wd4457',  # Declaration hides function parameter.
            '/wd4458',  # Declaration hides class member.
            '/wd4459',  # Declaration hides global declaration.
            '/wd4702',  # Unreachable code.
            '/wd4800',  # Forcing value to bool 'true' or 'false'.
            '/wd4838',  # Conversion from 'type' to 'type' requires a narrowing
                        # conversion.
          ] if sys.platform == 'win32' else None,
          dont_care_gn=[
            '-Wendif-labels',
            '-Wextra',
            '-Wsign-compare',
          ] if not sys.platform == 'win32' else None)
      self._CompareLists(filename, gyp_flags, gn_flags, 'other',
                         dont_care_gyp=['-g'], dont_care_gn=['-g2'])

  def _CompareLists(self, filename, gyp, gn, name,
                    dont_care_gyp=None, dont_care_gn=None):
    """Return a report of any differences between gyp and gn lists, ignoring
    anything in |dont_care_{gyp|gn}| respectively."""
    if gyp[name] == gn[name]:
      return
    if not dont_care_gyp:
      dont_care_gyp = []
    if not dont_care_gn:
      dont_care_gn = []
    gyp_set = set(gyp[name])
    gn_set = set(gn[name])
    missing_in_gyp = gyp_set - gn_set
    missing_in_gn = gn_set - gyp_set
    missing_in_gyp -= set(dont_care_gyp)
    missing_in_gn -= set(dont_care_gn)

    for m in missing_in_gyp:
      self._missing_gyp_flags.setdefault(name, {}) \
                             .setdefault(m, []).append(filename)
      self._total_diffs += 1
    self._missing_gyp_files.setdefault(filename, {}) \
                           .setdefault(name, set()).update(missing_in_gyp)

    for m in missing_in_gn:
      self._missing_gn_flags.setdefault(name, {}) \
                            .setdefault(m, []).append(filename)
      self._total_diffs += 1
    self._missing_gn_files.setdefault(filename, {}) \
                          .setdefault(name, set()).update(missing_in_gn)

  def _GetFlags(self, lines, build_dir):
    """Turn a list of command lines into a semi-structured dict."""
    is_win = sys.platform == 'win32'
    flags_by_output = {}
    for line in lines:
      line = FilterChromium(line)
      line = line.replace(os.getcwd(), '../../')
      line = line.replace('//', '/')
      command_line = shlex.split(line.strip(), posix=not is_win)[1:]

      output_name = _FindAndRemoveArgWithValue(command_line, '-o')
      dep_name = _FindAndRemoveArgWithValue(command_line, '-MF')

      command_line = _MergeSpacedArgs(command_line, '-Xclang')

      cc_file = [x for x in command_line if x.endswith('.cc') or
                                            x.endswith('.c') or
                                            x.endswith('.cpp') or
                                            x.endswith('.mm') or
                                            x.endswith('.m')]
      if len(cc_file) != 1:
        self._skipped.append(command_line)
        continue
      assert len(cc_file) == 1

      if is_win:
        rsp_file = [x for x in command_line if x.endswith('.rsp')]
        assert len(rsp_file) <= 1
        if rsp_file:
          rsp_file = os.path.join(build_dir, rsp_file[0][1:])
          with open(rsp_file, "r") as open_rsp_file:
            command_line = shlex.split(open_rsp_file, posix=False)

      defines = [x for x in command_line if x.startswith('-D')]
      include_dirs = [x for x in command_line if x.startswith('-I')]
      dash_f = [x for x in command_line if x.startswith('-f')]
      warnings = \
          [x for x in command_line if x.startswith('/wd' if is_win else '-W')]
      others = [x for x in command_line if x not in defines and \
                                           x not in include_dirs and \
                                           x not in dash_f and \
                                           x not in warnings and \
                                           x not in cc_file]

      for index, value in enumerate(include_dirs):
        if value == '-Igen':
          continue
        path = value[2:]
        if not os.path.isabs(path):
          path = os.path.join(build_dir, path)
        include_dirs[index] = '-I' + os.path.normpath(path)

      # GYP supports paths above the source root like <(DEPTH)/../foo while such
      # paths are unsupported by gn. But gn allows to use system-absolute paths
      # instead (paths that start with single '/'). Normalize all paths.
      cc_file = [os.path.normpath(os.path.join(build_dir, cc_file[0]))]

      # Filter for libFindBadConstructs.so having a relative path in one and
      # absolute path in the other.
      others_filtered = []
      for x in others:
        if x.startswith('-Xclang ') and \
            (x.endswith('libFindBadConstructs.so') or \
             x.endswith('libFindBadConstructs.dylib')):
          others_filtered.append(
              '-Xclang ' +
              os.path.join(os.getcwd(), os.path.normpath(
                  os.path.join('out/gn_flags', x.split(' ', 1)[1]))))
        elif x.startswith('-B'):
          others_filtered.append(
              '-B' +
              os.path.join(os.getcwd(), os.path.normpath(
                  os.path.join('out/gn_flags', x[2:]))))
        else:
          others_filtered.append(x)
      others = others_filtered

      flags_by_output[cc_file[0]] = {
        'output': output_name,
        'depname': dep_name,
        'defines': sorted(defines),
        'include_dirs': sorted(include_dirs),  # TODO(scottmg): This is wrong.
        'dash_f': sorted(dash_f),
        'warnings': sorted(warnings),
        'other': sorted(others),
      }
    return flags_by_output


def _FindAndRemoveArgWithValue(command_line, argname):
  """Given a command line as a list, remove and return the value of an option
  that takes a value as a separate entry.

  Modifies |command_line| in place.
  """
  if argname not in command_line:
    return ''
  location = command_line.index(argname)
  value = command_line[location + 1]
  command_line[location:location + 2] = []
  return value


def _MergeSpacedArgs(command_line, argname):
  """Combine all arguments |argname| with their values, separated by a space."""
  i = 0
  result = []
  while i < len(command_line):
    arg = command_line[i]
    if arg == argname:
      result.append(arg + ' ' + command_line[i + 1])
      i += 1
    else:
      result.append(arg)
    i += 1
  return result


def Run(command_line):
  """Run |command_line| as a subprocess and return stdout. Raises on error."""
  print >> sys.stderr, command_line
  return subprocess.check_output(command_line, shell=True)


def main():
  if len(sys.argv) < 3:
    print 'usage: %s gyp_dir gn_dir' % __file__
    print '   or: %s gyp_dir gn_dir target' % __file__
    print '   or: %s gyp_dir gn_dir gyp_target gn_target' % __file__
    return 1

  gyp_dir = sys.argv[1]
  gn_dir = sys.argv[2]

  gyp_target = gn_target = ""

  if len(sys.argv) > 3:
    gyp_target = sys.argv[3]
  if len(sys.argv) > 4:
    gn_target = sys.argv[4]

  print 'GYP output directory is %s' % gyp_dir
  print 'GN output directory is %s' % gn_dir

  comparison = Comparison(gyp_target, gn_target, gyp_dir, gn_dir)

  differing_files = set(comparison.missing_in_gn_by_file.keys()) & \
                    set(comparison.missing_in_gyp_by_file.keys())
  files_with_given_differences = {}
  for filename in differing_files:
    output = ''
    missing_in_gyp = comparison.missing_in_gyp_by_file.get(filename, {})
    missing_in_gn = comparison.missing_in_gn_by_file.get(filename, {})
    difference_types = sorted(set(missing_in_gyp.keys() + missing_in_gn.keys()))
    for difference_type in difference_types:
      if (len(missing_in_gyp[difference_type]) == 0 and
          len(missing_in_gn[difference_type]) == 0):
        continue
      output += '  %s differ:\n' % difference_type
      if (difference_type in missing_in_gyp and
          len(missing_in_gyp[difference_type])):
        output += '    In gyp, but not in GN:\n      %s' % '\n      '.join(
            sorted(missing_in_gyp[difference_type])) + '\n'
      if (difference_type in missing_in_gn and
          len(missing_in_gn[difference_type])):
        output += '    In GN, but not in gyp:\n      %s' % '\n      '.join(
            sorted(missing_in_gn[difference_type])) + '\n'
    if output:
      files_with_given_differences.setdefault(output, []).append(filename)

  for diff, files in files_with_given_differences.iteritems():
    print '\n'.join(sorted(files))
    print diff

  print 'Total differences:', comparison.total_differences
  # TODO(scottmg): Return failure on difference once we're closer to identical.
  return 0


if __name__ == '__main__':
  sys.exit(main())
