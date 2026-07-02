# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

from webkitbugspy.tracker import Tracker
from webkitcorepy import mocks

from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.upload import *
from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitscmpy import mocks as wmocks


class MockIssue(object):
    def __init__(self, link, redacted=False):
        self.link = link
        self.redacted = redacted


class UploadCommandsTest(CommandsTest):
    path = '/mock-checkout'

    def test_post(self):
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = "MOCK description"
        options.fast_cq = False
        options.non_interactive = False
        options.request_commit = False
        options.review = True
        options.suggest_reviewers = False
        options.issues = []
        expected_logs = """MOCK: user.open_url: file://...
Was that diff correct?
Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=MOCK description, mark_for_review=True, mark_for_commit_queue=False, mark_for_landing=False
MOCK: user.open_url: http://example.com/50000
"""
        self.assert_execute_outputs(Post(), [50000], options=options, expected_logs=expected_logs)

    def test_land_safely(self):
        options = MockOptions()
        options.fast_cq = False
        options.issues = []
        expected_logs = """Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=Patch for landing, mark_for_review=False, mark_for_commit_queue=False, mark_for_landing=True
"""
        self.assert_execute_outputs(LandSafely(), [50000], options=options, expected_logs=expected_logs)

    def test_land_safely_with_fast_cq(self):
        options = MockOptions()
        options.fast_cq = True
        options.issues = []
        expected_logs = """Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=[fast-cq] Patch for landing, mark_for_review=False, mark_for_commit_queue=False, mark_for_landing=True
"""
        self.assert_execute_outputs(LandSafely(), [50000], options=options, expected_logs=expected_logs)

    def test_prepare_diff_with_arg(self):
        options = MockOptions()
        options.sort_xcode_project = False
        self.assert_execute_outputs(Prepare(), [50000], options=options)

    def test_prepare(self):
        options = MockOptions()
        options.sort_xcode_project = False
        options.non_interactive = True
        expected_logs = "MOCK create_bug\nbug_title: Mock user response\nbug_description: Mock user response\ncomponent: MOCK component\ncc: MOCK cc\n"
        self.assert_execute_outputs(Prepare(), [], expected_logs=expected_logs, options=options)

    def test_prepare_with_cc(self):
        options = MockOptions()
        options.cc = "a@example.com,b@example.com"
        options.sort_xcode_project = False
        options.non_interactive = True
        expected_logs = "MOCK create_bug\nbug_title: Mock user response\nbug_description: Mock user response\ncomponent: MOCK component\ncc: a@example.com,b@example.com\n"
        self.assert_execute_outputs(Prepare(), [], expected_logs=expected_logs, options=options)

    def test_prepare_with_radar(self):
        options = MockOptions()
        options.cc_radar = True
        options.sort_xcode_project = False
        options.non_interactive = True
        expected_logs = "MOCK create_bug\nbug_title: Mock user response\nbug_description: Mock user response\ncomponent: MOCK component\ncc: webkit-bug-importer@group.apple.com,MOCK cc\n"
        self.assert_execute_outputs(Prepare(), [], expected_logs=expected_logs, options=options)

    def test_prepare_with_cc_and_radar(self):
        options = MockOptions()
        options.cc = "a@example.com,b@example.com"
        options.cc_radar = True
        options.sort_xcode_project = False
        options.non_interactive = True
        expected_logs = "MOCK create_bug\nbug_title: Mock user response\nbug_description: Mock user response\ncomponent: MOCK component\ncc: webkit-bug-importer@group.apple.com,a@example.com,b@example.com\n"
        self.assert_execute_outputs(Prepare(), [], expected_logs=expected_logs, options=options)

    def test_upload(self):
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = "MOCK description"
        options.fast_cq = False
        options.non_interactive = False
        options.request_commit = False
        options.review = True
        options.submit_to_ews = False
        options.sort_xcode_project = False
        options.suggest_reviewers = False
        options.issues = []
        expected_logs = """MOCK: user.open_url: file://...
Was that diff correct?
Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=MOCK description, mark_for_review=True, mark_for_commit_queue=False, mark_for_landing=False
MOCK: user.open_url: http://example.com/50000
"""
        with wmocks.local.Git(self.path):
            self.assert_execute_outputs(Upload(), [50000], options=options, expected_logs=expected_logs)

    def test_upload_fast_cq(self):
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = "MOCK description"
        options.fast_cq = True
        options.non_interactive = False
        options.request_commit = False
        options.review = True
        options.submit_to_ews = False
        options.sort_xcode_project = False
        options.suggest_reviewers = False
        options.issues = []
        expected_logs = """MOCK: user.open_url: file://...
Was that diff correct?
Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=[fast-cq] MOCK description, mark_for_review=True, mark_for_commit_queue=False, mark_for_landing=False
MOCK: user.open_url: http://example.com/50000
"""
        with wmocks.local.Git(self.path):
            self.assert_execute_outputs(Upload(), [50000], options=options, expected_logs=expected_logs)

    def test_upload_with_no_review_and_ews(self):
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = 'MOCK description'
        options.fast_cq = False
        options.non_interactive = False
        options.request_commit = False
        options.review = False
        options.ews = True
        options.sort_xcode_project = False
        options.suggest_reviewers = False
        options.issues = []
        expected_logs = """MOCK: user.open_url: file://...
Was that diff correct?
Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=MOCK description, mark_for_review=False, mark_for_commit_queue=False, mark_for_landing=False
MOCK: user.open_url: http://example.com/50000
MOCK: submit_to_ews: 10001
"""
        with wmocks.local.Git(self.path):
            self.assert_execute_outputs(Upload(), [50000], options=options, expected_logs=expected_logs)

    def test_mark_bug_fixed(self):
        tool = MockTool()
        tool._scm.last_svn_commit_log = lambda: "r9876 |"
        options = Mock()
        options.bug_id = 50000
        options.comment = "MOCK comment"
        expected_logs = """Bug: <http://example.com/50000> Bug with two r+'d and cq+'d patches, one of which has an invalid commit-queue setter.
Revision: 9876
MOCK: user.open_url: http://example.com/50000
Is this correct?
Adding comment to Bug 50000.
MOCK bug comment: bug_id=50000, cc=None, see_also=None
--- Begin comment ---
MOCK comment

Committed r9876 (5@main): <https://commits.webkit.org/5@main>
--- End comment ---

"""
        with mocks.Requests('commits.webkit.org', **{
            'r9876/json': mocks.Response.fromJson(dict(
                identifier='5@main',
                revision=9876,
            )),
        }), wmocks.local.Git(self.path):
            self.assert_execute_outputs(MarkBugFixed(), [], expected_logs=expected_logs, tool=tool, options=options)

    def test_edit_changelog(self):
        self.assert_execute_outputs(EditChangeLogs(), [])

    def test_upload_blocked(self):
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = "MOCK description"
        options.fast_cq = False
        options.non_interactive = False
        options.request_commit = False
        options.review = True
        options.submit_to_ews = False
        options.sort_xcode_project = False
        options.suggest_reviewers = False
        options.issues = [MockIssue(
            'https://bugs.webkit.org/show_bug.cgi?id=255559',
            redacted=Tracker.Redaction(True, 'is a security bug'),
        )]
        expected_logs = """The patch you are uploading references https://bugs.webkit.org/show_bug.cgi?id=255559
https://bugs.webkit.org/show_bug.cgi?id=255559 is a security bug and is thus redacted
Please use 'git-webkit' to upload this fix. 'webkit-patch' does not support security changes
"""
        with self.assertRaises(SystemExit):
            self.assert_execute_outputs(Upload(), [50000], options=options, expected_logs=expected_logs)

    def test_upload_exempt(self):
        self.maxDiff = None
        options = MockOptions()
        options.cc = None
        options.check_style = True
        options.check_style_filter = None
        options.comment = None
        options.description = "MOCK description"
        options.fast_cq = False
        options.non_interactive = False
        options.request_commit = False
        options.review = True
        options.submit_to_ews = False
        options.sort_xcode_project = False
        options.suggest_reviewers = False
        options.issues = [
            MockIssue('https://bugs.webkit.org/show_bug.cgi?id=255559',  redacted=Tracker.Redaction(True, 'is a security bug')),
            MockIssue('rdar://12345', redacted=Tracker.Redaction(True, 'has the public keyword', True)),
        ]
        expected_logs = """The patch you are uploading references rdar://12345
rdar://12345 has the public keyword and is exempt from redaction
Redaction exemption overrides the redaction of https://bugs.webkit.org/show_bug.cgi?id=255559
https://bugs.webkit.org/show_bug.cgi?id=255559 is a security bug and is thus redacted
MOCK: user.open_url: file://...
Was that diff correct?
Obsoleting 2 old patches on bug 50000
MOCK reassign_bug: bug_id=50000, assignee=None
MOCK add_patch_to_bug: bug_id=50000, description=MOCK description, mark_for_review=True, mark_for_commit_queue=False, mark_for_landing=False
MOCK: user.open_url: http://example.com/50000
"""
        with wmocks.local.Git(self.path):
            self.assert_execute_outputs(Upload(), [50000], options=options, expected_logs=expected_logs)
