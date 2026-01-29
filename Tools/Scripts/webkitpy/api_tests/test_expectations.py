# Copyright (C) 2024 Apple Inc. All rights reserved.
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

"""A helper class for reading in and dealing with test expectations for API tests."""

import enum
import fnmatch
import logging
import re
from collections import defaultdict

_log = logging.getLogger(__name__)


# Configuration category enum - ordered from least to most specific
class ConfigurationCategory(enum.IntEnum):
    PLATFORM = 1      # mac, ios, linux, win, gtk, wpe
    VERSION = 2       # Sonoma, iOS17, with +/-/range modifiers
    STYLE = 3         # debug, release, asan, guardmalloc
    HARDWARE = 4      # simulator, device, iphone, ipad
    ARCHITECTURE = 5  # arm64, x86_64, x86, arm64_32, armv7k
    FLAVOR = 6        # freeform: wk1, wk2, siteisolation, etc.


# Static tokens by category (alphabetically ordered within each)
PLATFORM_TOKENS = frozenset({'gtk', 'ios', 'linux', 'mac', 'win', 'wpe'})
STYLE_TOKENS = frozenset({'asan', 'debug', 'guardmalloc', 'release'})
HARDWARE_TOKENS = frozenset({'device', 'ipad', 'iphone', 'simulator'})
ARCHITECTURE_TOKENS = frozenset({'arm64', 'arm64_32', 'armv7k', 'x86', 'x86_64'})

# Category to token set mapping
CATEGORY_TOKENS = {
    ConfigurationCategory.PLATFORM: PLATFORM_TOKENS,
    ConfigurationCategory.STYLE: STYLE_TOKENS,
    ConfigurationCategory.HARDWARE: HARDWARE_TOKENS,
    ConfigurationCategory.ARCHITECTURE: ARCHITECTURE_TOKENS,
    # VERSION and FLAVOR are dynamic - handled separately
}

# Category display names for error messages
CATEGORY_NAMES = {
    ConfigurationCategory.PLATFORM: 'Platform',
    ConfigurationCategory.VERSION: 'Version',
    ConfigurationCategory.STYLE: 'Style',
    ConfigurationCategory.HARDWARE: 'Hardware',
    ConfigurationCategory.ARCHITECTURE: 'Architecture',
    ConfigurationCategory.FLAVOR: 'Flavor',
}


# Expectation constants (values 0-3 match Runner.STATUS_*)
PASS = 0
FAIL = 1
CRASH = 2
TIMEOUT = 3
SKIP = 4
SLOW = 5

# String to constant mappings
EXPECTATION_MAP = {
    'pass': PASS,
    'fail': FAIL,
    'failure': FAIL,
    'crash': CRASH,
    'timeout': TIMEOUT,
    'skip': SKIP,
}

EXPECTATION_NAMES = {
    PASS: 'Pass',
    FAIL: 'Fail',
    CRASH: 'Crash',
    TIMEOUT: 'Timeout',
    SKIP: 'Skip',
    SLOW: 'Slow',
}

# Combined set of all static configuration tokens (for backwards compatibility)
CONFIGURATION_TOKENS = PLATFORM_TOKENS | STYLE_TOKENS | HARDWARE_TOKENS | ARCHITECTURE_TOKENS


class VersionSpecifier(object):
    """Represents a version constraint with optional range/modifier."""

    class Type(enum.Enum):
        EXACT = 'exact'           # Sonoma (this version only)
        AND_LATER = 'and_later'   # Sonoma+ (this and later)
        AND_EARLIER = 'and_earlier'  # Sonoma- (this and earlier)
        RANGE = 'range'           # Ventura-Sequoia (inclusive range)

    def __init__(self, base_version, end_version=None, specifier_type=None):
        self.base_version = base_version.lower()
        self.end_version = end_version.lower() if end_version else None
        self.type = specifier_type or self.Type.EXACT
        self._original = None

    @classmethod
    def parse(cls, token):
        """Parse a version token into a VersionSpecifier.

        Returns None if the token is not a version specifier.
        """
        # Check for range first: Ventura-Sequoia (not ending with -)
        if '-' in token and not token.endswith('-'):
            parts = token.split('-', 1)
            if len(parts) == 2:
                spec = cls(parts[0], parts[1], cls.Type.RANGE)
                spec._original = token
                return spec
        # Check for and_later: Sonoma+
        elif token.endswith('+'):
            spec = cls(token[:-1], specifier_type=cls.Type.AND_LATER)
            spec._original = token
            return spec
        # Check for and_earlier: Sonoma-
        elif token.endswith('-'):
            spec = cls(token[:-1], specifier_type=cls.Type.AND_EARLIER)
            spec._original = token
            return spec
        # Exact version (handled during token categorization)
        return None

    def matches(self, current_version, version_order):
        """Check if current_version satisfies this specifier.

        Args:
            current_version: The version string to check (lowercase)
            version_order: List of version strings in order from oldest to newest
        """
        current_version = current_version.lower()
        try:
            current_idx = version_order.index(current_version)
            base_idx = version_order.index(self.base_version)
        except ValueError:
            return False

        if self.type == self.Type.EXACT:
            return current_version == self.base_version
        elif self.type == self.Type.AND_LATER:
            return current_idx >= base_idx
        elif self.type == self.Type.AND_EARLIER:
            return current_idx <= base_idx
        elif self.type == self.Type.RANGE:
            try:
                end_idx = version_order.index(self.end_version)
            except ValueError:
                return False
            return base_idx <= current_idx <= end_idx
        return False

    def __repr__(self):
        if self._original:
            return 'VersionSpecifier({!r})'.format(self._original)
        return 'VersionSpecifier({!r}, type={})'.format(self.base_version, self.type.value)


def get_token_category(token, version_tokens=None):
    """Return the ConfigurationCategory for a token, or None if unknown.

    Args:
        token: The configuration token (lowercase)
        version_tokens: Optional set of valid version tokens
    """
    token_lower = token.lower()

    # Check static categories first
    if token_lower in PLATFORM_TOKENS:
        return ConfigurationCategory.PLATFORM
    if token_lower in STYLE_TOKENS:
        return ConfigurationCategory.STYLE
    if token_lower in HARDWARE_TOKENS:
        return ConfigurationCategory.HARDWARE
    if token_lower in ARCHITECTURE_TOKENS:
        return ConfigurationCategory.ARCHITECTURE

    # Check if it's a version specifier (with +, -, or range)
    if token.endswith('+') or token.endswith('-') or ('-' in token and not token.endswith('-')):
        return ConfigurationCategory.VERSION

    # Check if it's a known version token
    if version_tokens and token_lower in version_tokens:
        return ConfigurationCategory.VERSION

    # If it looks like an identifier, treat as flavor (freeform)
    if token_lower.isidentifier() or re.match(r'^[a-z][a-z0-9_-]*$', token_lower):
        return ConfigurationCategory.FLAVOR

    return None


def runner_status_to_expectation(runner_status):
    """Convert a Runner status code to an expectation constant."""
    from webkitpy.api_tests.runner import Runner
    mapping = {
        Runner.STATUS_PASSED: PASS,
        Runner.STATUS_FAILED: FAIL,
        Runner.STATUS_CRASHED: CRASH,
        Runner.STATUS_TIMEOUT: TIMEOUT,
    }
    return mapping.get(runner_status)


class ParseError(Exception):
    """Exception raised when parsing test expectations fails."""
    def __init__(self, warnings):
        super(ParseError, self).__init__()
        self.warnings = warnings

    def __str__(self):
        return '\n'.join(str(w) for w in self.warnings)


class ExpectationWarning(object):
    """Represents a warning or error from parsing an expectation line."""
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


class APITestExpectation(object):
    """Represents expectations for a single API test or pattern."""

    def __init__(self, test_pattern, expectations, configurations=None,
                 slow_timeout=None, bug_ids=None, filename=None, line_number=None,
                 version_specifiers=None):
        self.test_pattern = test_pattern
        self.expectations = frozenset(expectations)
        self.configurations = frozenset(configurations) if configurations else frozenset()
        self.version_specifiers = tuple(version_specifiers) if version_specifiers else ()
        self.slow_timeout = slow_timeout
        self.bug_ids = tuple(bug_ids) if bug_ids else ()
        self.filename = filename
        self.line_number = line_number
        self._is_wildcard = test_pattern.endswith('*')
        self._prefix = test_pattern[:-1] if self._is_wildcard else None

    @property
    def is_wildcard(self):
        """Returns True if this expectation uses a wildcard pattern."""
        return self._is_wildcard

    def is_skip(self):
        """Returns True if test should be skipped."""
        return SKIP in self.expectations

    def is_slow(self):
        """Returns True if test is marked slow."""
        return self.slow_timeout is not None or SLOW in self.expectations

    def get_timeout(self, default_timeout):
        """Returns timeout for this test."""
        if self.slow_timeout is not None:
            return self.slow_timeout
        if SLOW in self.expectations:
            return default_timeout * 5
        return default_timeout

    def is_flaky(self):
        """Returns True if test can have multiple outcomes."""
        actual_expectations = self.expectations - {SKIP, SLOW}
        return len(actual_expectations) > 1

    def matches_test(self, test_name):
        """Check if this expectation matches the given test name."""
        if self._is_wildcard:
            return test_name.startswith(self._prefix)
        return test_name == self.test_pattern

    def matches_configuration(self, current_config, current_version=None, version_order=None):
        """Check if this expectation applies to current configuration.

        Args:
            current_config: Set of current configuration tokens (platform, style, etc.)
            current_version: Current OS version string (e.g., 'sonoma')
            version_order: List of version strings in order from oldest to newest
        """
        # Check non-version configurations
        if self.configurations and not self.configurations.issubset(current_config):
            return False

        # Check version specifiers
        if self.version_specifiers:
            if not current_version or not version_order:
                return False
            for spec in self.version_specifiers:
                if not spec.matches(current_version, version_order):
                    return False

        return True

    def result_is_expected(self, actual_status):
        """Check if an actual result matches expectations."""
        if self.is_skip() and actual_status == SKIP:
            return True
        return actual_status in self.expectations

    def __repr__(self):
        return 'APITestExpectation({!r}, {}, config={}, slow={})'.format(
            self.test_pattern,
            {EXPECTATION_NAMES.get(e, e) for e in self.expectations},
            self.configurations,
            self.slow_timeout)


class APITestExpectationParser(object):
    """Provides parsing facilities for lines in API test expectation files."""

    WEBKIT_BUG_PREFIX = 'webkit.org/b/'
    RADAR_BUG_PREFIX = 'rdar://'
    BUG_PATTERN = re.compile(r'Bug\((\w+)\)$', re.IGNORECASE)
    RADAR_PATTERN = re.compile(r'^rdar://(?:problem/)?(\d+)$')
    SLOW_TIMEOUT_PATTERN = re.compile(r'^Slow:(\d+)s?$', re.IGNORECASE)

    def __init__(self, version_tokens=None):
        """Initialize the parser.

        Args:
            version_tokens: Optional set of valid version token strings (lowercase)
        """
        self._version_tokens = version_tokens or set()

    def parse(self, filename, content):
        """Parse an expectations file and return list of expectation lines."""
        results = []
        line_number = 0
        for line in content.split('\n'):
            line_number += 1
            expectation, warnings = self._parse_line(filename, line, line_number)
            results.append((expectation, warnings))
        return results

    def _parse_line(self, filename, line, line_number):
        """Parse a single line from an expectations file."""
        warnings = []

        comment_index = line.find('#')
        if comment_index != -1:
            line = line[:comment_index]

        line = ' '.join(line.split())
        if not line:
            return None, warnings

        if line.startswith('//'):
            warnings.append(ExpectationWarning(
                filename, line_number, line,
                'Use "#" instead of "//" for comments'))
            return None, warnings

        bug_ids = []
        configurations = set()
        version_specifiers = []
        test_pattern = None
        expectations = set()
        slow_timeout = None

        tokens = line.split()
        state = 'start'

        for token in tokens:
            # Handle bug identifiers (webkit.org, rdar://, Bug())
            if token.startswith(self.WEBKIT_BUG_PREFIX) or token.startswith(self.RADAR_BUG_PREFIX) or token.lower().startswith('bug('):
                if state != 'start':
                    warnings.append(ExpectationWarning(
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
                        warnings.append(ExpectationWarning(
                            filename, line_number, line,
                            'Invalid radar format "{}". Use rdar://XXXXX or rdar://problem/XXXXX'.format(token)))
                        break
                else:
                    match = self.BUG_PATTERN.match(token)
                    if match:
                        bug_ids.append('Bug({})'.format(match.group(1)))
                    else:
                        warnings.append(ExpectationWarning(
                            filename, line_number, line,
                            'Unrecognized bug identifier "{}"'.format(token)))
                        break

            elif token == '[':
                if state == 'start':
                    state = 'configuration'
                elif state == 'name_found':
                    state = 'expectations'
                else:
                    warnings.append(ExpectationWarning(
                        filename, line_number, line,
                        'Unexpected "["'))
                    break

            elif token == ']':
                if state == 'configuration':
                    state = 'name'
                elif state == 'expectations':
                    state = 'done'
                else:
                    warnings.append(ExpectationWarning(
                        filename, line_number, line,
                        'Unexpected "]"'))
                    break

            elif state == 'configuration':
                token_lower = token.lower()
                # Get the category for this token
                category = get_token_category(token, self._version_tokens)
                if category is not None:
                    if category == ConfigurationCategory.VERSION:
                        # Try to parse as a version specifier with modifiers
                        spec = VersionSpecifier.parse(token)
                        if spec:
                            version_specifiers.append(spec)
                        else:
                            # Plain version token (e.g., "Sonoma" without +/-)
                            spec = VersionSpecifier(token, specifier_type=VersionSpecifier.Type.EXACT)
                            spec._original = token
                            version_specifiers.append(spec)
                    else:
                        configurations.add(token_lower)
                else:
                    warnings.append(ExpectationWarning(
                        filename, line_number, line,
                        'Unrecognized configuration "{}"'.format(token)))
                    break

            elif state == 'expectations':
                token_lower = token.lower()
                slow_match = self.SLOW_TIMEOUT_PATTERN.match(token)
                if slow_match:
                    slow_timeout = int(slow_match.group(1))
                    if slow_timeout < 1 or slow_timeout > 3600:
                        warnings.append(ExpectationWarning(
                            filename, line_number, line,
                            'Slow timeout must be between 1 and 3600 seconds, got {}'.format(slow_timeout)))
                        break
                    expectations.add(SLOW)
                elif token_lower == 'slow':
                    expectations.add(SLOW)
                elif token_lower in EXPECTATION_MAP:
                    expectations.add(EXPECTATION_MAP[token_lower])
                else:
                    warnings.append(ExpectationWarning(
                        filename, line_number, line,
                        'Unrecognized expectation "{}"'.format(token)))
                    break

            elif state == 'name_found':
                warnings.append(ExpectationWarning(
                    filename, line_number, line,
                    'Expecting "[", "#", or end of line instead of "{}"'.format(token)))
                break

            else:
                test_pattern = token
                state = 'name_found'

        if not warnings:
            if not test_pattern:
                warnings.append(ExpectationWarning(
                    filename, line_number, line,
                    'Did not find a test name'))
            elif state not in ('name_found', 'done'):
                warnings.append(ExpectationWarning(
                    filename, line_number, line,
                    'Missing a "]"'))

        if SKIP in expectations and len(expectations - {SKIP, SLOW}) > 0:
            warnings.append(ExpectationWarning(
                filename, line_number, line,
                'A test marked Skip must not have other expectations'))

        if SLOW in expectations and TIMEOUT in expectations:
            warnings.append(ExpectationWarning(
                filename, line_number, line,
                'A test cannot be both Slow and Timeout'))

        if not expectations and not warnings:
            expectations.add(PASS)

        if warnings:
            return None, warnings

        expectation = APITestExpectation(
            test_pattern=test_pattern,
            expectations=expectations,
            configurations=configurations,
            slow_timeout=slow_timeout,
            bug_ids=bug_ids,
            filename=filename,
            line_number=line_number,
            version_specifiers=version_specifiers,
        )
        return expectation, warnings


class APITestExpectationsModel(object):
    """In-memory model of all expectations with lookup by test name."""

    def __init__(self):
        self._exact_expectations = {}
        self._wildcard_expectations = []
        self._all_expectations = []
        self._seen_patterns = {}

    def add_expectation(self, expectation):
        """Add an expectation to the model. Later additions override earlier ones."""
        warning = None

        key = expectation.test_pattern
        if key in self._seen_patterns:
            prev_file, prev_line = self._seen_patterns[key]
            if prev_file == expectation.filename:
                warning = ExpectationWarning(
                    expectation.filename,
                    expectation.line_number,
                    expectation.test_pattern,
                    'Duplicate expectation (previous at line {})'.format(prev_line),
                    test=expectation.test_pattern
                )

        self._seen_patterns[key] = (expectation.filename, expectation.line_number)
        self._all_expectations.append(expectation)
        if expectation.is_wildcard:
            self._wildcard_expectations = [
                e for e in self._wildcard_expectations
                if e.test_pattern != expectation.test_pattern
            ]
            self._wildcard_expectations.append(expectation)
        else:
            self._exact_expectations[expectation.test_pattern] = expectation

        return warning

    def get_expectation(self, test_name, current_config=None):
        """Get the expectation for a specific test."""
        if current_config is None:
            current_config = set()

        if test_name in self._exact_expectations:
            exp = self._exact_expectations[test_name]
            if exp.matches_configuration(current_config):
                return exp

        best_match = None
        best_prefix_len = -1
        for exp in self._wildcard_expectations:
            if exp.matches_test(test_name) and exp.matches_configuration(current_config):
                prefix_len = len(exp._prefix) if exp._prefix else 0
                if prefix_len > best_prefix_len:
                    best_match = exp
                    best_prefix_len = prefix_len

        return best_match

    def get_expectation_or_pass(self, test_name, current_config=None):
        """Get expectation for test, defaulting to PASS if none specified."""
        exp = self.get_expectation(test_name, current_config)
        if exp:
            return exp
        return APITestExpectation(test_name, {PASS})

    def get_skipped_tests(self, all_tests, current_config=None):
        """Return set of test names that should be skipped."""
        skipped = set()
        for test in all_tests:
            exp = self.get_expectation(test, current_config)
            if exp and exp.is_skip():
                skipped.add(test)
        return skipped

    def get_slow_tests(self, all_tests, current_config=None):
        """Return dict of slow tests to their timeouts."""
        slow_tests = {}
        for test in all_tests:
            exp = self.get_expectation(test, current_config)
            if exp and exp.is_slow():
                slow_tests[test] = exp.slow_timeout
        return slow_tests

    def all_expectations(self):
        """Return all expectations for iteration."""
        return self._all_expectations


class APITestExpectations(object):
    """Main facade for loading and querying API test expectations."""

    def __init__(self, port, tests=None):
        self._port = port
        self._tests = set(tests) if tests else None
        self._model = APITestExpectationsModel()
        # Get version tokens from port if available
        version_tokens = set()
        if hasattr(port, 'api_test_version_tokens'):
            version_tokens = set(port.api_test_version_tokens().keys())
        self._parser = APITestExpectationParser(version_tokens=version_tokens)
        self._version_tokens = version_tokens
        self._warnings = []

    def model(self):
        """Return the underlying model."""
        return self._model

    def warnings(self):
        """Return list of all warnings from parsing."""
        return self._warnings

    def parse_all_expectations(self):
        """Load and parse all relevant expectations files."""
        for filepath in self._expectations_files():
            if self._port.host.filesystem.exists(filepath):
                _log.debug('Loading expectations from {}'.format(filepath))
                content = self._port.host.filesystem.read_text_file(filepath)
                self._parse_file(filepath, content)

    def _parse_file(self, filepath, content):
        """Parse a single expectations file and add to model."""
        results = self._parser.parse(filepath, content)
        for expectation, warnings in results:
            self._warnings.extend(warnings)
            if expectation:
                dup_warning = self._model.add_expectation(expectation)
                if dup_warning:
                    self._warnings.append(dup_warning)

    def _expectations_files(self):
        """Return list of expectations files in cascade order."""
        if hasattr(self._port, 'api_test_expectations_files'):
            return self._port.api_test_expectations_files()

        files = []
        base = self._api_test_expectations_dir()
        fs = self._port.host.filesystem

        generic_path = fs.join(base, 'TestExpectations')
        files.append(generic_path)

        for platform_dir in self._platform_cascade():
            platform_path = fs.join(base, 'platform', platform_dir, 'TestExpectations')
            files.append(platform_path)

        return files

    def _api_test_expectations_dir(self):
        """Return path to APITestExpectations directory."""
        if hasattr(self._port, 'api_test_expectations_dir'):
            return self._port.api_test_expectations_dir()
        return self._port.host.filesystem.join(
            self._port.path_from_webkit_base(), 'APITestExpectations')

    def _platform_cascade(self):
        """Return platform directories from least to most specific."""
        cascade = []
        port_name = self._port.port_name
        if 'mac' in port_name:
            cascade.append('mac')
        elif 'ios' in port_name:
            cascade.append('ios')
            if 'simulator' in port_name:
                cascade.append('ios-simulator')
        elif 'gtk' in port_name:
            cascade.append('gtk')
        elif 'wpe' in port_name:
            cascade.append('wpe')
        elif 'win' in port_name:
            cascade.append('win')
        return cascade

    def get_current_configuration(self):
        """Return set of current configuration strings."""
        config = set()
        if self._port.get_option('configuration'):
            config.add(self._port.get_option('configuration').lower())
        return config

    def lint(self, all_tests):
        """Validate expectations and return warnings."""
        lint_warnings = list(self._warnings)
        test_set = set(all_tests)

        for exp in self._model.all_expectations():
            if exp.is_wildcard:
                matches = [t for t in all_tests if exp.matches_test(t)]
                if not matches:
                    lint_warnings.append(ExpectationWarning(
                        exp.filename, exp.line_number, exp.test_pattern,
                        'Stale wildcard pattern - matches no tests',
                        test=exp.test_pattern))
            else:
                if exp.test_pattern not in test_set:
                    lint_warnings.append(ExpectationWarning(
                        exp.filename, exp.line_number, exp.test_pattern,
                        'Stale expectation - test does not exist',
                        test=exp.test_pattern))

        return lint_warnings


class LintWarning(object):
    """Represents a linting warning with optional fix."""
    def __init__(self, filename, line_number, message, fixable=False, fix_description=None):
        self.filename = filename
        self.line_number = line_number
        self.message = message
        self.fixable = fixable
        self.fix_description = fix_description

    def __str__(self):
        prefix = '[fixable] ' if self.fixable else ''
        return '{}:{}: {}{}'.format(self.filename, self.line_number, prefix, self.message)


class LineFix(object):
    """Represents a fix to apply to a specific line."""
    def __init__(self, line_number, new_content=None, delete=False):
        self.line_number = line_number
        self.new_content = new_content
        self.delete = delete


class APITestExpectationsLinter(object):
    """Linter for API test expectations files with auto-fix support."""

    def __init__(self, port, content, filepath, version_tokens=None):
        """Initialize the linter.

        Args:
            port: The port object
            content: The file content to lint
            filepath: Path to the file being linted
            version_tokens: Optional set of valid version token strings
        """
        self._port = port
        self._content = content
        self._filepath = filepath
        self._version_tokens = version_tokens or set()
        self._lines = content.split('\n')
        self._parsed_entries = []  # List of (line_num, raw_line, parsed_data)
        self._warnings = []
        self._fixes = []

    def lint(self):
        """Run all linting rules and return warnings."""
        self._parse_all_lines()
        self._check_category_ordering()
        self._check_alphabetical_ordering()
        self._check_combination_collapse()
        self._check_universal_skip()
        return self._warnings

    def get_fixes(self):
        """Return list of fixes that can be applied."""
        return self._fixes

    def apply_fixes(self):
        """Apply all fixes and return the fixed content."""
        if not self._fixes:
            return self._content

        # Sort fixes in reverse line order to preserve line numbers
        sorted_fixes = sorted(self._fixes, key=lambda f: -f.line_number)

        lines = self._lines[:]
        for fix in sorted_fixes:
            if fix.delete:
                del lines[fix.line_number - 1]
            elif fix.new_content is not None:
                lines[fix.line_number - 1] = fix.new_content

        return '\n'.join(lines)

    def _parse_all_lines(self):
        """Parse all lines to extract test entries."""
        for line_num, line in enumerate(self._lines, 1):
            # Skip comments and blank lines
            stripped = line.strip()
            if not stripped or stripped.startswith('#'):
                continue

            # Extract configuration tokens if present
            config_tokens = []
            test_pattern = None
            expectations = []

            # Simple parsing to extract parts
            comment_idx = line.find('#')
            if comment_idx != -1:
                line_content = line[:comment_idx]
            else:
                line_content = line

            # Find configuration block
            config_match = re.search(r'\[\s*([^\]]*)\s*\]', line_content)
            if config_match:
                config_str = config_match.group(1)
                # Check if this is config or expectations based on position
                config_start = config_match.start()
                before_config = line_content[:config_start].strip()

                # If there's a test pattern before the bracket, it's expectations
                # Remove bug identifiers from before_config
                before_words = before_config.split()
                non_bug_words = [w for w in before_words
                                 if not w.startswith('webkit.org/b/')
                                 and not w.startswith('rdar://')
                                 and not w.lower().startswith('bug(')]

                if non_bug_words:
                    # There's a test pattern, so this bracket is expectations
                    test_pattern = non_bug_words[-1]
                    expectations = config_str.split()
                else:
                    # No test pattern yet, so this is config
                    config_tokens = config_str.split()
                    # Look for test pattern after
                    rest = line_content[config_match.end():].strip()
                    # Find expectations bracket if present
                    exp_match = re.search(r'\[\s*([^\]]*)\s*\]', rest)
                    if exp_match:
                        test_pattern = rest[:exp_match.start()].strip()
                        expectations = exp_match.group(1).split()
                    else:
                        test_pattern = rest
            else:
                # No brackets, find test pattern
                words = line_content.split()
                for word in words:
                    if not word.startswith('webkit.org/b/') and \
                       not word.startswith('rdar://') and \
                       not word.lower().startswith('bug('):
                        test_pattern = word
                        break

            if test_pattern:
                self._parsed_entries.append({
                    'line_number': line_num,
                    'raw_line': line,
                    'config_tokens': config_tokens,
                    'test_pattern': test_pattern,
                    'expectations': expectations,
                })

    def _check_category_ordering(self):
        """Check that tokens in [ ] blocks appear in correct category order."""
        for entry in self._parsed_entries:
            config_tokens = entry['config_tokens']
            if len(config_tokens) < 2:
                continue

            last_category = None
            for token in config_tokens:
                category = get_token_category(token, self._version_tokens)
                if category is None:
                    continue

                if last_category is not None and category < last_category:
                    self._warnings.append(LintWarning(
                        self._filepath,
                        entry['line_number'],
                        'Configuration tokens out of order: "{}" ({}) appears after {} category. '
                        'Expected order: Platform -> Version -> Style -> Hardware -> Architecture -> Flavor'.format(
                            token, CATEGORY_NAMES.get(category, str(category)),
                            CATEGORY_NAMES.get(last_category, str(last_category))),
                        fixable=True
                    ))
                    # Create fix with reordered tokens
                    self._add_reorder_fix(entry)
                    break

                last_category = category

    def _check_alphabetical_ordering(self):
        """Check alphabetical ordering within categories and in the file."""
        # Check alphabetical ordering within categories for each entry
        for entry in self._parsed_entries:
            config_tokens = entry['config_tokens']
            if len(config_tokens) < 2:
                continue

            # Group tokens by category
            by_category = defaultdict(list)
            for token in config_tokens:
                category = get_token_category(token, self._version_tokens)
                if category:
                    by_category[category].append(token.lower())

            # Check each category is alphabetically ordered
            for category, tokens in by_category.items():
                sorted_tokens = sorted(tokens)
                if tokens != sorted_tokens:
                    self._warnings.append(LintWarning(
                        self._filepath,
                        entry['line_number'],
                        'Tokens in {} category not alphabetically ordered: {} should be {}'.format(
                            CATEGORY_NAMES.get(category, str(category)),
                            tokens, sorted_tokens),
                        fixable=True
                    ))
                    self._add_reorder_fix(entry)

        # Check test entries are alphabetically ordered in the file
        prev_test = None
        prev_line_num = None
        for entry in self._parsed_entries:
            test = entry['test_pattern']
            if prev_test and test.lower() < prev_test.lower():
                self._warnings.append(LintWarning(
                    self._filepath,
                    entry['line_number'],
                    'Test "{}" should appear before "{}" (line {}) for alphabetical order'.format(
                        test, prev_test, prev_line_num),
                    fixable=True,
                    fix_description='Reorder test entries alphabetically'
                ))
            prev_test = test
            prev_line_num = entry['line_number']

    def _check_combination_collapse(self):
        """Check for entries that can be collapsed."""
        # Group entries by test pattern
        by_test = defaultdict(list)
        for entry in self._parsed_entries:
            by_test[entry['test_pattern']].append(entry)

        for test_pattern, entries in by_test.items():
            if len(entries) < 2:
                continue

            # Check if all entries have the same expectations
            expectations_set = set(tuple(sorted(e['expectations'])) for e in entries)
            if len(expectations_set) != 1:
                continue  # Different expectations, can't collapse

            # Check which categories are fully covered
            for category, all_values in CATEGORY_TOKENS.items():
                if not all_values:
                    continue

                covered_values = set()
                entries_with_category = []
                for entry in entries:
                    cat_values = {t.lower() for t in entry['config_tokens']
                                  if get_token_category(t, self._version_tokens) == category}
                    if cat_values:
                        covered_values.update(cat_values)
                        entries_with_category.append(entry)

                if covered_values == all_values and len(entries_with_category) == len(all_values):
                    line_nums = [e['line_number'] for e in entries_with_category]
                    self._warnings.append(LintWarning(
                        self._filepath,
                        line_nums[0],
                        'Test "{}" has entries covering all {} values (lines {}). '
                        'Can be collapsed by removing {} specifier.'.format(
                            test_pattern,
                            CATEGORY_NAMES.get(category, str(category)),
                            ', '.join(str(n) for n in line_nums),
                            CATEGORY_NAMES.get(category, str(category))),
                        fixable=True
                    ))
                    # Mark extra lines for deletion, update first line
                    self._add_collapse_fix(entries_with_category, category)

    def _check_universal_skip(self):
        """Check for tests skipped on all configurations."""
        # Group entries by test pattern
        by_test = defaultdict(list)
        for entry in self._parsed_entries:
            by_test[entry['test_pattern']].append(entry)

        for test_pattern, entries in by_test.items():
            # Check if all entries are Skip
            all_skip = all(
                any(e.lower() == 'skip' for e in entry['expectations'])
                for entry in entries
            )
            if not all_skip:
                continue

            # Check if there's an unconditional entry or all platforms covered
            has_unconditional = any(not entry['config_tokens'] for entry in entries)
            covered_platforms = set()
            for entry in entries:
                for token in entry['config_tokens']:
                    if token.lower() in PLATFORM_TOKENS:
                        covered_platforms.add(token.lower())

            if has_unconditional or covered_platforms >= PLATFORM_TOKENS:
                line_nums = [e['line_number'] for e in entries]
                self._warnings.append(LintWarning(
                    self._filepath,
                    line_nums[0],
                    'Test "{}" is skipped on all configurations (lines {}). '
                    'Consider removing the test entirely.'.format(
                        test_pattern,
                        ', '.join(str(n) for n in line_nums)),
                    fixable=False  # Removing tests is a manual decision
                ))

    def _add_reorder_fix(self, entry):
        """Add a fix to reorder tokens in an entry."""
        config_tokens = entry['config_tokens']
        if not config_tokens:
            return

        # Sort tokens by category, then alphabetically within category
        def sort_key(token):
            category = get_token_category(token, self._version_tokens)
            return (category or ConfigurationCategory.FLAVOR, token.lower())

        sorted_tokens = sorted(config_tokens, key=sort_key)
        if sorted_tokens == config_tokens:
            return  # Already sorted

        # Reconstruct the line with sorted tokens
        old_line = entry['raw_line']

        # Find and replace the config block
        new_config = '[ {} ]'.format(' '.join(sorted_tokens))
        new_line = re.sub(r'\[\s*[^\]]*\s*\]', new_config, old_line, count=1)

        # Only add if not already in fixes
        existing = [f for f in self._fixes if f.line_number == entry['line_number']]
        if not existing:
            self._fixes.append(LineFix(entry['line_number'], new_line))

    def _add_collapse_fix(self, entries, category):
        """Add fixes to collapse entries by removing category specifier."""
        if len(entries) < 2:
            return

        # Keep first entry, modify to remove category, delete rest
        first_entry = entries[0]
        config_tokens = [t for t in first_entry['config_tokens']
                         if get_token_category(t, self._version_tokens) != category]

        old_line = first_entry['raw_line']

        if config_tokens:
            new_config = '[ {} ]'.format(' '.join(config_tokens))
            new_line = re.sub(r'\[\s*[^\]]*\s*\]', new_config, old_line, count=1)
        else:
            # Remove the entire config block
            new_line = re.sub(r'\[\s*[^\]]*\s*\]\s*', '', old_line, count=1)

        self._fixes.append(LineFix(first_entry['line_number'], new_line))

        # Delete other entries
        for entry in entries[1:]:
            self._fixes.append(LineFix(entry['line_number'], delete=True))
