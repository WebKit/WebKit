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

"""Linter for test expectations files with 3-section sorting and auto-fix support.

Sections are ordered:
  0 - Unannotated: no bug IDs and no inline comment
  1 - Commented:   has an inline ``# comment`` but no bug IDs
  2 - Bug-tracked: has ANY bug ID (rdar://, webkit.org/b/, Bug())

Within each section, entries are sorted by:
  1. Wildcard patterns (ending with ``*``) before exact patterns
  2. Alphabetical by test_pattern (case-insensitive)
"""

import re
from abc import ABC, abstractmethod
from collections import defaultdict
from dataclasses import dataclass
from typing import List, Optional

from webkitexpectationspy.configuration import (
    ConfigurationCategory, CATEGORY_TOKENS, CATEGORY_NAMES,
    PLATFORM_TOKENS, get_token_category, _CATEGORY_ORDER,
)


@dataclass
class LintWarning:
    filename: str
    line_number: int
    message: str
    fixable: bool = False
    fix_description: Optional[str] = None

    def __str__(self):
        prefix = '[fixable] ' if self.fixable else ''
        return '{}:{}: {}{}'.format(self.filename, self.line_number, prefix, self.message)

    def __repr__(self):
        return 'LintWarning({!r}, {}, {!r})'.format(
            self.filename, self.line_number, self.message)


@dataclass
class LineFix:
    line_number: int
    new_content: Optional[str] = None
    delete: bool = False


_BUG_PREFIXES = ('webkit.org/b/', 'rdar://')
_BUG_RE = re.compile(r'Bug\(\w+\)', re.IGNORECASE)


def _line_has_bug_id(line: str) -> bool:
    for prefix in _BUG_PREFIXES:
        if prefix in line:
            return True
    return bool(_BUG_RE.search(line))


def _line_has_inline_comment(line: str) -> bool:
    idx = line.find('#')
    if idx < 0:
        return False
    return len(line[:idx].strip()) > 0


def _classify_section(line: str) -> int:
    if _line_has_bug_id(line):
        return 2
    if _line_has_inline_comment(line):
        return 1
    return 0


class LintRule(ABC):
    @abstractmethod
    def check(self, entries, filepath, version_tokens):
        """Return (warnings, fixes) for the given parsed entries."""
        ...


class CategoryOrderingRule(LintRule):
    """Check that configuration tokens within a bracket are in canonical category order."""

    def check(self, entries, filepath, version_tokens):
        warnings = []
        fixes = []
        for entry in entries:
            config_tokens = entry['config_tokens']
            if len(config_tokens) < 2:
                continue

            last_category = None
            for token in config_tokens:
                category = get_token_category(token, version_tokens)
                if category is None:
                    continue
                if last_category is not None and _CATEGORY_ORDER[category] < _CATEGORY_ORDER[last_category]:
                    warnings.append(LintWarning(
                        filepath,
                        entry['line_number'],
                        'Configuration tokens out of order: "{}" ({}) appears after {} category. '
                        'Expected order: Platform -> Version -> Style -> Hardware -> Architecture -> Flavor'.format(
                            token, CATEGORY_NAMES.get(category, str(category)),
                            CATEGORY_NAMES.get(last_category, str(last_category))),
                        fixable=True
                    ))
                    fix = _make_reorder_fix(entry, version_tokens)
                    if fix:
                        fixes.append(fix)
                    break
                last_category = category

        return warnings, fixes


class SectionOrderingRule(LintRule):
    """Check 3-section ordering and alphabetical order within sections."""

    def check(self, entries, filepath, version_tokens):
        warnings = []
        fixes = []

        # Alphabetical within config categories
        for entry in entries:
            config_tokens = entry['config_tokens']
            if len(config_tokens) < 2:
                continue

            by_category = defaultdict(list)
            for token in config_tokens:
                category = get_token_category(token, version_tokens)
                if category:
                    by_category[category].append(token.lower())

            for category, tokens in by_category.items():
                sorted_tokens = sorted(tokens)
                if tokens != sorted_tokens:
                    warnings.append(LintWarning(
                        filepath,
                        entry['line_number'],
                        'Tokens in {} category not alphabetically ordered: {} should be {}'.format(
                            CATEGORY_NAMES.get(category, str(category)),
                            tokens, sorted_tokens),
                        fixable=True
                    ))
                    fix = _make_reorder_fix(entry, version_tokens)
                    if fix:
                        fixes.append(fix)

        # Entry ordering across sections
        prev_key = None
        prev_entry = None
        for entry in entries:
            raw_line = entry['raw_line']
            section = _classify_section(raw_line)
            is_wildcard = entry['test_pattern'].endswith('*')
            key = (section, 0 if is_wildcard else 1, entry['test_pattern'].lower())

            if prev_key is not None and key < prev_key:
                if key[0] < prev_key[0]:
                    section_names = {0: 'unannotated', 1: 'commented', 2: 'bug-tracked'}
                    warnings.append(LintWarning(
                        filepath,
                        entry['line_number'],
                        'Test "{}" is {} (section {}) but appears after {} entry "{}" (section {}). '
                        'Expected order: unannotated -> commented -> bug-tracked'.format(
                            entry['test_pattern'],
                            section_names.get(key[0], str(key[0])), key[0],
                            section_names.get(prev_key[0], str(prev_key[0])),
                            prev_entry['test_pattern'], prev_key[0]),
                        fixable=True,
                        fix_description='Reorder entries by section then alphabetically'
                    ))
                elif key[0] == prev_key[0] and key[1] < prev_key[1]:
                    warnings.append(LintWarning(
                        filepath,
                        entry['line_number'],
                        'Wildcard pattern "{}" should appear before exact pattern "{}" within section'.format(
                            entry['test_pattern'], prev_entry['test_pattern']),
                        fixable=True,
                        fix_description='Reorder: wildcards before exact patterns'
                    ))
                else:
                    warnings.append(LintWarning(
                        filepath,
                        entry['line_number'],
                        'Test "{}" should appear before "{}" (line {}) for alphabetical order'.format(
                            entry['test_pattern'], prev_entry['test_pattern'],
                            prev_entry['line_number']),
                        fixable=True,
                        fix_description='Reorder test entries alphabetically'
                    ))
            prev_key = key
            prev_entry = entry

        return warnings, fixes


class CombinationCollapseRule(LintRule):
    """Detect entries that cover all values in a category and can be collapsed."""

    def check(self, entries, filepath, version_tokens):
        warnings = []
        fixes = []

        by_test = defaultdict(list)
        for entry in entries:
            by_test[entry['test_pattern']].append(entry)

        for test_pattern, test_entries in by_test.items():
            if len(test_entries) < 2:
                continue

            expectations_set = set(tuple(sorted(e['expectations'])) for e in test_entries)
            if len(expectations_set) != 1:
                continue

            for category, all_values in CATEGORY_TOKENS.items():
                if not all_values:
                    continue

                covered_values = set()
                entries_with_category = []
                for entry in test_entries:
                    cat_values = {t.lower() for t in entry['config_tokens']
                                  if get_token_category(t, version_tokens) == category}
                    if cat_values:
                        covered_values.update(cat_values)
                        entries_with_category.append(entry)

                if covered_values == all_values and len(entries_with_category) == len(all_values):
                    line_nums = [e['line_number'] for e in entries_with_category]
                    warnings.append(LintWarning(
                        filepath,
                        line_nums[0],
                        'Test "{}" has entries covering all {} values (lines {}). '
                        'Can be collapsed by removing {} specifier.'.format(
                            test_pattern,
                            CATEGORY_NAMES.get(category, str(category)),
                            ', '.join(str(n) for n in line_nums),
                            CATEGORY_NAMES.get(category, str(category))),
                        fixable=True
                    ))
                    collapse_fixes = _make_collapse_fixes(entries_with_category, category, version_tokens)
                    fixes.extend(collapse_fixes)

        return warnings, fixes


class UniversalSkipRule(LintRule):
    """Detect tests skipped on all configurations."""

    def check(self, entries, filepath, version_tokens):
        warnings = []

        by_test = defaultdict(list)
        for entry in entries:
            by_test[entry['test_pattern']].append(entry)

        for test_pattern, test_entries in by_test.items():
            all_skip = all(
                any(e.lower() == 'skip' for e in entry['expectations'])
                for entry in test_entries
            )
            if not all_skip:
                continue

            has_unconditional = any(not entry['config_tokens'] for entry in test_entries)
            covered_platforms = set()
            for entry in test_entries:
                for token in entry['config_tokens']:
                    if token.lower() in PLATFORM_TOKENS:
                        covered_platforms.add(token.lower())

            if has_unconditional or covered_platforms >= PLATFORM_TOKENS:
                line_nums = [e['line_number'] for e in test_entries]
                warnings.append(LintWarning(
                    filepath,
                    line_nums[0],
                    'Test "{}" is skipped on all configurations (lines {}). '
                    'Consider removing the test entirely.'.format(
                        test_pattern,
                        ', '.join(str(n) for n in line_nums)),
                    fixable=False
                ))

        return warnings, []


def _make_reorder_fix(entry, version_tokens):
    config_tokens = entry['config_tokens']
    if not config_tokens:
        return None

    def sort_key(token):
        category = get_token_category(token, version_tokens)
        return (_CATEGORY_ORDER.get(category, _CATEGORY_ORDER[ConfigurationCategory.FLAVOR]), token.lower())

    sorted_tokens = sorted(config_tokens, key=sort_key)
    if sorted_tokens == config_tokens:
        return None

    old_line = entry['raw_line']
    new_config = '[ {} ]'.format(' '.join(sorted_tokens))
    new_line = re.sub(r'\[\s*[^\]]*\s*\]', new_config, old_line, count=1)
    return LineFix(entry['line_number'], new_line)


def _make_collapse_fixes(entries, category, version_tokens):
    if len(entries) < 2:
        return []

    fixes = []
    first_entry = entries[0]
    config_tokens = [t for t in first_entry['config_tokens']
                     if get_token_category(t, version_tokens) != category]

    old_line = first_entry['raw_line']
    if config_tokens:
        new_config = '[ {} ]'.format(' '.join(config_tokens))
        new_line = re.sub(r'\[\s*[^\]]*\s*\]', new_config, old_line, count=1)
    else:
        new_line = re.sub(r'\[\s*[^\]]*\s*\]\s*', '', old_line, count=1)

    fixes.append(LineFix(first_entry['line_number'], new_line))
    for entry in entries[1:]:
        fixes.append(LineFix(entry['line_number'], delete=True))

    return fixes


_DEFAULT_RULES = [
    CategoryOrderingRule(),
    SectionOrderingRule(),
    CombinationCollapseRule(),
    UniversalSkipRule(),
]


class ExpectationsLinter:
    def __init__(self, content, filepath, suite=None, rules=None):
        self._content = content
        self._filepath = filepath
        self._suite = suite
        self._version_tokens = self._suite.version_tokens if self._suite else set()
        self._lines = content.split('\n')
        self._parsed_entries = []
        self._warnings = []
        self._fixes = []
        self._rules = rules or _DEFAULT_RULES

    def lint(self):
        self._parse_all_lines()
        seen_fix_lines = set()
        for rule in self._rules:
            rule_warnings, rule_fixes = rule.check(
                self._parsed_entries, self._filepath, self._version_tokens)
            self._warnings.extend(rule_warnings)
            for fix in rule_fixes:
                if fix.line_number not in seen_fix_lines:
                    self._fixes.append(fix)
                    seen_fix_lines.add(fix.line_number)
        return self._warnings

    def get_fixes(self):
        return self._fixes

    def apply_fixes(self):
        if not self._fixes:
            return self._content

        sorted_fixes = sorted(self._fixes, key=lambda f: -f.line_number)
        lines = self._lines[:]
        for fix in sorted_fixes:
            if fix.delete:
                del lines[fix.line_number - 1]
            elif fix.new_content is not None:
                lines[fix.line_number - 1] = fix.new_content

        return '\n'.join(lines)

    def generate_sorted_content(self) -> str:
        """Generate a full-file reorder producing correct 3-section ordering."""
        groups = []
        header_lines = []
        entry_index = 0

        for line_num, line in enumerate(self._lines, 1):
            is_entry = any(e['line_number'] == line_num for e in self._parsed_entries)
            if is_entry:
                entry = self._parsed_entries[entry_index]
                entry_index += 1
                groups.append((list(header_lines), line, entry))
                header_lines = []
            else:
                header_lines.append(line)

        def sort_key(group):
            _, raw_line, entry = group
            section = _classify_section(raw_line)
            is_wildcard = entry['test_pattern'].endswith('*')
            return (section, 0 if is_wildcard else 1, entry['test_pattern'].lower())

        sorted_groups = sorted(groups, key=sort_key)

        result_lines = []
        for header, entry_line, _ in sorted_groups:
            result_lines.extend(header)
            result_lines.append(entry_line)
        result_lines.extend(header_lines)

        return '\n'.join(result_lines)

    def _parse_all_lines(self):
        for line_num, line in enumerate(self._lines, 1):
            stripped = line.strip()
            if not stripped or stripped.startswith('#'):
                continue

            config_tokens = []
            test_pattern = None
            expectations = []
            inline_comment = None

            comment_idx = line.find('#')
            if comment_idx != -1:
                inline_comment = line[comment_idx + 1:].strip()
            line_content = line[:comment_idx] if comment_idx != -1 else line

            config_match = re.search(r'\[\s*([^\]]*)\s*\]', line_content)
            if config_match:
                config_str = config_match.group(1)
                config_start = config_match.start()
                before_config = line_content[:config_start].strip()

                before_words = before_config.split()
                non_bug_words = [w for w in before_words
                                 if not w.startswith('webkit.org/b/')
                                 and not w.startswith('rdar://')
                                 and not w.lower().startswith('bug(')]

                if non_bug_words:
                    test_pattern = non_bug_words[-1]
                    expectations = config_str.split()
                else:
                    config_tokens = config_str.split()
                    rest = line_content[config_match.end():].strip()
                    exp_match = re.search(r'\[\s*([^\]]*)\s*\]', rest)
                    if exp_match:
                        test_pattern = rest[:exp_match.start()].strip()
                        expectations = exp_match.group(1).split()
                    else:
                        test_pattern = rest
            else:
                words = line_content.split()
                for word in words:
                    if (not word.startswith('webkit.org/b/') and
                            not word.startswith('rdar://') and
                            not word.lower().startswith('bug(')):
                        test_pattern = word
                        break

            if test_pattern:
                self._parsed_entries.append({
                    'line_number': line_num,
                    'raw_line': line,
                    'config_tokens': config_tokens,
                    'test_pattern': test_pattern,
                    'expectations': expectations,
                    'inline_comment': inline_comment,
                })
