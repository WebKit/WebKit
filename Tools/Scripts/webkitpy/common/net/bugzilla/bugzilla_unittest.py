# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import datetime
import logging
import unittest

from webkitcorepy import StringIO, OutputCapture

from webkitpy.common.config import urls
from webkitpy.common.config.committers import Reviewer, Committer, Contributor, CommitterList
from webkitpy.common.net.web_mock import MockBrowser
from webkitpy.common.net.bugzilla.bugzilla import Bugzilla, BugzillaQueries, CommitQueueFlag
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup
from webkitpy.thirdparty.mock import Mock


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
        'attach_date': datetime.datetime(2009, 7, 29, 10, 23),
        'bug_id': 100,
        'is_obsolete': True,
        'is_patch': True,
        'id': 33721,
        'url': "https://bugs.webkit.org/attachment.cgi?id=33721",
        'name': "Fixed whitespace issue",
        'type': "text/plain",
        'review': '+',
        'reviewer_email': 'one@test.com',
        'commit-queue': '+',
        'committer_email': 'two@test.com',
        'attacher_email': 'christian.plesner.hansen@gmail.com',
    }

    def test_url_creation(self):
        # FIXME: These would be all better as doctests
        bugs = Bugzilla()
        self.assertIsNone(bugs.bug_url_for_bug_id(None))
        self.assertIsNone(bugs.attachment_url_for_id(None))

    def test_parse_bug_id(self):
        # Test that we can parse the urls we produce.
        bugs = Bugzilla()
        self.assertEqual(12345, urls.parse_bug_id(bugs.bug_url_for_bug_id(12345)))
        self.assertEqual(12345, urls.parse_bug_id(bugs.bug_url_for_bug_id(12345, xml=True)))

    _bug_xml = """
    <bug>
          <bug_id>32585</bug_id>
          <creation_ts>2009-12-15 15:17 PST</creation_ts>
          <short_desc>bug to test webkit-patch&apos;s and commit-queue&apos;s failures</short_desc>
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
"""

    _single_bug_xml = """
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<!DOCTYPE bugzilla SYSTEM "https://bugs.webkit.org/bugzilla.dtd">
<bugzilla version="3.2.3"
          urlbase="https://bugs.webkit.org/"
          maintainer="admin@webkit.org"
          exporter="eric@webkit.org"
>
%s
</bugzilla>
""" % _bug_xml

    _expected_example_bug_parsing = {
        "id" : 32585,
        "title" : u"bug to test webkit-patch's and commit-queue's failures",
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
        "comments" : [{
                'comment_date': datetime.datetime(2009, 12, 15, 15, 17, 28),
                'comment_email': 'eric@webkit.org',
                'text': """bug to test webkit-patch and commit-queue failures

Ignore this bug.  Just for testing failure modes of webkit-patch and the commit-queue.""",
        }]
    }

    # FIXME: This should move to a central location and be shared by more unit tests.
    def _assert_dictionaries_equal(self, actual, expected):
        # Make sure we aren't parsing more or less than we expect
        self.assertEqual(sorted(actual.keys()), sorted(expected.keys()))

        for key, expected_value in expected.items():
            self.assertEqual(actual[key], expected_value, ("Failure for key: %s: Actual='%s' Expected='%s'" % (key, actual[key], expected_value)))

    def test_parse_bug_dictionary_from_xml(self):
        bug = Bugzilla()._parse_bug_dictionary_from_xml(self._single_bug_xml)
        self._assert_dictionaries_equal(bug, self._expected_example_bug_parsing)

    _sample_multi_bug_xml = """
<bugzilla version="3.2.3" urlbase="https://bugs.webkit.org/" maintainer="admin@webkit.org" exporter="eric@webkit.org">
    %s
    %s
</bugzilla>
""" % (_bug_xml, _bug_xml)

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
<div id="bug_title">
  Attachment 41073 Details for
  <a class="bz_bug_link bz_status_ASSIGNED " title="Sample bug" href="show_bug.cgi?id=27314">Bug 27314</a>: Sample bug</div>
"""

    def test_attachment_detail_bug_parsing(self):
        bugzilla = Bugzilla()
        self.assertEqual(27314, bugzilla._parse_bug_id_from_attachment_page(self._sample_attachment_detail_page))

    def test_add_cc_to_bug(self):
        bugzilla = Bugzilla()
        bugzilla.browser = MockBrowser()
        bugzilla.authenticate = lambda: None
        with OutputCapture(level=logging.INFO) as captured:
            bugzilla.add_cc_to_bug(42, ['adam@example.com'])
        self.assertEqual(captured.root.log.getvalue(), "Adding ['adam@example.com'] to the CC list for bug 42\n")

    def _mock_control_item(self, name):
        mock_item = Mock()
        mock_item.name = name
        return mock_item

    def _mock_find_control(self, item_names=[], selected_index=0):
        mock_control = Mock()
        mock_control.items = [self._mock_control_item(name) for name in item_names]
        mock_control.value = [item_names[selected_index]] if item_names else None
        return lambda name, type: mock_control

    def _assert_reopen(self, item_names=None, selected_index=None, extra_logs=None):
        bugzilla = Bugzilla()
        bugzilla.browser = MockBrowser()
        bugzilla.authenticate = lambda: None

        mock_find_control = self._mock_find_control(item_names, selected_index)
        bugzilla.browser.find_control = mock_find_control
        with OutputCapture(level=logging.INFO) as captured:
            bugzilla.reopen_bug(42, ['comment'])
        self.assertEqual(captured.root.log.getvalue(), "Re-opening bug 42\n['comment']\n" + (extra_logs or ''))

    def test_reopen_bug(self):
        self._assert_reopen(item_names=["REOPENED", "RESOLVED", "CLOSED"], selected_index=1)
        self._assert_reopen(item_names=["UNCONFIRMED", "RESOLVED", "CLOSED"], selected_index=1)
        extra_logs = "Did not reopen bug 42, it appears to already be open with status ['NEW'].\n"
        self._assert_reopen(item_names=["NEW", "RESOLVED"], selected_index=0, extra_logs=extra_logs)

    def test_file_object_for_upload(self):
        bugzilla = Bugzilla()
        file_object = StringIO()
        unicode_tor = u"WebKit \u2661 Tor Arne Vestb\u00F8!"
        utf8_tor = unicode_tor.encode("utf-8")
        self.assertEqual(bugzilla._file_object_for_upload(file_object), file_object)
        self.assertEqual(bugzilla._file_object_for_upload(utf8_tor).read(), utf8_tor)
        self.assertEqual(bugzilla._file_object_for_upload(unicode_tor).read(), utf8_tor)

    def test_filename_for_upload(self):
        bugzilla = Bugzilla()
        mock_file = Mock()
        mock_file.name = "foo"
        self.assertEqual(bugzilla._filename_for_upload(mock_file, 1234), 'foo')
        mock_timestamp = lambda: "now"
        filename = bugzilla._filename_for_upload(StringIO(), 1234, extension="patch", timestamp=mock_timestamp)
        self.assertEqual(filename, "bug-1234-now.patch")

    def test_commit_queue_flag(self):
        bugzilla = Bugzilla()

        bugzilla.committers = CommitterList(reviewers=[Reviewer("WebKit Reviewer", "reviewer@webkit.org")],
            committers=[Committer("WebKit Committer", "committer@webkit.org")],
            contributors=[Contributor("WebKit Contributor", "contributor@webkit.org")])

        def assert_commit_queue_flag(commit_flag, expected, username=None):
            bugzilla.username = username
            with OutputCapture():
                self.assertEqual(bugzilla._commit_queue_flag(commit_flag), expected)

        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_nothing, expected='X', username='unknown@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_commit_queue, expected='?', username='unknown@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_landing, expected='?', username='unknown@webkit.org')

        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_nothing, expected='X', username='contributor@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_commit_queue, expected='?', username='contributor@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_landing, expected='?', username='contributor@webkit.org')

        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_nothing, expected='X', username='committer@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_commit_queue, expected='?', username='committer@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_landing, expected='+', username='committer@webkit.org')

        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_nothing, expected='X', username='reviewer@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_commit_queue, expected='?', username='reviewer@webkit.org')
        assert_commit_queue_flag(commit_flag=CommitQueueFlag.mark_for_landing, expected='+', username='reviewer@webkit.org')

    def test__check_create_bug_response(self):
        bugzilla = Bugzilla()

        title_html_bugzilla_323 = "<title>Bug 101640 Submitted</title>"
        self.assertEqual(bugzilla._check_create_bug_response(title_html_bugzilla_323), '101640')

        title_html_bugzilla_425 = "<title>Bug 101640 Submitted &ndash; Testing webkit-patch again</title>"
        self.assertEqual(bugzilla._check_create_bug_response(title_html_bugzilla_425), '101640')

    def test__parse_attachment_id_from_add_patch_to_bug_response(self):
        bugzilla = Bugzilla()

        title_html = b'<title>Attachment 317591 added to Bug 175247</title>'
        self.assertEqual(bugzilla._parse_attachment_id_from_add_patch_to_bug_response(title_html), '317591')

        title_html = b'<title>Attachment 317591; malformed</title>'
        self.assertEqual(bugzilla._parse_attachment_id_from_add_patch_to_bug_response(title_html), None)

        title_html = b'<title>Attachment A added to Bug 175247</title>'
        self.assertEqual(bugzilla._parse_attachment_id_from_add_patch_to_bug_response(title_html), None)


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

    def test_quip_page_parsing(self):
        queries = BugzillaQueries(None)
        expected_quips = ["Everything should be made as simple as possible, but not simpler. - Albert Einstein", "Good artists copy. Great artists steal. - Pablo Picasso", u"\u00e7gua mole em pedra dura, tanto bate at\u008e que fura."]
        self.assertEqual(expected_quips, queries._parse_quips(self._sample_quip_page))

    def test_load_query(self):
        queries = BugzillaQueries(Mock())
        queries._load_query("request.cgi?action=queue&type=review&group=type")
