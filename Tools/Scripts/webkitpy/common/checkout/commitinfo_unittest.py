# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.config.committers import CommitterList, Committer, Reviewer


class CommitInfoTest(unittest.TestCase):

    def test_commit_info_creation(self):
        author = Committer("Author", "author@example.com")
        committer = Committer("Committer", "committer@example.com")
        reviewer = Reviewer("Reviewer", "reviewer@example.com")
        committer_list = CommitterList(committers=[author, committer], reviewers=[reviewer])

        changelog_data = {
            "bug_id": 1234,
            "author_name": "Committer",
            "author_email": "author@example.com",
            "author": author,
            "reviewer_text": "Reviewer",
            "reviewer": reviewer,
            "bug_description": "Bug description",
        }
        commit = CommitInfo(123, "committer@example.com", changelog_data, committer_list)

        self.assertEqual(commit.revision(), 123)
        self.assertEqual(commit.bug_id(), 1234)
        self.assertEqual(commit.author_name(), "Committer")
        self.assertEqual(commit.author_email(), "author@example.com")
        self.assertEqual(commit.author(), author)
        self.assertEqual(commit.reviewer_text(), "Reviewer")
        self.assertEqual(commit.reviewer(), reviewer)
        self.assertEqual(commit.committer(), committer)
        self.assertEqual(commit.committer_email(), "committer@example.com")
        self.assertEqual(commit.responsible_parties(), set([author, committer, reviewer]))
        self.assertEqual(commit.bug_description(), "Bug description")
