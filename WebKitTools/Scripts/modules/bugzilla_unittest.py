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

from modules.committers import CommitterList, Reviewer, Committer
from modules.bugzilla import Bugzilla

from modules.BeautifulSoup import BeautifulSoup

class BugzillaTest(unittest.TestCase):

    _example_attachment = '''
        <attachment
          isobsolete="1"
          ispatch="1"
          isprivate="0"
        >
        <attachid>33721</attachid>
        <date>2009-07-29 10:23 PDT</date>
        <desc>Fixed whitespace issue</desc>
        <filename>patch</filename>
        <type>text/plain</type>
        <size>9719</size>
        <attacher>christian.plesner.hansen@gmail.com</attacher>
          <flag name="review"
                id="17931"
                status="+"
                setter="one@test.com"
           />
          <flag name="commit-queue"
                id="17932"
                status="+"
                setter="two@test.com"
           />
        </attachment>
'''
    _expected_example_attachment_parsing = {
        'bug_id' : "100",
        'is_obsolete' : True,
        'is_patch' : True,
        'id' : "33721",
        'url' : "https://bugs.webkit.org/attachment.cgi?id=33721",
        'name' : "Fixed whitespace issue",
        'type' : "text/plain",
        'reviewer' : 'Test One',
        'commit-queue' : 'Test Two'
    }

    def test_attachment_parsing(self):
        reviewer = Reviewer('Test One', 'one@test.com')
        committer = Committer('Test Two', 'two@test.com')
        committer_list = CommitterList(committers=[committer], reviewers=[reviewer])
        bugzilla = Bugzilla(committers=committer_list)

        soup = BeautifulSoup(self._example_attachment)
        attachment_element = soup.find("attachment")
        attachment = bugzilla._parse_attachment_element(attachment_element, self._expected_example_attachment_parsing['bug_id'])
        self.assertTrue(attachment)

        for key, expected_value in self._expected_example_attachment_parsing.items():
            self.assertEquals(attachment[key], expected_value, ("Failure for key: %s: Actual='%s' Expected='%s'" % (key, attachment[key], expected_value)))

if __name__ == '__main__':
    unittest.main()
