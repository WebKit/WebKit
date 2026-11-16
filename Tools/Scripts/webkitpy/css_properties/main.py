# Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

PROCESS_CSS_PROPERTIES_PY_FILENAME = "process-css-properties.py"
TEST_CSS_PROPERTIES_JSON_FILENAME = 'TestCSSProperties.json'


class CSSPropertyCodeGenerationTests:

    def __init__(self, reset_results, executive, verbose, patterns, json_file_name):
        self.reset_results = reset_results
        self.executive = executive
        self.verbose = verbose
        self.patterns = patterns
        self.json_file_name = json_file_name

        if self.json_file_name:
            self.failures = []

    def process_css_properties(self, script, properties_json, defines="ENABLE_TEST_DEFINE ENABLE_OTHER_TEST_DEFINE"):
        cmd = [sys.executable,
               script,
               '--properties', properties_json,
               '--defines', defines
           ]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
            if output:
                print(output)
        except ScriptError as e:
            print(e.output)
            exit_code = e.exit_code
        return exit_code

    def detect_changes(self, work_directory, reference_directory):
        changes_found = False
        for filename in sorted(os.listdir(work_directory)):
            if self.detect_file_changes(work_directory, reference_directory, filename):
                changes_found = True
        return changes_found

    def detect_file_changes(self, work_directory, reference_directory, filename):
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
            print('FAIL: %s' % (filename))
            print(output)
            changes_found = True
            if self.json_file_name:
                self.failures.append('%s' % (filename))
        elif self.verbose:
            print('PASS: %s' % (filename))
        sys.stdout.flush()
        return changes_found

    def test_matches_patterns(self, test):
        if not self.patterns:
            return True
        for pattern in self.patterns:
            if fnmatch.fnmatch(test, pattern):
                return True
        return False

    def main(self):
        current_scm = detect_scm_system(os.path.dirname(__file__))
        source_directory = os.path.join(current_scm.checkout_root, 'Source')

        input_directory = os.path.join(source_directory, 'WebCore', 'css', 'scripts')
        reference_directory = os.path.join(source_directory, 'WebCore', 'css', 'scripts', 'test', 'TestCSSPropertiesResults')

        if self.reset_results:
            work_directory = reference_directory
        else:
            work_directory = tempfile.mkdtemp("css-property-code-generation-tests")

        os.chdir(work_directory)

        process_css_properties_py_file = os.path.join(input_directory, PROCESS_CSS_PROPERTIES_PY_FILENAME)
        test_css_properties_json_file = os.path.join(input_directory, 'test', TEST_CSS_PROPERTIES_JSON_FILENAME)

        if self.process_css_properties(process_css_properties_py_file, test_css_properties_json_file):
            return False

        if self.reset_results:
            print("Reset results: %s." % (reference_directory))
            return 0

        all_tests_passed = not self.detect_changes(work_directory, reference_directory)

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
            print('Some tests FAIL! (To update the reference files, execute "run-css-property-code-generation-tests --reset-results")')
            return -1
