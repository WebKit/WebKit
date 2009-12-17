#!/usr/bin/python
# Copyright (C) 2009 Google Inc. All rights reserved.
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

"""Unit test for text_style.py."""

import unittest

import text_style


class TextStyleTestCase(unittest.TestCase):
    """TestCase for text_style.py"""

    def assertNoError(self, lines):
        """Asserts that the specified lines has no errors."""
        self.had_error = False

        def error_for_test(filename, line_number, category, confidence, message):
            """Records if an error occurs."""
            self.had_error = True

        text_style.process_file_data('', lines, error_for_test)
        self.assert_(not self.had_error, '%s should not have any errors.' % lines)

    def assertError(self, lines, expected_line_number):
        """Asserts that the specified lines has an error."""
        self.had_error = False

        def error_for_test(filename, line_number, category, confidence, message):
            """Checks if the expected error occurs."""
            self.assertEquals(expected_line_number, line_number)
            self.assertEquals('whitespace/tab', category)
            self.had_error = True

        text_style.process_file_data('', lines, error_for_test)
        self.assert_(self.had_error, '%s should have an error [whitespace/tab].' % lines)


    def test_no_error(self):
        """Tests for no error cases."""
        self.assertNoError([''])
        self.assertNoError(['abc def', 'ggg'])


    def test_error(self):
        """Tests for error cases."""
        self.assertError(['2009-12-16\tKent Tamura\t<tkent@chromium.org>'], 1)
        self.assertError(['2009-12-16 Kent Tamura <tkent@chromium.org>',
                          '',
                          '\tReviewed by NOBODY.'], 3)


    def test_can_handle(self):
        """Tests for text_style.can_handle()."""
        self.assert_(not text_style.can_handle(''))
        self.assert_(not text_style.can_handle('-'))
        self.assert_(text_style.can_handle('ChangeLog'))
        self.assert_(text_style.can_handle('WebCore/ChangeLog'))
        self.assert_(text_style.can_handle('FooChangeLog.bak'))
        self.assert_(text_style.can_handle('WebKitTools/Scripts/check-webkit=style'))
        self.assert_(text_style.can_handle('WebKitTools/Scripts/modules/text_style.py'))
        self.assert_(not text_style.can_handle('WebKitTools/Scripts'))

        self.assert_(text_style.can_handle('foo.css'))
        self.assert_(text_style.can_handle('foo.html'))
        self.assert_(text_style.can_handle('foo.idl'))
        self.assert_(text_style.can_handle('foo.js'))
        self.assert_(text_style.can_handle('WebCore/inspector/front-end/inspector.js'))
        self.assert_(text_style.can_handle('foo.mm'))
        self.assert_(text_style.can_handle('foo.php'))
        self.assert_(text_style.can_handle('foo.pm'))
        self.assert_(text_style.can_handle('foo.py'))
        self.assert_(text_style.can_handle('foo.txt'))
        self.assert_(not text_style.can_handle('foo.c'))
        self.assert_(not text_style.can_handle('foo.c'))
        self.assert_(not text_style.can_handle('foo.c'))
        self.assert_(not text_style.can_handle('foo.png'))
        self.assert_(not text_style.can_handle('foo.c/bar.png'))
        self.assert_(not text_style.can_handle('WebKit/English.lproj/Localizable.strings'))
        self.assert_(not text_style.can_handle('Makefile'))
        self.assert_(not text_style.can_handle('WebCore/Android.mk'))
        self.assert_(not text_style.can_handle('LayoutTests/inspector/console-tests.js'))


if __name__ == '__main__':
    unittest.main()
