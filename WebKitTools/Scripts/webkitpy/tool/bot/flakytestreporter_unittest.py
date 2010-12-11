# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

from webkitpy.common.config.committers import Committer
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.bot.flakytestreporter import FlakyTestReporter
from webkitpy.tool.mocktool import MockTool, MockStatusServer


# Creating fake CommitInfos is a pain, so we use a mock one here.
class MockCommitInfo(object):
    def __init__(self, author_email):
        self._author_email = author_email

    def author(self):
        # It's definitely possible to have commits with authors who
        # are not in our committers.py list.
        if not self._author_email:
            return None
        return Committer("Mock Committer", self._author_email)


class FlakyTestReporterTest(unittest.TestCase):
    def _assert_emails_for_test(self, emails):
        tool = MockTool()
        reporter = FlakyTestReporter(tool, 'dummy-queue')
        commit_infos = [MockCommitInfo(email) for email in emails]
        tool.checkout().recent_commit_infos_for_files = lambda paths: set(commit_infos)
        self.assertEqual(reporter._author_emails_for_test([]), set(emails))

    def test_author_emails_for_test(self):
        self._assert_emails_for_test([])
        self._assert_emails_for_test(["test1@test.com", "test1@test.com"])
        self._assert_emails_for_test(["test1@test.com", "test2@test.com"])

    def test_create_bug_for_flaky_test(self):
        reporter = FlakyTestReporter(MockTool(), 'dummy-queue')
        expected_stderr = """MOCK create_bug
bug_title: Flaky Test: foo/bar.html
bug_description: This is an automatically generated bug from the dummy-queue.
foo/bar.html has been flaky on the dummy-queue.

foo/bar.html was authored by test@test.com.
http://trac.webkit.org/browser/trunk/LayoutTests/foo/bar.html

FLAKE_MESSAGE

The bots will update this with information from each new failure.

If you would like to track this test fix with another bug, please close this bug as a duplicate.

"""
        OutputCapture().assert_outputs(self, reporter._create_bug_for_flaky_test, ['foo/bar.html', ['test@test.com'], 'FLAKE_MESSAGE'], expected_stderr=expected_stderr)

    def test_follow_duplicate_chain(self):
        tool = MockTool()
        reporter = FlakyTestReporter(tool, 'dummy-queue')
        bug = tool.bugs.fetch_bug(78)
        self.assertEqual(reporter._follow_duplicate_chain(bug).id(), 76)

    def test_bot_information(self):
        tool = MockTool()
        tool.status_server = MockStatusServer("MockBotId")
        reporter = FlakyTestReporter(tool, 'dummy-queue')
        self.assertEqual(reporter._bot_information(), "Bot Id: MockBotId Port: MockPort OS: MockPlatform 1.0")

    # report_flaky_tests is tested by queues_unittest
