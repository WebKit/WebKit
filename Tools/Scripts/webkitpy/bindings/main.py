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
import time

from webkitcorepy import string_utils

from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError


class BindingsTests:
    generator_env = dict(os.environ,
                         ARCHS='arm64',
                         LLVM_TARGET_TRIPLE_VENDOR='apple',
                         LLVM_TARGET_TRIPLE_OS_VERSION='macos26.0')

    def __init__(self, reset_results, generators, executive, verbose, patterns, json_file_name):
        self.reset_results = reset_results
        self.generators = generators
        self.executive = executive
        self.verbose = verbose
        self.patterns = patterns
        self.json_file_name = json_file_name

        if self.json_file_name:
            self.failures = []

    def generate_from_idl(self, generator, idl_file, output_directory, supplemental_dependency_file, idl_files_list):
        cmd = ['perl', '-w',
               '-IWebCore/bindings/scripts',
               'WebCore/bindings/scripts/generate-bindings.pl',
               '--defines', 'TESTING_%s' % generator,
               '--generator', generator,
               '--outputDir', output_directory,
               '--supplementalDependencyFile', supplemental_dependency_file,
               '--idlAttributesFile', 'WebCore/bindings/scripts/IDLAttributes.json',
               '--idlFileNamesList', idl_files_list,
               idl_file]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd, env=self.generator_env)
            if output:
                print(output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
        return exit_code

    def generate_idl_file_names_list(self, input_directory, include_constructors=False):
        idl_files_list = tempfile.mkstemp()
        for dirpath, dirnames, filenames in os.walk(input_directory):
            for input_file in filenames:
                (name, extension) = os.path.splitext(input_file)
                if extension != '.idl':
                    continue
                if not include_constructors and name.endswith('Constructors'):
                    continue
                os.write(idl_files_list[0], string_utils.encode(os.path.join(dirpath, input_file) + "\n"))
        os.close(idl_files_list[0])
        return idl_files_list[1]

    def generate_supplemental_dependency(self, input_directory, supplemental_dependency_file, supplemental_makefile_dependency_file, window_constructors_file, workerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, serviceworkerglobalscope_constructors_file, sharedworkerglobalscope_constructors_file, workletglobalscope_constructors_file, paintworkletglobalscope_constructors_file, audioworkletglobalscope_constructors_file, testglobalscope_constructors_file):
        idl_files_list = self.generate_idl_file_names_list(input_directory)

        cmd = ['perl', '-w',
               '-IWebCore/bindings/scripts',
               'WebCore/bindings/scripts/preprocess-idls.pl',
               '--idlFileNamesList', idl_files_list,
               '--testGlobalContextName', 'TestGlobalObject',
               '--defines', '',
               '--idlAttributesFile', 'WebCore/bindings/scripts/IDLAttributes.json',
               '--supplementalDependencyFile', supplemental_dependency_file,
               '--supplementalMakefileDeps', supplemental_makefile_dependency_file,
               '--windowConstructorsFile', window_constructors_file,
               '--workerGlobalScopeConstructorsFile', workerglobalscope_constructors_file,
               '--dedicatedWorkerGlobalScopeConstructorsFile', dedicatedworkerglobalscope_constructors_file,
               '--serviceWorkerGlobalScopeConstructorsFile', serviceworkerglobalscope_constructors_file,
               '--sharedWorkerGlobalScopeConstructorsFile', sharedworkerglobalscope_constructors_file,
               '--workletGlobalScopeConstructorsFile', workletglobalscope_constructors_file,
               '--paintWorkletGlobalScopeConstructorsFile', paintworkletglobalscope_constructors_file,
               '--audioWorkletGlobalScopeConstructorsFile', audioworkletglobalscope_constructors_file,
               '--testGlobalScopeConstructorsFile', testglobalscope_constructors_file,
               '--validateAgainstParser']

        exit_code = 0
        try:
            output = self.executive.run_command(cmd, env=self.generator_env)
            if output:
                print(output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
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

    def run_tests(self, generator, input_directory, reference_directory, supplemental_dependency_file, idl_files_list):
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
                                      supplemental_dependency_file,
                                      idl_files_list):
                passed = False

            if self.reset_results:
                print("Reset results: (%s) %s" % (generator, input_file))
                continue

            # Detect changes
            if self.detect_changes(generator, work_directory, reference_directory):
                passed = False
            shutil.rmtree(work_directory)

        return passed

    def run_dependency_tests(self):
        work_directory = tempfile.mkdtemp()
        idl_directory = os.path.join(work_directory, 'idl')
        output_directory = os.path.join(work_directory, 'output')
        os.mkdir(idl_directory)
        os.mkdir(output_directory)
        idl_files_list = os.path.join(work_directory, 'IDLFileNamesList.txt')
        unmodified_time = time.time() - 1000

        def write_idl(name, extended_attributes, parent=None, members=''):
            inheritance = ' : %s' % parent if parent else ''
            with open(os.path.join(idl_directory, name + '.idl'), 'w') as idl_file:
                idl_file.write('[\n    %s,\n] interface %s%s {\n%s};\n' % (',\n    '.join(extended_attributes), name, inheritance, members))

        def bindings_contain(name, text):
            with open(os.path.join(output_directory, 'JS%s.cpp' % name)) as bindings_file:
                return text in bindings_file.read()

        def modification_times():
            return {name: os.stat(os.path.join(output_directory, name)).st_mtime_ns for name in os.listdir(output_directory)}

        def generate(modified_names):
            idl_files = sorted(os.path.join(idl_directory, name) for name in os.listdir(idl_directory))
            with open(idl_files_list, 'w') as list_file:
                list_file.write(''.join(idl_file + '\n' for idl_file in idl_files))
            for idl_file in idl_files:
                modification_time = time.time() + 1000 if os.path.splitext(os.path.basename(idl_file))[0] in modified_names else unmodified_time
                os.utime(idl_file, (modification_time, modification_time))

            before = modification_times()
            cmd = ['perl',
                   'WebCore/bindings/scripts/generate-bindings-all.pl',
                   '--outputDir', output_directory,
                   '--idlFilesList', idl_files_list,
                   '--idlFileNamesList', idl_files_list,
                   '--idlAttributesFile', 'WebCore/bindings/scripts/IDLAttributes.json',
                   '--defines', '',
                   '--generator', 'JS',
                   '--generatorDependency', 'WebCore/bindings/scripts/CodeGenerator.pm',
                   '--generatorDependency', 'WebCore/bindings/scripts/CodeGeneratorJS.pm']
            try:
                self.executive.run_command(cmd, env=self.generator_env)
            except ScriptError as e:
                print(e.output)
                return None
            after = modification_times()
            return {os.path.splitext(name)[0][len('JS'):] for name in after if name.endswith('.cpp') and before.get(name) != after[name]}

        passed = True

        def check(description, condition):
            nonlocal passed
            if not condition:
                print('FAIL: (incremental) %s' % description)
                passed = False
                if self.json_file_name:
                    self.failures.append('(incremental) %s' % description)
            elif self.verbose:
                print('PASS: (incremental) %s' % description)
            sys.stdout.flush()

        exposed = 'Exposed=TestGlobalObject'
        write_idl('TestParent', [exposed])
        write_idl('TestCustomParent', ['CustomToJSObject', exposed])
        check('Generating bindings for the first time', generate({'TestParent', 'TestCustomParent'}) == {'TestParent', 'TestCustomParent'})

        write_idl('TestChild', [exposed], parent='TestParent')
        write_idl('TestCustomChild', [exposed], parent='TestCustomParent')
        regenerated = generate({'TestChild', 'TestCustomChild'}) or set()
        check('Adding a child interface regenerates its parent', 'TestParent' in regenerated)
        check('The regenerated parent converts to the added child interface', bindings_contain('TestParent', 'is<TestChild>'))
        check("Adding a child interface doesn't regenerate a [CustomToJSObject] parent", 'TestCustomParent' not in regenerated)

        write_idl('TestChild', [exposed], parent='TestParent', members='    readonly attribute long value;\n')
        write_idl('TestCustomChild', [exposed], parent='TestCustomParent', members='    readonly attribute long value;\n')
        regenerated = generate({'TestChild', 'TestCustomChild'}) or set()
        check('Changing a child interface regenerates its parent', 'TestParent' in regenerated)
        check("Changing a child interface doesn't regenerate a [CustomToJSObject] parent", 'TestCustomParent' not in regenerated)

        os.remove(os.path.join(idl_directory, 'TestChild.idl'))
        regenerated = generate(set()) or set()
        check('Removing a child interface regenerates its parent', 'TestParent' in regenerated)
        check("The regenerated parent doesn't convert to the removed child interface", not bindings_contain('TestParent', 'is<TestChild>'))

        check('Unchanged interfaces are not regenerated', generate(set()) == set())

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
        sharedworkerglobalscope_constructors_filename = 'SharedWorkerGlobalScopeConstructors.idl'
        workletglobalscope_constructors_filename = 'WorkletGlobalScopeConstructors.idl'
        paintworkletglobalscope_constructors_filename = 'PaintWorkletGlobalScopeConstructors.idl'
        audioworkletglobalscope_constructors_filename = 'AudioWorkletGlobalScopeConstructors.idl'
        testglobalscope_constructors_filename = 'BindingTestGlobalConstructors.idl'

        work_directory = tempfile.mkdtemp()
        input_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test')
        supplemental_dependency_file = os.path.join(work_directory, supplemental_dependency_filename)
        supplemental_makefile_dependency_file = os.path.join(work_directory if not self.reset_results else input_directory, supplemental_makefile_dependency_filename)
        window_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, dom_window_constructors_filename)
        workerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, workerglobalscope_constructors_filename)
        dedicatedworkerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, dedicatedworkerglobalscope_constructors_filename)
        serviceworkerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, serviceworkerglobalscope_constructors_filename)
        sharedworkerglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, sharedworkerglobalscope_constructors_filename)
        workletglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, workletglobalscope_constructors_filename)
        paintworkletglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, paintworkletglobalscope_constructors_filename)
        audioworkletglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, audioworkletglobalscope_constructors_filename)
        testglobalscope_constructors_file = os.path.join(work_directory if not self.reset_results else input_directory, testglobalscope_constructors_filename)

        if self.generate_supplemental_dependency(input_directory, supplemental_dependency_file, supplemental_makefile_dependency_file, window_constructors_file, workerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, serviceworkerglobalscope_constructors_file, sharedworkerglobalscope_constructors_file, workletglobalscope_constructors_file, paintworkletglobalscope_constructors_file, audioworkletglobalscope_constructors_file, testglobalscope_constructors_file):
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
            if self.detect_file_changes('globalscope', work_directory, input_directory, sharedworkerglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, workletglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, paintworkletglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, audioworkletglobalscope_constructors_filename):
                all_tests_passed = False
            if self.detect_file_changes('globalscope', work_directory, input_directory, testglobalscope_constructors_filename):
                all_tests_passed = False

        idl_files_list = self.generate_idl_file_names_list(os.path.join('WebCore'), include_constructors=True)

        for generator in self.generators:
            input_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test')
            reference_directory = os.path.join('WebCore', 'bindings', 'scripts', 'test', generator)
            if not self.run_tests(generator, input_directory, reference_directory, supplemental_dependency_file, idl_files_list):
                all_tests_passed = False

        if not self.reset_results and self.test_matches_patterns('generate-bindings-all.pl'):
            if not self.run_dependency_tests():
                all_tests_passed = False

        shutil.rmtree(work_directory)
        os.remove(idl_files_list)

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
