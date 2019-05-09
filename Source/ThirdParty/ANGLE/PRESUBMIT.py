# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for code generation.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

from subprocess import call


# Fragment of a regular expression that matches C++ and Objective-C++ implementation files.
_IMPLEMENTATION_EXTENSIONS = r'\.(cc|cpp|cxx|mm)$'


# Fragment of a regular expression that matches C++ and Objective-C++ header files.
_HEADER_EXTENSIONS = r'\.(h|hpp|hxx)$'


def _CheckCodeGeneration(input_api, output_api):

    class Msg(output_api.PresubmitError):
      """Specialized error message"""
      def __init__(self, message):
        super(output_api.PresubmitError, self).__init__(message,
          long_text='Please ensure your ANGLE repositiory is synced to tip-of-tree\n'
          'and you have an up-to-date checkout of all ANGLE dependencies.\n'
          'If you are using ANGLE inside Chromium you may need to bootstrap ANGLE \n'
          'and run gclient sync. See the DevSetup documentation for details.\n')

    code_gen_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                           'scripts/run_code_generation.py')
    cmd_name = 'run_code_generation'
    cmd = [input_api.python_executable, code_gen_path, '--verify-no-dirty']
    test_cmd = input_api.Command(
          name=cmd_name,
          cmd=cmd,
          kwargs={},
          message=Msg)
    if input_api.verbose:
        print('Running ' + cmd_name)
    return input_api.RunTests([test_cmd])


# Taken directly from Chromium's PRESUBMIT.py
def _CheckNewHeaderWithoutGnChange(input_api, output_api):
  """Checks that newly added header files have corresponding GN changes.
  Note that this is only a heuristic. To be precise, run script:
  build/check_gn_headers.py.
  """

  def headers(f):
    return input_api.FilterSourceFile(
      f, white_list=(r'.+%s' % _HEADER_EXTENSIONS, ))

  new_headers = []
  for f in input_api.AffectedSourceFiles(headers):
    if f.Action() != 'A':
      continue
    new_headers.append(f.LocalPath())

  def gn_files(f):
    return input_api.FilterSourceFile(f, white_list=(r'.+\.gn', ))

  all_gn_changed_contents = ''
  for f in input_api.AffectedSourceFiles(gn_files):
    for _, line in f.ChangedContents():
      all_gn_changed_contents += line

  problems = []
  for header in new_headers:
    basename = input_api.os_path.basename(header)
    if basename not in all_gn_changed_contents:
      problems.append(header)

  if problems:
    return [output_api.PresubmitPromptWarning(
      'Missing GN changes for new header files', items=sorted(problems),
      long_text='Please double check whether newly added header files need '
      'corresponding changes in gn or gni files.\nThis checking is only a '
      'heuristic. Run build/check_gn_headers.py to be precise.\n'
      'Read https://crbug.com/661774 for more info.')]
  return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckCodeGeneration(input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
    results.extend(_CheckNewHeaderWithoutGnChange(input_api, output_api))
    results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckCodeGeneration(input_api, output_api))
    results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
    return results
