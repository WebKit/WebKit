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

import sys

from .branch import Branch
from .command import Command
from webkitcorepy import arguments
from webkitscmpy import local


class Pull(Command):
    name = 'pull'
    aliases = ['up', 'update']
    help = 'Update the current checkout, synchronize git-svn if configured'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--prune', '--no-prune',
            dest='prune', default=None,
            help='Prune deleted branches on the tracking remote when fetching',
            action=arguments.NoAction,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write('Cannot update remote repository\n')
            return 1

        if isinstance(repository, local.Git):
            branch_point = Branch.branch_point(repository)
            bp_remotes = set(repository.branches_for(hash=branch_point.hash, remote=None).keys())
            remote = None
            for rmt in repository.source_remotes():
                if rmt in bp_remotes:
                    remote = rmt
                    break
            return repository.pull(rebase=True, branch=branch_point.branch, remote=remote, prune=args.prune)
        if args.prune is not None:
            sys.stderr.write("'prune' arguments only valid for 'git' checkouts\n")
            return 1
        return repository.pull()
