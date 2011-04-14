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
import StringIO

from webkitpy.common.checkout.scm import CheckoutNeedsUpdate
from webkitpy.common.net.bugzilla import Attachment
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.layout_package import test_results
from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.queues import *
from webkitpy.tool.commands.queuestest import QueuesTest
from webkitpy.tool.commands.stepsequence import StepSequence
from webkitpy.tool.mocktool import MockTool, MockSCM, MockStatusServer


class TestQueue(AbstractPatchQueue):
    name = "test-queue"


class TestReviewQueue(AbstractReviewQueue):
    name = "test-review-queue"


class TestFeederQueue(FeederQueue):
    _sleep_duration = 0


class AbstractQueueTest(CommandsTest):
    def test_log_directory(self):
        self.assertEquals(TestQueue()._log_directory(), os.path.join("..", "test-queue-logs"))

    def _assert_run_webkit_patch(self, run_args, port=None):
        queue = TestQueue()
        tool = MockTool()
        tool.status_server.bot_id = "gort"
        tool.executive = Mock()
        queue.bind_to_tool(tool)
        queue._options = Mock()
        queue._options.port = port

        queue.run_webkit_patch(run_args)
        expected_run_args = ["echo", "--status-host=example.com", "--bot-id=gort"]
        if port:
            expected_run_args.append("--port=%s" % port)
        expected_run_args.extend(run_args)
        tool.executive.run_and_throw_if_fail.assert_called_with(expected_run_args)

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
MOCK setting flag 'commit-queue' to '-' on attachment '128' with comment 'Rejecting attachment 128 from commit-queue.' and additional comment 'non-committer@example.com does not have committer permissions according to http://trac.webkit.org/browser/trunk/Tools/Scripts/webkitpy/common/config/committers.py.

- If you do not have committer rights please read http://webkit.org/coding/contributing.html for instructions on how to use bugzilla flags.

- If you have committer rights please correct the error in Tools/Scripts/webkitpy/common/config/committers.py by adding yourself to the file (no review needed).  The commit-queue restarts itself every 2 hours.  After restart the commit-queue will correctly respect your committer rights.'
MOCK: update_work_items: commit-queue [106, 197]
Feeding commit-queue items [106, 197]
Feeding EWS (1 r? patch, 1 new)
MOCK: submit_to_ews: 103
""",
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(queue, tool=tool, expected_stderr=expected_stderr)


class AbstractPatchQueueTest(CommandsTest):
    def test_next_patch(self):
        queue = AbstractPatchQueue()
        tool = MockTool()
        queue.bind_to_tool(tool)
        queue._options = Mock()
        queue._options.port = None
        self.assertEquals(queue._next_patch(), None)
        tool.status_server = MockStatusServer(work_items=[2, 197])
        expected_stdout = "MOCK: fetch_attachment: 2 is not a known attachment id\n"  # A mock-only message to prevent us from making mistakes.
        expected_stderr = "MOCK: release_work_item: None 2\n"
        patch_id = OutputCapture().assert_outputs(self, queue._next_patch, [], expected_stdout=expected_stdout, expected_stderr=expected_stderr)
        self.assertEquals(patch_id, None)  # 2 is an invalid patch id
        self.assertEquals(queue._next_patch().id(), 197)


class NeedsUpdateSequence(StepSequence):
    def _run(self, tool, options, state):
        raise CheckoutNeedsUpdate([], 1, "", None)


class AlwaysCommitQueueTool(object):
    def __init__(self):
        self.status_server = MockStatusServer()

    def command_by_name(self, name):
        return CommitQueue


class SecondThoughtsCommitQueue(CommitQueue):
    def __init__(self):
        self._reject_patch = False
        CommitQueue.__init__(self)

    def run_command(self, command):
        # We want to reject the patch after the first validation,
        # so wait to reject it until after some other command has run.
        self._reject_patch = True
        return CommitQueue.run_command(self, command)

    def refetch_patch(self, patch):
        if not self._reject_patch:
            return self._tool.bugs.fetch_attachment(patch.id())

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
        return Attachment(attachment_dictionary, None)


class CommitQueueTest(QueuesTest):
    def _mock_test_result(self, testname):
        return test_results.TestResult(testname, [test_failures.FailureTextMismatch()])

    def test_commit_queue(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Processing patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK: update_status: commit-queue Cleaned working directory
MOCK: update_status: commit-queue Updated working directory
MOCK: update_status: commit-queue Applied patch
MOCK: update_status: commit-queue Built patch
MOCK: update_status: commit-queue Passed tests
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
MOCK: release_work_item: commit-queue 197
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting attachment 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        self.assert_queue_outputs(CommitQueue(), expected_stderr=expected_stderr)

    def test_commit_queue_failure(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Processing patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK: update_status: commit-queue Cleaned working directory
MOCK: update_status: commit-queue Updated working directory
MOCK: update_status: commit-queue Patch does not apply
MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting attachment 197 from commit-queue.' and additional comment 'MOCK script error'
MOCK: update_status: commit-queue Fail
MOCK: release_work_item: commit-queue 197
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting attachment 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        queue = CommitQueue()

        def mock_run_webkit_patch(command):
            if command == ['clean'] or command == ['update']:
                # We want cleaning to succeed so we can error out on a step
                # that causes the commit-queue to reject the patch.
                return
            raise ScriptError('MOCK script error')

        queue.run_webkit_patch = mock_run_webkit_patch
        self.assert_queue_outputs(queue, expected_stderr=expected_stderr)

    def test_rollout(self):
        tool = MockTool(log_executive=True)
        tool.filesystem.write_text_file('/mock/results.html', '')  # Otherwise the commit-queue will hit a KeyError trying to read the results from the MockFileSystem.
        tool.buildbot.light_tree_on_fire()
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Processing patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'clean']
MOCK: update_status: commit-queue Cleaned working directory
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'update']
MOCK: update_status: commit-queue Updated working directory
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'apply-attachment', '--no-update', '--non-interactive', 197]
MOCK: update_status: commit-queue Applied patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build', '--no-clean', '--no-update', '--build-style=both']
MOCK: update_status: commit-queue Built patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
MOCK: update_status: commit-queue Passed tests
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
MOCK: release_work_item: commit-queue 197
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '197' with comment 'Rejecting attachment 197 from commit-queue.' and additional comment 'Mock error message'\n",
            "handle_script_error": "ScriptError error message\n",
        }
        self.assert_queue_outputs(CommitQueue(), tool=tool, expected_stderr=expected_stderr)

    def test_rollout_lands(self):
        tool = MockTool(log_executive=True)
        tool.buildbot.light_tree_on_fire()
        rollout_patch = tool.bugs.fetch_attachment(106)  # _patch6, a rollout patch.
        assert(rollout_patch.is_rollout())
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("commit-queue", MockSCM.fake_checkout_root),
            "should_proceed_with_work_item": "MOCK: update_status: commit-queue Processing rollout patch\n",
            "next_work_item": "",
            "process_work_item": """MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'clean']
MOCK: update_status: commit-queue Cleaned working directory
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'update']
MOCK: update_status: commit-queue Updated working directory
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'apply-attachment', '--no-update', '--non-interactive', 106]
MOCK: update_status: commit-queue Applied patch
MOCK run_and_throw_if_fail: ['echo', '--status-host=example.com', 'land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 106]
MOCK: update_status: commit-queue Landed patch
MOCK: update_status: commit-queue Pass
MOCK: release_work_item: commit-queue 106
""",
            "handle_unexpected_error": "MOCK setting flag 'commit-queue' to '-' on attachment '106' with comment 'Rejecting attachment 106 from commit-queue.' and additional comment 'Mock error message'\n",
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
        queue._tool.filesystem.write_text_file('/mock/results.html', '')  # Otherwise the commit-queue will hit a KeyError trying to read the results from the MockFileSystem.
        queue._options = Mock()
        queue._options.port = None
        expected_stderr = """MOCK: update_status: commit-queue Cleaned working directory
MOCK: update_status: commit-queue Updated working directory
MOCK: update_status: commit-queue Applied patch
MOCK: update_status: commit-queue Built patch
MOCK: update_status: commit-queue Passed tests
MOCK: update_status: commit-queue Retry
MOCK: release_work_item: commit-queue 197
"""
        OutputCapture().assert_outputs(self, queue.process_work_item, [QueuesTest.mock_work_item], expected_stderr=expected_stderr)

    def test_report_flaky_tests(self):
        queue = CommitQueue()
        queue.bind_to_tool(MockTool())
        expected_stderr = """MOCK bug comment: bug_id=76, cc=None
--- Begin comment ---
The commit-queue just saw foo/bar.html flake (Text diff mismatch) while processing attachment 197 on bug 42.
Port: MockPort  Platform: MockPlatform 1.0
--- End comment ---

MOCK add_attachment_to_bug: bug_id=76, description=Failure diff from bot filename=failure.diff
MOCK bug comment: bug_id=76, cc=None
--- Begin comment ---
The commit-queue just saw bar/baz.html flake (Text diff mismatch) while processing attachment 197 on bug 42.
Port: MockPort  Platform: MockPlatform 1.0
--- End comment ---

MOCK add_attachment_to_bug: bug_id=76, description=Archive of layout-test-results from bot filename=layout-test-results.zip
MOCK bug comment: bug_id=42, cc=None
--- Begin comment ---
The commit-queue encountered the following flaky tests while processing attachment 197:

foo/bar.html bug 76 (author: abarth@webkit.org)
bar/baz.html bug 76 (author: abarth@webkit.org)
The commit-queue is continuing to process your patch.
--- End comment ---

"""
        test_names = ["foo/bar.html", "bar/baz.html"]
        test_results = [self._mock_test_result(name) for name in test_names]

        class MockZipFile(object):
            def __init__(self):
                self.fp = StringIO()

            def read(self, path):
                return ""

            def namelist(self):
                # This is intentionally missing one diffs.txt to exercise the "upload the whole zip" codepath.
                return ['foo/bar-diffs.txt']

        OutputCapture().assert_outputs(self, queue.report_flaky_tests, [QueuesTest.mock_work_item, test_results, MockZipFile()], expected_stderr=expected_stderr)

    def test_missing_layout_test_results(self):
        queue = CommitQueue()
        tool = MockTool()
        results_path = '/mock/results.html'
        tool.filesystem = MockFileSystem({results_path: None})
        queue.bind_to_tool(tool)
        # Make sure that our filesystem mock functions as we expect.
        self.assertRaises(IOError, tool.filesystem.read_text_file, results_path)
        # layout_test_results shouldn't raise even if the results.html file is missing.
        self.assertEquals(queue.layout_test_results(), None)

    def test_layout_test_results(self):
        queue = CommitQueue()
        queue.bind_to_tool(MockTool())
        queue._read_file_contents = lambda path: None
        self.assertEquals(queue.layout_test_results(), None)
        queue._read_file_contents = lambda path: ""
        self.assertEquals(queue.layout_test_results(), None)
        queue._create_layout_test_results = lambda: LayoutTestResults([])
        results = queue.layout_test_results()
        self.assertNotEquals(results, None)
        self.assertEquals(results.failure_limit_count(), 10)  # This value matches RunTests.NON_INTERACTIVE_FAILURE_LIMIT_COUNT

    def test_archive_last_layout_test_results(self):
        queue = CommitQueue()
        queue.bind_to_tool(MockTool())
        patch = queue._tool.bugs.fetch_attachment(128)
        # This is just to test that the method doesn't raise.
        queue.archive_last_layout_test_results(patch)

    def test_upload_results_archive_for_patch(self):
        queue = CommitQueue()
        queue.bind_to_tool(MockTool())
        patch = queue._tool.bugs.fetch_attachment(128)
        expected_stderr = """MOCK add_attachment_to_bug: bug_id=42, description=Archive of layout-test-results from bot filename=layout-test-results.zip
-- Begin comment --
The attached test failures were seen while running run-webkit-tests on the commit-queue.
Port: MockPort  Platform: MockPlatform 1.0
-- End comment --
"""
        OutputCapture().assert_outputs(self, queue._upload_results_archive_for_patch, [patch, Mock()], expected_stderr=expected_stderr)


class StyleQueueTest(QueuesTest):
    def test_style_queue(self):
        expected_stderr = {
            "begin_work_queue": self._default_begin_work_queue_stderr("style-queue", MockSCM.fake_checkout_root),
            "next_work_item": "",
            "should_proceed_with_work_item": "MOCK: update_status: style-queue Checking style\n",
            "process_work_item": "MOCK: update_status: style-queue Pass\nMOCK: release_work_item: style-queue 197\n",
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "MOCK: update_status: style-queue ScriptError error message\nMOCK bug comment: bug_id=42, cc=[]\n--- Begin comment ---\nAttachment 197 did not pass style-queue:\n\nScriptError error message\n\nIf any of these errors are false positives, please file a bug against check-webkit-style.\n--- End comment ---\n\n",
        }
        expected_exceptions = {
            "handle_script_error": SystemExit,
        }
        self.assert_queue_outputs(StyleQueue(), expected_stderr=expected_stderr, expected_exceptions=expected_exceptions)
