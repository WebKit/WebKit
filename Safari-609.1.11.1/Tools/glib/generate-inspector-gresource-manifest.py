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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import argparse
import os
import sys

VALID_EXTENSIONS = ['.html', '.js', '.css', '.svg', '.png']
COMPRESSIBLE_EXTENSIONS = ['.html', '.js', '.css', '.svg']
BASE_DIR = 'WebInspectorUI/'


def get_filenames(directory):
    filenames = []

    def should_ignore_resource(resource):
        if resource.startswith(os.path.join('Protocol', 'Legacy')):
            return True
        if os.path.splitext(resource)[1] not in VALID_EXTENSIONS:
            return True

    for root, dirs, files in os.walk(directory):
        dirs.sort()
        files.sort()
        for file in files:
            filename = os.path.join(root, file)
            base_dir_index = filename.rfind(BASE_DIR)
            if base_dir_index == -1:
                continue

            name = filename[base_dir_index + len(BASE_DIR):]
            # The result should use forward slashes, thus make sure any os-specific
            # separator, is properly replaced
            if os.sep != '/':
                name = name.replace(os.sep, '/')
            if not should_ignore_resource(name):
                filenames.append(name)

    return filenames


def is_compressible(filename):
    return os.path.splitext(filename)[1] in COMPRESSIBLE_EXTENSIONS


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a GResources file for the inspector.')
    parser.add_argument('--input', type=str,
                        help='the input directory')
    parser.add_argument('--output', nargs='?', type=argparse.FileType('w'), default=sys.stdout,
                        help='the output file')

    args = parser.parse_args(sys.argv[1:])

    args.output.write(\
    """<?xml version=1.0 encoding=UTF-8?>
    <gresources>
        <gresource prefix="/org/webkit/inspector">
""")

    for filename in get_filenames(args.input):
        line = '            <file'
        if is_compressible(filename):
            line += ' compressed="true"'
        if not filename.startswith('Localization'):
            alias = 'UserInterface/' + filename
            line += ' alias="%s"' % alias
        line += '>%s</file>\n' % filename

        args.output.write(line)

    args.output.write(\
    """    </gresource>
</gresources>
""")
