# Copyright (c) 2012 Google Inc. All rights reserved.
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

from webkitpy.tool.commands.chromechannels import ChromeChannels
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.mocktool import MockTool
from webkitpy.common.net.omahaproxy import OmahaProxy


class MockOmahaProxy(OmahaProxy):
    revisions = [{"commit": 20, "channel": "canary", "platform": "Mac", "date": "07/04/76"},
                 {"commit": 20, "channel": "canary", "platform": "Windows", "date": "07/04/76"},
                 {"commit": 25, "channel": "dev", "platform": "Mac", "date": "07/01/76"},
                 {"commit": 30, "channel": "dev", "platform": "Windows", "date": "03/29/82"},
                 {"commit": 30, "channel": "dev", "platform": "Linux", "date": "03/29/82"},
                 {"commit": 15, "channel": "beta", "platform": "Windows", "date": "07/04/67"},
                 {"commit": 15, "channel": "beta", "platform": "Linux", "date": "07/04/67"},
                 {"commit": 10, "channel": "stable", "platform": "Windows", "date": "07/01/67"},
                 {"commit": 20, "channel": "stable", "platform": "Linux", "date": "09/16/10"},
                 ]

    def get_revisions(self):
        return self.revisions


class TestableChromeChannels(ChromeChannels):
    def __init__(self):
        ChromeChannels.__init__(self)
        self._omahaproxy = MockOmahaProxy()


class ChromeChannelsTest(CommandsTest):

    single_bug_expectations = {
        50001: """50001 Bug with a patch needing review. (r35)
... not yet released in any Chrome channels.
""",
        50002: """50002 The third bug
... has too confusing a commit history to parse, skipping
""",
        50003: """50003 The fourth bug
... does not appear to have an associated commit.
""",
        50004: """50004 The fifth bug (r15)
 canary: Mac, Windows
    dev: Mac, Windows, Linux
   beta: Windows, Linux
 stable: Linux
"""}

    def test_single_bug(self):
        testable_chrome_channels = TestableChromeChannels()
        tool = MockTool()
        testable_chrome_channels.bind_to_tool(tool)
        revisions = testable_chrome_channels._omahaproxy.get_revisions()
        for bug_id, expectation in self.single_bug_expectations.items():
            self.assertEqual(testable_chrome_channels._channels_for_bug(revisions, testable_chrome_channels._tool.bugs.fetch_bug(bug_id)),
                             expectation)

    def test_with_query(self):
        expected_stdout = \
"""50001 Bug with a patch needing review. (r35)
... not yet released in any Chrome channels.
50002 The third bug
... has too confusing a commit history to parse, skipping
50003 The fourth bug
... does not appear to have an associated commit.
50004 The fifth bug (r15)
 canary: Mac, Windows
    dev: Mac, Windows, Linux
   beta: Windows, Linux
 stable: Linux
"""
        self.assert_execute_outputs(TestableChromeChannels(), ["foo"], expected_stdout=expected_stdout)
