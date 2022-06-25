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

VALID_EXTENSIONS = ['.svg', '.png']


def get_filenames(directory):
    filenames = []
    files = os.listdir(directory)
    files.sort()
    for filename in files:
        extension = os.path.splitext(filename)[1]
        if extension not in VALID_EXTENSIONS:
            continue
        filenames.append((os.path.join(directory, filename), filename, extension[1:]))

    return filenames


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate a GResources file for the modern media controls.')
    parser.add_argument('--input', type=str,
                        help='the input directory')
    parser.add_argument('--output', nargs='?', type=argparse.FileType('w'), default=sys.stdout,
                        help='the output file')

    args = parser.parse_args(sys.argv[1:])

    args.output.write("""<?xml version=1.0 encoding=UTF-8?>
<gresources>
    <gresource prefix="/org/webkit/media-controls">
""")

    indent = ' ' * 4 * 2
    for (path, alias, extension) in get_filenames(args.input):
        extra_attributes = ""
        if extension == "svg":
            extra_attributes = 'preprocess="xml-stripblanks"'
        args.output.write(f'{indent}<file compressed="true" {extra_attributes} alias="{alias}">{path}</file>\n')

    args.output.write("""    </gresource>
</gresources>
""")
