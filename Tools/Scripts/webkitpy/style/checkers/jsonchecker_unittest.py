# Copyright (C) 2010 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit test for jsonchecker.py."""

import json
import unittest
from unittest.mock import Mock

from pyfakefs import fake_filesystem_unittest

from webkitpy.style.checkers import jsonchecker


class MockErrorHandler(object):
    def __init__(self, handle_style_error):
        self.turned_off_filtering = False
        self._handle_style_error = handle_style_error

    def turn_off_line_filtering(self):
        self.turned_off_filtering = True

    def __call__(self, line_number, category, confidence, message):
        self._handle_style_error(self, line_number, category, confidence, message)
        return True


class JSONCheckerTest(unittest.TestCase):
    """Tests JSONChecker class."""

    def test_line_number_from_json_exception(self):
        tests = (
            (0, 'No JSON object could be decoded'),
            (2, 'Expecting property name: line 2 column 1 (char 2)'),
            (3, 'Expecting object: line 3 column 1 (char 15)'),
            (9, 'Expecting property name: line 9 column 21 (char 478)'),
        )
        for expected_line, message in tests:
            self.assertEqual(expected_line, jsonchecker.JSONChecker.line_number_from_json_exception(ValueError(message)))

    def assert_no_error(self, json_data):
        def handle_style_error(mock_error_handler, line_number, category, confidence, message):
            self.fail('Unexpected error: %d %s %d %s' % (line_number, category, confidence, message))

        error_handler = MockErrorHandler(handle_style_error)
        checker = jsonchecker.JSONChecker('foo.json', error_handler)
        checker.check(json_data.split('\n'))
        self.assertTrue(error_handler.turned_off_filtering)

    def assert_error(self, expected_line_number, expected_category, json_data):
        def handle_style_error(mock_error_handler, line_number, category, confidence, message):
            mock_error_handler.had_error = True
            self.assertEqual(expected_line_number, line_number)
            self.assertEqual(expected_category, category)
            self.assertIn(category, jsonchecker.JSONChecker.categories)

        error_handler = MockErrorHandler(handle_style_error)
        error_handler.had_error = False

        checker = jsonchecker.JSONChecker('foo.json', error_handler)
        checker.check(json_data.split('\n'))
        self.assertTrue(error_handler.had_error)
        self.assertTrue(error_handler.turned_off_filtering)

    def mock_handle_style_error(self):
        pass

    def test_conflict_marker(self):
        self.assert_error(1, 'json/syntax', '<<<<<<< HEAD\n{\n}\n')

    def test_single_quote(self):
        self.assert_error(2, 'json/syntax', "{\n'workers': []\n}\n")

    def test_init(self):
        error_handler = MockErrorHandler(self.mock_handle_style_error)
        checker = jsonchecker.JSONChecker('foo.json', error_handler)
        self.assertEqual(checker._handle_style_error, error_handler)

    def test_no_error(self):
        self.assert_no_error("""{
    "workers":     [ { "name": "test-worker", "platform": "*" },
                    { "name": "apple-xserve-4", "platform": "mac-snowleopard" }
                  ],

    "builders":   [ { "name": "SnowLeopard Intel Release (Build)", "type": "Build", "builddir": "snowleopard-intel-release",
                      "platform": "mac-snowleopard", "configuration": "release", "architectures": ["x86_64"],
                      "workernames": ["apple-xserve-4"]
                    }
                   ],

    "schedulers": [ { "type": "PlatformSpecificScheduler", "platform": "mac-snowleopard", "branch": "trunk", "treeStableTimer": 45.0,
                      "builderNames": ["SnowLeopard Intel Release (Build)", "SnowLeopard Intel Debug (Build)"]
                    }
                  ]
}
""")


class JSONImportExpectationsCheckerTest(fake_filesystem_unittest.TestCase):
    def setUp(self):
        self.setUpPyfakefs()

    def test_valid_json(self):
        error_handler = Mock()
        checker = jsonchecker.JSONChecker("foo.json", error_handler)
        checker.check(["{"])
        # We can't verify either line number or message, as these come from the JSON
        # parser, and vary between different Python versions.
        _, category, confidence, _ = error_handler.call_args[0]
        self.assertEqual(category, "json/syntax")
        self.assertEqual(confidence, 5)

    def test_top_level_is_obj(self):
        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker("foo.json", error_handler)
        checker.check(["{}"])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker("foo.json", error_handler)
        checker.check(["[]"])
        error_handler.assert_called_once_with(
            0, "json/syntax", 5, "The top-level data must be an object"
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker("foo.json", error_handler)
        checker.check(["true"])
        error_handler.assert_called_once_with(
            0, "json/syntax", 5, "The top-level data must be an object"
        )

    def test_wpt_only(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/abc")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/abc": "import"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests": "import"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-test/abc": "skip"}'])
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            "Each key must start with 'web-platform-tests/', got 'web-platform-test/abc'",
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"csswg-test/CSS2": "skip"}'])
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            "Each key must start with 'web-platform-tests/', got 'csswg-test/CSS2'",
        )

    def test_valid_values(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/exists")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/non-existing": "skip"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/exists": "import"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/exists": "nonsense"}'])
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

    def test_skip_existence(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/exists")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/a/b")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/non-existing": "import"}'])
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            "'web-platform-tests/non-existing' does not exist and is not skipped",
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/non-existing": "skip"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/exists": "import"}'])
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/exists": "skip"}'])
        error_handler.assert_called_once_with(
            1, "json/syntax", 5, "'web-platform-tests/exists' does exist and is skipped"
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            ['{"web-platform-tests/a": "skip", "web-platform-tests/a/b": "import" }']
        )
        error_handler.assert_not_called()

    def test_no_trailing_slash(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/exists")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/non-existing/": "skip"}'])
        error_handler.assert_called_once_with(
            1, "json/syntax", 5, "'web-platform-tests/non-existing/' has trailing slash"
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(['{"web-platform-tests/exists/": "import"}'])
        error_handler.assert_called_once_with(
            1, "json/syntax", 5, "'web-platform-tests/exists/' has trailing slash"
        )

    def test_redundant(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/a/b")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            ['{"web-platform-tests/a": "skip", "web-platform-tests/a/b": "import"}']
        )
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            ['{"web-platform-tests/a": "import", "web-platform-tests/a/x": "skip"}']
        )
        error_handler.assert_not_called()

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            ['{"web-platform-tests/x": "skip", "web-platform-tests/x/y": "skip"}']
        )
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            "'web-platform-tests/x/y' is redundant, 'web-platform-tests/x' already defines 'skip'",
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            ['{"web-platform-tests/a": "import", "web-platform-tests/a/b": "import"}']
        )
        error_handler.assert_called_once_with(
            1,
            "json/syntax",
            5,
            "'web-platform-tests/a/b' is redundant, 'web-platform-tests/a' already defines 'import'",
        )

    def test_line_no_attribution(self):
        self.fs.create_dir("/mock/test/directory")
        self.fs.cwd = "/mock/test/directory"
        self.fs.create_dir("/mock/test/directory/resources")
        self.fs.create_dir("/mock/test/directory/web-platform-tests/exists")

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(["{", '"web-platform-tests/exists": "nonsense"', "}"])
        error_handler.assert_called_once_with(
            2,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(["{", r'"web-pl\u0061tform-tests/exists": "nonsense"', "}"])
        error_handler.assert_called_once_with(
            2,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            [
                "{",
                '"web-platform-tests/a": "web-platform-tests/exists",',
                r'"web-pl\u0061tform-tests/exists": "nonsense"',
                "}",
            ]
        )
        self.assertEqual(error_handler.call_count, 2)
        error_handler.assert_any_call(
            2,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )
        # We can't uniquely determine what line "web-platform-tests/exists" is on, so we
        # return line 0.
        error_handler.assert_any_call(
            0,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            [
                "{",
                '"web-platform-tests/a":',
                '"nonsense"',
                "}",
            ]
        )
        error_handler.assert_called_once_with(
            2,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            [
                "{",
                '"web-platform-tests/exists": "import",',
                '"web-platform-tests/a": "nonsense"',
                "}",
            ]
        )
        error_handler.assert_called_once_with(
            3,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )

        error_handler = Mock()
        checker = jsonchecker.JSONImportExpectationsChecker(
            "resources/foo.json", error_handler
        )
        checker.check(
            [
                "{",
                '"web-platform-tests/exists": "web-platform-tests/exists"',
                "}",
            ]
        )
        error_handler.assert_called_once_with(
            2,
            "json/syntax",
            5,
            'Each value must be one of "import", "import-no-rewrite", "skip", or "skip-new-directories"',
        )


class JSONQuirkTableCheckerTest(fake_filesystem_unittest.TestCase):
    BEHAVIORS_YAML = '\n'.join([
        '# A comment.',
        'PlainQuirk:',
        '',
        'ScriptQuirk:',
        '  parameters: [Script]',
        '  conditions: [SecondaryURL]',
        '  implementation: custom',
        '',
        'RequiredSelectorQuirk:',
        '  conditions: [ElementSelector]',
        '  conditionsRequired: true',
    ])
    BUILD_CONDITIONS = '# Comment.\n\nmac: PLATFORM(MAC)\niOS: PLATFORM(IOS)\n'
    ENVIRONMENTS = 'enum class URLEnvironment : uint8_t {\n    SmallScreen,\n};\n'

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_file('/page/QuirkBehaviors.yaml', contents=self.BEHAVIORS_YAML)
        self.fs.create_file('/page/QuirkBuildConditions.yaml', contents=self.BUILD_CONDITIONS)
        self.fs.create_file('/page/QuirkMatchPattern.h', contents=self.ENVIRONMENTS)

    def errors_for(self, table):
        error_handler = Mock()
        checker = jsonchecker.JSONQuirkTableChecker('/page/QuirkTable.json', error_handler)
        checker.check(json.dumps(table, indent=4).split('\n'))
        return [call[0][3] for call in error_handler.call_args_list]

    def errors_for_row(self, **row):
        row.setdefault('matches', ['*://*.example.com/*'])
        row.setdefault('behaviors', [{'id': 'PlainQuirk'}])
        # Passing None for a field leaves it out of the row.
        return self.errors_for({'quirks': [{field: value for field, value in row.items() if value is not None}]})

    def assert_row_error(self, expected, **row):
        errors = self.errors_for_row(**row)
        self.assertTrue(any(expected in error for error in errors), 'expected an error containing %r, got %r' % (expected, errors))

    def test_valid_table(self):
        self.assertEqual(self.errors_for({'quirks': [
            {'bugs': ['rdar://1', 'https://webkit.org/b/2'], 'comment': 'Why.', 'matches': ['*://*.example.com/*'], 'available': 'mac || iOS', 'environment': 'SmallScreen', 'behaviors': [
                {'id': 'PlainQuirk', 'bugs': ['rdar://3']},
                {'id': 'ScriptQuirk', 'script': 'x', 'secondaryURL': ['*://cdn.example.com/*']},
                {'id': 'RequiredSelectorQuirk', 'elementSelector': '.x'},
            ]},
            {'embeddedMatches': ['*://*.example.org/*'], 'behaviors': [{'id': 'ScriptQuirk', 'script': 'x'}]},
        ]}), [])

    def test_missing_sibling_file(self):
        self.fs.remove('/page/QuirkBehaviors.yaml')
        self.assertEqual(self.errors_for({'quirks': []}), ['Could not read QuirkBehaviors.yaml, which QuirkTable.json is checked against.'])

    def assert_yaml_rejected(self, file_name, contents, expected):
        self.fs.remove('/page/' + file_name)
        self.fs.create_file('/page/' + file_name, contents=contents)
        errors = self.errors_for({'quirks': []})
        self.assertEqual(len(errors), 1, errors)
        self.assertTrue(errors[0].startswith(expected), errors)

    def test_non_flat_behaviors_yaml(self):
        self.assert_yaml_rejected('QuirkBehaviors.yaml', 'ScriptQuirk:\n  parameters:\n    - Script\n', 'QuirkBehaviors.yaml:2: expected "BehaviorName:" or an indented "key: value"')
        self.assert_yaml_rejected('QuirkBehaviors.yaml', '  parameters: [Script]\n', 'QuirkBehaviors.yaml:1: expected "BehaviorName:" or an indented "key: value"')
        self.assert_yaml_rejected('QuirkBehaviors.yaml', 'ScriptQuirk:\n  parameters: Script\n', 'QuirkBehaviors.yaml:2: expected "parameters" to be an inline list')
        self.assert_yaml_rejected('QuirkBehaviors.yaml', 'ScriptQuirk:\n  conditionsRequired: yes\n', 'QuirkBehaviors.yaml:2: expected "conditionsRequired" to be true or false')

    def test_non_flat_build_conditions_yaml(self):
        self.assert_yaml_rejected('QuirkBuildConditions.yaml', 'mac:\n  guard: PLATFORM(MAC)\n', 'QuirkBuildConditions.yaml:1: expected "name: condition"')
        self.assert_yaml_rejected('QuirkBuildConditions.yaml', 'mac: PLATFORM(MAC)\n  iOS: PLATFORM(IOS)\n', 'QuirkBuildConditions.yaml:2: expected "name: condition"')

    def test_top_level(self):
        for table in ([], {'rows': []}, {'quirks': {}}, {'quirks': [], 'extra': 1}):
            self.assertEqual(self.errors_for(table), ['The top level must be an object whose only key is "quirks", an array of rows.'])

    def test_row_structure(self):
        self.assert_row_error('a row must have "matches" or "embeddedMatches"', matches=None)
        self.assert_row_error('unknown field "matchez"', matchez=[])
        self.assert_row_error('"matches" must be a non-empty array', matches=[])
        self.assert_row_error('"behaviors" must be a non-empty array', behaviors=[])
        self.assert_row_error('"queryContains" must be a non-empty string', queryContains='')
        self.assert_row_error('"environment" must name a URLEnvironment', environment='Toaster')

    def test_available(self):
        self.assertEqual(self.errors_for_row(available='mac && iOS'), [])
        for available in ('!mac', '!!mac', '!mac && iOS', '!(mac || iOS)', '(mac || iOS) && !iOS', '((mac))', 'mac||iOS'):
            self.assertEqual(self.errors_for_row(available=available), [], available)
        self.assert_row_error('"available" must be', available='toaster')
        self.assert_row_error('"available" must be', available='mac ||')
        self.assert_row_error('"available" must be', available='')
        for available in ('!', '(mac', 'mac)', '()', 'mac iOS', 'mac ! iOS', 'mac & iOS', 'mac &', 'mac |', '!toaster', '(' * 100 + 'mac' + ')' * 100):
            self.assert_row_error('"available" must be', available=available)

    def test_bugs_and_comment(self):
        self.assert_row_error('"bugs" must be', bugs=[])
        self.assert_row_error('"bugs" must be', bugs=['rdar://problem/1'])
        self.assert_row_error('"bugs" must be', bugs=['https://bugs.webkit.org/show_bug.cgi?id=1'])
        self.assert_row_error('"bugs" lists a bug more than once', bugs=['rdar://1', 'rdar://1'])
        self.assert_row_error('"comment" must be a non-empty string', comment=['two', 'lines'])
        self.assert_row_error('"bugs" must be', behaviors=[{'id': 'PlainQuirk', 'bugs': ['nope']}])

    def test_behaviors(self):
        self.assert_row_error('"NoSuchQuirk" is not a behavior', behaviors=[{'id': 'NoSuchQuirk'}])
        self.assert_row_error('a behavior must have an "id"', behaviors=[{'comment': 'No id.'}])
        self.assert_row_error('"id" must be a non-empty string', behaviors=[{'id': 5}])
        self.assert_row_error('"id" must be a non-empty string', behaviors=[{'id': ['PlainQuirk']}])
        self.assert_row_error('"behaviors" lists PlainQuirk more than once', behaviors=[{'id': 'PlainQuirk'}, {'id': 'PlainQuirk'}])
        self.assert_row_error('"behaviors" lists ScriptQuirk more than once', behaviors=[{'id': 'ScriptQuirk', 'script': 'a'}, {'id': 'ScriptQuirk', 'script': 'b'}])
        self.assert_row_error('unknown field "colour"', behaviors=[{'id': 'PlainQuirk', 'colour': 'red'}])
        self.assert_row_error('ScriptQuirk: needs "script"', behaviors=[{'id': 'ScriptQuirk'}])
        self.assert_row_error('PlainQuirk: does not take "script"', behaviors=[{'id': 'PlainQuirk', 'script': 'x'}])
        self.assert_row_error('PlainQuirk: does not take "elementSelector"', behaviors=[{'id': 'PlainQuirk', 'elementSelector': '.x'}])
        self.assert_row_error('RequiredSelectorQuirk: needs "elementSelector"', behaviors=[{'id': 'RequiredSelectorQuirk'}])
        self.assert_row_error('"script" must be a non-empty string', behaviors=[{'id': 'ScriptQuirk', 'script': ''}])
        self.assert_row_error('"secondaryURL" must be a non-empty array of strings', behaviors=[{'id': 'ScriptQuirk', 'script': 'x', 'secondaryURL': []}])
