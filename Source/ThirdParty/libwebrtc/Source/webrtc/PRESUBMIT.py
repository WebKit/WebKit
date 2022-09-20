#!/usr/bin/env vpython3

# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import json
import os
import re
import sys
from collections import defaultdict
from contextlib import contextmanager

# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True

# Files and directories that are *skipped* by cpplint in the presubmit script.
CPPLINT_EXCEPTIONS = [
    'api/video_codecs/video_decoder.h',
    'common_types.cc',
    'common_types.h',
    'examples/objc',
    'media/base/stream_params.h',
    'media/base/video_common.h',
    'modules/audio_coding',
    'modules/audio_device',
    'modules/audio_processing',
    'modules/desktop_capture',
    'modules/include/module_common_types.h',
    'modules/utility',
    'modules/video_capture',
    'p2p/base/pseudo_tcp.cc',
    'p2p/base/pseudo_tcp.h',
    'PRESUBMIT.py',
    'presubmit_test_mocks.py',
    'presubmit_test.py',
    'rtc_base',
    'sdk/android/src/jni',
    'sdk/objc',
    'system_wrappers',
    'test',
    'tools_webrtc',
    'voice_engine',
]

# These filters will always be removed, even if the caller specifies a filter
# set, as they are problematic or broken in some way.
#
# Justifications for each filter:
# - build/c++11         : Rvalue ref checks are unreliable (false positives),
#                         include file and feature blocklists are
#                         google3-specific.
# - runtime/references  : Mutable references are not banned by the Google
#                         C++ style guide anymore (starting from May 2020).
# - whitespace/operators: Same as above (doesn't seem sufficient to eliminate
#                         all move-related errors).
DISABLED_LINT_FILTERS = [
    '-build/c++11',
    '-runtime/references',
    '-whitespace/operators',
]

# List of directories of "supported" native APIs. That means changes to headers
# will be done in a compatible way following this scheme:
# 1. Non-breaking changes are made.
# 2. The old APIs as marked as deprecated (with comments).
# 3. Deprecation is announced to discuss-webrtc@googlegroups.com and
#    webrtc-users@google.com (internal list).
# 4. (later) The deprecated APIs are removed.
NATIVE_API_DIRS = (
    'api',  # All subdirectories of api/ are included as well.
    'media/base',
    'media/engine',
    'modules/audio_device/include',
    'pc',
)

# These directories should not be used but are maintained only to avoid breaking
# some legacy downstream code.
LEGACY_API_DIRS = (
    'common_audio/include',
    'modules/audio_coding/include',
    'modules/audio_processing/include',
    'modules/congestion_controller/include',
    'modules/include',
    'modules/remote_bitrate_estimator/include',
    'modules/rtp_rtcp/include',
    'modules/rtp_rtcp/source',
    'modules/utility/include',
    'modules/video_coding/codecs/h264/include',
    'modules/video_coding/codecs/vp8/include',
    'modules/video_coding/codecs/vp9/include',
    'modules/video_coding/include',
    'rtc_base',
    'system_wrappers/include',
)

# NOTE: The set of directories in API_DIRS should be the same as those
# listed in the table in native-api.md.
API_DIRS = NATIVE_API_DIRS[:] + LEGACY_API_DIRS[:]

# TARGET_RE matches a GN target, and extracts the target name and the contents.
TARGET_RE = re.compile(
    r'(?P<indent>\s*)(?P<target_type>\w+)\("(?P<target_name>\w+)"\) {'
    r'(?P<target_contents>.*?)'
    r'(?P=indent)}', re.MULTILINE | re.DOTALL)

# SOURCES_RE matches a block of sources inside a GN target.
SOURCES_RE = re.compile(r'sources \+?= \[(?P<sources>.*?)\]',
                        re.MULTILINE | re.DOTALL)

# DEPS_RE matches a block of sources inside a GN target.
DEPS_RE = re.compile(r'\bdeps \+?= \[(?P<deps>.*?)\]',
                     re.MULTILINE | re.DOTALL)

# FILE_PATH_RE matches a file path.
FILE_PATH_RE = re.compile(r'"(?P<file_path>(\w|\/)+)(?P<extension>\.\w+)"')


def FindSrcDirPath(starting_dir):
  """Returns the abs path to the src/ dir of the project."""
  src_dir = starting_dir
  while os.path.basename(src_dir) != 'src':
    src_dir = os.path.normpath(os.path.join(src_dir, os.pardir))
  return src_dir


@contextmanager
def _AddToPath(*paths):
  original_sys_path = sys.path
  sys.path.extend(paths)
  try:
    yield
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path


def VerifyNativeApiHeadersListIsValid(input_api, output_api):
  """Ensures the list of native API header directories is up to date."""
  non_existing_paths = []
  native_api_full_paths = [
      input_api.os_path.join(input_api.PresubmitLocalPath(), *path.split('/'))
      for path in API_DIRS
  ]
  for path in native_api_full_paths:
    if not os.path.isdir(path):
      non_existing_paths.append(path)
  if non_existing_paths:
    return [
        output_api.PresubmitError(
            'Directories to native API headers have changed which has made '
            'the list in PRESUBMIT.py outdated.\nPlease update it to the '
            'current location of our native APIs.', non_existing_paths)
    ]
  return []


API_CHANGE_MSG = """
You seem to be changing native API header files. Please make sure that you:
  1. Make compatible changes that don't break existing clients. Usually
     this is done by keeping the existing method signatures unchanged.
  2. Mark the old stuff as deprecated (use the ABSL_DEPRECATED macro).
  3. Create a timeline and plan for when the deprecated stuff will be
     removed. (The amount of time we give users to change their code
     should be informed by how much work it is for them. If they just
     need to replace one name with another or something equally
     simple, 1-2 weeks might be good; if they need to do serious work,
     up to 3 months may be called for.)
  4. Update/inform existing downstream code owners to stop using the
     deprecated stuff. (Send announcements to
     discuss-webrtc@googlegroups.com and webrtc-users@google.com.)
  5. Remove the deprecated stuff, once the agreed-upon amount of time
     has passed.
Related files:
"""


def CheckNativeApiHeaderChanges(input_api, output_api):
  """Checks to remind proper changing of native APIs."""
  files = []
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, files_to_check=[r'.+\.(gn|gni|h)$'])
  for f in input_api.AffectedSourceFiles(source_file_filter):
    for path in API_DIRS:
      dn = os.path.dirname(f.LocalPath())
      if path == 'api':
        # Special case: Subdirectories included.
        if dn == 'api' or dn.startswith('api/'):
          files.append(f.LocalPath())
      else:
        # Normal case: Subdirectories not included.
        if dn == path:
          files.append(f.LocalPath())

  if files:
    return [output_api.PresubmitNotifyResult(API_CHANGE_MSG, files)]
  return []


def CheckNoIOStreamInHeaders(input_api, output_api, source_file_filter):
  """Checks to make sure no .h files include <iostream>."""
  files = []
  pattern = input_api.re.compile(r'^#include\s*<iostream>',
                                 input_api.re.MULTILINE)
  file_filter = lambda x: (input_api.FilterSourceFile(x) and source_file_filter(
      x))
  for f in input_api.AffectedSourceFiles(file_filter):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if len(files) > 0:
    return [
        output_api.PresubmitError(
            'Do not #include <iostream> in header files, since it inserts '
            'static initialization into every file including the header. '
            'Instead, #include <ostream>. See http://crbug.com/94794', files)
    ]
  return []


def CheckNoPragmaOnce(input_api, output_api, source_file_filter):
  """Make sure that banned functions are not used."""
  files = []
  pattern = input_api.re.compile(r'^#pragma\s+once', input_api.re.MULTILINE)
  file_filter = lambda x: (input_api.FilterSourceFile(x) and source_file_filter(
      x))
  for f in input_api.AffectedSourceFiles(file_filter):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if files:
    return [
        output_api.PresubmitError(
            'Do not use #pragma once in header files.\n'
            'See http://www.chromium.org/developers/coding-style'
            '#TOC-File-headers', files)
    ]
  return []


def CheckNoFRIEND_TEST(# pylint: disable=invalid-name
        input_api,
        output_api,
        source_file_filter):
  """Make sure that gtest's FRIEND_TEST() macro is not used, the
  FRIEND_TEST_ALL_PREFIXES() macro from testsupport/gtest_prod_util.h should be
  used instead since that allows for FLAKY_, FAILS_ and DISABLED_ prefixes."""
  problems = []

  file_filter = lambda f: (f.LocalPath().endswith(('.cc', '.h')) and
                           source_file_filter(f))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      if 'FRIEND_TEST(' in line:
        problems.append('    %s:%d' % (f.LocalPath(), line_num))

  if not problems:
    return []
  return [
      output_api.PresubmitPromptWarning(
          'WebRTC\'s code should not use gtest\'s FRIEND_TEST() macro. '
          'Include testsupport/gtest_prod_util.h and use '
          'FRIEND_TEST_ALL_PREFIXES() instead.\n' + '\n'.join(problems))
  ]


def IsLintDisabled(disabled_paths, file_path):
  """ Checks if a file is disabled for lint check."""
  for path in disabled_paths:
    if file_path == path or os.path.dirname(file_path).startswith(path):
      return True
  return False


def CheckApprovedFilesLintClean(input_api, output_api,
                                source_file_filter=None):
  """Checks that all new or non-exempt .cc and .h files pass cpplint.py.
  This check is based on CheckChangeLintsClean in
  depot_tools/presubmit_canned_checks.py but has less filters and only checks
  added files."""
  result = []

  # Initialize cpplint.
  import cpplint
  # Access to a protected member _XX of a client class
  # pylint: disable=W0212
  cpplint._cpplint_state.ResetErrorCounts()

  lint_filters = cpplint._Filters()
  lint_filters.extend(DISABLED_LINT_FILTERS)
  cpplint._SetFilters(','.join(lint_filters))

  # Create a platform independent exempt list for cpplint.
  disabled_paths = [
      input_api.os_path.join(*path.split('/')) for path in CPPLINT_EXCEPTIONS
  ]

  # Use the strictest verbosity level for cpplint.py (level 1) which is the
  # default when running cpplint.py from command line. To make it possible to
  # work with not-yet-converted code, we're only applying it to new (or
  # moved/renamed) files and files not listed in CPPLINT_EXCEPTIONS.
  verbosity_level = 1
  files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    # Note that moved/renamed files also count as added.
    if f.Action() == 'A' or not IsLintDisabled(disabled_paths, f.LocalPath()):
      files.append(f.AbsoluteLocalPath())

  for file_name in files:
    cpplint.ProcessFile(file_name, verbosity_level)

  if cpplint._cpplint_state.error_count > 0:
    if input_api.is_committing:
      res_type = output_api.PresubmitError
    else:
      res_type = output_api.PresubmitPromptWarning
    result = [res_type('Changelist failed cpplint.py check.')]

  return result


def CheckNoSourcesAbove(input_api, gn_files, output_api):
  # Disallow referencing source files with paths above the GN file location.
  source_pattern = input_api.re.compile(r' +sources \+?= \[(.*?)\]',
                                        re.MULTILINE | re.DOTALL)
  file_pattern = input_api.re.compile(r'"((\.\./.*?)|(//.*?))"')
  violating_gn_files = set()
  violating_source_entries = []
  for gn_file in gn_files:
    contents = input_api.ReadFile(gn_file)
    for source_block_match in source_pattern.finditer(contents):
      # Find all source list entries starting with ../ in the source block
      # (exclude overrides entries).
      for file_list_match in file_pattern.finditer(source_block_match.group(1)):
        source_file = file_list_match.group(1)
        if 'overrides/' not in source_file:
          violating_source_entries.append(source_file)
          violating_gn_files.add(gn_file)
  if violating_gn_files:
    return [
        output_api.PresubmitError(
            'Referencing source files above the directory of the GN file '
            'is not allowed. Please introduce new GN targets in the proper '
            'location instead.\n'
            'Invalid source entries:\n'
            '%s\n'
            'Violating GN files:' % '\n'.join(violating_source_entries),
            items=violating_gn_files)
    ]
  return []


def CheckAbseilDependencies(input_api, gn_files, output_api):
  """Checks that Abseil dependencies are declared in `absl_deps`."""
  absl_re = re.compile(r'third_party/abseil-cpp', re.MULTILINE | re.DOTALL)
  target_types_to_check = [
      'rtc_library',
      'rtc_source_set',
      'rtc_static_library',
      'webrtc_fuzzer_test',
  ]
  error_msg = ('Abseil dependencies in target "%s" (file: %s) '
               'should be moved to the "absl_deps" parameter.')
  errors = []

  # pylint: disable=too-many-nested-blocks
  for gn_file in gn_files:
    gn_file_content = input_api.ReadFile(gn_file)
    for target_match in TARGET_RE.finditer(gn_file_content):
      target_type = target_match.group('target_type')
      target_name = target_match.group('target_name')
      target_contents = target_match.group('target_contents')
      if target_type in target_types_to_check:
        for deps_match in DEPS_RE.finditer(target_contents):
          deps = deps_match.group('deps').splitlines()
          for dep in deps:
            if re.search(absl_re, dep):
              errors.append(
                  output_api.PresubmitError(error_msg %
                                            (target_name, gn_file.LocalPath())))
              break  # no need to warn more than once per target
  return errors


def CheckNoMixingSources(input_api, gn_files, output_api):
  """Disallow mixing C, C++ and Obj-C/Obj-C++ in the same target.

  See bugs.webrtc.org/7743 for more context.
  """

  def _MoreThanOneSourceUsed(*sources_lists):
    sources_used = 0
    for source_list in sources_lists:
      if len(source_list) > 0:
        sources_used += 1
    return sources_used > 1

  errors = defaultdict(lambda: [])
  for gn_file in gn_files:
    gn_file_content = input_api.ReadFile(gn_file)
    for target_match in TARGET_RE.finditer(gn_file_content):
      # list_of_sources is a list of tuples of the form
      # (c_files, cc_files, objc_files) that keeps track of all the
      # sources defined in a target. A GN target can have more that
      # on definition of sources (since it supports if/else statements).
      # E.g.:
      # rtc_static_library("foo") {
      #   if (is_win) {
      #     sources = [ "foo.cc" ]
      #   } else {
      #     sources = [ "foo.mm" ]
      #   }
      # }
      # This is allowed and the presubmit check should support this case.
      list_of_sources = []
      c_files = []
      cc_files = []
      objc_files = []
      target_name = target_match.group('target_name')
      target_contents = target_match.group('target_contents')
      for sources_match in SOURCES_RE.finditer(target_contents):
        if '+=' not in sources_match.group(0):
          if c_files or cc_files or objc_files:
            list_of_sources.append((c_files, cc_files, objc_files))
          c_files = []
          cc_files = []
          objc_files = []
        for file_match in FILE_PATH_RE.finditer(sources_match.group(1)):
          file_path = file_match.group('file_path')
          extension = file_match.group('extension')
          if extension == '.c':
            c_files.append(file_path + extension)
          if extension == '.cc':
            cc_files.append(file_path + extension)
          if extension in ['.m', '.mm']:
            objc_files.append(file_path + extension)
      list_of_sources.append((c_files, cc_files, objc_files))
      for c_files_list, cc_files_list, objc_files_list in list_of_sources:
        if _MoreThanOneSourceUsed(c_files_list, cc_files_list, objc_files_list):
          all_sources = sorted(c_files_list + cc_files_list + objc_files_list)
          errors[gn_file.LocalPath()].append((target_name, all_sources))
  if errors:
    return [
        output_api.PresubmitError(
            'GN targets cannot mix .c, .cc and .m (or .mm) source files.\n'
            'Please create a separate target for each collection of '
            'sources.\n'
            'Mixed sources: \n'
            '%s\n'
            'Violating GN files:\n%s\n' %
            (json.dumps(errors, indent=2), '\n'.join(list(errors.keys()))))
    ]
  return []


def CheckNoPackageBoundaryViolations(input_api, gn_files, output_api):
  cwd = input_api.PresubmitLocalPath()
  with _AddToPath(
      input_api.os_path.join(cwd, 'tools_webrtc', 'presubmit_checks_lib')):
    from check_package_boundaries import CheckPackageBoundaries
  build_files = [os.path.join(cwd, gn_file.LocalPath()) for gn_file in gn_files]
  errors = CheckPackageBoundaries(cwd, build_files)[:5]
  if errors:
    return [
        output_api.PresubmitError(
            'There are package boundary violations in the following GN '
            'files:',
            long_text='\n\n'.join(str(err) for err in errors))
    ]
  return []


def _ReportFileAndLine(filename, line_num):
  """Default error formatter for _FindNewViolationsOfRule."""
  return '%s (line %s)' % (filename, line_num)


def CheckNoWarningSuppressionFlagsAreAdded(gn_files,
                                           input_api,
                                           output_api,
                                           error_formatter=_ReportFileAndLine):
  """Ensure warning suppression flags are not added without a reason."""
  msg = ('Usage of //build/config/clang:extra_warnings is discouraged '
         'in WebRTC.\n'
         'If you are not adding this code (e.g. you are just moving '
         'existing code) or you want to add an exception,\n'
         'you can add a comment on the line that causes the problem:\n\n'
         '"-Wno-odr"  # no-presubmit-check TODO(bugs.webrtc.org/BUG_ID)\n'
         '\n'
         'Affected files:\n')
  errors = []  # 2-element tuples with (file, line number)
  clang_warn_re = input_api.re.compile(r'//build/config/clang:extra_warnings')
  # pylint: disable-next=fixme
  no_presubmit_re = input_api.re.compile(
      r'# no-presubmit-check TODO\(bugs\.webrtc\.org/\d+\)')
  for f in gn_files:
    for line_num, line in f.ChangedContents():
      if clang_warn_re.search(line) and not no_presubmit_re.search(line):
        errors.append(error_formatter(f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitError(msg, errors)]
  return []


def CheckNoTestCaseUsageIsAdded(input_api,
                                output_api,
                                source_file_filter,
                                error_formatter=_ReportFileAndLine):
  error_msg = ('Usage of legacy GoogleTest API detected!\nPlease use the '
               'new API: https://github.com/google/googletest/blob/master/'
               'googletest/docs/primer.md#beware-of-the-nomenclature.\n'
               'Affected files:\n')
  errors = []  # 2-element tuples with (file, line number)
  test_case_re = input_api.re.compile(r'TEST_CASE')
  file_filter = lambda f: (source_file_filter(f) and f.LocalPath().endswith(
      '.cc'))
  for f in input_api.AffectedSourceFiles(file_filter):
    for line_num, line in f.ChangedContents():
      if test_case_re.search(line):
        errors.append(error_formatter(f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitError(error_msg, errors)]
  return []


def CheckNoStreamUsageIsAdded(input_api,
                              output_api,
                              source_file_filter,
                              error_formatter=_ReportFileAndLine):
  """Make sure that no more dependencies on stringstream are added."""
  error_msg = ('Usage of <sstream>, <istream> and <ostream> in WebRTC is '
               'deprecated.\n'
               'This includes the following types:\n'
               'std::istringstream, std::ostringstream, std::wistringstream, '
               'std::wostringstream,\n'
               'std::wstringstream, std::ostream, std::wostream, std::istream,'
               'std::wistream,\n'
               'std::iostream, std::wiostream.\n'
               'If you are not adding this code (e.g. you are just moving '
               'existing code),\n'
               'you can add a comment on the line that causes the problem:\n\n'
               '#include <sstream>  // no-presubmit-check TODO(webrtc:8982)\n'
               'std::ostream& F() {  // no-presubmit-check TODO(webrtc:8982)\n'
               '\n'
               'If you are adding new code, consider using '
               'rtc::SimpleStringBuilder\n'
               '(in rtc_base/strings/string_builder.h).\n'
               'Affected files:\n')
  errors = []  # 2-element tuples with (file, line number)
  include_re = input_api.re.compile(r'#include <(i|o|s)stream>')
  usage_re = input_api.re.compile(r'std::(w|i|o|io|wi|wo|wio)(string)*stream')
  no_presubmit_re = input_api.re.compile(
      r'// no-presubmit-check TODO\(webrtc:8982\)')
  file_filter = lambda x: (input_api.FilterSourceFile(x) and source_file_filter(
      x))

  def _IsException(file_path):
    is_test = any(
        file_path.endswith(x)
        for x in ['_test.cc', '_tests.cc', '_unittest.cc', '_unittests.cc'])
    return (file_path.startswith('examples') or file_path.startswith('test')
            or is_test)

  for f in input_api.AffectedSourceFiles(file_filter):
    # Usage of stringstream is allowed under examples/ and in tests.
    if f.LocalPath() == 'PRESUBMIT.py' or _IsException(f.LocalPath()):
      continue
    for line_num, line in f.ChangedContents():
      if ((include_re.search(line) or usage_re.search(line))
          and not no_presubmit_re.search(line)):
        errors.append(error_formatter(f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitError(error_msg, errors)]
  return []


def CheckPublicDepsIsNotUsed(gn_files, input_api, output_api):
  """Checks that public_deps is not used without a good reason."""
  result = []
  no_presubmit_check_re = input_api.re.compile(
      r'# no-presubmit-check TODO\(webrtc:\d+\)')
  error_msg = ('public_deps is not recommended in WebRTC BUILD.gn files '
               'because it doesn\'t map well to downstream build systems.\n'
               'Used in: %s (line %d).\n'
               'If you are not adding this code (e.g. you are just moving '
               'existing code) or you have a good reason, you can add this '
               'comment (verbatim) on the line that causes the problem:\n\n'
               'public_deps = [  # no-presubmit-check TODO(webrtc:8603)\n')
  for affected_file in gn_files:
    for (line_number, affected_line) in affected_file.ChangedContents():
      if 'public_deps' in affected_line:
        surpressed = no_presubmit_check_re.search(affected_line)
        if not surpressed:
          result.append(
              output_api.PresubmitError(
                  error_msg % (affected_file.LocalPath(), line_number)))
  return result


def CheckCheckIncludesIsNotUsed(gn_files, input_api, output_api):
  result = []
  error_msg = ('check_includes overrides are not allowed since it can cause '
               'incorrect dependencies to form. It effectively means that your '
               'module can include any .h file without depending on its '
               'corresponding target. There are some exceptional cases when '
               'this is allowed: if so, get approval from a .gn owner in the '
               'root OWNERS file.\n'
               'Used in: %s (line %d).')
  # pylint: disable-next=fixme
  no_presubmit_re = input_api.re.compile(
      r'# no-presubmit-check TODO\(bugs\.webrtc\.org/\d+\)')
  for affected_file in gn_files:
    for (line_number, affected_line) in affected_file.ChangedContents():
      if ('check_includes' in affected_line
          and not no_presubmit_re.search(affected_line)):
        result.append(
            output_api.PresubmitError(error_msg %
                                      (affected_file.LocalPath(), line_number)))
  return result


def CheckGnChanges(input_api, output_api):
  file_filter = lambda x: (input_api.FilterSourceFile(
      x,
      files_to_check=(r'.+\.(gn|gni)$', ),
      files_to_skip=(r'.*/presubmit_checks_lib/testdata/.*', )))

  gn_files = []
  for f in input_api.AffectedSourceFiles(file_filter):
    gn_files.append(f)

  result = []
  if gn_files:
    result.extend(CheckNoSourcesAbove(input_api, gn_files, output_api))
    result.extend(CheckNoMixingSources(input_api, gn_files, output_api))
    result.extend(CheckAbseilDependencies(input_api, gn_files, output_api))
    result.extend(
        CheckNoPackageBoundaryViolations(input_api, gn_files, output_api))
    result.extend(CheckPublicDepsIsNotUsed(gn_files, input_api, output_api))
    result.extend(CheckCheckIncludesIsNotUsed(gn_files, input_api, output_api))
    result.extend(
        CheckNoWarningSuppressionFlagsAreAdded(gn_files, input_api, output_api))
  return result


def CheckGnGen(input_api, output_api):
  """Runs `gn gen --check` with default args to detect mismatches between
  #includes and dependencies in the BUILD.gn files, as well as general build
  errors.
  """
  with _AddToPath(
      input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools_webrtc',
                             'presubmit_checks_lib')):
    from build_helpers import RunGnCheck
  errors = RunGnCheck(FindSrcDirPath(input_api.PresubmitLocalPath()))[:5]
  if errors:
    return [
        output_api.PresubmitPromptWarning(
            'Some #includes do not match the build dependency graph. '
            'Please run:\n'
            '  gn gen --check <out_dir>',
            long_text='\n\n'.join(errors))
    ]
  return []


def CheckUnwantedDependencies(input_api, output_api, source_file_filter):
  """Runs checkdeps on #include statements added in this
  change. Breaking - rules is an error, breaking ! rules is a
  warning.
  """
  # Copied from Chromium's src/PRESUBMIT.py.

  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  src_path = FindSrcDirPath(input_api.PresubmitLocalPath())
  checkdeps_path = input_api.os_path.join(src_path, 'buildtools', 'checkdeps')
  if not os.path.exists(checkdeps_path):
    return [
        output_api.PresubmitError(
            'Cannot find checkdeps at %s\nHave you run "gclient sync" to '
            'download all the DEPS entries?' % checkdeps_path)
    ]
  with _AddToPath(checkdeps_path):
    import checkdeps
    from cpp_checker import CppChecker
    from rules import Rule

  added_includes = []
  for f in input_api.AffectedFiles(file_filter=source_file_filter):
    if not CppChecker.IsCppFile(f.LocalPath()):
      continue

    changed_lines = [line for _, line in f.ChangedContents()]
    added_includes.append([f.LocalPath(), changed_lines])

  deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())

  error_descriptions = []
  warning_descriptions = []
  for path, rule_type, rule_description in deps_checker.CheckAddedCppIncludes(
      added_includes):
    description_with_path = '%s\n    %s' % (path, rule_description)
    if rule_type == Rule.DISALLOW:
      error_descriptions.append(description_with_path)
    else:
      warning_descriptions.append(description_with_path)

  results = []
  if error_descriptions:
    results.append(
        output_api.PresubmitError(
            'You added one or more #includes that violate checkdeps rules.'
            '\nCheck that the DEPS files in these locations contain valid '
            'rules.\nSee '
            'https://cs.chromium.org/chromium/src/buildtools/checkdeps/ '
            'for more details about checkdeps.', error_descriptions))
  if warning_descriptions:
    results.append(
        output_api.PresubmitPromptOrNotify(
            'You added one or more #includes of files that are temporarily'
            '\nallowed but being removed. Can you avoid introducing the\n'
            '#include? See relevant DEPS file(s) for details and contacts.'
            '\nSee '
            'https://cs.chromium.org/chromium/src/buildtools/checkdeps/ '
            'for more details about checkdeps.', warning_descriptions))
  return results


def CheckCommitMessageBugEntry(input_api, output_api):
  """Check that bug entries are well-formed in commit message."""
  bogus_bug_msg = (
      'Bogus Bug entry: %s. Please specify the issue tracker prefix and the '
      'issue number, separated by a colon, e.g. webrtc:123 or chromium:12345.')
  results = []
  for bug in input_api.change.BugsFromDescription():
    bug = bug.strip()
    if bug.lower() == 'none':
      continue
    if 'b/' not in bug and ':' not in bug:
      try:
        if int(bug) > 100000:
          # Rough indicator for current chromium bugs.
          prefix_guess = 'chromium'
        else:
          prefix_guess = 'webrtc'
        results.append('Bug entry requires issue tracker prefix, e.g. %s:%s' %
                       (prefix_guess, bug))
      except ValueError:
        results.append(bogus_bug_msg % bug)
    elif not (re.match(r'\w+:\d+', bug) or re.match(r'b/\d+', bug)):
      results.append(bogus_bug_msg % bug)
  return [output_api.PresubmitError(r) for r in results]


def CheckChangeHasBugField(input_api, output_api):
  """Requires that the changelist is associated with a bug.

  This check is stricter than the one in depot_tools/presubmit_canned_checks.py
  since it fails the presubmit if the bug field is missing or doesn't contain
  a bug reference.

  This supports both 'BUG=' and 'Bug:' since we are in the process of migrating
  to Gerrit and it encourages the usage of 'Bug:'.
  """
  if input_api.change.BugsFromDescription():
    return []
  return [
      output_api.PresubmitError(
          'The "Bug: [bug number]" footer is mandatory. Please create a '
          'bug and reference it using either of:\n'
          ' * https://bugs.webrtc.org - reference it using Bug: '
          'webrtc:XXXX\n'
          ' * https://crbug.com - reference it using Bug: chromium:XXXXXX')
  ]


def CheckJSONParseErrors(input_api, output_api, source_file_filter):
  """Check that JSON files do not contain syntax errors."""

  def FilterFile(affected_file):
    return (input_api.os_path.splitext(affected_file.LocalPath())[1] == '.json'
            and source_file_filter(affected_file))

  def GetJSONParseError(input_api, filename):
    try:
      contents = input_api.ReadFile(filename)
      input_api.json.loads(contents)
    except ValueError as e:
      return e
    return None

  results = []
  for affected_file in input_api.AffectedFiles(file_filter=FilterFile,
                                               include_deletes=False):
    parse_error = GetJSONParseError(input_api,
                                    affected_file.AbsoluteLocalPath())
    if parse_error:
      results.append(
          output_api.PresubmitError('%s could not be parsed: %s' %
                                    (affected_file.LocalPath(), parse_error)))
  return results


def RunPythonTests(input_api, output_api):
  def Join(*args):
    return input_api.os_path.join(input_api.PresubmitLocalPath(), *args)

  excluded_files = [
      # These tests should be run manually after webrtc_dashboard_upload target
      # has been built.
      'catapult_uploader_test.py',
      'process_perf_results_test.py',
  ]

  test_directories = [
      input_api.PresubmitLocalPath(),
      Join('rtc_tools', 'py_event_log_analyzer'),
      Join('audio', 'test', 'unittests'),
  ] + [
      root for root, _, files in os.walk(Join('tools_webrtc')) if any(
          f.endswith('_test.py') and f not in excluded_files for f in files)
  ]

  tests = []

  for directory in test_directories:
    tests.extend(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            directory,
            files_to_check=[r'.+_test\.py$'],
            run_on_python2=False))
  return input_api.RunTests(tests, parallel=True)


def CheckUsageOfGoogleProtobufNamespace(input_api, output_api,
                                        source_file_filter):
  """Checks that the namespace google::protobuf has not been used."""
  files = []
  pattern = input_api.re.compile(r'google::protobuf')
  proto_utils_path = os.path.join('rtc_base', 'protobuf_utils.h')
  file_filter = lambda x: (input_api.FilterSourceFile(x) and source_file_filter(
      x))
  for f in input_api.AffectedSourceFiles(file_filter):
    if f.LocalPath() in [proto_utils_path, 'PRESUBMIT.py']:
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if files:
    return [
        output_api.PresubmitError(
            'Please avoid to use namespace `google::protobuf` directly.\n'
            'Add a using directive in `%s` and include that header instead.' %
            proto_utils_path, files)
    ]
  return []


def _LicenseHeader(input_api):
  """Returns the license header regexp."""
  # Accept any year number from 2003 to the current year
  current_year = int(input_api.time.strftime('%Y'))
  allowed_years = (str(s) for s in reversed(range(2003, current_year + 1)))
  years_re = '(' + '|'.join(allowed_years) + ')'
  license_header = (
      r'.*? Copyright( \(c\))? %(year)s The WebRTC [Pp]roject [Aa]uthors\. '
      r'All [Rr]ights [Rr]eserved\.\n'
      r'.*?\n'
      r'.*? Use of this source code is governed by a BSD-style license\n'
      r'.*? that can be found in the LICENSE file in the root of the source\n'
      r'.*? tree\. An additional intellectual property rights grant can be '
      r'found\n'
      r'.*? in the file PATENTS\.  All contributing project authors may\n'
      r'.*? be found in the AUTHORS file in the root of the source tree\.\n'
  ) % {
      'year': years_re,
  }
  return license_header


def CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  # Filter out files that are in objc or ios dirs from being cpplint-ed since
  # they do not follow C++ lint rules.
  exception_list = input_api.DEFAULT_FILES_TO_SKIP + (
      r".*\bobjc[\\\/].*",
      r".*objc\.[hcm]+$",
  )
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, None, exception_list)
  results.extend(
      CheckApprovedFilesLintClean(input_api, output_api, source_file_filter))
  results.extend(
      input_api.canned_checks.CheckLicense(input_api, output_api,
                                           _LicenseHeader(input_api)))

  # TODO(bugs.webrtc.org/12114): Delete this filter and run pylint on
  # all python files. This is a temporary solution.
  python_file_filter = lambda f: (f.LocalPath().endswith('.py') and
                                  source_file_filter(f))
  python_changed_files = [
      f.LocalPath()
      for f in input_api.AffectedFiles(include_deletes=False,
                                       file_filter=python_file_filter)
  ]

  results.extend(
      input_api.canned_checks.RunPylint(
          input_api,
          output_api,
          files_to_check=python_changed_files,
          files_to_skip=(
              r'^base[\\\/].*\.py$',
              r'^build[\\\/].*\.py$',
              r'^buildtools[\\\/].*\.py$',
              r'^infra[\\\/].*\.py$',
              r'^ios[\\\/].*\.py$',
              r'^out.*[\\\/].*\.py$',
              r'^testing[\\\/].*\.py$',
              r'^third_party[\\\/].*\.py$',
              r'^tools[\\\/].*\.py$',
              r'^xcodebuild.*[\\\/].*\.py$',
          ),
          pylintrc='pylintrc',
          version='2.7'))

  # TODO(bugs.webrtc.org/13606): talk/ is no more, so make below checks simpler?
  # WebRTC can't use the presubmit_canned_checks.PanProjectChecks function
  # since we need to have different license checks
  # in talk/ and webrtc/directories.
  # Instead, hand-picked checks are included below.

  # .m and .mm files are ObjC files. For simplicity we will consider
  # .h files in ObjC subdirectories ObjC headers.
  objc_filter_list = (r'.+\.m$', r'.+\.mm$', r'.+objc\/.+\.h$')
  # Skip long-lines check for DEPS and GN files.
  build_file_filter_list = (r'.+\.gn$', r'.+\.gni$', 'DEPS')
  # Also we will skip most checks for third_party directory.
  third_party_filter_list = (r'(^|.*[\\\/])third_party[\\\/].+', )
  eighty_char_sources = lambda x: input_api.FilterSourceFile(
      x,
      files_to_skip=build_file_filter_list + objc_filter_list +
      third_party_filter_list)
  hundred_char_sources = lambda x: input_api.FilterSourceFile(
      x, files_to_check=objc_filter_list)
  non_third_party_sources = lambda x: input_api.FilterSourceFile(
      x, files_to_skip=third_party_filter_list)

  results.extend(
      input_api.canned_checks.CheckLongLines(
          input_api,
          output_api,
          maxlen=80,
          source_file_filter=eighty_char_sources))
  results.extend(
      input_api.canned_checks.CheckLongLines(
          input_api,
          output_api,
          maxlen=100,
          source_file_filter=hundred_char_sources))
  results.extend(
      input_api.canned_checks.CheckChangeHasNoTabs(
          input_api, output_api, source_file_filter=non_third_party_sources))
  results.extend(
      input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
          input_api, output_api, source_file_filter=non_third_party_sources))
  results.extend(
      input_api.canned_checks.CheckAuthorizedAuthor(
          input_api,
          output_api,
          bot_allowlist=[
              'chromium-webrtc-autoroll@webrtc-ci.iam.gserviceaccount.com',
              'webrtc-version-updater@webrtc-ci.iam.gserviceaccount.com',
          ]))
  results.extend(
      input_api.canned_checks.CheckChangeTodoHasOwner(
          input_api, output_api, source_file_filter=non_third_party_sources))
  results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
  results.extend(CheckNativeApiHeaderChanges(input_api, output_api))
  results.extend(
      CheckNoIOStreamInHeaders(input_api,
                               output_api,
                               source_file_filter=non_third_party_sources))
  results.extend(
      CheckNoPragmaOnce(input_api,
                        output_api,
                        source_file_filter=non_third_party_sources))
  results.extend(
      CheckNoFRIEND_TEST(input_api,
                         output_api,
                         source_file_filter=non_third_party_sources))
  results.extend(CheckGnChanges(input_api, output_api))
  results.extend(
      CheckUnwantedDependencies(input_api,
                                output_api,
                                source_file_filter=non_third_party_sources))
  results.extend(
      CheckJSONParseErrors(input_api,
                           output_api,
                           source_file_filter=non_third_party_sources))
  results.extend(RunPythonTests(input_api, output_api))
  results.extend(
      CheckUsageOfGoogleProtobufNamespace(
          input_api, output_api, source_file_filter=non_third_party_sources))
  results.extend(
      CheckOrphanHeaders(input_api,
                         output_api,
                         source_file_filter=non_third_party_sources))
  results.extend(
      CheckNewlineAtTheEndOfProtoFiles(
          input_api, output_api, source_file_filter=non_third_party_sources))
  results.extend(
      CheckNoStreamUsageIsAdded(input_api, output_api, non_third_party_sources))
  results.extend(
      CheckNoTestCaseUsageIsAdded(input_api, output_api,
                                  non_third_party_sources))
  results.extend(CheckAddedDepsHaveTargetApprovals(input_api, output_api))
  results.extend(CheckApiDepsFileIsUpToDate(input_api, output_api))
  results.extend(
      CheckAbslMemoryInclude(input_api, output_api, non_third_party_sources))
  results.extend(
      CheckAssertUsage(input_api, output_api, non_third_party_sources))
  results.extend(
      CheckBannedAbslMakeUnique(input_api, output_api, non_third_party_sources))
  results.extend(
      CheckObjcApiSymbols(input_api, output_api, non_third_party_sources))
  return results


def CheckApiDepsFileIsUpToDate(input_api, output_api):
  """Check that 'include_rules' in api/DEPS is up to date.

  The file api/DEPS must be kept up to date in order to avoid to avoid to
  include internal header from WebRTC's api/ headers.

  This check is focused on ensuring that 'include_rules' contains a deny
  rule for each root level directory. More focused allow rules can be
  added to 'specific_include_rules'.
  """
  results = []
  api_deps = os.path.join(input_api.PresubmitLocalPath(), 'api', 'DEPS')
  with open(api_deps) as f:
    deps_content = _ParseDeps(f.read())

  include_rules = deps_content.get('include_rules', [])
  dirs_to_skip = set(['api', 'docs'])

  # Only check top level directories affected by the current CL.
  dirs_to_check = set()
  for f in input_api.AffectedFiles():
    path_tokens = [t for t in f.LocalPath().split(os.sep) if t]
    if len(path_tokens) > 1:
      if (path_tokens[0] not in dirs_to_skip and os.path.isdir(
          os.path.join(input_api.PresubmitLocalPath(), path_tokens[0]))):
        dirs_to_check.add(path_tokens[0])

  missing_include_rules = set()
  for p in dirs_to_check:
    rule = '-%s' % p
    if rule not in include_rules:
      missing_include_rules.add(rule)

  if missing_include_rules:
    error_msg = [
        'include_rules = [\n',
        '  ...\n',
    ]

    for r in sorted(missing_include_rules):
      error_msg.append('  "%s",\n' % str(r))

    error_msg.append('  ...\n')
    error_msg.append(']\n')

    results.append(
        output_api.PresubmitError(
            'New root level directory detected! WebRTC api/ headers should '
            'not #include headers from \n'
            'the new directory, so please update "include_rules" in file\n'
            '"%s". Example:\n%s\n' % (api_deps, ''.join(error_msg))))

  return results


def CheckBannedAbslMakeUnique(input_api, output_api, source_file_filter):
  file_filter = lambda f: (f.LocalPath().endswith(('.cc', '.h')) and
                           source_file_filter(f))

  files = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    for _, line in f.ChangedContents():
      if 'absl::make_unique' in line:
        files.append(f)
        break

  if files:
    return [
        output_api.PresubmitError(
            'Please use std::make_unique instead of absl::make_unique.\n'
            'Affected files:', files)
    ]
  return []


def CheckObjcApiSymbols(input_api, output_api, source_file_filter):
  rtc_objc_export = re.compile(r'RTC_OBJC_EXPORT(.|\n){26}',
                               re.MULTILINE | re.DOTALL)
  file_filter = lambda f: (f.LocalPath().endswith(('.h')) and
                           source_file_filter(f))

  files = []
  file_filter = lambda x: (input_api.FilterSourceFile(x) and source_file_filter(
      x))
  for f in input_api.AffectedSourceFiles(file_filter):
    if not f.LocalPath().endswith('.h') or not 'sdk/objc' in f.LocalPath():
      continue
    if f.LocalPath().endswith('sdk/objc/base/RTCMacros.h'):
      continue
    contents = input_api.ReadFile(f)
    for match in rtc_objc_export.finditer(contents):
      export_block = match.group(0)
      if 'RTC_OBJC_TYPE' not in export_block:
        files.append(f.LocalPath())

  if len(files) > 0:
    return [
        output_api.PresubmitError(
            'RTC_OBJC_EXPORT types must be wrapped into an RTC_OBJC_TYPE() ' +
            'macro.\n\n' + 'For example:\n' +
            'RTC_OBJC_EXPORT @protocol RTC_OBJC_TYPE(RtcFoo)\n\n' +
            'RTC_OBJC_EXPORT @interface RTC_OBJC_TYPE(RtcFoo)\n\n' +
            'Please fix the following files:', files)
    ]
  return []


def CheckAssertUsage(input_api, output_api, source_file_filter):
  pattern = input_api.re.compile(r'\bassert\(')
  file_filter = lambda f: (f.LocalPath().endswith(('.cc', '.h', '.m', '.mm'))
                           and source_file_filter(f))

  files = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    for _, line in f.ChangedContents():
      if pattern.search(line):
        files.append(f.LocalPath())
        break

  if len(files) > 0:
    return [
        output_api.PresubmitError(
            'Usage of assert() has been detected in the following files, '
            'please use RTC_DCHECK() instead.\n Files:', files)
    ]
  return []


def CheckAbslMemoryInclude(input_api, output_api, source_file_filter):
  pattern = input_api.re.compile(r'^#include\s*"absl/memory/memory.h"',
                                 input_api.re.MULTILINE)
  file_filter = lambda f: (f.LocalPath().endswith(('.cc', '.h')) and
                           source_file_filter(f))

  files = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      continue
    for _, line in f.ChangedContents():
      if 'absl::WrapUnique' in line:
        files.append(f)
        break

  if len(files) > 0:
    return [
        output_api.PresubmitError(
            'Please include "absl/memory/memory.h" header for '
            'absl::WrapUnique.\nThis header may or may not be included '
            'transitively depending on the C++ standard version.', files)
    ]
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(CommonChecks(input_api, output_api))
  results.extend(CheckGnGen(input_api, output_api))
  results.extend(input_api.canned_checks.CheckGNFormatted(
      input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(CommonChecks(input_api, output_api))
  results.extend(VerifyNativeApiHeadersListIsValid(input_api, output_api))
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangeWasUploaded(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangeHasDescription(input_api, output_api))
  results.extend(CheckChangeHasBugField(input_api, output_api))
  results.extend(CheckCommitMessageBugEntry(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckTreeIsOpen(
          input_api,
          output_api,
          json_url='http://webrtc-status.appspot.com/current?format=json'))
  return results


def CheckOrphanHeaders(input_api, output_api, source_file_filter):
  # We need to wait until we have an input_api object and use this
  # roundabout construct to import prebubmit_checks_lib because this file is
  # eval-ed and thus doesn't have __file__.
  error_msg = """{} should be listed in {}."""
  results = []
  exempt_paths = [re.escape(os.path.join('tools_webrtc', 'ios', 'SDK'))]

  with _AddToPath(
      input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools_webrtc',
                             'presubmit_checks_lib')):
    from check_orphan_headers import GetBuildGnPathFromFilePath
    from check_orphan_headers import IsHeaderInBuildGn

  file_filter = lambda x: input_api.FilterSourceFile(
      x, files_to_skip=exempt_paths) and source_file_filter(x)
  for f in input_api.AffectedSourceFiles(file_filter):
    if f.LocalPath().endswith('.h'):
      file_path = os.path.abspath(f.LocalPath())
      root_dir = os.getcwd()
      gn_file_path = GetBuildGnPathFromFilePath(file_path, os.path.exists,
                                                root_dir)
      in_build_gn = IsHeaderInBuildGn(file_path, gn_file_path)
      if not in_build_gn:
        results.append(
            output_api.PresubmitError(
                error_msg.format(f.LocalPath(), os.path.relpath(gn_file_path))))
  return results


def CheckNewlineAtTheEndOfProtoFiles(input_api, output_api,
                                     source_file_filter):
  """Checks that all .proto files are terminated with a newline."""
  error_msg = 'File {} must end with exactly one newline.'
  results = []
  file_filter = lambda x: input_api.FilterSourceFile(
      x, files_to_check=(r'.+\.proto$', )) and source_file_filter(x)
  for f in input_api.AffectedSourceFiles(file_filter):
    file_path = f.LocalPath()
    with open(file_path) as f:
      lines = f.readlines()
      if len(lines) > 0 and not lines[-1].endswith('\n'):
        results.append(output_api.PresubmitError(error_msg.format(file_path)))
  return results


def _ExtractAddRulesFromParsedDeps(parsed_deps):
  """Extract the rules that add dependencies from a parsed DEPS file.

  Args:
    parsed_deps: the locals dictionary from evaluating the DEPS file."""
  add_rules = set()
  add_rules.update([
      rule[1:] for rule in parsed_deps.get('include_rules', [])
      if rule.startswith('+') or rule.startswith('!')
  ])
  for _, rules in parsed_deps.get('specific_include_rules', {}).items():
    add_rules.update([
        rule[1:] for rule in rules
        if rule.startswith('+') or rule.startswith('!')
    ])
  return add_rules


def _ParseDeps(contents):
  """Simple helper for parsing DEPS files."""

  # Stubs for handling special syntax in the root DEPS file.
  class VarImpl:
    def __init__(self, local_scope):
      self._local_scope = local_scope

    def Lookup(self, var_name):
      """Implements the Var syntax."""
      try:
        return self._local_scope['vars'][var_name]
      except KeyError as var_not_defined:
        raise Exception('Var is not defined: %s' %
                        var_name) from var_not_defined

  local_scope = {}
  global_scope = {
      'Var': VarImpl(local_scope).Lookup,
  }
  exec(contents, global_scope, local_scope)
  return local_scope


def _CalculateAddedDeps(os_path, old_contents, new_contents):
  """Helper method for _CheckAddedDepsHaveTargetApprovals. Returns
  a set of DEPS entries that we should look up.

  For a directory (rather than a specific filename) we fake a path to
  a specific filename by adding /DEPS. This is chosen as a file that
  will seldom or never be subject to per-file include_rules.
  """
  # We ignore deps entries on auto-generated directories.
  auto_generated_dirs = ['grit', 'jni']

  old_deps = _ExtractAddRulesFromParsedDeps(_ParseDeps(old_contents))
  new_deps = _ExtractAddRulesFromParsedDeps(_ParseDeps(new_contents))

  added_deps = new_deps.difference(old_deps)

  results = set()
  for added_dep in added_deps:
    if added_dep.split('/')[0] in auto_generated_dirs:
      continue
    # Assume that a rule that ends in .h is a rule for a specific file.
    if added_dep.endswith('.h'):
      results.add(added_dep)
    else:
      results.add(os_path.join(added_dep, 'DEPS'))
  return results


def CheckAddedDepsHaveTargetApprovals(input_api, output_api):
  """When a dependency prefixed with + is added to a DEPS file, we
    want to make sure that the change is reviewed by an OWNER of the
    target file or directory, to avoid layering violations from being
    introduced. This check verifies that this happens.
    """
  virtual_depended_on_files = set()

  file_filter = lambda f: not input_api.re.match(
      r"^third_party[\\\/](WebKit|blink)[\\\/].*", f.LocalPath())
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    filename = input_api.os_path.basename(f.LocalPath())
    if filename == 'DEPS':
      virtual_depended_on_files.update(
          _CalculateAddedDeps(input_api.os_path, '\n'.join(f.OldContents()),
                              '\n'.join(f.NewContents())))

  if not virtual_depended_on_files:
    return []

  if input_api.is_committing:
    if input_api.tbr:
      return [
          output_api.PresubmitNotifyResult(
              '--tbr was specified, skipping OWNERS check for DEPS '
              'additions')
      ]
    if input_api.dry_run:
      return [
          output_api.PresubmitNotifyResult(
              'This is a dry run, skipping OWNERS check for DEPS '
              'additions')
      ]
    if not input_api.change.issue:
      return [
          output_api.PresubmitError(
              "DEPS approval by OWNERS check failed: this change has "
              "no change number, so we can't check it for approvals.")
      ]
    output = output_api.PresubmitError
  else:
    output = output_api.PresubmitNotifyResult

  owner_email, reviewers = (
      input_api.canned_checks.GetCodereviewOwnerAndReviewers(
          input_api, None, approval_needed=input_api.is_committing))

  owner_email = owner_email or input_api.change.author_email

  approval_status = input_api.owners_client.GetFilesApprovalStatus(
      virtual_depended_on_files, reviewers.union([owner_email]), [])
  missing_files = [
      f for f in virtual_depended_on_files
      if approval_status[f] != input_api.owners_client.APPROVED
  ]

  # We strip the /DEPS part that was added by
  # _FilesToCheckForIncomingDeps to fake a path to a file in a
  # directory.
  def StripDeps(path):
    start_deps = path.rfind('/DEPS')
    if start_deps != -1:
      return path[:start_deps]
    return path

  unapproved_dependencies = [
      "'+%s'," % StripDeps(path) for path in missing_files
  ]

  if unapproved_dependencies:
    output_list = [
        output('You need LGTM from owners of depends-on paths in DEPS that '
               ' were modified in this CL:\n    %s' %
               '\n    '.join(sorted(unapproved_dependencies)))
    ]
    suggested_owners = input_api.owners_client.SuggestOwners(
        missing_files, exclude=[owner_email])
    output_list.append(
        output('Suggested missing target path OWNERS:\n    %s' %
               '\n    '.join(suggested_owners or [])))
    return output_list

  return []
