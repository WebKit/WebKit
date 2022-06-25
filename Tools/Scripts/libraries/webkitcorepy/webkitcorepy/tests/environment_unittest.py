# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import os

from webkitcorepy import testing, Environment


class TestEnvironment(testing.PathTestCase):
    basepath = 'mock/secrets'

    def test_basic(self):
        try:
            with open(os.path.join(self.path, 'KEY'), 'w') as file:
                file.write('value\n')
            self.assertEqual(Environment.instance(self.path).load()['KEY'], 'value')
        finally:
            Environment._instance = None

    def test_scoped(self):
        try:
            with open(os.path.join(self.path, 'KEY'), 'w') as file:
                file.write('value ')
            with open(os.path.join(self.path, 'scope___KEY_A'), 'w') as file:
                file.write('value_a')
            with open(os.path.join(self.path, 'other___KEY_B'), 'w') as file:
                file.write('value_b')
            self.assertEqual(Environment.instance(self.path).load('scope')['KEY'], 'value ')
            self.assertEqual(Environment.instance(self.path).load('scope')['KEY_A'], 'value_a')
            self.assertIsNone(Environment.instance(self.path).load('scope').get('KEY_B'))
        finally:
            Environment._instance = None

    def test_list(self):
        try:
            with open(os.path.join(self.path, 'KEY_A'), 'w') as file:
                file.write('value_a')
            with open(os.path.join(self.path, 'KEY_B'), 'w') as file:
                file.write('value_b')

            Environment.instance(self.path).load()
            self.assertEqual(
                sorted(list(Environment.instance(self.path).keys())),
                sorted(['KEY_A', 'KEY_B'] + list(os.environ.keys())),
            )
            self.assertEqual(
                sorted(list(Environment.instance(self.path).values())),
                sorted(['value_a', 'value_b'] + list(os.environ.values())),
            )
            self.assertEqual(
                sorted(list(Environment.instance(self.path).items())),
                sorted([('KEY_A', 'value_a'), ('KEY_B', 'value_b')] + list(os.environ.items())),
            )
        finally:
            Environment._instance = None

    def test_secure(self):
        try:
            with open(os.path.join(self.path, 'KEY'), 'w') as file:
                file.write('value ')
            with open(os.path.join(self.path, 'scope___KEY_A'), 'w') as file:
                file.write('value_a')
            with open(os.path.join(self.path, 'other___KEY_B'), 'w') as file:
                file.write('value_b')
            with open(os.path.join(self.path, 'KEY_C'), 'w') as file:
                file.write('value_c')

            Environment.instance(self.path).load('scope')
            Environment.instance(self.path).secure(os.path.join(self.path, 'KEY_C'))

            self.assertTrue(os.path.isfile(os.path.join(self.path, 'KEY')))
            self.assertTrue(os.path.isfile(os.path.join(self.path, 'scope___KEY_A')))
            self.assertFalse(os.path.isfile(os.path.join(self.path, 'other___KEY_B')))
            self.assertFalse(os.path.isfile(os.path.join(self.path, 'KEY_C')))
        finally:
            Environment._instance = None
