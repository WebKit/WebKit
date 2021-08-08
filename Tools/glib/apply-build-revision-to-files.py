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
        with open(in_file) as fd:
            data = fd.read().replace('${BUILD_REVISION}', build_revision)
        with open(in_file, 'w') as fd:
            fd.write(data)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
