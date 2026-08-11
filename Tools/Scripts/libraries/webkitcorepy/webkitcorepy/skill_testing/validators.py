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
import os
import re


class ValidationResult(object):
    def __init__(self, passed, rule, message, severity='error'):
        self.passed = passed
        self.rule = rule
        self.message = message
        self.severity = severity

    def __repr__(self):
        status = 'PASS' if self.passed else self.severity.upper()
        return '{}: [{}] {}'.format(status, self.rule, self.message)

    def __bool__(self):
        return self.passed


class SkillValidator(object):
    VALID_MODELS = {'opus', 'sonnet', 'haiku'}
    VALID_EFFORTS = {'max', 'high', 'medium', 'low'}
    KNOWN_FRONTMATTER_KEYS = {
        'name', 'description', 'allowed-tools', 'allowedTools',
        'user-invocable', 'model', 'effort',
        'disable-model-invocation', 'argument-hint',
    }
    TOOL_PATTERN = re.compile(
        r'^[A-Za-z_][A-Za-z0-9_]*'
        r'(?:__[A-Za-z0-9_*-]+)*'
        r'(?:\(.*\))?$'
    )

    @staticmethod
    def validate_frontmatter_presence(skill):
        if skill.frontmatter is not None:
            return [ValidationResult(True, 'frontmatter.presence', 'Frontmatter present')]
        return [ValidationResult(False, 'frontmatter.presence', 'No YAML frontmatter found')]

    @staticmethod
    def validate_required_fields(skill):
        results = []
        fm = skill.frontmatter
        if fm is None:
            results.append(ValidationResult(False, 'frontmatter.required.name', 'No frontmatter, cannot check required fields'))
            return results

        for field in ('name', 'description'):
            value = fm.get(field)
            if value:
                results.append(ValidationResult(True, 'frontmatter.required.{}'.format(field), '{} is present'.format(field)))
            else:
                results.append(ValidationResult(False, 'frontmatter.required.{}'.format(field), 'Required field \'{}\' is missing'.format(field)))
        return results

    @classmethod
    def validate_field_values(cls, skill):
        results = []
        fm = skill.frontmatter
        if fm is None:
            return results

        model = fm.get('model')
        if model is not None:
            if model in cls.VALID_MODELS:
                results.append(ValidationResult(True, 'frontmatter.values.model', 'model \'{}\' is valid'.format(model)))
            else:
                results.append(ValidationResult(
                    False, 'frontmatter.values.model',
                    'model \'{}\' is not valid, expected one of: {}'.format(model, ', '.join(sorted(cls.VALID_MODELS))),
                ))

        effort = fm.get('effort')
        if effort is not None:
            if effort in cls.VALID_EFFORTS:
                results.append(ValidationResult(True, 'frontmatter.values.effort', 'effort \'{}\' is valid'.format(effort)))
            else:
                results.append(ValidationResult(
                    False, 'frontmatter.values.effort',
                    'effort \'{}\' is not valid, expected one of: {}'.format(effort, ', '.join(sorted(cls.VALID_EFFORTS))),
                ))

        for field in ('user-invocable', 'disable-model-invocation'):
            value = fm.get(field)
            if value is not None and not isinstance(value, bool):
                results.append(ValidationResult(
                    False, 'frontmatter.values.{}'.format(field),
                    '\'{}\' should be a boolean, got {}'.format(field, type(value).__name__),
                ))
            elif value is not None:
                results.append(ValidationResult(True, 'frontmatter.values.{}'.format(field), '\'{}\' is a valid boolean'.format(field)))

        return results

    @classmethod
    def validate_unknown_keys(cls, skill):
        results = []
        fm = skill.frontmatter
        if fm is None:
            return results

        unknown = set(fm.keys()) - cls.KNOWN_FRONTMATTER_KEYS
        if unknown:
            for key in sorted(unknown):
                results.append(ValidationResult(
                    False, 'frontmatter.unknown_key',
                    'Unknown frontmatter key: \'{}\''.format(key),
                    severity='warning',
                ))
        else:
            results.append(ValidationResult(True, 'frontmatter.unknown_key', 'No unknown frontmatter keys'))
        return results

    @classmethod
    def validate_allowed_tools_format(cls, skill):
        results = []
        tools = skill.allowed_tools
        if not tools:
            return results

        for tool in tools:
            if cls.TOOL_PATTERN.match(tool):
                results.append(ValidationResult(True, 'frontmatter.allowed_tools.format', '\'{}\' is valid'.format(tool)))
            else:
                results.append(ValidationResult(
                    False, 'frontmatter.allowed_tools.format',
                    '\'{}\' does not match expected format ToolName or ToolName(pattern)'.format(tool),
                ))
        return results

    @staticmethod
    def validate_name_matches_directory(skill):
        fm = skill.frontmatter
        if fm is None or not fm.get('name'):
            return []
        dirname = os.path.basename(skill.skill_dir)
        name = fm['name']
        if name == dirname:
            return [ValidationResult(True, 'frontmatter.name_matches_dir', 'name \'{}\' matches directory'.format(name))]
        return [ValidationResult(
            False, 'frontmatter.name_matches_dir',
            'name \'{}\' does not match directory \'{}\''.format(name, dirname),
            severity='warning',
        )]

    @staticmethod
    def validate_references_exist(skill):
        results = []
        refs = skill.references
        if not refs:
            return results

        for ref in refs:
            full_path = os.path.normpath(os.path.join(skill.skill_dir, ref))
            if os.path.exists(full_path):
                results.append(ValidationResult(True, 'references.exist', '\'{}\' exists'.format(ref)))
            else:
                results.append(ValidationResult(
                    False, 'references.exist',
                    'Referenced file \'{}\' does not exist (resolved to {})'.format(ref, full_path),
                    severity='warning',
                ))
        return results

    @classmethod
    def validate_all(cls, skill):
        results = []
        results.extend(cls.validate_frontmatter_presence(skill))
        results.extend(cls.validate_required_fields(skill))
        results.extend(cls.validate_field_values(skill))
        results.extend(cls.validate_unknown_keys(skill))
        results.extend(cls.validate_allowed_tools_format(skill))
        results.extend(cls.validate_name_matches_directory(skill))
        results.extend(cls.validate_references_exist(skill))
        return results


class DirectoryValidator(object):
    @staticmethod
    def validate_settings_json(claude_dir):
        path = os.path.join(claude_dir, 'settings.json')
        if not os.path.exists(path):
            return [ValidationResult(True, 'settings.json', 'settings.json not present (optional)')]
        try:
            with open(path, 'r') as f:
                json.load(f)
        except (json.JSONDecodeError, ValueError) as e:
            return [ValidationResult(False, 'settings.json', 'settings.json is not valid JSON: {}'.format(e))]
        return [ValidationResult(True, 'settings.json', 'settings.json is valid JSON')]

    @staticmethod
    def validate_marketplace_json(claude_dir):
        plugins_dir = os.path.join(claude_dir, 'plugins')
        if not os.path.isdir(plugins_dir):
            return []
        path = os.path.join(plugins_dir, '.claude-plugin', 'marketplace.json')
        if not os.path.exists(path):
            return [ValidationResult(False, 'marketplace.json', 'plugins directory exists but marketplace.json is missing')]
        try:
            with open(path, 'r') as f:
                json.load(f)
        except (json.JSONDecodeError, ValueError) as e:
            return [ValidationResult(False, 'marketplace.json', 'marketplace.json is not valid JSON: {}'.format(e))]
        return [ValidationResult(True, 'marketplace.json', 'marketplace.json is valid JSON')]
