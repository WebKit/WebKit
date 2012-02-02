# Copyright (C) 2009 Google Inc. All rights reserved.
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

from webkitpy.thirdparty.mock import Mock
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.bot.queueengine import QueueEngine
from webkitpy.tool.commands.earlywarningsystem import *
from webkitpy.tool.commands.queuestest import QueuesTest
from webkitpy.tool.mocktool import MockTool, MockOptions


class AbstractEarlyWarningSystemTest(QueuesTest):
    def test_failing_tests_message(self):
        # Needed to define port_name, used in AbstractEarlyWarningSystem.__init__
        class TestEWS(AbstractEarlyWarningSystem):
            port_name = "win"  # Needs to be a port which port/factory understands.

        ews = TestEWS()
        ews.bind_to_tool(MockTool())
        ews._options = MockOptions(port=None, confirm=False)
        OutputCapture().assert_outputs(self, ews.begin_work_queue, expected_stderr=self._default_begin_work_queue_stderr(ews.name))
        ews._expected_failures.unexpected_failures_observed = lambda results: set(["foo.html", "bar.html"])
        task = Mock()
        patch = ews._tool.bugs.fetch_attachment(10000)
        self.assertEqual(ews._failing_tests_message(task, patch), "New failing tests:\nbar.html\nfoo.html")


class EarlyWarningSytemTest(QueuesTest):
    def _default_expected_stderr(self, ews):
        string_replacemnts = {
            "name": ews.name,
            "port": ews.port_name,
        }
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr(ews.name),
            "handle_unexpected_error": "Mock error message\n",
            "next_work_item": "",
            "process_work_item": "MOCK: update_status: %(name)s Pass\nMOCK: release_work_item: %(name)s 10000\n" % string_replacemnts,
            "handle_script_error": "ScriptError error message\n",
        }
        return expected_stderr

    def _test_builder_ews(self, ews):
        ews.bind_to_tool(MockTool())
        self.assert_queue_outputs(ews, expected_stderr=self._default_expected_stderr(ews))

    def _test_committer_only_ews(self, ews):
        ews.bind_to_tool(MockTool())
        expected_stderr = self._default_expected_stderr(ews)
        string_replacemnts = {"name": ews.name}
        expected_stderr["process_work_item"] = "MOCK: update_status: %(name)s Error: %(name)s cannot process patches from non-committers :(\nMOCK: release_work_item: %(name)s 10000\n" % string_replacemnts
        self.assert_queue_outputs(ews, expected_stderr=expected_stderr)

    def _test_testing_ews(self, ews):
        ews.layout_test_results = lambda: None
        ews.bind_to_tool(MockTool())
        expected_stderr = self._default_expected_stderr(ews)
        expected_stderr["handle_script_error"] = "ScriptError error message\n"
        self.assert_queue_outputs(ews, expected_stderr=expected_stderr)

    def test_committer_only_ewses(self):
        self._test_committer_only_ews(ChromiumMacEWS())

    def test_builder_ewses(self):
        self._test_builder_ews(MacEWS())
        self._test_builder_ews(ChromiumWindowsEWS())
        self._test_builder_ews(QtEWS())
        self._test_builder_ews(GtkEWS())
        self._test_builder_ews(EflEWS())

    def test_testing_ewses(self):
        self._test_testing_ews(ChromiumLinuxEWS())
