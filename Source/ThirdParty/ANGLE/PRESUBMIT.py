# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for code generation.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

# Fragment of a regular expression that matches C++ and Objective-C++ implementation files and headers.
_IMPLEMENTATION_AND_HEADER_EXTENSIONS = r'\.(cc|cpp|cxx|mm|h|hpp|hxx)$'

# Fragment of a regular expression that matches C++ and Objective-C++ header files.
_HEADER_EXTENSIONS = r'\.(h|hpp|hxx)$'

_PRIMARY_EXPORT_TARGETS = [
    '//:libEGL',
    '//:libGLESv1_CM',
    '//:libGLESv2',
    '//:translator',
]


def _CheckChangeHasBugField(input_api, output_api):
    """Requires that the changelist have a Bug: field from a known project."""
    bugs = input_api.change.BugsFromDescription()
    if not bugs:
        return [
            output_api.PresubmitError('Please ensure that your description contains:\n'
                                      '"Bug: angleproject:[bug number]"\n'
                                      'directly above the Change-Id tag.')
        ]

    # The bug must be in the form of "project:number".  None is also accepted, which is used by
    # rollers as well as in very minor changes.
    if len(bugs) == 1 and bugs[0] == 'None':
        return []

    projects = ['angleproject', 'chromium', 'dawn', 'fuchsia', 'skia', 'swiftshader']
    bug_regex = re.compile(r"([a-z]+):(\d+)")
    errors = []
    extra_help = None

    for bug in bugs:
        if bug == 'None':
            errors.append(
                output_api.PresubmitError('Invalid bug tag "None" in presence of other bug tags.'))
            continue

        match = re.match(bug_regex, bug)
        if match == None or bug != match.group(0) or match.group(1) not in projects:
            errors.append(output_api.PresubmitError('Incorrect bug tag "' + bug + '".'))
            if not extra_help:
                extra_help = output_api.PresubmitError('Acceptable format is:\n\n'
                                                       '    Bug: project:bugnumber\n\n'
                                                       'Acceptable projects are:\n\n    ' +
                                                       '\n    '.join(projects))

    if extra_help:
        errors.append(extra_help)

    return errors


def _CheckCodeGeneration(input_api, output_api):

    class Msg(output_api.PresubmitError):
        """Specialized error message"""

        def __init__(self, message):
            super(output_api.PresubmitError, self).__init__(
                message,
                long_text='Please ensure your ANGLE repositiory is synced to tip-of-tree\n'
                'and all ANGLE DEPS are fully up-to-date by running gclient sync.\n'
                '\n'
                'If that fails, run scripts/run_code_generation.py to refresh generated hashes.\n'
                '\n'
                'If you are building ANGLE inside Chromium you must bootstrap ANGLE\n'
                'before gclient sync. See the DevSetup documentation for more details.\n')

    code_gen_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                           'scripts/run_code_generation.py')
    cmd_name = 'run_code_generation'
    cmd = [input_api.python_executable, code_gen_path, '--verify-no-dirty']
    test_cmd = input_api.Command(name=cmd_name, cmd=cmd, kwargs={}, message=Msg)
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
        return input_api.FilterSourceFile(f, white_list=(r'.+%s' % _HEADER_EXTENSIONS,))

    new_headers = []
    for f in input_api.AffectedSourceFiles(headers):
        if f.Action() != 'A':
            continue
        new_headers.append(f.LocalPath())

    def gn_files(f):
        return input_api.FilterSourceFile(f, white_list=(r'.+\.gn',))

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
        return [
            output_api.PresubmitPromptWarning(
                'Missing GN changes for new header files',
                items=sorted(problems),
                long_text='Please double check whether newly added header files need '
                'corresponding changes in gn or gni files.\nThis checking is only a '
                'heuristic. Run build/check_gn_headers.py to be precise.\n'
                'Read https://crbug.com/661774 for more info.')
        ]
    return []


def _CheckExportValidity(input_api, output_api):
    outdir = tempfile.mkdtemp()
    # shell=True is necessary on Windows, as otherwise subprocess fails to find
    # either 'gn' or 'vpython3' even if they are findable via PATH.
    use_shell = input_api.is_windows
    try:
        try:
            subprocess.check_output(['gn', 'gen', outdir], shell=use_shell)
        except subprocess.CalledProcessError as e:
            return [
                output_api.PresubmitError(
                    'Unable to run gn gen for export_targets.py: %s' % e.output)
            ]
        export_target_script = os.path.join(input_api.PresubmitLocalPath(), 'scripts',
                                            'export_targets.py')
        try:
            subprocess.check_output(
                ['vpython3', export_target_script, outdir] + _PRIMARY_EXPORT_TARGETS,
                stderr=subprocess.STDOUT,
                shell=use_shell)
        except subprocess.CalledProcessError as e:
            if input_api.is_committing:
                return [output_api.PresubmitError('export_targets.py failed: %s' % e.output)]
            return [
                output_api.PresubmitPromptWarning(
                    'export_targets.py failed, this may just be due to your local checkout: %s' %
                    e.output)
            ]
        return []
    finally:
        shutil.rmtree(outdir)


def _CheckTabsInSourceFiles(input_api, output_api):
    """Forbids tab characters in source files due to a WebKit repo requirement. """

    def implementation_and_headers(f):
        return input_api.FilterSourceFile(
            f, white_list=(r'.+%s' % _IMPLEMENTATION_AND_HEADER_EXTENSIONS,))

    files_with_tabs = []
    for f in input_api.AffectedSourceFiles(implementation_and_headers):
        for (num, line) in f.ChangedContents():
            if '\t' in line:
                files_with_tabs.append(f)
                break

    if files_with_tabs:
        return [
            output_api.PresubmitError(
                'Tab characters in source files.',
                items=sorted(files_with_tabs),
                long_text=
                'Tab characters are forbidden in ANGLE source files because WebKit\'s Subversion\n'
                'repository does not allow tab characters in source files.\n'
                'Please remove tab characters from these files.')
        ]
    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTabsInSourceFiles(input_api, output_api))
    results.extend(_CheckCodeGeneration(input_api, output_api))
    results.extend(_CheckChangeHasBugField(input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(input_api, output_api))
    results.extend(_CheckNewHeaderWithoutGnChange(input_api, output_api))
    results.extend(_CheckExportValidity(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(
            input_api, output_api, result_factory=output_api.PresubmitError))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckCodeGeneration(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(
            input_api, output_api, result_factory=output_api.PresubmitError))
    results.extend(_CheckChangeHasBugField(input_api, output_api))
    results.extend(_CheckExportValidity(input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(input_api, output_api))
    return results
