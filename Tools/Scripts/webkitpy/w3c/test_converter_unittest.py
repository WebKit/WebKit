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
from random import randint
import re
import unittest2 as unittest

from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup

from webkitpy.w3c.test_converter import TestConverter
from webkitpy.w3c import test_converter


class TestConverterTest(unittest.TestCase):

    def testLoadPrefixedPropList(self):
        """ Tests that the current list of properties requiring the -webkit- prefix load correctly """

        # Get the full list of prefixed properties
        converter = TestConverter()
        prop_list = converter.prefixed_properties

        # Verify we got properties back
        self.assertGreater(len(prop_list), 0, 'No prefixed properties found')

        # Verify they're all prefixed
        for prop in prop_list:
            self.assertTrue(prop.startswith('-webkit-'))

    def test_convertForWebkitNothingToConvert(self):
        """ Tests convert_for_webkit() using a basic test that has nothing to convert """

        test_html = """<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>CSS Test: DESCRIPTION OF TEST</title>
<link rel="author" title="NAME_OF_AUTHOR"
href="mailto:EMAIL OR http://CONTACT_PAGE"/>
<link rel="help" href="RELEVANT_SPEC_SECTION"/>
<meta name="assert" content="TEST ASSERTION"/>
<style type="text/css"><![CDATA[
CSS FOR TEST
]]></style>
</head>
<body>
CONTENT OF TEST
</body>
</html>
"""
        converter = TestConverter()

        # Try to convert the html
        converted = converter.convert_for_webkit('/nothing/to/convert', contents=test_html, filename='somefile.html')

        # Verify nothing was converted
        self.assertEqual(converted, None, 'test was should not have been converted')

    def test_convertForWebkitHarnessOnly(self):
        """ Tests convert_for_webkit() using a basic JS test that uses testharness.js only and has no prefixed properties """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        converter = TestConverter()

        # Create path to a fake test directory
        test_path = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                              test_converter.LAYOUT_TESTS_DIRECTORY,
                                              test_converter.CSS_SUBDIR,
                                              'harnessonly')

        # Convert the html
        converted = converter.convert_for_webkit(test_path, contents=test_html, filename='somefile.html')

        # Verify a conversion happened
        self.assertNotEqual(converted, None, 'test was not converted')

        # Verify that both the harness paths were converted correctly
        self.verifyTestHarnessPaths(converted[1], test_path, 1, 1)

        # Verify no properties were converted
        self.verifyPrefixedProperties(converted, [])

    def test_convertForWebkitPropsOnly(self):
        """ Tests convert_for_webkit() using a test that has 2 prefixed properties: 1 in a style block + 1 inline style """

        test_html = """<html>
<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<style type="text/css">

#block1 { @test0@: propvalue; }

</style>
</head>
<body>
<div id="elem1" style="@test1@: propvalue;"></div>
</body>
</html>
"""

        converter = TestConverter()

        # Create path to a fake test directory
        test_path = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                              test_converter.LAYOUT_TESTS_DIRECTORY,
                                              test_converter.CSS_SUBDIR,
                                              'harnessandprops')

        # Generate & insert the test properties into the test_html
        test_content = self.generateTestContent(converter.prefixed_properties, 2, test_html)

        # Convert the html
        converted = converter.convert_for_webkit(test_path, contents=test_content[1], filename='somefile.html')

        # Verify a conversion happened
        self.assertNotEqual(converted, None, 'test was not converted')

        # Verify that both the harness paths and properties were all converted correctly
        self.verifyTestHarnessPaths(converted[1], test_path, 1, 1)
        self.verifyPrefixedProperties(converted, test_content[0])

    def test_convertForWebkitHarnessAndProps(self):
        """ Tests convert_for_webkit() using a basic JS test that uses testharness.js and testharness.css and has 4 prefixed properties: 3 in a style block + 1 inline style """

        test_html = """<html>
<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<style type="text/css">

#block1 { @test0@: propvalue; }
#block2 { @test1@: propvalue; }
#block3 { @test2@: propvalue; }

</style>
</head>
<body>
<div id="elem1" style="@test3@: propvalue;"></div>
</body>
</html>
"""
        converter = TestConverter()

        # Create path to a fake test directory
        test_path = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                              test_converter.LAYOUT_TESTS_DIRECTORY,
                                              test_converter.CSS_SUBDIR,
                                              'harnessandprops')

        # Generate & insert the test properties into the test_html
        test_content = self.generateTestContent(converter.prefixed_properties, 4, test_html)

        # Convert the html
        converted = converter.convert_for_webkit(test_path, contents=test_content[1], filename='somefile.html')

        # Verify a conversion happened
        self.assertNotEqual(converted, None, 'test was not converted')

        # Verify that both the harness paths and properties were all converted correctly
        self.verifyTestHarnessPaths(converted[1], test_path, 1, 1)
        self.verifyPrefixedProperties(converted, test_content[0])

    def test_convertTestHarnessPaths(self):
        """ Tests convert_testharness_paths() with a test that uses all three testharness files """

        # Basic test content using all three testharness files
        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
"""
        converter = TestConverter()

        # Create path to a fake test directory
        test_path = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                              test_converter.LAYOUT_TESTS_DIRECTORY,
                                              test_converter.CSS_SUBDIR,
                                              'testharnesspaths')

        # Convert the html
        doc = BeautifulSoup(test_html)
        converted = converter.convert_testharness_paths(doc, test_path)

        # Verify a conversion happened
        self.assertTrue(converted, 'test was not converted')

        # Verify all got converted correctly
        self.verifyTestHarnessPaths(doc, test_path, 2, 1)

    def test_convertPrefixedProperties(self):
        """ Tests convert_prefixed_properties() file that has 20 properties requiring the -webkit- prefix:
        10 in one style block + 5 in another style
        block + 5 inline styles, including one with multiple prefixed properties.
        The properties in the test content are in all sorts of wack formatting.
        """

        test_html = """<html>
<style type="text/css"><![CDATA[

.block1 {
    width: 300px;
    height: 300px
}

.block2 {
    @test0@: propvalue;
}

.block3{@test1@: propvalue;}

.block4 { @test2@:propvalue; }

.block5{ @test3@ :propvalue; }

#block6 {    @test4@   :   propvalue;  }

#block7
{
    @test5@: propvalue;
}

#block8 { @test6@: propvalue; }

#block9:pseudo
{

    @test7@: propvalue;
    @test8@:  propvalue propvalue propvalue;;
}

]]></style>
</head>
<body>
    <div id="elem1" style="@test9@: propvalue;"></div>
    <div id="elem2" style="propname: propvalue; @test10@ : propvalue; propname:propvalue;"></div>
    <div id="elem2" style="@test11@: propvalue; @test12@ : propvalue; @test13@   :propvalue;"></div>
    <div id="elem3" style="@test14@:propvalue"></div>
</body>
<style type="text/css"><![CDATA[

.block10{ @test15@: propvalue; }
.block11{ @test16@: propvalue; }
.block12{ @test17@: propvalue; }
#block13:pseudo
{
    @test18@: propvalue;
    @test19@: propvalue;
}

]]></style>
</html>
"""
        converter = TestConverter()

        # Generate & insert the test properties into the test_html
        test_content = self.generateTestContent(converter.prefixed_properties, 20, test_html)

        # Convert the html
        converted = converter.convert_prefixed_properties(BeautifulSoup(test_content[1]))

        # Verify a conversion happened
        self.assertNotEqual(converted, None, 'test was not converted')

        # Verify all the generated test properties were converted correctly
        self.verifyPrefixedProperties(converted, test_content[0])

    def verifyTestHarnessPaths(self, converted, test_path, num_src_paths, num_href_paths):
        """ Verifies all W3C-style paths to test harness files were converted correctly """

        # Make soup if we need to
        if isinstance(converted, basestring):
            converted = BeautifulSoup(converted)

        # Build the path to LayoutTests/resources
        resources_dir = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                                  test_converter.LAYOUT_TESTS_DIRECTORY,
                                                  test_converter.RESOURCES_DIRECTORY)

        # Verify the original paths are no longer there
        orig_path_pattern = re.compile('\"/resources/testharness')
        self.assertEquals(len(converted.findAll(src=orig_path_pattern)), 0,
                          'testharness src path was not converted')
        self.assertEquals(len(converted.findAll(href=orig_path_pattern)), 0,
                          'testharness href path was not converted')

        # Get the new relpath from the tester dir
        new_relpath = os.path.relpath(resources_dir, test_path)

        # Verify it's in all the right places
        relpath_pattern = re.compile(new_relpath)
        self.assertEquals(len(converted.findAll(src=relpath_pattern)), num_src_paths,
                          'testharness src relative path not correct')
        self.assertEquals(len(converted.findAll(href=relpath_pattern)), num_href_paths,
                          'testharness href relative path not correct')

    def verifyPrefixedProperties(self, converted, test_properties):
        """ Verifies a list of test_properties were converted correctly """

        # Verify that the number of test properties equals the number that were converted
        self.assertEqual(len(converted[0]), len(test_properties), 'Incorrect number of properties converted')

        # Verify they're all in the converted document
        for test_prop in test_properties:
            self.assertTrue((test_prop in converted[1]), 'Property ' + test_prop + ' not found in converted doc')

    def generateTestContent(self, full_prop_list, num_test_properties, html):
        """ Generates a random list of unique properties requiring the -webkit- prefix and inserts them into html content that's been tokenized using \'@testXX@\' syntax """

        test_properties = []

        # Grab a random bunch of 20 unique properties requiring prefixes to test with
        count = 0
        while count < num_test_properties:
            idx = randint(0, len(full_prop_list) - 1)
            if not(full_prop_list[idx] in test_properties):
                test_properties.append(full_prop_list[idx])
                count += 1

        # Replace the tokens in the testhtml with the test properties. Walk backward
        # through the list to replace the double-digit tokens first
        idx = len(test_properties) - 1
        while idx >= 0:
            # Use the unprefixed version
            test_prop = test_properties[idx].replace('-webkit-', '')
            # Replace the token
            html = html.replace('@test' + str(idx) + '@', test_prop)
            idx -= 1

        return (test_properties, html)
