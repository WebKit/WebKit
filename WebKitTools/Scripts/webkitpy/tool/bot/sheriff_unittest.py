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

import os
import unittest

from webkitpy.common.net.buildbot import Builder
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.bot.sheriff import Sheriff
from webkitpy.tool.mocktool import MockTool


class MockSheriffBot(object):
    name = "mock-sheriff-bot"
    watchers = [
        "watcher@example.com",
    ]

    def run_webkit_patch(self, args):
        return "Created bug https://bugs.webkit.org/show_bug.cgi?id=36936\n"


class SheriffTest(unittest.TestCase):
    def test_rollout_reason(self):
        sheriff = Sheriff(MockTool(), MockSheriffBot())
        builders = [
            Builder("Foo", None),
            Builder("Bar", None),
        ]
        reason = "Caused builders Foo and Bar to fail."
        self.assertEquals(sheriff._rollout_reason(builders), reason)

    def test_post_blame_comment_on_bug(self):
        def run():
            sheriff = Sheriff(MockTool(), MockSheriffBot())
            builders = [
                Builder("Foo", None),
                Builder("Bar", None),
            ]
            commit_info = Mock()
            commit_info.bug_id = lambda: None
            commit_info.revision = lambda: 4321
            # Should do nothing with no bug_id
            sheriff.post_blame_comment_on_bug(commit_info, builders, [])
            sheriff.post_blame_comment_on_bug(commit_info, builders, [2468, 5646])
            # Should try to post a comment to the bug, but MockTool.bugs does nothing.
            commit_info.bug_id = lambda: 1234
            sheriff.post_blame_comment_on_bug(commit_info, builders, [])
            sheriff.post_blame_comment_on_bug(commit_info, builders, [3432])
            sheriff.post_blame_comment_on_bug(commit_info, builders, [841, 5646])

        expected_stderr = u"MOCK bug comment: bug_id=1234, cc=['watcher@example.com']\n--- Begin comment ---\\http://trac.webkit.org/changeset/4321 might have broken Foo and Bar\n--- End comment ---\n\nMOCK bug comment: bug_id=1234, cc=['watcher@example.com']\n--- Begin comment ---\\http://trac.webkit.org/changeset/4321 might have broken Foo and Bar\n--- End comment ---\n\nMOCK bug comment: bug_id=1234, cc=['watcher@example.com']\n--- Begin comment ---\\http://trac.webkit.org/changeset/4321 might have broken Foo and Bar\nThe following changes are on the blame list:\nhttp://trac.webkit.org/changeset/841\nhttp://trac.webkit.org/changeset/5646\n--- End comment ---\n\n"
        OutputCapture().assert_outputs(self, run, expected_stderr=expected_stderr)

    def test_provoke_flaky_builders(self):
        def run():
            tool = MockTool()
            tool.buildbot.light_tree_on_fire()
            sheriff = Sheriff(tool, MockSheriffBot())
            revisions_causing_failures = {}
            sheriff.provoke_flaky_builders(revisions_causing_failures)
        expected_stderr = "MOCK: force_build: name=Builder2, username=mock-sheriff-bot, comments=Probe for flakiness.\n"
        OutputCapture().assert_outputs(self, run, expected_stderr=expected_stderr)

    def test_post_blame_comment_on_bug(self):
        sheriff = Sheriff(MockTool(), MockSheriffBot())
        builders = [
            Builder("Foo", None),
            Builder("Bar", None),
        ]
        commit_info = Mock()
        commit_info.bug_id = lambda: None
        commit_info.revision = lambda: 4321
        commit_info.committer = lambda: None
        commit_info.committer_email = lambda: "foo@example.com"
        commit_info.reviewer = lambda: None
        commit_info.author = lambda: None
        sheriff.post_automatic_rollout_patch(commit_info, builders)

