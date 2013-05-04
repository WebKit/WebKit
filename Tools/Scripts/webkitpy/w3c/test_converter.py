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
import re
import os

from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup, Tag

SOURCE_SUBDIR = 'Source'
WEBCORE_SUBDIR = 'WebCore'
CSS_SUBDIR = 'css'
CSS_PROPERTY_NAMES_IN = 'CSSPropertyNames.in'
LAYOUT_TESTS_DIRECTORY = 'LayoutTests'
RESOURCES_DIRECTORY = 'resources'
WEBKIT_ROOT = __file__.split(os.path.sep + 'Tools')[0]


class TestConverter(object):

    def __init__(self):
        self.prefixed_properties = self.load_prefixed_prop_list()

    def load_prefixed_prop_list(self):
        """ Reads the current list of CSS properties requiring the -webkit- prefix from Source/WebCore/CSS/CSSPropertyNames.in and stores them in an instance variable """

        prefixed_prop_list = []

        # Open CSSPropertyNames.in to get the current list of things requiring prefixes
        f = open(os.path.join(os.path.sep, WEBKIT_ROOT, SOURCE_SUBDIR, WEBCORE_SUBDIR,
                                           CSS_SUBDIR, CSS_PROPERTY_NAMES_IN))
        lines = f.readlines()
        for line in lines:
            # Look for anything with the -webkit- prefix
            match = re.search('^-webkit-[\w|-]*', line)
            if match is not None:
                # Ignore the ones where both the prefixed and non-prefixed property
                # are supported - denoted by -webkit-some-property = some-property
                if len(line.split('=')) == 2 and \
                   line.split('=')[1].strip() in line.split('=')[0].strip():
                    continue
                else:
                    prefixed_prop_list.append(match.group(0))

        return prefixed_prop_list

    def load_file(self, filename):
        """ Opens the test file |filename| to convert """

        f = open(filename)
        contents = f.read()
        f.close()

        return contents

    def convert_for_webkit(self, new_path, contents=None, filename=None):
        """ Converts a file's |contents| so it will function correctly in its |new_path| in Webkit. Returns the list of prefixed CSS properties and the modified contents."""

        if contents is None and filename is None:
            return None

        if contents is None:
            contents = self.load_file(filename)

        # Handle plain old CSS files
        if filename.endswith('.css'):
            return self.scrub_unprefixed_props(contents)

        # Otherwise, parse it as if HTML
        doc = BeautifulSoup(contents)
        jstest = self.convert_testharness_paths(doc, new_path)
        converted = self.convert_prefixed_properties(doc)

        if len(converted[0]) > 0 or jstest is True:
            return converted
        else:
            return None

    def convert_testharness_paths(self, doc, new_path):
        """ Update links to testharness.js in the BeautifulSoup |doc| to point to the copy in |new_path|. Returns whether the document was modified."""

        # Look for the W3C-style path to any testharness files - scripts (.js) or links (.css)
        pattern = re.compile('/resources/testharness')
        script_tags = doc.findAll(src=pattern)
        link_tags = doc.findAll(href=pattern)
        testharness_tags = script_tags + link_tags

        if len(testharness_tags) != 0:

            # Construct the relative path from the new directory to LayoutTests/resources
            resources_path = os.path.join(os.path.sep, WEBKIT_ROOT, LAYOUT_TESTS_DIRECTORY, RESOURCES_DIRECTORY)
            resources_relpath = os.path.relpath(resources_path, new_path)

            for tag in testharness_tags:
                # Build a new tag with the correct path
                attr = 'src'
                if tag.name != 'script':
                    attr = 'href'

                # Build a new tag with the correct relative path
                old_path = tag[attr]
                new_tag = Tag(doc, tag.name, tag.attrs)
                new_tag[attr] = re.sub(pattern, resources_relpath + '/testharness', old_path)

                # Update the doc with the new tag
                self.replace_tag(doc, tag, new_tag)

            return True
        else:
            return False

    def convert_prefixed_properties(self, doc):
        """ Searches a BeautifulSoup |doc| for any CSS properties requiring the -webkit- prefix and converts them. Returns the list of converted properties and the modified document as a string """

        converted_props = []

        # Look for inline and document styles
        inline_styles = doc.findAll(style=re.compile('.*'))
        style_tags = doc.findAll('style')
        all_styles = inline_styles + style_tags

        for tag in all_styles:

            # Get the text whether in a style tag or style attribute
            style_text = ''
            if tag.name == 'style':
                if len(tag.contents) == 0:
                    continue
                else:
                    style_text = tag.contents[0]
            else:
                style_text = tag['style']

            # Scrub it for props requiring prefixes
            scrubbed_style = self.scrub_unprefixed_props(style_text)

            # Rewrite tag only if changes were made
            if len(scrubbed_style[0]) > 0:
                converted_props.extend(scrubbed_style[0])

                # Build a new tag with the modified text
                new_tag = Tag(doc, tag.name, tag.attrs)
                new_tag.insert(0, scrubbed_style[1])

                # Update the doc with the new tag
                self.replace_tag(doc, tag, new_tag)

        return (converted_props, doc.prettify())

    def scrub_unprefixed_props(self, text):
        """ Searches |text| for instances of properties requiring the -webkit- prefix and adds the prefix. Returns the list of converted properties and the modified string """

        converted_props = []

        # Spin through the whole list of prefixed properties
        for prefixed_prop in self.prefixed_properties:

            # Pull the prefix off
            unprefixed_prop = prefixed_prop.replace('-webkit-', '')

            # Look for the various ways it might be in the CSS
            # Match the the property preceded by either whitespace or left curly brace
            # or at the beginning of the string (for inline style attribute)
            pattern = '([\s{]|^)' + unprefixed_prop + '(\s+:|:)'
            if re.search(pattern, text):
                print 'converting ' + unprefixed_prop + ' -> ' + prefixed_prop
                converted_props.append(prefixed_prop)
                text = re.sub(pattern, prefixed_prop + ':', text)

        # TODO: Handle the JS versions of these properties, too.

        return (converted_props, text)

    def replace_tag(self, doc, old_tag, new_tag):
        """ Inserts a |new_tag| into a BeautifulSoup |doc| and removes the |old_tag| """

        # Replace the previous tag with the new one
        index = old_tag.parent.contents.index(old_tag)
        old_tag.parent.insert(index, new_tag)
        old_tag.extract()
