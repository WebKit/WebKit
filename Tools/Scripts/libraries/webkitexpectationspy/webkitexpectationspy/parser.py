# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

"""Parser for test expectation files.

This parser creates Expectation objects parameterised by the suite's
result status Flag enum and modifier dataclass.
"""

import re
from typing import List, Optional, Set

from webkitexpectationspy.expectations import Expectation, ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase
from webkitexpectationspy.configuration import (
    ConfigurationCategory, get_token_category
)
from webkitexpectationspy.version_specifier import VersionSpecifier


class ParseWarning:
    def __init__(self, filename, line_number, line, error, test=None):
        self.filename = filename
        self.line_number = line_number
        self.line = line
        self.error = error
        self.test = test

    def __str__(self):
        return '{}:{}: {} {}'.format(
            self.filename, self.line_number, self.error,
            self.test if self.test else self.line)

    def __repr__(self):
        return 'ParseWarning({!r}, {}, {!r})'.format(
            self.filename, self.line_number, self.error)


class ExpectationParser:
    WEBKIT_BUG_PREFIX = 'webkit.org/b/'
    RADAR_BUG_PREFIX = 'rdar://'
    BUG_PATTERN = re.compile(r'Bug\((\w+)\)$', re.IGNORECASE)
    RADAR_PATTERN = re.compile(r'^rdar://(?:problem/)?([\d/]+)$')
    SLOW_TIMEOUT_PATTERN = re.compile(r'^Slow:(\d+)s?$', re.IGNORECASE)

    def __init__(self, suite=None):
        self._suite = suite

        if self._suite:
            self._expectation_map = self._suite.expectation_map
            self._modifier_map = self._suite.modifier_map
            self._modifier_class = self._suite.modifier_class
        else:
            self._expectation_map = {
                'pass': ResultStatus.PASS,
                'fail': ResultStatus.FAIL,
                'failure': ResultStatus.FAIL,
                'crash': ResultStatus.CRASH,
                'timeout': ResultStatus.TIMEOUT,
            }
            self._modifier_map = {'skip', 'slow'}
            self._modifier_class = ModifiersBase

        self._version_tokens = self._suite.version_tokens if self._suite else set()

    def parse(self, filename, content):
        results = []
        line_number = 0
        for line in content.split('\n'):
            line_number += 1
            expectation, warnings = self._parse_line(filename, line, line_number)
            results.append((expectation, warnings))
        return results

    def _parse_line(self, filename, line, line_number):
        warnings = []

        comment_index = line.find('#')
        if comment_index != -1:
            line = line[:comment_index]

        line = ' '.join(line.split())
        if not line:
            return None, warnings

        if line.startswith('//'):
            warnings.append(ParseWarning(
                filename, line_number, line,
                'Use "#" instead of "//" for comments'))
            return None, warnings

        bug_ids = []
        configurations = set()
        version_specifiers = []
        test_pattern = None
        expected_results = set()
        modifier_fields = {}
        slow_timeout = None

        tokens = line.split()
        state = 'start'

        for token in tokens:
            if (token.startswith(self.WEBKIT_BUG_PREFIX) or
                    token.startswith(self.RADAR_BUG_PREFIX) or
                    token.lower().startswith('bug(')):
                if state != 'start':
                    warnings.append(ParseWarning(
                        filename, line_number, line,
                        '"{}" is not at the start of the line'.format(token)))
                    break
                if token.startswith(self.WEBKIT_BUG_PREFIX):
                    bug_ids.append(token)
                elif token.startswith(self.RADAR_BUG_PREFIX):
                    match = self.RADAR_PATTERN.match(token)
                    if match:
                        bug_ids.append(token)
                    else:
                        warnings.append(ParseWarning(
                            filename, line_number, line,
                            'Invalid radar format "{}". Use rdar://XXXXX or rdar://problem/XXXXX'.format(token)))
                        break
                else:
                    match = self.BUG_PATTERN.match(token)
                    if match:
                        bug_ids.append('Bug({})'.format(match.group(1)))
                    else:
                        warnings.append(ParseWarning(
                            filename, line_number, line,
                            'Unrecognized bug identifier "{}"'.format(token)))
                        break

            elif token == '[':
                if state == 'start':
                    state = 'configuration'
                elif state == 'name_found':
                    state = 'expectations'
                else:
                    warnings.append(ParseWarning(
                        filename, line_number, line,
                        'Unexpected "["'))
                    break

            elif token == ']':
                if state == 'configuration':
                    state = 'name'
                elif state == 'expectations':
                    state = 'done'
                else:
                    warnings.append(ParseWarning(
                        filename, line_number, line,
                        'Unexpected "]"'))
                    break

            elif state == 'configuration':
                token_lower = token.lower()
                category = get_token_category(token, self._version_tokens)
                if category is not None:
                    if category == ConfigurationCategory.VERSION:
                        spec = VersionSpecifier.parse(token)
                        if spec:
                            version_specifiers.append(spec)
                        else:
                            spec = VersionSpecifier(token, specifier_type=VersionSpecifier.Type.EXACT)
                            spec._original = token
                            version_specifiers.append(spec)
                    else:
                        configurations.add(token_lower)
                else:
                    warnings.append(ParseWarning(
                        filename, line_number, line,
                        'Unrecognized configuration "{}"'.format(token)))
                    break

            elif state == 'expectations':
                token_lower = token.lower()

                slow_match = self.SLOW_TIMEOUT_PATTERN.match(token)
                if slow_match:
                    if self._suite and not self._suite.supports_custom_timeout():
                        warnings.append(ParseWarning(
                            filename, line_number, line,
                            'Custom timeout not supported in this format'))
                        break
                    slow_timeout = int(slow_match.group(1))
                    if slow_timeout < 1 or slow_timeout > 3600:
                        warnings.append(ParseWarning(
                            filename, line_number, line,
                            'Slow timeout must be between 1 and 3600 seconds, got {}'.format(slow_timeout)))
                        break
                    modifier_fields['slow'] = float(slow_timeout)

                elif self._is_modifier(token_lower):
                    if token_lower == 'skip':
                        modifier_fields['skip'] = True
                    elif token_lower == 'slow':
                        modifier_fields['slow'] = -1.0
                    elif token_lower == 'wontfix':
                        modifier_fields['wontfix'] = True
                    elif token_lower == 'rebaseline':
                        modifier_fields['rebaseline'] = True

                elif token_lower in self._expectation_map:
                    expected_results.add(self._expectation_map[token_lower])

                else:
                    warnings.append(ParseWarning(
                        filename, line_number, line,
                        'Unrecognized expectation "{}"'.format(token)))
                    break

            elif state == 'name_found':
                warnings.append(ParseWarning(
                    filename, line_number, line,
                    'Expecting "[", "#", or end of line instead of "{}"'.format(token)))
                break

            else:
                if self._suite:
                    validated = self._suite.validate_test_pattern(token)
                    if validated is None:
                        warnings.append(ParseWarning(
                            filename, line_number, line,
                            'Invalid test pattern "{}"'.format(token)))
                        break
                    test_pattern = validated
                else:
                    test_pattern = token
                state = 'name_found'

        if not warnings:
            if not test_pattern:
                warnings.append(ParseWarning(
                    filename, line_number, line,
                    'Did not find a test name'))
            elif state not in ('name_found', 'done'):
                warnings.append(ParseWarning(
                    filename, line_number, line,
                    'Missing a "]"'))

        if 'slow' in modifier_fields:
            timeout_status = self._expectation_map.get('timeout')
            if timeout_status and timeout_status in expected_results:
                warnings.append(ParseWarning(
                    filename, line_number, line,
                    'A test cannot be both Slow and Timeout'))

        if not expected_results and not warnings:
            default_pass = self._expectation_map.get('pass', ResultStatus.PASS)
            expected_results.add(default_pass)

        if warnings:
            return None, warnings

        modifiers = self._build_modifiers(modifier_fields)

        combined_expected = None
        for flag in expected_results:
            combined_expected = flag if combined_expected is None else (combined_expected | flag)

        expectation = Expectation(
            test_pattern=test_pattern,
            expected=combined_expected,
            modifiers=modifiers,
            configurations=frozenset(configurations),
            bug_ids=tuple(bug_ids),
            filename=filename,
            line_number=line_number,
            version_specifiers=tuple(version_specifiers),
        )
        return expectation, warnings

    def _is_modifier(self, token_lower):
        return token_lower in self._modifier_map

    def _build_modifiers(self, fields):
        slow_val = fields.get('slow')
        if slow_val is not None and slow_val < 0:
            slow_val = -1.0

        kwargs = {
            'skip': fields.get('skip', False),
            'slow': slow_val,
        }
        if hasattr(self._modifier_class, 'wontfix'):
            kwargs['wontfix'] = fields.get('wontfix', False)
        if hasattr(self._modifier_class, 'rebaseline'):
            kwargs['rebaseline'] = fields.get('rebaseline', False)

        return self._modifier_class(**kwargs)
