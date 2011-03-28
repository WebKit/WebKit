#!/usr/bin/env python
#
# Copyright (C) 2010 Apple Inc. All rights reserved.
# Copyright (C) 2011 Patrick Gansterer <paroga@paroga.com>
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

"""Unit test for changelog.py."""

import changelog
import unittest


class ChangeLogCheckerTest(unittest.TestCase):
    """Tests ChangeLogChecker class."""

    def assert_no_error(self, changelog_data):
        def handle_style_error(line_number, category, confidence, message):
            self.fail('Unexpected error: %d %s %d %s' % (line_number, category, confidence, message))
        checker = changelog.ChangeLogChecker('ChangeLog', handle_style_error)
        checker.check(changelog_data.split('\n'))

    def assert_error(self, expected_line_number, expected_category, changelog_data):
        self.had_error = False

        def handle_style_error(line_number, category, confidence, message):
            self.had_error = True
            self.assertEquals(expected_line_number, line_number)
            self.assertEquals(expected_category, category)

        checker = changelog.ChangeLogChecker('ChangeLog', handle_style_error)
        checker.check(changelog_data.split('\n'))
        self.assertTrue(self.had_error)

    def mock_handle_style_error(self):
        pass

    def test_init(self):
        checker = changelog.ChangeLogChecker('ChangeLog', self.mock_handle_style_error)
        self.assertEquals(checker.file_path, 'ChangeLog')
        self.assertEquals(checker.handle_style_error, self.mock_handle_style_error)

    def test_missing_bug_number(self):
        entries = [
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        http://bugs.webkit.org/show_bug.cgi?id=\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        https://bugs.webkit.org/show_bug.cgi?id=\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        http://webkit.org/b/\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        http://trac.webkit.org/changeset/12345\n',
            'Example bug\n        https://bugs.webkit.org/show_bug.cgi\n\n2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n',
            'Example bug\n        More text about bug.\n\n2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n',
        ]

        for entry in entries:
            self.assert_error(1, 'changelog/bugnumber', entry)

    def test_no_error(self):
        entries = [
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        http://bugs.webkit.org/show_bug.cgi?id=12345\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        https://bugs.webkit.org/show_bug.cgi?id=12345\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Example bug\n        http://webkit.org/b/12345\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Unreview build fix for r12345.\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Fix build after a bad change.\n',
            '2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n\n        Fix example port build.\n',
            'Example bug\n        https://bugs.webkit.org/show_bug.cgi?id=12345\n\n2011-01-01  Patrick Gansterer  <paroga@paroga.com>\n',
        ]

        for entry in entries:
            self.assert_no_error(entry)

if __name__ == '__main__':
    unittest.main()
