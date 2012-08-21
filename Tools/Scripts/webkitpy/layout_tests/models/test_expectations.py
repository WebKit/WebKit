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

import logging
import re

from webkitpy.layout_tests.models.test_configuration import TestConfigurationConverter

_log = logging.getLogger(__name__)


# Test expectation and modifier constants.
# FIXME: range() starts with 0 which makes if expectation checks harder
# as PASS is 0.
(PASS, FAIL, TEXT, IMAGE, IMAGE_PLUS_TEXT, AUDIO, TIMEOUT, CRASH, SKIP, WONTFIX,
 SLOW, REBASELINE, MISSING, FLAKY, NOW, NONE) = range(16)


def result_was_expected(result, expected_results, test_needs_rebaselining, test_is_skipped):
    """Returns whether we got a result we were expecting.
    Args:
        result: actual result of a test execution
        expected_results: set of results listed in test_expectations
        test_needs_rebaselining: whether test was marked as REBASELINE
        test_is_skipped: whether test was marked as SKIP"""
    if result in expected_results:
        return True
    if result == MISSING and test_needs_rebaselining:
        return True
    if result == SKIP and test_is_skipped:
        return True
    return False


def remove_pixel_failures(expected_results):
    """Returns a copy of the expected results for a test, except that we
    drop any pixel failures and return the remaining expectations. For example,
    if we're not running pixel tests, then tests expected to fail as IMAGE
    will PASS."""
    expected_results = expected_results.copy()
    if IMAGE in expected_results:
        expected_results.remove(IMAGE)
        expected_results.add(PASS)
    if IMAGE_PLUS_TEXT in expected_results:
        expected_results.remove(IMAGE_PLUS_TEXT)
        expected_results.add(TEXT)
    return expected_results


def has_pixel_failures(actual_results):
    return IMAGE in actual_results or IMAGE_PLUS_TEXT in actual_results


# FIXME: Perhas these two routines should be part of the Port instead?
BASELINE_SUFFIX_LIST = ('png', 'wav', 'txt')


def suffixes_for_expectations(expectations):
    suffixes = set()
    if expectations.intersection(set([TEXT, IMAGE_PLUS_TEXT])):
        suffixes.add('txt')
    if expectations.intersection(set([IMAGE, IMAGE_PLUS_TEXT])):
        suffixes.add('png')
    if AUDIO in expectations:
        suffixes.add('wav')
    return set(suffixes)


class ParseError(Exception):
    def __init__(self, warnings):
        super(ParseError, self).__init__()
        self.warnings = warnings

    def __str__(self):
        return '\n'.join(map(str, self.warnings))

    def __repr__(self):
        return 'ParseError(warnings=%s)' % self.warnings


class TestExpectationSerializer(object):
    """Provides means of serializing TestExpectationLine instances."""
    def __init__(self, test_configuration_converter=None):
        self._test_configuration_converter = test_configuration_converter
        self._parsed_expectation_to_string = dict([[parsed_expectation, expectation_string] for expectation_string, parsed_expectation in TestExpectations.EXPECTATIONS.items()])

    def to_string(self, expectation_line, include_modifiers=True, include_expectations=True, include_comment=True):
        if expectation_line.is_invalid():
            return expectation_line.original_string or ''

        if expectation_line.name is None:
            return '' if expectation_line.comment is None else "//%s" % expectation_line.comment

        if self._test_configuration_converter and expectation_line.parsed_bug_modifiers:
            specifiers_list = self._test_configuration_converter.to_specifiers_list(expectation_line.matching_configurations)
            result = []
            for specifiers in specifiers_list:
                modifiers = self._parsed_modifier_string(expectation_line, specifiers)
                expectations = self._parsed_expectations_string(expectation_line)
                result.append(self._format_result(modifiers, expectation_line.name, expectations, expectation_line.comment))
            return "\n".join(result) if result else None

        return self._format_result(" ".join(expectation_line.modifiers),
                                   expectation_line.name,
                                   " ".join(expectation_line.expectations),
                                   expectation_line.comment,
                                   include_modifiers, include_expectations, include_comment)

    def to_csv(self, expectation_line):
        # Note that this doesn't include the comments.
        return '%s,%s,%s' % (expectation_line.name, ' '.join(expectation_line.modifiers), ' '.join(expectation_line.expectations))

    def _parsed_expectations_string(self, expectation_line):
        result = []
        for index in TestExpectations.EXPECTATION_ORDER:
            if index in expectation_line.parsed_expectations:
                result.append(self._parsed_expectation_to_string[index])
        return ' '.join(result)

    def _parsed_modifier_string(self, expectation_line, specifiers):
        assert(self._test_configuration_converter)
        result = []
        if expectation_line.parsed_bug_modifiers:
            result.extend(sorted(expectation_line.parsed_bug_modifiers))
        result.extend(sorted(expectation_line.parsed_modifiers))
        result.extend(self._test_configuration_converter.specifier_sorter().sort_specifiers(specifiers))
        return ' '.join(result)

    @classmethod
    def _format_result(cls, modifiers, name, expectations, comment, include_modifiers=True, include_expectations=True, include_comment=True):
        result = ''
        if include_modifiers:
            result += '%s : ' % modifiers.upper()
        result += name
        if include_expectations:
            result += ' = %s' % expectations.upper()
        if include_comment and comment is not None:
            result += " //%s" % comment
        return result

    @classmethod
    def list_to_string(cls, expectation_lines, test_configuration_converter=None, reconstitute_only_these=None):
        serializer = cls(test_configuration_converter)

        def serialize(expectation_line):
            # If reconstitute_only_these is an empty list, we want to return original_string.
            # So we need to compare reconstitute_only_these to None, not just check if it's falsey.
            if reconstitute_only_these is None or expectation_line in reconstitute_only_these:
                return serializer.to_string(expectation_line)
            return expectation_line.original_string

        def nones_out(expectation_line):
            return expectation_line is not None

        return "\n".join(filter(nones_out, map(serialize, expectation_lines)))


class TestExpectationParser(object):
    """Provides parsing facilities for lines in the test_expectation.txt file."""

    DUMMY_BUG_MODIFIER = "bug_dummy"
    BUG_MODIFIER_PREFIX = 'bug'
    BUG_MODIFIER_REGEX = 'bug\d+'
    REBASELINE_MODIFIER = 'rebaseline'
    PASS_EXPECTATION = 'pass'
    SKIP_MODIFIER = 'skip'
    SLOW_MODIFIER = 'slow'
    WONTFIX_MODIFIER = 'wontfix'

    TIMEOUT_EXPECTATION = 'timeout'

    def __init__(self, port, full_test_list, allow_rebaseline_modifier):
        self._port = port
        self._test_configuration_converter = TestConfigurationConverter(set(port.all_test_configurations()), port.configuration_specifier_macros())
        self._full_test_list = full_test_list
        self._allow_rebaseline_modifier = allow_rebaseline_modifier

    def parse(self, filename, expectations_string):
        expectation_lines = []
        line_number = 0
        for line in expectations_string.split("\n"):
            line_number += 1
            test_expectation = self._tokenize_line(filename, line, line_number)
            self._parse_line(test_expectation)
            expectation_lines.append(test_expectation)
        return expectation_lines

    def expectation_for_skipped_test(self, test_name):
        expectation_line = TestExpectationLine()
        expectation_line.original_string = test_name
        expectation_line.modifiers = [TestExpectationParser.DUMMY_BUG_MODIFIER, TestExpectationParser.SKIP_MODIFIER]
        # FIXME: It's not clear what the expectations for a skipped test should be; the expectations
        # might be different for different entries in a Skipped file, or from the command line, or from
        # only running parts of the tests. It's also not clear if it matters much.
        expectation_line.modifiers.append(TestExpectationParser.WONTFIX_MODIFIER)
        expectation_line.name = test_name
        # FIXME: we should pass in a more descriptive string here.
        expectation_line.filename = '<Skipped file>'
        expectation_line.line_number = 0
        expectation_line.expectations = [TestExpectationParser.PASS_EXPECTATION]
        self._parse_line(expectation_line)
        return expectation_line

    def _parse_line(self, expectation_line):
        if not expectation_line.name:
            return

        expectation_line.is_file = self._port.test_isfile(expectation_line.name)
        if not expectation_line.is_file and self._check_path_does_not_exist(expectation_line):
            return

        if expectation_line.is_file:
            expectation_line.path = expectation_line.name
        else:
            expectation_line.path = self._port.normalize_test_name(expectation_line.name)

        self._collect_matching_tests(expectation_line)

        self._parse_modifiers(expectation_line)
        self._parse_expectations(expectation_line)

    def _parse_modifiers(self, expectation_line):
        has_wontfix = False
        has_bugid = False
        parsed_specifiers = set()

        modifiers = [modifier.lower() for modifier in expectation_line.modifiers]
        expectations = [expectation.lower() for expectation in expectation_line.expectations]

        if self.SLOW_MODIFIER in modifiers and self.TIMEOUT_EXPECTATION in expectations:
            expectation_line.warnings.append('A test can not be both SLOW and TIMEOUT. If it times out indefinitely, then it should be just TIMEOUT.')

        for modifier in modifiers:
            if modifier in TestExpectations.MODIFIERS:
                expectation_line.parsed_modifiers.append(modifier)
                if modifier == self.WONTFIX_MODIFIER:
                    has_wontfix = True
            elif modifier.startswith(self.BUG_MODIFIER_PREFIX):
                has_bugid = True
                if re.match(self.BUG_MODIFIER_REGEX, modifier):
                    expectation_line.warnings.append('BUG\d+ is not allowed, must be one of BUGCR\d+, BUGWK\d+, BUGV8_\d+, or a non-numeric bug identifier.')
                else:
                    expectation_line.parsed_bug_modifiers.append(modifier)
            else:
                parsed_specifiers.add(modifier)

        if not expectation_line.parsed_bug_modifiers and not has_wontfix and not has_bugid:
            expectation_line.warnings.append('Test lacks BUG modifier.')

        if self._allow_rebaseline_modifier and self.REBASELINE_MODIFIER in modifiers:
            expectation_line.warnings.append('REBASELINE should only be used for running rebaseline.py. Cannot be checked in.')

        expectation_line.matching_configurations = self._test_configuration_converter.to_config_set(parsed_specifiers, expectation_line.warnings)

    def _parse_expectations(self, expectation_line):
        result = set()
        for part in expectation_line.expectations:
            expectation = TestExpectations.expectation_from_string(part)
            if expectation is None:  # Careful, PASS is currently 0.
                expectation_line.warnings.append('Unsupported expectation: %s' % part)
                continue
            result.add(expectation)
        expectation_line.parsed_expectations = result

    def _check_path_does_not_exist(self, expectation_line):
        # WebKit's way of skipping tests is to add a -disabled suffix.
        # So we should consider the path existing if the path or the
        # -disabled version exists.
        if (not self._port.test_exists(expectation_line.name)
            and not self._port.test_exists(expectation_line.name + '-disabled')):
            # Log a warning here since you hit this case any
            # time you update TestExpectations without syncing
            # the LayoutTests directory
            expectation_line.warnings.append('Path does not exist.')
            return True
        return False

    def _collect_matching_tests(self, expectation_line):
        """Convert the test specification to an absolute, normalized
        path and make sure directories end with the OS path separator."""
        # FIXME: full_test_list can quickly contain a big amount of
        # elements. We should consider at some point to use a more
        # efficient structure instead of a list. Maybe a dictionary of
        # lists to represent the tree of tests, leaves being test
        # files and nodes being categories.

        if not self._full_test_list:
            expectation_line.matching_tests = [expectation_line.path]
            return

        if not expectation_line.is_file:
            # this is a test category, return all the tests of the category.
            expectation_line.matching_tests = [test for test in self._full_test_list if test.startswith(expectation_line.path)]
            return

        # this is a test file, do a quick check if it's in the
        # full test suite.
        if expectation_line.path in self._full_test_list:
            expectation_line.matching_tests.append(expectation_line.path)

    @classmethod
    def _tokenize_line(cls, filename, expectation_string, line_number):
        """Tokenizes a line from TestExpectations and returns an unparsed TestExpectationLine instance.

        The format of a test expectation line is:

        [[<modifiers>] : <name> = <expectations>][ //<comment>]

        Any errant whitespace is not preserved.

        """
        expectation_line = TestExpectationLine()
        expectation_line.original_string = expectation_string
        expectation_line.line_number = line_number
        expectation_line.filename = filename
        comment_index = expectation_string.find("//")
        if comment_index == -1:
            comment_index = len(expectation_string)
        else:
            expectation_line.comment = expectation_string[comment_index + 2:]

        remaining_string = re.sub(r"\s+", " ", expectation_string[:comment_index].strip())
        if len(remaining_string) == 0:
            return expectation_line

        parts = remaining_string.split(':')
        if len(parts) != 2:
            expectation_line.warnings.append("Missing a ':'" if len(parts) < 2 else "Extraneous ':'")
        else:
            test_and_expectation = parts[1].split('=')
            if len(test_and_expectation) != 2:
                expectation_line.warnings.append("Missing expectations" if len(test_and_expectation) < 2 else "Extraneous '='")

        if not expectation_line.is_invalid():
            expectation_line.modifiers = cls._split_space_separated(parts[0])
            expectation_line.name = test_and_expectation[0].strip()
            expectation_line.expectations = cls._split_space_separated(test_and_expectation[1])

        return expectation_line

    @classmethod
    def _split_space_separated(cls, space_separated_string):
        """Splits a space-separated string into an array."""
        return [part.strip() for part in space_separated_string.strip().split(' ')]


class TestExpectationLine(object):
    """Represents a line in test expectations file."""

    def __init__(self):
        """Initializes a blank-line equivalent of an expectation."""
        self.original_string = None
        self.filename = None  # this is the path to the expectations file for this line
        self.line_number = None
        self.name = None  # this is the path in the line itself
        self.path = None  # this is the normpath of self.name
        self.modifiers = []
        self.parsed_modifiers = []
        self.parsed_bug_modifiers = []
        self.matching_configurations = set()
        self.expectations = []
        self.parsed_expectations = set()
        self.comment = None
        self.matching_tests = []
        self.warnings = []

    def is_invalid(self):
        return len(self.warnings) > 0

    def is_flaky(self):
        return len(self.parsed_expectations) > 1

    @classmethod
    def create_passing_expectation(cls, test):
        expectation_line = TestExpectationLine()
        expectation_line.name = test
        expectation_line.path = test
        expectation_line.parsed_expectations = set([PASS])
        expectation_line.expectations = set(['PASS'])
        expectation_line.matching_tests = [test]
        return expectation_line


# FIXME: Refactor API to be a proper CRUD.
class TestExpectationsModel(object):
    """Represents relational store of all expectations and provides CRUD semantics to manage it."""

    def __init__(self, shorten_filename=None):
        # Maps a test to its list of expectations.
        self._test_to_expectations = {}

        # Maps a test to list of its modifiers (string values)
        self._test_to_modifiers = {}

        # Maps a test to a TestExpectationLine instance.
        self._test_to_expectation_line = {}

        self._modifier_to_tests = self._dict_of_sets(TestExpectations.MODIFIERS)
        self._expectation_to_tests = self._dict_of_sets(TestExpectations.EXPECTATIONS)
        self._timeline_to_tests = self._dict_of_sets(TestExpectations.TIMELINES)
        self._result_type_to_tests = self._dict_of_sets(TestExpectations.RESULT_TYPES)

        self._shorten_filename = shorten_filename or (lambda x: x)

    def _dict_of_sets(self, strings_to_constants):
        """Takes a dict of strings->constants and returns a dict mapping
        each constant to an empty set."""
        d = {}
        for c in strings_to_constants.values():
            d[c] = set()
        return d

    def get_test_set(self, modifier, expectation=None, include_skips=True):
        if expectation is None:
            tests = self._modifier_to_tests[modifier]
        else:
            tests = (self._expectation_to_tests[expectation] &
                self._modifier_to_tests[modifier])

        if not include_skips:
            tests = tests - self.get_test_set(SKIP, expectation)

        return tests

    def get_test_set_for_keyword(self, keyword):
        # FIXME: get_test_set() is an awkward public interface because it requires
        # callers to know the difference between modifiers and expectations. We
        # should replace that with this where possible.
        expectation_enum = TestExpectations.EXPECTATIONS.get(keyword.lower(), None)
        if expectation_enum is not None:
            return self._expectation_to_tests[expectation_enum]
        modifier_enum = TestExpectations.MODIFIERS.get(keyword.lower(), None)
        if modifier_enum is not None:
            return self._modifier_to_tests[modifier_enum]

        # We must not have an index on this modifier.
        matching_tests = set()
        for test, modifiers in self._test_to_modifiers.iteritems():
            if keyword.lower() in modifiers:
                matching_tests.add(test)
        return matching_tests

    def get_tests_with_result_type(self, result_type):
        return self._result_type_to_tests[result_type]

    def get_tests_with_timeline(self, timeline):
        return self._timeline_to_tests[timeline]

    def get_modifiers(self, test):
        """This returns modifiers for the given test (the modifiers plus the BUGXXXX identifier). This is used by the LTTF dashboard."""
        return self._test_to_modifiers[test]

    def has_modifier(self, test, modifier):
        return test in self._modifier_to_tests[modifier]

    def has_keyword(self, test, keyword):
        return (keyword.upper() in self.get_expectations_string(test) or
                keyword.lower() in self.get_modifiers(test))

    def has_test(self, test):
        return test in self._test_to_expectation_line

    def get_expectation_line(self, test):
        return self._test_to_expectation_line.get(test)

    def get_expectations(self, test):
        return self._test_to_expectations[test]

    def get_expectations_string(self, test):
        """Returns the expectatons for the given test as an uppercase string.
        If there are no expectations for the test, then "PASS" is returned."""
        expectations = self.get_expectations(test)
        retval = []

        for expectation in expectations:
            retval.append(self.expectation_to_string(expectation))

        return " ".join(retval)

    def expectation_to_string(self, expectation):
        """Return the uppercased string equivalent of a given expectation."""
        for item in TestExpectations.EXPECTATIONS.items():
            if item[1] == expectation:
                return item[0].upper()
        raise ValueError(expectation)


    def add_expectation_line(self, expectation_line, in_skipped=False):
        """Returns a list of warnings encountered while matching modifiers."""

        if expectation_line.is_invalid():
            return

        for test in expectation_line.matching_tests:
            if not in_skipped and self._already_seen_better_match(test, expectation_line):
                continue

            self._clear_expectations_for_test(test)
            self._test_to_expectation_line[test] = expectation_line
            self._add_test(test, expectation_line)

    def _add_test(self, test, expectation_line):
        """Sets the expected state for a given test.

        This routine assumes the test has not been added before. If it has,
        use _clear_expectations_for_test() to reset the state prior to
        calling this."""
        self._test_to_expectations[test] = expectation_line.parsed_expectations
        for expectation in expectation_line.parsed_expectations:
            self._expectation_to_tests[expectation].add(test)

        self._test_to_modifiers[test] = expectation_line.modifiers
        for modifier in expectation_line.parsed_modifiers:
            mod_value = TestExpectations.MODIFIERS[modifier]
            self._modifier_to_tests[mod_value].add(test)

        if TestExpectationParser.WONTFIX_MODIFIER in expectation_line.parsed_modifiers:
            self._timeline_to_tests[WONTFIX].add(test)
        else:
            self._timeline_to_tests[NOW].add(test)

        if TestExpectationParser.SKIP_MODIFIER in expectation_line.parsed_modifiers:
            self._result_type_to_tests[SKIP].add(test)
        elif expectation_line.parsed_expectations == set([PASS]):
            self._result_type_to_tests[PASS].add(test)
        elif expectation_line.is_flaky():
            self._result_type_to_tests[FLAKY].add(test)
        else:
            # FIXME: What is this?
            self._result_type_to_tests[FAIL].add(test)

    def _clear_expectations_for_test(self, test):
        """Remove prexisting expectations for this test.
        This happens if we are seeing a more precise path
        than a previous listing.
        """
        if self.has_test(test):
            self._test_to_expectations.pop(test, '')
            self._remove_from_sets(test, self._expectation_to_tests)
            self._remove_from_sets(test, self._modifier_to_tests)
            self._remove_from_sets(test, self._timeline_to_tests)
            self._remove_from_sets(test, self._result_type_to_tests)

    def _remove_from_sets(self, test, dict_of_sets_of_tests):
        """Removes the given test from the sets in the dictionary.

        Args:
          test: test to look for
          dict: dict of sets of files"""
        for set_of_tests in dict_of_sets_of_tests.itervalues():
            if test in set_of_tests:
                set_of_tests.remove(test)

    def _already_seen_better_match(self, test, expectation_line):
        """Returns whether we've seen a better match already in the file.

        Returns True if we've already seen a expectation_line.name that matches more of the test
            than this path does
        """
        # FIXME: See comment below about matching test configs and specificity.
        if not self.has_test(test):
            # We've never seen this test before.
            return False

        prev_expectation_line = self._test_to_expectation_line[test]

        if prev_expectation_line.filename != expectation_line.filename:
            # We've moved on to a new expectation file, which overrides older ones.
            return False

        if len(prev_expectation_line.path) > len(expectation_line.path):
            # The previous path matched more of the test.
            return True

        if len(prev_expectation_line.path) < len(expectation_line.path):
            # This path matches more of the test.
            return False

        # At this point we know we have seen a previous exact match on this
        # base path, so we need to check the two sets of modifiers.

        # FIXME: This code was originally designed to allow lines that matched
        # more modifiers to override lines that matched fewer modifiers.
        # However, we currently view these as errors.
        #
        # To use the "more modifiers wins" policy, change the errors for overrides
        # to be warnings and return False".

        if prev_expectation_line.matching_configurations == expectation_line.matching_configurations:
            expectation_line.warnings.append('Duplicate or ambiguous entry for %s on lines %s:%d and %s:%d.' % (expectation_line.name,
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_number,
                self._shorten_filename(expectation_line.filename), expectation_line.line_number))
            return True

        if prev_expectation_line.matching_configurations >= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%d overrides line %s:%d.' % (expectation_line.name,
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_number,
                self._shorten_filename(expectation_line.filename), expectation_line.line_number))
            # FIXME: return False if we want more specific to win.
            return True

        if prev_expectation_line.matching_configurations <= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%d overrides line %s:%d.' % (expectation_line.name,
                self._shorten_filename(expectation_line.filename), expectation_line.line_number,
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_number))
            return True

        if prev_expectation_line.matching_configurations & expectation_line.matching_configurations:
            expectation_line.warnings.append('Entries for %s on lines %s:%d and %s:%d match overlapping sets of configurations.' % (expectation_line.name,
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_number,
                self._shorten_filename(expectation_line.filename), expectation_line.line_number))
            return True

        # Configuration sets are disjoint, then.
        return False


class TestExpectations(object):
    """Test expectations consist of lines with specifications of what
    to expect from layout test cases. The test cases can be directories
    in which case the expectations apply to all test cases in that
    directory and any subdirectory. The format is along the lines of:

      LayoutTests/fast/js/fixme.js = TEXT
      LayoutTests/fast/js/flaky.js = TEXT PASS
      LayoutTests/fast/js/crash.js = CRASH TIMEOUT TEXT PASS
      ...

    To add modifiers:
      SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
      DEBUG : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
      DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
      LINUX DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
      LINUX WIN : LayoutTests/fast/js/no-good.js = TIMEOUT PASS

    SKIP: Doesn't run the test.
    SLOW: The test takes a long time to run, but does not timeout indefinitely.
    WONTFIX: For tests that we never intend to pass on a given platform.

    Notes:
      -A test cannot be both SLOW and TIMEOUT
      -A test should only be one of IMAGE, TEXT, IMAGE+TEXT, or AUDIO.
      -A test can be included twice, but not via the same path.
      -If a test is included twice, then the more precise path wins.
      -CRASH tests cannot be WONTFIX
    """

    EXPECTATIONS = {'pass': PASS,
                    'text': TEXT,
                    'image': IMAGE,
                    'image+text': IMAGE_PLUS_TEXT,
                    'audio': AUDIO,
                    'timeout': TIMEOUT,
                    'crash': CRASH,
                    'missing': MISSING}

    # (aggregated by category, pass/fail/skip, type)
    EXPECTATION_DESCRIPTIONS = {SKIP: ('skipped', 'skipped', ''),
                                PASS: ('passes', 'passed', ''),
                                TEXT: ('text failures', 'failed', ' (text diff)'),
                                IMAGE: ('image-only failures', 'failed', ' (image diff)'),
                                IMAGE_PLUS_TEXT: ('both image and text failures', 'failed', ' (both image and text diffs'),
                                AUDIO: ('audio failures', 'failed', ' (audio diff)'),
                                CRASH: ('crashes', 'crashed', ''),
                                TIMEOUT: ('timeouts', 'timed out', ''),
                                MISSING: ('no expected results found', 'no expected result found', '')}

    EXPECTATION_ORDER = (PASS, CRASH, TIMEOUT, MISSING, IMAGE_PLUS_TEXT, TEXT, IMAGE, AUDIO, SKIP)

    BUILD_TYPES = ('debug', 'release')

    MODIFIERS = {TestExpectationParser.SKIP_MODIFIER: SKIP,
                 TestExpectationParser.WONTFIX_MODIFIER: WONTFIX,
                 TestExpectationParser.SLOW_MODIFIER: SLOW,
                 TestExpectationParser.REBASELINE_MODIFIER: REBASELINE,
                 'none': NONE}

    TIMELINES = {TestExpectationParser.WONTFIX_MODIFIER: WONTFIX,
                 'now': NOW}

    RESULT_TYPES = {'skip': SKIP,
                    'pass': PASS,
                    'fail': FAIL,
                    'flaky': FLAKY}

    @classmethod
    def expectation_from_string(cls, string):
        assert(' ' not in string)  # This only handles one expectation at a time.
        return cls.EXPECTATIONS.get(string.lower())

    def __init__(self, port, tests=None, is_lint_mode=False, include_overrides=True):
        self._full_test_list = tests
        self._test_config = port.test_configuration()
        self._is_lint_mode = is_lint_mode
        self._model = TestExpectationsModel(self._shorten_filename)
        self._parser = TestExpectationParser(port, tests, is_lint_mode)
        self._port = port
        self._skipped_tests_warnings = []

        expectations_dict = port.expectations_dict()
        self._expectations = self._parser.parse(expectations_dict.keys()[0], expectations_dict.values()[0])
        self._add_expectations(self._expectations)

        if len(expectations_dict) > 1 and include_overrides:
            for name in expectations_dict.keys()[1:]:
                expectations = self._parser.parse(name, expectations_dict[name])
                self._add_expectations(expectations)
                self._expectations += expectations

        # FIXME: move ignore_tests into port.skipped_layout_tests()
        self.add_skipped_tests(port.skipped_layout_tests(tests).union(set(port.get_option('ignore_tests', []))))

        self._has_warnings = False
        self._report_warnings()
        self._process_tests_without_expectations()

    # TODO(ojan): Allow for removing skipped tests when getting the list of
    # tests to run, but not when getting metrics.
    def model(self):
        return self._model

    def get_rebaselining_failures(self):
        return (self._model.get_test_set(REBASELINE, IMAGE) |
                self._model.get_test_set(REBASELINE, TEXT) |
                self._model.get_test_set(REBASELINE, IMAGE_PLUS_TEXT) |
                self._model.get_test_set(REBASELINE, AUDIO))

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_expectations(self, test):
        return self._model.get_expectations(test)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def has_modifier(self, test, modifier):
        return self._model.has_modifier(test, modifier)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_tests_with_result_type(self, result_type):
        return self._model.get_tests_with_result_type(result_type)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_test_set(self, modifier, expectation=None, include_skips=True):
        return self._model.get_test_set(modifier, expectation, include_skips)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_modifiers(self, test):
        return self._model.get_modifiers(test)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_tests_with_timeline(self, timeline):
        return self._model.get_tests_with_timeline(timeline)

    def get_expectations_string(self, test):
        return self._model.get_expectations_string(test)

    def expectation_to_string(self, expectation):
        return self._model.expectation_to_string(expectation)

    def matches_an_expected_result(self, test, result, pixel_tests_are_enabled):
        expected_results = self._model.get_expectations(test)
        if not pixel_tests_are_enabled:
            expected_results = remove_pixel_failures(expected_results)
        return result_was_expected(result,
                                   expected_results,
                                   self.is_rebaselining(test),
                                   self._model.has_modifier(test, SKIP))

    def is_rebaselining(self, test):
        return self._model.has_modifier(test, REBASELINE)

    def _shorten_filename(self, filename):
        if filename.startswith(self._port.path_from_webkit_base()):
            return self._port.host.filesystem.relpath(filename, self._port.path_from_webkit_base())
        return filename

    def _report_warnings(self):
        warnings = []
        for expectation in self._expectations:
            for warning in expectation.warnings:
                warnings.append('%s:%d %s %s' % (self._shorten_filename(expectation.filename), expectation.line_number,
                                warning, expectation.name if expectation.expectations else expectation.original_string))

        if warnings:
            self._has_warnings = True
            if self._is_lint_mode:
                raise ParseError(warnings)

    def _process_tests_without_expectations(self):
        if self._full_test_list:
            for test in self._full_test_list:
                if not self._model.has_test(test):
                    self._model.add_expectation_line(TestExpectationLine.create_passing_expectation(test))

    def has_warnings(self):
        return self._has_warnings

    def remove_configuration_from_test(self, test, test_configuration):
        expectations_to_remove = []
        modified_expectations = []

        for expectation in self._expectations:
            if expectation.name != test or expectation.is_flaky() or not expectation.parsed_expectations:
                continue
            if iter(expectation.parsed_expectations).next() not in (TEXT, IMAGE, IMAGE_PLUS_TEXT, AUDIO):
                continue
            if test_configuration not in expectation.matching_configurations:
                continue

            expectation.matching_configurations.remove(test_configuration)
            if expectation.matching_configurations:
                modified_expectations.append(expectation)
            else:
                expectations_to_remove.append(expectation)

        for expectation in expectations_to_remove:
            self._expectations.remove(expectation)

        return TestExpectationSerializer.list_to_string(self._expectations, self._parser._test_configuration_converter, modified_expectations)

    def remove_rebaselined_tests(self, except_these_tests, filename):
        """Returns a copy of the expectations in the file with the tests removed."""
        def without_rebaseline_modifier(expectation):
            return not (not expectation.is_invalid() and
                        expectation.name in except_these_tests and
                        'rebaseline' in expectation.parsed_modifiers and
                        filename == expectation.filename)

        return TestExpectationSerializer.list_to_string(filter(without_rebaseline_modifier, self._expectations))

    def _add_expectations(self, expectation_list):
        for expectation_line in expectation_list:
            if not expectation_line.expectations:
                continue

            if self._is_lint_mode or self._test_config in expectation_line.matching_configurations:
                self._model.add_expectation_line(expectation_line)

    def add_skipped_tests(self, tests_to_skip):
        if not tests_to_skip:
            return
        for test in self._expectations:
            if test.name and test.name in tests_to_skip:
                test.warnings.append('%s:%d %s is also in a Skipped file.' % (test.filename, test.line_number, test.name))

        for test_name in tests_to_skip:
            expectation_line = self._parser.expectation_for_skipped_test(test_name)
            self._model.add_expectation_line(expectation_line, in_skipped=True)
