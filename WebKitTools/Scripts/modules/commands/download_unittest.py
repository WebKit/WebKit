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

import unittest

from modules.commands.commandtest import CommandsTest
from modules.commands.download import *
from modules.mock import Mock

class DownloadCommandsTest(CommandsTest):
    def _default_options(self):
        options = Mock()
        options.force_clean = False
        options.clean = True
        options.check_builders = True
        options.quiet = False
        options.non_interactive = False
        options.update = True
        options.build = True
        options.test = True
        options.close_bug = True
        return options

    def test_build(self):
        self.assert_execute_outputs(Build(), [], options=self._default_options())

    def test_land_diff(self):
        self.assert_execute_outputs(LandDiff(), [42], options=self._default_options())

    def test_check_style(self):
        self.assert_execute_outputs(CheckStyle(), [197], options=self._default_options())

    def test_build_attachment(self):
        self.assert_execute_outputs(BuildAttachment(), [197], options=self._default_options())

    def test_land_attachment(self):
        self.assert_execute_outputs(LandAttachment(), [197], options=self._default_options())

    def test_land_patches(self):
        self.assert_execute_outputs(LandPatches(), [42], options=self._default_options())
