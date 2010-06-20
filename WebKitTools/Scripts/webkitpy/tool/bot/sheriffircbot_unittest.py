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
import random

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.bot.sheriff import Sheriff
from webkitpy.tool.bot.sheriffircbot import SheriffIRCBot
from webkitpy.tool.bot.sheriff_unittest import MockSheriffBot
from webkitpy.tool.mocktool import MockTool


def run(message):
    tool = MockTool()
    tool.ensure_irc_connected(None)
    bot = SheriffIRCBot(tool, Sheriff(tool, MockSheriffBot()))
    bot._message_queue.post(["mock_nick", message])
    bot.process_pending_messages()


class SheriffIRCBotTest(unittest.TestCase):
    def test_hi(self):
        random.seed(23324)
        expected_stderr = 'MOCK: irc.post: "Only you can prevent forest fires." -- Smokey the Bear\n'
        OutputCapture().assert_outputs(self, run, args=["hi"], expected_stderr=expected_stderr)

    def test_help(self):
        expected_stderr = "MOCK: irc.post: mock_nick: Available commands: rollout, hi, help, restart, last-green-revision\n"
        OutputCapture().assert_outputs(self, run, args=["help"], expected_stderr=expected_stderr)

    def test_lgr(self):
        expected_stderr = "MOCK: irc.post: mock_nick: http://trac.webkit.org/changeset/9479\n"
        OutputCapture().assert_outputs(self, run, args=["last-green-revision"], expected_stderr=expected_stderr)

    def test_rollout(self):
        expected_stderr = "MOCK: irc.post: Preparing rollout for r21654...\nMOCK: irc.post: mock_nick: Created rollout: http://example.com/36936\n"
        OutputCapture().assert_outputs(self, run, args=["rollout 21654 This patch broke the world"], expected_stderr=expected_stderr)

    def test_rollout_with_r_in_svn_revision(self):
        expected_stderr = "MOCK: irc.post: Preparing rollout for r21654...\nMOCK: irc.post: mock_nick: Created rollout: http://example.com/36936\n"
        OutputCapture().assert_outputs(self, run, args=["rollout r21654 This patch broke the world"], expected_stderr=expected_stderr)

    def test_rollout_bananas(self):
        expected_stderr = "MOCK: irc.post: mock_nick: Usage: SVN_REVISION REASON\n"
        OutputCapture().assert_outputs(self, run, args=["rollout bananas"], expected_stderr=expected_stderr)

    def test_rollout_invalidate_revision(self):
        expected_stderr = ("MOCK: irc.post: Preparing rollout for r--component=Tools...\n"
                           "MOCK: irc.post: mock_nick: Failed to create rollout patch:\n"
                           "MOCK: irc.post: Invalid svn revision number \"--component=Tools\".\n")
        OutputCapture().assert_outputs(self, run,
                                       args=["rollout "
                                             "--component=Tools 21654"],
                                       expected_stderr=expected_stderr)

    def test_rollout_invalidate_reason(self):
        expected_stderr = ("MOCK: irc.post: Preparing rollout for "
                           "r21654...\nMOCK: irc.post: mock_nick: Failed to "
                           "create rollout patch:\nMOCK: irc.post: The rollout"
                           " reason may not begin with - (\"-bad (Requested "
                           "by mock_nick on #webkit).\").\n")
        OutputCapture().assert_outputs(self, run,
                                       args=["rollout "
                                             "21654 -bad"],
                                       expected_stderr=expected_stderr)

    def test_rollout_no_reason(self):
        expected_stderr = "MOCK: irc.post: mock_nick: Usage: SVN_REVISION REASON\n"
        OutputCapture().assert_outputs(self, run, args=["rollout 21654"], expected_stderr=expected_stderr)
