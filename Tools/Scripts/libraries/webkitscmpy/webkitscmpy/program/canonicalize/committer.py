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
import re
import sys

from webkitcorepy import run
from webkitscmpy import Contributor, local

EMAIL_RE = re.compile(r'(?P<email>[^@]+@[^@]+)(@.*)?')


def canonicalize(name, email, contributors):
    match = EMAIL_RE.match(email)
    if match:
        email = match.group('email')

    contributor = contributors.get(email, contributors.get(email.lower()))
    if contributor:
        return contributor.name, email if match else contributor.email
    contributor = contributors.get(name)
    if contributor:
        return contributor.name, contributor.email
    return name, email


def main(contributor_file):
    REPOSITORY_PATH = os.environ.get('OLDPWD')
    GIT_COMMIT = os.environ.get('GIT_COMMIT')

    with open(contributor_file, 'r') as file:
        contributors = Contributor.Mapping.load(file)

    author, author_email = canonicalize(
        os.environ.get('GIT_AUTHOR_NAME', 'Unknown'),
        os.environ.get('GIT_AUTHOR_EMAIL', 'null'),
        contributors,
    )
    committer, committer_email = canonicalize(
        os.environ.get('GIT_COMMITTER_NAME', 'Unknown'),
        os.environ.get('GIT_COMMITTER_EMAIL', 'null'),
        contributors,
    )

    # Attempt to extract patch-by
    if author_email == committer_email and REPOSITORY_PATH and GIT_COMMIT:
        log = run(
            [local.Git.executable(), 'log', GIT_COMMIT, '-1'],
            cwd=REPOSITORY_PATH, capture_output=True, encoding='utf-8',
        )

        patch_by = re.search(
            r'\s+Patch by (?P<author>[^<]+) \<(?P<email>[^<]+)\>.* on \d+-\d+-\d+',
            log.stdout,
        )
        if patch_by:
            author, author_email = canonicalize(
                patch_by.group('author'),
                patch_by.group('email'),
                contributors,
            )

    print(u'GIT_AUTHOR_NAME {}'.format(author))
    print(u'GIT_AUTHOR_EMAIL {}'.format(author_email))
    print(u'GIT_COMMITTER_NAME {}'.format(committer))
    print(u'GIT_COMMITTER_EMAIL {}'.format(committer_email))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
