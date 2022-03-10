#!/usr/bin/env python3
# Copyright (C) 2022 Igalia S.L.
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

VALID_EXTENSIONS = ['.html', '.js', '.css', '.svg', '.png', '.gif', '.cur', '.bcmap', '.properties', '.pfb', '.ttf']
COMPRESSIBLE_EXTENSIONS = ['.html', '.js', '.css', '.svg', '.properties']
BASE_DIRS = ['pdfjs/', 'pdfjs-extras/']


def get_filenames(directory):
    filenames = []

    def should_ignore_resource(resource):
        if os.path.splitext(resource)[1] not in VALID_EXTENSIONS:
            return True

    def resource_name(filename):
        for base_directory in BASE_DIRS:
            base_dir_index = filename.rfind(base_directory)
            if base_dir_index != -1:
                return filename[base_dir_index + len(base_directory):]
        return None

    for root, dirs, files in os.walk(directory):
        dirs.sort()
        files.sort()
        for file in files:
            if os.path.basename(root) == 'cocoa':
                continue
            filename = os.path.join(root, file)
            name = resource_name(filename)
            if name is None:
                continue

            # The result should use forward slashes, thus make sure any os-specific
            # separator is properly replaced.
            if os.sep != '/':
                name = name.replace(os.sep, '/')
            if not should_ignore_resource(name):
                filenames.append(name)

    return filenames


def is_compressible(filename):
    return os.path.splitext(filename)[1] in COMPRESSIBLE_EXTENSIONS


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a GResources file for pdfjs.')
    parser.add_argument('--input', type=str,
                        help='the input directory')
    parser.add_argument('--output', nargs='?', type=argparse.FileType('w'), default=sys.stdout,
                        help='the output file')

    args = parser.parse_args(sys.argv[1:])

    args.output.write("""<?xml version=1.0 encoding=UTF-8?>
<gresources>
    <gresource prefix="/org/webkit/pdfjs">
""")

    for filename in get_filenames(args.input):
        line = '        <file'
        if is_compressible(filename):
            line += ' compressed="true"'
        if not filename.startswith('build/') and not filename.startswith('web/'):
            alias = 'extras/' + filename
            line += ' alias="%s"' % alias
        line += '>%s</file>\n' % filename

        args.output.write(line)

    args.output.write("""    </gresource>
</gresources>
""")
