# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
# Copyright (C) 2015, 2020 Apple Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import logging
import os
import unittest

from webkitpy.w3c.test_parser import TestParser

from webkitcorepy import OutputCapture


options = {'all': False, 'no_overwrite': False}


class TestParserTest(unittest.TestCase):

    def test_analyze_test_reftest_one_match(self):
        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
</head>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
        test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertTrue('type' in test_info.keys(), 'did not find a reference type')
        self.assertEqual(test_info['type'], 'match', 'reference type is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

    def test_analyze_test_reftest_one_mismatch(self):
        test_html = """<head>
<link rel="mismatch" href="green-box-ref.xht" />
</head>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
        test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertTrue('type' in test_info.keys(), 'did not find a reference type')
        self.assertEqual(test_info['type'], 'mismatch', 'reference type is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

    def test_analyze_test_reftest_multiple_matches(self):
        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
<link rel="match" href="blue-box-ref.xht" />
<link rel="match" href="orange-box-ref.xht" />
</head>
"""
        with OutputCapture(level=logging.INFO) as captured:
            test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
            parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
            test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

        self.assertEqual(
            captured.root.log.getvalue(),
            'Multiple references are not supported. Importing the first ref defined in somefile.html\n',
        )

    def test_analyze_test_reftest_match_and_mismatch(self):
        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
<link rel="match" href="blue-box-ref.xht" />
<link rel="mismatch" href="orange-box-notref.xht" />
</head>
"""
        with OutputCapture(level=logging.INFO) as captured:
            test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
            parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
            test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

        self.assertEqual(
            captured.root.log.getvalue(),
            'Multiple references are not supported. Importing the first ref defined in somefile.html\n',
        )

    def test_analyze_test_reftest_with_ref_support_Files(self):
        """ Tests analyze_test() using a reftest that has refers to a reference file outside of the tests directory and the reference file has paths to other support files """

        test_html = """<html>
<head>
<link rel="match" href="../reference/green-box-ref.xht" />
</head>
"""
        ref_html = """<head>
<link href="support/css/ref-stylesheet.css" rel="stylesheet" type="text/css">
<style type="text/css">
    background-image: url("../../support/some-image.png")
    background-image: url("../../support/some-other-image.png")
</style>
</head>
<body>
<div><img src="../support/black96x96.png" alt="Image download support must be enabled" /></div>
</body>
</html>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
        test_info = parser.analyze_test(test_contents=test_html, ref_contents=ref_html)

        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertTrue('reference_support_info' in test_info.keys(), 'there should be reference_support_info for this test')
        self.assertEqual(len(test_info['reference_support_info']['files']), 4, 'there should be 4 support files in this reference')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

    def test_analyze_jstest(self):
        """ Tests analyze_test() using a jstest """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
        test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None, 'test_info is None')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertFalse('reference' in test_info.keys(), 'shold not have found a reference file')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertTrue('jstest' in test_info.keys(), 'test should be a jstest')

    def test_analyze_manual_wpt_test(self):
        """ Tests analyze_test() using a manual jstest """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile-manual.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertTrue(test_info['manualtest'], 'test_info is manual')

        parser = TestParser(options, os.path.join(test_path, 'somefile-manual.https.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertTrue(test_info['manualtest'], 'test_info is manual')

        parser = TestParser(options, os.path.join(test_path, 'somefile-manual-https.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertFalse('manualtest' in test_info.keys() and test_info['manualtest'], 'test_info is not manual')

        parser = TestParser(options, os.path.join(test_path, 'somefile-ref-manual.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertFalse('manualtest' in test_info.keys() and test_info['manualtest'], 'test_info is not manual')

        ref_test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path', 'reference')
        parser = TestParser(options, os.path.join(ref_test_path, 'somefile-manual.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertFalse('manualtest' in test_info.keys() and test_info['manualtest'], 'test_info is not manual')

    def test_analyze_manual_reference_wpt_test(self):
        """ Tests analyze_test() using a manual reference test """

        test_html = """<head>
<link rel="match" href="reference/somefile-manual-ref.html">
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile-manual.html'))
        test_info = parser.analyze_test(test_contents=test_html)
        self.assertTrue(test_info['manualtest'], 'test_info is manual')

    def test_analyze_css_manual_test(self):
        """ Tests analyze_test() using a css manual test """

        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile.html'))

        for content in ["", "flag1", "flag1 flag2", "flag1 flag2 flag3", "asis"]:
            test_html = """
<head>
  <meta name="flags" content="%s">
</head>""" % content
            test_info = parser.analyze_test(test_contents=test_html)
            self.assertEqual(test_info, None, 'test_info should be None')

        for flag in ["animated", "font", "history", "interact", "paged", "speech", "userstyle"]:
            test_html = """
<head>
  <meta name="flags" content="flag1 flag2">
  <meta name="flags" content="flag3 %s flag4 flag5">
  <meta name="flags" content="flag6">
</head>
""" % flag
            test_info = parser.analyze_test(test_contents=test_html)
            self.assertTrue(test_info['manualtest'], 'test with CSS flag %s should be manual' % flag)

    def test_analyze_crash_test(self):
        test_html = """<body></body>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'somefile-crash.html'))
        test_info = parser.analyze_test(test_contents=test_html)

        self.assertNotEqual(test_info, None)
        self.assertTrue('test' in test_info.keys())
        self.assertTrue('crashtest' in test_info.keys())
        self.assertFalse('reference' in test_info.keys())
        self.assertFalse('type' in test_info.keys())
        self.assertFalse('refsupport' in test_info.keys())
        self.assertFalse('jstest' in test_info.keys())

    def test_analyze_pixel_test_all_true(self):
        """ Tests analyze_test() using a test that is neither a reftest or jstest with all=False """

        test_html = """<html>
<head>
<title>CSS Test: DESCRIPTION OF TEST</title>
<link rel="author" title="NAME_OF_AUTHOR" />
<style type="text/css"><![CDATA[
CSS FOR TEST
]]></style>
</head>
<body>
CONTENT OF TEST
</body>
</html>
"""
        # Set options to 'all' so this gets found
        global options
        orig_options = options.copy()
        try:
            options['all'] = True

            test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
            parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
            test_info = parser.analyze_test(test_contents=test_html)

            self.assertNotEqual(test_info, None, 'test_info is None')
            self.assertTrue('test' in test_info.keys(), 'did not find a test file')
            self.assertFalse('reference' in test_info.keys(), 'shold not have found a reference file')
            self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
            self.assertFalse('jstest' in test_info.keys(), 'test should not be a jstest')
        finally:
            options = orig_options

    def test_analyze_pixel_test_all_false(self):
        """ Tests analyze_test() using a test that is neither a reftest or jstest, with -all=False """

        test_html = """<html>
<head>
<title>CSS Test: DESCRIPTION OF TEST</title>
<link rel="author" title="NAME_OF_AUTHOR" />
<style type="text/css"><![CDATA[
CSS FOR TEST
]]></style>
</head>
<body>
CONTENT OF TEST
</body>
</html>
"""
        # Set all to false so this gets skipped
        global options
        orig_options = options.copy()
        try:
            options['all'] = False

            test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
            parser = TestParser(options, os.path.join(test_path, 'somefile.html'))
            test_info = parser.analyze_test(test_contents=test_html)

            self.assertEqual(test_info, None, 'test should have been skipped')
        finally:
            options = orig_options

    def test_analyze_non_html_file(self):
        """ Tests analyze_test() with a file that has no html"""
        # FIXME: use a mock filesystem
        parser = TestParser(options, os.path.join(os.path.dirname(__file__), 'test_parser.py'))
        test_info = parser.analyze_test()
        self.assertEqual(test_info, None, 'no tests should have been found in this file')

    def test_reference_test(self):
        """ Tests analyze_test() using a test that is a reference file having a <link rel="match"> tag"""

        test_html = """<html>
<head>
<title>CSS Test: DESCRIPTION OF TEST</title>
<link rel="match" href="test-ref.html" />
<link rel="author" title="NAME_OF_AUTHOR" />
<style type="text/css"><![CDATA[
CSS FOR TEST
]]></style>
</head>
<body>
CONTENT OF TEST
</body>
</html>
"""
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser(options, os.path.join(test_path, 'test-ref.html'))
        test_info = parser.analyze_test(test_contents=test_html)

        self.assertTrue('referencefile' in test_info, 'test should be detected as reference file')

    def _test_info_from_test_with_contents(self, test_contents):
        test_path = os.path.join(os.path.sep, 'some', 'madeup', 'path')
        parser = TestParser({'all': True}, os.path.join(test_path, 'test-fuzzy.html'))
        return parser.analyze_test(test_contents=test_contents)

    def test_simple_fuzzy_data(self):
        """ Tests basic form of fuzzy_metadata()"""

        test_html = """<html><head>
<meta name=fuzzy content="maxDifference = 15 ; totalPixels = 300">
</head>
<body>CONTENT OF TEST</body></html>
"""
        test_info = self._test_info_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[15, 15], [300, 300]]}
        self.assertEqual(test_info['fuzzy'], expected_fuzzy, 'fuzzy data did not match expected')

    def test_nameless_fuzzy_data(self):
        """ Tests fuzzy_metadata() in short form"""

        test_html = """<html><head>
<meta name=fuzzy content=" 15 ; 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        test_info = self._test_info_from_test_with_contents(test_html)
        expected_fuzzy = {None: [[15, 15], [300, 300]]}
        self.assertEqual(test_info['fuzzy'], expected_fuzzy, 'fuzzy data did not match expected')

    def test_range_fuzzy_data(self):
        """ Tests fuzzy_metadata() in range form"""

        test_html = """<html><head>
<meta name=fuzzy content="maxDifference=5-15;totalPixels =  200 - 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        test_info = self._test_info_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[5, 15], [200, 300]]}
        self.assertEqual(test_info['fuzzy'], expected_fuzzy, 'fuzzy data did not match expected')

    def test_nameless_range_fuzzy_data(self):
        """ Tests fuzzy_metadata() in short range form"""

        test_html = """<html><head>
<meta name=fuzzy content="5-15;  200 - 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        test_info = self._test_info_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[5, 15], [200, 300]]}
        self.assertEqual(test_info['fuzzy'], expected_fuzzy, 'fuzzy data did not match expected')

    def test_per_ref_fuzzy_data(self):
        """ Tests fuzzy_metadata() with values for difference reference files"""

        test_html = """<html><head>
<meta name=fuzzy content="5-15;200-300 ">
<meta name=fuzzy content="close-match-ref.html:5;20">
<meta name=fuzzy content="worse-match-ref.html: 15;30">
</head>
<body>CONTENT OF TEST</body></html>
"""
        test_info = self._test_info_from_test_with_contents(test_html)

        expected_fuzzy = {
            None: [[5, 15], [200, 300]],
            'close-match-ref.html': [[5, 5], [20, 20]],
            'worse-match-ref.html': [[15, 15], [30, 30]]
        }
        self.assertEqual(test_info['fuzzy'], expected_fuzzy, 'fuzzy data did not match expected')
