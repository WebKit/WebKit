#!/usr/bin/env python
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.system import filesystem_mock

from webkitpy.layout_tests.port.webkit import WebKitPort
from webkitpy.layout_tests.port import port_testcase


class TestWebKitPort(WebKitPort):
    def __init__(self, symbol_list=None, feature_list=None,
                 expectations_file=None, skips_file=None, **kwargs):
        self.symbol_list = symbol_list
        self.feature_list = feature_list
        self.expectations_file = expectations_file
        self.skips_file = skips_file
        WebKitPort.__init__(self, **kwargs)

    def _runtime_feature_list(self):
        return self.feature_list

    def _supported_symbol_list(self):
        return self.symbol_list

    def _tests_for_other_platforms(self):
        return ["media", ]

    def _tests_for_disabled_features(self):
        return ["accessibility", ]

    def path_to_test_expectations_file(self):
        if self.expectations_file:
            return self.expectations_file
        return WebKitPort.path_to_test_expectations_file(self)

    def _skipped_file_paths(self):
        if self.skips_file:
            return [self.skips_file]
        return []


class WebKitPortTest(port_testcase.PortTestCase):
    def port_maker(self, platform):
        return WebKitPort

    def test_driver_cmd_line(self):
        # Routine is not implemented.
        pass

    def test_baseline_search_path(self):
        # Routine is not implemented.
        pass

    def test_skipped_directories_for_symbols(self):
        supported_symbols = ["GraphicsLayer", "WebCoreHas3DRendering", "isXHTMLMPDocument", "fooSymbol"]
        expected_directories = set(["mathml", "fast/canvas/webgl", "compositing/webgl", "http/tests/canvas/webgl", "http/tests/wml", "fast/wml", "wml", "fast/wcss"])
        result_directories = set(TestWebKitPort(supported_symbols, None)._skipped_tests_for_unsupported_features())
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_directories_for_features(self):
        supported_features = ["Accelerated Compositing", "Foo Feature"]
        expected_directories = set(["animations/3d", "transforms/3d"])
        result_directories = set(TestWebKitPort(None, supported_features)._skipped_tests_for_unsupported_features())
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_layout_tests(self):
        self.assertEqual(TestWebKitPort(None, None).skipped_layout_tests(),
                         set(["media", "accessibility"]))

    def test_test_expectations(self):
        # Check that we read both the expectations file and anything in a
        # Skipped file, and that we include the feature and platform checks.
        files = {
            '/tmp/test_expectations.txt': 'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n',
            '/tmp/Skipped': 'fast/html/keygen.html',
        }
        mock_fs = filesystem_mock.MockFileSystem(files)
        port = TestWebKitPort(expectations_file='/tmp/test_expectations.txt',
                              skips_file='/tmp/Skipped', filesystem=mock_fs)
        self.assertEqual(port.test_expectations(),
        """BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL
BUG_SKIPPED SKIP : fast/html/keygen.html = FAIL
BUG_SKIPPED SKIP : media = FAIL
BUG_SKIPPED SKIP : accessibility = FAIL""")


if __name__ == '__main__':
    unittest.main()
