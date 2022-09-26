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

import os
import sys
from pathlib import Path
import subprocess
try:
    from urllib.parse import urlparse  # pylint: disable=E0611
except ImportError:
    from urlparse import urlparse

WEBKIT_TOP_LEVEL = Path(__file__).parent.parent.parent.resolve()


def get_revision_from_most_recent_git_commit():
    with open(os.devnull, 'w') as devnull:
        try:
            commit_message = subprocess.check_output(("git", "log", "-1", "--pretty=%B", "origin/HEAD"), stderr=devnull)
        except subprocess.CalledProcessError:
            # This may happen with shallow checkouts whose HEAD has been
            # modified; there is no origin reference anymore, and git
            # will fail - let's pretend that this is not a repo at all
            return None

        # Commit messages tend to be huge and the metadata we're looking
        # for is at the very end. Also a spoofed 'Canonical link' mention
        # could appear early on. So make sure we get the right metadata by
        # reversing the contents. And this is a micro-optimization as well.
        for line in reversed(commit_message.splitlines()):
            parsed = line.split(b':')
            key = parsed[0]
            contents = b':'.join(parsed[1:])
            if key == b'Canonical link':
                url = contents.decode('utf-8').strip()
                revision = urlparse(url).path[1:]  # strip leading /
                return revision
    return None

def get_build_revision():
    git_path = str(WEBKIT_TOP_LEVEL.joinpath('.git'))
    # In case of git worktrees the .git is a file.
    if os.path.isdir(git_path) or os.path.isfile(git_path):
        return get_revision_from_most_recent_git_commit()

    return None

def main(args):
    build_revision = get_build_revision() or "unknown"

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
