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
from webkitcorepy.skill_testing import SkillFile


class SkillFileParseTest(testing.PathTestCase):
    basepath = 'mock/skills'

    def _write_skill(self, content, name='SKILL.md'):
        path = os.path.join(self.path, name)
        with open(path, 'w') as f:
            f.write(content)
        return path

    def test_basic_frontmatter(self):
        path = self._write_skill(
            '---\n'
            'name: test-skill\n'
            'description: A test skill\n'
            '---\n'
            '\n'
            '# Body\n'
            'Some content\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.name, 'test-skill')
        self.assertEqual(skill.description, 'A test skill')
        self.assertIn('# Body', skill.body)

    def test_no_frontmatter(self):
        path = self._write_skill('# Just markdown\n\nNo frontmatter here.\n')
        skill = SkillFile(path)
        self.assertIsNone(skill.frontmatter)
        self.assertIsNone(skill.name)
        self.assertIn('Just markdown', skill.body)

    def test_allowed_tools_string(self):
        path = self._write_skill(
            '---\n'
            'name: tools-test\n'
            'description: test\n'
            'allowed-tools: Bash(git log:*), Read, Grep(src/**)\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.allowed_tools, ['Bash(git log:*)', 'Read', 'Grep(src/**)'])

    def test_allowed_tools_list(self):
        path = self._write_skill(
            '---\n'
            'name: tools-test\n'
            'description: test\n'
            'allowedTools:\n'
            '  - Bash(make *)\n'
            '  - Read\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.allowed_tools, ['Bash(make *)', 'Read'])

    def test_no_allowed_tools(self):
        path = self._write_skill(
            '---\n'
            'name: no-tools\n'
            'description: test\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.allowed_tools, [])

    def test_boolean_fields(self):
        path = self._write_skill(
            '---\n'
            'name: bool-test\n'
            'description: test\n'
            'user-invocable: true\n'
            'disable-model-invocation: false\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertTrue(skill.user_invocable)

    def test_model_and_effort(self):
        path = self._write_skill(
            '---\n'
            'name: model-test\n'
            'description: test\n'
            'model: opus\n'
            'effort: max\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.model, 'opus')
        self.assertEqual(skill.effort, 'max')

    def test_sections(self):
        path = self._write_skill(
            '---\n'
            'name: sections-test\n'
            'description: test\n'
            '---\n'
            '\n'
            '# Top heading\n'
            '\n'
            '## Section One\n'
            '\n'
            'Content\n'
            '\n'
            '## Section Two\n'
            '\n'
            '### Subsection\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.sections, ['Top heading', 'Section One', 'Section Two', 'Subsection'])

    def test_references(self):
        path = self._write_skill(
            '---\n'
            'name: refs-test\n'
            'description: test\n'
            '---\n'
            '\n'
            'See [config](../config.json) and [docs](https://example.com).\n'
            'Also [anchor](#section).\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.references, ['../config.json'])

    def test_skill_dir(self):
        path = self._write_skill(
            '---\n'
            'name: dir-test\n'
            'description: test\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertEqual(skill.skill_dir, self.path)

    def test_unclosed_frontmatter(self):
        path = self._write_skill(
            '---\n'
            'name: unclosed\n'
            'description: test\n'
            '\n'
            '# Body starts here\n'
        )
        skill = SkillFile(path)
        self.assertIsNone(skill.frontmatter)
        self.assertIn('---', skill.body)

    def test_repr(self):
        path = self._write_skill(
            '---\n'
            'name: repr-test\n'
            'description: test\n'
            '---\n'
        )
        skill = SkillFile(path)
        self.assertIn('SKILL.md', repr(skill))


class SkillFileDiscoverTest(testing.PathTestCase):
    basepath = 'mock/repo'

    def test_discover_finds_skills(self):
        claude_dir = os.path.join(self.path, '.claude')
        skill_dir = os.path.join(claude_dir, 'skills', 'my-skill')
        os.makedirs(skill_dir)
        with open(os.path.join(skill_dir, 'SKILL.md'), 'w') as f:
            f.write('---\nname: my-skill\ndescription: test\n---\n')

        skills = SkillFile.discover(claude_dir)
        self.assertEqual(len(skills), 1)
        self.assertEqual(skills[0].name, 'my-skill')

    def test_discover_finds_plugin_skills(self):
        claude_dir = os.path.join(self.path, '.claude')
        skill_dir = os.path.join(claude_dir, 'plugins', 'my-plugin', 'skills', 'my-skill')
        os.makedirs(skill_dir)
        with open(os.path.join(skill_dir, 'SKILL.md'), 'w') as f:
            f.write('---\nname: my-skill\ndescription: test\n---\n')

        skills = SkillFile.discover(claude_dir)
        self.assertEqual(len(skills), 1)
        self.assertEqual(skills[0].name, 'my-skill')

    def test_discover_empty_repo(self):
        claude_dir = os.path.join(self.path, '.claude')
        os.makedirs(claude_dir)
        skills = SkillFile.discover(claude_dir)
        self.assertEqual(len(skills), 0)

    def test_discover_multiple_skills(self):
        claude_dir = os.path.join(self.path, '.claude')
        for name in ('alpha', 'beta'):
            skill_dir = os.path.join(claude_dir, 'skills', name)
            os.makedirs(skill_dir)
            with open(os.path.join(skill_dir, 'SKILL.md'), 'w') as f:
                f.write('---\nname: {}\ndescription: test\n---\n'.format(name))

        skills = SkillFile.discover(claude_dir)
        self.assertEqual(len(skills), 2)
        self.assertEqual(skills[0].name, 'alpha')
        self.assertEqual(skills[1].name, 'beta')
