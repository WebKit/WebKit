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
    def test_can_build(self):
        # Needed to define port_name, used in AbstractEarlyWarningSystem.__init__
        class TestEWS(AbstractEarlyWarningSystem):
            port_name = "win"  # Needs to be a port which port/factory understands.

        queue = TestEWS()
        queue.bind_to_tool(MockTool(log_executive=True))
        queue._options = MockOptions(port=None)
        expected_stderr = "MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build', '--port=win', '--build-style=release', '--force-clean', '--no-update']\n"
        OutputCapture().assert_outputs(self, queue._can_build, [], expected_stderr=expected_stderr)

        def mock_run_webkit_patch(args):
            raise ScriptError("MOCK script error")

        queue.run_webkit_patch = mock_run_webkit_patch
        expected_stderr = "MOCK: update_status: None Unable to perform a build\n"
        OutputCapture().assert_outputs(self, queue._can_build, [], expected_stderr=expected_stderr)

    # FIXME: This belongs on an AbstractReviewQueueTest object in queues_unittest.py
    def test_subprocess_handled_error(self):
        queue = AbstractReviewQueue()
        queue.bind_to_tool(MockTool())

        def mock_review_patch(patch):
            raise ScriptError('MOCK script error', exit_code=QueueEngine.handled_error_code)

        queue.review_patch = mock_review_patch
        mock_patch = queue._tool.bugs.fetch_attachment(197)
        expected_stderr = "MOCK: release_work_item: None 197\n"
        OutputCapture().assert_outputs(self, queue.process_work_item, [mock_patch], expected_stderr=expected_stderr, expected_exception=ScriptError)


class EarlyWarningSytemTest(QueuesTest):
    def test_failed_builds(self):
        ews = ChromiumLinuxEWS()
        ews.bind_to_tool(MockTool())
        ews._build = lambda patch, first_run=False: False
        ews._can_build = lambda: True
        mock_patch = ews._tool.bugs.fetch_attachment(197)
        ews.review_patch(mock_patch)

    def _default_expected_stderr(self, ews):
        string_replacemnts = {
            "name": ews.name,
            "port": ews.port_name,
            "watchers": ews.watchers,
        }
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr(ews.name, ews._tool.scm().checkout_root),
            "handle_unexpected_error": "Mock error message\n",
            "next_work_item": "",
            "process_work_item": "MOCK: update_status: %(name)s Pass\nMOCK: release_work_item: %(name)s 197\n" % string_replacemnts,
            "handle_script_error": "MOCK: update_status: %(name)s ScriptError error message\nMOCK bug comment: bug_id=42, cc=%(watchers)s\n--- Begin comment ---\nAttachment 197 did not build on %(port)s:\nBuild output: http://dummy_url\n--- End comment ---\n\n" % string_replacemnts,
        }
        return expected_stderr

    def _test_ews(self, ews):
        ews.bind_to_tool(MockTool())
        expected_exceptions = {
            "handle_script_error": SystemExit,
        }
        self.assert_queue_outputs(ews, expected_stderr=self._default_expected_stderr(ews), expected_exceptions=expected_exceptions)

    def _test_committer_only_ews(self, ews):
        ews.bind_to_tool(MockTool())
        expected_stderr = self._default_expected_stderr(ews)
        string_replacemnts = {"name": ews.name}
        expected_stderr["process_work_item"] = "MOCK: update_status: %(name)s Error: %(name)s cannot process patches from non-committers :(\nMOCK: release_work_item: %(name)s 197\n" % string_replacemnts
        expected_exceptions = {"handle_script_error": SystemExit}
        self.assert_queue_outputs(ews, expected_stderr=expected_stderr, expected_exceptions=expected_exceptions)

    # FIXME: If all EWSes are going to output the same text, we
    # could test them all in one method with a for loop over an array.
    def test_chromium_linux_ews(self):
        self._test_ews(ChromiumLinuxEWS())

    def test_chromium_windows_ews(self):
        self._test_ews(ChromiumWindowsEWS())

    def test_qt_ews(self):
        self._test_ews(QtEWS())

    def test_gtk_ews(self):
        self._test_ews(GtkEWS())

    def test_efl_ews(self):
        self._test_ews(EflEWS())

    def test_mac_ews(self):
        self._test_committer_only_ews(MacEWS())

    def test_chromium_mac_ews(self):
        self._test_committer_only_ews(ChromiumMacEWS())
