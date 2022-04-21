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

from .command import Command
from .branch import Branch
from .pull_request import PullRequest

from webkitbugspy import Tracker
from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log, remote


class Revert(Command):
    name = 'revert'
    help = 'Revert provided list of commits and create a pull-request with this revert commit'

    @classmethod
    def parser(cls, parser, loggers=None):
        PullRequest.parser(parser, loggers=loggers)
        # Only allow revert one commit one time, because git automatically generate mesaage will only contain one commit information
        parser.add_argument(
            'commit',
            help='git hash, svn revision or identifer you want to revert'
        )

    @classmethod
    def revert_commit(cls, args, repository, **kwargs):
        # Check if there are any outstanding changes:
        if repository.modified():
            sys.stderr.write('Please commit your changes or stash them before you revert commit: {}'.format(args.commit))
            return 1
        # Make sure we have the commit that user want to revert
        try:
            commit = repository.find(args.commit, include_log=False)
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write('Could not find "{}"'.format(args.commit) + '\n')
            return 1

        result = run([repository.executable(), 'revert', '--no-commit'] + [commit.hash], cwd=repository.root_path, capture_output=True)
        if result.returncode:
            # git revert will output nothing if this commit is already reverted
            if not result.stdout.strip():
                sys.stderr.write('The commits you spiecfied seems already be reverted.')
                return 2
            # print out
            sys.stderr.write(result.stdout.decode('utf-8'))
            sys.stderr.write(result.stderr.decode('utf-8'))
            sys.stderr.write('If you have merge conflicts, after resolving them, please use git-webkit pfr to publish your pull request')
            return 1
        # restore all ChangeLog changes, we should not revert those entries
        modifiled_files = repository.modified()
        if not modifiled_files:
            sys.stderr.write('Failed to detect any diff after revert.')
            return 1
        for file in modifiled_files:
            if 'ChangeLog' in file:
                result = run([repository.executable(), 'restore', '--staged', file], cwd=repository.root_path)
                if result.returncode:
                    sys.stderr.write('Failed to restore staged file: {}'.format(file))
                    run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
                    return 1

                result = run([repository.executable(), 'checkout', file], cwd=repository.root_path)
                if result.returncode:
                    sys.stderr.write('Failed to checkout file: {}'.format(file))
                    run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
                    return 1

        result = run([repository.executable(), 'revert', '--continue', '--no-edit'], cwd=repository.root_path)
        if result.returncode:
            run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
            sys.stderr.write('Failed revert commit')
            return 1
        log.info('Reverted {}'.format(commit.hash))
        return 0

    @classmethod
    def add_comment_to_reverted_commit_bug_tracker(cls, repository, args):
        source_remote = args.remote or 'origin'
        rmt = repository.remote(name=source_remote)
        if not rmt:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1
        if not rmt.pull_requests:
            sys.stderr.write("'{}' cannot generate pull-requests\n".format(rmt.url))
            return 1

        revert_pr = PullRequest.find_existing_pull_request(repository, rmt)

        commit = repository.find(args.commit, include_log=True)
        for line in commit.message.split():
            tracker = Tracker.from_string(line)
            if tracker:
                tracker.add_comment('Reverted by {}'.format(revert_pr.link))
                continue
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        if not PullRequest.check_pull_request_args(repository, args):
            return 1

        branch_point = PullRequest.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        result = cls.revert_commit(args, repository, **kwargs)
        if result:
            return result

        result = PullRequest.create_pull_request(repository, args, branch_point)
        if result:
            return result

        log.info('Adding comment for reverted commits...')
        return cls.add_comment_to_reverted_commit_bug_tracker(repository, args)
