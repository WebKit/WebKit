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

"""High-level manager for test expectations."""

import logging
import os
from typing import List, Set, Dict, Optional

from webkitexpectationspy.expectations import Expectation
from webkitexpectationspy.parser import ExpectationParser, ParseWarning
from webkitexpectationspy.model import ExpectationsModel
from webkitexpectationspy.linter import ExpectationsLinter, LintWarning
from webkitexpectationspy.suites.base import TestSuiteFormat

_log = logging.getLogger(__name__)


class ExpectationsManager:
    """High-level facade for loading and querying test expectations."""

    def __init__(
        self,
        suite: Optional[TestSuiteFormat] = None,
    ) -> None:
        self._suite = suite
        self._parser = ExpectationParser(self._suite)
        self._model = ExpectationsModel(self._suite)
        self._warnings: List[ParseWarning] = []
        self._loaded_files: List[str] = []

    def load_file(self, path: str) -> List[ParseWarning]:
        if not os.path.exists(path):
            _log.debug('Expectations file not found: {}'.format(path))
            return []

        _log.debug('Loading expectations from {}'.format(path))
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
        return self._parse_content(path, content)

    def load_files(self, paths: List[str]) -> List[ParseWarning]:
        """Load expectations from multiple files in order; later files override earlier ones."""
        all_warnings: List[ParseWarning] = []
        for path in paths:
            warnings = self.load_file(path)
            all_warnings.extend(warnings)
        return all_warnings

    def load_content(self, filename: str, content: str) -> List[ParseWarning]:
        return self._parse_content(filename, content)

    def _parse_content(self, filename: str, content: str) -> List[ParseWarning]:
        results = self._parser.parse(filename, content)
        file_warnings = []

        for expectation, warnings in results:
            file_warnings.extend(warnings)
            self._warnings.extend(warnings)
            if expectation:
                dup_warning = self._model.add_expectation(expectation)
                if dup_warning:
                    file_warnings.append(dup_warning)
                    self._warnings.append(dup_warning)

        self._loaded_files.append((filename, content))
        return file_warnings

    def get_expectation(
        self,
        test_name: str,
        current_config: Optional[Set[str]] = None,
        current_version: Optional[str] = None,
        version_order: Optional[List[str]] = None
    ) -> Optional[Expectation]:
        if version_order is None and self._suite:
            version_order = self._suite.version_order
        return self._model.get_expectation(
            test_name, current_config, current_version, version_order)

    def get_expectation_or_pass(
        self,
        test_name: str,
        current_config: Optional[Set[str]] = None,
        current_version: Optional[str] = None,
        version_order: Optional[List[str]] = None
    ) -> Expectation:
        if version_order is None and self._suite:
            version_order = self._suite.version_order
        return self._model.get_expectation_or_pass(
            test_name, current_config, current_version, version_order)

    def get_skipped_tests(
        self,
        all_tests: List[str],
        current_config: Optional[Set[str]] = None,
        current_version: Optional[str] = None,
        version_order: Optional[List[str]] = None
    ) -> Set[str]:
        if version_order is None and self._suite:
            version_order = self._suite.version_order
        return self._model.get_skipped_tests(
            all_tests, current_config, current_version, version_order)

    def get_slow_tests(
        self,
        all_tests: List[str],
        current_config: Optional[Set[str]] = None,
        current_version: Optional[str] = None,
        version_order: Optional[List[str]] = None
    ) -> Dict[str, Optional[int]]:
        """Return dict mapping slow test names to timeouts (None for default 5x)."""
        if version_order is None and self._suite:
            version_order = self._suite.version_order
        return self._model.get_slow_tests(
            all_tests, current_config, current_version, version_order)

    def lint(self, all_tests=None):
        """Validate expectations and return warnings; pass all_tests to detect stale entries."""
        lint_warnings = list(self._warnings)

        # Lint each loaded file
        for filename, content in self._loaded_files:
            linter = ExpectationsLinter(content, filename, self._suite)
            lint_warnings.extend(linter.lint())

        # Check for stale expectations
        if all_tests:
            test_set = set(all_tests)
            for exp in self._model.all_expectations():
                is_wildcard = (
                    self._suite.is_wildcard_pattern(exp.test_pattern)
                    if self._suite else exp.test_pattern.endswith('*')
                )
                if is_wildcard:
                    matches = [t for t in all_tests
                               if (self._suite.matches_test(exp.test_pattern, t)
                                   if self._suite else exp.matches_test(t))]
                    if not matches:
                        lint_warnings.append(ParseWarning(
                            exp.filename, exp.line_number, exp.test_pattern,
                            'Stale wildcard pattern - matches no tests',
                            test=exp.test_pattern))
                else:
                    if exp.test_pattern not in test_set:
                        lint_warnings.append(ParseWarning(
                            exp.filename, exp.line_number, exp.test_pattern,
                            'Stale expectation - test does not exist',
                            test=exp.test_pattern))

        return lint_warnings

    def all_expectations(self):
        return self._model.all_expectations()

    def warnings(self):
        return self._warnings

    def clear(self):
        self._model.clear()
        self._warnings.clear()
        self._loaded_files.clear()
