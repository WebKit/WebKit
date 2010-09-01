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

from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.earlywarningsystem import *
from webkitpy.tool.commands.queuestest import QueuesTest

class EarlyWarningSytemTest(QueuesTest):
    def test_failed_builds(self):
        ews = ChromiumLinuxEWS()
        ews._build = lambda patch, first_run=False: False
        ews._can_build = lambda: True
        ews.review_patch(Mock())

    def _default_expected_stderr(self, ews):
        string_replacemnts = {
            "name": ews.name,
            "checkout_dir": os.getcwd(),  # FIXME: Use of os.getcwd() is wrong, should be scm.checkout_root
            "port": ews.port_name,
            "watchers": ews.watchers,
        }
        expected_stderr = {
            "begin_work_queue": "CAUTION: %(name)s will discard all local changes in \"%(checkout_dir)s\"\nRunning WebKit %(name)s.\n" % string_replacemnts,
            "handle_unexpected_error": "Mock error message\n",
            "next_work_item": "MOCK: update_work_items: %(name)s [103]\n" % string_replacemnts,
            "process_work_item": "MOCK: update_status: %(name)s Pass\n" % string_replacemnts,
            "handle_script_error": "MOCK: update_status: %(name)s ScriptError error message\nMOCK bug comment: bug_id=345, cc=%(watchers)s\n--- Begin comment ---\\Attachment 1234 did not build on %(port)s:\nBuild output: http://dummy_url\n--- End comment ---\n\n" % string_replacemnts,
        }
        return expected_stderr

    def _test_ews(self, ews):
        expected_exceptions = {
            "handle_script_error": SystemExit,
        }
        self.assert_queue_outputs(ews, expected_stderr=self._default_expected_stderr(ews), expected_exceptions=expected_exceptions)

    # FIXME: If all EWSes are going to output the same text, we
    # could test them all in one method with a for loop over an array.
    def test_chromium_linux_ews(self):
        self._test_ews(ChromiumLinuxEWS())

    def test_chromium_windows_ews(self):
        self._test_ews(ChromiumWindowsEWS())

    def test_qt_ews(self):
        self._test_ews(QtEWS())

    def test_gtk_ews(self):
        self._test_ews(GtkEWS())

    def test_efl_ews(self):
        self._test_ews(EflEWS())

    def test_mac_ews(self):
        ews = MacEWS()
        expected_stderr = self._default_expected_stderr(ews)
        expected_stderr["process_work_item"] = "MOCK: update_status: mac-ews Error: mac-ews cannot process patches from non-committers :(\n"
        expected_exceptions = {
            "handle_script_error": SystemExit,
        }
        self.assert_queue_outputs(ews, expected_stderr=expected_stderr, expected_exceptions=expected_exceptions)
