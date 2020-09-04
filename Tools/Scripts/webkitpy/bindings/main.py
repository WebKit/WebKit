# Copyright (C) 2011 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import fnmatch
import json
import os
import os.path
import shutil
import sys
import tempfile

from webkitcorepy import string_utils

from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError


class BindingsTests:

    def __init__(self, reset_results, generators, executive, verbose, patterns, json_file_name):
        self.reset_results = reset_results
        self.generators = generators
        self.executive = executive
        self.verbose = verbose
        self.patterns = patterns
        self.json_file_name = json_file_name

        if self.json_file_name:
            self.failures = []

    def generate_from_idl(self, generator, idl_file, output_directory, supplemental_dependency_file):
        cmd = ['perl', '-w',
               '-IWebCore/bindings/scripts',
               'WebCore/bindings/scripts/generate-bindings.pl',
               # idl include directories (path relative to generate-bindings.pl)
               '--include', '.',
               '--defines', 'TESTING_%s' % generator,
               '--generator', generator,
               '--outputDir', output_directory,
               '--supplementalDependencyFile', supplemental_dependency_file,
               '--idlAttributesFile', 'WebCore/bindings/scripts/IDLAttributes.json',
               idl_file]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
            if output:
                print(output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
        return exit_code

    def generate_supplemental_dependency(self, input_directory, supplemental_dependency_file, supplemental_makefile_dependency_file, window_constructors_file, workerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, serviceworkerglobalscope_constructors_file, workletglobalscope_constructors_file, paintworkletglobalscope_constructors_file, testglobalscope_constructors_file):
        idl_files_list = tempfile.mkstemp()
        for input_file in os.listdir(input_directory):
            (name, extension) = os.path.splitext(input_file)
            if extension != '.idl':
                continue
            if name.endswith('Constructors'):
                continue
            os.write(idl_files_list[0], string_utils.encode(os.path.join(input_directory, input_file) + "\n"))
        os.close(idl_files_list[0])

        cmd = ['perl', '-w',
               '-IWebCore/bindings/scripts',
               'WebCore/bindings/scripts/preprocess-idls.pl',
               '--idlFilesList', idl_files_list[1],
               '--testGlobalContextName', 'TestGlobalObject',
               '--defines', '',
               '--supplementalDependencyFile', supplemental_dependency_file,
               '--supplementalMakefileDeps', supplemental_makefile_dependency_file,
               '--windowConstructorsFile', window_constructors_file,
               '--workerGlobalScopeConstructorsFile', workerglobalscope_constructors_file,
               '--dedicatedWorkerGlobalScopeConstructorsFile', dedicatedworkerglobalscope_constructors_file,
               '--serviceWorkerGlobalScopeConstructorsFile', serviceworkerglobalscope_constructors_file,
               '--workletGlobalScopeConstructorsFile', workletglobalscope_constructors_file,
               '--paintWorkletGlobalScopeConstructorsFile', paintworkletglobalscope_constructors_file,
               '--testGlobalScopeConstructorsFile', testglobalscope_constructors_file]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
            if output:
                print(output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
        os.remove(idl_files_list[1])
        return exit_code

    def detect_changes(self, generator, work_directory, reference_directory):
        changes_found = False
        for filename in os.listdir(work_directory):
            if self.detect_file_changes(generator, work_directory, reference_directory, filename):
                changes_found = True
        return changes_found

    def detect_file_changes(self, generator, work_directory, reference_directory, filename):
        changes_found = False
        cmd = ['diff',
               '-u',
               '-N',
               os.path.join(reference_directory, filename),
               os.path.join(work_directory, filename)]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
        except ScriptError as e:
            output = e.output
            exit_code = e.exit_code

        if exit_code or output:
            print('FAIL: (%s) %s' % (generator, filename))
            print(output)
            changes_found = True
            if self.json_file_name:
                self.failures.append("(%s) %s" % (generator, filename))
        elif self.verbose:
            print('PASS: (%s) %s' % (generator, filename))
        sys.stdout.flush()
        return changes_found

    def test_matches_patterns(self, test):
        if not self.patterns:
            return True
        for pattern in self.patterns:
            if fnmatch.fnmatch(test, pattern):
                return True
        return False

    def run_tests(self, generator, input_directory, reference_directory, supplemental_dependency_file):
        work_directory = reference_directory

        passed = True
        for input_file in os.listdir(input_directory):
            (name, extension) = os.path.splitext(input_file)
            if extension != '.idl':
                continue

            if not self.test_matches_patterns(input_file):
                continue

            # Generate output into the work directory (either the given one or a
            # temp one if not reset_results is performed)
            if not self.reset_results:
                work_directory = tempfile.mkdtemp()

            if self.generate_from_idl(generator,
                                      os.path.join(input_directory, input_file),
                                      work_directory,
                                      supplemental_dependency_file):
                passed = False

            if self.reset_results:
                print("Reset results: (%s) %s" % (generator, input_file))
                continue

            # Detect changes
            if self.detect_changes(generator, work_directory, reference_directory):
                passed = False
            shutil.rmtree(work_directory)

        return passed

    def main(self):
        current_scm = detect_scm_system(os.path.dirname(__file__))
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        all_tests_passed = True

        supplemental_dependency_filename = 'SupplementalDependencies.txt'
        supplemental_makefile_dependency_filename = 'SupplementalDependencies.dep'
        dom_window_constructors_filename = 'DOMWindowConstructors.idl'
        workerglobalscope_constructors_filename = 'WorkerGlobalScopeConstructors.idl'
        dedicatedworkerglobalscope_constructors_filename = 'DedicatedWorkerGlobalScopeConstructors.idl'
        serviceworkerglobalscope_constructors_filename = 'ServiceWorkerGlobalScopeConstructors.idl'
        workletglobalscope_constructors_filename = 'WorkletGlobalScopeConstructors.idl'
        paintworkletglobalscope_constructors_filename = 'PaintWorkletGlobalScopeConstructors.idl'
        testglobalscope_constructors_filename = 'BindingTestGlobalConstructors.idl'

        work_directory = tempfile.mkdtemp()
        input_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test')
        supplemental_dependency_file = os.path.join(work_directory, supplemental_dependency_filename)
        supplemental_makefile_dependency_file = os.path.join(work_directory if not self.reset_results else input_directory, supplemental_makefile_dependency_filename)
        window_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, dom_window_constructors_filename)
        workerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, workerglobalscope_constructors_filename)
        dedicatedworkerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, dedicatedworkerglobalscope_constructors_filename)
        serviceworkerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, serviceworkerglobalscope_constructors_filename)
        workletglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, workletglobalscope_constructors_filename)
        paintworkletglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, paintworkletglobalscope_constructors_filename)
        testglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, testglobalscope_constructors_filename)

        if self.generate_supplemental_dependency(input_directory, supplemental_dependency_file, supplemental_makefile_dependency_file, window_constructors_file, workerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, serviceworkerglobalscope_constructors_file, workletglobalscope_constructors_file, paintworkletglobalscope_constructors_file, testglobalscope_constructors_file):
            print('Failed to generate a supplemental dependency file.')
            shutil.rmtree(work_directory)
            return -1

        if not self.reset_results:
            if self.detect_file_changes('dependencies', work_directory, input_directory, supplemental_makefile_dependency_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, dom_window_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, workerglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, dedicatedworkerglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, serviceworkerglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, workletglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, paintworkletglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, testglobalscope_constructors_filename):
                all_tests_passed = False

        for generator in self.generators:
            input_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test')
            reference_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test', generator)
            if not self.run_tests(generator, input_directory, reference_directory, supplemental_dependency_file):
                all_tests_passed = False

        shutil.rmtree(work_directory)

        if self.json_file_name:
            json_data = {
                'failures': self.failures,
            }

            with open(self.json_file_name, 'w') as json_file:
                json.dump(json_data, json_file)

        print('')
        if all_tests_passed:
            print('All tests PASS!')
            return 0
        else:
            print('Some tests FAIL! (To update the reference files, execute "run-bindings-tests --reset-results")')
            return -1
