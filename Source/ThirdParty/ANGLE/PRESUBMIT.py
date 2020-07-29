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


def _CheckCommitMessageFormatting(input_api, output_api):

    def _IsLineBlank(line):
        return line.isspace() or line == ""

    def _PopBlankLines(lines, reverse=False):
        if reverse:
            while len(lines) > 0 and _IsLineBlank(lines[-1]):
                lines.pop()
        else:
            while len(lines) > 0 and _IsLineBlank(lines[0]):
                lines.pop(0)

    def _IsTagLine(line):
        return ":" in line

    def _SplitIntoMultipleCommits(description_text):
        paragraph_split_pattern = r"((?m)^\s*$\n)"
        multiple_paragraphs = re.split(paragraph_split_pattern, description_text)
        multiple_commits = [""]
        change_id_pattern = re.compile(r"(?m)^Change-Id: [a-zA-Z0-9]*$")
        for paragraph in multiple_paragraphs:
            multiple_commits[-1] += paragraph
            if change_id_pattern.search(paragraph):
                multiple_commits.append("")
        if multiple_commits[-1] == "":
            multiple_commits.pop()
        return multiple_commits

    def _CheckTabInCommit(lines):
        return all([line.find("\t") == -1 for line in lines])

    whitelist_strings = ['Revert "', 'Roll ', 'Reland ']
    summary_linelength_warning_lower_limit = 65
    summary_linelength_warning_upper_limit = 70
    description_linelength_limit = 72

    git_output = input_api.change.DescriptionText()

    multiple_commits = _SplitIntoMultipleCommits(git_output)
    errors = []

    for k in range(len(multiple_commits)):
        commit_msg_lines = multiple_commits[k].splitlines()
        commit_number = len(multiple_commits) - k
        commit_tag = "Commit " + str(commit_number) + ":"
        commit_msg_line_numbers = {}
        for i in range(len(commit_msg_lines)):
            commit_msg_line_numbers[commit_msg_lines[i]] = i + 1
        _PopBlankLines(commit_msg_lines, True)
        _PopBlankLines(commit_msg_lines, False)
        whitelisted = False
        if len(commit_msg_lines) > 0:
            for whitelist_string in whitelist_strings:
                if commit_msg_lines[0].startswith(whitelist_string):
                    whitelisted = True
                    break
        if whitelisted:
            continue

        if not _CheckTabInCommit(commit_msg_lines):
            errors.append(
                output_api.PresubmitError(commit_tag + "Tabs are not allowed in commit message."))

        # the tags paragraph is at the end of the message
        # the break between the tags paragraph is the first line without ":"
        # this is sufficient because if a line is blank, it will not have ":"
        last_paragraph_line_count = 0
        while len(commit_msg_lines) > 0 and _IsTagLine(commit_msg_lines[-1]):
            last_paragraph_line_count += 1
            commit_msg_lines.pop()
        if last_paragraph_line_count == 0:
            errors.append(
                output_api.PresubmitError(
                    commit_tag +
                    "Please ensure that there are tags (e.g., Bug:, Test:) in your description."))
        if len(commit_msg_lines) > 0:
            if not _IsLineBlank(commit_msg_lines[-1]):
                output_api.PresubmitError(commit_tag +
                                          "Please ensure that there exists 1 blank line " +
                                          "between tags and description body.")
            else:
                # pop the blank line between tag paragraph and description body
                commit_msg_lines.pop()
                if len(commit_msg_lines) > 0 and _IsLineBlank(commit_msg_lines[-1]):
                    errors.append(
                        output_api.PresubmitError(
                            commit_tag + 'Please ensure that there exists only 1 blank line '
                            'between tags and description body.'))
                    # pop all the remaining blank lines between tag and description body
                    _PopBlankLines(commit_msg_lines, True)
        if len(commit_msg_lines) == 0:
            errors.append(
                output_api.PresubmitError(commit_tag +
                                          'Please ensure that your description summary'
                                          ' and description body are not blank.'))
            continue

        if summary_linelength_warning_lower_limit <= len(commit_msg_lines[0]) \
        <= summary_linelength_warning_upper_limit:
            errors.append(
                output_api.PresubmitPromptWarning(
                    commit_tag + "Your description summary should be on one line of " +
                    str(summary_linelength_warning_lower_limit - 1) + " or less characters."))
        elif len(commit_msg_lines[0]) > summary_linelength_warning_upper_limit:
            errors.append(
                output_api.PresubmitError(
                    commit_tag + "Please ensure that your description summary is on one line of " +
                    str(summary_linelength_warning_lower_limit - 1) + " or less characters."))
        commit_msg_lines.pop(0)  # get rid of description summary
        if len(commit_msg_lines) == 0:
            continue
        if not _IsLineBlank(commit_msg_lines[0]):
            errors.append(
                output_api.PresubmitError(commit_tag +
                                          'Please ensure the summary is only 1 line and '
                                          'there is 1 blank line between the summary '
                                          'and description body.'))
        else:
            commit_msg_lines.pop(0)  # pop first blank line
            if len(commit_msg_lines) == 0:
                continue
            if _IsLineBlank(commit_msg_lines[0]):
                errors.append(
                    output_api.PresubmitError(commit_tag +
                                              'Please ensure that there exists only 1 blank line '
                                              'between description summary and description body.'))
                # pop all the remaining blank lines between
                # description summary and description body
                _PopBlankLines(commit_msg_lines)

        # loop through description body
        while len(commit_msg_lines) > 0:
            line = commit_msg_lines.pop(0)
            # lines starting with 4 spaces or lines without space(urls)
            # are exempt from length check
            if line.startswith("    ") or " " not in line:
                continue
            if len(line) > description_linelength_limit:
                errors.append(
                    output_api.PresubmitError(
                        commit_tag + 'Line ' + str(commit_msg_line_numbers[line]) +
                        ' is too long.\n' + '"' + line + '"\n' + 'Please wrap it to ' +
                        str(description_linelength_limit) + ' characters. ' +
                        "Lines without spaces or lines starting with 4 spaces are exempt."))
                break
    return errors


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

    projects = ['angleproject:', 'chromium:', 'dawn:', 'fuchsia:', 'skia:', 'swiftshader:', 'b/']
    bug_regex = re.compile(r"([a-z]+[:/])(\d+)")
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


# https://stackoverflow.com/a/196392
def is_ascii(s):
    return all(ord(c) < 128 for c in s)


def _CheckNonAsciiInSourceFiles(input_api, output_api):
    """Forbids non-ascii characters in source files. """

    def implementation_and_headers(f):
        return input_api.FilterSourceFile(
            f, white_list=(r'.+%s' % _IMPLEMENTATION_AND_HEADER_EXTENSIONS,))

    files_with_non_ascii = []
    for f in input_api.AffectedSourceFiles(implementation_and_headers):
        for (num, line) in f.ChangedContents():
            if not is_ascii(line):
                files_with_non_ascii.append("%s: %s" % (f, line))
                break

    if files_with_non_ascii:
        return [
            output_api.PresubmitError(
                'Non-ASCII characters in source files.',
                items=sorted(files_with_non_ascii),
                long_text='Non-ASCII characters are forbidden in ANGLE source files.\n'
                'Please remove non-ASCII characters from these files.')
        ]
    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTabsInSourceFiles(input_api, output_api))
    results.extend(_CheckNonAsciiInSourceFiles(input_api, output_api))
    results.extend(_CheckCodeGeneration(input_api, output_api))
    results.extend(_CheckChangeHasBugField(input_api, output_api))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(input_api, output_api))
    results.extend(_CheckNewHeaderWithoutGnChange(input_api, output_api))
    results.extend(_CheckExportValidity(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(
            input_api, output_api, result_factory=output_api.PresubmitError))
    results.extend(_CheckCommitMessageFormatting(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    return CheckChangeOnUpload(input_api, output_api)
