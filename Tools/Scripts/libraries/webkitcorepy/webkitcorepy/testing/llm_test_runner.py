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
import shutil
import unittest

from webkitcorepy.skill_testing import SkillFile, SkillTest, SkillValidator, DirectoryValidator
from webkitcorepy.skill_testing.skill_test import log as skill_test_log
from webkitcorepy.testing.test_runner import TestRunner


class LLMTestRunner(TestRunner):
    VALIDATORS = [
        ('validate_frontmatter_presence', SkillValidator.validate_frontmatter_presence),
        ('validate_required_fields', SkillValidator.validate_required_fields),
        ('validate_field_values', SkillValidator.validate_field_values),
        ('validate_unknown_keys', SkillValidator.validate_unknown_keys),
        ('validate_allowed_tools_format', SkillValidator.validate_allowed_tools_format),
        ('validate_name_matches_directory', SkillValidator.validate_name_matches_directory),
        ('validate_references_exist', SkillValidator.validate_references_exist),
    ]

    DIRECTORY_VALIDATORS = [
        ('validate_settings_json', DirectoryValidator.validate_settings_json),
        ('validate_marketplace_json', DirectoryValidator.validate_marketplace_json),
    ]

    def __init__(self, description, claude_dir=None, name=None, directories=None, loggers=None):
        loggers = list(loggers or [logging.getLogger()])
        if skill_test_log not in loggers:
            loggers.append(skill_test_log)
        super(LLMTestRunner, self).__init__(description, loggers=loggers)

        self.parser.add_argument(
            '-f', '--fast',
            help='Skip functional tests that invoke claude',
            action='store_true',
            default=False,
        )

        if directories:
            self._directories = directories
        else:
            self._directories = [(name, claude_dir)]
        self._tests = {}
        self._functional_tests = set()
        self._discover()

    @staticmethod
    def _skill_relative_name(skill, claude_dir):
        rel = os.path.relpath(skill.skill_dir, claude_dir)
        parts = rel.split(os.sep)
        if skill.name:
            parts[-1] = skill.name
        return '.'.join(parts)

    @staticmethod
    def _make_validator_method(validator_func, skill):
        def test_method(self):
            results = validator_func(skill)
            errors = [r for r in results if not r.passed and r.severity == 'error']
            warnings = [r for r in results if not r.passed and r.severity == 'warning']
            for w in warnings:
                self.warnings.append(str(w))
            if errors:
                self.fail('\n'.join(str(e) for e in errors))
        return test_method

    @classmethod
    def _make_skill_test_cases(cls, skills, claude_dir, name=None):
        prefix = '{}.llm_testing'.format(name) if name else 'llm_testing'
        test_cases = {}
        for skill in skills:
            rel_name = cls._skill_relative_name(skill, claude_dir)
            safe_name = rel_name.replace('-', '_')

            attrs = {'skill': skill, 'warnings': [], '__module__': prefix}
            for validator_name, validator_func in cls.VALIDATORS:
                attrs['test_{}'.format(validator_name)] = cls._make_validator_method(validator_func, skill)

            test_cases[safe_name] = type(safe_name, (unittest.TestCase,), attrs)

        return test_cases

    @classmethod
    def _make_directory_test_case(cls, claude_dir, name=None):
        prefix = '{}.llm_testing'.format(name) if name else 'llm_testing'

        attrs = {'warnings': [], '__module__': prefix}
        for validator_name, validator_func in cls.DIRECTORY_VALIDATORS:
            attrs['test_{}'.format(validator_name)] = cls._make_validator_method(validator_func, claude_dir)

        return type('directory', (unittest.TestCase,), attrs)

    @staticmethod
    def _get_installed_plugins(project_path):
        installed_path = os.path.join(os.path.expanduser('~'), '.claude', 'plugins', 'installed_plugins.json')
        if not os.path.exists(installed_path):
            return set()
        try:
            with open(installed_path, 'r') as f:
                data = json.load(f)
        except (json.JSONDecodeError, ValueError):
            return set()

        installed = set()
        for key, entries in data.get('plugins', {}).items():
            plugin_name = key.split('@')[0]
            for entry in entries:
                entry_path = entry.get('projectPath')
                if not entry_path:
                    continue
                if project_path == entry_path or project_path.startswith(entry_path + os.sep):
                    installed.add(plugin_name)
        return installed

    @staticmethod
    def _plugin_name_for_skill(skill, claude_dir):
        rel = os.path.relpath(skill.skill_dir, claude_dir)
        parts = rel.split(os.sep)
        if len(parts) >= 2 and parts[0] == 'plugins':
            return parts[1]
        return None

    @classmethod
    def _make_functional_test_cases(cls, skills, claude_dir, name=None):
        prefix = '{}.llm_testing'.format(name) if name else 'llm_testing'
        cwd = os.path.dirname(claude_dir)
        installed_plugins = cls._get_installed_plugins(cwd)
        test_cases = {}

        for skill in skills:
            plugin = cls._plugin_name_for_skill(skill, claude_dir)
            if plugin and plugin not in installed_plugins:
                continue

            skill_tests = SkillTest.discover(skill)
            if not skill_tests:
                continue

            rel_name = cls._skill_relative_name(skill, claude_dir)
            safe_skill = rel_name.replace('-', '_')

            for skill_test in skill_tests:
                safe_test = skill_test.name.replace('-', '_') if skill_test.name else os.path.splitext(os.path.basename(skill_test.path))[0]

                def _make_functional_method(st, working_dir):
                    def test_method(self):
                        passed, reason = st.run(working_dir)
                        if not passed:
                            self.fail(reason)
                    return test_method

                attrs = {
                    'warnings': [],
                    '__module__': prefix,
                    'test_{}'.format(safe_test): _make_functional_method(skill_test, cwd),
                }
                test_cases['{}.{}'.format(safe_skill, safe_test)] = type(safe_skill, (unittest.TestCase,), attrs)

        return test_cases

    def _discover(self):
        for name, claude_dir in self._directories:
            skills = SkillFile.discover(claude_dir)

            for cls_name, cls in self._make_skill_test_cases(skills, claude_dir, name=name).items():
                for test in unittest.TestLoader().loadTestsFromTestCase(cls):
                    self._tests[test.id()] = test

            for test in unittest.TestLoader().loadTestsFromTestCase(self._make_directory_test_case(claude_dir, name=name)):
                self._tests[test.id()] = test

            for cls_name, cls in self._make_functional_test_cases(skills, claude_dir, name=name).items():
                for test in unittest.TestLoader().loadTestsFromTestCase(cls):
                    self._tests[test.id()] = test
                    self._functional_tests.add(test.id())

    def _skip_functional(self, args):
        if args and getattr(args, 'fast', False):
            return True
        if not shutil.which('claude'):
            return True
        return False

    def tests(self, args=None):
        filters = [] if not args or not args.tests else args.tests
        skip_functional = self._skip_functional(args)
        matched_filters = {pattern.pattern: 0 for pattern in filters}
        for name in sorted(self._tests.keys()):
            if skip_functional and name in self._functional_tests:
                continue
            if not filters:
                yield name
                continue
            for pattern in filters:
                if pattern.match(name):
                    yield name
                    matched_filters[pattern.pattern] += 1
                    break

    def run_test(self, test):
        result = unittest.TestResult()
        try:
            to_run = self._tests[test]
            to_run.run(result=result)
        except KeyError:
            result.errors.append((test, "No test named '{}'\n".format(test)))
        return result
