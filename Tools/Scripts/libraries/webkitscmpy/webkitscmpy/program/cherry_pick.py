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

import sys

from .branch import Branch
from .command import Command
from webkitbugspy import Tracker, radar
from webkitcorepy import run
from webkitscmpy import local


class CherryPick(Command):
    name = 'cherry-pick'
    help = 'Cherry-pick a commit with an annotated commit message'

    @classmethod
    def parser(cls, parser, loggers=None):
        Branch.parser(parser, loggers=loggers)
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit to be cherry-picked',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only cherry-pick from local 'git' repository\n")
            return 1

        try:
            commit = repository.find(args.argument[0], include_log=True)
        except (local.Scm.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        issue = Tracker.from_string(args.issue) if args.issue else None
        if not issue and commit.issues:
            issue = commit.issues[0]
            if radar.Tracker.radarclient():
                for candidate in commit.issues:
                    if isinstance(candidate.tracker, radar.Tracker):
                        issue = candidate
                        break

        if str(commit) == commit.hash[:commit.HASH_LABEL_SIZE]:
            cherry_pick_string = str(commit)
        else:
            cherry_pick_string = '{} ({})'.format(commit, commit.hash[:commit.HASH_LABEL_SIZE])

        return run(
            [repository.executable(), 'cherry-pick', '-e', commit.hash],
            cwd=repository.root_path,
            env=dict(
                GIT_WEBKIT_CHERRY_PICKED=cherry_pick_string,
                GIT_WEBKIT_BUG=issue.link if issue else '',
            ), capture_output=False,
        ).returncode
