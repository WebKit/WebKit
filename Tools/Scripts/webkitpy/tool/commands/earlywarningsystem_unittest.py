# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2017 Apple Inc. All rights reserved.
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
from webkitpy.common.net.jsctestresults import JSCTestResults
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


# Needed to define port_name, used in AbstractEarlyWarningSystem.__init__
class TestEWS(AbstractEarlyWarningSystem):
    port_name = "win"  # Needs to be a port which port/factory understands.
    _build_style = None
    _group = None


class TestJSCEWS(AbstractEarlyWarningSystem):
    port_name = "mac"  # Needs to be a port which port/factory understands.
    _build_style = None
    _group = "jsc"


class AbstractEarlyWarningSystemTest(QueuesTest):
    def _test_message(self, ews, results, message):
        ews.bind_to_tool(MockTool())
        ews.host = MockHost()
        ews._options = MockOptions(port=None, confirm=False)
        OutputCapture().assert_outputs(self, ews.begin_work_queue, expected_logs=self._default_begin_work_queue_logs(ews.name))
        task = Mock()
        task.results_from_patch_test_run = results
        patch = ews._tool.bugs.fetch_attachment(10000)
        self.assertMultiLineEqual(ews._failing_tests_message(task, patch), message)

    def test_failing_tests_message(self):
        ews = TestEWS()
        results = lambda a: LayoutTestResults([test_results.TestResult("foo.html", failures=[test_failures.FailureTextMismatch()]),
                                                test_results.TestResult("bar.html", failures=[test_failures.FailureTextMismatch()])],
                                                did_exceed_test_failure_limit=False)
        message = "New failing tests:\nfoo.html\nbar.html"
        self._test_message(ews, results, message)

    def test_failing_jsc_tests_message(self):
        ews = TestJSCEWS()
        results = lambda a: JSCTestResults(False, ["es6.yaml/es6/typed_arrays_Int8Array.js.default", "es6.yaml/es6/typed_arrays_Uint8Array.js.default"])
        message = "New failing tests:\nes6.yaml/es6/typed_arrays_Int8Array.js.default\nes6.yaml/es6/typed_arrays_Uint8Array.js.default\napiTests"
        self._test_message(ews, results, message)


class MockEarlyWarningSystemTaskForInconclusiveJSCResults(EarlyWarningSystemTask):
    def _test_patch(self):
        self._test()
        results = self._delegate.test_results()
        return bool(results)


class MockAbstractEarlyWarningSystemForInconclusiveJSCResults(AbstractEarlyWarningSystem):
    def _create_task(self, patch):
        task = MockEarlyWarningSystemTaskForInconclusiveJSCResults(self, patch, self._options.run_tests)
        return task


class EarlyWarningSystemTest(QueuesTest):
    def _default_expected_logs(self, ews, conclusive):
        string_replacements = {
            "name": ews.name,
            "port": ews.port_name,
            "architecture": " --architecture=%s" % ews.architecture if ews.architecture else "",
            "build_style": ews.build_style(),
            "group": ews.group(),
        }

        if ews.should_build:
            build_line = "Running: webkit-patch build --no-clean --no-update --build-style=%(build_style)s --group=%(group)s --port=%(port)s%(architecture)s\n" % string_replacements
        else:
            build_line = ""
        string_replacements['build_line'] = build_line

        if ews.run_tests:
            run_tests_line = "Running: webkit-patch build-and-test --no-clean --no-update --test --non-interactive --build-style=%(build_style)s --group=%(group)s --port=%(port)s%(architecture)s\n" % string_replacements
        else:
            run_tests_line = ""
        string_replacements['run_tests_line'] = run_tests_line

        string_replacements['result_lines'] = ""

        expected_logs = {
            "begin_work_queue": self._default_begin_work_queue_logs(ews.name),
            "process_work_item": """Running: webkit-patch clean --port=%(port)s%(architecture)s
Running: webkit-patch update --port=%(port)s%(architecture)s
Running: webkit-patch apply-attachment --no-update --non-interactive 10000 --port=%(port)s%(architecture)s
Running: webkit-patch check-patch-relevance --quiet --group=%(group)s --port=%(port)s%(architecture)s
%(build_line)s%(run_tests_line)s%(result_lines)s""" % string_replacements,
            "handle_unexpected_error": "Mock error message\n",
            "handle_script_error": "ScriptError error message\n\nMOCK output\n",
        }
        return expected_logs

    def _test_ews(self, ews, results_are_conclusive=True):
        ews.bind_to_tool(MockTool())
        ews.host = MockHost()
        options = Mock()
        options.port = None
        options.run_tests = ews.run_tests
        self.assert_queue_outputs(ews, expected_logs=self._default_expected_logs(ews, results_are_conclusive), options=options)

    def test_ewses(self):
        classes = AbstractEarlyWarningSystem.load_ews_classes()
        self.assertTrue(classes)
        self.maxDiff = None
        for ews_class in classes:
            self._test_ews(ews_class())

    def test_inconclusive_jsc_test_results(self):
        classes = MockAbstractEarlyWarningSystemForInconclusiveJSCResults.load_ews_classes()
        self.assertTrue(classes)
        self.maxDiff = None
        test_classes = filter(lambda x: x.run_tests and x.group == "jsc", classes)
        for ews_class in test_classes:
            self._test_ews(ews_class(), False)

    def test_ews_name(self):
        # These are the names EWS's infrastructure expects, check that they work
        expected_names = {
            'gtk-wk2-ews',
            'ios-ews',
            'ios-sim-ews',
            'jsc-ews',
            'jsc-i386-ews',
            'jsc-mips-ews',
            'jsc-armv7-ews',
            'mac-debug-ews',
            'mac-ews',
            'mac-wk2-ews',
            'win-ews',
            'wpe-ews',
            'wincairo-ews',
        }
        classes = AbstractEarlyWarningSystem.load_ews_classes()
        names = {cls.name for cls in classes}
        missing_names = expected_names - names
        unexpected_names = names - expected_names
        if missing_names:
            raise AssertionError("'{}' is not a valid EWS command but is used by EWS's infrastructure".format(missing_names.pop()))
        if unexpected_names:
            raise AssertionError("'{}' is a valid EWS command but is not used by EWS's infrastructure".format(unexpected_names.pop()))
