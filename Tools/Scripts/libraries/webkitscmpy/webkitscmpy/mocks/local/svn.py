# Copyright (C) 2020 Apple Inc. All rights reserved.
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

from webkitcorepy import mocks
from webkitscmpy import local


class Svn(mocks.Subprocess):
    def __init__(self, path='/.invalid-svn', branch=None, remote=None):
        self.path = path
        self.branch = branch or 'trunk'
        self.remote = remote or 'https://svn.mock.org/repository/{}'.format(os.path.basename(path))

        super(Svn, self).__init__(
            mocks.Subprocess.Route(
                local.Svn.executable, 'info',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=
                        'Path: .\n'
                        'Working Copy Root Path: {path}\n'
                        'URL: {remote}/{branch}{directory}\n'
                        'Relative URL: ^/{branch}{directory}\n'
                        'Repository Root: {remote}\n'.format(
                            directory=kwargs.get('cwd', '')[len(self.path):],
                            path=self.path,
                            remote=self.remote,
                            branch=self.branch,
                        ),
                ),
            ), mocks.Subprocess.Route(
                local.Svn.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr="Type 'svn help' for usage.\n",
                )
            ), mocks.Subprocess.Route(
                local.Svn.executable,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=1,
                    stderr="svn: E155007: '{}' is not a working copy\n".format(kwargs.get('cwd')),
                )
            ),
        )
