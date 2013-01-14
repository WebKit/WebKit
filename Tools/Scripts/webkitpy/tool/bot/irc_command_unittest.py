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

import os
import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.bot.irc_command import *
from webkitpy.tool.mocktool import MockTool
from webkitpy.common.system.executive_mock import MockExecutive


class IRCCommandTest(unittest.TestCase):
    def test_whois(self):
        whois = Whois()
        self.assertEqual("tom: Usage: whois SEARCH_STRING",
                          whois.execute("tom", [], None, None))
        self.assertEqual("tom: Usage: whois SEARCH_STRING",
                          whois.execute("tom", ["Adam", "Barth"], None, None))
        self.assertEqual("tom: Sorry, I don't know any contributors matching 'unknown@example.com'.",
                          whois.execute("tom", ["unknown@example.com"], None, None))
        self.assertEqual("tom: tonyg@chromium.org is tonyg-cr. Why do you ask?",
                          whois.execute("tom", ["tonyg@chromium.org"], None, None))
        self.assertEqual("tom: TonyG@Chromium.org is tonyg-cr. Why do you ask?",
                          whois.execute("tom", ["TonyG@Chromium.org"], None, None))
        self.assertEqual("tom: rniwa is rniwa (rniwa@webkit.org). Why do you ask?",
                          whois.execute("tom", ["rniwa"], None, None))
        self.assertEqual("tom: lopez is xan (xan.lopez@gmail.com, xan@gnome.org, xan@webkit.org, xlopez@igalia.com). Why do you ask?",
                          whois.execute("tom", ["lopez"], None, None))
        self.assertEqual('tom: "Vicki Murley" <vicki@apple.com> hasn\'t told me their nick. Boo hoo :-(',
                          whois.execute("tom", ["vicki@apple.com"], None, None))
        self.assertEqual('tom: I\'m not sure who you mean?  gavinp or gbarra could be \'Gavin\'.',
                          whois.execute("tom", ["Gavin"], None, None))
        self.assertEqual('tom: More than 5 contributors match \'david\', could you be more specific?',
                          whois.execute("tom", ["david"], None, None))

    @staticmethod
    def _sheriff_test_data_url(suffix):
        return "file://" + os.path.join(os.path.dirname(os.path.abspath(__file__)), "testdata", "webkit_sheriff_%s.js" % suffix)

    def test_sheriffs(self):
        sheriffs = Sheriffs()
        self.assertEqual("tom: There are no Chromium Webkit sheriffs currently assigned.",
                         sheriffs.execute("tom", [self._sheriff_test_data_url("0")], None, None))
        self.assertEqual("tom: The current Chromium Webkit sheriff is: test_user",
                         sheriffs.execute("tom", [self._sheriff_test_data_url("1")], None, None))
        self.assertEqual("tom: The current Chromium Webkit sheriffs are: test_user_1, test_user_2",
                         sheriffs.execute("tom", [self._sheriff_test_data_url("2")], None, None))
        self.assertEqual("tom: Failed to parse URL: " + self._sheriff_test_data_url("malformed"),
                         sheriffs.execute("tom", [self._sheriff_test_data_url("malformed")], None, None))
        self.assertEqual("tom: Failed to parse URL: http://invalid/",
                         sheriffs.execute("tom", ["http://invalid/"], None, None))

    def test_create_bug(self):
        create_bug = CreateBug()
        self.assertEqual("tom: Usage: create-bug BUG_TITLE",
                          create_bug.execute("tom", [], None, None))

        example_args = ["sherrif-bot", "should", "have", "a", "create-bug", "command"]
        tool = MockTool()

        # MockBugzilla has a create_bug, but it logs to stderr, this avoids any logging.
        tool.bugs.create_bug = lambda a, b, cc=None, assignee=None: 50004
        self.assertEqual("tom: Created bug: http://example.com/50004",
                          create_bug.execute("tom", example_args, tool, None))

        def mock_create_bug(title, description, cc=None, assignee=None):
            raise Exception("Exception from bugzilla!")
        tool.bugs.create_bug = mock_create_bug
        self.assertEqual("tom: Failed to create bug:\nException from bugzilla!",
                          create_bug.execute("tom", example_args, tool, None))

    def test_roll_chromium_deps(self):
        roll = RollChromiumDEPS()
        self.assertEqual(None, roll._parse_args([]))
        self.assertEqual("1234", roll._parse_args(["1234"]))
        self.assertEqual('"Alan Cutter" <alancutter@chromium.org>', roll._expand_irc_nickname("alancutter"))
        self.assertEqual("unknown_irc_nickname", roll._expand_irc_nickname("unknown_irc_nickname"))

    def test_rollout_updates_working_copy(self):
        rollout = Rollout()
        tool = MockTool()
        tool.executive = MockExecutive(should_log=True)
        expected_logs = "MOCK run_and_throw_if_fail: ['mock-update-webkit'], cwd=/mock-checkout\n"
        OutputCapture().assert_outputs(self, rollout._update_working_copy, [tool], expected_logs=expected_logs)

    def test_rollout(self):
        rollout = Rollout()
        self.assertEqual(([1234], "testing foo"),
                          rollout._parse_args(["1234", "testing", "foo"]))

        self.assertEqual(([554], "testing foo"),
                          rollout._parse_args(["r554", "testing", "foo"]))

        self.assertEqual(([556, 792], "testing foo"),
                          rollout._parse_args(["r556", "792", "testing", "foo"]))

        self.assertEqual(([128, 256], "testing foo"),
                          rollout._parse_args(["r128,r256", "testing", "foo"]))

        self.assertEqual(([512, 1024, 2048], "testing foo"),
                          rollout._parse_args(["512,", "1024,2048", "testing", "foo"]))

        # Test invalid argument parsing:
        self.assertEqual((None, None), rollout._parse_args([]))
        self.assertEqual((None, None), rollout._parse_args(["--bar", "1234"]))

        # Invalid arguments result in the USAGE message.
        self.assertEqual("tom: Usage: rollout SVN_REVISION [SVN_REVISIONS] REASON",
                          rollout.execute("tom", [], None, None))

        # FIXME: We need a better way to test IRCCommands which call tool.irc().post()
