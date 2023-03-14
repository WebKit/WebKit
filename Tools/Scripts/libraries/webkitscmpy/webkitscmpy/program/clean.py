# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
import re
import sys

from .command import Command
from webkitcorepy import run, Terminal
from webkitscmpy import log, remote


class Clean(Command):
    name = 'clean'
    help = 'Remove all local changes from current checkout or delete local and remote branches associated with a pull-request'

    PR_RE = re.compile(r'^\[?[Pp][Rr][ -](?P<number>\d+)]?$')
    REMOTE_RE = re.compile(r'^remote\.(?P<name>\S+)\.fetch$')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='*',
            type=str, default=None,
            help='String representation(s) of a commit or branch to be cleaned',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Specify remote to search for pull request from.',
        )

    @classmethod
    def cleanup(cls, repository, argument, remote_target=None, verbosity=0):
        if not repository.is_git:
            sys.stderr.write('Can only clean up branches on git repositories\n')
            return 1

        rmt = repository.remote(name=remote_target)
        if isinstance(rmt, remote.GitHub) and remote_target in (None, 'origin'):
            target = 'fork'
        elif isinstance(rmt, remote.GitHub):
            target = '{}-fork'.format(remote_target)
        else:
            target = 'origin'

        match = cls.PR_RE.match(argument)
        if match:
            if not rmt:
                sys.stderr.write("'{}' doesn't have a recognized remote named '{}'\n".format(repository.root_path, remote_target or repository.default_remote))
                return 1
            if not rmt.pull_requests:
                sys.stderr.write("'{}' cannot generate pull-requests\n".format(rmt.url))
                return 1

            pr = rmt.pull_requests.get(int(match.group('number')))
            if not pr:
                sys.stderr.write("No pull request found for '{}'\n".format(argument))
                return 1

            if pr.opened and Terminal.choose("'{arg}' is open, cleaning it up will close the pull request.\nAre you sure you want to close {arg}?".format(arg=argument), default='No') != 'Yes':
                sys.stderr.write("Keeping '{}' because it is still open\n".format(argument))
                return 0

            argument = pr.head

        push_env = os.environ.copy()
        push_env['VERBOSITY'] = str(verbosity)

        did_delete = False
        code = 0
        regex = re.compile(r'^{}-(?P<count>\d+)$'.format(argument))
        for to_delete in repository.branches_for(remote=False):
            if to_delete == argument or regex.match(to_delete):
                code += run([repository.executable(), 'branch', '-D', to_delete], cwd=repository.root_path).returncode
                did_delete = True
        for to_delete in repository.branches_for(remote=target):
            if to_delete == argument or regex.match(to_delete) and target == 'fork':
                code += run([repository.executable(), 'push', target, '--delete', to_delete], cwd=repository.root_path, env=push_env).returncode
                did_delete = True

        if not did_delete:
            sys.stderr.write("No branches matching '{}' were found\n".format(argument))
            return 1

        return code

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write('Cannot clean on remote repository\n')
            return 1

        if not args.arguments:
            return repository.clean()

        result = 0
        for argument in args.arguments:
            result += cls.cleanup(repository, argument, remote_target=args.remote, verbosity=args.verbose)
        return result


class DeletePRBranches(Command):
    name = 'delete-pr-branches'
    help = 'Iterate through all local development branches, find matching PRs and, if those PRs are closed, delete the local branches.'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Specify remote to search for pull request from.',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write('Cannot clean on remote repository "{}"\n'.format(repository.url))
            return 1

        rmt = repository.remote(name=args.remote)
        if not rmt:
            sys.stderr.write("'{}' doesn't have a recognized remote named '{}'\n".format(repository.root_path, args.remote or repository.default_remote))
            return 1
        if not rmt.pull_requests:
            sys.stderr.write("'{}' does not have associated pull-requests\n".format(rmt.url))
            return 1

        result = 0
        for branch in repository.branches_for(remote=False):
            if not repository.dev_branches.match(branch):
                continue
            log.info('Checking {}...'.format(branch))
            prs = list(rmt.pull_requests.find(opened=None, head=branch))
            if not prs:
                continue
            if any([pr.opened for pr in prs]):
                continue
            result += Clean.cleanup(repository, branch, remote_target=args.remote, verbosity=args.verbose)

        for name in repository.config().keys():
            match = Clean.REMOTE_RE.match(name)
            if not match:
                continue
            name = match.group('name')
            log.info('Deleting {}...'.format(name))
            result += run([repository.executable(), 'fetch', name, '-f'], cwd=repository.root_path).returncode

        return result
