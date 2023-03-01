# Copyright (C) 2019 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from werkzeug.exceptions import BadRequest
from webkitflaskpy.util import boolean_query, limit_for_query, unescape_argument


class UtilTest(unittest.TestCase):

    def test_boolean_query(self):
        self.assertTrue(all(boolean_query('True', 'true')))
        self.assertTrue(all(boolean_query('Yes', 'yes')))
        self.assertTrue(all(boolean_query('1', '100')))

        self.assertFalse(any(boolean_query('False', 'false')))
        self.assertFalse(any(boolean_query('No', 'no')))
        self.assertFalse(any(boolean_query('0', 'any string')))

    def test_limit_decorator(self):
        @limit_for_query()
        def func(limit=None):
            return limit

        self.assertEqual(func(), 100)
        self.assertEqual(func(limit=['10']), 10)
        self.assertEqual(func(limit=['10', '1']), 1)
        self.assertRaises(BadRequest, func, limit=['string'])
        self.assertRaises(BadRequest, func, limit=['0'])
        self.assertRaises(BadRequest, func, limit=['-1'])

    def test_unescape_decorator(self):
        @unescape_argument(
            value=['?', '#'],
        )
        def func(value=None):
            return value

        self.assertEqual(func(value='test'), 'test')
        self.assertEqual(func(value='test$3Fvalue'), 'test?value')
        self.assertEqual(
            func(value=('test$3Fvalue1', 'test$3Fvalue2')),
            ('test?value1', 'test?value2'),
        )
        self.assertEqual(func(value='test$40value'), 'test$40value')
        self.assertEqual(func(value='test$23value'), 'test#value')
