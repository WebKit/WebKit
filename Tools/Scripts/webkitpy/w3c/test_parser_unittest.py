#!/usr/bin/env python

# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
import os
import StringIO
import sys
import unittest2 as unittest

from webkitpy.w3c.test_parser import TestParser


options = {'all': False, 'no_overwrite': False}


class TestParserTest(unittest.TestCase):

    def test_analyzeTestReftestOneMatch(self):
        """ Tests analyze_test() using a basic reftest with one match link and no mismatch links """

        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
</head>
"""
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

    def test_analyzeTestReftestMultipleMatches(self):
        """ Tests analyze_test() using a reftest with multiple match links and no mismatch links """

        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
<link rel="match" href="blue-box-ref.xht" />
<link rel="match" href="orange-box-ref.xht" />
</head>
"""
        # Capture stdout for this test
        stdout = sys.stdout
        output = StringIO.StringIO()
        sys.stdout = output

        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Restore stdout
        sys.stdout = stdout

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

        # Verify we got the warning about multiple matches
        self.assertTrue(output.getvalue().startswith('Warning'), 'no warning about multiple matches')

    def test_analyzeTestReftestMatchAndMismatch(self):
        """ Tests analyze_test() using a reftest with a one match link and one mismatch link """

        test_html = """<head>
<link rel="match" href="green-box-ref.xht" />
<link rel="match" href="blue-box-ref.xht" />
<link rel="mismatch" href="orange-box-notref.xht" />
</head>
"""
        # Capture stdout for this test
        stdout = sys.stdout
        output = StringIO.StringIO()
        sys.stdout = output

        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Restore stdout
        sys.stdout = stdout

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

        # Verify we got multiple warnings
        warnings = output.getvalue().splitlines()
        self.assertEquals(len(warnings), 2, 'all warnings were not displayed')
        # Verify the both warnings mention the issue
        self.assertTrue('multiple matches', 'no warning about multiple matches')
        self.assertTrue('mismatch', 'no warning about mismatches')

    def test_analyzeTestReftestWithRefSupportFiles(self):
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
</style>
</head>
<body>
<div><img src="../support/black96x96.png" alt="Image download support must be enabled" /></div>
</body>
</html>
"""
        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html, ref_contents=ref_html)

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'did not find a test')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertTrue('reference' in test_info.keys(), 'did not find a reference file')
        self.assertTrue(test_info['reference'].startswith(test_path), 'reference path is not correct')
        self.assertTrue('refsupport' in test_info.keys(), 'there should be refsupport files for this test')
        self.assertEquals(len(test_info['refsupport']), 3, 'there should be 3 support files in this reference')
        self.assertFalse('jstest' in test_info.keys(), 'test should not have been analyzed as a jstest')

    def test_analyzeJSTest(self):
        """ Tests analyze_test() using a jstest """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'test_info is None')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertFalse('reference' in test_info.keys(), 'shold not have found a reference file')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertTrue('jstest' in test_info.keys(), 'test should be a jstest')

    def test_analyzePixelTestAllTrue(self):
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
        options['all'] = True

        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Verify the correct test info was found
        self.assertNotEqual(test_info, None, 'test_info is None')
        self.assertTrue('test' in test_info.keys(), 'did not find a test file')
        self.assertFalse('reference' in test_info.keys(), 'shold not have found a reference file')
        self.assertFalse('refsupport' in test_info.keys(), 'there should be no refsupport files for this test')
        self.assertFalse('jstest' in test_info.keys(), 'test should not be a jstest')

    def test_analyzePixelTestAllFalse(self):
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
        options['all'] = False

        # Analyze the test
        test_path = '/some/madeup/path/'
        parser = TestParser(options, test_path + 'somefile.html')
        test_info = parser.analyze_test(test_contents=test_html)

        # Verify the correct test info was found
        self.assertEqual(test_info, None, 'test should have been skipped')

    def test_analyzeNonHTMLFile(self):
        """ Tests analyze_test() with a file this isn't html (this one!) """

        # Try to analyze the script file we're testing (how meta)
        test_parser_py_file = os.path.join(os.path.dirname(__file__), 'test_parser.py')

        # Analyze the file
        parser = TestParser(options, test_parser_py_file)
        test_info = parser.analyze_test()

        # Verify the correct test info was found
        self.assertEqual(test_info, None, 'no tests should have been found in this file')
