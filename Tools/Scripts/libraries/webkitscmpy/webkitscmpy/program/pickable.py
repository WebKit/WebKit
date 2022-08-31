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

import json
import sys
from re import search

from .command import Command
from .find import Info
from datetime import datetime
from webkitcorepy import arguments
from webkitscmpy import Commit, local


class Pickable(Command):
    name = 'pickable'
    help = 'List commits in a range which can be cherry-picked'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit, branch or range of commits to filter for cherry-pickable commits',
        )
        parser.add_argument(
            '--json', '-j',
            help='Convert the commit to a machine-readable JSON object',
            action='store_true',
            dest='json',
            default=False,
        )

    @classmethod
    def pickable(cls, commits):
        filtered_commits = []
        for commit in commits:
            title = commit.message.splitlines()[0]
            if search('Cherry-pick', title) or search('Versioning', title):
                continue
            filtered_commits.append(commit)

        # FIXME: We can eliminate more classes of commits by reasoning about commit relationship
        #    For example, a revert of a non-pickable commit is not itself pickable
        return filtered_commits

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        reference = args.argument[0]
        if reference != repository.default_branch and reference in repository.branches:
            reference = '0@{}..{}'.format(reference, reference)

        try:
            if '..' in reference:
                if '...' in reference:
                    sys.stderr.write("'pickable' sub-command only supports '..' notation\n")
                    return 1
                references = reference.split('..')
                if len(references) > 2:
                    sys.stderr.write('Can only include two references in a range\n')
                    return 1
                commits = [commit for commit in repository.commits(begin=dict(argument=references[0]), end=dict(argument=references[1]))]

            else:
                commits = [repository.find(reference, include_log=True)]

        except (local.Scm.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        commits = cls.pickable(commits)
        if not commits:
            sys.stderr.write("No commits in specified range are 'pickable'\n")
            return 1
        return Info.print_(args, cls.pickable(commits), verbose_default=1)
