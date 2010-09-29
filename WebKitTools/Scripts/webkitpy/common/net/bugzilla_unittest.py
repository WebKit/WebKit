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

import datetime

from webkitpy.common.config.committers import CommitterList, Reviewer, Committer
from webkitpy.common.net.bugzilla import Bugzilla, BugzillaQueries, parse_bug_id, CommitterValidator, Bug
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockBrowser
from webkitpy.thirdparty.mock import Mock
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup


class BugTest(unittest.TestCase):
    def test_is_unassigned(self):
        for email in Bug.unassigned_emails:
            bug = Bug({"assigned_to_email" : email}, bugzilla=None)
            self.assertTrue(bug.is_unassigned())
        bug = Bug({"assigned_to_email" : "test@test.com"}, bugzilla=None)
        self.assertFalse(bug.is_unassigned())


class CommitterValidatorTest(unittest.TestCase):
    def test_flag_permission_rejection_message(self):
        validator = CommitterValidator(bugzilla=None)
        self.assertEqual(validator._committers_py_path(), "WebKitTools/Scripts/webkitpy/common/config/committers.py")
        expected_messsage="""foo@foo.com does not have review permissions according to http://trac.webkit.org/browser/trunk/WebKitTools/Scripts/webkitpy/common/config/committers.py.

- If you do not have review rights please read http://webkit.org/coding/contributing.html for instructions on how to use bugzilla flags.

- If you have review rights please correct the error in WebKitTools/Scripts/webkitpy/common/config/committers.py by adding yourself to the file (no review needed).  The commit-queue restarts itself every 2 hours.  After restart the commit-queue will correctly respect your review rights."""
        self.assertEqual(validator._flag_permission_rejection_message("foo@foo.com", "review"), expected_messsage)


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
          <flag name="in-rietveld"
                id="17933"
                status="+"
                setter="three@test.com"
           />
        </attachment>
'''
    _expected_example_attachment_parsing = {
        'attach_date': datetime.datetime(2009, 07, 29, 10, 23),
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
        'in-rietveld': '+',
        'rietveld_uploader_email': 'three@test.com',
        'attacher_email' : 'christian.plesner.hansen@gmail.com',
    }

    def test_url_creation(self):
        # FIXME: These would be all better as doctests
        bugs = Bugzilla()
        self.assertEquals(None, bugs.bug_url_for_bug_id(None))
        self.assertEquals(None, bugs.short_bug_url_for_bug_id(None))
        self.assertEquals(None, bugs.attachment_url_for_id(None))

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

    _example_bug = """
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<!DOCTYPE bugzilla SYSTEM "https://bugs.webkit.org/bugzilla.dtd">
<bugzilla version="3.2.3"
          urlbase="https://bugs.webkit.org/"
          maintainer="admin@webkit.org"
          exporter="eric@webkit.org"
>
    <bug>
          <bug_id>32585</bug_id>
          <creation_ts>2009-12-15 15:17 PST</creation_ts>
          <short_desc>bug to test webkit-patch and commit-queue failures</short_desc>
          <delta_ts>2009-12-27 21:04:50 PST</delta_ts>
          <reporter_accessible>1</reporter_accessible>
          <cclist_accessible>1</cclist_accessible>
          <classification_id>1</classification_id>
          <classification>Unclassified</classification>
          <product>WebKit</product>
          <component>Tools / Tests</component>
          <version>528+ (Nightly build)</version>
          <rep_platform>PC</rep_platform>
          <op_sys>Mac OS X 10.5</op_sys>
          <bug_status>NEW</bug_status>
          <priority>P2</priority>
          <bug_severity>Normal</bug_severity>
          <target_milestone>---</target_milestone>
          <everconfirmed>1</everconfirmed>
          <reporter name="Eric Seidel">eric@webkit.org</reporter>
          <assigned_to name="Nobody">webkit-unassigned@lists.webkit.org</assigned_to>
          <cc>foo@bar.com</cc>
    <cc>example@example.com</cc>
          <long_desc isprivate="0">
            <who name="Eric Seidel">eric@webkit.org</who>
            <bug_when>2009-12-15 15:17:28 PST</bug_when>
            <thetext>bug to test webkit-patch and commit-queue failures

Ignore this bug.  Just for testing failure modes of webkit-patch and the commit-queue.</thetext>
          </long_desc>
          <attachment 
              isobsolete="0"
              ispatch="1"
              isprivate="0"
          > 
            <attachid>45548</attachid> 
            <date>2009-12-27 23:51 PST</date> 
            <desc>Patch</desc> 
            <filename>bug-32585-20091228005112.patch</filename> 
            <type>text/plain</type> 
            <size>10882</size> 
            <attacher>mjs@apple.com</attacher> 
            
              <token>1261988248-dc51409e9c421a4358f365fa8bec8357</token> 
              <data encoding="base64">SW5kZXg6IFdlYktpdC9tYWMvQ2hhbmdlTG9nCj09PT09PT09PT09PT09PT09PT09PT09PT09PT09
removed-because-it-was-really-long
ZEZpbmlzaExvYWRXaXRoUmVhc29uOnJlYXNvbl07Cit9CisKIEBlbmQKIAogI2VuZGlmCg==
</data>        
 
            <flag name="review"
                id="27602"
                status="?"
                setter="mjs@apple.com"
            />
        </attachment>
    </bug>
</bugzilla>
"""
    _expected_example_bug_parsing = {
        "id" : 32585,
        "title" : u"bug to test webkit-patch and commit-queue failures",
        "cc_emails" : ["foo@bar.com", "example@example.com"],
        "reporter_email" : "eric@webkit.org",
        "assigned_to_email" : "webkit-unassigned@lists.webkit.org",
        "bug_status": "NEW",
        "attachments" : [{
            "attach_date": datetime.datetime(2009, 12, 27, 23, 51),
            'name': u'Patch',
            'url' : "https://bugs.webkit.org/attachment.cgi?id=45548",
            'is_obsolete': False,
            'review': '?',
            'is_patch': True,
            'attacher_email': 'mjs@apple.com',
            'bug_id': 32585,
            'type': 'text/plain',
            'id': 45548
        }],
    }

    # FIXME: This should move to a central location and be shared by more unit tests.
    def _assert_dictionaries_equal(self, actual, expected):
        # Make sure we aren't parsing more or less than we expect
        self.assertEquals(sorted(actual.keys()), sorted(expected.keys()))

        for key, expected_value in expected.items():
            self.assertEquals(actual[key], expected_value, ("Failure for key: %s: Actual='%s' Expected='%s'" % (key, actual[key], expected_value)))

    def test_bug_parsing(self):
        bug = Bugzilla()._parse_bug_page(self._example_bug)
        self._assert_dictionaries_equal(bug, self._expected_example_bug_parsing)

    # This could be combined into test_bug_parsing later if desired.
    def test_attachment_parsing(self):
        bugzilla = Bugzilla()
        soup = BeautifulSoup(self._example_attachment)
        attachment_element = soup.find("attachment")
        attachment = bugzilla._parse_attachment_element(attachment_element, self._expected_example_attachment_parsing['bug_id'])
        self.assertTrue(attachment)
        self._assert_dictionaries_equal(attachment, self._expected_example_attachment_parsing)

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

    def test_add_cc_to_bug(self):
        bugzilla = Bugzilla()
        bugzilla.browser = MockBrowser()
        bugzilla.authenticate = lambda: None
        expected_stderr = "Adding ['adam@example.com'] to the CC list for bug 42\n"
        OutputCapture().assert_outputs(self, bugzilla.add_cc_to_bug, [42, ["adam@example.com"]], expected_stderr=expected_stderr)

    def _mock_control_item(self, name):
        mock_item = Mock()
        mock_item.name = name
        return mock_item

    def _mock_find_control(self, item_names=[], selected_index=0):
        mock_control = Mock()
        mock_control.items = [self._mock_control_item(name) for name in item_names]
        mock_control.value = [item_names[selected_index]] if item_names else None
        return lambda name, type: mock_control

    def _assert_reopen(self, item_names=None, selected_index=None, extra_stderr=None):
        bugzilla = Bugzilla()
        bugzilla.browser = MockBrowser()
        bugzilla.authenticate = lambda: None

        mock_find_control = self._mock_find_control(item_names, selected_index)
        bugzilla.browser.find_control = mock_find_control
        expected_stderr = "Re-opening bug 42\n['comment']\n"
        if extra_stderr:
            expected_stderr += extra_stderr
        OutputCapture().assert_outputs(self, bugzilla.reopen_bug, [42, ["comment"]], expected_stderr=expected_stderr)

    def test_reopen_bug(self):
        self._assert_reopen(item_names=["REOPENED", "RESOLVED", "CLOSED"], selected_index=1)
        self._assert_reopen(item_names=["UNCONFIRMED", "RESOLVED", "CLOSED"], selected_index=1)
        extra_stderr = "Did not reopen bug 42, it appears to already be open with status ['NEW'].\n"
        self._assert_reopen(item_names=["NEW", "RESOLVED"], selected_index=0, extra_stderr=extra_stderr)


class BugzillaQueriesTest(unittest.TestCase):
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
    _sample_quip_page = u"""
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
                      "http://www.w3.org/TR/html4/loose.dtd">
<html>
  <head>
    <title>Bugzilla Quip System</title>
  </head>
  <body>
    <h2>

      Existing quips:
    </h2>
    <ul>
        <li>Everything should be made as simple as possible, but not simpler. - Albert Einstein</li>
        <li>Good artists copy. Great artists steal. - Pablo Picasso</li>
        <li>\u00e7gua mole em pedra dura, tanto bate at\u008e que fura.</li>

    </ul>
  </body>
</html>
"""

    def test_request_page_parsing(self):
        queries = BugzillaQueries(None)
        self.assertEquals([40511, 40722, 40723], queries._parse_attachment_ids_request_query(self._sample_request_page))

    def test_quip_page_parsing(self):
        queries = BugzillaQueries(None)
        expected_quips = ["Everything should be made as simple as possible, but not simpler. - Albert Einstein", "Good artists copy. Great artists steal. - Pablo Picasso", u"\u00e7gua mole em pedra dura, tanto bate at\u008e que fura."]
        self.assertEquals(expected_quips, queries._parse_quips(self._sample_quip_page))

    def test_load_query(self):
        queries = BugzillaQueries(Mock())
        queries._load_query("request.cgi?action=queue&type=review&group=type")
