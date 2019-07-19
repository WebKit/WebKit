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

from resultsdbpy.controller.commit import Commit


class CommitUnittest(unittest.TestCase):

    def test_compare(self):
        commit1 = Commit(
            repository_id='safari', branch='master',
            id='e64810a40c3fecb728871e12ca31482ca715b383',
            timestamp=1537550685,
        )
        commit2 = Commit(
            repository_id='safari', branch='master',
            id='7be4084258a452e8fe22f36287c5b321e9c8249b',
            timestamp=1537550685, order=1,
        )
        commit3 = Commit(
            repository_id='safari', branch='master',
            id='bb6bda5f44dd24d0b54539b8ff6e8c17f519249a',
            timestamp=1537810281,
        )
        commit4 = Commit(
            repository_id='webkit', branch='master',
            id=236522,
            timestamp=1537826614,
        )

        self.assertTrue(commit2 > commit1)
        self.assertTrue(commit2 >= commit1)
        self.assertTrue(commit1 >= commit1)
        self.assertTrue(commit1 < commit2)
        self.assertTrue(commit1 <= commit2)
        self.assertTrue(commit1 <= commit1)
        self.assertTrue(commit1 == commit1)

        self.assertTrue(commit2 < commit3)
        self.assertTrue(commit3 > commit2)
        self.assertEqual(commit1.timestamp, commit2.timestamp)

        self.assertTrue(commit4 > commit3)
        self.assertNotEqual(commit3.repository_id, commit4.repository_id)

    def test_encoding(self):
        commits_to_test = [
            Commit(
                repository_id='safari', branch='master',
                id='e64810a40c3fecb728871e12ca31482ca715b383',
                timestamp=1537550685,
            ), Commit(
                repository_id='safari', branch='master',
                id='7be4084258a452e8fe22f36287c5b321e9c8249b',
                timestamp=1537550685, order=1,
                committer='email1@webkit.org', message='Changelog',
            ), Commit(
                repository_id='webkit', branch='master',
                id=236522,
                timestamp=1537826614,
            ),
        ]
        for commit in commits_to_test:
            converted_commit = Commit.from_json(commit.to_json())

            self.assertEqual(commit, converted_commit)
            self.assertEqual(commit.repository_id, converted_commit.repository_id)
            self.assertEqual(commit.id, converted_commit.id)
            self.assertEqual(commit.timestamp, converted_commit.timestamp)
            self.assertEqual(commit.order, converted_commit.order)
            self.assertEqual(commit.committer, converted_commit.committer)
            self.assertEqual(commit.message, converted_commit.message)

    def test_udid(self):
        self.assertEqual(Commit(
            repository_id='safari', branch='master',
            id='7be4084258a452e8fe22f36287c5b321e9c8249b',
            timestamp=1537550685, order=1,
        ).uuid, 153755068501)

    def test_invalid(self):
        with self.assertRaises(ValueError):
            Commit(
                repository_id='safari', branch='master',
                id='7be4084258a452e8fe22f36287c5b321e9c8249b',
                timestamp=None,
            )
