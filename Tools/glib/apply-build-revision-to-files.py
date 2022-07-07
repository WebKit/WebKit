#! /usr/bin/env python3
#
# Copyright (C) 2021, 2022 Igalia S.L.
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

import sys
from pathlib import Path

WEBKIT_TOP_LEVEL = Path(__file__).parent.parent.parent.resolve()
sys.path.insert(0, str(Path(WEBKIT_TOP_LEVEL.joinpath("Tools", "Scripts"))))

import webkitpy  # noqa
from webkitscmpy import local  # noqa


def get_build_revision():
    try:
        repository = local.Scm.from_path(str(WEBKIT_TOP_LEVEL), contributors=None)
        return str(repository.find("HEAD", include_log=False))
    except (local.Scm.Exception, TypeError, OSError, KeyError):
        # This may happen with shallow checkouts whose HEAD has been
        # modified; there is no origin reference anymore, and git
        # will fail - let's pretend that this is not a repo at all
        return "unknown"


def main(args):
    build_revision = get_build_revision()

    for in_file in args:
        file = Path(in_file)
        if file.name == "BuildRevision.h":
            with open("Source/WebKit/Shared/glib/BuildRevision.h.in") as template:
                data = template.read()
        elif file.suffix == '.pc':
            # Restore a valid BUILD_REVISION template.
            lines = []
            with file.open() as fd:
                for line in fd.readlines():
                    if line.startswith("revision"):
                        line = "revision=@BUILD_REVISION@\n"
                    lines.append(line)
            data = "".join(lines)
        else:
            print(f"Support for expanding @BUILD_REVISION@ in {file!s} is missing.")
            return 1

        with file.open('w') as fd:
            fd.write(data.replace('@BUILD_REVISION@', build_revision))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
