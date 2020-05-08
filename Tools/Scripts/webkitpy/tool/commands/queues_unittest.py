# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from webkitpy.common.checkout.scm import CheckoutNeedsUpdate
from webkitpy.common.checkout.scm.scm_mock import MockSCM
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.bugzilla import Attachment
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.unicode_compatibility import StringIO
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_failures
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.queues import *
from webkitpy.tool.commands.queuestest import QueuesTest
from webkitpy.tool.commands.stepsequence import StepSequence
from webkitpy.tool.mocktool import MockTool, MockOptions


class TestQueue(AbstractPatchQueue):
    name = "test-queue"


class TestReviewQueue(AbstractReviewQueue):
    name = "test-review-queue"


class AbstractQueueTest(CommandsTest):
    def test_log_directory(self):
        self.assertEqual(TestQueue()._log_directory(), os.path.join("..", "test-queue-logs"))

    def _assert_run_webkit_patch(self, run_args, port=None):
        queue = TestQueue()
        tool = MockTool()
        tool.executive = Mock()
        queue.bind_to_tool(tool)
        queue._options = Mock()
        queue._options.port = port

        queue.run_webkit_patch(run_args)
        expected_run_args = ["echo"]
        if port:
            expected_run_args.append("--port=%s" % port)
        expected_run_args.extend(run_args)
        tool.executive.run_command.assert_called_with(expected_run_args, cwd='/mock-checkout')

    def test_run_webkit_patch(self):
        self._assert_run_webkit_patch([1])
        self._assert_run_webkit_patch(["one", 2])
        self._assert_run_webkit_patch([1], port="mockport")

    def test_iteration_count(self):
        queue = TestQueue()
        queue._options = Mock()
        queue._options.iterations = 3
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertFalse(queue.should_continue_work_queue())

    def test_no_iteration_count(self):
        queue = TestQueue()
        queue._options = Mock()
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())



class PatchProcessingQueueTest(CommandsTest):
    def test_upload_results_archive_for_patch(self):
        queue = PatchProcessingQueue()
        queue.name = "mock-queue"
        tool = MockTool()
        queue.bind_to_tool(tool)
        queue._options = Mock()
        queue._options.port = None
        patch = queue._tool.bugs.fetch_attachment(10001)
        expected_logs = """MOCK add_attachment_to_bug: bug_id=50000, description=Archive of layout-test-results for mac-highsierra filename=layout-test-results.zip mimetype=None
-- Begin comment --
The attached test failures were seen while running run-webkit-tests on the mock-queue.
Port: mac-highsierra  Platform: MockPlatform 1.0
-- End comment --
"""
        OutputCapture().assert_outputs(self, queue._upload_results_archive_for_patch, [patch, Mock()], expected_logs=expected_logs)


class NeedsUpdateSequence(StepSequence):
    def _run(self, tool, options, state):
        raise CheckoutNeedsUpdate([], 1, "", None)


class StyleQueueTest(QueuesTest):
    def test_style_queue_with_style_exception(self):
        expected_logs = {
            "begin_work_queue": self._default_begin_work_queue_logs("style-queue"),
            "process_work_item": """Running: webkit-patch clean
Running: webkit-patch update
Running: webkit-patch apply-attachment --no-update --non-interactive 10000
Running: webkit-patch apply-watchlist-local 50000
Running: webkit-patch check-style-local --non-interactive --quiet
""",
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "MOCK output\n",
        }
        tool = MockTool(executive_throws_when_run=set(['check-style']))
        self.assert_queue_outputs(StyleQueue(), expected_logs=expected_logs, tool=tool)

    def test_style_queue_with_watch_list_exception(self):
        expected_logs = {
            "begin_work_queue": self._default_begin_work_queue_logs("style-queue"),
            "process_work_item": """Running: webkit-patch clean
Running: webkit-patch update
Running: webkit-patch apply-attachment --no-update --non-interactive 10000
Running: webkit-patch apply-watchlist-local 50000
Exception for ['echo', 'apply-watchlist-local', 50000]

MOCK command output
Running: webkit-patch check-style-local --non-interactive --quiet
""",
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "MOCK output\n",
        }
        tool = MockTool(executive_throws_when_run=set(['apply-watchlist-local']))
        self.assert_queue_outputs(StyleQueue(), expected_logs=expected_logs, tool=tool)

    def test_non_valid_patch(self):
        tool = MockTool()
        patch = tool.bugs.fetch_attachment(10007)  # _patch8, resolved bug, without review flag, not marked obsolete (maybe already landed)
        expected_logs = {
            "begin_work_queue": self._default_begin_work_queue_logs("style-queue"),
            "process_work_item": "",
        }
        self.assert_queue_outputs(StyleQueue(), tool=tool, work_item=patch, expected_logs=expected_logs)
