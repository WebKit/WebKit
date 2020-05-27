# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2014 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import os.path
import shutil
import sys
import tempfile
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError


class InspectorGeneratorTests:

    def __init__(self, reset_results, executive):
        self.reset_results = reset_results
        self.executive = executive

    def generate_from_json(self, json_file, output_directory):
        cmd = ['python',
               'JavaScriptCore/inspector/scripts/generate-inspector-protocol-bindings.py',
               '--outputDir', output_directory,
               '--force',
               '--framework', 'Test',
               '--test',
               json_file]

        exit_code = 0
        try:
            stderr_output = self.executive.run_command(cmd)
            if stderr_output:
                self.write_error_file(json_file, output_directory, stderr_output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
        return exit_code

    def write_error_file(self, input_filepath, output_directory, error_output):
        output_filepath = os.path.join(output_directory, os.path.basename(input_filepath) + '-error')

        with open(output_filepath, "w") as output_file:
            output_file.write(error_output)

    def detect_changes(self, work_directory, reference_directory):
        changes_found = False
        for output_file in os.listdir(work_directory):
            cmd = ['diff',
                   '-u',
                   '-N',
                   os.path.join(reference_directory, output_file),
                   os.path.join(work_directory, output_file)]

            exit_code = 0
            try:
                output = self.executive.run_command(cmd)
            except ScriptError as e:
                output = e.output
                exit_code = e.exit_code

            if exit_code or output:
                print('FAIL: %s' % output_file)
                print(output)
                changes_found = True
            else:
                print('PASS: %s' % output_file)
        return changes_found

    def run_tests(self, input_directory, reference_directory):
        work_directory = reference_directory

        passed = True
        for input_file in os.listdir(input_directory):
            (name, extension) = os.path.splitext(input_file)
            if extension != '.json':
                continue
            # Generate output into the work directory (either the given one or a
            # temp one if not reset_results is performed)
            if not self.reset_results:
                work_directory = tempfile.mkdtemp()

            if self.generate_from_json(os.path.join(input_directory, input_file), work_directory):
                passed = False

            if self.reset_results:
                print("Reset results for test: %s" % (input_file))
                continue

            # Detect changes
            if self.detect_changes(work_directory, reference_directory):
                passed = False
            shutil.rmtree(work_directory)

        return passed

    def main(self):
        current_scm = detect_scm_system(os.path.dirname(os.path.abspath(__file__)))
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        all_tests_passed = True

        base_directory = os.path.join('JavaScriptCore', 'inspector', 'scripts', 'tests')
        input_directory = base_directory
        reference_directory = os.path.join(input_directory, 'expected')
        if os.path.isdir(input_directory) and os.path.isdir(reference_directory):
            all_tests_passed = self.run_tests(input_directory, reference_directory)

        print('')
        if all_tests_passed:
            print('All tests PASS!')
            return 0
        else:
            print('Some tests FAIL! (To update the reference files, execute "run-inspector-generator-tests --reset-results")')
            return -1
