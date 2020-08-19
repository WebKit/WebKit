# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import logging
import random
import unittest

from webkitpy.tool.bot import irc_command
from webkitpy.tool.bot.queueengine import TerminateQueue
from webkitpy.tool.bot.sheriff import Sheriff
from webkitpy.tool.bot.ircbot import IRCBot
from webkitpy.tool.bot.ircbot import UnknownCommand
from webkitpy.tool.bot.sheriff_unittest import MockSheriffBot
from webkitpy.tool.mocktool import MockTool

from webkitcorepy import OutputCapture


def run(message):
    tool = MockTool()
    tool.ensure_irc_connected(None)
    bot = IRCBot("sheriffbot", tool, Sheriff(tool, MockSheriffBot()), irc_command.commands)
    bot._message_queue.post(["mock_nick", message])
    bot.process_pending_messages()


class IRCBotTest(unittest.TestCase):
    def test_parse_command_and_args(self):
        tool = MockTool()
        bot = IRCBot("sheriffbot", tool, Sheriff(tool, MockSheriffBot()), irc_command.commands)
        self.assertEqual(bot._parse_command_and_args(""), (UnknownCommand, [""]))
        self.assertEqual(bot._parse_command_and_args("   "), (UnknownCommand, [""]))
        self.assertEqual(bot._parse_command_and_args(" hi "), (irc_command.Hi, []))
        self.assertEqual(bot._parse_command_and_args(" hi there "), (irc_command.Hi, ["there"]))

    def test_exception_during_command(self):
        tool = MockTool()
        tool.ensure_irc_connected(None)
        bot = IRCBot("sheriffbot", tool, Sheriff(tool, MockSheriffBot()), irc_command.commands)

        class CommandWithException(object):
            def execute(self, nick, args, tool, sheriff):
                raise Exception("mock_exception")

        bot._parse_command_and_args = lambda request: (CommandWithException, [])
        with OutputCapture(level=logging.INFO) as captured:
            bot.process_message('mock_nick', 'ignored message')
        self.assertEqual(captured.root.log.getvalue(), 'MOCK: irc.post: Exception executing command: mock_exception\n')


        class CommandWithException(object):
            def execute(self, nick, args, tool, sheriff):
                raise KeyboardInterrupt()

        bot._parse_command_and_args = lambda request: (CommandWithException, [])
        # KeyboardInterrupt and SystemExit are not subclasses of Exception and thus correctly will not be caught.
        with self.assertRaises(KeyboardInterrupt):
            with OutputCapture(level=logging.INFO):
                bot.process_message('mock_nick', 'ignored message')

    def test_hi(self):
        random.seed(23324)
        with OutputCapture(level=logging.INFO) as captured:
            run('hi')
        self.assertEqual(
            captured.root.log.getvalue(),
            'MOCK: irc.post: "Only you can prevent forest fires." -- Smokey the Bear\n',
        )

    def test_help(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('help')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Available commands: create-bug, help, hi, ping, restart, revert, whois, yt?
MOCK: irc.post: mock_nick: Type "mock-sheriff-bot: help COMMAND" for help on my individual commands.
''',
        )

        expected_logs = '''MOCK: irc.post: mock_nick: Usage: hi
MOCK: irc.post: mock_nick: Responds with hi.
MOCK: irc.post: mock_nick: Aliases: hello
'''
        with OutputCapture(level=logging.INFO) as captured:
            run('help hi')
        self.assertEqual(captured.root.log.getvalue(), expected_logs)

        with OutputCapture(level=logging.INFO) as captured:
            run('help hello')
        self.assertEqual(captured.root.log.getvalue(), expected_logs)

    def test_restart(self):
        with OutputCapture(level=logging.INFO) as captured:
            with self.assertRaises(TerminateQueue):
                run('restart')
        self.assertEqual(captured.root.log.getvalue(), 'MOCK: irc.post: Restarting...\n')

    def test_rollout(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('rollout 21654 This patch broke the world')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Created a revert patch: http://example.com/36936
''',
        )

    def test_revert(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654 This patch broke the world')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Created a revert patch: http://example.com/36936
''',
        )

    def test_multi_revert(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654 21655 21656 This 21654 patch broke the world')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654, https://trac.webkit.org/changeset/21655, and https://trac.webkit.org/changeset/21656 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Created a revert patch: http://example.com/36936
''',
        )

    def test_revert_with_r_in_svn_revision(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert r21654 This patch broke the world')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Created a revert patch: http://example.com/36936
''',
        )

    def test_multi_revert_with_r_in_svn_revision(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert r21654 21655 r21656 This r21654 patch broke the world')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654, https://trac.webkit.org/changeset/21655, and https://trac.webkit.org/changeset/21656 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Created a revert patch: http://example.com/36936
''',
        )

    def test_revert_bananas(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert bananas')
        self.assertEqual(
            captured.root.log.getvalue(),
            'MOCK: irc.post: mock_nick: Usage: revert SVN_REVISION [SVN_REVISIONS] REASON\n',
        )

    def test_revert_invalidate_revision(self):
        # When folks pass junk arguments, we should just spit the usage back at them.
        with OutputCapture(level=logging.INFO) as captured:
            run('revert --component=Tools 21654')
        self.assertEqual(
            captured.root.log.getvalue(),
            'MOCK: irc.post: mock_nick: Usage: revert SVN_REVISION [SVN_REVISIONS] REASON\n',
        )

    def test_revert_invalidate_reason(self):
        # FIXME: I'm slightly confused as to why this doesn't return the USAGE message.
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654 -bad')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Failed to create revert patch:
MOCK: irc.post: The revert reason may not begin with - (\"-bad (Requested by mock_nick on #webkit).\").
''',
        )

    def test_multi_revert_invalidate_reason(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654 21655 r21656 -bad')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''MOCK: irc.post: mock_nick: Preparing revert for https://trac.webkit.org/changeset/21654, https://trac.webkit.org/changeset/21655, and https://trac.webkit.org/changeset/21656 ...
MOCK: irc.post: mock_nick, abarth, ap, darin: Failed to create revert patch:
MOCK: irc.post: The revert reason may not begin with - (\"-bad (Requested by mock_nick on #webkit).\").
''',
        )

    def test_revert_no_reason(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654')
        self.assertEqual(
            captured.root.log.getvalue(),
            'MOCK: irc.post: mock_nick: Usage: revert SVN_REVISION [SVN_REVISIONS] REASON\n',
        )

    def _test_multi_revert_no_reason(self):
        with OutputCapture(level=logging.INFO) as captured:
            run('revert 21654 21655 r21656')
        self.assertEqual(
            captured.root.log.getvalue(),
            'MOCK: irc.post: mock_nick: Usage: revert SVN_REVISION [SVN_REVISIONS] REASON\n',
        )
