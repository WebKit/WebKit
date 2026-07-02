# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import unittest

from webkitcorepy import testing
from webkitcorepy.skill_testing import SkillFile, SkillTest


class SkillTestParseTest(testing.PathTestCase):
    basepath = 'mock/skills/my-skill/tests'

    def _write_test(self, content, name='basic.yaml'):
        path = os.path.join(self.path, name)
        with open(path, 'w') as f:
            f.write(content)
        return path

    def test_basic_parse(self):
        path = self._write_test(
            'name: basic-test\n'
            'test:\n'
            '  prompt: "Tell me about something"\n'
            'validation:\n'
            '  prompt: "Should mention something"\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.name, 'basic-test')
        self.assertEqual(st.test_prompt, 'Tell me about something')
        self.assertEqual(st.validation_prompt, 'Should mention something')
        self.assertIsNone(st.validation_command)
        self.assertIsNone(st.validation_expectation)
        self.assertEqual(st.test_args, [])
        self.assertEqual(st.validation_args, [])
        self.assertEqual(st.files, [])

    def test_with_args(self):
        path = self._write_test(
            'name: args-test\n'
            'test:\n'
            '  prompt: "Write a script"\n'
            '  args: ["--allowedTools", "Edit,Write"]\n'
            'validation:\n'
            '  prompt: "Run the script"\n'
            '  args: ["--allowedTools", "python3 script.py"]\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.test_args, ['--allowedTools', 'Edit,Write'])
        self.assertEqual(st.validation_args, ['--allowedTools', 'python3 script.py'])

    def test_with_files(self):
        path = self._write_test(
            'name: files-test\n'
            'test:\n'
            '  prompt: "Create output.txt"\n'
            'validation:\n'
            '  prompt: "Check output.txt"\n'
            'files: ["output.txt", "temp_dir"]\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.files, ['output.txt', 'temp_dir'])

    def test_command_validation(self):
        path = self._write_test(
            'name: cmd-test\n'
            'test:\n'
            '  prompt: "Write a script"\n'
            '  args: ["--allowedTools", "Edit,Write"]\n'
            'validation:\n'
            '  command: ["python3", "script.py"]\n'
            '  expectation: "hello"\n'
            'files: ["script.py"]\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.validation_command, ['python3', 'script.py'])
        self.assertEqual(st.validation_expectation, 'hello')
        self.assertIsNone(st.validation_prompt)

    def test_command_validation_no_expectation(self):
        path = self._write_test(
            'name: cmd-no-expect\n'
            'test:\n'
            '  prompt: "Do something"\n'
            'validation:\n'
            '  command: ["true"]\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.validation_command, ['true'])
        self.assertIsNone(st.validation_expectation)

    def test_no_args_defaults(self):
        path = self._write_test(
            'name: defaults-test\n'
            'test:\n'
            '  prompt: "Do something"\n'
            'validation:\n'
            '  prompt: "Check result"\n'
        )
        st = SkillTest(path)
        self.assertEqual(st.test_args, [])
        self.assertEqual(st.validation_args, [])
        self.assertEqual(st.files, [])

    def test_repr(self):
        path = self._write_test(
            'name: repr-test\n'
            'test:\n'
            '  prompt: "test"\n'
            'validation:\n'
            '  prompt: "check"\n'
        )
        st = SkillTest(path)
        self.assertIn('basic.yaml', repr(st))


class SkillTestDiscoverTest(testing.PathTestCase):
    basepath = 'mock/skills'

    BASIC_YAML = (
        'name: basic\n'
        'test:\n'
        '  prompt: "test"\n'
        'validation:\n'
        '  prompt: "result"\n'
    )

    def _make_skill_with_tests(self, skill_name, test_files):
        skill_dir = os.path.join(self.path, skill_name)
        os.makedirs(skill_dir)
        with open(os.path.join(skill_dir, 'SKILL.md'), 'w') as f:
            f.write('---\nname: {}\ndescription: test\n---\n'.format(skill_name))
        tests_dir = os.path.join(skill_dir, 'tests')
        os.makedirs(tests_dir)
        for filename, content in test_files.items():
            with open(os.path.join(tests_dir, filename), 'w') as f:
                f.write(content)
        return SkillFile(os.path.join(skill_dir, 'SKILL.md'))

    def test_discover_yaml_tests(self):
        advanced_yaml = self.BASIC_YAML.replace('basic', 'advanced')
        skill = self._make_skill_with_tests('my-skill', {
            'basic.yaml': self.BASIC_YAML,
            'advanced.yml': advanced_yaml,
        })
        tests = SkillTest.discover(skill)
        self.assertEqual(len(tests), 2)
        self.assertEqual(tests[0].name, 'advanced')
        self.assertEqual(tests[1].name, 'basic')

    def test_discover_ignores_non_yaml(self):
        skill = self._make_skill_with_tests('my-skill', {
            'basic.yaml': self.BASIC_YAML,
            'notes.txt': 'not a test\n',
        })
        tests = SkillTest.discover(skill)
        self.assertEqual(len(tests), 1)

    def test_discover_no_tests_dir(self):
        skill_dir = os.path.join(self.path, 'no-tests')
        os.makedirs(skill_dir)
        with open(os.path.join(skill_dir, 'SKILL.md'), 'w') as f:
            f.write('---\nname: no-tests\ndescription: test\n---\n')
        skill = SkillFile(os.path.join(skill_dir, 'SKILL.md'))
        tests = SkillTest.discover(skill)
        self.assertEqual(len(tests), 0)
