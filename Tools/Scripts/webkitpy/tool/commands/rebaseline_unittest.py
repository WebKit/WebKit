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

import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.rebaseline import *
from webkitpy.tool.mocktool import MockTool, MockOptions
from webkitpy.common.system.executive_mock import MockExecutive


class TestRebaseline(unittest.TestCase):
    def test_tests_to_update(self):
        command = Rebaseline()
        command.bind_to_tool(MockTool())
        build = Mock()
        OutputCapture().assert_outputs(self, command._tests_to_update, [build])

    def test_rebaseline_updates_expectations_file_noop(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(lion_port.path_to_test_expectations_file(), """BUGB MAC LINUX XP DEBUG : fast/dom/Window/window-postmessage-clone-really-deep-array.html = PASS
BUGA DEBUG : fast/css/large-list-of-rules-crash.html = TEXT
""")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "fast/dom/Window/window-postmessage-clone-really-deep-array.html"), "Dummy test contents")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "fast/css/large-list-of-rules-crash.html"), "Dummy test contents")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test.html"), "Dummy test contents")

        expected_stdout = """Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.png.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.wav.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test_and_update_expectations, ["Webkit Mac10.7", "userscripts/another-test.html", None], expected_stdout=expected_stdout)

        new_expectations = tool.filesystem.read_text_file(lion_port.path_to_test_expectations_file())
        self.assertEqual(new_expectations, """BUGB MAC LINUX XP DEBUG : fast/dom/Window/window-postmessage-clone-really-deep-array.html = PASS
BUGA DEBUG : fast/css/large-list-of-rules-crash.html = TEXT
""")

    def test_rebaseline_updates_expectations_file(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(lion_port.path_to_test_expectations_file(), "BUGX MAC : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test.html"), "Dummy test contents")

        expected_stdout = """Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.png.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.wav.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test_and_update_expectations, ["Webkit Mac10.7", "userscripts/another-test.html", None], expected_stdout=expected_stdout)

        new_expectations = tool.filesystem.read_text_file(lion_port.path_to_test_expectations_file())
        self.assertEqual(new_expectations, "BUGX LEOPARD SNOWLEOPARD : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")

    def test_rebaseline_test(self):
        command = RebaselineTest()
        command.bind_to_tool(MockTool())
        expected_stdout = "Retrieving http://example.com/f/builders/Webkit Linux/results/layout-test-results/userscripts/another-test-actual.txt.\n"
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Linux", "userscripts/another-test.html", None, "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_and_copy_test(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_stdout = """Copying baseline from /mock-checkout/LayoutTests/userscripts/another-test-expected.txt to /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_and_copy_test_no_existing_result(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        expected_stdout = """No existing baseline for userscripts/another-test.html.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_and_copy_test_with_lion_result(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_stdout = """Copying baseline from /mock-checkout/LayoutTests/platform/chromium-mac/userscripts/another-test-expected.txt to /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt.
Copying baseline from /mock-checkout/LayoutTests/platform/chromium-mac/userscripts/another-test-expected.txt to /mock-checkout/LayoutTests/platform/chromium-mac-leopard/userscripts/another-test-expected.txt.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard", "chromium-mac-leopard"], "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_and_copy_no_overwrite_test(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        snowleopard_port = tool.port_factory.get_from_builder_name("Webkit Mac10.6")
        tool.filesystem.write_text_file(os.path.join(snowleopard_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_stdout = """Existing baseline at /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt, not copying over it.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_expectations(self):
        command = RebaselineExpectations()
        tool = MockTool()
        command.bind_to_tool(tool)

        for port_name in tool.port_factory.all_port_names():
            port = tool.port_factory.get(port_name)
            tool.filesystem.write_text_file(port.path_to_test_expectations_file(), '')

        # Don't enable logging until after we create the mock expectation files as some Port.__init__'s run subcommands.
        tool.executive = MockExecutive(should_log=True)

        expected_stdout = """Retrieving results for chromium-linux-x86 from Webkit Linux 32.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-linux-x86_64 from Webkit Linux.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-mac-leopard from Webkit Mac10.5.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-mac-lion from Webkit Mac10.7.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-mac-snowleopard from Webkit Mac10.6.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-vista from Webkit Vista.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-win7 from Webkit Win7.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-xp from Webkit Win.
    userscripts/another-test.html
    userscripts/images.svg
"""

        expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux 32', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux 32', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.5', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.5', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.7', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.7', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.6', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.6', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Vista', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Vista', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win7', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win7', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win', 'userscripts/images.svg'], cwd=/mock-checkout
"""

        command._tests_to_rebaseline = lambda port: ['userscripts/another-test.html', 'userscripts/images.svg']
        OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=False), [], tool], expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        expected_stdout_with_optimize = expected_stdout + (
            "Optimizing baselines for userscripts/another-test.html.\n"
            "Optimizing baselines for userscripts/images.svg.\n")
        expected_stderr_with_optimize = expected_stderr + (
            "MOCK run_command: ['echo', 'optimize-baselines', 'userscripts/another-test.html'], cwd=/mock-checkout\n"
            "MOCK run_command: ['echo', 'optimize-baselines', 'userscripts/images.svg'], cwd=/mock-checkout\n")

        command._tests_to_rebaseline = lambda port: ['userscripts/another-test.html', 'userscripts/images.svg']
        OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True), [], tool], expected_stdout=expected_stdout_with_optimize, expected_stderr=expected_stderr_with_optimize)
