#!/usr/bin/env python3

# Copyright (C) 2020, 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys

from webkitscmpy.local import Git


def main(inputfile, identifier_template):
    REPOSITORY_PATH = os.environ.get('OLDPWD')
    GIT_COMMIT = os.environ.get('GIT_COMMIT')

    if not REPOSITORY_PATH:
        sys.stderr.write('Failed to retrieve repository path\n')
        return -1
    if not GIT_COMMIT:
        sys.stderr.write('Failed to retrieve git hash\n')
        return -1

    repository = Git(REPOSITORY_PATH)
    commit = repository.commit(hash=GIT_COMMIT)
    if not commit:
        sys.stderr.write("Failed to find '{}' in the repository".format(GIT_COMMIT))
        return -1
    if not commit.identifier or not commit.branch:
        sys.stderr.write("Failed to compute the identifier for '{}'".format(GIT_COMMIT))
        return -1

    lines = []
    for line in inputfile.readlines():
        lines.append(line.rstrip())

    identifier_index = len(lines)
    if identifier_index and repository.GIT_SVN_REVISION.match(lines[-1]):
        identifier_index -= 1

    # We're trying to cover cases where there is a space between link and git-svn-id:
    #     <commit message content>
    #
    #     Canonical link: ...
    #     git-svn-id: ...
    # OR
    #     <commit message content>
    #     Canonical link: ...
    #
    #     git-svn-id: ...
    if identifier_index and lines[identifier_index - 1].startswith(identifier_template.format('').split(':')[0]):
        lines[identifier_index - 1] = identifier_template.format(commit)
        identifier_index = identifier_index - 2
    elif identifier_index and lines[identifier_index - 2].startswith(identifier_template.format('').split(':')[0]):
        del lines[identifier_index - 2]
        lines.insert(identifier_index - 1, identifier_template.format(commit))
        identifier_index = identifier_index - 2
    else:
        lines.insert(identifier_index, identifier_template.format(commit))

    if lines[identifier_index]:
        lines.insert(identifier_index, '')

    for line in lines:
        print(line)


if __name__ == '__main__':
    sys.exit(main(sys.stdin, sys.argv[1]))
