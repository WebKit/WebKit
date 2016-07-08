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
import tempfile
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError


class BuiltinsGeneratorTests:

    def __init__(self, reset_results, executive):
        self.reset_results = reset_results
        self.executive = executive

    def generate_from_js_builtins(self, builtins_file, output_directory, framework_name="", combined_outputs=False, generate_wrapper=False):
        cmd = ['python',
               'JavaScriptCore/Scripts/generate-js-builtins.py',
               '--output-directory', output_directory,
               '--force',
               '--framework', framework_name,
               '--test']

        if combined_outputs:
            cmd.append('--combined')

        if generate_wrapper:
            cmd.append('--with-wrapper-files')

        cmd.append(builtins_file)

        exit_code = 0
        try:
            stderr_output = self.executive.run_command(cmd)
            if stderr_output:
                self.write_error_file(builtins_file, output_directory, stderr_output)
        except ScriptError, e:
            print e.output
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
            except ScriptError, e:
                output = e.output
                exit_code = e.exit_code

            if exit_code or output:
                print 'FAIL: %s' % output_file
                print output
                changes_found = True
            else:
                print 'PASS: %s' % output_file
        return changes_found

    def run_tests(self, input_directory, reference_directory):
        work_directory = reference_directory

        passed = True
        for input_file in os.listdir(input_directory):
            (test_name, extension) = os.path.splitext(input_file)
            if extension != '.js':
                continue
            # Generate output into the work directory (either the given one or a
            # temp one if not reset_results is performed)
            if not self.reset_results:
                work_directory = tempfile.mkdtemp()

            (framework_name, test_case, output_mode) = test_name.split('-')
            if not framework_name or not output_mode or not test_case:
                print "Invalid test case name: should be Framework-TestCaseName-OutputMode.js"
                continue

            combined_outputs = output_mode == "Combined"
            generate_wrapper = framework_name == "WebCore"
            if self.generate_from_js_builtins(os.path.join(input_directory, input_file), work_directory, framework_name=framework_name, combined_outputs=combined_outputs, generate_wrapper=generate_wrapper):
                passed = False

            if self.reset_results:
                print "Reset results for test: %s" % (input_file)
                continue

            # Detect changes
            if self.detect_changes(work_directory, reference_directory):
                passed = False
            shutil.rmtree(work_directory)

        return passed

    def main(self):
        current_scm = detect_scm_system(os.curdir)
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        all_tests_passed = True

        input_directory = os.path.join('JavaScriptCore', 'Scripts', 'tests', 'builtins')
        reference_directory = os.path.join('JavaScriptCore', 'Scripts', 'tests', 'builtins', 'expected')
        if not self.run_tests(input_directory, reference_directory):
            all_tests_passed = False

        print ''
        if all_tests_passed:
            print 'All tests PASS!'
            return 0
        else:
            print 'Some tests FAIL! (To update the reference files, execute "run-builtins-generator-tests --reset-results")'
            return -1
