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
import glob
import os
import sys

ALLOWED_EXTENSIONS = ['.html', '.js', '.css', '.png', '.svg']
COMPRESSIBLE_EXTENSIONS = ['.html', '.js', '.css', '.svg']


def find_all_files_in_directory(directory):
    directory = os.path.abspath(directory) + os.path.sep
    to_return = []

    def select_file(name):
        return os.path.splitext(name)[1] in ALLOWED_EXTENSIONS

    for root, dirs, files in os.walk(directory):
        files = filter(select_file, files)
        for file_name in files:
            file_name = os.path.abspath(os.path.join(root, file_name))
            to_return.append(file_name.replace(directory, ''))

    return to_return


def is_compressible(filename):
    return os.path.splitext(filename)[1] in COMPRESSIBLE_EXTENSIONS


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a GResources file for the inspector.')
    parser.add_argument('--output', nargs='?', type=argparse.FileType('w'), default=sys.stdout,
                        help='the output file')

    arguments, extra_args = parser.parse_known_args(sys.argv)

    arguments.output.write(\
    """<?xml version=1.0 encoding=UTF-8?>
    <gresources>
        <gresource prefix="/org/webkitgtk/inspector">
""")

    for directory in extra_args[1:]:
        for filename in find_all_files_in_directory(directory):
            line = '            <file'
            if is_compressible(filename):
                line += ' compressed="true"'
            line += '>%s</file>\n' % filename

            arguments.output.write(line)

    arguments.output.write(\
    """    </gresource>
</gresources>
""")

