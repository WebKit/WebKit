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

from webkitpy.common.checkout.scm import CheckoutNeedsUpdate
from webkitpy.common.net.bugzilla import Attachment
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.queues import *
from webkitpy.tool.commands.queuestest import QueuesTest, MockPatch
from webkitpy.tool.commands.stepsequence import StepSequence
from webkitpy.tool.mocktool import MockTool, MockSCM, MockStatusServer


class TestQueue(AbstractPatchQueue):
    name = "test-queue"


class TestReviewQueue(AbstractReviewQueue):
    name = "test-review-queue"


class TestFeederQueue(FeederQueue):
    _sleep_duration = 0


class MockRolloutPatch(MockPatch):
    def is_rollout(self):
        return True


class AbstractQueueTest(CommandsTest):
    def test_log_directory(self):
        self.assertEquals(TestQueue()._log_directory(), "test-queue-logs")

    def _assert_run_webkit_patch(self, run_args):
        queue = TestQueue()
        tool = MockTool()
        tool.executive = Mock()
        queue.bind_to_tool(tool)

        queue.run_webkit_patch(run_args)
        expected_run_args = ["echo", "--status-host=example.com"] + run_args
        tool.executive.run_and_throw_if_fail.assert_called_with(expected_run_args)

    def test_run_webkit_patch(self):
        self._assert_run_webkit_patch([1])
        self._assert_run_webkit_patch(["one", 2])

    def test_iteration_count(self):
        queue = TestQueue()
        queue.options = Mock()
        queue.options.iterations = 3
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertFalse(queue.should_continue_work_queue())

    def test_no_iteration_count(self):
        queue = TestQueue()
        queue.options = Mock()
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())
        self.assertTrue(queue.should_continue_work_queue())

    def _assert_log_message(self, script_error, log_message):
        failure_log = AbstractQueue._log_from_script_error_for_upload(script_error, output_limit=10)
        self.assertTrue(failure_log.read(), log_message)

    def test_log_from_script_error_for_upload(self):
        self._assert_log_message(ScriptError("test"), "test")
        # In python 2.5 unicode(Exception) is busted. See:
        # http://bugs.python.org/issue2517
        # With no good workaround, we just ignore these tests.
        if not hasattr(Exception, "__unicode__"):
            return

        unicode_tor = u"WebKit \u2661 Tor Arne Vestb\u00F8!"
        utf8_tor = unicode_tor.encode("utf-8")
        self._assert_log_message(ScriptError(unicode_tor), utf8_tor)
        script_error = ScriptError(unicode_tor, output=unicode_tor)
        expected_output = "%s\nLast %s characters of output:\n%s" % (utf8_tor, 10, utf8_tor[-10:])
        self._assert_log_message(script_error, expected_output)


class FeederQueueTest(QueuesTest):
    def test_feeder_queue(self):
        queue = TestFeederQueue()
        tool = MockTool(log_executive=True)
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("feeder-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "",
            "next_work_item": "",
            "process_work_item": """Warning, attachment 128 on bug 42 has invalid committer (non-committer@example.com)
Warning, attachment 128 on bug 42 has invalid committer (non-committer@example.com)
MOCK setting flag 'commit-queue' to '-' on attachment '128' with comment 'Rejecting patch 128 from commit-queue.' and additional comment 'non-committer@example.com does not have committer permissions according to http://trac.webkit.org/browser/trunk/WebKitTools/Scripts/webkitpy/common/config/committers.py.

- If you do not have committer rights please read http://webkit.org/coding/contributing.html for instructions on how to use bugzilla flags.

- If you have committer rights please correct the error in WebKitTools/Scripts/webkitpy/common/config/committers.py by adding yourself to the file (no review needed).  The commit-queue restarts itself every 2 hours.  After restart the commit-queue will correctly respect your committer rights.'
MOCK: update_work_items: commit-queue [106, 197]
Feeding commit-queue items [106, 197]
""",
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(queue, tool=tool, expected_stderr=expected_stderr)


class AbstractPatchQueueTest(CommandsTest):
    def test_fetch_next_work_item(self):
        queue = AbstractPatchQueue()
        tool = MockTool()
        queue.bind_to_tool(tool)
        self.assertEquals(queue._fetch_next_work_item(), None)
        tool.status_server = MockStatusServer(work_items=[2, 1, 3])
        self.assertEquals(queue._fetch_next_work_item(), 2)


class AbstractReviewQueueTest(CommandsTest):
    def test_patch_collection_delegate_methods(self):
        queue = TestReviewQueue()
        tool = MockTool()
        queue.bind_to_tool(tool)
        self.assertEquals(queue.collection_name(), "test-review-queue")
        self.assertEquals(queue.fetch_potential_patch_ids(), [103])
        queue.status_server()
        self.assertTrue(queue.is_terminal_status("Pass"))
        self.assertTrue(queue.is_terminal_status("Fail"))
        self.assertTrue(queue.is_terminal_status("Error: Your patch exploded"))
        self.assertFalse(queue.is_terminal_status("Foo"))


class NeedsUpdateSequence(StepSequence):
    def _run(self, tool, options, state):
        raise CheckoutNeedsUpdate([], 1, "", None)


class AlwaysCommitQueueTool(object):
    def __init__(self):
        self.status_server = MockStatusServer()

    def command_by_name(self, name):
        return CommitQueue


class SecondThoughtsCommitQueue(CommitQueue):
    def _build_and_test_patch(self, patch, first_run=True):
        attachment_dictionary = {
            "id": patch.id(),
            "bug_id": patch.bug_id(),
            "name": "Rejected",
            "is_obsolete": True,
            "is_patch": False,
            "review": "-",
            "reviewer_email": "foo@bar.com",
            "commit-queue": "-",
            "committer_email": "foo@bar.com",
            "attacher_email": "Contributer1",
        }
        patch = Attachment(attachment_dictionary, None)
        self._tool.bugs.set_override_patch(patch)
        return True


class CommitQueueTest(QueuesTest):
    def test_commit_queue(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Landing patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK: update_status: commit-queue Applied patch
MOCK: update_status: commit-queue Built patch
MOCK: update_status: commit-queue Passed tests
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting patch 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        self.assert_queue_outputs(CommitQueue(), expected_stderr=expected_stderr)

    def test_rollout(self):
        tool = MockTool(log_executive=True)
        tool.buildbot.light_tree_on_fire()
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Landing patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'apply-attachment', '--force-clean', '--non-interactive', '--quiet', 197]
MOCK: update_status: commit-queue Applied patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build', '--no-clean', '--no-update', '--build', '--build-style=both', '--quiet']
MOCK: update_status: commit-queue Built patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build-and-test', '--no-clean', '--no-update', '--test', '--quiet', '--non-interactive']
MOCK: update_status: commit-queue Passed tests
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'land-attachment', '--force-clean', '--ignore-builders', '--quiet', '--non-interactive', '--parent-command=commit-queue', 197]
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting patch 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        self.assert_queue_outputs(CommitQueue(), tool=tool, expected_stderr=expected_stderr)

    def test_rollout_lands(self):
        tool = MockTool(log_executive=True)
        tool.buildbot.light_tree_on_fire()
        rollout_patch = MockRolloutPatch()
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Landing rollout patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'apply-attachment', '--force-clean', '--non-interactive', '--quiet', 197]
MOCK: update_status: commit-queue Applied patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build', '--no-clean', '--no-update', '--build', '--build-style=both', '--quiet']
MOCK: update_status: commit-queue Built patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build-and-test', '--no-clean', '--no-update', '--test', '--quiet', '--non-interactive']
MOCK: update_status: commit-queue Passed tests
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'land-attachment', '--force-clean', '--ignore-builders', '--quiet', '--non-interactive', '--parent-command=commit-queue', 197]
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting patch 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        self.assert_queue_outputs(CommitQueue(), tool=tool, work_item=rollout_patch, expected_stderr=expected_stderr)

    def test_auto_retry(self):
        queue = CommitQueue()
        options = Mock()
        options.parent_command = "commit-queue"
        tool = AlwaysCommitQueueTool()
        sequence = NeedsUpdateSequence(None)

        expected_stderr = "Commit failed because the checkout is out of date.  Please update and try again.\nMOCK: update_status: commit-queue Tests passed, but commit failed (checkout out of date).  Updating, then landing without building or re-running tests.\n"
        state = {'patch': None}
        OutputCapture().assert_outputs(self, sequence.run_and_handle_errors, [tool, options, state], expected_exception=TryAgain, expected_stderr=expected_stderr)

        self.assertEquals(options.update, True)
        self.assertEquals(options.build, False)
        self.assertEquals(options.test, False)

    def test_manual_reject_during_processing(self):
        queue = SecondThoughtsCommitQueue()
        queue.bind_to_tool(MockTool())
        expected_stderr = """MOCK: update_status: commit-queue Applied patch
MOCK: update_status: commit-queue Built patch
MOCK: update_status: commit-queue Passed tests
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
"""
        OutputCapture().assert_outputs(self, queue.process_work_item, [MockPatch()], expected_stderr=expected_stderr)


class RietveldUploadQueueTest(QueuesTest):
    def test_rietveld_upload_queue(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("rietveld-upload-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: rietveld-upload-queue Uploading patch\n",
            "process_work_item": "MOCK: update_status: rietveld-upload-queue Pass\n",
            "handle_unexpected_error": "Mock error message\nMOCK setting flag 'in-rietveld' to '-' on attachment '197' with comment 'None' and additional comment 'None'\n",
            "handle_script_error": "ScriptError error message\nMOCK: update_status: rietveld-upload-queue ScriptError error message\nMOCK setting flag 'in-rietveld' to '-' on attachment '197' with comment 'None' and additional comment 'None'\n",
        }
        self.assert_queue_outputs(RietveldUploadQueue(), expected_stderr=expected_stderr)


class StyleQueueTest(QueuesTest):
    def test_style_queue(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("style-queue", MockSCM.fake_checkout_root),
            "next_work_item": "MOCK: update_work_items: style-queue [103]\n",
            "should_proceed_with_work_item": "MOCK: update_status: style-queue Checking style\n",
            "process_work_item": "MOCK: update_status: style-queue Pass\n",
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "MOCK: update_status: style-queue ScriptError error message\nMOCK bug comment: bug_id=142, cc=[]\n--- Begin comment ---\\Attachment 197 did not pass style-queue:\n\nScriptError error message\n\nIf any of these errors are false positives, please file a bug against check-webkit-style.\n--- End comment ---\n\n",
        }
        expected_exceptions = {
            "handle_script_error": SystemExit,
        }
        self.assert_queue_outputs(StyleQueue(), expected_stderr=expected_stderr, expected_exceptions=expected_exceptions)
