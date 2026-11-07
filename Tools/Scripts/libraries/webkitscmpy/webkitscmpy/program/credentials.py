# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

from __future__ import annotations

import os
import sys
from typing import TYPE_CHECKING, Any

from .command import Command
from webkitcorepy import arguments
from webkitscmpy import local

if TYPE_CHECKING:
    from argparse import Namespace
    from webkitscmpy import ScmBase, remote


class Credentials(Command):
    name = 'credentials'
    help = 'Return https credentials for a repository in a format digestable by git'

    @classmethod
    def main(cls, args: Namespace, repository: local.Git | local.Svn | remote.Scm | None, **kwargs: Any) -> int:
        if not repository:
            sys.stderr.write('No repository defined\n')
            return 1
        if repository.is_svn and not repository.is_git:
            sys.stderr.write('Cannot extract credentials from subversion repository\n')
            return 1

        rmt: ScmBase | None
        if repository.path:
            assert isinstance(repository, local.Scm)  # remote repositories have no path
            rmt = repository.remote()
        else:
            rmt = repository

        username = None
        password = None

        try:
            # Only some remotes (GitHub, BitBucket) provide credentials
            credentials = getattr(rmt, 'credentials', None) if rmt else None
            if credentials:
                username, password = credentials()

            print('username={}'.format(username or ''))
            print('password={}'.format(password or ''))

        except OSError:
            pass

        if not username or not password:
            sys.stderr.write('No username and password found\n')
            sys.stderr.write("Try running '{} setup' to prime the cache with your credentials\n".format(os.path.basename(sys.argv[0])))
            return 1
        return 0
