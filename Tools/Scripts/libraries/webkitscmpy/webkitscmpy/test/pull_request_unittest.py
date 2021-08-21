# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import unittest

from webkitscmpy import Commit, PullRequest


class TestPullRequest(unittest.TestCase):
    def test_representation(self):
        self.assertEqual('PR 123 | [scoping] Bug to fix', str(PullRequest(123, title='[scoping] Bug to fix')))
        self.assertEqual('PR 1234', str(PullRequest(1234)))

    def test_create_body_single(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''',
        )

    def test_parse_body_single(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix\n\nReviewed by Tim Contributor.')

    def test_create_body_multiple(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix (Part 2)\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='53ea230fcedbce327eb1c45a6ab65a88de864505',
                message='[scoping] Bug to fix (Part 1)\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix (Part 2)

Reviewed by Tim Contributor.
```
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
```
[scoping] Bug to fix (Part 1)

Reviewed by Tim Contributor.
```''',
        )

    def test_parse_body_multiple(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix (Part 2)

Reviewed by Tim Contributor.
```
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
```
[scoping] Bug to fix (Part 1)

Reviewed by Tim Contributor.
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 2)

        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix (Part 2)\n\nReviewed by Tim Contributor.')

        self.assertEqual(commits[1].hash, '53ea230fcedbce327eb1c45a6ab65a88de864505')
        self.assertEqual(commits[1].message, '[scoping] Bug to fix (Part 1)\n\nReviewed by Tim Contributor.')

    def test_create_body_empty(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(hash='11aa76f9fc380e9fe06157154f32b304e8dc4749')]),
            '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
???
```''',
        )

    def test_parse_body_empty(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
???
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, None)

    def test_create_body_comment(self):
        self.assertEqual(
            PullRequest.create_body('Comment body', [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''',
        )

    def test_parse_body_single(self):
        body, commits = PullRequest.parse_body('''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''')
        self.assertEqual(body, 'Comment body')
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix\n\nReviewed by Tim Contributor.')
