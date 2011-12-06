# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest
import sys

from webkitpy.common.system.crashlogs import *
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.thirdparty.mock import Mock


class CrashLogsTest(unittest.TestCase):
    def test_find_log_darwin(self):
        if sys.platform != "darwin":
            return
        older_mock_crash_report = "Older Mock Crash Report"
        mock_crash_report = "Mock Crash Report"
        newer_mock_crash_report = "Newer Mock Crash Report"
        files = {}
        files['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150718_quadzen.crash'] = older_mock_crash_report
        files['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150719_quadzen.crash'] = mock_crash_report
        files['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150720_quadzen.crash'] = newer_mock_crash_report
        xattrs = {}
        xattrs['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150718_quadzen.crash'] = {}
        xattrs['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150719_quadzen.crash'] = {'app_description': 'TextMate[28530] version ??? (???)'}
        xattrs['/Users/mock/Library/Logs/DiagnosticReports/TextMate_2011-06-13-150720_quadzen.crash'] = {'app_description': 'TextMate[28529] version ??? (???)'}
        filesystem = MockFileSystem(files, xattrs=xattrs)
        crash_logs = CrashLogs(filesystem)
        log = crash_logs.find_newest_log("TextMate")
        self.assertEqual(log, newer_mock_crash_report)
        log = crash_logs.find_newest_log("TextMate", 28529)
        self.assertEqual(log, newer_mock_crash_report)
        log = crash_logs.find_newest_log("TextMate", 28530)
        self.assertEqual(log, mock_crash_report)
        log = crash_logs.find_newest_log("TextMate", 28531)
        self.assertEqual(log, None)
