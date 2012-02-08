#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A helper class for reading in and dealing with tests expectations
for layout tests.
"""

import itertools
import json
import logging
import re

from webkitpy.layout_tests.models.test_configuration import TestConfiguration, TestConfigurationConverter
from webkitpy.layout_tests.models import test_expectations

_log = logging.getLogger(__name__)


class BugManager(object):
    """A simple interface for managing bugs from TestExpectationsEditor."""
    def close_bug(self, bug_ids, reference_bug_ids=None):
        raise NotImplementedError("BugManager.close_bug")

    def create_bug(self):
        """Should return a newly created bug id in the form of r"BUG[^\d].*"."""
        raise NotImplementedError("BugManager.create_bug")


class TestExpectationsEditor(object):
    """
    The editor assumes that the expectation data is error-free.
    """

    def __init__(self, expectation_lines, bug_manager):
        self._bug_manager = bug_manager
        self._expectation_lines = expectation_lines
        self._tests_with_directory_paths = set()
        # FIXME: Unify this with TestExpectationsModel.
        self._test_to_expectation_lines = {}
        for expectation_line in expectation_lines:
            for test in expectation_line.matching_tests:
                if test == expectation_line.path:
                    self._test_to_expectation_lines.setdefault(test, []).append(expectation_line)
                else:
                    self._tests_with_directory_paths.add(test)

    def remove_expectation(self, test, test_config_set, remove_flakes=False):
        """Removes existing expectations for {test} in the of test configurations {test_config_set}.
        If the test is flaky, the expectation is not removed, unless remove_flakes is True.

        In this context, removing expectations does not imply that the test is passing -- we are merely removing
        any information about this test from the expectations.

        We do not remove the actual expectation lines here. Instead, we adjust TestExpectationLine.matching_configurations.
        The serializer will figure out what to do:
        * An empty matching_configurations set means that the this line matches nothing and will serialize as None.
        * A matching_configurations set that can't be expressed as one line will be serialized as multiple lines.

        Also, we do only adjust matching_configurations for lines that match tests exactly, because expectation lines with
        better path matches are valid and always win.

        For example, the expectation with the path "fast/events/shadow/" will
        be ignored when removing expectations for the test "fast/event/shadow/awesome-crash.html", since we can just
        add a new expectation line for "fast/event/shadow/awesome-crash.html" to influence expected results.
        """
        expectation_lines = self._test_to_expectation_lines.get(test, [])
        for expectation_line in expectation_lines:
            if (not expectation_line.is_flaky() or remove_flakes) and expectation_line.matching_configurations & test_config_set:
                expectation_line.matching_configurations = expectation_line.matching_configurations - test_config_set
                if not expectation_line.matching_configurations:
                    self._bug_manager.close_bug(expectation_line.parsed_bug_modifiers)
                return

    def update_expectation(self, test, test_config_set, expectation_set, parsed_bug_modifiers=None):
        """Updates expectations for {test} in the set of test configuration {test_config_set} to the values of {expectation_set}.
        If {parsed_bug_modifiers} is supplied, it is used for updated expectations. Otherwise, a new bug is created.

        Here, we treat updating expectations to PASS as special: if possible, the corresponding lines are completely removed.
        """
        # FIXME: Allow specifying modifiers (SLOW, SKIP, WONTFIX).
        updated_expectations = []
        expectation_lines = self._test_to_expectation_lines.get(test, [])
        remaining_configurations = test_config_set.copy()
        bug_ids = self._get_valid_bug_ids(parsed_bug_modifiers)
        new_expectation_line_insertion_point = len(self._expectation_lines)
        remove_expectations = expectation_set == set([test_expectations.PASS]) and test not in self._tests_with_directory_paths

        for expectation_line in expectation_lines:
            if expectation_line.matching_configurations == remaining_configurations:
                # Tweak expectations on existing line.
                if expectation_line.parsed_expectations == expectation_set:
                    return updated_expectations
                self._bug_manager.close_bug(expectation_line.parsed_bug_modifiers, bug_ids)
                updated_expectations.append(expectation_line)
                if remove_expectations:
                    expectation_line.matching_configurations = set()
                else:
                    expectation_line.parsed_expectations = expectation_set
                    expectation_line.parsed_bug_modifiers = bug_ids
                return updated_expectations
            elif expectation_line.matching_configurations >= remaining_configurations:
                # 1) Split up into two expectation lines:
                # * one with old expectations (existing expectation_line)
                # * one with new expectations (new expectation_line)
                # 2) Finish looking, since there will be no more remaining configs to test for.
                expectation_line.matching_configurations -= remaining_configurations
                updated_expectations.append(expectation_line)
                new_expectation_line_insertion_point = self._expectation_lines.index(expectation_line) + 1
                break
            elif expectation_line.matching_configurations <= remaining_configurations:
                # Remove existing expectation line.
                self._bug_manager.close_bug(expectation_line.parsed_bug_modifiers, bug_ids)
                expectation_line.matching_configurations = set()
                updated_expectations.append(expectation_line)
            else:
                intersection = expectation_line.matching_configurations & remaining_configurations
                if intersection:
                    expectation_line.matching_configurations -= intersection
                    updated_expectations.append(expectation_line)
            new_expectation_line_insertion_point = self._expectation_lines.index(expectation_line) + 1

        if not remove_expectations:
            new_expectation_line = self._create_new_line(test, bug_ids, remaining_configurations, expectation_set)
            updated_expectations.append(new_expectation_line)
            self._expectation_lines.insert(new_expectation_line_insertion_point, new_expectation_line)

        return updated_expectations

    def _get_valid_bug_ids(self, suggested_bug_ids):
        # FIXME: Flesh out creating a bug properly (title, etc.)
        return suggested_bug_ids or [self._bug_manager.create_bug()]

    def _create_new_line(self, name, bug_ids, config_set, expectation_set):
        new_line = test_expectations.TestExpectationLine()
        new_line.name = name
        new_line.parsed_bug_modifiers = bug_ids
        new_line.matching_configurations = config_set
        new_line.parsed_expectations = expectation_set
        # Ensure index integrity for multiple operations.
        self._test_to_expectation_lines.setdefault(name, []).append(new_line)
        return new_line
