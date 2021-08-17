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
import subprocess
try:
    from urllib.parse import urlparse  # pylint: disable=E0611
except ImportError:
    from urlparse import urlparse


def get_build_revision():
    revision = "unknown"
    with open(os.devnull, 'w') as devnull:
        gitsvn = os.path.join('.git', 'svn')
        if os.path.isdir(gitsvn) and os.listdir(gitsvn):
            for line in subprocess.check_output(("git", "svn", "info"), stderr=devnull).splitlines():
                parsed = line.split(b':')
                key = parsed[0]
                contents = b':'.join(parsed[1:])
                if key == b'Revision':
                    revision = "r%s" % contents.decode('utf-8').strip()
                    break
        elif os.path.isdir('.git'):
            commit_message = subprocess.check_output(("git", "log", "-1", "--pretty=%B", "origin/HEAD"), stderr=devnull)
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
                    break
        else:
            revision = "r%s" % subprocess.check_output(("svnversion"), stderr=devnull).decode('utf-8').strip()

    return revision

def main(args):
    build_revision = get_build_revision()

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
