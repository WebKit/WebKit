# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
from webkitcorepy import arguments
from webkitscmpy import local, log, remote


class Checkout(Command):
    name = 'checkout'
    help = "Given an identifier, revision, hash or pull-request, normalize and checkout that commit." \
           " Pull requests expected in the form 'PR-#'"

    PR_RE = re.compile(r'^\[?[Pp][Rr][ -](?P<number>\d+)]?$')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit, branch or pull request (pr-#) to apply locally',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Specify remote to search for pull request from.',
        )
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
            sys.stderr.write('Cannot checkout on remote repository\n')
            return 1

        target = args.argument[0]
        match = cls.PR_RE.match(target)
        if match:
            rmt = repository.remote(name=args.remote)
            if not rmt:
                sys.stderr.write('Repository does not have associated remote\n')
                return 1
            if not rmt.pull_requests:
                sys.stderr.write('No pull-requests associated with repository\n')
                return 1
            pr = rmt.pull_requests.get(number=int(match.group('number')))
            if not pr:
                sys.stderr.write("Failed to find 'PR-{}' associated with this repository\n".format(match.group('number')))
                return 1

            fork_key = (pr._metadata or {}).get('full_name', pr.author.github)
            if isinstance(rmt, remote.GitHub) and fork_key:
                target = '{}:{}'.format(fork_key, pr.head)
            else:
                target = pr.head
            log.info("Found associated branch '{}' for '{}'".format(target, pr))
        elif args.remote:
            sys.stderr.write('Caller specified --remote, but argument was not a pull request.')
            return 1

        try:
            if isinstance(repository, local.Git):
                commit = repository.checkout(target, prune=args.prune)
            elif args.prune is not None:
                sys.stderr.write("'prune' arguments only valid for 'git' checkouts\n")
                return 1
            else:
                commit = repository.checkout(target)
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        if not commit:
            sys.stderr.write("Failed to checkout '{}'\n".format(args.argument[0]))
            return 1
        return 0
