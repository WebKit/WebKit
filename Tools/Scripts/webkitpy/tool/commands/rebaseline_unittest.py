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
from webkitpy.tool.mocktool import MockTool


class TestRebaseline(unittest.TestCase):
    # This just makes sure the code runs without exceptions.
    def test_tests_to_update(self):
        command = Rebaseline()
        command.bind_to_tool(MockTool())
        build = Mock()
        OutputCapture().assert_outputs(self, command._tests_to_update, [build])


class TestRebaselineTest(unittest.TestCase):
    def test_tests_to_update(self):
        command = RebaselineTest()
        command.bind_to_tool(MockTool())
        build = Mock()
        expected_stdout = "Retrieving http://example.com/f/builders/Webkit Linux/results//userscripts/another-test-actual.txt ...\n"
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Linux", "userscripts/another-test.html", "txt"], expected_stdout=expected_stdout)


class BuilderToPortTest(unittest.TestCase):
    def test_port_for_builder(self):
        converter = BuilderToPort()
        port = converter.port_for_builder("Leopard Intel Debug (Tests)")
        self.assertEqual(str(port.test_configuration()), "<leopard, x86, debug, cpu>")
        port = converter.port_for_builder("Leopard Intel Release (Tests)")
        self.assertEqual(str(port.test_configuration()), "<leopard, x86, release, cpu>")
        port = converter.port_for_builder("Webkit Win (dbg)(1)")
        self.assertEqual(str(port.test_configuration()), "<xp, x86, debug, cpu>")
