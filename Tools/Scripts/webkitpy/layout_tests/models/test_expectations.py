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

from webkitpy.common.iteration_compatibility import iteritems, itervalues
from webkitpy.layout_tests.models.test_configuration import (
    TestConfiguration,
    TestConfigurationConverter,
)
from webkitpy.port.base import Port

MYPY = False
if MYPY:
    # MYPY is True when running Mypy; typing is stdlib only in Py3
    from typing import (
        Any,
        Callable,
        Container,
        Dict,
        Iterable,
        List,
        Optional,
        Set,
        Tuple,
        Union,
    )

_log = logging.getLogger(__name__)


# Test expectation and modifier constants.
#
# FIXME: range() starts with 0 which makes if expectation checks harder
# as PASS is 0.
(PASS, FAIL, TEXT, IMAGE, IMAGE_PLUS_TEXT, AUDIO, TIMEOUT, CRASH, SKIP, WONTFIX,
 SLOW, LEAK, DUMPJSCONSOLELOGINSTDERR, REBASELINE, MISSING, FLAKY, NOW, NONE) = range(18)

# FIXME: Perhas these two routines should be part of the Port instead?
BASELINE_SUFFIX_LIST = ('png', 'wav', 'txt')


class ParseError(Exception):
    def __init__(self, warnings):
        # type: (List[TestExpectationWarning]) -> None
        super(ParseError, self).__init__()
        self.warnings = warnings

    def __str__(self):
        # type: () -> str
        return '\n'.join(map(str, self.warnings))

    def __repr__(self):
        # type: () -> str
        return 'ParseError(warnings=%s)' % self.warnings


class TestExpectationWarning(object):
    def __init__(self, filename, line_number, line, error, test=None):
        # type: (str, int, str, str, Optional[str]) -> None
        self.filename = filename
        self.line_number = line_number
        self.line = line
        self.error = error
        self.test = test

        self.related_files = {}  # type: Dict[str, Optional[List[int]]]

    def __str__(self):
        # type: () -> str
        return '{}:{} {} {}'.format(self.filename, self.line_number, self.error, self.test if self.test else self.line)


class TestExpectationParser(object):
    """Provides parsing facilities for lines in the test_expectation.txt file."""

    BUG_MODIFIER_PREFIX = 'bug'
    BUG_MODIFIER_REGEX = r'bug\d+'
    REBASELINE_MODIFIER = 'rebaseline'
    PASS_EXPECTATION = 'pass'
    SKIP_MODIFIER = 'skip'
    SLOW_MODIFIER = 'slow'
    DUMPJSCONSOLELOGINSTDERR_MODIFIER = 'dumpjsconsoleloginstderr'
    WONTFIX_MODIFIER = 'wontfix'

    TIMEOUT_EXPECTATION = 'timeout'

    MISSING_BUG_WARNING = 'Test lacks BUG modifier.'

    def __init__(self, port, full_test_list, allow_rebaseline_modifier, shorten_filename=lambda x: x):
        # type: (Port, Optional[List[str]], bool, Callable[[str], str]) -> None
        self._port = port
        self._test_configuration_converter = TestConfigurationConverter(set(port.all_test_configurations()), port.configuration_specifier_macros())
        if full_test_list is None:
            self._full_test_list = None  # type: Optional[Set[str]]
        else:
            self._full_test_list = set(full_test_list)
        self._allow_rebaseline_modifier = allow_rebaseline_modifier
        self._shorten_filename = shorten_filename

    def parse(self, filename, expectations_string):
        # type: (str, str) -> List[TestExpectationLine]
        expectation_lines = []
        line_number = 0
        for line in expectations_string.split("\n"):
            line_number += 1
            test_expectation = self._tokenize_line(filename, line, line_number)
            self._parse_line(test_expectation)
            expectation_lines.append(test_expectation)
        return expectation_lines

    def expectation_for_skipped_test(self, test_name):
        # type: (str) -> TestExpectationLine
        if not self._port.test_exists(test_name):
            _log.warning('The following test %s from the Skipped list doesn\'t exist' % test_name)
        expectation_line = TestExpectationLine()
        expectation_line.original_string = test_name
        expectation_line.modifiers = [TestExpectationParser.SKIP_MODIFIER]
        # FIXME: It's not clear what the expectations for a skipped test should be; the expectations
        # might be different for different entries in a Skipped file, or from the command line, or from
        # only running parts of the tests. It's also not clear if it matters much.
        expectation_line.modifiers.append(TestExpectationParser.WONTFIX_MODIFIER)
        expectation_line.name = test_name
        # FIXME: we should pass in a more descriptive string here.
        expectation_line.filename = '<Skipped file>'
        expectation_line.line_number = 0
        expectation_line.expectations = [TestExpectationParser.PASS_EXPECTATION]
        expectation_line.not_applicable_to_current_platform = True
        self._parse_line(expectation_line)
        return expectation_line

    def _parse_line(self, expectation_line):
        # type: (TestExpectationLine) -> None
        if not expectation_line.name:
            return

        if not self._check_test_exists(expectation_line):
            return

        expectation_line.is_file = self._port.test_isfile(expectation_line.name)
        if expectation_line.is_file:
            expectation_line.path = expectation_line.name
        else:
            expectation_line.path = self._port.normalize_test_name(expectation_line.name)

        self._collect_matching_tests(expectation_line)

        self._parse_modifiers(expectation_line)
        self._parse_expectations(expectation_line)

    def _parse_modifiers(self, expectation_line):
        # type: (TestExpectationLine) -> None
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
                    expectation_line.warnings.append(r'BUG\d+ is not allowed, must be one of BUGWK\d+, or a non-numeric bug identifier.')
                else:
                    expectation_line.parsed_bug_modifiers.append(modifier)
            else:
                parsed_specifiers.add(modifier)

        if not expectation_line.parsed_bug_modifiers and not has_wontfix and not has_bugid and self._port.warn_if_bug_missing_in_test_expectations():
            expectation_line.warnings.append(self.MISSING_BUG_WARNING)

        if self._allow_rebaseline_modifier and self.REBASELINE_MODIFIER in modifiers:
            expectation_line.warnings.append('REBASELINE should only be used for running rebaseline.py. Cannot be checked in.')

        expectation_line.matching_configurations = self._test_configuration_converter.to_config_set(parsed_specifiers, expectation_line.warnings)

    def _parse_expectations(self, expectation_line):
        # type: (TestExpectationLine) -> None
        result = set()
        for part in expectation_line.expectations:
            expectation = TestExpectations.expectation_from_string(part)
            if expectation is None:  # Careful, PASS is currently 0.
                expectation_line.warnings.append('Unsupported expectation: %s' % part)
                continue
            result.add(expectation)
        expectation_line.parsed_expectations = result

    def _check_test_exists(self, expectation_line):
        # type: (TestExpectationLine) -> bool
        assert expectation_line.name is not None
        # WebKit's way of skipping tests is to add a -disabled suffix.
        # So we should consider the path existing if the path or the
        # -disabled version exists.
        if not self._port.test_exists(expectation_line.name) and not self._port.test_exists(expectation_line.name + '-disabled'):
            # Log a warning here since you hit this case any
            # time you update TestExpectations without syncing
            # the LayoutTests directory
            expectation_line.warnings.append('Path does not exist.')
            expected_path = self._shorten_filename(self._port.abspath_for_test(expectation_line.name))
            expectation_line.related_files[expected_path] = None
            return False
        return True

    def _collect_matching_tests(self, expectation_line):
        # type: (TestExpectationLine) -> None
        """Convert the test specification to an absolute, normalized
        path and make sure directories end with the OS path separator."""
        assert expectation_line.path is not None

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

    # FIXME: Update the original modifiers and remove this once the old syntax is gone.
    _configuration_tokens_list = [
        'SnowLeopard', 'Lion', 'MountainLion', 'Mavericks', 'Yosemite', 'ElCapitan', # Legacy macOS
        'Mac', 'Sierra', 'HighSierra', 'Mojave', 'Catalina', 'BigSur',
        'x86_64', 'x86', 'arm64', 'arm64_32', 'armv7k',
        'Win', 'XP', 'Vista', 'Win7',
        'Linux',
        'Android',
        'Release',
        'Debug',
    ]

    _configuration_tokens = dict((token, token.upper()) for token in _configuration_tokens_list)
    _inverted_configuration_tokens = dict((value, name) for name, value in _configuration_tokens.items())

    # FIXME: Update the original modifiers list and remove this once the old syntax is gone.
    _expectation_tokens = {
        'Crash': 'CRASH',
        'DumpJSConsoleLogInStdErr': 'DUMPJSCONSOLELOGINSTDERR',
        'Failure': 'FAIL',
        'ImageOnlyFailure': 'IMAGE',
        'Leak': 'LEAK',
        'Missing': 'MISSING',
        'Pass': 'PASS',
        'Rebaseline': 'REBASELINE',
        'Skip': 'SKIP',
        'Slow': 'SLOW',
        'Timeout': 'TIMEOUT',
        'WontFix': 'WONTFIX',
    }

    _inverted_expectation_tokens = dict([(value, name) for name, value in _expectation_tokens.items()] +
                                        [('TEXT', 'Failure'), ('IMAGE+TEXT', 'Failure'), ('AUDIO', 'Failure')])

    # FIXME: Seems like these should be classmethods on TestExpectationLine instead of TestExpectationParser.
    @classmethod
    def _tokenize_line(cls, filename, expectation_string, line_number):
        # type: (str, str, int) -> TestExpectationLine
        """Tokenizes a line from TestExpectations and returns an unparsed TestExpectationLine instance using the old format.

        The new format for a test expectation line is:

        [[bugs] [ "[" <configuration modifiers> "]" <name> [ "[" <expectations> "]" ["#" <comment>]

        Any errant whitespace is not preserved.

        """
        expectation_line = TestExpectationLine()
        expectation_line.original_string = expectation_string
        expectation_line.filename = filename
        expectation_line.line_number = line_number

        comment_index = expectation_string.find("#")
        if comment_index == -1:
            comment_index = len(expectation_string)
        else:
            expectation_line.comment = expectation_string[comment_index + 1:]

        remaining_string = re.sub(r"\s+", " ", expectation_string[:comment_index].strip())
        if len(remaining_string) == 0:
            return expectation_line

        # special-case parsing this so that we fail immediately instead of treating this as a test name
        if remaining_string.startswith('//'):
            expectation_line.warnings = ['use "#" instead of "//" for comments']
            return expectation_line

        bugs = []
        modifiers = []
        name = None
        expectations = []
        warnings = []

        WEBKIT_BUG_PREFIX = 'webkit.org/b/'

        tokens = remaining_string.split()
        state = 'start'
        for token in tokens:
            if token.startswith(WEBKIT_BUG_PREFIX) or token.startswith('Bug('):
                if state != 'start':
                    warnings.append('"%s" is not at the start of the line.' % token)
                    break
                if token.startswith(WEBKIT_BUG_PREFIX):
                    bugs.append(token.replace(WEBKIT_BUG_PREFIX, 'BUGWK'))
                else:
                    match = re.match(r'Bug\((\w+)\)$', token)
                    if not match:
                        warnings.append('unrecognized bug identifier "%s"' % token)
                        break
                    else:
                        bugs.append('BUG' + match.group(1).upper())
            elif token.startswith('BUG'):
                warnings.append('unrecognized old-style bug identifier "%s"' % token)
                break
            elif token == '[':
                if state == 'start':
                    state = 'configuration'
                elif state == 'name_found':
                    state = 'expectations'
                else:
                    warnings.append('unexpected "["')
                    break
            elif token == ']':
                if state == 'configuration':
                    state = 'name'
                elif state == 'expectations':
                    state = 'done'
                else:
                    warnings.append('unexpected "]"')
                    break
            elif token in ('//', ':', '='):
                warnings.append('"%s" is not legal in the new TestExpectations syntax.' % token)
                break
            elif state == 'configuration':
                modifiers.append(cls._configuration_tokens.get(token, token))
            elif state == 'expectations':
                if token in ('Rebaseline', 'Skip', 'Slow', 'WontFix', 'DumpJSConsoleLogInStdErr'):
                    modifiers.append(token.upper())
                elif token not in cls._expectation_tokens:
                    warnings.append('Unrecognized expectation "%s"' % token)
                else:
                    expectations.append(cls._expectation_tokens.get(token, token))
            elif state == 'name_found':
                warnings.append('expecting "[", "#", or end of line instead of "%s"' % token)
                break
            else:
                name = token
                state = 'name_found'

        if not warnings:
            if not name:
                warnings.append('Did not find a test name.')
            elif state not in ('name_found', 'done'):
                warnings.append('Missing a "]"')

        if 'WONTFIX' in modifiers and 'SKIP' not in modifiers and not expectations:
            modifiers.append('SKIP')

        if 'SKIP' in modifiers and expectations:
            # FIXME: This is really a semantic warning and shouldn't be here. Remove when we drop the old syntax.
            warnings.append('A test marked Skip must not have other expectations.')
        elif not expectations:
            # FIXME: We can probably simplify this adding 'SKIP' if modifiers is empty
            if 'SKIP' not in modifiers and 'REBASELINE' not in modifiers and 'SLOW' not in modifiers and 'DUMPJSCONSOLELOGINSTDERR' not in modifiers:
                modifiers.append('SKIP')
            expectations = ['PASS']

        # FIXME: expectation line should just store bugs and modifiers separately.
        expectation_line.modifiers = bugs + modifiers
        expectation_line.expectations = expectations
        expectation_line.name = name
        expectation_line.warnings = warnings
        return expectation_line

    @classmethod
    def _split_space_separated(cls, space_separated_string):
        # type: (str) -> List[str]
        """Splits a space-separated string into an array."""
        return [part.strip() for part in space_separated_string.strip().split(' ')]


class TestExpectationLine(object):
    """Represents a line in test expectations file."""

    def __init__(self):
        # type: () -> None
        """Initializes a blank-line equivalent of an expectation."""
        self.original_string = None  # type: Optional[str]
        self.filename = None  # type: Optional[str]  # this is the path to the expectations file for this line;
        self.line_number = None  # type: Optional[int]
        self.name = None  # type: Optional[str]  # this is the path in the line itself
        self.path = None  # type: Optional[str]  # this is the normpath of self.name
        self.modifiers = []  # type: List[str]
        self.parsed_modifiers = []  # type: List[str]
        self.parsed_bug_modifiers = []  # type: List[str]
        self.matching_configurations = set()  # type: Set[TestConfiguration]
        self.expectations = []  # type: List[str]
        self.parsed_expectations = set()  # type: Set[int]
        self.comment = None  # type: Optional[str]
        self.matching_tests = []  # type: List[str]
        self.warnings = []  # type: List[str]
        self.related_files = {}  # type: Dict[str, Optional[List[int]]]  # Dictionary of files to lines number in that file which may have caused the list of warnings.
        self.not_applicable_to_current_platform = False
        self.is_file = False

    def __str__(self):
        # type: () -> str
        s = self.to_string(None)
        if s is None:
            return ''
        else:
            return s

    def is_invalid(self):
        # type: () -> bool
        return bool(self.warnings and self.warnings != [TestExpectationParser.MISSING_BUG_WARNING])

    def is_flaky(self):
        # type: () -> bool
        return len(self.parsed_expectations) > 1

    @property
    def expected_behavior(self):
        # type: () -> List[str]
        expectations = self.expectations[:]
        if "SLOW" in self.modifiers:
            expectations += ["SLOW"]

        if "SKIP" in self.modifiers:
            expectations = ["SKIP"]
        elif "WONTFIX" in self.modifiers:
            expectations = ["WONTFIX"]
        elif "CRASH" in self.modifiers:
            expectations += ["CRASH"]

        return expectations

    @staticmethod
    def create_passing_expectation(test):
        # type: (str) -> TestExpectationLine
        expectation_line = TestExpectationLine()
        expectation_line.name = test
        expectation_line.path = test
        expectation_line.parsed_expectations = set([PASS])
        expectation_line.expectations = ['PASS']
        expectation_line.matching_tests = [test]
        return expectation_line

    def to_string(self, test_configuration_converter, include_modifiers=True, include_expectations=True, include_comment=True):
        # type: (Optional[TestConfigurationConverter], bool, bool, bool) -> Optional[str]
        parsed_expectation_to_string = dict((parsed_expectation, expectation_string) for expectation_string, parsed_expectation in TestExpectations.EXPECTATIONS.items())  # type: Dict[int, str]

        if self.is_invalid():
            return self.original_string or ''

        if self.name is None:
            return '' if self.comment is None else "#%s" % self.comment

        if test_configuration_converter and self.parsed_bug_modifiers:
            specifiers_list = test_configuration_converter.to_specifiers_list(self.matching_configurations)  # type: List[Set[Tuple[str, str]]]
            result = []
            for specifiers in specifiers_list:
                # FIXME: this is silly that we join the modifiers and then immediately split them.
                modifiers = self._serialize_parsed_modifiers(test_configuration_converter, specifiers).split()
                expectations = self._serialize_parsed_expectations(parsed_expectation_to_string).split()
                result.append(self._format_line(modifiers, self.name, expectations, self.comment))
            return "\n".join(result) if result else None

        return self._format_line(self.modifiers, self.name, self.expectations, self.comment,
            include_modifiers, include_expectations, include_comment)

    def _serialize_parsed_expectations(self, parsed_expectation_to_string):
        # type: (Dict[int, str]) -> str
        result = []
        for index in TestExpectations.EXPECTATION_ORDER:
            if index in self.parsed_expectations:
                result.append(parsed_expectation_to_string[index])
        return ' '.join(result)

    def _serialize_parsed_modifiers(self, test_configuration_converter, specifiers):
        # type: (TestConfigurationConverter, Set[Tuple[str, str]]) -> str
        result = []
        if self.parsed_bug_modifiers:
            result.extend(sorted(self.parsed_bug_modifiers))
        result.extend(sorted(self.parsed_modifiers))
        result.extend(test_configuration_converter.specifier_sorter().sort_specifiers(specifiers))
        return ' '.join(result)

    @staticmethod
    def _format_line(
        modifiers,  # type: List[str]
        name,  # type: str
        expectations,  # type: List[str]
        comment,  # type: Optional[str]
        include_modifiers=True,  # type: bool
        include_expectations=True,  # type: bool
        include_comment=True,  # type: bool
    ):
        # type: (...) -> str
        bugs = []  # type: List[str]
        new_modifiers = []  # type: List[str]
        new_expectations = []  # type: List[str]
        for modifier in modifiers:
            modifier = modifier.upper()
            if modifier.startswith('BUGWK'):
                bugs.append('webkit.org/b/' + modifier.replace('BUGWK', ''))
            elif modifier.startswith('BUG'):
                # FIXME: we should preserve case once we can drop the old syntax.
                bugs.append('Bug(' + modifier[3:].lower() + ')')
            elif modifier in ('SLOW', 'SKIP', 'REBASELINE', 'WONTFIX', 'DUMPJSCONSOLELOGINSTDERR'):
                new_expectations.append(TestExpectationParser._inverted_expectation_tokens[modifier])
            else:
                new_modifiers.append(TestExpectationParser._inverted_configuration_tokens.get(modifier, modifier))

        for expectation in expectations:
            expectation = expectation.upper()
            new_expectations.append(TestExpectationParser._inverted_expectation_tokens.get(expectation, expectation))

        result = ''
        if include_modifiers and (bugs or new_modifiers):
            if bugs:
                result += ' '.join(bugs) + ' '
            if new_modifiers:
                result += '[ %s ] ' % ' '.join(new_modifiers)
        result += name
        if include_expectations and new_expectations and set(new_expectations) != set(['Skip', 'Pass']):
            result += ' [ %s ]' % ' '.join(sorted(set(new_expectations)))
        if include_comment and comment is not None:
            result += " #%s" % comment
        return result


# FIXME: Refactor API to be a proper CRUD.
class TestExpectationsModel(object):
    """Represents relational store of all expectations and provides CRUD semantics to manage it."""

    def __init__(self, shorten_filename=lambda x: x):
        # type: (Callable[[str], str]) -> None
        # Maps a test to its list of expectations.
        self._test_to_expectations = {}  # type: Dict[str, Set[int]]

        # Maps a test to list of its modifiers (string values)
        self._test_to_modifiers = {}  # type: Dict[str, List[str]]

        # Maps a test to a TestExpectationLine instance.
        self._test_to_expectation_line = {}  # type: Dict[str, TestExpectationLine]

        self._modifier_to_tests = self._dict_of_sets(TestExpectations.MODIFIERS)  # type: Dict[int, Set[str]]
        self._expectation_to_tests = self._dict_of_sets(TestExpectations.EXPECTATIONS)  # type: Dict[int, Set[str]]
        self._timeline_to_tests = self._dict_of_sets(TestExpectations.TIMELINES)  # type: Dict[int, Set[str]]
        self._result_type_to_tests = self._dict_of_sets(TestExpectations.RESULT_TYPES)  # type: Dict[int, Set[str]]

        self._shorten_filename = shorten_filename

    def _dict_of_sets(self, strings_to_constants):
        # type: (Dict[str, int]) -> Dict[int, Set[Any]]
        """Takes a dict of strings->constants and returns a dict mapping
        each constant to an empty set."""
        d = {}  # type: Dict[int, Set[Any]]
        for c in strings_to_constants.values():
            d[c] = set()
        return d

    def get_test_set(self, modifier, expectation=None, include_skips=True):
        # type: (int, None, bool) -> Set[str]
        if expectation is None:
            tests = self._modifier_to_tests[modifier]
        else:
            tests = (self._expectation_to_tests[expectation] &
                self._modifier_to_tests[modifier])

        if not include_skips:
            tests = tests - self.get_test_set(SKIP, expectation)

        return tests

    def get_test_set_for_keyword(self, keyword):
        # type: (str) -> Set[str]
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
        for test, modifiers in iteritems(self._test_to_modifiers):
            if keyword.lower() in modifiers:
                matching_tests.add(test)
        return matching_tests

    def get_tests_with_result_type(self, result_type):
        # type: (int) -> Set[str]
        return self._result_type_to_tests[result_type]

    def get_tests_with_timeline(self, timeline):
        # type: (int) -> Set[str]
        return self._timeline_to_tests[timeline]

    def get_modifiers(self, test):
        # type: (str) -> List[str]
        """This returns modifiers for the given test (the modifiers plus the BUGXXXX identifier). This is used by the LTTF dashboard."""
        return self._test_to_modifiers[test]

    def has_modifier(self, test, modifier):
        # type: (str, int) -> bool
        return test in self._modifier_to_tests[modifier]

    def has_keyword(self, test, keyword):
        # type: (str, str) -> bool
        return (keyword.upper() in self.get_expectations_string(test) or
                keyword.lower() in self.get_modifiers(test))

    def has_test(self, test):
        # type: (str) -> bool
        return test in self._test_to_expectation_line

    def get_expectation_line(self, test):
        # type: (str) -> Optional[TestExpectationLine]
        return self._test_to_expectation_line.get(test)

    def get_expectations(self, test):
        # type: (str) -> Set[int]
        return self._test_to_expectations[test]

    def get_expectations_or_pass(self, test):
        # type: (str) -> Set[int]
        try:
            return self.get_expectations(test)
        except:
            return set([PASS])

    def expectations_to_string(self, expectations):
        # type: (Iterable[int]) -> str
        retval = []

        for expectation in expectations:
            retval.append(self.expectation_to_string(expectation))

        return " ".join(retval)

    def get_expectations_string(self, test):
        # type: (str) -> str
        """Returns the expectations for the given test as an uppercase string.
        If there are no expectations for the test, then "PASS" is returned."""
        try:
            expectations = self.get_expectations(test)
        except:
            return "PASS"
        retval = []

        for expectation in expectations:
            retval.append(self.expectation_to_string(expectation))

        return " ".join(retval)

    def expectation_to_string(self, expectation):
        # type: (int) -> str
        """Return the uppercased string equivalent of a given expectation."""
        for item in TestExpectations.EXPECTATIONS.items():
            if item[1] == expectation:
                return item[0].upper()
        raise ValueError(expectation)

    def add_expectation_line(self, expectation_line, in_skipped=False):
        # type: (TestExpectationLine, bool) -> None
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
        # type: (str, TestExpectationLine) -> None
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
        # type: (str) -> None
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
        # type: (str, Dict[int, Set[str]]) -> None
        """Removes the given test from the sets in the dictionary.

        Args:
          test: test to look for
          dict: dict of sets of files"""
        for set_of_tests in itervalues(dict_of_sets_of_tests):
            if test in set_of_tests:
                set_of_tests.remove(test)

    def _already_seen_better_match(self, test, expectation_line):
        # type: (str, TestExpectationLine) -> bool
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

        assert prev_expectation_line.path is not None
        assert expectation_line.path is not None

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
        assert prev_expectation_line.filename is not None
        assert expectation_line.filename is not None
        shortened_expectation_filename = self._shorten_filename(expectation_line.filename)
        shortened_previous_expectation_filename = self._shorten_filename(prev_expectation_line.filename)

        assert prev_expectation_line.line_number is not None
        assert expectation_line.line_number is not None

        if prev_expectation_line.matching_configurations == expectation_line.matching_configurations:
            expectation_line.warnings.append('Duplicate or ambiguous entry lines %s:%d and %s:%d.' % (
                shortened_previous_expectation_filename, prev_expectation_line.line_number,
                shortened_expectation_filename, expectation_line.line_number))

        elif prev_expectation_line.matching_configurations >= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%d overrides line %s:%d.' % (expectation_line.name,
                shortened_previous_expectation_filename, prev_expectation_line.line_number,
                shortened_expectation_filename, expectation_line.line_number))
            # FIXME: return False if we want more specific to win.

        elif prev_expectation_line.matching_configurations <= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%d overrides line %s:%d.' % (expectation_line.name,
                shortened_expectation_filename, expectation_line.line_number,
                shortened_previous_expectation_filename, prev_expectation_line.line_number))

        elif prev_expectation_line.matching_configurations & expectation_line.matching_configurations:
            expectation_line.warnings.append('Entries for %s on lines %s:%d and %s:%d match overlapping sets of configurations.' % (expectation_line.name,
                shortened_previous_expectation_filename, prev_expectation_line.line_number,
                shortened_expectation_filename, expectation_line.line_number))

        else:
            # Configuration sets are disjoint.
            return False

        # Missing files will be 'None'. It should be impossible to have a missing file which also has a line associated with it.
        if shortened_previous_expectation_filename not in expectation_line.related_files:
            expectation_line.related_files[shortened_previous_expectation_filename] = []

        expectation_line_related = expectation_line.related_files[shortened_previous_expectation_filename]
        assert expectation_line_related is not None
        expectation_line_related.append(prev_expectation_line.line_number)

        if shortened_expectation_filename not in prev_expectation_line.related_files:
            prev_expectation_line.related_files[shortened_expectation_filename] = []

        prev_expectation_line_related = prev_expectation_line.related_files[shortened_expectation_filename]
        assert prev_expectation_line_related is not None
        prev_expectation_line_related.append(expectation_line.line_number)

        return True


class TestExpectations(object):
    """Test expectations consist of lines with specifications of what
    to expect from layout test cases. The test cases can be directories
    in which case the expectations apply to all test cases in that
    directory and any subdirectory. The format is along the lines of:

      LayoutTests/js/fixme.js [ Failure ]
      LayoutTests/js/flaky.js [ Failure Pass ]
      LayoutTests/js/crash.js [ Crash Failure Pass Timeout ]
      ...

    To add modifiers:
      LayoutTests/js/no-good.js
      [ Debug ] LayoutTests/js/no-good.js [ Pass Timeout ]
      [ Debug ] LayoutTests/js/no-good.js [ Pass Skip Timeout ]
      [ Linux Debug ] LayoutTests/js/no-good.js [ Pass Skip Timeout ]
      [ Linux Win ] LayoutTests/js/no-good.js [ Pass Skip Timeout ]

    Skip: Doesn't run the test.
    Slow: The test takes a long time to run, but does not timeout indefinitely.
    WontFix: For tests that we never intend to pass on a given platform (treated like Skip).

    Notes:
      -A test cannot be both SLOW and TIMEOUT
      -A test can be included twice, but not via the same path.
      -If a test is included twice, then the more precise path wins.
      -CRASH tests cannot be WONTFIX
    """

    # FIXME: Update to new syntax once the old format is no longer supported.
    EXPECTATIONS = {'pass': PASS,
                    'audio': AUDIO,
                    'fail': FAIL,
                    'image': IMAGE,
                    'image+text': IMAGE_PLUS_TEXT,
                    'text': TEXT,
                    'timeout': TIMEOUT,
                    'crash': CRASH,
                    'missing': MISSING,
                    'leak': LEAK,
                    'skip': SKIP}

    # Singulars
    EXPECTATION_DESCRIPTION = {SKIP: 'skipped',
                                PASS: 'pass',
                                FAIL: 'failure',
                                IMAGE: 'image-only failure',
                                TEXT: 'text-only failure',
                                IMAGE_PLUS_TEXT: 'image and text failure',
                                AUDIO: 'audio failure',
                                CRASH: 'crash',
                                TIMEOUT: 'timeout',
                                MISSING: 'missing',
                                LEAK: 'leak'}

    # (aggregated by category, pass/fail/skip, type)
    EXPECTATION_DESCRIPTIONS = {SKIP: 'skipped',
                                PASS: 'passes',
                                FAIL: 'failures',
                                IMAGE: 'image-only failures',
                                TEXT: 'text-only failures',
                                IMAGE_PLUS_TEXT: 'image and text failures',
                                AUDIO: 'audio failures',
                                CRASH: 'crashes',
                                TIMEOUT: 'timeouts',
                                MISSING: 'missing results',
                                LEAK: 'leaks'}

    EXPECTATION_ORDER = (PASS, CRASH, TIMEOUT, MISSING, FAIL, IMAGE, LEAK, SKIP)

    BUILD_TYPES = ('debug', 'release')

    MODIFIERS = {TestExpectationParser.SKIP_MODIFIER: SKIP,
                 TestExpectationParser.WONTFIX_MODIFIER: WONTFIX,
                 TestExpectationParser.SLOW_MODIFIER: SLOW,
                 TestExpectationParser.DUMPJSCONSOLELOGINSTDERR_MODIFIER: DUMPJSCONSOLELOGINSTDERR,
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
        # type: (str) -> Optional[int]
        assert(' ' not in string)  # This only handles one expectation at a time.
        return cls.EXPECTATIONS.get(string.lower())

    @staticmethod
    def result_was_expected(result, expected_results, test_needs_rebaselining, test_is_skipped):
        # type: (int, Set[int], bool, bool) -> bool
        """Returns whether we got a result we were expecting.
        Args:
            result: actual result of a test execution
            expected_results: set of results listed in test_expectations
            test_needs_rebaselining: whether test was marked as REBASELINE
            test_is_skipped: whether test was marked as SKIP"""
        if result in expected_results:
            return True
        if result in (TEXT, IMAGE_PLUS_TEXT, AUDIO) and (FAIL in expected_results):
            return True
        if result == MISSING and test_needs_rebaselining:
            return True
        if result == SKIP and test_is_skipped:
            return True
        return False

    @staticmethod
    def remove_pixel_failures(expected_results):
        # type: (Set[int]) -> Set[int]
        """Returns a copy of the expected results for a test, except that we
        drop any pixel failures and return the remaining expectations. For example,
        if we're not running pixel tests, then tests expected to fail as IMAGE
        will PASS."""
        expected_results = expected_results.copy()
        if IMAGE in expected_results:
            expected_results.remove(IMAGE)
            expected_results.add(PASS) # FIXME: does it always become a pass?
        return expected_results

    @staticmethod
    def remove_leak_failures(expected_results):
        # type: (Set[int]) -> Set[int]
        """Returns a copy of the expected results for a test, except that we
        drop any leak failures and return the remaining expectations. For example,
        if we're not running with --world-leaks, then tests expected to fail as LEAK
        will PASS."""
        expected_results = expected_results.copy()
        if LEAK in expected_results:
            expected_results.remove(LEAK)
            if not expected_results:
                expected_results.add(PASS)
        return expected_results

    @staticmethod
    def has_pixel_failures(actual_results):
        # type: (Container[int]) -> bool
        return IMAGE in actual_results or FAIL in actual_results

    @staticmethod
    def suffixes_for_expectations(expectations):
        # type: (Set[int]) -> Set[str]
        suffixes = set()
        if IMAGE in expectations:
            suffixes.add('png')
        if FAIL in expectations:
            suffixes.add('txt')
            suffixes.add('png')
            suffixes.add('wav')
        return set(suffixes)

    def __init__(
        self,
        port,  # type: Port
        tests=None,  # type: Optional[List[str]]
        include_generic=True,  # type: bool
        include_overrides=True,  # type: bool
        expectations_to_lint=None,  # type: Optional[Dict[str, str]]
        force_expectations_pass=False,  # type: bool
        device_type=None,  # type: Optional[str]
    ):
        # type: (...) -> None
        self._full_test_list = tests
        self._test_config = port.test_configuration()  # type: TestConfiguration
        self._is_lint_mode = expectations_to_lint is not None
        self._model = TestExpectationsModel(self._shorten_filename)
        self._parser = TestExpectationParser(port, tests, self._is_lint_mode, self._shorten_filename)
        self._port = port
        self._skipped_tests_warnings = []  # type: List[TestExpectationWarning]
        self._expectations = []  # type: List[TestExpectationLine]
        self._force_expectations_pass = force_expectations_pass
        self._device_type = device_type
        self._include_generic = include_generic
        self._include_overrides = include_overrides
        self._expectations_to_lint = expectations_to_lint

    def readable_filename_and_line_number(self, line):
        # type: (TestExpectationLine) -> str
        if line.not_applicable_to_current_platform:
            return "(skipped for this platform)"
        if not line.filename:
            return ''
        if line.filename.startswith(self._port.path_from_webkit_base()):
            return '{}:{}'.format(self._port.host.filesystem.relpath(line.filename, self._port.path_from_webkit_base()), line.line_number)
        return '{}:{}'.format(line.filename, line.line_number)

    def parse_generic_expectations(self):
        # type: () -> None
        if self._port.path_to_generic_test_expectations_file() in self._expectations_dict:
            if self._include_generic:
                expectations = self._parser.parse(list(self._expectations_dict.keys())[self._expectations_dict_index], list(self._expectations_dict.values())[self._expectations_dict_index])
                self._add_expectations(expectations)
                self._expectations += expectations
            self._expectations_dict_index += 1

    def parse_default_port_expectations(self):
        # type: () -> None
        if len(self._expectations_dict) > self._expectations_dict_index:
            expectations = self._parser.parse(list(self._expectations_dict.keys())[self._expectations_dict_index], list(self._expectations_dict.values())[self._expectations_dict_index])
            self._add_expectations(expectations)
            self._expectations += expectations
            self._expectations_dict_index += 1

    def parse_override_expectations(self):
        # type: () -> None
        while len(self._expectations_dict) > self._expectations_dict_index and self._include_overrides:
            expectations = self._parser.parse(list(self._expectations_dict.keys())[self._expectations_dict_index], list(self._expectations_dict.values())[self._expectations_dict_index])
            self._add_expectations(expectations)
            self._expectations += expectations
            self._expectations_dict_index += 1

    def parse_all_expectations(self):
        # type: () -> None
        self._expectations_dict = self._expectations_to_lint or self._port.expectations_dict(device_type=self._device_type)  # type: Dict[str, str]
        self._expectations_dict_index = 0

        self._has_warnings = False

        self.parse_generic_expectations()
        self.parse_default_port_expectations()
        self.parse_override_expectations()

        self.add_skipped_tests(self._port.skipped_layout_tests(device_type=self._device_type))

        self._report_warnings()
        self._process_tests_without_expectations()

    # TODO(ojan): Allow for removing skipped tests when getting the list of
    # tests to run, but not when getting metrics.
    def model(self):
        # type: () -> TestExpectationsModel
        return self._model

    def get_rebaselining_failures(self):
        # type: () -> Set[str]
        return self._model.get_test_set(REBASELINE)

    def filtered_expectations_for_test(self, test, pixel_tests_are_enabled, world_leaks_are_enabled):
        # type: (str, bool, bool) -> Set[int]
        expected_results = self._model.get_expectations_or_pass(test)
        if not pixel_tests_are_enabled:
            expected_results = self.remove_pixel_failures(expected_results)
        if not world_leaks_are_enabled:
            expected_results = self.remove_leak_failures(expected_results)

        return expected_results

    def matches_an_expected_result(self, test, result, expected_results):
        # type: (str, int, Set[int]) -> bool
        return self.result_was_expected(result,
                                   expected_results,
                                   self.is_rebaselining(test),
                                   self._model.has_modifier(test, SKIP))

    def is_rebaselining(self, test):
        # type: (str) -> bool
        return self._model.has_modifier(test, REBASELINE)

    def _shorten_filename(self, filename):
        # type: (str) -> str
        if filename.startswith(self._port.path_from_webkit_base()):
            trimmed = self._port.host.filesystem.relpath(filename, self._port.path_from_webkit_base())
            if MYPY:
                # this doesn't actually hold on Py2 (it's actually unicode)
                assert isinstance(trimmed, str)
            return trimmed
        return filename

    def _report_warnings(self):
        # type: () -> None
        warnings = []
        for expectation in self._expectations:
            assert expectation.filename is not None
            assert expectation.line_number is not None
            assert expectation.original_string is not None
            for warning in expectation.warnings:
                warning_obj = TestExpectationWarning(
                    self._shorten_filename(expectation.filename), expectation.line_number,
                    expectation.original_string, warning, expectation.name if expectation.expectations else None)
                warning_obj.related_files = expectation.related_files
                warnings.append(warning_obj)

        if warnings:
            self._has_warnings = True
            if self._is_lint_mode:
                raise ParseError(warnings)
            _log.warning('--lint-test-files warnings:')
            for warning_obj in warnings:
                _log.warning(warning_obj)
            _log.warning('')

    def _process_tests_without_expectations(self):
        # type: () -> None
        if self._full_test_list:
            for test in self._full_test_list:
                if not self._model.has_test(test):
                    self._model.add_expectation_line(TestExpectationLine.create_passing_expectation(test))

    def has_warnings(self):
        # type: () -> bool
        return self._has_warnings

    def remove_configuration_from_test(self, test, test_configuration):
        # type: (str, TestConfiguration) -> str
        expectations_to_remove = []
        modified_expectations = []

        for expectation in self._expectations:
            if expectation.name != test or expectation.is_flaky() or not expectation.parsed_expectations:
                continue
            if not any([value in (FAIL, IMAGE) for value in expectation.parsed_expectations]):
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

        return self.list_to_string(self._expectations, self._parser._test_configuration_converter, modified_expectations)

    def remove_rebaselined_tests(self, except_these_tests, filename):
        # type: (List[str], str) -> str
        """Returns a copy of the expectations in the file with the tests removed."""
        def without_rebaseline_modifier(expectation):
            # type: (TestExpectationLine) -> bool
            return (expectation.filename == filename and
                    not (not expectation.is_invalid() and
                         expectation.name in except_these_tests and
                         'rebaseline' in expectation.parsed_modifiers))

        return self.list_to_string(filter(without_rebaseline_modifier, self._expectations), reconstitute_only_these=[])

    def _add_expectations(self, expectation_list):
        # type: (List[TestExpectationLine]) -> None
        for expectation_line in expectation_list:
            if self._force_expectations_pass:
                expectation_line.expectations = ['PASS']
                expectation_line.parsed_expectations = set([PASS])

            elif not expectation_line.expectations:
                continue

            if self._is_lint_mode or self._test_config in expectation_line.matching_configurations:
                self._model.add_expectation_line(expectation_line)

    def add_skipped_tests(self, tests_to_skip):
        # type: (Set[str]) -> None
        if not tests_to_skip:
            return
        for test in self._expectations:
            if test.name and test.name in tests_to_skip:
                assert test.filename is not None
                assert test.line_number is not None
                test.warnings.append('%s:%d %s is also in a Skipped file.' % (test.filename, test.line_number, test.name))

        for test_name in tests_to_skip:
            expectation_line = self._parser.expectation_for_skipped_test(test_name)
            self._model.add_expectation_line(expectation_line, in_skipped=True)

    @staticmethod
    def list_to_string(
        expectation_lines,  # type: Iterable[TestExpectationLine]
        test_configuration_converter=None,  # type: Optional[TestConfigurationConverter]
        reconstitute_only_these=None,  # type: Optional[List[TestExpectationLine]]
    ):
        # type: (...) -> str
        def serialize(expectation_line):
            # type: (TestExpectationLine) -> Optional[str]
            # If reconstitute_only_these is an empty list, we want to return original_string.
            # So we need to compare reconstitute_only_these to None, not just check if it's falsey.
            if reconstitute_only_these is None or expectation_line in reconstitute_only_these:
                return expectation_line.to_string(test_configuration_converter)
            return expectation_line.original_string

        return "\n".join(line for line in map(serialize, expectation_lines) if line is not None)
