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
import logging
import re

import webkitpy.thirdparty.simplejson as simplejson

_log = logging.getLogger("webkitpy.layout_tests.layout_package."
                         "test_expectations")

# Test expectation and modifier constants.
(PASS, FAIL, TEXT, IMAGE, IMAGE_PLUS_TEXT, AUDIO, TIMEOUT, CRASH, SKIP, WONTFIX,
 SLOW, REBASELINE, MISSING, FLAKY, NOW, NONE) = range(16)

# Test expectation file update action constants
(NO_CHANGE, REMOVE_TEST, REMOVE_PLATFORM, ADD_PLATFORMS_EXCEPT_THIS) = range(4)


def result_was_expected(result, expected_results, test_needs_rebaselining,
                        test_is_skipped):
    """Returns whether we got a result we were expecting.
    Args:
        result: actual result of a test execution
        expected_results: set of results listed in test_expectations
        test_needs_rebaselining: whether test was marked as REBASELINE
        test_is_skipped: whether test was marked as SKIP"""
    if result in expected_results:
        return True
    if result in (IMAGE, TEXT, IMAGE_PLUS_TEXT) and FAIL in expected_results:
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


class TestExpectations:
    TEST_LIST = "test_expectations.txt"

    def __init__(self, port, tests, expectations, test_config,
                 is_lint_mode, overrides=None):
        """Loads and parses the test expectations given in the string.
        Args:
            port: handle to object containing platform-specific functionality
            tests: list of all of the test files
            expectations: test expectations as a string
            test_config: specific values to check against when
                parsing the file (usually port.test_config(),
                but may be different when linting or doing other things).
            is_lint_mode: If True, just parse the expectations string
                looking for errors.
            overrides: test expectations that are allowed to override any
                entries in |expectations|. This is used by callers
                that need to manage two sets of expectations (e.g., upstream
                and downstream expectations).
        """
        self._expected_failures = TestExpectationsFile(port, expectations,
            tests, test_config, is_lint_mode,
            overrides=overrides)

    # TODO(ojan): Allow for removing skipped tests when getting the list of
    # tests to run, but not when getting metrics.
    # TODO(ojan): Replace the Get* calls here with the more sane API exposed
    # by TestExpectationsFile below. Maybe merge the two classes entirely?

    def get_expectations_json_for_all_platforms(self):
        return (
            self._expected_failures.get_expectations_json_for_all_platforms())

    def get_rebaselining_failures(self):
        return (self._expected_failures.get_test_set(REBASELINE, FAIL) |
                self._expected_failures.get_test_set(REBASELINE, IMAGE) |
                self._expected_failures.get_test_set(REBASELINE, TEXT) |
                self._expected_failures.get_test_set(REBASELINE,
                                                     IMAGE_PLUS_TEXT) |
                self._expected_failures.get_test_set(REBASELINE, AUDIO))

    def get_options(self, test):
        return self._expected_failures.get_options(test)

    def get_expectations(self, test):
        return self._expected_failures.get_expectations(test)

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
        for item in TestExpectationsFile.EXPECTATIONS.items():
            if item[1] == expectation:
                return item[0].upper()
        raise ValueError(expectation)

    def get_tests_with_result_type(self, result_type):
        return self._expected_failures.get_tests_with_result_type(result_type)

    def get_tests_with_timeline(self, timeline):
        return self._expected_failures.get_tests_with_timeline(timeline)

    def matches_an_expected_result(self, test, result,
                                   pixel_tests_are_enabled):
        expected_results = self._expected_failures.get_expectations(test)
        if not pixel_tests_are_enabled:
            expected_results = remove_pixel_failures(expected_results)
        return result_was_expected(result, expected_results,
            self.is_rebaselining(test), self.has_modifier(test, SKIP))

    def is_rebaselining(self, test):
        return self._expected_failures.has_modifier(test, REBASELINE)

    def has_modifier(self, test, modifier):
        return self._expected_failures.has_modifier(test, modifier)

    def remove_rebaselined_tests(self, tests):
        return self._expected_failures.remove_rebaselined_tests(tests)


def strip_comments(line):
    """Strips comments from a line and return None if the line is empty
    or else the contents of line with leading and trailing spaces removed
    and all other whitespace collapsed"""

    commentIndex = line.find('//')
    if commentIndex is -1:
        commentIndex = len(line)

    line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
    if line == '':
        return None
    else:
        return line


class ParseError(Exception):
    def __init__(self, fatal, errors):
        self.fatal = fatal
        self.errors = errors

    def __str__(self):
        return '\n'.join(map(str, self.errors))

    def __repr__(self):
        return 'ParseError(fatal=%s, errors=%s)' % (self.fatal, self.errors)


class ModifiersAndExpectations:
    """A holder for modifiers and expectations on a test that serializes to
    JSON."""

    def __init__(self, modifiers, expectations):
        self.modifiers = modifiers
        self.expectations = expectations


class ExpectationsJsonEncoder(simplejson.JSONEncoder):
    """JSON encoder that can handle ModifiersAndExpectations objects."""
    def default(self, obj):
        # A ModifiersAndExpectations object has two fields, each of which
        # is a dict. Since JSONEncoders handle all the builtin types directly,
        # the only time this routine should be called is on the top level
        # object (i.e., the encoder shouldn't recurse).
        assert isinstance(obj, ModifiersAndExpectations)
        return {"modifiers": obj.modifiers,
                "expectations": obj.expectations}


class TestExpectationsFile:
    """Test expectation files consist of lines with specifications of what
    to expect from layout test cases. The test cases can be directories
    in which case the expectations apply to all test cases in that
    directory and any subdirectory. The format of the file is along the
    lines of:

      LayoutTests/fast/js/fixme.js = FAIL
      LayoutTests/fast/js/flaky.js = FAIL PASS
      LayoutTests/fast/js/crash.js = CRASH TIMEOUT FAIL PASS
      ...

    To add other options:
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
      -A test should only be one of IMAGE, TEXT, IMAGE+TEXT, AUDIO, or FAIL.
       FAIL is a legacy value that currently means either IMAGE,
       TEXT, or IMAGE+TEXT. Once we have finished migrating the expectations,
       we should change FAIL to have the meaning of IMAGE+TEXT and remove the
       IMAGE+TEXT identifier.
      -A test can be included twice, but not via the same path.
      -If a test is included twice, then the more precise path wins.
      -CRASH tests cannot be WONTFIX
    """

    EXPECTATIONS = {'pass': PASS,
                    'fail': FAIL,
                    'text': TEXT,
                    'image': IMAGE,
                    'image+text': IMAGE_PLUS_TEXT,
                    'audio': AUDIO,
                    'timeout': TIMEOUT,
                    'crash': CRASH,
                    'missing': MISSING}

    EXPECTATION_DESCRIPTIONS = {SKIP: ('skipped', 'skipped'),
                                PASS: ('pass', 'passes'),
                                FAIL: ('failure', 'failures'),
                                TEXT: ('text diff mismatch',
                                       'text diff mismatch'),
                                IMAGE: ('image mismatch', 'image mismatch'),
                                IMAGE_PLUS_TEXT: ('image and text mismatch',
                                                  'image and text mismatch'),
                                AUDIO: ('audio mismatch', 'audio mismatch'),
                                CRASH: ('DumpRenderTree crash',
                                        'DumpRenderTree crashes'),
                                TIMEOUT: ('test timed out', 'tests timed out'),
                                MISSING: ('no expected result found',
                                          'no expected results found')}

    EXPECTATION_ORDER = (PASS, CRASH, TIMEOUT, MISSING, IMAGE_PLUS_TEXT,
       TEXT, IMAGE, AUDIO, FAIL, SKIP)

    BUILD_TYPES = ('debug', 'release')

    MODIFIERS = {'skip': SKIP,
                 'wontfix': WONTFIX,
                 'slow': SLOW,
                 'rebaseline': REBASELINE,
                 'none': NONE}

    TIMELINES = {'wontfix': WONTFIX,
                 'now': NOW}

    RESULT_TYPES = {'skip': SKIP,
                    'pass': PASS,
                    'fail': FAIL,
                    'flaky': FLAKY}

    def __init__(self, port, expectations, full_test_list,
                 test_config, is_lint_mode, overrides=None):
        # See argument documentation in TestExpectation(), above.

        self._port = port
        self._fs = port._filesystem
        self._expectations = expectations
        self._full_test_list = full_test_list
        self._test_config = test_config
        self._is_lint_mode = is_lint_mode
        self._overrides = overrides
        self._errors = []
        self._non_fatal_errors = []

        # Maps relative test paths as listed in the expectations file to a
        # list of maps containing modifiers and expectations for each time
        # the test is listed in the expectations file. We use this to
        # keep a representation of the entire list of expectations, even
        # invalid ones.
        self._all_expectations = {}

        # Maps a test to its list of expectations.
        self._test_to_expectations = {}

        # Maps a test to its list of options (string values)
        self._test_to_options = {}

        # Maps a test to its list of modifiers: the constants associated with
        # the options minus any bug or platform strings
        self._test_to_modifiers = {}

        # Maps a test to the base path that it was listed with in the list and
        # the number of matches that base path had.
        self._test_list_paths = {}

        self._modifier_to_tests = self._dict_of_sets(self.MODIFIERS)
        self._expectation_to_tests = self._dict_of_sets(self.EXPECTATIONS)
        self._timeline_to_tests = self._dict_of_sets(self.TIMELINES)
        self._result_type_to_tests = self._dict_of_sets(self.RESULT_TYPES)

        self._read(self._get_iterable_expectations(self._expectations),
                   overrides_allowed=False)

        # List of tests that are in the overrides file (used for checking for
        # duplicates inside the overrides file itself). Note that just because
        # a test is in this set doesn't mean it's necessarily overridding a
        # expectation in the regular expectations; the test might not be
        # mentioned in the regular expectations file at all.
        self._overridding_tests = set()

        if overrides:
            self._read(self._get_iterable_expectations(self._overrides),
                       overrides_allowed=True)

        self._handle_any_read_errors()
        self._process_tests_without_expectations()

    def _handle_any_read_errors(self):
        if len(self._errors) or len(self._non_fatal_errors):
            _log.error("FAILURES FOR %s" % str(self._test_config))

            for error in self._errors:
                _log.error(error)
            for error in self._non_fatal_errors:
                _log.error(error)

            if len(self._errors):
                raise ParseError(fatal=True, errors=self._errors)
            if len(self._non_fatal_errors) and self._is_lint_mode:
                raise ParseError(fatal=False, errors=self._non_fatal_errors)

    def _process_tests_without_expectations(self):
        expectations = set([PASS])
        options = []
        modifiers = []
        num_matches = 0
        if self._full_test_list:
            for test in self._full_test_list:
                if not test in self._test_list_paths:
                    self._add_test(test, modifiers, num_matches, expectations,
                                   options, overrides_allowed=False)

    def _dict_of_sets(self, strings_to_constants):
        """Takes a dict of strings->constants and returns a dict mapping
        each constant to an empty set."""
        d = {}
        for c in strings_to_constants.values():
            d[c] = set()
        return d

    def _get_iterable_expectations(self, expectations_str):
        """Returns an object that can be iterated over. Allows for not caring
        about whether we're iterating over a file or a new-line separated
        string."""
        iterable = [x + "\n" for x in expectations_str.split("\n")]
        # Strip final entry if it's empty to avoid added in an extra
        # newline.
        if iterable[-1] == "\n":
            return iterable[:-1]
        return iterable

    def get_test_set(self, modifier, expectation=None, include_skips=True):
        if expectation is None:
            tests = self._modifier_to_tests[modifier]
        else:
            tests = (self._expectation_to_tests[expectation] &
                self._modifier_to_tests[modifier])

        if not include_skips:
            tests = tests - self.get_test_set(SKIP, expectation)

        return tests

    def get_tests_with_result_type(self, result_type):
        return self._result_type_to_tests[result_type]

    def get_tests_with_timeline(self, timeline):
        return self._timeline_to_tests[timeline]

    def get_options(self, test):
        """This returns the entire set of options for the given test
        (the modifiers plus the BUGXXXX identifier). This is used by the
        LTTF dashboard."""
        return self._test_to_options[test]

    def has_modifier(self, test, modifier):
        return test in self._modifier_to_tests[modifier]

    def get_expectations(self, test):
        return self._test_to_expectations[test]

    def get_expectations_json_for_all_platforms(self):
        # Specify separators in order to get compact encoding.
        return ExpectationsJsonEncoder(separators=(',', ':')).encode(
            self._all_expectations)

    def get_non_fatal_errors(self):
        return self._non_fatal_errors

    def remove_rebaselined_tests(self, tests):
        """Returns a copy of the expectations with the tests removed."""
        lines = []
        for (lineno, line) in enumerate(self._get_iterable_expectations(self._expectations)):
            test, options, _ = self.parse_expectations_line(line, lineno)
            if not (test and test in tests and 'rebaseline' in options):
                lines.append(line)
        return ''.join(lines)

    def parse_expectations_line(self, line, lineno):
        """Parses a line from test_expectations.txt and returns a tuple
        with the test path, options as a list, expectations as a list."""
        line = strip_comments(line)
        if not line:
            return (None, None, None)

        options = []
        if line.find(":") is -1:
            self._add_error(lineno, "Missing a ':'", line)
            return (None, None, None)

        parts = line.split(':')

        # FIXME: verify that there is exactly one colon in the line.

        options = self._get_options_list(parts[0])
        test_and_expectation = parts[1].split('=')
        test = test_and_expectation[0].strip()
        if (len(test_and_expectation) is not 2):
            self._add_error(lineno, "Missing expectations.",
                           test_and_expectation)
            expectations = None
        else:
            expectations = self._get_options_list(test_and_expectation[1])

        return (test, options, expectations)

    def _add_to_all_expectations(self, test, options, expectations):
        # Make all paths unix-style so the dashboard doesn't need to.
        test = test.replace('\\', '/')
        if not test in self._all_expectations:
            self._all_expectations[test] = []
        self._all_expectations[test].append(
            ModifiersAndExpectations(options, expectations))

    def _read(self, expectations, overrides_allowed):
        """For each test in an expectations iterable, generate the
        expectations for it."""
        lineno = 0
        matcher = ModifierMatcher(self._test_config)
        for line in expectations:
            lineno += 1
            self._process_line(line, lineno, matcher, overrides_allowed)

    def _process_line(self, line, lineno, matcher, overrides_allowed):
        test_list_path, options, expectations = \
            self.parse_expectations_line(line, lineno)
        if not expectations:
            return

        self._add_to_all_expectations(test_list_path,
                                        " ".join(options).upper(),
                                        " ".join(expectations).upper())

        num_matches = self._check_options(matcher, options, lineno,
                                          test_list_path)
        if num_matches == ModifierMatcher.NO_MATCH:
            return

        expectations = self._parse_expectations(expectations, lineno,
            test_list_path)

        self._check_options_against_expectations(options, expectations,
            lineno, test_list_path)

        if self._check_path_does_not_exist(lineno, test_list_path):
            return

        if not self._full_test_list:
            tests = [test_list_path]
        else:
            tests = self._expand_tests(test_list_path)

        modifiers = [o for o in options if o in self.MODIFIERS]
        self._add_tests(tests, expectations, test_list_path, lineno,
                        modifiers, num_matches, options, overrides_allowed)

    def _get_options_list(self, listString):
        return [part.strip().lower() for part in listString.strip().split(' ')]

    def _parse_expectations(self, expectations, lineno, test_list_path):
        result = set()
        for part in expectations:
            if not part in self.EXPECTATIONS:
                self._add_error(lineno, 'Unsupported expectation: %s' % part,
                    test_list_path)
                continue
            expectation = self.EXPECTATIONS[part]
            result.add(expectation)
        return result

    def _check_options(self, matcher, options, lineno, test_list_path):
        match_result = self._check_syntax(matcher, options, lineno,
                                          test_list_path)
        self._check_semantics(options, lineno, test_list_path)
        return match_result.num_matches

    def _check_syntax(self, matcher, options, lineno, test_list_path):
        match_result = matcher.match(options)
        for error in match_result.errors:
            self._add_error(lineno, error, test_list_path)
        for warning in match_result.warnings:
            self._log_non_fatal_error(lineno, warning, test_list_path)
        return match_result

    def _check_semantics(self, options, lineno, test_list_path):
        has_wontfix = 'wontfix' in options
        has_bug = False
        for opt in options:
            if opt.startswith('bug'):
                has_bug = True
                if re.match('bug\d+', opt):
                    self._add_error(lineno,
                        'BUG\d+ is not allowed, must be one of '
                        'BUGCR\d+, BUGWK\d+, BUGV8_\d+, '
                        'or a non-numeric bug identifier.', test_list_path)

        if not has_bug and not has_wontfix:
            self._log_non_fatal_error(lineno, 'Test lacks BUG modifier.',
                                      test_list_path)

        if self._is_lint_mode and 'rebaseline' in options:
            self._add_error(lineno,
                'REBASELINE should only be used for running rebaseline.py. '
                'Cannot be checked in.', test_list_path)

    def _check_options_against_expectations(self, options, expectations,
                                            lineno, test_list_path):
        if 'slow' in options and TIMEOUT in expectations:
            self._add_error(lineno,
                'A test can not be both SLOW and TIMEOUT. If it times out '
                'indefinitely, then it should be just TIMEOUT.', test_list_path)

    def _check_path_does_not_exist(self, lineno, test_list_path):
        full_path = self._fs.join(self._port.layout_tests_dir(),
                                  test_list_path)
        full_path = self._fs.normpath(full_path)
        # WebKit's way of skipping tests is to add a -disabled suffix.
            # So we should consider the path existing if the path or the
        # -disabled version exists.
        if (not self._port.path_exists(full_path)
            and not self._port.path_exists(full_path + '-disabled')):
            # Log a non fatal error here since you hit this case any
            # time you update test_expectations.txt without syncing
            # the LayoutTests directory
            self._log_non_fatal_error(lineno, 'Path does not exist.',
                                      test_list_path)
            return True
        return False

    def _expand_tests(self, test_list_path):
        """Convert the test specification to an absolute, normalized
        path and make sure directories end with the OS path separator."""
        # FIXME: full_test_list can quickly contain a big amount of
        # elements. We should consider at some point to use a more
        # efficient structure instead of a list. Maybe a dictionary of
        # lists to represent the tree of tests, leaves being test
        # files and nodes being categories.

        path = self._fs.join(self._port.layout_tests_dir(), test_list_path)
        path = self._fs.normpath(path)
        if self._fs.isdir(path):
            # this is a test category, return all the tests of the category.
            path = self._fs.join(path, '')

            return [test for test in self._full_test_list if test.startswith(path)]

        # this is a test file, do a quick check if it's in the
        # full test suite.
        result = []
        if path in self._full_test_list:
            result = [path, ]
        return result

    def _add_tests(self, tests, expectations, test_list_path, lineno,
                   modifiers, num_matches, options, overrides_allowed):
        for test in tests:
            if self._already_seen_better_match(test, test_list_path,
                num_matches, lineno, overrides_allowed):
                continue

            self._clear_expectations_for_test(test, test_list_path)
            self._test_list_paths[test] = (self._fs.normpath(test_list_path),
                num_matches, lineno)
            self._add_test(test, modifiers, num_matches, expectations, options,
                           overrides_allowed)

    def _add_test(self, test, modifiers, num_matches, expectations, options,
                  overrides_allowed):
        """Sets the expected state for a given test.

        This routine assumes the test has not been added before. If it has,
        use _clear_expectations_for_test() to reset the state prior to
        calling this.

        Args:
          test: test to add
          modifiers: sequence of modifier keywords ('wontfix', 'slow', etc.)
          num_matches: number of modifiers that matched the configuration
          expectations: sequence of expectations (PASS, IMAGE, etc.)
          options: sequence of keywords and bug identifiers.
          overrides_allowed: whether we're parsing the regular expectations
              or the overridding expectations"""
        self._test_to_expectations[test] = expectations
        for expectation in expectations:
            self._expectation_to_tests[expectation].add(test)

        self._test_to_options[test] = options
        self._test_to_modifiers[test] = set()
        for modifier in modifiers:
            mod_value = self.MODIFIERS[modifier]
            self._modifier_to_tests[mod_value].add(test)
            self._test_to_modifiers[test].add(mod_value)

        if 'wontfix' in modifiers:
            self._timeline_to_tests[WONTFIX].add(test)
        else:
            self._timeline_to_tests[NOW].add(test)

        if 'skip' in modifiers:
            self._result_type_to_tests[SKIP].add(test)
        elif expectations == set([PASS]):
            self._result_type_to_tests[PASS].add(test)
        elif len(expectations) > 1:
            self._result_type_to_tests[FLAKY].add(test)
        else:
            self._result_type_to_tests[FAIL].add(test)

        if overrides_allowed:
            self._overridding_tests.add(test)

    def _clear_expectations_for_test(self, test, test_list_path):
        """Remove prexisting expectations for this test.
        This happens if we are seeing a more precise path
        than a previous listing.
        """
        if test in self._test_list_paths:
            self._test_to_expectations.pop(test, '')
            self._remove_from_sets(test, self._expectation_to_tests)
            self._remove_from_sets(test, self._modifier_to_tests)
            self._remove_from_sets(test, self._timeline_to_tests)
            self._remove_from_sets(test, self._result_type_to_tests)

        self._test_list_paths[test] = self._fs.normpath(test_list_path)

    def _remove_from_sets(self, test, dict):
        """Removes the given test from the sets in the dictionary.

        Args:
          test: test to look for
          dict: dict of sets of files"""
        for set_of_tests in dict.itervalues():
            if test in set_of_tests:
                set_of_tests.remove(test)

    def _already_seen_better_match(self, test, test_list_path, num_matches,
                                   lineno, overrides_allowed):
        """Returns whether we've seen a better match already in the file.

        Returns True if we've already seen a test_list_path that matches more of the test
            than this path does
        """
        # FIXME: See comment below about matching test configs and num_matches.

        if not test in self._test_list_paths:
            # We've never seen this test before.
            return False

        prev_base_path, prev_num_matches, prev_lineno = self._test_list_paths[test]
        base_path = self._fs.normpath(test_list_path)

        if len(prev_base_path) > len(base_path):
            # The previous path matched more of the test.
            return True

        if len(prev_base_path) < len(base_path):
            # This path matches more of the test.
            return False

        if overrides_allowed and test not in self._overridding_tests:
            # We have seen this path, but that's okay because it is
            # in the overrides and the earlier path was in the
            # expectations (not the overrides).
            return False

        # At this point we know we have seen a previous exact match on this
        # base path, so we need to check the two sets of modifiers.

        if overrides_allowed:
            expectation_source = "override"
        else:
            expectation_source = "expectation"

        # FIXME: This code was originally designed to allow lines that matched
        # more modifiers to override lines that matched fewer modifiers.
        # However, we currently view these as errors. If we decide to make
        # this policy permanent, we can probably simplify this code
        # and the ModifierMatcher code a fair amount.
        #
        # To use the "more modifiers wins" policy, change the "_add_error" lines for overrides
        # to _log_non_fatal_error() and change the commented-out "return False".

        if prev_num_matches == num_matches:
            self._add_error(lineno,
                'Duplicate or ambiguous %s.' % expectation_source,
                test)
            return True

        if prev_num_matches < num_matches:
            self._add_error(lineno,
                'More specific entry on line %d overrides line %d' %
                (lineno, prev_lineno), test_list_path)
            # FIXME: return False if we want more specific to win.
            return True

        self._add_error(lineno,
            'More specific entry on line %d overrides line %d' %
            (prev_lineno, lineno), test_list_path)
        return True

    def _add_error(self, lineno, msg, path):
        """Reports an error that will prevent running the tests. Does not
        immediately raise an exception because we'd like to aggregate all the
        errors so they can all be printed out."""
        self._errors.append('Line:%s %s %s' % (lineno, msg, path))

    def _log_non_fatal_error(self, lineno, msg, path):
        """Reports an error that will not prevent running the tests. These are
        still errors, but not bad enough to warrant breaking test running."""
        self._non_fatal_errors.append('Line:%s %s %s' % (lineno, msg, path))


class ModifierMatchResult(object):
    def __init__(self, options):
        self.num_matches = ModifierMatcher.NO_MATCH
        self.options = options
        self.errors = []
        self.warnings = []
        self.modifiers = []
        self._matched_regexes = set()
        self._matched_macros = set()


class ModifierMatcher(object):

    """
    This class manages the interpretation of the "modifiers" for a given
    line in the expectations file. Modifiers are the tokens that appear to the
    left of the colon on a line. For example, "BUG1234", "DEBUG", and "WIN" are
    all modifiers. This class gets what the valid modifiers are, and which
    modifiers are allowed to exist together on a line, from the
    TestConfiguration object that is passed in to the call.

    This class detects *intra*-line errors like unknown modifiers, but
    does not detect *inter*-line modifiers like duplicate expectations.

    More importantly, this class is also used to determine if a given line
    matches the port in question. Matches are ranked according to the number
    of modifiers that match on a line. A line with no modifiers matches
    everything and has a score of zero. A line with one modifier matches only
    ports that have that modifier and gets a score of 1, and so one. Ports
    that don't match at all get a score of -1.

    Given two lines in a file that apply to the same test, if both expectations
    match the current config, then the expectation is considered ambiguous,
    even if one expectation matches more of the config than the other. For
    example, in:

    BUG1 RELEASE : foo.html = FAIL
    BUG1 WIN RELEASE : foo.html = PASS
    BUG2 WIN : bar.html = FAIL
    BUG2 DEBUG : bar.html = PASS

    lines 1 and 2 would produce an error on a Win XP Release bot (the scores
    would be 1 and 2, respectively), and lines three and four would produce
    a duplicate expectation on a Win Debug bot since both the 'win' and the
    'debug' expectations would apply (both had scores of 1).

    In addition to the definitions of all of the modifiers, the class
    supports "macros" that are expanded prior to interpretation, and "ignore
    regexes" that can be used to skip over modifiers like the BUG* modifiers.
    """
    MACROS = {
        'mac-snowleopard': ['mac', 'snowleopard'],
        'mac-leopard': ['mac', 'leopard'],
        'win-xp': ['win', 'xp'],
        'win-vista': ['win', 'vista'],
        'win-win7': ['win', 'win7'],
    }

    # We don't include the "none" modifier because it isn't actually legal.
    REGEXES_TO_IGNORE = (['bug\w+'] +
                         TestExpectationsFile.MODIFIERS.keys()[:-1])
    DUPLICATE_REGEXES_ALLOWED = ['bug\w+']

    # Magic value returned when the options don't match.
    NO_MATCH = -1

    # FIXME: The code currently doesn't detect combinations of modifiers
    # that are syntactically valid but semantically invalid, like
    # 'MAC XP'. See ModifierMatchTest.test_invalid_combinations() in the
    # _unittest.py file.

    def __init__(self, test_config):
        """Initialize a ModifierMatcher argument with the TestConfiguration it
        should be matched against."""
        self.test_config = test_config
        self.allowed_configurations = test_config.all_test_configurations()
        self.macros = self.MACROS

        self.regexes_to_ignore = {}
        for regex_str in self.REGEXES_TO_IGNORE:
            self.regexes_to_ignore[regex_str] = re.compile(regex_str)

        # Keep a set of all of the legal modifiers for quick checking.
        self._all_modifiers = set()

        # Keep a dict mapping values back to their categories.
        self._categories_for_modifiers = {}
        for config in self.allowed_configurations:
            for category, modifier in config.items():
                self._categories_for_modifiers[modifier] = category
                self._all_modifiers.add(modifier)

    def match(self, options):
        """Checks a list of options against the config set in the constructor.
        Options may be either actual modifier strings, "macro" strings
        that get expanded to a list of modifiers, or strings that are allowed
        to be ignored. All of the options must be passed in in lower case.

        Returns the number of matching categories, or NO_MATCH (-1) if it
        doesn't match or there were errors found. Matches are prioritized
        by the number of matching categories, because the more specific
        the options list, the more categories will match.

        The results of the most recent match are available in the 'options',
        'modifiers', 'num_matches', 'errors', and 'warnings' properties.
        """
        result = ModifierMatchResult(options)
        self._parse(result)
        if result.errors:
            return result
        self._count_matches(result)
        return result

    def _parse(self, result):
        # FIXME: Should we warn about lines having every value in a category?
        for option in result.options:
            self._parse_one(option, result)

    def _parse_one(self, option, result):
        if option in self._all_modifiers:
            self._add_modifier(option, result)
        elif option in self.macros:
            self._expand_macro(option, result)
        elif not self._matches_any_regex(option, result):
            result.errors.append("Unrecognized option '%s'" % option)

    def _add_modifier(self, option, result):
        if option in result.modifiers:
            result.errors.append("More than one '%s'" % option)
        else:
            result.modifiers.append(option)

    def _expand_macro(self, macro, result):
        if macro in result._matched_macros:
            result.errors.append("More than one '%s'" % macro)
            return

        mods = []
        for modifier in self.macros[macro]:
            if modifier in result.options:
                result.errors.append("Can't specify both modifier '%s' and "
                                     "macro '%s'" % (modifier, macro))
            else:
                mods.append(modifier)
        result._matched_macros.add(macro)
        result.modifiers.extend(mods)

    def _matches_any_regex(self, option, result):
        for regex_str, pattern in self.regexes_to_ignore.iteritems():
            if pattern.match(option):
                self._handle_regex_match(regex_str, result)
                return True
        return False

    def _handle_regex_match(self, regex_str, result):
        if (regex_str in result._matched_regexes and
            regex_str not in self.DUPLICATE_REGEXES_ALLOWED):
            result.errors.append("More than one option matching '%s'" %
                                 regex_str)
        else:
            result._matched_regexes.add(regex_str)

    def _count_matches(self, result):
        """Returns the number of modifiers that match the test config."""
        categorized_modifiers = self._group_by_category(result.modifiers)
        result.num_matches = 0
        for category, modifier in self.test_config.items():
            if category in categorized_modifiers:
                if modifier in categorized_modifiers[category]:
                    result.num_matches += 1
                else:
                    result.num_matches = self.NO_MATCH
                    return

    def _group_by_category(self, modifiers):
        # Returns a dict of category name -> list of modifiers.
        modifiers_by_category = {}
        for m in modifiers:
            modifiers_by_category.setdefault(self._category(m), []).append(m)
        return modifiers_by_category

    def _category(self, modifier):
        return self._categories_for_modifiers[modifier]
