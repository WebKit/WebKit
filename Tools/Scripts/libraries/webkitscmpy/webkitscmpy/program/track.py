# Copyright (C) 2022 Apple Inc. All rights reserved.
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

import re
import sys

from .command import Command
from webkitcorepy import arguments, run
from webkitscmpy import log, local


class Track(Command):
    name = 'track'
    help = 'Track a specific remote branch (or branches) in your local checkout'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='+',
            type=str, default=None,
            help='String representation(s) of a branch to be tracked locally',
        )
        parser.add_argument(
            '--fetch', '--no-fetch', '-f',
            dest='fetch', default=None,
            help='Fetch canonical remotes before determining which remote defines a branch',
            action=arguments.NoAction,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only 'track' branches in a git checkout\n")
            return 1

        code = 0
        if 'all' in args.arguments:
            args.fetch = True if args.fetch is None else args.fetch

        if args.fetch:
            for remote in repository.source_remotes():
                log.info("Fetching '{}'...".format(remote))
                result = run(
                    [repository.executable(), 'fetch', remote, '--prune'],
                    capture_output=True, cwd=repository.root_path, encoding='utf-8',
                )
                code += result.returncode
                if result.returncode:
                    sys.stderr.write(result.stderr)

        if 'all' in args.arguments:
            args.arguments.remove('all')
            branches = set()
            for remote in repository.source_remotes():
                branches_for = repository.branches_for(remote=remote)
                log.info("Adding {} branches from '{}'...".format(len(branches_for), remote))
                for branch in branches_for:
                    branches.add(branch)
            args.arguments += list(branches)

        for arg in args.arguments:
            remote = repository.remote_for(arg)
            if not remote:
                sys.stderr.write("No remote for '{}' found\n".format(arg))
                continue
            print("Tracking '{}' via 'remotes/{}'".format(arg, remote))
            result = run([
                repository.executable(), 'branch',
                '--set-upstream-to' if arg in repository.branches_for(remote=False) else '--track',
                arg, '{}/{}'.format(remote, arg),
            ], capture_output=True, encoding='utf-8', cwd=repository.root_path)
            code += result.returncode
            if result.returncode:
                sys.stderr.write(result.stderr)

        return code
