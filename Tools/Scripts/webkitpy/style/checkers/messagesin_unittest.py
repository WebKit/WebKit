# Copyright (C) 2013 University of Szeged. All rights reserved.
# Copyright (C) 2013 Samsung Electronics. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit test for messagesin.py."""

import unittest

from messagesin import MessagesInChecker


class MessagesInCheckerStyleTestCase(unittest.TestCase):
    """Test MessagesInChecker style checking issues."""

    test_file_content = """#if ENABLE(SOME_GUARD)

messages -> GoodName {
    # BadHandler contains WTF:: prefix that should raise an error
\tBadHandler(Vector<WTF::String> list) -> (bool result)

    # GoodHandler is OK
    GoodHandler(Vector<String> list, WebKit::SomeClass data) -> (bool result)
}

#endif
"""

    expected_errors = set([
        (5, 'whitespace/tab', 5, 'Line contains tab character.'),
        (5, 'build/messagesin/wtf', 5, 'Line contains WTF:: prefix.'),
    ])

    def test_checker(self):
        """Test for errors"""

        errors_found = set()

        def error_handler_for_test(line_number, category, confidence, message):
            errors_found.add((line_number, category, confidence, message))

        messages_in_checker = MessagesInChecker('foo.messages.in', error_handler_for_test)
        lines = self.test_file_content.split('\n')
        messages_in_checker.check(lines)
        self.assertEqual(self.expected_errors, errors_found)


class MessagesInCheckerTestCase(unittest.TestCase):
    """Test MessagesInChecker technical issues."""

    def test_init(self):

        def error_handler_for_test(line_number, category, confidence, message):
            pass

        checker = MessagesInChecker('foo.messages.in', error_handler_for_test)
        self.assertEqual(checker.file_path, 'foo.messages.in')
        self.assertEqual(checker.handle_style_error, error_handler_for_test)
