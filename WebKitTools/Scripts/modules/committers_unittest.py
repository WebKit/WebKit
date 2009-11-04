# Copyright (C) 2009 Google Inc. All rights reserved.
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
from committers import CommitterList, Committer, Reviewer

class CommittersTest(unittest.TestCase):

    def test_committer_lookup(self):
        committer = Committer('Test One', 'one@test.com')
        reviewer = Reviewer('Test Two', ['two@test.com', 'two@rad.com', 'so_two@gmail.com'])
        committer_list = CommitterList(committers=[committer], reviewers=[reviewer])

        # Test valid committer and reviewer lookup
        self.assertEqual(committer_list.committer_by_email('one@test.com'), committer)
        self.assertEqual(committer_list.reviewer_by_email('two@test.com'), reviewer)
        self.assertEqual(committer_list.committer_by_email('two@test.com'), reviewer)
        self.assertEqual(committer_list.committer_by_email('two@rad.com'), reviewer)
        self.assertEqual(committer_list.reviewer_by_email('so_two@gmail.com'), reviewer)

        # Test that a known committer is not returned during reviewer lookup
        self.assertEqual(committer_list.reviewer_by_email('one@test.com'), None)

        # Test that unknown email address fail both committer and reviewer lookup
        self.assertEqual(committer_list.committer_by_email('bar@bar.com'), None)
        self.assertEqual(committer_list.reviewer_by_email('bar@bar.com'), None)

        # Test that emails returns a list.
        self.assertEqual(committer.emails, ['one@test.com'])

        # Test that committers returns committers and reviewers and reviewers() just reviewers.
        self.assertEqual(committer_list.committers(), [committer, reviewer])
        self.assertEqual(committer_list.reviewers(), [reviewer])


if __name__ == '__main__':
    unittest.main()
