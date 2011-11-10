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
from webkitpy.common.config.committers import Account, CommitterList, Contributor, Committer, Reviewer

class CommittersTest(unittest.TestCase):
    def test_committer_lookup(self):
        account = Account('Test Zero', ['zero@test.com', 'zero@gmail.com'], 'zero')
        committer = Committer('Test One', 'one@test.com', 'one')
        reviewer = Reviewer('Test Two', ['two@test.com', 'Two@rad.com', 'so_two@gmail.com'])
        contributor = Contributor('Test Three', ['Three@test.com'], 'three')
        contributor_with_two_nicknames = Contributor('Other Four', ['otherfour@webkit.org'], ['four', 'otherfour'])
        committer_list = CommitterList(watchers=[account], committers=[committer], reviewers=[reviewer], contributors=[contributor, contributor_with_two_nicknames])

        # Test valid committer, reviewer and contributor lookup
        self.assertEqual(committer_list.account_by_email('zero@test.com'), account)
        self.assertEqual(committer_list.committer_by_email('one@test.com'), committer)
        self.assertEqual(committer_list.reviewer_by_email('two@test.com'), reviewer)
        self.assertEqual(committer_list.committer_by_email('two@test.com'), reviewer)
        self.assertEqual(committer_list.committer_by_email('two@rad.com'), reviewer)
        self.assertEqual(committer_list.reviewer_by_email('so_two@gmail.com'), reviewer)
        self.assertEqual(committer_list.contributor_by_email('three@test.com'), contributor)

        # Test valid committer, reviewer and contributor lookup
        self.assertEqual(committer_list.committer_by_name("Test One"), committer)
        self.assertEqual(committer_list.committer_by_name("Test Two"), reviewer)
        self.assertEqual(committer_list.committer_by_name("Test Three"), None)
        self.assertEqual(committer_list.contributor_by_name("Test Three"), contributor)

        # Test that the first email is assumed to be the Bugzilla email address (for now)
        self.assertEqual(committer_list.committer_by_email('two@rad.com').bugzilla_email(), 'two@test.com')

        # Test lookup by login email address
        self.assertEqual(committer_list.account_by_login('zero@test.com'), account)
        self.assertEqual(committer_list.account_by_login('zero@gmail.com'), None)
        self.assertEqual(committer_list.account_by_login('one@test.com'), committer)
        self.assertEqual(committer_list.account_by_login('two@test.com'), reviewer)
        self.assertEqual(committer_list.account_by_login('Two@rad.com'), None)
        self.assertEqual(committer_list.account_by_login('so_two@gmail.com'), None)

        # Test that a known committer is not returned during reviewer lookup
        self.assertEqual(committer_list.reviewer_by_email('one@test.com'), None)
        self.assertEqual(committer_list.reviewer_by_email('three@test.com'), None)
        # and likewise that a known contributor is not returned for committer lookup.
        self.assertEqual(committer_list.committer_by_email('three@test.com'), None)

        # Test that unknown email address fail both committer and reviewer lookup
        self.assertEqual(committer_list.committer_by_email('bar@bar.com'), None)
        self.assertEqual(committer_list.reviewer_by_email('bar@bar.com'), None)

        # Test that emails returns a list.
        self.assertEqual(committer.emails, ['one@test.com'])

        self.assertEqual(committer.irc_nicknames, ['one'])
        self.assertEqual(committer_list.contributor_by_irc_nickname('one'), committer)
        self.assertEqual(committer_list.contributor_by_irc_nickname('three'), contributor)
        self.assertEqual(committer_list.contributor_by_irc_nickname('four'), contributor_with_two_nicknames)
        self.assertEqual(committer_list.contributor_by_irc_nickname('otherfour'), contributor_with_two_nicknames)

        # Test that the lists returned are are we expect them.
        self.assertEqual(committer_list.contributors(), [contributor, contributor_with_two_nicknames, committer, reviewer])
        self.assertEqual(committer_list.committers(), [committer, reviewer])
        self.assertEqual(committer_list.reviewers(), [reviewer])

        self.assertEqual(committer_list.contributors_by_search_string('test'), [contributor, committer, reviewer])
        self.assertEqual(committer_list.contributors_by_search_string('rad'), [reviewer])
        self.assertEqual(committer_list.contributors_by_search_string('Two'), [reviewer])
