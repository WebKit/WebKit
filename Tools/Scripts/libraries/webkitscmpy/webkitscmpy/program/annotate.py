# Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

from .branch import Branch
from .command import Command
from webkitcorepy import arguments, run
from webkitscmpy import local


class Annotate(Command):
    name = 'annotate'
    help = 'Add structured Git notes to a commit for cherry-pick tracking'

    @classmethod
    def parser(cls, parser, loggers=None):
        Branch.parser(parser, loggers=loggers)
        parser.add_argument(
            'commit',
            type=str,
            help='Commit hash or identifier to annotate'
        )
        parser.add_argument(
            '--picked-from',
            type=str,
            help='Commit that this commit was picked from (adds Picked-from: note)'
        )
        parser.add_argument(
            '--cherry-picked-from',
            type=str,
            help='Original commit that this commit was cherry-picked from (adds Cherry-picked-from: note)'
        )
        parser.add_argument(
            '--append',
            action='store_true',
            help='Append to existing note instead of replacing it'
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only annotate commits in a local 'git' repository\n")
            return 1

        if not args.picked_from and not args.cherry_picked_from:
            sys.stderr.write('At least one of --picked-from or --cherry-picked-from must be specified\n')
            return 1

        try:
            commit = repository.find(args.commit, include_log=False)
        except (local.Scm.Exception, TypeError, ValueError) as exception:
            sys.stderr.write(str(exception) + '\n')
            return 1

        # Build note content
        notes = []
        if args.picked_from:
            notes.append(f'Picked-from: {args.picked_from}')
        if args.cherry_picked_from:
            notes.append(f'Cherry-picked-from: {args.cherry_picked_from}')
        note_body = '\n'.join(notes) + '\n'

        # Determine if we should append or replace
        command = [
            repository.executable(), 'notes', 'append' if args.append else 'add',
            '-m', note_body, commit.hash,
        ]

        result = run(command, cwd=repository.root_path, capture_output=False)
        return result.returncode
