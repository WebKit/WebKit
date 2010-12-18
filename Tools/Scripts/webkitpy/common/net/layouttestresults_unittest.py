# Copyright (c) 2010, Google Inc. All rights reserved.
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

from webkitpy.common.net.layouttestresults import LayoutTestResults


class LayoutTestResultsTest(unittest.TestCase):
    _example_results_html = """
<html>
<head>
<title>Layout Test Results</title>
</head>
<body>
<p>Tests that had stderr output:</p>
<table>
<tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/accessibility/aria-activedescendant-crash.html">accessibility/aria-activedescendant-crash.html</a></td>
<td><a href="accessibility/aria-activedescendant-crash-stderr.txt">stderr</a></td>
</tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/http/tests/security/canvas-remote-read-svg-image.html">http/tests/security/canvas-remote-read-svg-image.html</a></td>
<td><a href="http/tests/security/canvas-remote-read-svg-image-stderr.txt">stderr</a></td>
</tr>
</table><p>Tests that had no expected results (probably new):</p>
<table>
<tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/fast/repaint/no-caret-repaint-in-non-content-editable-element.html">fast/repaint/no-caret-repaint-in-non-content-editable-element.html</a></td>
<td><a href="fast/repaint/no-caret-repaint-in-non-content-editable-element-actual.txt">result</a></td>
</tr>
</table></body>
</html>
"""

    _expected_layout_test_results = {
        'Tests that had stderr output:': [
            'accessibility/aria-activedescendant-crash.html',
        ],
        'Tests that had no expected results (probably new):': [
            'fast/repaint/no-caret-repaint-in-non-content-editable-element.html',
        ],
    }

    def test_parse_layout_test_results(self):
        results = LayoutTestResults._parse_results_html(self._example_results_html)
        self.assertEqual(self._expected_layout_test_results, results)

    def test_results_from_string(self):
        self.assertEqual(LayoutTestResults.results_from_string(None), None)
        self.assertEqual(LayoutTestResults.results_from_string(""), None)
        results = LayoutTestResults.results_from_string(self._example_results_html)
        self.assertEqual(len(results.failing_tests()), 0)
