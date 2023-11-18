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

from .command import Command
from .branch import Branch
from .pull_request import PullRequest

from webkitbugspy import Tracker
from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log, remote
from ..commit import Commit


class Revert(Command):
    name = 'revert'
    help = 'Revert provided list of commits and create a pull-request with this revert commit'
    REVERT_TITLE_TEMPLATE = 'Unreviewed, reverting {}'
    REVERT_TITLE_RE = re.compile(r'^Unreviewed, reverting {}'.format(Commit.IDENTIFIER_RE.pattern))

    @classmethod
    def parser(cls, parser, loggers=None):
        PullRequest.parser(parser, loggers=loggers)
        # Only allow revert one commit one time, because git automatically generate mesaage will only contain one commit information
        parser.add_argument(
            'commit',
            help='git hash, svn revision or identifier you want to revert'
        )

        parser.add_argument(
            '--revert-issue',
            dest='revert_issue',
            type=str,
            help='Issue for the revert'
        )

        parser.add_argument(
            '--pr',
            default=False,
            action='store_true',
            help='Create a pull request at the same time'
        )

    @classmethod
    def get_issue_info(cls, args, repository, **kwargs):
        # Can give either a bug URL or a title of the new issue.
        # Using --revert-issue to differentiate from PR arg --issue/-i
        if not args.revert_issue:
            prompt = 'Enter issue URL or title of new issue for the revert: '
            args.issue = Terminal.input(prompt, alert_after=2 * Terminal.RING_INTERVAL)
        else:
            args.issue = args.revert_issue

        issue = Tracker.from_string(args.issue)

        if not issue and Tracker.instance() and getattr(args, 'update_issue', True):
            if getattr(Tracker.instance(), 'credentials', None):
                Tracker.instance().credentials(required=True, validate=True)
            issue = Tracker.instance().create(
                title=args.issue,
                description=Terminal.input('Issue description: '),
            )
            if not issue:
                sys.stderr.write('Failed to create new issue\n')
                return None
            print("Created '{}'".format(issue))
        elif not Tracker.instance():
            sys.stderr.write('Could not find tracker instance.\n')
        return issue

    @classmethod
    def create_revert_commit_msg(cls, args, repository, **kwargs):
        # Make sure we have the commit that user want to revert
        try:
            commit = repository.find(args.commit, include_log=True)
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write('Could not find "{}"'.format(args.commit) + '\n')
            return 1
        bug_urls = []
        commit_title = None
        for line in commit.message.splitlines():
            if not commit_title:
                commit_title = line
            tracker = Tracker.from_string(line)
            if tracker:
                bug_urls.append(line)
        revert_issue = Tracker.from_string(args.issue)
        if not revert_issue:
            sys.stderr.write('Could not find issue {}'.format(revert_issue))

        env = os.environ
        env['COMMIT_MESSAGE_TITLE'] = cls.REVERT_TITLE_TEMPLATE.format(commit, commit_title)
        env['COMMIT_MESSAGE_REVERT'] = '{}\n\n{}\n\nReverted changeset:\n\n{}'.format(revert_issue.link, revert_issue.title, commit_title)
        env['COMMIT_MESSAGE_BUG'] = '\n'.join(bug_urls)

        if commit.identifier and commit.branch:
            env['COMMIT_MESSAGE_BUG'] += '\nhttps://commits.webkit.org/{}@{}'.format(commit.identifier, commit.branch)
        else:
            sys.stderr.write('Could not find "{}"'.format(args.commit) + '\n')
            return 1

        return commit, commit_title, bug_urls

    @classmethod
    def revert_commit(cls, args, repository, issue, commit, commit_title, bug_urls, **kwargs):
        result = run([repository.executable(), 'revert', '--no-commit'] + [commit.hash], cwd=repository.root_path, capture_output=True)
        if result.returncode:
            # git revert will output nothing if this commit is already reverted
            if not result.stdout.strip():
                sys.stderr.write('The commit(s) you specified seem(s) to be already reverted.')
                return 2
            # print out
            sys.stderr.write(result.stdout.decode('utf-8'))
            sys.stderr.write(result.stderr.decode('utf-8'))
            sys.stderr.write('If you have merge conflicts, after resolving them, please use git-webkit pfr to publish your pull request')
            return 1

        cls.write_branch_variables(
            repository, repository.branch,
            title=cls.REVERT_TITLE_TEMPLATE.format(commit, commit_title),
            bug=bug_urls,
        )

        result = run([repository.executable(), 'commit', '--date=now'], cwd=repository.root_path, env=os.environ)
        if result.returncode:
            run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
            sys.stderr.write('Failed revert commit')
            return 1
        log.info('Reverted {}'.format(commit.hash))
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        # Check if there are any outstanding changes:
        if repository.modified():
            sys.stderr.write('Please commit your changes or stash them before you revert commit: {}'.format(args.commit))
            return 1

        if not PullRequest.check_pull_request_args(repository, args):
            return 1

        issue = cls.get_issue_info(args, repository, **kwargs)
        if not issue:
            return 1

        commit, commit_title, bug_urls = cls.create_revert_commit_msg(args, repository, **kwargs)
        if not commit or not commit_title:
            return 1

        branch_point = PullRequest.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        result = cls.revert_commit(args, repository, issue, commit, commit_title, bug_urls, **kwargs)
        if result:
            return result

        if not args.pr:
            return result
        else:
            return PullRequest.create_pull_request(repository, args, branch_point)

