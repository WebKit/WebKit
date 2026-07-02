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

import json
import logging
import os
import subprocess

from webkitcorepy import AutoInstall

log = logging.getLogger('skill_testing')


class SkillTest(object):
    @classmethod
    def discover(cls, skill):
        tests_dir = os.path.join(skill.skill_dir, 'tests')
        if not os.path.isdir(tests_dir):
            return []
        results = []
        for filename in sorted(os.listdir(tests_dir)):
            if filename.endswith(('.yaml', '.yml')):
                results.append(cls(os.path.join(tests_dir, filename)))
        return results

    def __init__(self, path):
        self.path = os.path.abspath(path)

        AutoInstall.install('pyyaml')
        import yaml

        with open(self.path, 'r') as f:
            data = yaml.safe_load(f)

        self.name = data.get('name')

        test_section = data.get('test', {})
        self.test_prompt = test_section.get('prompt')
        self.test_args = test_section.get('args', [])

        validation_section = data.get('validation', {})
        self.validation_prompt = validation_section.get('prompt')
        self.validation_args = validation_section.get('args', [])
        self.validation_command = validation_section.get('command')
        self.validation_expectation = validation_section.get('expectation')

        self.files = data.get('files', [])

    def _invoke_claude(self, prompt, args, cwd):
        cmd = ['claude'] + args + ['-p', prompt]
        log.debug('\nRunning: %s', ' '.join(cmd))
        return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)

    def _cleanup_files(self, cwd):
        for filename in self.files:
            filepath = os.path.join(cwd, filename)
            try:
                if os.path.isfile(filepath):
                    os.remove(filepath)
                elif os.path.isdir(filepath):
                    import shutil
                    shutil.rmtree(filepath)
            except OSError:
                pass

    def _validate_with_command(self, cwd):
        log.debug('\nRunning: %s', ' '.join(self.validation_command))
        result = subprocess.run(self.validation_command, cwd=cwd, capture_output=True, text=True)
        if result.returncode != 0:
            return False, 'Command {} exited with code {}: {}'.format(
                self.validation_command, result.returncode, result.stderr.strip())
        if self.validation_expectation is not None:
            stdout = result.stdout.strip()
            if self.validation_expectation not in stdout:
                return False, 'Expected \'{}\' in output, got: {}'.format(self.validation_expectation, stdout)
        return True, 'Command succeeded'

    def _validate_with_claude(self, cwd, dump_path):
        assess_prompt = (
            "You are assessing the output of a skill. "
            "Look at '{}'. "
            "Does this match the expectation '{}'. "
            "Answer json in the form of "
            "{{\"status\": \"passed\", \"reason\": <rationale>}} or "
            "{{\"status\": \"failed\", \"reason\": <rationale>}}"
        ).format(dump_path, self.validation_prompt)

        assess_result = self._invoke_claude(assess_prompt, self.validation_args, cwd)

        response = assess_result.stdout.strip()
        start = response.find('{')
        end = response.rfind('}')
        if start == -1 or end == -1:
            return False, 'Assessment response is not valid JSON: {}'.format(response)

        try:
            verdict = json.loads(response[start:end + 1])
        except (json.JSONDecodeError, ValueError) as e:
            return False, 'Failed to parse assessment JSON: {}'.format(e)

        status = verdict.get('status', '').lower()
        reason = verdict.get('reason', '')

        if status == 'passed':
            return True, reason
        return False, reason

    def run(self, cwd):
        dump_path = os.path.join(cwd, '.llm_test_dump.txt')
        try:
            test_result = self._invoke_claude(self.test_prompt, self.test_args, cwd)

            with open(dump_path, 'w') as f:
                f.write(test_result.stdout)

            if self.validation_command:
                return self._validate_with_command(cwd)
            return self._validate_with_claude(cwd, dump_path)
        finally:
            try:
                os.remove(dump_path)
            except OSError:
                pass
            self._cleanup_files(cwd)

    def __repr__(self):
        return 'SkillTest({!r})'.format(self.path)
