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
import json
import sys

CHECKER_MAP = {
    'Uncounted call argument for a raw pointer/reference parameter': 'UncountedCallArgsChecker',
    'Uncounted raw pointer or reference not provably backed by ref-counted variable': 'UncountedLocalVarsChecker'
}

PROJECTS = ['WebKit', 'WebCore']


def parser():
    parser = argparse.ArgumentParser(description='analyze clang results')
    parser.add_argument(
        'results_dir',
        help='directory of results to parse'
    )
    parser.add_argument(
        '--output-dir',
        dest='output_dir',
        help='output directory for dirty files list',
        default='smart-pointer-result-archive'
    )
    parser.add_argument(
        '--build-dir',
        dest='build_dir',
        help='path to build directory, used to standardize file paths'
    )

    return parser.parse_args()


def parse_results_file(args, file_path):
    bug_type, bug_file, issue_hash, bug_line = None, None, None, None
    with open(file_path, 'r') as f:
        while True:
            lines = f.readlines(250)
            if not lines:
                break
            for line in lines:
                if 'BUGFILE' in line:
                    bug_file = line.removeprefix('<!-- BUGFILE ')
                    bug_file = bug_file.removesuffix(' -->\n')
                    if args.build_dir:
                        bug_file = bug_file.removeprefix(f'{args.build_dir}/')
                if 'ISSUEHASHCONTENTOFLINEINCONTEXT' in line:
                    issue_hash = line.removeprefix('<!-- ISSUEHASHCONTENTOFLINEINCONTEXT ')
                    issue_hash = issue_hash.removesuffix(' -->\n')
                if 'BUGTYPE' in line:
                    bug_type = line.removeprefix('<!-- BUGTYPE ')
                    bug_type = bug_type.removesuffix(' -->\n')
                if 'BUGLINE' in line:
                    bug_line = line.removeprefix('<!-- BUGLINE ')
                    bug_line = bug_line.removesuffix(' -->\n')
                if bug_file and issue_hash and bug_type and bug_line:
                    return bug_file, issue_hash, bug_type, bug_line
    return None, None, None, None


def find_project_results(args, project, file_list, results_data):
    bug_counts = {
        'Uncounted call argument for a raw pointer/reference parameter': 0,
        'Uncounted raw pointer or reference not provably backed by ref-counted variable': 0
    }

    for result_file in file_list:
        if result_file:
            file_name, issue_hash, bug_type, bug_line = parse_results_file(args, result_file)
            if not file_name:
                continue

            # Create files listing issue hashes and file names.
            bug_counts[bug_type] += 1
            issue_obj = {"hash": issue_hash, "bugtype": bug_type, "line": bug_line}
            list_of_issues = results_data.get(file_name, [])
            list_of_issues.append(issue_obj)
            results_data[file_name] = list_of_issues

            output_file_name = os.path.abspath(f'{args.output_dir}/{project}/{CHECKER_MAP[bug_type]}-issues')
            f = open(output_file_name, 'a')
            f.write(f'{issue_hash}\n')
            f.close()

            output_file_name_2 = os.path.abspath(f'{args.output_dir}/{project}/{CHECKER_MAP[bug_type]}-files')
            f = open(output_file_name_2, 'a')
            f.write(f'{file_name}\n')
            f.close()

    for type, count in bug_counts.items():
        print(f'    {type}: {count}')
    return results_data


def find_all_results(args):
    file_list = []
    results_data = {}
    result_counts = {}

    for project in PROJECTS:
        subprocess.run(['mkdir', os.path.abspath(f'{args.output_dir}/{project}')])
        path = os.path.abspath(os.path.join(args.results_dir, 'StaticAnalyzer', project))
        command = 'find {} -name report\\*.html -print'.format(path)
        try:
            result_files = subprocess.check_output(command, shell=True, text=True)
        except subprocess.CalledProcessError as e:
            sys.stderr.write(f'{e.output}')
            sys.stderr.write(f'Could not find results for {project}\n')
            return -1
        project_files = result_files.splitlines()
        file_list.extend(project_files)
        result_counts[project] = len(project_files)

        print(f'\n------ {project} ------\n')
        print(f'TOTAL ISSUES: {len(project_files)}')
        find_project_results(args, project, project_files, results_data)

    print("\nWriting results files...")
    results_data_file = os.path.abspath(f'{args.output_dir}/dirty_file_data.json')
    with open(results_data_file, "w") as f:
        results_data_obj = json.dumps(results_data, indent=4)
        f.write(results_data_obj)
    print(f'Done! Find them in {os.path.abspath(args.output_dir)}\n')

    results_msg = f'Total ({sum([c for c in result_counts.values()])}) '
    for proj, count in result_counts.items():
        results_msg += f'{proj} ({count}) '
    print(results_msg)


def main():
    args = parser()
    try:
        subprocess.run(['mkdir', '-p', args.output_dir])
    except subprocess.CalledProcessError as e:
        sys.stderr.write(f'{e.output}\n')

    if args.results_dir:
        find_all_results(args)


if __name__ == '__main__':
    main()
