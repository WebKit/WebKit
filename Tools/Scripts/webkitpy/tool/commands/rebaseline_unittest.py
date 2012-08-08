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
from webkitpy.common.net.buildbot.buildbot_mock import MockBuilder
from webkitpy.common.system.executive_mock import MockExecutive


class TestRebaseline(unittest.TestCase):
    def test_tests_to_update(self):
        command = Rebaseline()
        command.bind_to_tool(MockTool())
        build = Mock()
        OutputCapture().assert_outputs(self, command._tests_to_update, [build])

    def test_baseline_directory(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)
        self.assertEqual(command._baseline_directory("Apple Win XP Debug (Tests)"), "/mock-checkout/LayoutTests/platform/win-xp")
        self.assertEqual(command._baseline_directory("Apple Win 7 Release (Tests)"), "/mock-checkout/LayoutTests/platform/win")
        self.assertEqual(command._baseline_directory("Apple Lion Release WK1 (Tests)"), "/mock-checkout/LayoutTests/platform/mac")
        self.assertEqual(command._baseline_directory("Apple Lion Release WK2 (Tests)"), "/mock-checkout/LayoutTests/platform/mac-wk2")
        self.assertEqual(command._baseline_directory("GTK Linux 32-bit Release"), "/mock-checkout/LayoutTests/platform/gtk")
        self.assertEqual(command._baseline_directory("EFL Linux 64-bit Debug"), "/mock-checkout/LayoutTests/platform/efl")
        self.assertEqual(command._baseline_directory("Qt Linux Release"), "/mock-checkout/LayoutTests/platform/qt")
        self.assertEqual(command._baseline_directory("Webkit Mac10.7"), "/mock-checkout/LayoutTests/platform/chromium-mac")
        self.assertEqual(command._baseline_directory("Webkit Mac10.6"), "/mock-checkout/LayoutTests/platform/chromium-mac-snowleopard")

    def test_rebaseline_updates_expectations_file_noop(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        # FIXME: work around the chromium skia expectations file to avoid getting a bunch of confusing warnings.
        tool.filesystem.write_text_file(lion_port.path_from_chromium_base('skia', 'skia_test_expectations.txt'), '')
        for path in lion_port.expectations_files():
            tool.filesystem.write_text_file(path, '')
        tool.filesystem.write_text_file(lion_port.path_to_test_expectations_file(), """BUGB MAC LINUX XP DEBUG : fast/dom/Window/window-postmessage-clone-really-deep-array.html = PASS
BUGA DEBUG : fast/css/large-list-of-rules-crash.html = TEXT
""")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "fast/dom/Window/window-postmessage-clone-really-deep-array.html"), "Dummy test contents")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "fast/css/large-list-of-rules-crash.html"), "Dummy test contents")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test.html"), "Dummy test contents")

        expected_logs = """Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.png.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.wav.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test_and_update_expectations, ["Webkit Mac10.7", "userscripts/another-test.html", None], expected_logs=expected_logs)

        new_expectations = tool.filesystem.read_text_file(lion_port.path_to_test_expectations_file())
        self.assertEqual(new_expectations, """BUGB MAC LINUX XP DEBUG : fast/dom/Window/window-postmessage-clone-really-deep-array.html = PASS
BUGA DEBUG : fast/css/large-list-of-rules-crash.html = TEXT
""")

    def test_rebaseline_updates_expectations_file(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(lion_port.path_from_chromium_base('skia', 'skia_test_expectations.txt'), '')
        tool.filesystem.write_text_file(lion_port.path_to_test_expectations_file(), "BUGX MAC : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test.html"), "Dummy test contents")

        expected_logs = """Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.png.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.wav.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test_and_update_expectations, ["Webkit Mac10.7", "userscripts/another-test.html", None], expected_logs=expected_logs)

        new_expectations = tool.filesystem.read_text_file(lion_port.path_to_test_expectations_file())
        self.assertEqual(new_expectations, "BUGX SNOWLEOPARD : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")

    def test_rebaseline_does_not_include_overrides(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(lion_port.path_from_chromium_base('skia', 'skia_test_expectations.txt'), '')
        tool.filesystem.write_text_file(lion_port.path_to_test_expectations_file(), "BUGX MAC : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")
        tool.filesystem.write_text_file(lion_port.path_from_chromium_base('skia', 'skia_test_expectations.txt'), "BUGY MAC : other-test.html = TEXT\n")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test.html"), "Dummy test contents")

        expected_logs = """Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.png.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.wav.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test_and_update_expectations, ["Webkit Mac10.7", "userscripts/another-test.html", None], expected_logs=expected_logs)

        new_expectations = tool.filesystem.read_text_file(lion_port.path_to_test_expectations_file())
        self.assertEqual(new_expectations, "BUGX SNOWLEOPARD : userscripts/another-test.html = IMAGE\nBUGZ LINUX : userscripts/another-test.html = IMAGE\n")

    def test_rebaseline_test(self):
        command = RebaselineTest()
        command.bind_to_tool(MockTool())
        expected_logs = "Retrieving http://example.com/f/builders/Webkit Linux/results/layout-test-results/userscripts/another-test-actual.txt.\n"
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Linux", "userscripts/another-test.html", None, "txt"], expected_logs=expected_logs)

    def test_rebaseline_test_and_print_scm_changes(self):
        command = RebaselineTest()
        command.bind_to_tool(MockTool())
        expected_logs = "Retrieving http://example.com/f/builders/Webkit Linux/results/layout-test-results/userscripts/another-test-actual.txt.\n"
        command._print_scm_changes = True
        command._scm_changes = {'add': [], 'delete': []}
        command._tool._scm.exists = lambda x: False
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Linux", "userscripts/another-test.html", None, "txt"], expected_logs=expected_logs)
        self.assertEquals(command._scm_changes, {'add': ['/mock-checkout/LayoutTests/platform/chromium-linux/userscripts/another-test-expected.txt'], 'delete': []})

    def test_rebaseline_and_copy_test(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.layout_tests_dir(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_logs = """Copying baseline from /mock-checkout/LayoutTests/userscripts/another-test-expected.txt to /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_logs=expected_logs)

    def test_rebaseline_and_copy_test_no_existing_result(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        expected_logs = """No existing baseline for userscripts/another-test.html.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_logs=expected_logs)

    def test_rebaseline_and_copy_test_with_lion_result(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_logs = """Copying baseline from /mock-checkout/LayoutTests/platform/chromium-mac/userscripts/another-test-expected.txt to /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_logs=expected_logs)

    def test_rebaseline_and_copy_no_overwrite_test(self):
        command = RebaselineTest()
        tool = MockTool()
        command.bind_to_tool(tool)

        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(os.path.join(lion_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        snowleopard_port = tool.port_factory.get_from_builder_name("Webkit Mac10.6")
        tool.filesystem.write_text_file(os.path.join(snowleopard_port.baseline_path(), "userscripts/another-test-expected.txt"), "Dummy expected result")

        expected_logs = """Existing baseline at /mock-checkout/LayoutTests/platform/chromium-mac-snowleopard/userscripts/another-test-expected.txt, not copying over it.
Retrieving http://example.com/f/builders/Webkit Mac10.7/results/layout-test-results/userscripts/another-test-actual.txt.
"""
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Mac10.7", "userscripts/another-test.html", ["chromium-mac-snowleopard"], "txt"], expected_logs=expected_logs)

    def test_rebaseline_all(self):
        old_exact_matches = builders._exact_matches
        builders._exact_matches = {
            "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
            "MOCK builder (Debug)": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier", "debug"])},
        }

        command = RebaselineJson()
        tool = MockTool()
        options = MockOptions()
        options.optimize = True
        command.bind_to_tool(tool)
        tool.executive = MockExecutive(should_log=True)

        expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'user-scripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt,png', 'user-scripts/another-test.html'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, command._rebaseline, [options, {"user-scripts/another-test.html":{"MOCK builder": ["txt", "png"]}}], expected_stderr=expected_stderr)

        expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder (Debug)', '--test', 'user-scripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt,png', 'user-scripts/another-test.html'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, command._rebaseline, [options, {"user-scripts/another-test.html":{"MOCK builder (Debug)": ["txt", "png"]}}], expected_stderr=expected_stderr)

        expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK builder', '--test', 'user-scripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt', 'user-scripts/another-test.html'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, command._rebaseline, [options, {"user-scripts/another-test.html":{"MOCK builder (Debug)": ["txt", "png"], "MOCK builder": ["txt"]}}], expected_stderr=expected_stderr)

        builders._exact_matches = old_exact_matches

    def test_rebaseline_expectations(self):
        command = RebaselineExpectations()
        tool = MockTool()
        command.bind_to_tool(tool)

        # FIXME: work around the chromium skia expectations file to avoid getting a bunch of confusing warnings.
        lion_port = tool.port_factory.get_from_builder_name("Webkit Mac10.7")
        tool.filesystem.write_text_file(lion_port.path_from_chromium_base('skia', 'skia_test_expectations.txt'), '')
        for port_name in tool.port_factory.all_port_names():
            port = tool.port_factory.get(port_name)
            for path in port.expectations_files():
                tool.filesystem.write_text_file(path, '')

        # Don't enable logging until after we create the mock expectation files as some Port.__init__'s run subcommands.
        tool.executive = MockExecutive(should_log=True)

        def run_in_parallel(commands):
            print commands
            return ""

        tool.executive.run_in_parallel = run_in_parallel

        expected_logs = """Retrieving results for chromium-linux-x86 from Webkit Linux 32.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for chromium-linux-x86_64 from Webkit Linux.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for chromium-mac-lion from Webkit Mac10.7.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for chromium-mac-snowleopard from Webkit Mac10.6.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for chromium-win-win7 from Webkit Win7.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for chromium-win-xp from Webkit Win.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for efl from EFL Linux 64-bit Release.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for gtk from GTK Linux 64-bit Release.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for mac-lion from Apple Lion Release WK1 (Tests).
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for qt-linux from Qt Linux Release.
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
Retrieving results for win-7sp0 from Apple Win 7 Release (Tests).
    userscripts/another-test.html (txt)
    userscripts/images.svg (png)
"""

        expected_stdout = """[(['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Linux 32', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Linux', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Mac10.6', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Mac10.7', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Win7', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Apple Win 7 Release (Tests)', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'EFL Linux 64-bit Release', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Webkit Win', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'GTK Linux 64-bit Release', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Qt Linux Release', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'Apple Lion Release WK1 (Tests)', '--test', 'userscripts/another-test.html'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Linux 32', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Linux', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Mac10.6', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Mac10.7', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Win7', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Apple Win 7 Release (Tests)', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'EFL Linux 64-bit Release', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Webkit Win', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'GTK Linux 64-bit Release', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Qt Linux Release', '--test', 'userscripts/images.svg'], '/mock-checkout'), (['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'Apple Lion Release WK1 (Tests)', '--test', 'userscripts/images.svg'], '/mock-checkout')]
"""

        expected_stderr = """MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
"""

        command._tests_to_rebaseline = lambda port: {'userscripts/another-test.html': set(['txt']), 'userscripts/images.svg': set(['png'])}
        OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=False), [], tool], expected_logs=expected_logs, expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        expected_stderr_with_optimize = """MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'png', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
MOCK run_command: ['qmake', '-v'], cwd=None
"""

        command._tests_to_rebaseline = lambda port: {'userscripts/another-test.html': set(['txt']), 'userscripts/images.svg': set(['png'])}
        OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True), [], tool], expected_logs=expected_logs, expected_stdout=expected_stdout, expected_stderr=expected_stderr_with_optimize)

    def test_overrides_are_included_correctly(self):
        command = RebaselineExpectations()
        tool = MockTool()
        command.bind_to_tool(tool)
        port = tool.port_factory.get('chromium-mac-lion')

        # This tests that the any tests marked as REBASELINE in the overrides are found, but
        # that the overrides do not get written into the main file.
        expectations_path = port.expectations_files()[0]
        expectations_contents = ''
        port._filesystem.write_text_file(expectations_path, expectations_contents)
        port.expectations_dict = lambda: {
            expectations_path: expectations_contents,
            'overrides': ('BUGX REBASELINE : userscripts/another-test.html = TEXT\n'
                          'BUGY : userscripts/test.html = CRASH\n')}

        for path in port.expectations_files():
            port._filesystem.write_text_file(path, '')
        port._filesystem.write_text_file(port.layout_tests_dir() + '/userscripts/another-test.html', '')
        self.assertEquals(command._tests_to_rebaseline(port), {'userscripts/another-test.html': set(['txt'])})
        self.assertEquals(port._filesystem.read_text_file(expectations_path), expectations_contents)

    def test_rebaseline(self):
        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
            }

            command = Rebaseline()
            tool = MockTool()
            command.bind_to_tool(tool)

            for port_name in tool.port_factory.all_port_names():
                port = tool.port_factory.get(port_name)
                for path in port.expectations_files():
                    tool.filesystem.write_text_file(path, '')

            tool.executive = MockExecutive(should_log=True)

            def mock_builders_to_pull_from():
                return [MockBuilder('MOCK builder')]

            def mock_tests_to_update(build):
                return ['mock/path/to/test.html']

            command._builders_to_pull_from = mock_builders_to_pull_from
            command._tests_to_update = mock_tests_to_update

            expected_stdout = """rebaseline-json: {'mock/path/to/test.html': {'MOCK builder': ['txt']}}
"""

            expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK builder', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt', 'mock/path/to/test.html'], cwd=/mock-checkout
"""

            OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True, builders=None, suffixes=["txt"], verbose=True), [], tool], expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        finally:
            builders._exact_matches = old_exact_matches

    def test_rebaseline_command_line_flags(self):
        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
            }

            command = Rebaseline()
            tool = MockTool()
            command.bind_to_tool(tool)

            for port_name in tool.port_factory.all_port_names():
                port = tool.port_factory.get(port_name)
                for path in port.expectations_files():
                    tool.filesystem.write_text_file(path, '')

            tool.executive = MockExecutive(should_log=True)

            expected_stdout = """rebaseline-json: {'mock/path/to/test.html': {'MOCK builder': ['txt']}}
"""

            expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK builder', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt', 'mock/path/to/test.html'], cwd=/mock-checkout
"""

            builder = "MOCK builder"
            test = "mock/path/to/test.html"
            OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True, builders=[builder], suffixes=["txt"], verbose=True), [test], tool], expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        finally:
            builders._exact_matches = old_exact_matches

    def test_rebaseline_multiple_builders(self):
        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK builder2": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier2"])},
            }

            command = Rebaseline()
            tool = MockTool()
            command.bind_to_tool(tool)

            for port_name in tool.port_factory.all_port_names():
                port = tool.port_factory.get(port_name)
                for path in port.expectations_files():
                    tool.filesystem.write_text_file(path, '')

            tool.executive = MockExecutive(should_log=True)

            def mock_builders_to_pull_from():
                return [MockBuilder('MOCK builder'), MockBuilder('MOCK builder2')]

            def mock_tests_to_update(build):
                return ['mock/path/to/test.html']

            command._builders_to_pull_from = mock_builders_to_pull_from
            command._tests_to_update = mock_tests_to_update

            expected_stdout = """rebaseline-json: {'mock/path/to/test.html': {'MOCK builder2': ['txt'], 'MOCK builder': ['txt']}}
"""

            expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK builder2', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK builder', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt', 'mock/path/to/test.html'], cwd=/mock-checkout
"""

            OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True, builders=None, suffixes=["txt"], verbose=True), [], tool], expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        finally:
            builders._exact_matches = old_exact_matches

    def test_rebaseline_multiple_builders_and_tests_command_line(self):
        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK builder2": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier2"])},
                "MOCK builder3": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier2"])},
            }

            command = Rebaseline()
            tool = MockTool()
            command.bind_to_tool(tool)

            for port_name in tool.port_factory.all_port_names():
                port = tool.port_factory.get(port_name)
                for path in port.expectations_files():
                    tool.filesystem.write_text_file(path, '')

            tool.executive = MockExecutive(should_log=True)

            expected_stdout = """rebaseline-json: {'mock/path/to/test.html': {'MOCK builder2': ['wav', 'txt', 'png'], 'MOCK builder': ['wav', 'txt', 'png'], 'MOCK builder3': ['wav', 'txt', 'png']}, 'mock/path/to/test2.html': {'MOCK builder2': ['wav', 'txt', 'png'], 'MOCK builder': ['wav', 'txt', 'png'], 'MOCK builder3': ['wav', 'txt', 'png']}}
"""

            expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'wav,txt,png', '--builder', 'MOCK builder2', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'wav,txt,png', '--builder', 'MOCK builder', '--test', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'wav,txt,png', '--builder', 'MOCK builder2', '--test', 'mock/path/to/test2.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test-internal', '--suffixes', 'wav,txt,png', '--builder', 'MOCK builder', '--test', 'mock/path/to/test2.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'wav,txt,png', 'mock/path/to/test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'wav,txt,png', 'mock/path/to/test2.html'], cwd=/mock-checkout
"""

            OutputCapture().assert_outputs(self, command.execute, [MockOptions(optimize=True, builders=["MOCK builder,MOCK builder2", "MOCK builder3"], suffixes=["txt,png", "png,wav,txt"], verbose=True), ["mock/path/to/test.html", "mock/path/to/test2.html"], tool], expected_stdout=expected_stdout, expected_stderr=expected_stderr)

        finally:
            builders._exact_matches = old_exact_matches
