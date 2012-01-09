# Copyright (C) 2011 Google Inc. All rights reserved.
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

import sys
import unittest

from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.host_mock import MockHost


class TestBaselineOptimizer(BaselineOptimizer):
    def __init__(self, mock_results_by_directory):
        host = MockHost()
        BaselineOptimizer.__init__(self, host)
        self._mock_results_by_directory = mock_results_by_directory

    # We override this method for testing so we don't have to construct an
    # elaborate mock file system.
    def _read_results_by_directory(self, baseline_name):
        return self._mock_results_by_directory


class BaselineOptimizerTest(unittest.TestCase):
    def _assertOptimization(self, results_by_directory, expected_new_results_by_directory):
        baseline_optimizer = TestBaselineOptimizer(results_by_directory)
        _, new_results_by_directory = baseline_optimizer._find_optimal_result_placement('mock-baseline.png')
        self.assertEqual(new_results_by_directory, expected_new_results_by_directory)

    def test_move_baselines(self):
        host = MockHost()
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/platform/chromium-win/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/platform/chromium-mac/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/platform/chromium/another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(host)
        baseline_optimizer._move_baselines('another/test-expected.txt', {
            'LayoutTests/platform/chromium-win': 'aaa',
            'LayoutTests/platform/chromium-mac': 'aaa',
            'LayoutTests/platform/chromium': 'bbb',
        }, {
            'LayoutTests/platform/chromium': 'aaa',
        })
        self.assertEqual(host.filesystem.read_binary_file('/mock-checkout/LayoutTests/platform/chromium/another/test-expected.txt'), 'result A')

    def test_chromium_linux_redundant_with_win(self):
        self._assertOptimization({
            'LayoutTests/platform/chromium-win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium-linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/chromium-win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_chromium_covers_mac_win_linux(self):
        self._assertOptimization({
            'LayoutTests/platform/chromium-mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium-win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium-linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/chromium': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_chromium_mac_redundant_with_apple_mac(self):
        self._assertOptimization({
            'LayoutTests/platform/chromium-mac-snowleopard': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/mac-snowleopard': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/mac-snowleopard': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_mac_future(self):
        self._assertOptimization({
            'LayoutTests/platform/mac-snowleopard': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/mac-snowleopard': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_qt_unknown(self):
        self._assertOptimization({
            'LayoutTests/platform/qt': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/qt': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_common_directory_includes_root(self):
        # Note: The resulting directories are "wrong" in the sense that
        # enacting this plan would change semantics. However, this test case
        # demonstrates that we don't throw an exception in this case. :)
        self._assertOptimization({
            'LayoutTests/platform/gtk': 'e8608763f6241ddacdd5c1ef1973ba27177d0846',
            'LayoutTests/platform/qt': 'bcbd457d545986b7abf1221655d722363079ac87',
            'LayoutTests/platform/chromium-win': '3764ac11e1f9fbadd87a90a2e40278319190a0d3',
            'LayoutTests/platform/mac': 'e8608763f6241ddacdd5c1ef1973ba27177d0846',
        }, {
            'LayoutTests/platform/qt': 'bcbd457d545986b7abf1221655d722363079ac87',
            'LayoutTests/platform/chromium-win': '3764ac11e1f9fbadd87a90a2e40278319190a0d3',
            'LayoutTests': 'e8608763f6241ddacdd5c1ef1973ba27177d0846',
        })

        self._assertOptimization({
            'LayoutTests/platform/chromium-win': '23a30302a6910f8a48b1007fa36f3e3158341834',
            'LayoutTests': '9c876f8c3e4cc2aef9519a6c1174eb3432591127',
            'LayoutTests/platform/chromium-mac': '23a30302a6910f8a48b1007fa36f3e3158341834',
            'LayoutTests/platform/chromium-mac': '23a30302a6910f8a48b1007fa36f3e3158341834',
        }, {
            'LayoutTests/platform/chromium': '23a30302a6910f8a48b1007fa36f3e3158341834',
            'LayoutTests': '9c876f8c3e4cc2aef9519a6c1174eb3432591127',
        })

    def test_complex_shadowing(self):
        # This test relies on OS specific functionality, so it doesn't work on Windows.
        # FIXME: What functionality does this rely on?  When can we remove this if?
        if sys.platform == 'win32':
            return
        self._assertOptimization({
            'LayoutTests/platform/chromium-win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/mac': '5daa78e55f05d9f0d1bb1f32b0cd1bc3a01e9364',
            'LayoutTests/platform/chromium-win-xp': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium-mac-leopard': '65e7d42f8b4882b29d46dc77bb879dd41bc074dc',
            'LayoutTests/platform/mac-leopard': '7ad045ece7c030e2283c5d21d9587be22bcba56e',
            'LayoutTests/platform/chromium-win-vista': 'f83af9732ce74f702b8c9c4a3d9a4c6636b8d3bd',
            'LayoutTests/platform/win': '5b1253ef4d5094530d5f1bc6cdb95c90b446bec7',
            'LayoutTests/platform/chromium-linux': 'f52fcdde9e4be8bd5142171cd859230bd4471036',
        }, {
            'LayoutTests/platform/chromium-win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/mac': '5daa78e55f05d9f0d1bb1f32b0cd1bc3a01e9364',
            'LayoutTests/platform/chromium-win-xp': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium-mac-leopard': '65e7d42f8b4882b29d46dc77bb879dd41bc074dc',
            'LayoutTests/platform/mac-leopard': '7ad045ece7c030e2283c5d21d9587be22bcba56e',
            'LayoutTests/platform/chromium-win-vista': 'f83af9732ce74f702b8c9c4a3d9a4c6636b8d3bd',
            'LayoutTests/platform/win': '5b1253ef4d5094530d5f1bc6cdb95c90b446bec7',
            'LayoutTests/platform/chromium-linux': 'f52fcdde9e4be8bd5142171cd859230bd4471036'
        })
