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

COMPRESSIBLE_EXTENSIONS = ['.html', '.js', '.css', '.svg']
BASE_DIR = 'WebInspectorUI/'


def get_filenames(args):
    filenames = []

    for filename in args:
        base_dir_index = filename.rfind(BASE_DIR)
        if base_dir_index != -1:
            filenames.append(filename[base_dir_index + len(BASE_DIR):])
    return filenames


def is_compressible(filename):
    return os.path.splitext(filename)[1] in COMPRESSIBLE_EXTENSIONS


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a GResources file for the inspector.')
    parser.add_argument('--output', nargs='?', type=argparse.FileType('w'), default=sys.stdout,
                        help='the output file')
    parser.add_argument('filenames', metavar='FILES', nargs='+',
                        help='the list of files to include')

    args = parser.parse_args(sys.argv[1:])

    args.output.write(\
"""<?xml version=1.0 encoding=UTF-8?>
<gresources>
    <gresource prefix="/org/webkitgtk/inspector">
""")

    for filename in get_filenames(args.filenames):
        line = '        <file'
        if is_compressible(filename):
            line += ' compressed="true"'
        line += '>%s</file>\n' % filename

        args.output.write(line)

    args.output.write(\
"""    </gresource>
</gresources>
""")

