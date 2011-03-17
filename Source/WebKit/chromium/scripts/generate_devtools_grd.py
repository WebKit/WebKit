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

"""Creates a grd file for packaging the inspector files."""

from __future__ import with_statement

import errno
import os
import shutil
import sys
from xml.dom import minidom

kDevToolsResourcePrefix = 'IDR_DEVTOOLS_'
kGrdTemplate = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="0" current_release="1">
  <outputs>
    <output filename="grit/devtools_resources.h" type="rc_header">
      <emit emit_type='prepend'></emit>
    </output>
    <output filename="grit/devtools_resources_map.cc" type="resource_file_map_source" />
    <output filename="grit/devtools_resources_map.h" type="resource_map_header" />

    <output filename="devtools_resources.pak" type="data_package" />
  </outputs>
  <release seq="1">
    <includes></includes>
  </release>
</grit>
'''


class ParsedArgs:
    def __init__(self, source_files, image_dirs, output_filename):
        self.source_files = source_files
        self.image_dirs = image_dirs
        self.output_filename = output_filename


def parse_args(argv):
    images_position = argv.index('--images')
    output_position = argv.index('--output')
    source_files = argv[:images_position]
    image_dirs = argv[images_position + 1:output_position]
    return ParsedArgs(source_files, image_dirs, argv[output_position + 1])


def make_name_from_filename(filename):
    return (filename.replace('/', '_')
                    .replace('\\', '_')
                    .replace('.', '_')).upper()


def add_file_to_grd(grd_doc, filename):
    includes_node = grd_doc.getElementsByTagName('includes')[0]
    includes_node.appendChild(grd_doc.createTextNode('\n      '))

    new_include_node = grd_doc.createElement('include')
    new_include_node.setAttribute('name', make_name_from_filename(filename))
    new_include_node.setAttribute('file', filename)
    new_include_node.setAttribute('type', 'BINDATA')
    includes_node.appendChild(new_include_node)


def main(argv):
    parsed_args = parse_args(argv[1:])

    doc = minidom.parseString(kGrdTemplate)
    output_directory = os.path.dirname(parsed_args.output_filename)

    try:
        os.makedirs(os.path.join(output_directory, 'Images'))
    except OSError, e:
        if e.errno != errno.EEXIST:
            raise e

    for filename in parsed_args.source_files:
        shutil.copy(filename, output_directory)
        add_file_to_grd(doc, os.path.basename(filename))

    for dirname in parsed_args.image_dirs:
        for filename in os.listdir(dirname):
            if not filename.endswith('.png') and not filename.endswith('.gif'):
                continue
            shutil.copy(os.path.join(dirname, filename),
                        os.path.join(output_directory, 'Images'))
            add_file_to_grd(doc, os.path.join('Images', filename))

    with open(parsed_args.output_filename, 'w') as output_file:
        output_file.write(doc.toxml(encoding='UTF-8'))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
