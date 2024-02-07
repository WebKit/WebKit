# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import mock
import os
import unittest

from .results import get_project_issue_count_as_string

INVALID_INPUT_LONG = f'<html><head><title>Results</title></head><body>\n'
INVALID_INPUT_SHORT = f'<html>\n'
VALID_INPUT = f'<tr style="font-weight:bold"><td class="SUMM_DESC">All Bugs</td><td class="Q">12</td><td><center><input type="checkbox" id="AllBugsCheck" onClick="CopyCheckedStateToCheckButtons(this);" checked/></center></td></tr>\n'


class StaticAnalysisResultsTest(unittest.TestCase):

    @mock.patch('os.path.exists', return_value=False)
    def test_get_project_issue_count_as_string_no_file(self, mocked_os_path_exists):
        path = 'path'
        self.assertEqual(get_project_issue_count_as_string(path), '0')
        mocked_os_path_exists.assert_called_once_with(path)

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('subprocess.check_output', return_value=INVALID_INPUT_LONG)
    def test_get_project_issue_count_as_string_invalid_long(
            self, mocked_subprocess_check_output, mocked_os_path_exists):
        path = 'path'
        self.assertEqual(get_project_issue_count_as_string(path), '0')
        mocked_subprocess_check_output.assert_called_once_with(['/usr/bin/grep', 'All Bugs', path], text=True)
        mocked_os_path_exists.assert_called_once_with(path)

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('subprocess.check_output', return_value=INVALID_INPUT_SHORT)
    def test_get_project_issue_count_as_string_invalid_short(
            self, mocked_subprocess_check_output, mocked_os_path_exists):
        path = 'path'
        self.assertEqual(get_project_issue_count_as_string(path), '0')
        mocked_subprocess_check_output.assert_called_once_with(['/usr/bin/grep', 'All Bugs', path], text=True)
        mocked_os_path_exists.assert_called_once_with(path)

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('subprocess.check_output', return_value=VALID_INPUT)
    def test_get_project_issue_count_as_string_valid(self, mocked_subprocess_check_output, mocked_os_path_exists):
        path = 'path'
        self.assertEqual(get_project_issue_count_as_string(path), '12')
        mocked_subprocess_check_output.assert_called_once_with(['/usr/bin/grep', 'All Bugs', path], text=True)
        mocked_os_path_exists.assert_called_once_with(path)
