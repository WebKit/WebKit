#!/usr/bin/env python3
# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import subprocess
import argparse
import sys

CHECKERS = ['UncountedCallArgsChecker', 'UncountedLocalVarsChecker']
PROJECTS = ['WebKit', 'WebCore']


def parser():
    parser = argparse.ArgumentParser(description='compare dirty file lists')
    parser.add_argument(
        'archived_dir',
        help='directory of dirty lists from previous build'
    )
    parser.add_argument(
        'new_dir',
        help='directory of dirty lists from new build'
    )
    parser.add_argument(
        '--build-output',
        dest='build_output',
        help='output from new build',
        required=True
    )
    parser.add_argument(
        '--scan-build-path',
        dest='scan_build',
        help='path to scan-build'
    )

    return parser.parse_args()


def find_diff(file1, file2, mode):
    # Find new regressions
    new_lines_list = []
    find_issues_cmd = f"/usr/bin/grep -F -v -f {file1}-{mode} {file2}-{mode}"
    try:
        new_lines = subprocess.check_output(find_issues_cmd, shell=True, stderr=subprocess.STDOUT, text=True)
        new_lines_list = new_lines.splitlines()
    except subprocess.CalledProcessError as e:
        if not e.returncode == 1:
            sys.stderr.write(f'{e.output}')

    # Find all fixes
    fixed_lines_list = []
    find_fixes_cmd = f'grep -F -v -f {file2}-{mode} {file1}-{mode}'
    try:
        fixed_lines = subprocess.check_output(find_fixes_cmd, shell=True, text=True, stderr=subprocess.STDOUT)
        fixed_lines_list = fixed_lines.splitlines()
    except subprocess.CalledProcessError as e:
        if not e.returncode == 1:
            sys.stderr.write(f'{e.output}')

    return set(new_lines_list), set(fixed_lines_list)


def compare_project_results(args, archive_path, new_path, project):
    new_issues_total = set()
    new_files_total = set()
    fixed_issues_total = set()
    fixed_files_total = set()

    for checker in CHECKERS:
        print(f'{checker}:')
        new_issues, fixed_issues = find_diff(f'{archive_path}/{checker}', f'{new_path}/{checker}', 'issues')
        new_files, fixed_files = find_diff(f'{archive_path}/{checker}', f'{new_path}/{checker}', 'files')
        fixed_issues_total.update(fixed_issues)
        fixed_files_total.update(fixed_files)
        new_issues_total.update(new_issues)
        new_files_total.update(new_files)

        print(f'    Fixed {len(fixed_issues)} issue(s).')
        print(f'    Fixed {len(fixed_files)} file(s).')
        print(f'    {len(new_issues)} new issue(s).')
        print(f'    {len(new_files)} new file(s) with issues.\n')

    if new_issues_total:
        create_filtered_results_dir(args, project, new_issues_total, 'StaticAnalyzerRegressions')

    return new_issues_total


def create_filtered_results_dir(args, project, issues, category='StaticAnalyzerRegressions'):
    print(f'Creating {category} and linking results...')
    # Create symlinks to new issues only so that we can run scan-build to generate new index.html files
    path_to_reports = os.path.abspath(f'{args.build_output}/{category}/{project}/StaticAnalyzerReports')
    subprocess.run(['mkdir', '-p', path_to_reports])
    for issue_hash in issues:
        report = f"report-{issue_hash[:6]}.html"
        path_to_report = f'{args.build_output}/StaticAnalyzer/{project}/StaticAnalyzerReports/{report}'
        path_to_report_new = os.path.join(path_to_reports, report)
        subprocess.run(['ln', '-s', os.path.abspath(path_to_report), path_to_report_new])

    path_to_project = f'{args.build_output}/{category}/{project}'
    subprocess.run([args.scan_build, '--generate-index-only', os.path.abspath(path_to_project)])


def main():
    args = parser()
    new_issues_total = set()

    for project in PROJECTS:
        archive_path = os.path.abspath(f'{args.archived_dir}/{project}')
        new_path = os.path.abspath(f'{args.new_dir}/{project}')
        print(f'\n------ {project} ------\n')
        new_issues = compare_project_results(args, archive_path, new_path, project)
        new_issues_total.update(new_issues)

    if new_issues_total:
        print(f'\nTotal new issues: {len(new_issues_total)}')

    return 0


if __name__ == '__main__':
    main()
