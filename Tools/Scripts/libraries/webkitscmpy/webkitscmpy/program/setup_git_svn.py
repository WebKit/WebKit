# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

from .command import Command
from webkitcorepy import run
from webkitscmpy import remote


class SetupGitSvn(Command):
    name = 'setup-git-svn'
    help = 'Connect a git checkout to the Subversion repository it is (or was) based on'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--all-branches', '-a',
            help='Match all git branches to their subversion counterparts',
            action='store_true',
            dest='all_branches',
            default=False,
        )

    @classmethod
    def main(cls, args, repository, subversion=None, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write('Cannot setup git-svn on remote repository\n')
            return 1
        if not repository.is_git:
            sys.stderr.write('Cannot setup git-svn on Subversion repository\n')
            return 1
        if not subversion:
            sys.stderr.write('Failed to find Subversion remote: {}\n'.format(subversion))
            return 1

        print('Adding svn-remote to git config')
        config_path = os.path.join(repository.root_path, '.git', 'config')
        config_data = []
        with open(config_path, 'r') as config:
            is_in_svn_remote = False
            for line in config.readlines():
                if is_in_svn_remote and not line[:1].isspace():
                    is_in_svn_remote = False
                if line.startswith('[svn-remote "svn"]'):
                    is_in_svn_remote = True
                if not is_in_svn_remote:
                    config_data.append(line)

        with open(config_path, 'w') as config:
            for line in config_data:
                config.write(line)
            config.write('[svn-remote "svn"]\n')
            config.write('\turl = {}\n'.format(subversion))
            config.write('\tfetch = trunk:refs/remotes/origin/{}\n'.format(repository.default_branch))

            if args.all_branches:
                svn_remote = remote.Svn(
                    url=subversion,
                    dev_branches=repository.dev_branches,
                    prod_branches=repository.prod_branches,
                    contributors=repository.contributors,
                )

                git_branches = repository.branches
                for branch in sorted(svn_remote.branches):
                    if branch not in git_branches:
                        continue
                    config.write('\tfetch = branches/{branch}:refs/remotes/origin/{branch}\n'.format(branch=branch))

        print('Populating svn commit mapping (will take a few minutes)...')
        return run([repository.executable(), 'svn', 'fetch', '--log-window-size=5000', '-r', '1:HEAD'], cwd=repository.root_path).returncode
