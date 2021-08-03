#!/usr/bin/env python
#
# Copyright (C) 2021 Igalia S.L.
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

import os
import sys

top_level_directory = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(top_level_directory, "Tools", "Scripts"))

from webkitpy.common.checkout.scm.detection import SCMDetector  # nopep8
from webkitpy.common.system.executive import Executive  # nopep8
from webkitpy.common.system.filesystem import FileSystem  # nopep8

def main(args):
    scm = SCMDetector(FileSystem(), Executive()).default_scm()
    svn_revision = scm.head_svn_revision()
    build_revision = "r{}".format(svn_revision)

    for in_file in args:
        filename = os.path.basename(in_file)
        _, extension = os.path.splitext(filename)
        if filename == "BuildRevision.h":
            with open("Source/WebKit/Shared/glib/BuildRevision.h.in") as template:
                data = template.read()
        elif extension == '.pc':
            # Restore a valid BUILD_REVISION template.
            lines = []
            with open(in_file) as fd:
                for line in fd.readlines():
                    if line.startswith("revision"):
                        line = "revision=${BUILD_REVISION}\n"
                    lines.append(line)
            data = "".join(lines)
        else:
            print("Support for expanding $BUILD_REVISION in {} is missing.".format(in_file))
            return 1

        with open(in_file, 'w') as fd:
            fd.write(data.replace('${BUILD_REVISION}', build_revision))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
