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

import logging
import re
import sys
import os
import re
from unittest import result

from .command import Command
from .branch import Branch

from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log, remote
from ..commit import Commit


class Squash(Command):
    name = 'squash'
    help = 'Combine all commits on the current development branch into a single commit'

    @classmethod
    def parser(cls, parser, loggers=None):
        group = parser.add_mutually_exclusive_group(required=False)
        group.add_argument(
            '--interactive',
            dest='interactive',
            default=False,
            action='store_true',
            help='Use git rebase to interactivly to select what commits you want to squash.'
        )

        group.add_argument(
            '--new-msg', '--new-message', '--no-sub-commit-message',
            default=False,
            dest='no_sub_commit_message',
            action='store_true',
            help='Do not include messages of commits you want squash.'
        )

        parser.add_argument(
            '--base-commit',
            dest='base_commit',
            help='git hash, svn revision or identifier for the base commit that you want to squash to (merged commit will not include this commit)',
            default=None,
        )

    @classmethod
    def get_commits_hashes(cls, repository, base_commit):
        result = run([repository.executable(), 'rev-list', 'HEAD...{}'.format(base_commit.hash)], capture_output=True, cwd=repository.root_path)
        if result.returncode:
            sys.stderr.write(result.stderr)
            sys.stderr.write('Failed to get all commits from HEAD to {}'.format(base_commit.hash))
            return None
        return result.stdout.decode('utf-8').strip().splitlines()

    @classmethod
    def undo_reset(cls, repository):
        return run([repository.executable(), 'reset', "'HEAD@{1}'"], cwd=repository.root_path)

    @classmethod
    def squash_commit(cls, args, repository, branch_point, **kwargs):
        # Make sure we have the commit that user want to revert
        try:
            if args.base_commit:
                base_commit = repository.find(args.base_commit, include_log=True)
                if hasattr(base_commit, 'identifier') and base_commit <= branch_point:
                    sys.stderr.write('It seems you are trying to squash beyond branch point.')
                    return 1
            else:
                base_commit = branch_point
            # Check if there are any outstanding changes:
            if repository.modified():
                sys.stderr.write('Please commit your changes or stash them before you squash to base commit: {}'.format(base_commit))
                return 1
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write('Could not find "{}"'.format(args.base_commit) + '\n')
            return 1

        if args.interactive:
            result = run([repository.executable(), 'rebase', '-i'] + [base_commit.hash], cwd=repository.root_path)
        else:
            previous_history = ''
            commit_hash_list = cls.get_commits_hashes(repository, base_commit)
            if not commit_hash_list:
                return 1
            if not args.no_sub_commit_message:
                commits = map(lambda hash: repository.find(hash, include_log=True), commit_hash_list)
                previous_history += '\n\n'.join(map(lambda commit: commit.message, commits))
            result = run([repository.executable(), 'reset'] + [base_commit.hash], cwd=repository.root_path)
            if result.returncode:
                sys.stderr.write('Failed to merge the diff.')
                cls.undo_reset()
            modified_files = repository.modified()
            if not modified_files:
                sys.stderr.write('Failed to detect any diff to merge.')
                return 1
            for file in modified_files:
                if run([repository.executable(), 'add', file], cwd=repository.root_path).returncode:
                    sys.stderr.write("Failed to add '{}'\n".format(file))
                    return 1
            env = os.environ
            env['COMMIT_MESSAGE_CONTENT'] = '\n\nThis commit include:\n\n' + previous_history

            result = run([repository.executable(), 'commit', '--date=now'], cwd=repository.root_path, env=env)
            log.info('    Squashed {} commits'.format(len(commit_hash_list)))

        if result.returncode:
            sys.stderr.write('Failed to generate squashed commit\n')
            return 1
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        branch_point = repository.branch_point()
        if not branch_point:
            return 1

        return cls.squash_commit(args, repository, branch_point, **kwargs)
