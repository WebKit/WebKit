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
import os
import re
import sys
import time

import simplejson

# Test expectation and modifier constants.
(PASS, FAIL, TEXT, IMAGE, IMAGE_PLUS_TEXT, TIMEOUT, CRASH, SKIP, WONTFIX,
 DEFER, SLOW, REBASELINE, MISSING, FLAKY, NOW, NONE) = range(16)

# Test expectation file update action constants
(NO_CHANGE, REMOVE_TEST, REMOVE_PLATFORM, ADD_PLATFORMS_EXCEPT_THIS) = range(4)


class TestExpectations:
    TEST_LIST = "test_expectations.txt"

    def __init__(self, port, tests, expectations, test_platform_name,
                 is_debug_mode, is_lint_mode, tests_are_present=True):
        """Loads and parses the test expectations given in the string.
        Args:
            port: handle to object containing platform-specific functionality
            test: list of all of the test files
            expectations: test expectations as a string
            test_platform_name: name of the platform to match expectations
                against. Note that this may be different than
                port.test_platform_name() when is_lint_mode is True.
            is_debug_mode: whether to use the DEBUG or RELEASE modifiers
                in the expectations
            is_lint_mode: If True, just parse the expectations string
                looking for errors.
            tests_are_present: whether the test files exist in the file
                system and can be probed for. This is useful for distinguishing
                test files from directories, and is needed by the LTTF
                dashboard, where the files aren't actually locally present.
        """
        self._expected_failures = TestExpectationsFile(port, expectations,
            tests, test_platform_name, is_debug_mode, is_lint_mode,
            tests_are_present=tests_are_present)

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
                                                     IMAGE_PLUS_TEXT))

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
            for item in TestExpectationsFile.EXPECTATIONS.items():
                if item[1] == expectation:
                    retval.append(item[0])
                    break

        return " ".join(retval).upper()

    def get_timeline_for_test(self, test):
        return self._expected_failures.get_timeline_for_test(test)

    def get_tests_with_result_type(self, result_type):
        return self._expected_failures.get_tests_with_result_type(result_type)

    def get_tests_with_timeline(self, timeline):
        return self._expected_failures.get_tests_with_timeline(timeline)

    def matches_an_expected_result(self, test, result):
        """Returns whether we got one of the expected results for this test."""
        return (result in self._expected_failures.get_expectations(test) or
                (result in (IMAGE, TEXT, IMAGE_PLUS_TEXT) and
                FAIL in self._expected_failures.get_expectations(test)) or
                result == MISSING and self.is_rebaselining(test) or
                result == SKIP and self._expected_failures.has_modifier(test,
                                                                        SKIP))

    def is_rebaselining(self, test):
        return self._expected_failures.has_modifier(test, REBASELINE)

    def has_modifier(self, test, modifier):
        return self._expected_failures.has_modifier(test, modifier)

    def remove_platform_from_file(self, tests, platform, backup=False):
        return self._expected_failures.remove_platform_from_file(tests,
                                                                 platform,
                                                                 backup)


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


class ModifiersAndExpectations:
    """A holder for modifiers and expectations on a test that serializes to
    JSON."""

    def __init__(self, modifiers, expectations):
        self.modifiers = modifiers
        self.expectations = expectations


class ExpectationsJsonEncoder(simplejson.JSONEncoder):
    """JSON encoder that can handle ModifiersAndExpectations objects.
    """

    def default(self, obj):
        if isinstance(obj, ModifiersAndExpectations):
            return {"modifiers": obj.modifiers,
                    "expectations": obj.expectations}
        else:
            return JSONEncoder.default(self, obj)


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
      DEFER LINUX WIN : LayoutTests/fast/js/no-good.js = TIMEOUT PASS

    SKIP: Doesn't run the test.
    SLOW: The test takes a long time to run, but does not timeout indefinitely.
    WONTFIX: For tests that we never intend to pass on a given platform.
    DEFER: Test does not count in our statistics for the current release.
    DEBUG: Expectations apply only to the debug build.
    RELEASE: Expectations apply only to release build.
    LINUX/WIN/WIN-XP/WIN-VISTA/WIN-7/MAC: Expectations apply only to these
        platforms.

    Notes:
      -A test cannot be both SLOW and TIMEOUT
      -A test cannot be both DEFER and WONTFIX
      -A test should only be one of IMAGE, TEXT, IMAGE+TEXT, or FAIL. FAIL is
       a migratory state that currently means either IMAGE, TEXT, or
       IMAGE+TEXT. Once we have finished migrating the expectations, we will
       change FAIL to have the meaning of IMAGE+TEXT and remove the IMAGE+TEXT
       identifier.
      -A test can be included twice, but not via the same path.
      -If a test is included twice, then the more precise path wins.
      -CRASH tests cannot be DEFER or WONTFIX
    """

    EXPECTATIONS = {'pass': PASS,
                    'fail': FAIL,
                    'text': TEXT,
                    'image': IMAGE,
                    'image+text': IMAGE_PLUS_TEXT,
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
                                CRASH: ('test shell crash',
                                        'test shell crashes'),
                                TIMEOUT: ('test timed out', 'tests timed out'),
                                MISSING: ('no expected result found',
                                          'no expected results found')}

    EXPECTATION_ORDER = (PASS, CRASH, TIMEOUT, MISSING, IMAGE_PLUS_TEXT,
       TEXT, IMAGE, FAIL, SKIP)

    BUILD_TYPES = ('debug', 'release')

    MODIFIERS = {'skip': SKIP,
                 'wontfix': WONTFIX,
                 'defer': DEFER,
                 'slow': SLOW,
                 'rebaseline': REBASELINE,
                 'none': NONE}

    TIMELINES = {'wontfix': WONTFIX,
                 'now': NOW,
                 'defer': DEFER}

    RESULT_TYPES = {'skip': SKIP,
                    'pass': PASS,
                    'fail': FAIL,
                    'flaky': FLAKY}

    def __init__(self, port, expectations, full_test_list, test_platform_name,
        is_debug_mode, is_lint_mode, suppress_errors=False,
        tests_are_present=True):
        """
        expectations: Contents of the expectations file
        full_test_list: The list of all tests to be run pending processing of
            the expections for those tests.
        test_platform_name: name of the platform to match expectations
            against. Note that this may be different than
            port.test_platform_name() when is_lint_mode is True.
        is_debug_mode: Whether we testing a test_shell built debug mode.
        is_lint_mode: Whether this is just linting test_expecatations.txt.
        suppress_errors: Whether to suppress lint errors.
        tests_are_present: Whether the test files are present in the local
            filesystem. The LTTF Dashboard uses False here to avoid having to
            keep a local copy of the tree.
        """

        self._port = port
        self._expectations = expectations
        self._full_test_list = full_test_list
        self._test_platform_name = test_platform_name
        self._is_debug_mode = is_debug_mode
        self._is_lint_mode = is_lint_mode
        self._tests_are_present = tests_are_present
        self._suppress_errors = suppress_errors
        self._errors = []
        self._non_fatal_errors = []

        # Maps relative test paths as listed in the expectations file to a
        # list of maps containing modifiers and expectations for each time
        # the test is listed in the expectations file.
        self._all_expectations = {}

        # Maps a test to its list of expectations.
        self._test_to_expectations = {}

        # Maps a test to its list of options (string values)
        self._test_to_options = {}

        # Maps a test to its list of modifiers: the constants associated with
        # the options minus any bug or platform strings
        self._test_to_modifiers = {}

        # Maps a test to the base path that it was listed with in the list.
        self._test_list_paths = {}

        self._modifier_to_tests = self._dict_of_sets(self.MODIFIERS)
        self._expectation_to_tests = self._dict_of_sets(self.EXPECTATIONS)
        self._timeline_to_tests = self._dict_of_sets(self.TIMELINES)
        self._result_type_to_tests = self._dict_of_sets(self.RESULT_TYPES)

        self._read(self._get_iterable_expectations())

    def _dict_of_sets(self, strings_to_constants):
        """Takes a dict of strings->constants and returns a dict mapping
        each constant to an empty set."""
        d = {}
        for c in strings_to_constants.values():
            d[c] = set()
        return d

    def _get_iterable_expectations(self):
        """Returns an object that can be iterated over. Allows for not caring
        about whether we're iterating over a file or a new-line separated
        string."""
        iterable = [x + "\n" for x in
            self._expectations.split("\n")]
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

    def contains(self, test):
        return test in self._test_to_expectations

    def remove_platform_from_file(self, tests, platform, backup=False):
        """Remove the platform option from test expectations file.

        If a test is in the test list and has an option that matches the given
        platform, remove the matching platform and save the updated test back
        to the file. If no other platforms remaining after removal, delete the
        test from the file.

        Args:
          tests: list of tests that need to update..
          platform: which platform option to remove.
          backup: if true, the original test expectations file is saved as
                  [self.TEST_LIST].orig.YYYYMMDDHHMMSS

        Returns:
          no
        """

        # FIXME - remove_platform_from file worked by writing a new
        # test_expectations.txt file over the old one. Now that we're just
        # parsing strings, we need to change this to return the new
        # expectations string.
        raise NotImplementedException('remove_platform_from_file')

        new_file = self._path + '.new'
        logging.debug('Original file: "%s"', self._path)
        logging.debug('New file: "%s"', new_file)
        f_orig = self._get_iterable_expectations()
        f_new = open(new_file, 'w')

        tests_removed = 0
        tests_updated = 0
        lineno = 0
        for line in f_orig:
            lineno += 1
            action = self._get_platform_update_action(line, lineno, tests,
                                                      platform)
            if action == NO_CHANGE:
                # Save the original line back to the file
                logging.debug('No change to test: %s', line)
                f_new.write(line)
            elif action == REMOVE_TEST:
                tests_removed += 1
                logging.info('Test removed: %s', line)
            elif action == REMOVE_PLATFORM:
                parts = line.split(':')
                new_options = parts[0].replace(platform.upper() + ' ', '', 1)
                new_line = ('%s:%s' % (new_options, parts[1]))
                f_new.write(new_line)
                tests_updated += 1
                logging.info('Test updated: ')
                logging.info('  old: %s', line)
                logging.info('  new: %s', new_line)
            elif action == ADD_PLATFORMS_EXCEPT_THIS:
                parts = line.split(':')
                new_options = parts[0]
                for p in self._port.test_platform_names():
                    p = p.upper()
                    # This is a temp solution for rebaselining tool.
                    # Do not add tags WIN-7 and WIN-VISTA to test expectations
                    # if the original line does not specify the platform
                    # option.
                    # TODO(victorw): Remove WIN-VISTA and WIN-7 once we have
                    # reliable Win 7 and Win Vista buildbots setup.
                    if not p in (platform.upper(), 'WIN-VISTA', 'WIN-7'):
                        new_options += p + ' '
                new_line = ('%s:%s' % (new_options, parts[1]))
                f_new.write(new_line)
                tests_updated += 1
                logging.info('Test updated: ')
                logging.info('  old: %s', line)
                logging.info('  new: %s', new_line)
            else:
                logging.error('Unknown update action: %d; line: %s',
                              action, line)

        logging.info('Total tests removed: %d', tests_removed)
        logging.info('Total tests updated: %d', tests_updated)

        f_orig.close()
        f_new.close()

        if backup:
            date_suffix = time.strftime('%Y%m%d%H%M%S',
                                        time.localtime(time.time()))
            backup_file = ('%s.orig.%s' % (self._path, date_suffix))
            if os.path.exists(backup_file):
                os.remove(backup_file)
            logging.info('Saving original file to "%s"', backup_file)
            os.rename(self._path, backup_file)
        else:
            os.remove(self._path)

        logging.debug('Saving new file to "%s"', self._path)
        os.rename(new_file, self._path)
        return True

    def parse_expectations_line(self, line, lineno):
        """Parses a line from test_expectations.txt and returns a tuple
        with the test path, options as a list, expectations as a list."""
        line = strip_comments(line)
        if not line:
            return (None, None, None)

        options = []
        if line.find(":") is -1:
            test_and_expectation = line.split("=")
        else:
            parts = line.split(":")
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

    def _get_platform_update_action(self, line, lineno, tests, platform):
        """Check the platform option and return the action needs to be taken.

        Args:
          line: current line in test expectations file.
          lineno: current line number of line
          tests: list of tests that need to update..
          platform: which platform option to remove.

        Returns:
          NO_CHANGE: no change to the line (comments, test not in the list etc)
          REMOVE_TEST: remove the test from file.
          REMOVE_PLATFORM: remove this platform option from the test.
          ADD_PLATFORMS_EXCEPT_THIS: add all the platforms except this one.
        """
        test, options, expectations = self.parse_expectations_line(line,
                                                                   lineno)
        if not test or test not in tests:
            return NO_CHANGE

        has_any_platform = False
        for option in options:
            if option in self._port.test_platform_names():
                has_any_platform = True
                if not option == platform:
                    return REMOVE_PLATFORM

        # If there is no platform specified, then it means apply to all
        # platforms. Return the action to add all the platforms except this
        # one.
        if not has_any_platform:
            return ADD_PLATFORMS_EXCEPT_THIS

        return REMOVE_TEST

    def _has_valid_modifiers_for_current_platform(self, options, lineno,
        test_and_expectations, modifiers):
        """Returns true if the current platform is in the options list or if
        no platforms are listed and if there are no fatal errors in the
        options list.

        Args:
          options: List of lowercase options.
          lineno: The line in the file where the test is listed.
          test_and_expectations: The path and expectations for the test.
          modifiers: The set to populate with modifiers.
        """
        has_any_platform = False
        has_bug_id = False
        for option in options:
            if option in self.MODIFIERS:
                modifiers.add(option)
            elif option in self._port.test_platform_names():
                has_any_platform = True
            elif option.startswith('bug'):
                has_bug_id = True
            elif option not in self.BUILD_TYPES:
                self._add_error(lineno, 'Invalid modifier for test: %s' %
                                option, test_and_expectations)

        if has_any_platform and not self._match_platform(options):
            return False

        if not has_bug_id and 'wontfix' not in options:
            # TODO(ojan): Turn this into an AddError call once all the
            # tests have BUG identifiers.
            self._log_non_fatal_error(lineno, 'Test lacks BUG modifier.',
                test_and_expectations)

        if 'release' in options or 'debug' in options:
            if self._is_debug_mode and 'debug' not in options:
                return False
            if not self._is_debug_mode and 'release' not in options:
                return False

        if 'wontfix' in options and 'defer' in options:
            self._add_error(lineno, 'Test cannot be both DEFER and WONTFIX.',
                test_and_expectations)

        if self._is_lint_mode and 'rebaseline' in options:
            self._add_error(lineno,
                'REBASELINE should only be used for running rebaseline.py. '
                'Cannot be checked in.', test_and_expectations)

        return True

    def _match_platform(self, options):
        """Match the list of options against our specified platform. If any
        of the options prefix-match self._platform, return True. This handles
        the case where a test is marked WIN and the platform is WIN-VISTA.

        Args:
          options: list of options
        """
        for opt in options:
            if self._test_platform_name.startswith(opt):
                return True
        return False

    def _add_to_all_expectations(self, test, options, expectations):
        # Make all paths unix-style so the dashboard doesn't need to.
        test = test.replace('\\', '/')
        if not test in self._all_expectations:
            self._all_expectations[test] = []
        self._all_expectations[test].append(
            ModifiersAndExpectations(options, expectations))

    def _read(self, expectations):
        """For each test in an expectations iterable, generate the
        expectations for it."""
        lineno = 0
        for line in expectations:
            lineno += 1

            test_list_path, options, expectations = \
                self.parse_expectations_line(line, lineno)
            if not expectations:
                continue

            self._add_to_all_expectations(test_list_path,
                                          " ".join(options).upper(),
                                          " ".join(expectations).upper())

            modifiers = set()
            if options and not self._has_valid_modifiers_for_current_platform(
                options, lineno, test_list_path, modifiers):
                continue

            expectations = self._parse_expectations(expectations, lineno,
                test_list_path)

            if 'slow' in options and TIMEOUT in expectations:
                self._add_error(lineno,
                    'A test can not be both slow and timeout. If it times out '
                    'indefinitely, then it should be just timeout.',
                    test_list_path)

            full_path = os.path.join(self._port.layout_tests_dir(),
                                     test_list_path)
            full_path = os.path.normpath(full_path)
            # WebKit's way of skipping tests is to add a -disabled suffix.
            # So we should consider the path existing if the path or the
            # -disabled version exists.
            if (self._tests_are_present and not os.path.exists(full_path)
                and not os.path.exists(full_path + '-disabled')):
                # Log a non fatal error here since you hit this case any
                # time you update test_expectations.txt without syncing
                # the LayoutTests directory
                self._log_non_fatal_error(lineno, 'Path does not exist.',
                                       test_list_path)
                continue

            if not self._full_test_list:
                tests = [test_list_path]
            else:
                tests = self._expand_tests(test_list_path)

            self._add_tests(tests, expectations, test_list_path, lineno,
                           modifiers, options)

        if not self._suppress_errors and (
            len(self._errors) or len(self._non_fatal_errors)):
            if self._is_debug_mode:
                build_type = 'DEBUG'
            else:
                build_type = 'RELEASE'
            print "\nFAILURES FOR PLATFORM: %s, BUILD_TYPE: %s" \
                % (self._test_platform_name.upper(), build_type)

            for error in self._non_fatal_errors:
                logging.error(error)
            if len(self._errors):
                raise SyntaxError('\n'.join(map(str, self._errors)))

        # Now add in the tests that weren't present in the expectations file
        expectations = set([PASS])
        options = []
        modifiers = []
        if self._full_test_list:
            for test in self._full_test_list:
                if not test in self._test_list_paths:
                    self._add_test(test, modifiers, expectations, options)

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

    def _expand_tests(self, test_list_path):
        """Convert the test specification to an absolute, normalized
        path and make sure directories end with the OS path separator."""
        path = os.path.join(self._port.layout_tests_dir(), test_list_path)
        path = os.path.normpath(path)
        path = self._fix_dir(path)

        result = []
        for test in self._full_test_list:
            if test.startswith(path):
                result.append(test)
        return result

    def _fix_dir(self, path):
        """Check to see if the path points to a directory, and if so, append
        the directory separator if necessary."""
        if self._tests_are_present:
            if os.path.isdir(path):
                path = os.path.join(path, '')
        else:
            # If we can't check the filesystem to see if this is a directory,
            # we assume that files w/o an extension are directories.
            # TODO(dpranke): What happens w/ LayoutTests/css2.1 ?
            if os.path.splitext(path)[1] == '':
                path = os.path.join(path, '')
        return path

    def _add_tests(self, tests, expectations, test_list_path, lineno,
                   modifiers, options):
        for test in tests:
            if self._already_seen_test(test, test_list_path, lineno):
                continue

            self._clear_expectations_for_test(test, test_list_path)
            self._add_test(test, modifiers, expectations, options)

    def _add_test(self, test, modifiers, expectations, options):
        """Sets the expected state for a given test.

        This routine assumes the test has not been added before. If it has,
        use _ClearExpectationsForTest() to reset the state prior to
        calling this.

        Args:
          test: test to add
          modifiers: sequence of modifier keywords ('wontfix', 'slow', etc.)
          expectations: sequence of expectations (PASS, IMAGE, etc.)
          options: sequence of keywords and bug identifiers."""
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
        elif 'defer' in modifiers:
            self._timeline_to_tests[DEFER].add(test)
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

        self._test_list_paths[test] = os.path.normpath(test_list_path)

    def _remove_from_sets(self, test, dict):
        """Removes the given test from the sets in the dictionary.

        Args:
          test: test to look for
          dict: dict of sets of files"""
        for set_of_tests in dict.itervalues():
            if test in set_of_tests:
                set_of_tests.remove(test)

    def _already_seen_test(self, test, test_list_path, lineno):
        """Returns true if we've already seen a more precise path for this test
        than the test_list_path.
        """
        if not test in self._test_list_paths:
            return False

        prev_base_path = self._test_list_paths[test]
        if (prev_base_path == os.path.normpath(test_list_path)):
            self._add_error(lineno, 'Duplicate expectations.', test)
            return True

        # Check if we've already seen a more precise path.
        return prev_base_path.startswith(os.path.normpath(test_list_path))

    def _add_error(self, lineno, msg, path):
        """Reports an error that will prevent running the tests. Does not
        immediately raise an exception because we'd like to aggregate all the
        errors so they can all be printed out."""
        self._errors.append('\nLine:%s %s %s' % (lineno, msg, path))

    def _log_non_fatal_error(self, lineno, msg, path):
        """Reports an error that will not prevent running the tests. These are
        still errors, but not bad enough to warrant breaking test running."""
        self._non_fatal_errors.append('Line:%s %s %s' % (lineno, msg, path))
