#!/usr/bin/env python
#
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
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
#

# This script concatenates in place CSS files in the order specified
# using <link> tags in a given 'order.html' file.

from HTMLParser import HTMLParser
from cStringIO import StringIO
import os.path
import sys


class OrderedCSSFilesExtractor(HTMLParser):

    def __init__(self, order_html_name):
        HTMLParser.__init__(self)
        self.ordered_css_files = []
        order_html = open(order_html_name, 'r')
        self.feed(order_html.read())

    def handle_starttag(self, tag, attrs):
        if tag == 'link':
            attrs_dict = dict(attrs)
            if ('type' in attrs_dict and attrs_dict['type'] == 'text/css' and
                'href' in attrs_dict):
                self.ordered_css_files.append(attrs_dict['href'])


class PathExpander:

    def __init__(self, paths):
        self.paths = paths

    def expand(self, filename):
        last_path = None
        expanded_name = None
        for path in self.paths:
            fname = "%s/%s" % (path, filename)
            if (os.access(fname, os.F_OK)):
                if (last_path != None):
                    raise Exception('Ambiguous file %s: found in %s and %s' %
                                    (filename, last_path, path))
                expanded_name = fname
                last_path = path
        return expanded_name


def main(argv):

    if len(argv) < 3:
        print('usage: %s order.html input_source_dir_1 input_source_dir_2 ... '
              'output_file' % argv[0])
        return 1

    output_file_name = argv.pop()
    input_order_file_name = argv[1]
    extractor = OrderedCSSFilesExtractor(input_order_file_name)
    # Unconditionally append devTools.css. It will contain concatenated files.
    extractor.ordered_css_files.append('devTools.css')

    expander = PathExpander(argv[2:])
    output = StringIO()

    for input_file_name in extractor.ordered_css_files:
        full_path = expander.expand(input_file_name)
        if (full_path is None):
            raise Exception('File %s referenced in %s not found on any source paths, '
                            'check source tree for consistency' %
                            (input_file_name, input_order_file_name))
        output.write('/* %s */\n\n' % input_file_name)
        input_file = open(full_path, 'r')
        output.write(input_file.read())
        output.write('\n')
        input_file.close()

    output_file = open(output_file_name, 'w')
    output_file.write(output.getvalue())
    output_file.close()
    output.close()

    # Touch output file directory to make sure that Xcode will copy
    # modified resource files.
    if sys.platform == 'darwin':
        output_dir_name = os.path.dirname(output_file_name)
        os.utime(output_dir_name, None)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
