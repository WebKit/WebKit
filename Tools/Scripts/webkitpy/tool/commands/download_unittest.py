# Copyright (C) 2009, 2011 Google Inc. All rights reserved.
# Copyright (C) 2021 Apple Inc. All rights reserved.
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

from webkitcorepy import OutputCapture, mocks

from webkitpy.common.checkout.checkout_mock import MockCheckout
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.download import *
from webkitpy.tool.mocktool import MockOptions, MockTool


class AbstractRevertPrepCommandTest(unittest.TestCase):
    def test_commit_info(self):
        command = AbstractRevertPrepCommand()
        tool = MockTool()
        command.bind_to_tool(tool)

        with OutputCapture(level=logging.INFO) as captured:
            commit_info = command._commit_info(1234)
        self.assertEqual(captured.root.log.getvalue(), 'Preparing revert for bug 50000.\n')
        self.assertTrue(commit_info)

        mock_commit_info = Mock()
        mock_commit_info.bug_id = lambda: None
        tool._checkout.commit_info_for_revision = lambda revision: mock_commit_info
        with OutputCapture(level=logging.INFO) as captured:
            commit_info = command._commit_info(1234)
        self.assertEqual(captured.root.log.getvalue(), 'Unable to parse bug number from diff.\n')
        self.assertEqual(commit_info, mock_commit_info)

    def test_prepare_state(self):
        command = AbstractRevertPrepCommand()
        mock_commit_info = MockCheckout().commit_info_for_revision(123)
        command._commit_info = lambda revision: mock_commit_info

        state = command._prepare_state(None, ["124 123 125", "Reason"], None)
        self.assertEqual('r123', state["revision"])
        self.assertEqual(['r123', 'r124', 'r125'], state["revision_list"])

        state = command._prepare_state(None, ["125 r122 123", "Reason"], None)
        self.assertEqual('r122', state["revision"])
        self.assertEqual(['r122', 'r123', 'r125'], state["revision_list"])

        state = command._prepare_state(None, ["125 1234@main 123", "Reason"], None)
        self.assertEqual('r123', state["revision"])
        self.assertEqual(['r123', 'r125', '1234@main'], state["revision_list"])

        command._commit_info = lambda revision: None
        state = command._prepare_state(None, ["124 123 125", "Reason"], None)
        self.assertEqual('r123', state["revision"])
        self.assertEqual(['r123', 'r124', 'r125'], state["revision_list"])


class DownloadCommandsTest(CommandsTest):
    def _default_options(self):
        options = MockOptions()
        options.build = True
        options.build_style = "release"
        options.check_style = True
        options.check_style_filter = None
        options.clean = True
        options.close_bug = True
        options.comment_bug = True
        options.force_clean = False
        options.non_interactive = False
        options.parent_command = 'MOCK parent command'
        options.quiet = False
        options.test = True
        options.update = True
        options.architecture = 'MOCK ARCH'
        options.iterate_on_new_tests = 0
        options.group = None
        options.sort_xcode_project = False
        return options

    def mock_svn_remote(self):
        return mocks.Requests('commits.webkit.org', **{
            'r49824/json': mocks.Response.fromJson(dict(
                identifier='5@main',
                revision=49824,
            )),
        })

    def test_create_revert(self):
        expected_logs = """Preparing revert for bug 50000.
Updating working directory
MOCK create_bug
bug_title: REGRESSION(r852): Reason
bug_description: https://commits.webkit.org/r852 introduced a regression:
Reason
component: MOCK component
cc: MOCK cc
blocked: 50000
MOCK add_patch_to_bug: bug_id=60001, description=REVERT of r852, mark_for_review=False, mark_for_commit_queue=True, mark_for_landing=False
-- Begin comment --
Any committer can land this patch automatically by marking it commit-queue+.  The commit-queue will build and test the patch before landing to ensure that the revert will be successful.  This process takes approximately 15 minutes.

If you would like to land the revert faster, you can use the following command:

  webkit-patch land-attachment ATTACHMENT_ID

where ATTACHMENT_ID is the ID of this attachment.
-- End comment --
"""
        self.assert_execute_outputs(CreateRevert(), [852, "Reason"], options=self._default_options(), expected_logs=expected_logs)

    def test_create_revert_multiple_revision(self):
        expected_logs = """Preparing revert for bug 50000.
Preparing revert for bug 50000.
Unable to parse bug number from diff.
Updating working directory
MOCK create_bug
bug_title: REGRESSION(r852): Reason
bug_description: https://commits.webkit.org/r852 introduced a regression:
Reason
component: MOCK component
cc: MOCK cc
blocked: 50000, 50000
MOCK add_patch_to_bug: bug_id=60001, description=REVERT of r852, mark_for_review=False, mark_for_commit_queue=True, mark_for_landing=False
-- Begin comment --
Any committer can land this patch automatically by marking it commit-queue+.  The commit-queue will build and test the patch before landing to ensure that the revert will be successful.  This process takes approximately 15 minutes.

If you would like to land the revert faster, you can use the following command:

  webkit-patch land-attachment ATTACHMENT_ID

where ATTACHMENT_ID is the ID of this attachment.
-- End comment --
"""
        self.assert_execute_outputs(CreateRevert(), ["855 852 854", "Reason"], options=self._default_options(), expected_logs=expected_logs)

    def test_create_revert_multiple_revision_with_one_resolved(self):
        expected_logs = """Preparing revert for bug 50000.
Unable to parse bug number from diff.
Preparing revert for bug 50004.
Updating working directory
MOCK create_bug
bug_title: REGRESSION(r852): Reason
bug_description: https://commits.webkit.org/r852 introduced a regression:
Reason
component: MOCK component
cc: MOCK cc
blocked: 50000, 50004
MOCK reopen_bug 50004 with comment 'Re-opened since this is blocked by bug 60001'
MOCK add_patch_to_bug: bug_id=60001, description=REVERT of r852, mark_for_review=False, mark_for_commit_queue=True, mark_for_landing=False
-- Begin comment --
Any committer can land this patch automatically by marking it commit-queue+.  The commit-queue will build and test the patch before landing to ensure that the revert will be successful.  This process takes approximately 15 minutes.

If you would like to land the revert faster, you can use the following command:

  webkit-patch land-attachment ATTACHMENT_ID

where ATTACHMENT_ID is the ID of this attachment.
-- End comment --
"""
        self.assert_execute_outputs(CreateRevert(), ["855 852 3001", "Reason"], options=self._default_options(), expected_logs=expected_logs)

    def test_create_revert_resolved(self):
        expected_logs = """Preparing revert for bug 50004.
Updating working directory
MOCK create_bug
bug_title: REGRESSION(r3001): Reason
bug_description: https://commits.webkit.org/r3001 introduced a regression:
Reason
component: MOCK component
cc: MOCK cc
blocked: 50004
MOCK reopen_bug 50004 with comment 'Re-opened since this is blocked by bug 60001'
MOCK add_patch_to_bug: bug_id=60001, description=REVERT of r3001, mark_for_review=False, mark_for_commit_queue=True, mark_for_landing=False
-- Begin comment --
Any committer can land this patch automatically by marking it commit-queue+.  The commit-queue will build and test the patch before landing to ensure that the revert will be successful.  This process takes approximately 15 minutes.

If you would like to land the revert faster, you can use the following command:

  webkit-patch land-attachment ATTACHMENT_ID

where ATTACHMENT_ID is the ID of this attachment.
-- End comment --
"""
        self.assert_execute_outputs(CreateRevert(), [3001, "Reason"], options=self._default_options(), expected_logs=expected_logs)

    def test_create_revert_multiple_resolved(self):
        expected_logs = """Preparing revert for bug 50005.
Preparing revert for bug 50006.
Preparing revert for bug 50004.
Updating working directory
MOCK create_bug
bug_title: REGRESSION(r963): Reason
bug_description: https://commits.webkit.org/r963 introduced a regression:
Reason
component: MOCK component
cc: MOCK cc
blocked: 50005, 50006, 50004
MOCK reopen_bug 50005 with comment 'Re-opened since this is blocked by bug 60001'
MOCK reopen_bug 50006 with comment 'Re-opened since this is blocked by bug 60001'
MOCK reopen_bug 50004 with comment 'Re-opened since this is blocked by bug 60001'
MOCK add_patch_to_bug: bug_id=60001, description=REVERT of r963, mark_for_review=False, mark_for_commit_queue=True, mark_for_landing=False
-- Begin comment --
Any committer can land this patch automatically by marking it commit-queue+.  The commit-queue will build and test the patch before landing to ensure that the revert will be successful.  This process takes approximately 15 minutes.

If you would like to land the revert faster, you can use the following command:

  webkit-patch land-attachment ATTACHMENT_ID

where ATTACHMENT_ID is the ID of this attachment.
-- End comment --
"""
        self.assert_execute_outputs(CreateRevert(), ["987 3001 963", "Reason"], options=self._default_options(), expected_logs=expected_logs)

    def test_revert(self):
        expected_logs = """Preparing revert for bug 50000.
Updating working directory
MOCK: user.open_url: file://...
Was that diff correct?
Building WebKit
Committed r49824: <https://commits.webkit.org/r49824>
MOCK reopen_bug 50000 with comment 'Reverted r852 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
"""
        with self.mock_svn_remote():
            self.assert_execute_outputs(Revert(), [852, "Reason", "Description"], options=self._default_options(), expected_logs=expected_logs)

    def test_revert_two_revisions(self):
        expected_logs = """Preparing revert for bug 50000.
Preparing revert for bug 50005.
Updating working directory
MOCK: user.open_url: file://...
Was that diff correct?
Building WebKit
Committed r49824: <https://commits.webkit.org/r49824>
MOCK reopen_bug 50000 with comment 'Reverted r852 and r963 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
MOCK reopen_bug 50005 with comment 'Reverted r852 and r963 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
"""
        with self.mock_svn_remote():
            self.assert_execute_outputs(Revert(), ["852 963", "Reason", "Description"], options=self._default_options(), expected_logs=expected_logs)

    def test_revert_multiple_revisions(self):
        expected_logs = """Preparing revert for bug 50000.
Preparing revert for bug 50005.
Preparing revert for bug 50004.
Updating working directory
MOCK: user.open_url: file://...
Was that diff correct?
Building WebKit
Committed r49824: <https://commits.webkit.org/r49824>
MOCK reopen_bug 50000 with comment 'Reverted r852, r963, and r3001 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
MOCK reopen_bug 50005 with comment 'Reverted r852, r963, and r3001 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
MOCK reopen_bug 50004 with comment 'Reverted r852, r963, and r3001 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
"""
        with self.mock_svn_remote():
            self.assert_execute_outputs(Revert(), ["852 3001 963", "Reason", "Description"], options=self._default_options(), expected_logs=expected_logs)

    def test_revert_multiple_revisions_with_a_missing_bug_id(self):
        expected_logs = """Preparing revert for bug 50000.
Preparing revert for bug 50005.
Unable to parse bug number from diff.
Updating working directory
MOCK: user.open_url: file://...
Was that diff correct?
Building WebKit
Committed r49824: <https://commits.webkit.org/r49824>
MOCK reopen_bug 50000 with comment 'Reverted r852, r963, and r999 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
MOCK reopen_bug 50005 with comment 'Reverted r852, r963, and r999 for reason:

Reason Description

Committed r49824 (5@main): <https://commits.webkit.org/5@main>'
"""
        with self.mock_svn_remote():
            self.assert_execute_outputs(Revert(), ["852 999 963", "Reason", "Description"], options=self._default_options(), expected_logs=expected_logs)
