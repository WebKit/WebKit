#!/usr/bin/env python3
#
# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import json
import os
import re
import sys
from collections import Counter


class QuirkTableValidator(object):
    """Validates a parsed QuirkTable.json against the files next to it.
    This validator mirrors the runtime parser in RuntimeQuirkTable.cpp
    """

    ROW_FIELDS = ('bugs', 'comment', 'matches', 'embeddedMatches', 'excludeMatches', 'queryContains', 'fragmentContains', 'environment', 'available', 'behaviors')
    PATTERN_FIELDS = ('matches', 'embeddedMatches', 'excludeMatches')
    SUBSTRING_FIELDS = ('queryContains', 'fragmentContains')
    PARAMETER_FIELDS = {'script': 'Script', 'userAgent': 'UserAgent', 'chromeCompatibilityVersion': 'ChromeCompatibilityVersion', 'cookieNames': 'CookieNames'}
    CONDITION_FIELDS = {'elementSelector': 'ElementSelector', 'secondaryURL': 'SecondaryURL', 'documentSelector': 'DocumentSelector'}
    LIST_FIELDS = ('cookieNames', 'secondaryURL')
    BEHAVIOR_FIELDS = ('id',) + tuple(PARAMETER_FIELDS) + tuple(CONDITION_FIELDS) + ('bugs', 'comment')
    BUG_LINK = re.compile(r'^(rdar://\d+|https://webkit\.org/b/\d+)$')
    AVAILABLE_TOKEN = re.compile(r' *(\|\||&&|!|\(|\)|[A-Za-z0-9_]+)')
    MAXIMUM_AVAILABLE_NESTING_DEPTH = 32

    def __init__(self, directory):
        self._directory = directory

    def validate(self, table):
        """Returns the errors in table, the parsed contents of QuirkTable.json, as a list of messages."""
        self._errors = []

        self._behaviors = self._read_behaviors(os.path.join(self._directory, 'QuirkBehaviors.yaml'))
        self._build_conditions = self._read_build_conditions(os.path.join(self._directory, 'QuirkBuildConditions.yaml'))
        self._environments = self._read_enumerators(os.path.join(self._directory, 'QuirkMatchPattern.h'), 'URLEnvironment')
        if self._behaviors is None or self._build_conditions is None or self._environments is None:
            return self._errors

        if not isinstance(table, dict) or list(table.keys()) != ['quirks'] or not isinstance(table['quirks'], list):
            self._error('The top level must be an object whose only key is "quirks", an array of rows.')
            return self._errors

        for index, row in enumerate(table['quirks']):
            self._check_row(row, index)
        return self._errors

    def _error(self, message):
        self._errors.append(message)

    def _read_file(self, path):
        try:
            with open(path) as file:
                return file.read()
        except (IOError, OSError):
            self._error('Could not read %s, which QuirkTable.json is checked against.' % os.path.basename(path))
            return None

    def _reject_line(self, path, line_number, expected):
        self._error('%s:%d: expected %s (the quirk table validator only reads flat YAML).' % (os.path.basename(path), line_number, expected))

    @staticmethod
    def _content_lines(contents):
        for line_number, line in enumerate(contents.splitlines(), 1):
            if line.strip() and not line.lstrip().startswith('#'):
                yield line_number, line

    def _read_behaviors(self, path):
        contents = self._read_file(path)
        if contents is None:
            return None

        behaviors = {}
        current = None
        for line_number, line in self._content_lines(contents):
            match = re.match(r'^(\w+):\s*$', line)
            if match:
                current = behaviors[match.group(1)] = {'parameters': [], 'conditions': [], 'conditionsRequired': False}
                continue
            match = re.match(r'^\s+(\w+):\s+(\S.*?)\s*$', line)
            if not match or current is None:
                self._reject_line(path, line_number, '"BehaviorName:" or an indented "key: value" under a behavior')
                return None
            key, value = match.groups()
            if key in ('parameters', 'conditions'):
                if not re.match(r'^\[[\w, ]*\]$', value):
                    self._reject_line(path, line_number, '"%s" to be an inline list like [A, B]' % key)
                    return None
                current[key] = [item.strip() for item in value.strip('[]').split(',') if item.strip()]
            elif key == 'conditionsRequired':
                if value not in ('true', 'false'):
                    self._reject_line(path, line_number, '"conditionsRequired" to be true or false')
                    return None
                current[key] = value == 'true'
        return behaviors

    def _read_build_conditions(self, path):
        contents = self._read_file(path)
        if contents is None:
            return None

        conditions = set()
        for line_number, line in self._content_lines(contents):
            match = re.match(r'^(\w+):\s+\S', line)
            if not match:
                self._reject_line(path, line_number, '"name: condition"')
                return None
            conditions.add(match.group(1))
        return conditions

    def _read_enumerators(self, path, enum_name):
        contents = self._read_file(path)
        if contents is None:
            return None
        enum = re.search(r'enum class %s\b[^{]*\{([^}]*)\}' % enum_name, contents)
        return set(re.findall(r'\w+', enum.group(1))) if enum else set()

    def _is_available_expression(self, expression):
        if not isinstance(expression, str):
            return False
        tokens = []
        position = 0
        expression = expression.rstrip(' ')
        while position < len(expression):
            match = self.AVAILABLE_TOKEN.match(expression, position)
            if not match:
                return False
            tokens.append(match.group(1))
            position = match.end()

        index = 0

        def take(token):
            nonlocal index
            if index < len(tokens) and tokens[index] == token:
                index += 1
                return True
            return False

        def parse_or(depth):
            if not parse_and(depth):
                return False
            while take('||'):
                if not parse_and(depth):
                    return False
            return True

        def parse_and(depth):
            if not parse_unary(depth):
                return False
            while take('&&'):
                if not parse_unary(depth):
                    return False
            return True

        def parse_unary(depth):
            nonlocal index
            depth += 1
            if depth > self.MAXIMUM_AVAILABLE_NESTING_DEPTH:
                return False
            if take('!'):
                return parse_unary(depth)
            if take('('):
                return parse_or(depth) and take(')')
            if index < len(tokens) and tokens[index] in self._build_conditions:
                index += 1
                return True
            return False

        return parse_or(0) and index == len(tokens)

    @staticmethod
    def _is_non_empty_string(value):
        return isinstance(value, str) and bool(value)

    @classmethod
    def _is_non_empty_string_list(cls, value):
        return isinstance(value, list) and bool(value) and all(cls._is_non_empty_string(element) for element in value)

    @staticmethod
    def _describe(row, index):
        patterns = row.get('matches', row.get('embeddedMatches')) if isinstance(row, dict) else None
        if isinstance(patterns, list) and patterns and isinstance(patterns[0], str):
            return 'quirks[%d] (%s)' % (index, patterns[0])
        return 'quirks[%d]' % index

    def _check_notes(self, entry, location):
        if 'comment' in entry and not self._is_non_empty_string(entry['comment']):
            self._error('%s: "comment" must be a non-empty string.' % location)
        if 'bugs' not in entry:
            return
        bugs = entry['bugs']
        if not self._is_non_empty_string_list(bugs) or not all(self.BUG_LINK.match(bug) for bug in bugs):
            self._error('%s: "bugs" must be a non-empty array of rdar://N or https://webkit.org/b/N references.' % location)
        elif len(set(bugs)) != len(bugs):
            self._error('%s: "bugs" lists a bug more than once.' % location)

    def _check_row(self, row, index):
        location = self._describe(row, index)
        if not isinstance(row, dict):
            self._error('%s: a row must be an object.' % location)
            return

        for field in row:
            if field not in self.ROW_FIELDS:
                self._error('%s: unknown field "%s".' % (location, field))
        self._check_notes(row, location)

        for field in self.PATTERN_FIELDS:
            if field in row and not self._is_non_empty_string_list(row[field]):
                self._error('%s: "%s" must be a non-empty array of match patterns.' % (location, field))
        if 'matches' not in row and 'embeddedMatches' not in row:
            self._error('%s: a row must have "matches" or "embeddedMatches".' % location)
        for field in self.SUBSTRING_FIELDS:
            if field in row and not self._is_non_empty_string(row[field]):
                self._error('%s: "%s" must be a non-empty string.' % (location, field))

        if 'environment' in row and row['environment'] not in self._environments:
            self._error('%s: "environment" must name a URLEnvironment.' % location)

        if 'available' in row and not self._is_available_expression(row['available']):
            self._error('%s: "available" must be build condition names combined with !, &&, || and parentheses.' % location)

        behaviors = row.get('behaviors')
        if not isinstance(behaviors, list) or not behaviors:
            self._error('%s: "behaviors" must be a non-empty array.' % location)
            return
        for behavior_index, behavior in enumerate(behaviors):
            self._check_behavior(behavior, '%s.behaviors[%d]' % (location, behavior_index))

        counts = Counter(behavior['id'] for behavior in behaviors if isinstance(behavior, dict) and isinstance(behavior.get('id'), str) and behavior['id'] in self._behaviors)
        for duplicate in sorted(id for id, count in counts.items() if count > 1):
            self._error('%s: "behaviors" lists %s more than once.' % (location, duplicate))

    def _check_behavior(self, entry, location):
        if not isinstance(entry, dict):
            self._error('%s: a behavior must be an object.' % location)
            return

        for field in entry:
            if field not in self.BEHAVIOR_FIELDS:
                self._error('%s: unknown field "%s".' % (location, field))
        self._check_notes(entry, location)

        if 'id' not in entry:
            self._error('%s: a behavior must have an "id".' % location)
            return
        if not self._is_non_empty_string(entry['id']):
            self._error('%s: "id" must be a non-empty string.' % location)
            return
        behavior = self._behaviors.get(entry['id'])
        if behavior is None:
            self._error('%s: "id" %s is not a behavior in QuirkBehaviors.yaml.' % (location, json.dumps(entry['id'])))
            return
        location = '%s %s' % (location, entry['id'])

        for field in tuple(self.PARAMETER_FIELDS) + tuple(self.CONDITION_FIELDS):
            if field not in entry:
                continue
            is_list = field in self.LIST_FIELDS
            if not (self._is_non_empty_string_list(entry[field]) if is_list else self._is_non_empty_string(entry[field])):
                self._error('%s: "%s" must be a non-empty %s.' % (location, field, 'array of strings' if is_list else 'string'))

        for field, parameter in self.PARAMETER_FIELDS.items():
            if parameter in behavior['parameters'] and field not in entry:
                self._error('%s: needs "%s".' % (location, field))
            elif parameter not in behavior['parameters'] and field in entry:
                self._error('%s: does not take "%s".' % (location, field))

        for field, condition in self.CONDITION_FIELDS.items():
            if field in entry and condition not in behavior['conditions']:
                self._error('%s: does not take "%s".' % (location, field))
            elif field not in entry and behavior['conditionsRequired'] and condition in behavior['conditions']:
                self._error('%s: needs "%s".' % (location, field))


def main(argv):
    if len(argv) > 2:
        print('Usage: %s [path/to/QuirkTable.json]' % os.path.basename(argv[0]), file=sys.stderr)
        return 2

    table_path = argv[1] if len(argv) == 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'page', 'QuirkTable.json')
    try:
        with open(table_path) as file:
            table = json.load(file)
    except (IOError, OSError, ValueError) as error:
        print('%s: %s' % (table_path, error), file=sys.stderr)
        return 1

    errors = QuirkTableValidator(os.path.dirname(os.path.abspath(table_path))).validate(table)
    for error in errors:
        print('%s: %s' % (table_path, error), file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
