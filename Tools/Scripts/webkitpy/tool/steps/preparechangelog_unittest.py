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

import os
import unittest

from webkitpy.common.checkout.changelog_unittest import ChangeLogTest
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.preparechangelog import PrepareChangeLog


class PrepareChangeLogTest(ChangeLogTest):
    def test_ensure_bug_url(self):
        capture = OutputCapture()
        step = PrepareChangeLog(MockTool(), MockOptions())
        changelog_contents = u"%s\n%s" % (self._new_entry_boilerplate, self._example_changelog)
        changelog_path = self._write_tmp_file_with_contents(changelog_contents.encode("utf-8"))
        state = {
            "bug_title": "Example title",
            "bug_id": 1234,
            "changelogs": [changelog_path],
        }
        capture.assert_outputs(self, step.run, [state])
        actual_contents = self._read_file_contents(changelog_path, "utf-8")
        expected_message = "Example title\n        http://example.com/1234"
        expected_contents = changelog_contents.replace("Need a short description and bug URL (OOPS!)", expected_message)
        os.remove(changelog_path)
        self.assertEquals(actual_contents, expected_contents)
