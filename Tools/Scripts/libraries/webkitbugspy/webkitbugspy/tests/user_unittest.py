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

import json
import unittest

from webkitbugspy import User


class TestUser(unittest.TestCase):
    def test_string(self):
        self.assertEqual(
            str(User(name='Tim Contributor', username='tcontributor', emails=['tcontributor@webkit.org'])),
            'Tim Contributor <tcontributor>',
        )
        self.assertEqual(
            str(User(username='tcontributor', emails=['tcontributor@webkit.org'])),
            'tcontributor <tcontributor@webkit.org>',
        )
        self.assertEqual(
            str(User(name='Tim Contributor', emails=['tcontributor@webkit.org'])),
            'Tim Contributor <tcontributor@webkit.org>',
        )

    def test_constructor(self):
        usr = User('Tim Contributor', 'tcontributor', ['tcontributor@webkit.org'])
        self.assertEqual(usr.name, 'Tim Contributor')
        self.assertEqual(usr.username, 'tcontributor')
        self.assertEqual(usr.emails, ['tcontributor@webkit.org'])

    def test_compare(self):
        self.assertEqual(User('Tim Contributor', 'tcontributor'), User(username='tcontributor'))
        self.assertEqual(User('Tim Contributor', emails=['tcontributor@webkit.org']), User('Tim Contributor'))
        self.assertNotEqual(User('Tim Contributor', 'tcontributor'), User('Tim Contributor'))
        self.assertNotEqual(User('Tim Contributor'), None)

    def test_json(self):
        self.assertEqual(
            User.Encoder().default(User(name='Tim Contributor', username='tcontributor', emails=['tcontributor@webkit.org'])),
            dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@webkit.org']),
        )
        self.assertEqual(
            User.Encoder().default(User(username='tcontributor', emails=['tcontributor@webkit.org'])),
            dict(username='tcontributor', emails=['tcontributor@webkit.org']),
        )
        self.assertEqual(
            User.Encoder().default(User(name='Tim Contributor', emails=['tcontributor@webkit.org'])),
            dict(name='Tim Contributor', emails=['tcontributor@webkit.org']),
        )
        self.assertEqual(
            User.Encoder().default(User(name='Tim Contributor', username='tcontributor')),
            dict(name='Tim Contributor', username='tcontributor'),
        )

    def test_mapping(self):
        mapping = User.Mapping()
        user = mapping.create(name='Tim Contributor', username='tcontributor', emails=['tcontributor@webkit.org'])
        self.assertEqual(user, mapping['Tim Contributor'])
        self.assertEqual(user, mapping['tcontributor'])
        self.assertEqual(user, mapping['tcontributor@webkit.org'])

        mapping.create(name='Tim Contributor', emails=['tcontributor-alt@webkit.org'])
        self.assertEqual(user, mapping['tcontributor-alt@webkit.org'])
        self.assertEqual(user.emails, ['tcontributor@webkit.org', 'tcontributor-alt@webkit.org'])

        mapping.create(name='Ricky Reviewer', emails=['rreviewer@webkit.org'])
        with self.assertRaises(RuntimeError):
            mapping.create(name='Tim Contributor', emails=['rreviewer@webkit.org'])
