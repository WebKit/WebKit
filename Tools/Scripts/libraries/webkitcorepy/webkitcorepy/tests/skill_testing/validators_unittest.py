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
from webkitcorepy.skill_testing import SkillFile, SkillValidator, DirectoryValidator, ValidationResult


class ValidationResultTest(unittest.TestCase):
    def test_pass_is_truthy(self):
        r = ValidationResult(True, 'test.rule', 'passed')
        self.assertTrue(r)

    def test_fail_is_falsy(self):
        r = ValidationResult(False, 'test.rule', 'failed')
        self.assertFalse(r)

    def test_repr_pass(self):
        r = ValidationResult(True, 'test.rule', 'all good')
        self.assertIn('PASS', repr(r))

    def test_repr_fail_error(self):
        r = ValidationResult(False, 'test.rule', 'bad', severity='error')
        self.assertIn('ERROR', repr(r))

    def test_repr_fail_warning(self):
        r = ValidationResult(False, 'test.rule', 'meh', severity='warning')
        self.assertIn('WARNING', repr(r))


class ValidateFrontmatterPresenceTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_with_frontmatter(self):
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n')
        results = SkillValidator.validate_frontmatter_presence(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_without_frontmatter(self):
        skill = self._make_skill('# Just markdown\n')
        results = SkillValidator.validate_frontmatter_presence(skill)
        self.assertTrue(any(not r.passed for r in results))


class ValidateRequiredFieldsTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_all_present(self):
        skill = self._make_skill('---\nname: test\ndescription: A test\n---\n')
        results = SkillValidator.validate_required_fields(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_missing_name(self):
        skill = self._make_skill('---\ndescription: A test\n---\n')
        results = SkillValidator.validate_required_fields(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('name', failed[0].message)

    def test_missing_description(self):
        skill = self._make_skill('---\nname: test\n---\n')
        results = SkillValidator.validate_required_fields(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('description', failed[0].message)

    def test_no_frontmatter(self):
        skill = self._make_skill('# Just markdown\n')
        results = SkillValidator.validate_required_fields(skill)
        self.assertTrue(any(not r.passed for r in results))


class ValidateFieldValuesTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_valid_model(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nmodel: opus\n---\n')
        results = SkillValidator.validate_field_values(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_invalid_model(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nmodel: gpt4\n---\n')
        results = SkillValidator.validate_field_values(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('gpt4', failed[0].message)

    def test_valid_effort(self):
        skill = self._make_skill('---\nname: x\ndescription: y\neffort: max\n---\n')
        results = SkillValidator.validate_field_values(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_invalid_effort(self):
        skill = self._make_skill('---\nname: x\ndescription: y\neffort: turbo\n---\n')
        results = SkillValidator.validate_field_values(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('turbo', failed[0].message)

    def test_boolean_field_valid(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nuser-invocable: true\n---\n')
        results = SkillValidator.validate_field_values(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_boolean_field_invalid(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nuser-invocable: "yes"\n---\n')
        results = SkillValidator.validate_field_values(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('boolean', failed[0].message)

    def test_no_optional_fields(self):
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n')
        results = SkillValidator.validate_field_values(skill)
        self.assertEqual(len(results), 0)


class ValidateUnknownKeysTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_known_keys_only(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nmodel: opus\n---\n')
        results = SkillValidator.validate_unknown_keys(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_unknown_key(self):
        skill = self._make_skill('---\nname: x\ndescription: y\ntypo-field: bad\n---\n')
        results = SkillValidator.validate_unknown_keys(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('typo-field', failed[0].message)
        self.assertEqual(failed[0].severity, 'warning')


class ValidateAllowedToolsFormatTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_valid_tools(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nallowed-tools: Bash(git log:*), Read, Grep(src/**)\n---\n')
        results = SkillValidator.validate_allowed_tools_format(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_tool_name_only(self):
        skill = self._make_skill('---\nname: x\ndescription: y\nallowed-tools: Read\n---\n')
        results = SkillValidator.validate_allowed_tools_format(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_no_tools(self):
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n')
        results = SkillValidator.validate_allowed_tools_format(skill)
        self.assertEqual(len(results), 0)


class ValidateNameMatchesDirectoryTest(testing.PathTestCase):
    basepath = 'mock/skills/my-skill'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_name_matches(self):
        skill = self._make_skill('---\nname: my-skill\ndescription: test\n---\n')
        results = SkillValidator.validate_name_matches_directory(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_name_mismatch(self):
        skill = self._make_skill('---\nname: other-name\ndescription: test\n---\n')
        results = SkillValidator.validate_name_matches_directory(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertEqual(failed[0].severity, 'warning')


class ValidateReferencesExistTest(testing.PathTestCase):
    basepath = 'mock/skills/ref-skill'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_existing_reference(self):
        with open(os.path.join(self.path, 'config.json'), 'w') as f:
            f.write('{}')
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n\nSee [cfg](config.json).\n')
        results = SkillValidator.validate_references_exist(skill)
        self.assertTrue(all(r.passed for r in results))

    def test_missing_reference(self):
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n\nSee [cfg](missing.json).\n')
        results = SkillValidator.validate_references_exist(skill)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('missing.json', failed[0].message)

    def test_no_references(self):
        skill = self._make_skill('---\nname: x\ndescription: y\n---\n\nNo links here.\n')
        results = SkillValidator.validate_references_exist(skill)
        self.assertEqual(len(results), 0)


class ValidateAllTest(testing.PathTestCase):
    basepath = 'mock/skills/all-test'

    def _make_skill(self, content):
        path = os.path.join(self.path, 'SKILL.md')
        with open(path, 'w') as f:
            f.write(content)
        return SkillFile(path)

    def test_valid_skill(self):
        skill = self._make_skill(
            '---\n'
            'name: all-test\n'
            'description: A fully valid skill\n'
            'model: opus\n'
            'effort: max\n'
            'user-invocable: true\n'
            '---\n'
            '\n'
            '# Instructions\n'
        )
        results = SkillValidator.validate_all(skill)
        errors = [r for r in results if not r.passed and r.severity == 'error']
        self.assertEqual(len(errors), 0)

    def test_invalid_skill(self):
        skill = self._make_skill('# No frontmatter at all\n')
        results = SkillValidator.validate_all(skill)
        errors = [r for r in results if not r.passed and r.severity == 'error']
        self.assertGreater(len(errors), 0)


class ValidateSettingsJsonTest(testing.PathTestCase):
    basepath = 'mock/claude'

    def test_valid_settings(self):
        with open(os.path.join(self.path, 'settings.json'), 'w') as f:
            f.write('{"permissions": {"allow": ["Read"]}}')
        results = DirectoryValidator.validate_settings_json(self.path)
        self.assertTrue(all(r.passed for r in results))

    def test_invalid_settings(self):
        with open(os.path.join(self.path, 'settings.json'), 'w') as f:
            f.write('{bad json}')
        results = DirectoryValidator.validate_settings_json(self.path)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('not valid JSON', failed[0].message)

    def test_no_settings(self):
        results = DirectoryValidator.validate_settings_json(self.path)
        self.assertTrue(all(r.passed for r in results))


class ValidateMarketplaceJsonTest(testing.PathTestCase):
    basepath = 'mock/claude'

    def test_valid_marketplace(self):
        plugin_dir = os.path.join(self.path, 'plugins', '.claude-plugin')
        os.makedirs(plugin_dir)
        with open(os.path.join(plugin_dir, 'marketplace.json'), 'w') as f:
            f.write('{"name": "test", "plugins": []}')
        results = DirectoryValidator.validate_marketplace_json(self.path)
        self.assertTrue(all(r.passed for r in results))

    def test_invalid_marketplace(self):
        plugin_dir = os.path.join(self.path, 'plugins', '.claude-plugin')
        os.makedirs(plugin_dir)
        with open(os.path.join(plugin_dir, 'marketplace.json'), 'w') as f:
            f.write('{not valid}')
        results = DirectoryValidator.validate_marketplace_json(self.path)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('not valid JSON', failed[0].message)

    def test_missing_marketplace(self):
        os.makedirs(os.path.join(self.path, 'plugins'))
        results = DirectoryValidator.validate_marketplace_json(self.path)
        failed = [r for r in results if not r.passed]
        self.assertEqual(len(failed), 1)
        self.assertIn('missing', failed[0].message)

    def test_no_plugins_dir(self):
        results = DirectoryValidator.validate_marketplace_json(self.path)
        self.assertEqual(len(results), 0)
