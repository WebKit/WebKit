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
import os
import re

from webkitpy.common.host import Host
from webkitpy.common.webkit_finder import WebKitFinder

from html.parser import HTMLParser

_log = logging.getLogger(__name__)


def convert_for_webkit(new_path, filename, reference_support_info, reference_file_renames, host=Host(), webkit_test_runner_options=''):
    """ Converts a file's |contents| so it will function correctly in its |new_path| in Webkit.

    Returns the list of modified properties and the modified text if the file was modifed, None otherwise."""
    contents = host.filesystem.read_text_file(filename)

    converter = _W3CTestConverter(new_path, filename, reference_support_info, reference_file_renames, host, webkit_test_runner_options)
    if filename.endswith('.css'):
        result = converter.add_webkit_prefix_to_unprefixed_properties(contents)
        if not result[0]:
            return None
        return result
    elif filename.endswith(('.html', '.htm', '.xhtml', '.xht', '.xml', '.svg')):
        converter.feed(contents)
        converter.close()
        return converter.output()
    else:
        assert False, f"Unexpected file extension: {filename}"


class _W3CTestConverter(HTMLParser):
    def __init__(self, new_path, filename, reference_support_info, reference_file_renames=[], host=Host(), webkit_test_runner_options=''):
        HTMLParser.__init__(self, convert_charrefs=False)

        self._host = host
        self._filesystem = self._host.filesystem
        self._webkit_root = WebKitFinder(self._filesystem).webkit_base()

        self.converted_data = []
        self.converted_properties = []
        self.in_style_tag = False
        self.filename = filename
        self.reference_support_info = reference_support_info
        self.reference_file_renames = reference_file_renames
        self.webkit_test_runner_options = webkit_test_runner_options
        self.has_started = False
        self._modified = False
        self._source = ''
        self._line_offsets = [0]
        self._pending_source_text = None
        self._copied_up_to = 0
        self._style_start = None

        # These settings might vary between WebKit and Blink
        css_property_file = self.path_from_webkit_root('Source', 'WebCore', 'css', 'CSSProperties.json')

        self.prefixed_properties = self.read_webkit_prefixed_css_property_list(css_property_file)
        prop_regex = r'([\s{]|^)(' + "|".join(prop.replace('-webkit-', '') for prop in self.prefixed_properties) + r')(\s+:|:)'
        self.prop_re = re.compile(prop_regex)

    def output(self):
        if not self._modified:
            return None
        return (self.converted_properties, ''.join(self.converted_data))

    def path_from_webkit_root(self, *comps):
        return self._filesystem.abspath(self._filesystem.join(self._webkit_root, *comps))

    def read_webkit_prefixed_css_property_list(self, file_name):
        contents = self._filesystem.read_text_file(file_name)
        if not contents:
            return []
        properties = json.loads(contents)['properties']
        property_names = []
        for property_name, property_dict in properties.items():
            property_names.append(property_name)
            if 'codegen-properties' in property_dict:
                codegen_options = property_dict['codegen-properties']
                if 'aliases' in codegen_options:
                    property_names.extend(codegen_options['aliases'])

        prefixed_properties = []
        unprefixed_properties = set()
        for property_name in property_names:
            # Find properties starting with the -webkit- prefix.
            match = re.match(r'-webkit-([\w|-]*)', property_name)
            if match:
                prefixed_properties.append(match.group(1))
            else:
                unprefixed_properties.add(property_name)

        # Ignore any prefixed properties for which an unprefixed version is supported
        return [prop for prop in prefixed_properties if prop not in unprefixed_properties]

    def add_webkit_prefix_to_unprefixed_properties(self, text):
        """ Searches |text| for instances of properties requiring the -webkit- prefix and adds the prefix to them.

        Returns the list of converted properties and the modified text."""
        converted_list = set()

        def replace(m):
            converted_list.add(m.group(2))
            return f'{m.group(1)}-webkit-{m.group(2)}{m.group(3)}'

        new_text = self.prop_re.sub(replace, text)

        for item in converted_list:
            _log.info('  converting %s', item)

        return (converted_list, new_text)

    def convert_reference_relpaths(self, text):
        """ Searches |text| for instances of files in reference_support_info and updates the relative path to be correct for the new ref file location"""
        converted = text
        for path in self.reference_support_info['files']:
            if converted.find(path) != -1:
                self._modified = True
                # FIXME: This doesn't handle an edge case where simply removing the relative path doesn't work.
                # See http://webkit.org/b/135677 for details.
                new_path = re.sub(re.escape(self.reference_support_info['reference_relpath']), '', path, 1)
                converted = re.sub(re.escape(path), new_path, converted)

        return converted

    def convert_style_data(self, data):
        converted = self.add_webkit_prefix_to_unprefixed_properties(data)
        if converted[0]:
            self._modified = True
            self.converted_properties.extend(converted[0])

        if self.reference_support_info is None or self.reference_support_info == {}:
            return converted[1]

        return self.convert_reference_relpaths(converted[1])

    def convert_attributes_if_needed(self, tag, attrs):
        converted = self.get_starttag_text()

        for attr in attrs:
            if attr[0] == 'style':
                new_style = self.convert_style_data(attr[1])
                converted = re.sub(re.escape(attr[1]), new_style, converted)
            if attr[0] == 'name' and attr[1] == 'fuzzy' and tag == 'meta':
                for rename in self.reference_file_renames:
                    new_converted = re.sub(rename['src'], rename['dest'], converted)
                    if new_converted != converted:
                        self._modified = True
                        converted = new_converted

        # Convert relative paths
        src_tags = ('script', 'style', 'img', 'frame', 'iframe', 'input', 'layer', 'textarea', 'video', 'audio')
        if self.reference_support_info is not None and self.reference_support_info != {}:
            if tag in src_tags:
                for attr_name, attr_value in attrs:
                    if attr_name == 'src':
                        new_path = self.convert_reference_relpaths(attr_value)
                        converted = re.sub(re.escape(attr_value), new_path, converted)
            if tag == 'link':
                for attr_name, attr_value in attrs:
                    if attr_name == 'href':
                        new_path = self.convert_reference_relpaths(attr_value)
                        converted = re.sub(re.escape(attr_value), new_path, converted)
        self.converted_data.append(converted)

    def add_webkit_test_runner_options_if_needed(self):
        if self.has_started:
            return
        self.has_started = True
        if self.webkit_test_runner_options:
            self._modified = True
            self.converted_data[-1] = self.converted_data[-1] + self.webkit_test_runner_options

    # HTMLParser reports end tags, references, comments, declarations, processing instructions,
    # and style contents normalized, and, depending on the Python version, might not report some
    # malformed or unfinished markup at all. So copy those from the source: getpos() gives where
    # each construct starts, and the next one, or the end of the input, is where it ends.
    def feed(self, data):
        self._line_offsets.extend(len(self._source) + match.end() for match in re.finditer('\n', data))
        self._source += data
        HTMLParser.feed(self, data)

    def close(self):
        HTMLParser.close(self)
        if self.in_style_tag:
            self.converted_data.append(self.convert_style_data(self._source[self._style_start:]))
            self.in_style_tag = False
            self._copied_up_to = len(self._source)
        self._copy_source_text_up_to(len(self._source))

    def _source_position(self):
        line, column = self.getpos()
        return self._line_offsets[line - 1] + column

    def _copy_source_text_up_to(self, position):
        if self._pending_source_text is not None:
            index, start = self._pending_source_text
            self.converted_data[index] = self._source[start:position] + self.converted_data[index]
            self._pending_source_text = None
        elif position > self._copied_up_to and not self.in_style_tag:
            self.converted_data.append(self._source[self._copied_up_to:position])
        self._copied_up_to = max(self._copied_up_to, position)

    def _start_construct(self):
        position = self._source_position()
        self._copy_source_text_up_to(position)
        return position

    def _append_source_text(self, start=None):
        if start is None:
            start = self._start_construct()
        self._pending_source_text = (len(self.converted_data), start)
        self.converted_data.append('')

    def handle_starttag(self, tag, attrs):
        start = self._start_construct()
        self._copied_up_to = start + len(self.get_starttag_text())
        if tag == 'style':
            self.in_style_tag = True
            self._style_start = self._copied_up_to
        self.convert_attributes_if_needed(tag, attrs)
        self.add_webkit_test_runner_options_if_needed()

    def handle_endtag(self, tag):
        start = self._start_construct()
        if tag == 'style' and self.in_style_tag:
            self.converted_data.append(self.convert_style_data(self._source[self._style_start:start]))
            self.in_style_tag = False
        self._append_source_text(start)

    def handle_startendtag(self, tag, attrs):
        start = self._start_construct()
        self._copied_up_to = start + len(self.get_starttag_text())
        self.convert_attributes_if_needed(tag, attrs)

    def handle_data(self, data):
        start = self._start_construct()
        if not self.in_style_tag:
            self.converted_data.append(data)
            self._copied_up_to = start + len(data)

    def handle_entityref(self, name):
        self._append_source_text()

    def handle_charref(self, name):
        self._append_source_text()

    def handle_comment(self, data):
        self._append_source_text()
        self.add_webkit_test_runner_options_if_needed()

    def handle_decl(self, decl):
        self._append_source_text()
        self.add_webkit_test_runner_options_if_needed()

    def unknown_decl(self, data):
        self._append_source_text()

    def handle_pi(self, data):
        self._append_source_text()
        self.add_webkit_test_runner_options_if_needed()
