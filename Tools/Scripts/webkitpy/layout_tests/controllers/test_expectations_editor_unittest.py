#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.controllers.test_expectations_editor import *
from webkitpy.layout_tests.models.test_configuration import *
from webkitpy.layout_tests.models.test_expectations import *
from webkitpy.layout_tests.models.test_configuration import *


class MockBugManager(object):
    def close_bug(self, bug_id, reference_bug_id=None):
        pass

    def create_bug(self):
        return "BUG_NEWLY_CREATED"


class TestExpectationEditorTests(unittest.TestCase):
    WIN_RELEASE_CPU_CONFIGS = set([
        TestConfiguration('vista', 'x86', 'release', 'cpu'),
        TestConfiguration('win7', 'x86', 'release', 'cpu'),
        TestConfiguration('xp', 'x86', 'release', 'cpu'),
    ])

    RELEASE_CONFIGS = set([
        TestConfiguration('vista', 'x86', 'release', 'cpu'),
        TestConfiguration('win7', 'x86', 'release', 'cpu'),
        TestConfiguration('xp', 'x86', 'release', 'cpu'),
        TestConfiguration('vista', 'x86', 'release', 'gpu'),
        TestConfiguration('win7', 'x86', 'release', 'gpu'),
        TestConfiguration('xp', 'x86', 'release', 'gpu'),
        TestConfiguration('snowleopard', 'x86', 'release', 'cpu'),
        TestConfiguration('leopard', 'x86', 'release', 'cpu'),
        TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
        TestConfiguration('leopard', 'x86', 'release', 'gpu'),
        TestConfiguration('lucid', 'x86', 'release', 'cpu'),
        TestConfiguration('lucid', 'x86_64', 'release', 'cpu'),
        TestConfiguration('lucid', 'x86', 'release', 'gpu'),
        TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
    ])

    def __init__(self, testFunc):
        host = MockHost()
        self.test_port = host.port_factory.get('test-win-xp', None)
        self.full_test_list = ['failures/expected/keyboard.html', 'failures/expected/audio.html']
        unittest.TestCase.__init__(self, testFunc)

    def make_parsed_expectation_lines(self, in_string):
        parser = TestExpectationParser(self.test_port, self.full_test_list, allow_rebaseline_modifier=False)
        expectation_lines = parser.parse(in_string)
        for expectation_line in expectation_lines:
            self.assertFalse(expectation_line.is_invalid())
        return expectation_lines

    def assert_remove_roundtrip(self, in_string, test, expected_string, remove_flakes=False):
        test_config_set = set([self.test_port.test_configuration()])
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        editor.remove_expectation(test, test_config_set, remove_flakes)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        result = TestExpectationSerializer.list_to_string(expectation_lines, converter)
        self.assertEquals(result, expected_string)

    def assert_update_roundtrip(self, in_string, test, expectation_set, expected_string, expected_update_count, remove_flakes=False, parsed_bug_modifiers=None, test_configs=None):
        test_config_set = test_configs or set([self.test_port.test_configuration()])
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        updated_expectation_lines = editor.update_expectation(test, test_config_set, expectation_set, parsed_bug_modifiers=parsed_bug_modifiers)
        for updated_expectation_line in updated_expectation_lines:
            self.assertTrue(updated_expectation_line in expectation_lines)
        self.assertEquals(len(updated_expectation_lines), expected_update_count)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        result = TestExpectationSerializer.list_to_string(expectation_lines, converter)
        self.assertEquals(result, expected_string)

    def test_remove_expectation(self):
        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/hang.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 MAC : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html',  """
BUGX1 MAC : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 LINUX MAC VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected = PASS
BUGX2 XP RELEASE : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 WIN : failures/expected = PASS
BUGX2 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 WIN : failures/expected = FAIL""", 'failures/expected/keyboard.html', """
BUGX1 WIN : failures/expected = FAIL""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE PASS
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = PASS IMAGE
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""")

        self.assert_remove_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE PASS
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", 'failures/expected/keyboard.html', """
BUGX2 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE""", remove_flakes=True)

    def test_remove_expectation_multiple(self):
        in_string = """
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE"""
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        test = "failures/expected/keyboard.html"

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'release', 'cpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'cpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('vista', 'x86', 'debug', 'gpu'), TestConfiguration('win7', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG GPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA RELEASE : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'gpu'), TestConfiguration('xp', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA RELEASE : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set([TestConfiguration('vista', 'x86', 'debug', 'cpu'), TestConfiguration('vista', 'x86', 'debug', 'gpu'), TestConfiguration('vista', 'x86', 'release', 'gpu')]))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.remove_expectation(test, set(self.test_port.all_test_configurations()))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        test = "failures/expected/audio.html"

        editor.remove_expectation(test, set(self.test_port.all_test_configurations()))
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), "")

    def test_update_expectation(self):
        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), '', 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE CPU : failures/expected = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP RELEASE CPU : failures/expected = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = PASS""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([TEXT]), """
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 0)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1, parsed_bug_modifiers=['BUGAWESOME'])

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT""", 1)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, parsed_bug_modifiers=['BUGAWESOME'])

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT""", 1)

        self.assert_update_roundtrip("""
BUGX1 WIN : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUGX1 XP GPU : failures/expected/keyboard.html = TEXT
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED XP RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 XP RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([PASS]), '', 1, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 RELEASE CPU: failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 LINUX MAC RELEASE CPU : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 2, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

        self.assert_update_roundtrip("""
BUGX1 MAC : failures/expected/keyboard.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 MAC : failures/expected/keyboard.html = TEXT
BUG_NEWLY_CREATED WIN RELEASE CPU : failures/expected/keyboard.html = IMAGE""", 1, test_configs=self.WIN_RELEASE_CPU_CONFIGS)

    def test_update_expectation_relative(self):
        self.assert_update_roundtrip("""
BUGX1 XP RELEASE : failures/expected/keyboard.html = TEXT
BUGX2 MAC : failures/expected/audio.html = TEXT""", 'failures/expected/keyboard.html', set([IMAGE]), """
BUGX1 XP RELEASE GPU : failures/expected/keyboard.html = TEXT
BUGAWESOME XP RELEASE CPU : failures/expected/keyboard.html = IMAGE
BUGX2 MAC : failures/expected/audio.html = TEXT""", 2, parsed_bug_modifiers=['BUGAWESOME'])

    def test_update_expectation_multiple(self):
        in_string = """
BUGX1 WIN : failures/expected/keyboard.html = IMAGE
BUGX2 WIN : failures/expected/audio.html = IMAGE"""
        expectation_lines = self.make_parsed_expectation_lines(in_string)
        converter = TestConfigurationConverter(self.test_port.all_test_configurations(), self.test_port.configuration_specifier_macros())
        editor = TestExpectationsEditor(expectation_lines, MockBugManager())
        test = "failures/expected/keyboard.html"

        editor.update_expectation(test, set([TestConfiguration('xp', 'x86', 'release', 'cpu')]), set([IMAGE_PLUS_TEXT]), ['BUG_UPDATE1'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUG_UPDATE1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE+TEXT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, set([TestConfiguration('xp', 'x86', 'debug', 'cpu')]), set([TEXT]), ['BUG_UPDATE2'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE1 XP RELEASE CPU : failures/expected/keyboard.html = IMAGE+TEXT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, self.WIN_RELEASE_CPU_CONFIGS, set([CRASH]), ['BUG_UPDATE3'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 VISTA DEBUG CPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 RELEASE GPU : failures/expected/keyboard.html = IMAGE
BUGX1 WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA XP GPU : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE3 WIN RELEASE CPU : failures/expected/keyboard.html = CRASH
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, self.RELEASE_CONFIGS, set([FAIL]), ['BUG_UPDATE4'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUGX1 XP DEBUG GPU : failures/expected/keyboard.html = IMAGE
BUGX1 VISTA WIN7 DEBUG : failures/expected/keyboard.html = IMAGE
BUG_UPDATE2 XP DEBUG CPU : failures/expected/keyboard.html = TEXT
BUG_UPDATE4 RELEASE : failures/expected/keyboard.html = FAIL
BUGX2 WIN : failures/expected/audio.html = IMAGE""")

        editor.update_expectation(test, set(self.test_port.all_test_configurations()), set([TIMEOUT]), ['BUG_UPDATE5'])
        self.assertEquals(TestExpectationSerializer.list_to_string(expectation_lines, converter), """
BUG_UPDATE5 : failures/expected/keyboard.html = TIMEOUT
BUGX2 WIN : failures/expected/audio.html = IMAGE""")


if __name__ == '__main__':
    unittest.main()
