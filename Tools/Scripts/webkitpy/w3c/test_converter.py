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

import json
import logging
import re

from webkitpy.common.host import Host
from webkitpy.common.webkit_finder import WebKitFinder
from HTMLParser import HTMLParser

_log = logging.getLogger(__name__)


def convert_for_webkit(new_path, filename, reference_support_info, host=Host(), convert_test_harness_links=True, webkit_test_runner_options=''):
    """ Converts a file's |contents| so it will function correctly in its |new_path| in Webkit.

    Returns the list of modified properties and the modified text if the file was modifed, None otherwise."""
    contents = host.filesystem.read_binary_file(filename)
    converter = _W3CTestConverter(new_path, filename, reference_support_info, host, convert_test_harness_links, webkit_test_runner_options)
    if filename.endswith('.css'):
        return converter.add_webkit_prefix_to_unprefixed_properties_and_values(contents)
    else:
        converter.feed(contents)
        converter.close()
        return converter.output()


class _W3CTestConverter(HTMLParser):
    def __init__(self, new_path, filename, reference_support_info, host=Host(), convert_test_harness_links=True, webkit_test_runner_options=''):
        HTMLParser.__init__(self)

        self._host = host
        self._filesystem = self._host.filesystem
        self._webkit_root = WebKitFinder(self._filesystem).webkit_base()

        self.converted_data = []
        self.converted_properties = []
        self.converted_property_values = []
        self.in_style_tag = False
        self.style_data = []
        self.filename = filename
        self.reference_support_info = reference_support_info
        self.webkit_test_runner_options = webkit_test_runner_options
        self.has_started = False

        resources_path = self.path_from_webkit_root('LayoutTests', 'resources')
        resources_relpath = self._filesystem.relpath(resources_path, new_path)
        self.new_test_harness_path = resources_relpath
        self.convert_test_harness_links = convert_test_harness_links

        # These settings might vary between WebKit and Blink
        css_property_file = self.path_from_webkit_root('Source', 'WebCore', 'css', 'CSSProperties.json')
        css_property_value_file = self.path_from_webkit_root('Source', 'WebCore', 'css', 'CSSValueKeywords.in')

        self.test_harness_re = re.compile('/resources/testharness')

        self.prefixed_properties = self.read_webkit_prefixed_css_property_list(css_property_file)
        prop_regex = '([\s{]|^)(' + "|".join(prop.replace('-webkit-', '') for prop in self.prefixed_properties) + ')(\s+:|:)'
        self.prop_re = re.compile(prop_regex)

        self.prefixed_property_values = self.legacy_read_webkit_prefixed_css_property_list(css_property_value_file)
        prop_value_regex = '(:\s*|^\s*)(' + "|".join(value.replace('-webkit-', '') for value in self.prefixed_property_values) + ')(\s*;|\s*}|\s*$)'
        self.prop_value_re = re.compile(prop_value_regex)

    def output(self):
        return (self.converted_properties, self.converted_property_values, ''.join(self.converted_data))

    def path_from_webkit_root(self, *comps):
        return self._filesystem.abspath(self._filesystem.join(self._webkit_root, *comps))

    def read_webkit_prefixed_css_property_list(self, file_name):
        contents = self._filesystem.read_text_file(file_name)
        if not contents:
            return []
        properties = json.loads(contents)['properties']
        property_names = []
        for property_name, property_dict in properties.iteritems():
            property_names.append(property_name)
            if 'codegen-properties' in property_dict:
                codegen_options = property_dict['codegen-properties']
                if 'aliases' in codegen_options:
                    property_names.extend(codegen_options['aliases'])

        prefixed_properties = []
        unprefixed_properties = set()
        for property_name in property_names:
            # Find properties starting with the -webkit- prefix.
            match = re.match('-webkit-([\w|-]*)', property_name)
            if match:
                prefixed_properties.append(match.group(1))
            else:
                unprefixed_properties.add(property_name)

        # Ignore any prefixed properties for which an unprefixed version is supported
        return [prop for prop in prefixed_properties if prop not in unprefixed_properties]

    def legacy_read_webkit_prefixed_css_property_list(self, file_name):
        contents = self._filesystem.read_text_file(file_name)
        prefixed_properties = []
        unprefixed_properties = set()

        for line in contents.splitlines():
            if re.match('^(#|//)', line) or len(line.strip()) == 0:
                # skip comments and preprocessor directives and empty lines.
                continue
            # Property name is always first on the line.
            property_name = line.split(' ', 1)[0]
            # Find properties starting with the -webkit- prefix.
            match = re.match('-webkit-([\w|-]*)', property_name)
            if match:
                prefixed_properties.append(match.group(1))
            else:
                unprefixed_properties.add(property_name.strip())

        # Ignore any prefixed properties for which an unprefixed version is supported
        return [prop for prop in prefixed_properties if prop not in unprefixed_properties]

    def add_webkit_prefix_to_unprefixed_properties_and_values(self, text):
        """ Searches |text| for instances of properties and values requiring the -webkit- prefix and adds the prefix to them.

        Returns the list of converted properties, values and the modified text."""

        converted_properties = self.add_webkit_prefix_following_regex(text, self.prop_re)
        converted_property_values = self.add_webkit_prefix_following_regex(converted_properties[1], self.prop_value_re)

        # FIXME: Handle the JS versions of these properties and values and GetComputedStyle, too.
        return (converted_properties[0], converted_property_values[0], converted_property_values[1])

    def add_webkit_prefix_following_regex(self, text, regex):
        converted_list = set()
        text_chunks = []
        cur_pos = 0
        for m in regex.finditer(text):
            text_chunks.extend([text[cur_pos:m.start()], m.group(1), '-webkit-', m.group(2), m.group(3)])
            converted_list.add(m.group(2))
            cur_pos = m.end()
        text_chunks.append(text[cur_pos:])

        for item in converted_list:
            _log.info('  converting %s', item)

        return (converted_list, ''.join(text_chunks))

    def convert_reference_relpaths(self, text):
        """ Searches |text| for instances of files in reference_support_info and updates the relative path to be correct for the new ref file location"""
        converted = text
        for path in self.reference_support_info['files']:
            if text.find(path) != -1:
                # FIXME: This doesn't handle an edge case where simply removing the relative path doesn't work.
                # See http://webkit.org/b/135677 for details.
                new_path = re.sub(self.reference_support_info['reference_relpath'], '', path, 1)
                converted = re.sub(path, new_path, text)

        return converted

    def convert_style_data(self, data):
        converted = self.add_webkit_prefix_to_unprefixed_properties_and_values(data)
        if converted[0]:
            self.converted_properties.extend(list(converted[0]))
        if converted[1]:
            self.converted_property_values.extend(list(converted[1]))

        if self.reference_support_info is None or self.reference_support_info == {}:
            return converted[2]

        return self.convert_reference_relpaths(converted[2])

    def convert_attributes_if_needed(self, tag, attrs):
        converted = self.get_starttag_text()
        if self.convert_test_harness_links and tag in ('script', 'link'):
            attr_name = 'src'
            if tag != 'script':
                attr_name = 'href'
            for attr in attrs:
                if attr[0] == attr_name:
                    new_path = re.sub(self.test_harness_re, self.new_test_harness_path + '/testharness', attr[1])
                    converted = re.sub(re.escape(attr[1]), new_path, converted)

        for attr in attrs:
            if attr[0] == 'style':
                new_style = self.convert_style_data(attr[1])
                converted = re.sub(re.escape(attr[1]), new_style, converted)

        src_tags = ('script', 'style', 'img', 'frame', 'iframe', 'input', 'layer', 'textarea', 'video', 'audio')
        if tag in src_tags and self.reference_support_info is not None and  self.reference_support_info != {}:
            for attr_name, attr_value in attrs:
                if attr_name == 'src':
                    new_path = self.convert_reference_relpaths(attr_value)
                    converted = re.sub(re.escape(attr_value), new_path, converted)

        self.converted_data.append(converted)

    def add_webkit_test_runner_options_if_needed(self):
        if self.has_started:
            return
        self.has_started = True
        if self.webkit_test_runner_options:
            self.converted_data[-1] = self.converted_data[-1] + self.webkit_test_runner_options

    def handle_starttag(self, tag, attrs):
        if tag == 'style':
            self.in_style_tag = True
        self.convert_attributes_if_needed(tag, attrs)
        self.add_webkit_test_runner_options_if_needed()

    def handle_endtag(self, tag):
        if tag == 'style':
            self.converted_data.append(self.convert_style_data(''.join(self.style_data)))
            self.in_style_tag = False
            self.style_data = []
        self.converted_data.extend(['</', tag, '>'])

    def handle_startendtag(self, tag, attrs):
        self.convert_attributes_if_needed(tag, attrs)

    def handle_data(self, data):
        if self.in_style_tag:
            self.style_data.append(data)
        else:
            self.converted_data.append(data)

    def handle_entityref(self, name):
        self.converted_data.extend(['&', name, ';'])

    def handle_charref(self, name):
        self.converted_data.extend(['&#', name, ';'])

    def handle_comment(self, data):
        self.converted_data.extend(['<!-- ', data, ' -->'])
        self.add_webkit_test_runner_options_if_needed()

    def handle_decl(self, decl):
        self.converted_data.extend(['<!', decl, '>'])
        self.add_webkit_test_runner_options_if_needed()

    def handle_pi(self, data):
        self.converted_data.extend(['<?', data, '>'])
        self.add_webkit_test_runner_options_if_needed()
