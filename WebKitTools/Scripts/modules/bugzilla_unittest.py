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
from modules.bugzilla import Bugzilla, parse_bug_id

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
        'bug_id' : 100,
        'is_obsolete' : True,
        'is_patch' : True,
        'id' : 33721,
        'url' : "https://bugs.webkit.org/attachment.cgi?id=33721",
        'name' : "Fixed whitespace issue",
        'type' : "text/plain",
        'review' : '+',
        'reviewer_email' : 'one@test.com',
        'commit-queue' : '+',
        'committer_email' : 'two@test.com',
    }

    def test_parse_bug_id(self):
        # FIXME: These would be all better as doctests
        bugs = Bugzilla()
        self.assertEquals(12345, parse_bug_id("http://webkit.org/b/12345"))
        self.assertEquals(12345, parse_bug_id("http://bugs.webkit.org/show_bug.cgi?id=12345"))
        self.assertEquals(12345, parse_bug_id(bugs.short_bug_url_for_bug_id(12345)))
        self.assertEquals(12345, parse_bug_id(bugs.bug_url_for_bug_id(12345)))
        self.assertEquals(12345, parse_bug_id(bugs.bug_url_for_bug_id(12345, xml=True)))

        # Our bug parser is super-fragile, but at least we're testing it.
        self.assertEquals(None, parse_bug_id("http://www.webkit.org/b/12345"))
        self.assertEquals(None, parse_bug_id("http://bugs.webkit.org/show_bug.cgi?ctype=xml&id=12345"))


    def test_attachment_parsing(self):
        bugzilla = Bugzilla()

        soup = BeautifulSoup(self._example_attachment)
        attachment_element = soup.find("attachment")
        attachment = bugzilla._parse_attachment_element(attachment_element, self._expected_example_attachment_parsing['bug_id'])
        self.assertTrue(attachment)

        # Make sure we aren't parsing more or less than we expect
        self.assertEquals(attachment.keys(), self._expected_example_attachment_parsing.keys())

        for key, expected_value in self._expected_example_attachment_parsing.items():
            self.assertEquals(attachment[key], expected_value, ("Failure for key: %s: Actual='%s' Expected='%s'" % (key, attachment[key], expected_value)))

    _sample_attachment_detail_page = """
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
                      "http://www.w3.org/TR/html4/loose.dtd">
<html>
  <head>
    <title>
  Attachment 41073 Details for Bug 27314</title>
<link rel="Top" href="https://bugs.webkit.org/">
    <link rel="Up" href="show_bug.cgi?id=27314">
"""

    def test_attachment_detail_bug_parsing(self):
        bugzilla = Bugzilla()
        self.assertEquals(27314, bugzilla._parse_bug_id_from_attachment_page(self._sample_attachment_detail_page))

    _sample_request_page = """
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
                      "http://www.w3.org/TR/html4/loose.dtd">
<html>
  <head>
    <title>Request Queue</title>
  </head>
<body>

<h3>Flag: review</h3>
  <table class="requests" cellspacing="0" cellpadding="4" border="1">
    <tr>
        <th>Requester</th>
        <th>Requestee</th>
        <th>Bug</th>
        <th>Attachment</th>
        <th>Created</th>
    </tr>
    <tr>
        <td>Shinichiro Hamaji &lt;hamaji&#64;chromium.org&gt;</td>
        <td></td>
        <td><a href="show_bug.cgi?id=30015">30015: text-transform:capitalize is failing in CSS2.1 test suite</a></td>
        <td><a href="attachment.cgi?id=40511&amp;action=review">
40511: Patch v0</a></td>
        <td>2009-10-02 04:58 PST</td>
    </tr>
    <tr>
        <td>Zan Dobersek &lt;zandobersek&#64;gmail.com&gt;</td>
        <td></td>
        <td><a href="show_bug.cgi?id=26304">26304: [GTK] Add controls for playing html5 video.</a></td>
        <td><a href="attachment.cgi?id=40722&amp;action=review">
40722: Media controls, the simple approach</a></td>
        <td>2009-10-06 09:13 PST</td>
    </tr>
    <tr>
        <td>Zan Dobersek &lt;zandobersek&#64;gmail.com&gt;</td>
        <td></td>
        <td><a href="show_bug.cgi?id=26304">26304: [GTK] Add controls for playing html5 video.</a></td>
        <td><a href="attachment.cgi?id=40723&amp;action=review">
40723: Adjust the media slider thumb size</a></td>
        <td>2009-10-06 09:15 PST</td>
    </tr>
  </table>
</body>
</html>
"""

    def test_request_page_parsing(self):
        bugzilla = Bugzilla()
        self.assertEquals([40511, 40722, 40723], bugzilla._parse_attachment_ids_request_query(self._sample_request_page))

if __name__ == '__main__':
    unittest.main()
