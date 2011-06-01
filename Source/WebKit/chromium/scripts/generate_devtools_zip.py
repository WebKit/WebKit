#!/usr/bin/env python
#
# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""Creates a zip file containing the web inspector files for debugging.

The zip file contains all the .html, .js, .css and image files used by the
web inspector.  The js files and css files are not concatenated and are not
minified."""

from __future__ import with_statement

import os
import StringIO
import sys
import zipfile

import concatenate_css_files
import concatenate_js_files
import generate_devtools_html


class ParsedArgs:
    def __init__(self, inspector_html, devtools_files, search_dirs,
                 image_search_dirs, output_filename):
        self.inspector_html = inspector_html
        self.devtools_files = devtools_files
        self.search_dirs = search_dirs
        self.image_search_dirs = image_search_dirs
        self.output_filename = output_filename


def parse_args(argv):
    inspector_html = argv[0]

    devtools_files_position = argv.index('--devtools-files')
    search_path_position = argv.index('--search-path')
    image_search_path_position = argv.index('--image-search-path')
    output_position = argv.index('--output')

    devtools_files = argv[devtools_files_position + 1:search_path_position]
    search_dirs = argv[search_path_position + 1:image_search_path_position]
    image_search_dirs = argv[image_search_path_position + 1:output_position]

    return ParsedArgs(inspector_html, devtools_files, search_dirs,
                      image_search_dirs, argv[output_position + 1])


def main(argv):
    parsed_args = parse_args(argv[1:])

    devtools_html = StringIO.StringIO()
    with open(parsed_args.inspector_html, 'r') as inspector_file:
        generate_devtools_html.write_devtools_html(
            inspector_file, devtools_html, True, parsed_args.devtools_files)

    zip = zipfile.ZipFile(parsed_args.output_filename, 'w', zipfile.ZIP_DEFLATED)
    zip.writestr("devtools.html", devtools_html.getvalue())

    css_extractor = concatenate_css_files.OrderedCSSFilesExtractor(
        devtools_html.getvalue())
    js_extractor = concatenate_js_files.OrderedJSFilesExtractor(
        devtools_html.getvalue())

    expander = concatenate_css_files.PathExpander(parsed_args.search_dirs)
    files = css_extractor.ordered_css_files + js_extractor.ordered_js_files
    for input_file_name in set(files):
        full_path = expander.expand(input_file_name)
        if full_path is None:
            raise Exception('File %s referenced in %s not found on any source paths, '
                            'check source tree for consistency' %
                            (input_file_name, 'devtools.html'))
        zip.write(full_path, os.path.basename(full_path))

    for dirname in parsed_args.image_search_dirs:
        for filename in os.listdir(dirname):
            if not filename.endswith('.png') and not filename.endswith('.gif'):
                continue
            zip.write(os.path.join(dirname, filename),
                      os.path.join('Images', filename))

    # It would be nice to use the with statement to scope closing the archive,
    # but that wasn't added until python 2.7.
    zip.close()


if '__main__' == __name__:
    sys.exit(main(sys.argv))
