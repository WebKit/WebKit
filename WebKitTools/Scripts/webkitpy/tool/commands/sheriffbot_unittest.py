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

from webkitpy.tool.commands.queuestest import QueuesTest
from webkitpy.tool.commands.sheriffbot import SheriffBot
from webkitpy.tool.mocktool import *


class SheriffBotTest(QueuesTest):
    builder1 = MockBuilder("Builder1")
    builder2 = MockBuilder("Builder2")

    def test_sheriff_bot(self):
        mock_work_item = MockFailureMap(MockTool().buildbot)
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("sheriff-bot", os.getcwd()),
            "next_work_item": "",
            "process_work_item": """MOCK: irc.post: abarth, darin, eseidel: http://trac.webkit.org/changeset/29837 might have broken Builder1
MOCK bug comment: bug_id=42, cc=['abarth@webkit.org', 'eric@webkit.org']
--- Begin comment ---
http://trac.webkit.org/changeset/29837 might have broken Builder1
The following tests are not passing:
mock-test-1
--- End comment ---

""",
            "handle_unexpected_error": "Mock error message\n"
        }
        self.assert_queue_outputs(SheriffBot(), work_item=mock_work_item, expected_stderr=expected_stderr)
