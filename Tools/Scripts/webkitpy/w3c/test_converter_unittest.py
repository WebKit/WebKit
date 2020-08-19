# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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
import re
import unittest

from webkitcorepy import unicode

from webkitpy.common.host import Host
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup
from webkitpy.w3c.test_converter import _W3CTestConverter

from webkitcorepy import OutputCapture

DUMMY_FILENAME = 'dummy.html'
DUMMY_PATH = 'dummy/testharness/path'


class W3CTestConverterTest(unittest.TestCase):

    # FIXME: When we move to using a MockHost, this method should be removed, since
    #        then we can just pass in a dummy dir path
    def fake_dir_path(self, dirname):
        filesystem = Host().filesystem
        webkit_root = WebKitFinder(filesystem).webkit_base()
        return filesystem.abspath(filesystem.join(webkit_root, "LayoutTests", "css", dirname))

    def test_read_prefixed_property_list(self):
        """ Tests that the current list of properties requiring the -webkit- prefix load correctly """

        # FIXME: We should be passing in a MockHost here ...
        converter = _W3CTestConverter(DUMMY_PATH, DUMMY_FILENAME, None)
        prop_list = converter.prefixed_properties
        self.assertTrue(prop_list, 'No prefixed properties found')
        property_values_list = converter.prefixed_property_values
        self.assertTrue(property_values_list, 'No prefixed property values found')

    def test_convert_for_webkit_nothing_to_convert(self):
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
        converter = _W3CTestConverter(DUMMY_PATH, DUMMY_FILENAME, None)

        with OutputCapture():
            converter.feed(test_html)
            converter.close()
            converted = converter.output()

        self.verify_no_conversion_happened(converted, test_html)

    def test_convert_for_webkit_harness_only(self):
        """ Tests convert_for_webkit() using a basic JS test that uses testharness.js only and has no prefixed properties """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
</head>
"""
        fake_dir_path = self.fake_dir_path("harnessonly")
        converter = _W3CTestConverter(fake_dir_path, DUMMY_FILENAME, None)
        converter.feed(test_html)
        converter.close()
        converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_test_harness_paths(converter, converted[2], fake_dir_path, 1, 1)
        self.verify_prefixed_properties(converted, [])
        self.verify_prefixed_property_values(converted, [])

    def test_convert_for_webkit_properties_only(self):
        """ Tests convert_for_webkit() using a test that has 2 prefixed properties: 1 in a style block + 1 inline style """

        test_html = """<html>
<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<style type="text/css">

#block1 { @test0@: @propvalue0@; }

</style>
</head>
<body>
<div id="elem1" style="@test1@: @propvalue1@;"></div>
</body>
</html>
"""
        fake_dir_path = self.fake_dir_path('harnessandprops')
        converter = _W3CTestConverter(fake_dir_path, DUMMY_FILENAME, None)
        test_content = self.generate_test_content_properties_and_values(converter.prefixed_properties, converter.prefixed_property_values, 1, test_html)

        with OutputCapture():
            converter.feed(test_content[2])
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_test_harness_paths(converter, converted[2], fake_dir_path, 1, 1)
        self.verify_prefixed_properties(converted, test_content[0])
        self.verify_prefixed_property_values(converted, test_content[1])

    def test_convert_for_webkit_harness_and_properties(self):
        """ Tests convert_for_webkit() using a basic JS test that uses testharness.js and testharness.css and has 4 prefixed properties: 3 in a style block + 1 inline style """

        test_html = """<html>
<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<style type="text/css">

#block1 { @test0@: @propvalue0@; }
#block2 { @test1@: @propvalue1@; }
#block3 { @test2@: @propvalue2@; }

</style>
</head>
<body>
<div id="elem1" style="@test3@: @propvalue3@;"></div>
</body>
</html>
"""
        fake_dir_path = self.fake_dir_path('harnessandprops')
        converter = _W3CTestConverter(fake_dir_path, DUMMY_FILENAME, None)

        with OutputCapture():
            test_content = self.generate_test_content_properties_and_values(converter.prefixed_properties, converter.prefixed_property_values, 2, test_html)
            converter.feed(test_content[2])
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_test_harness_paths(converter, converted[2], fake_dir_path, 1, 1)
        self.verify_prefixed_properties(converted, test_content[0])
        self.verify_prefixed_property_values(converted, test_content[1])

    def test_convert_test_harness_paths(self):
        """ Tests convert_testharness_paths() with a test that uses all three testharness files """

        test_html = """<head>
<link href="/resources/testharness.css" rel="stylesheet" type="text/css">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
"""
        fake_dir_path = self.fake_dir_path('testharnesspaths')
        converter = _W3CTestConverter(fake_dir_path, DUMMY_FILENAME, None)

        with OutputCapture():
            converter.feed(test_html)
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_test_harness_paths(converter, converted[2], fake_dir_path, 2, 1)

    def test_convert_prefixed_properties(self):
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
    @test0@: @propvalue0@;
}

.block3{@test1@: @propvalue1@;}

.block4 { @test2@:@propvalue2@; }

.block5{ @test3@ :@propvalue3@; }

#block6 {    @test4@   :   @propvalue4@   ;  }

#block7
{
    @test5@: @propvalue5@;
}

#block8 { @test6@: @propvalue6@ }

#block9:pseudo
{

    @test7@: @propvalue7@;
    @test8@:  propvalue propvalue propvalue;;
    propname:
@propvalue8@;
}

]]></style>
</head>
<body>
    <div id="elem1" style="@test9@: @propvalue9@;"></div>
    <div id="elem2" style="propname: propvalue; @test10@ : @propvalue10@; propname:propvalue;"></div>
    <div id="elem2" style="@test11@: @propvalue11@; @test12@ : @propvalue12@; @test13@   :   @propvalue13@   ;"></div>
    <div id="elem3" style="@test14@:@propvalue14@"></div>
</body>
<style type="text/css"><![CDATA[

.block10{ @test15@: @propvalue15@; }
.block11{ @test16@: @propvalue16@; }
.block12{ @test17@: @propvalue17@; }
#block13:pseudo
{
    @test18@: @propvalue18@;
    @test19@: @propvalue19@;
}

]]></style>
</html>
"""
        converter = _W3CTestConverter(DUMMY_PATH, DUMMY_FILENAME, None)
        test_content = self.generate_test_content_properties_and_values(converter.prefixed_properties, converter.prefixed_property_values, 20, test_html)

        with OutputCapture():
            converter.feed(test_content[2])
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_prefixed_properties(converted, test_content[0])
        self.verify_prefixed_property_values(converted, test_content[1])

    def test_convert_attributes_if_needed(self):
        """ Tests convert_attributes_if_needed() using a reference file that has some relative src paths """

        test_html = """<html>
<head>
<link href="../support/base-style.css">
<video src="resources/video.mkv"></video>
<script src="../../some-script.js"></script>
<style src="../../../some-style.css"></style>
</head>
<body>
<img src="../../../../some-image.jpg">
</body>
</html>
"""
        test_reference_support_info = {'reference_relpath': '../', 'files': ['../../some-script.js', '../../../some-style.css', '../../../../some-image.jpg', '../support/base-style.css', 'resources/video.mkv'],
                                       'elements': ['script', 'style', 'img', 'link', 'video']}
        converter = _W3CTestConverter(DUMMY_PATH, DUMMY_FILENAME, test_reference_support_info)

        with OutputCapture():
            converter.feed(test_html)
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)
        self.verify_reference_relative_paths(converted, test_reference_support_info)

    def test_convert_style_multiple_url(self):
        """ Tests convert_attributes_if_needed() using a reference file that has several relative URL paths in the style """

        test_html = """<html>
<head>
 <style type="text/css">
        .redSquare {
            position: absolute;
            left:50px;
            width: 100px;
            height: 100px;
            background-image:url(../support/yyy.png);
        }
        .greenSquare {
            position: absolute;
            left:50px;
            width: 100px;
            height: 100px;
            background-image:url(../support/yy.png);
        }
        .yellowSquare {
            position: absolute;
            left:50px;
            width: 100px;
            height: 100px;
            background-image:url(../../another/directory/x.png);
        }
        .container {
            position: absolute;
        }
    </style>
</head>
<body>
</body>
</html>
"""
        test_reference_support_info = {'reference_relpath': '../', 'files': ['../support/yyy.png', '../support/yy.png', '../../another/directory/x.png']}
        converter = _W3CTestConverter(DUMMY_PATH, DUMMY_FILENAME, test_reference_support_info)

        with OutputCapture():
            converter.feed(test_html)
            converter.close()
            converted = converter.output()

        self.verify_conversion_happened(converted)

        for path in test_reference_support_info['files']:
            expected_path = re.sub(test_reference_support_info['reference_relpath'], '', path, 1)
            expected_url = 'background-image:url(' + expected_path + ');'
            self.assertTrue(expected_url in converted[2], 'relative path ' + path + ' was not converted correcty')


    def verify_conversion_happened(self, converted):
        self.assertTrue(converted, "conversion didn't happen")

    def verify_no_conversion_happened(self, converted, original):
        self.assertEqual(converted[2], original, 'test should not have been converted')

    def verify_test_harness_paths(self, converter, converted, test_path, num_src_paths, num_href_paths):
        if isinstance(converted, str) or isinstance(converted, unicode):
            converted = BeautifulSoup(converted)

        resources_dir = converter.path_from_webkit_root("LayoutTests", "resources")

        # Verify the original paths are gone, and the new paths are present.
        orig_path_pattern = re.compile('\"/resources/testharness')
        self.assertEquals(len(converted.findAll(src=orig_path_pattern)), 0, 'testharness src path was not converted')
        self.assertEquals(len(converted.findAll(href=orig_path_pattern)), 0, 'testharness href path was not converted')

        new_relpath = os.path.relpath(resources_dir, test_path)
        relpath_pattern = re.compile(new_relpath)
        self.assertEquals(len(converted.findAll(src=relpath_pattern)), num_src_paths, 'testharness src relative path not correct')
        self.assertEquals(len(converted.findAll(href=relpath_pattern)), num_href_paths, 'testharness href relative path not correct')

    def verify_prefixed_properties(self, converted, test_properties):
        self.assertEqual(len(set(converted[0])), len(set(test_properties)), 'Incorrect number of properties converted')
        for test_prop in test_properties:
            self.assertTrue((test_prop in converted[2]), 'Property ' + test_prop + ' not found in converted doc')

    def verify_prefixed_property_values(self, converted, test_property_values):
        self.assertEqual(len(set(converted[1])), len(set(test_property_values)), 'Incorrect number of property values converted ' + str(len(set(converted[1]))) + ' vs ' + str(len(set(test_property_values))))
        for test_value in test_property_values:
            self.assertTrue((test_value in converted[2]), 'Property value ' + test_value + ' not found in converted doc')

    def verify_reference_relative_paths(self, converted, reference_support_info):
        idx = 0
        for path in reference_support_info['files']:
            expected_path = re.sub(re.escape(reference_support_info['reference_relpath']), '', path, 1)
            element = reference_support_info['elements'][idx]
            element_src = 'href' if element == 'link' else 'src'
            expected_tag = '<' + element + ' ' + element_src + '=\"' + expected_path + '\">'
            self.assertTrue(expected_tag in converted[2], 'relative path ' + path + ' was not converted correcty')
            idx += 1

    def generate_test_content_properties_and_values(self, full_property_list, fully_property_values_list, num_test_properties_and_values, html):
        """Inserts properties requiring a -webkit- prefix into the content, replacing \'@testXX@\' with a property and \'@propvalueXX@\' with a value."""
        test_content_properties = self.generate_test_content(full_property_list, num_test_properties_and_values, 'test', html)
        test_content_property_values = self.generate_test_content(fully_property_values_list, num_test_properties_and_values, 'propvalue', test_content_properties[1])
        return (test_content_properties[0], test_content_property_values[0], test_content_property_values[1])

    def generate_test_content(self, full_list, num_test, suffix, html):
        test_list = []
        count = 0
        while count < num_test:
            test_list.append(full_list[count])
            count += 1

        # Replace the tokens in the testhtml with the test properties or values.
        # Walk backward through the list to replace the double-digit tokens first.
        index = len(test_list) - 1
        while index >= 0:
            # Use the unprefixed version
            test = test_list[index].replace('-webkit-', '')
            # Replace the token
            html = html.replace('@' + suffix + str(index) + '@', test)
            index -= 1

        return (test_list, html)
