# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for manager.py."""

import unittest

from webkitpy.api_tests.manager import Manager


class ManagerTest(unittest.TestCase):
    def test_test_list_from_output(self):
        gtest_list_tests_output = """WKWebViewDisableSelection.
  DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled
  DragDoesNotSelectWhenTextInteractionsAreDisabled
Misc/ValueParametrizedTest.
  ValueParametrizedTestsSupported/CatRed  # GetParam() = (Cat, Red)
  ValueParametrizedTestsSupported/CatGreen  # GetParam() = (Cat, Green)
  ValueParametrizedTestsSupported/DogRed  # GetParam() = (Dog, Red)
  ValueParametrizedTestsSupported/DogGreen  # GetParam() = (Dog, Green)
A.
  B
"""
        expected_tests = [
            "WKWebViewDisableSelection.DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled",
            "WKWebViewDisableSelection.DragDoesNotSelectWhenTextInteractionsAreDisabled",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/CatRed",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/CatGreen",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/DogRed",
            "Misc/ValueParametrizedTest.ValueParametrizedTestsSupported/DogGreen",
            "A.B",
        ]
        got_tests = Manager._test_list_from_output(gtest_list_tests_output)
        self.assertEqual(expected_tests, got_tests)
