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

from webkitpy.thirdparty.mock import Mock
from webkitpy.common.host import Host
from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_failures
from webkitpy.port.factory import PortFactory
from webkitpy.tool.bot.queueengine import QueueEngine
from webkitpy.tool.commands.earlywarningsystem import *
from webkitpy.tool.commands.queues import PatchProcessingQueue
from webkitpy.tool.commands.queuestest import QueuesTest
from webkitpy.tool.mocktool import MockTool, MockOptions


class AbstractEarlyWarningSystemTest(QueuesTest):
    def test_failing_tests_message(self):
        # Needed to define port_name, used in AbstractEarlyWarningSystem.__init__
        class TestEWS(AbstractEarlyWarningSystem):
            port_name = "win"  # Needs to be a port which port/factory understands.
            _build_style = None

        ews = TestEWS()
        ews.bind_to_tool(MockTool())
        ews.host = MockHost()
        ews._options = MockOptions(port=None, confirm=False)
        OutputCapture().assert_outputs(self, ews.begin_work_queue, expected_logs=self._default_begin_work_queue_logs(ews.name))
        task = Mock()
        task.results_from_patch_test_run = lambda a: LayoutTestResults([test_results.TestResult("foo.html", failures=[test_failures.FailureTextMismatch()]),
                                                                          test_results.TestResult("bar.html", failures=[test_failures.FailureTextMismatch()])],
                                                                          did_exceed_test_failure_limit=False)
        patch = ews._tool.bugs.fetch_attachment(10000)
        self.assertMultiLineEqual(ews._failing_tests_message(task, patch), "New failing tests:\nfoo.html\nbar.html")


class EarlyWarningSystemTest(QueuesTest):
    def _default_expected_logs(self, ews):
        string_replacements = {
            "name": ews.name,
            "port": ews.port_name,
            "architecture": " --architecture=%s" % ews.architecture if ews.architecture else "",
            "build_style": ews.build_style(),
        }
        if ews.run_tests:
            run_tests_line = "Running: webkit-patch --status-host=example.com build-and-test --no-clean --no-update --test --non-interactive --build-style=%(build_style)s --port=%(port)s%(architecture)s\n" % string_replacements
        else:
            run_tests_line = ""
        string_replacements['run_tests_line'] = run_tests_line

        expected_logs = {
            "begin_work_queue": self._default_begin_work_queue_logs(ews.name),
            "process_work_item": """MOCK: update_status: %(name)s Started processing patch
Running: webkit-patch --status-host=example.com clean --port=%(port)s%(architecture)s
Running: webkit-patch --status-host=example.com update --port=%(port)s%(architecture)s
Running: webkit-patch --status-host=example.com apply-attachment --no-update --non-interactive 10000 --port=%(port)s%(architecture)s
Running: webkit-patch --status-host=example.com build --no-clean --no-update --build-style=%(build_style)s --port=%(port)s%(architecture)s
%(run_tests_line)sMOCK: update_status: %(name)s Pass
MOCK: release_work_item: %(name)s 10000
""" % string_replacements,
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "ScriptError error message\n\nMOCK output\n",
        }
        return expected_logs

    def _test_ews(self, ews):
        ews.bind_to_tool(MockTool())
        ews.host = MockHost()
        options = Mock()
        options.port = None
        options.run_tests = ews.run_tests
        self.assert_queue_outputs(ews, expected_logs=self._default_expected_logs(ews), options=options)

    def test_ewses(self):
        classes = AbstractEarlyWarningSystem.load_ews_classes()
        self.assertTrue(classes)
        self.maxDiff = None
        for ews_class in classes:
            self._test_ews(ews_class())
