# Copyright (C) 2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.net.abstracttestresults import AbstractTestResults


class AbstractTestResultsTest(unittest.TestCase):
    def test_parse_json_string_invalid_inputs(self):
        none_item = None
        empty_json = ''
        invalid_json = '{"allApiTestsPassed":'
        invalid_json_v2 = '{"err'
        self.assertEqual(None, AbstractTestResults.parse_json_string(none_item))
        self.assertEqual(None, AbstractTestResults.parse_json_string(empty_json))
        self.assertEqual(None, AbstractTestResults.parse_json_string(invalid_json))
        self.assertEqual(None, AbstractTestResults.parse_json_string(invalid_json_v2))

    def test_parse_json_string_valid_input(self):
        test_string = '{"failures": ["fail1", "fail2"], "errors": []}'
        test_dict = {"failures": ["fail1", "fail2"], "errors": []}

        self.assertEqual(test_dict, AbstractTestResults.parse_json_string(test_string))
