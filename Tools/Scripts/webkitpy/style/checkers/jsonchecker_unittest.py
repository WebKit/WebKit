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

import unittest

from mock import Mock

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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
        )
        # We can't uniquely determine what line "web-platform-tests/exists" is on, so we
        # return line 0.
        error_handler.assert_any_call(
            0,
            "json/syntax",
            5,
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
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
            'Each value must be one of "import", "skip", or "skip-new-directories"',
        )
