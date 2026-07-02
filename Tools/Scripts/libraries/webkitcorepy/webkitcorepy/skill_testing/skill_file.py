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
import re

from webkitcorepy import AutoInstall


class SkillFile(object):
    FRONTMATTER_DELIMITER = '---'

    @classmethod
    def discover(cls, claude_dir):
        results = []
        for dirpath, dirnames, filenames in os.walk(claude_dir, followlinks=True):
            dirnames[:] = [d for d in dirnames if d != '__pycache__']
            if 'SKILL.md' in filenames:
                results.append(cls(os.path.join(dirpath, 'SKILL.md')))
        return sorted(results, key=lambda s: s.path)

    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.skill_dir = os.path.dirname(self.path)

        frontmatter = None
        body = None

        with open(self.path, 'r') as f:
            content = f.read()

        lines = content.split('\n')
        if not lines or lines[0].strip() != self.FRONTMATTER_DELIMITER:
            body = content
        else:
            end = None
            for i in range(1, len(lines)):
                if lines[i].strip() == self.FRONTMATTER_DELIMITER:
                    end = i
                    break

            if end is None:
                body = content
            else:
                AutoInstall.install('pyyaml')
                import yaml
                try:
                    frontmatter = yaml.safe_load('\n'.join(lines[1:end])) or {}
                except yaml.YAMLError:
                    frontmatter = None
                    body = content

                if frontmatter is not None:
                    body = '\n'.join(lines[end + 1:])

        self.frontmatter = frontmatter
        self.body = body

        if frontmatter:
            self.name = frontmatter.get('name')
            self.description = frontmatter.get('description')
            self.user_invocable = frontmatter.get('user-invocable')
            self.model = frontmatter.get('model')
            self.effort = frontmatter.get('effort')

            value = frontmatter.get('allowed-tools') or frontmatter.get('allowedTools')
            if value is None:
                self.allowed_tools = []
            elif isinstance(value, list):
                self.allowed_tools = [str(t).strip() for t in value if str(t).strip()]
            else:
                self.allowed_tools = [t.strip() for t in str(value).split(',') if t.strip()]
        else:
            self.name = None
            self.description = None
            self.user_invocable = None
            self.model = None
            self.effort = None
            self.allowed_tools = []

        if body:
            self.sections = re.findall(r'^#{1,6}\s+(.+)$', body, re.MULTILINE)
            link_refs = re.findall(r'\[.*?\]\(([^)]+)\)', body)
            self.references = [ref for ref in link_refs if not ref.startswith(('http://', 'https://', '#'))]
        else:
            self.sections = []
            self.references = []

    def __repr__(self):
        return 'SkillFile({!r})'.format(self.path)
