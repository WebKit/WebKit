#!/usr/bin/env python
# Copyright (C) 2013 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

import common
import errno
import os
import re
import sys

from ConfigParser import SafeConfigParser


class WebKitDOMDocGenerator(object):

    def __init__(self, symbol_files, file_handle):
        self._symbol_files = symbol_files
        self._file_handle = file_handle

    def write_header(self):
        pass

    def write_section(self, symbol_file):
        raise NotImplementedError

    def write_footer(self):
        pass

    def write(self, string):
        self._file_handle.write(string)

    def generate(self):
        self.write_header()
        for symbol_file in self._symbol_files:
            self.write_section(symbol_file)
        self.write_footer()


class WebKitDOMDocGeneratorSGML(WebKitDOMDocGenerator):
    def write_header(self):
        self.write('''<?xml version="1.0"?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.1.2//EN"
               "http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd" [
<!ENTITY version SYSTEM "version.xml">
]>
<book id="index" xmlns:xi="http://www.w3.org/2003/XInclude">
  <bookinfo>
    <title>WebKitDOMGTK+ Reference Manual</title>
    <releaseinfo>for WebKitDOMGTK+ &version;</releaseinfo>
  </bookinfo>

  <chapter>
    <title>Class Overview</title>
''')

    def write_section(self, symbol_file):
        basename = os.path.basename(symbol_file)
        self.write('    <xi:include href="xml/%s"/>\n' % basename.replace(".symbols", ".xml"))

    def write_footer(self):
        self.write('''  </chapter>

  <index id="index-all">
    <title>Index</title>
    <xi:include href="xml/api-index-full.xml"><xi:fallback /></xi:include>
  </index>

  <xi:include href="xml/annotation-glossary.xml"><xi:fallback /></xi:include>
</book>
''')


class WebKitDOMDocGeneratorSections(WebKitDOMDocGenerator):
    def __init__(self, symbol_files, file_handle):
        super(WebKitDOMDocGeneratorSections, self).__init__(symbol_files, file_handle)

        self._first_decamelize_re = re.compile('(.)([A-Z][a-z]+)')
        self._second_decamelize_re = re.compile('([a-z0-9])([A-Z])')
        self._dom_class_re = re.compile('(^WebKitDOM)(.+)$')
        self._function_re = re.compile('^.+ (.+)\((.+)\)$')
        self._constant_re = re.compile('^[A-Z_]+$')

    def _dom_class(self, class_name):
        return self._dom_class_re.sub(r'\2', class_name)

    def _dom_class_decamelize(self, class_name):
        s1 = self._first_decamelize_re.sub(r'\1_\2', self._dom_class(class_name))
        retval = self._second_decamelize_re.sub(r'\1_\2', s1)

        # Fix some exceptions.
        retval = retval.replace('Web_Kit', 'WebKit')
        retval = retval.replace('X_Path', 'XPath')
        retval = retval.replace('HTMLI_Frame', 'HTML_IFrame')
        retval = retval.replace('HTMLBR', 'HTML_BR')
        retval = retval.replace('HTMLHR', 'HTML_HR')
        retval = retval.replace('HTMLLI', 'HTML_LI')
        retval = retval.replace('HTMLD', 'HTML_D')
        retval = retval.replace('HTMLO', 'HTML_O')
        retval = retval.replace('HTMLU', 'HTML_U')

        return retval

    def _symbol_list(self, symbol_file):
        retval = []
        f = open(symbol_file, 'r')
        for line in f.readlines():
            match = self._constant_re.match(line)
            if match:
                retval.append(line.strip('\n'))
                continue

            match = self._function_re.match(line)
            if not match or match.group(1).endswith('get_type'):
                continue
            retval.append(match.group(1))

        return retval

    def write_section(self, symbol_file):
        class_name = os.path.basename(symbol_file).replace(".symbols", "")
        is_custom = class_name == 'WebKitDOMCustom'
        is_interface = class_name in ['WebKitDOMEventTarget', 'WebKitDOMNodeFilter', 'WebKitDOMXPathNSResolver']
        is_object = class_name == 'WebKitDOMObject'
        self.write('<SECTION>\n')
        self.write('<FILE>%s</FILE>\n<TITLE>%s</TITLE>\n' % (class_name, class_name))
        if not is_custom:
            self.write('%s\n' % class_name)
        self.write('\n')
        self.write('\n'.join(self._symbol_list(symbol_file)) + '\n')
        if not is_custom:
            self.write('\n<SUBSECTION Standard>\n')
            if is_interface:
                self.write('%sIface\n' % class_name)
            else:
                self.write('%sClass\n' % class_name)
            dom_class = self._dom_class_decamelize(class_name).upper()
            self.write('WEBKIT_DOM_TYPE_%s\n' % dom_class)
            self.write('WEBKIT_DOM_%s\n' % dom_class)
            self.write('WEBKIT_DOM_IS_%s\n' % dom_class)
            self.write('WEBKIT_DOM_%s_CLASS\n' % dom_class)
            if is_interface:
                self.write('WEBKIT_DOM_%s_GET_IFACE\n' % dom_class)
            else:
                self.write('WEBKIT_DOM_IS_%s_CLASS\n' % dom_class)
                self.write('WEBKIT_DOM_%s_GET_CLASS\n' % dom_class)
            self.write('\n<SUBSECTION Private>\n')
            if is_object:
                self.write('%sPrivate\n' % class_name)
            self.write('webkit_dom_%s_get_type\n' % dom_class.lower())
        self.write('</SECTION>\n\n')

    def write_footer(self):
        self.write('<SECTION>\n')
        self.write('<FILE>webkitdomdefines</FILE>\n<TITLE>WebKitDOMDefines</TITLE>\n')
        self.write('<SUBSECTION Private>\n')
        self.write('WEBKIT_API\nWEBKIT_DEPRECATED\nWEBKIT_DEPRECATED_FOR\n')
        self.write('</SECTION>\n\n')


def write_doc_files():
    doc_dir = common.build_path('DerivedSources', 'webkitdom', 'docs')

    try:
        os.mkdir(doc_dir)
    except OSError, e:
        if e.errno != errno.EEXIST or not os.path.isdir(doc_dir):
            sys.stderr.write("Could not create doc dir at %s: %s\n" % (doc_dir, str(e)))
            sys.exit(1)

    with open(os.path.join(doc_dir, 'webkitdomgtk-sections.txt'), 'w') as sections_file:
        generator = WebKitDOMDocGeneratorSections(get_all_webkitdom_symbol_files(), sections_file)
        generator.generate()
    with open(os.path.join(doc_dir, 'webkitdomgtk-docs.sgml'), 'w') as sgml_file:
        generator = WebKitDOMDocGeneratorSGML(get_all_webkitdom_symbol_files(), sgml_file)
        generator.generate()


def header_name_list_from_gtkdoc_config_file():
    config_file = common.build_path('gtkdoc-webkitdom.cfg')
    if not os.path.isfile(config_file):
        sys.stderr.write("Could not find config file at %s\n" % config_file)
        return sys.exit(1)

    config = SafeConfigParser()
    config.read(config_file)
    module_name = config.sections()[0]
    return [os.path.basename(f) for f in str(config.get(module_name, 'headers')).replace(';', ' ').split()]


def get_all_webkitdom_symbol_files():
    static_symbol_files_path = common.top_level_path('Source', 'WebCore', 'bindings', 'gobject')
    generated_symbol_files_path = common.build_path('DerivedSources', 'webkitdom')

    symbol_files = []
    for header_name in header_name_list_from_gtkdoc_config_file():
        # webkitdomdefines.h doesn't have a corresponding symbols file and webkitdom.symbols is a
        # file containing the expected symbols results.
        if header_name in ("webkitdom.h", "webkitdomdefines.h"):
            continue

        symbol_file = header_name.replace(".h", ".symbols")
        path = os.path.join(static_symbol_files_path, symbol_file)
        if os.path.exists(path):
            symbol_files.append(path)
            continue
        path = os.path.join(generated_symbol_files_path, symbol_file)
        if os.path.exists(path):
            symbol_files.append(path)
            continue

    return symbol_files
