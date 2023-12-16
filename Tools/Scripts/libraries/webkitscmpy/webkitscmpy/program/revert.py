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
from .commit import Commit as CommitProgram

from webkitbugspy import Tracker, bugzilla
from webkitcorepy import arguments, run, Terminal, string_utils
from webkitscmpy import local, log, remote
from ..commit import Commit


class Revert(Command):
    name = 'revert'
    help = 'Revert provided list of commits and create a pull-request with this revert commit'
    REVERT_TITLE_TEMPLATE = 'Unreviewed, reverting {}'
    REVERT_TITLE_RE = re.compile(r'^Unreviewed, reverting {}'.format(Commit.IDENTIFIER_RE.pattern))
    RES = [
        re.compile(r'<?rdar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?radar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?rdar:\/\/(?P<id>\d+)>?'),
        re.compile(r'<?radar:\/\/(?P<id>\d+)>?'),
    ]

    @classmethod
    def parser(cls, parser, loggers=None):
        PullRequest.parser(parser, loggers=loggers)
        parser.add_argument(
            'commit',
            nargs='+',
            help='git hash, svn revision or identifier of commits to revert'
        )

        parser.add_argument(
            '--reason', '--why', '-w',
            dest='reason',
            type=str,
            help='Reason for the revert'
        )

        parser.add_argument(
            '--pr',
            default=False,
            action='store_true',
            help='Create a pull request at the same time'
        )

    @classmethod
    def get_commit_info(cls, args, repository, **kwargs):
        commit_objects = []
        commits_to_revert = []
        commit_bugs = set()
        for c in args.commit:
            try:
                commit = repository.find(c, include_log=True)
            except (local.Scm.Exception, ValueError) as exception:
                sys.stderr.write('Could not find "{}"'.format(c) + '\n')
                return None, None, None
            commit_objects.append(commit)
            commits_to_revert.append(commit.hash)
            commit_bugs.update({Tracker.from_string(i.link) for i in commit.issues if isinstance(Tracker.from_string(i.link).tracker, bugzilla.Tracker)})
        return commit_objects, commits_to_revert, commit_bugs

    @classmethod
    def get_issue_info(cls, args, repository, commit_objects, commit_bugs, **kwargs):
        # Can give either a bug URL or a title of the new issue
        if not args.issue and not args.reason:
            prompt = 'Enter issue URL or reason for the revert: '
            args.issue = Terminal.input(prompt, alert_after=2 * Terminal.RING_INTERVAL)
        elif args.reason:
            args.issue = args.reason

        issue = Tracker.from_string(args.issue)
        commit_bugs_list = list(commit_bugs)
        if not issue and Tracker.instance() and getattr(args, 'update_issue', True):
            if getattr(Tracker.instance(), 'credentials', None):
                Tracker.instance().credentials(required=True, validate=True)

            # Automatically set project, component, and version of new bug
            print('Setting bug properties...')
            commit_bug = commit_bugs_list.pop()
            project = commit_bug.project
            component = commit_bug.component
            version = commit_bug.version

            # Check whether properties are the same if multiple commits are reverted
            while len(commit_bugs_list):
                commit_bug = commit_bugs_list.pop()
                # Setting properties to None will prompt for user input
                if commit_bug.project != project:
                    project = None
                if commit_bug.component != component:
                    component = None
                if commit_bug.version != version:
                    version = None

            commits = [repr(c) for c in commit_objects]
            issue = Tracker.instance().create(
                title=args.issue,
                description='Revert {} because {}.'.format(string_utils.join(commits), args.issue),
                project=project,
                component=component,
                version=version,
            )
            if not issue:
                sys.stderr.write('Failed to create new issue\n')
                return None
            args.issue = issue.link
            print("Created '{}'".format(issue))
        elif not Tracker.instance():
            sys.stderr.write('Could not find tracker instance.\n')

        rdar = Branch.cc_radar(args, repository, issue)
        if rdar:
            log.info("Created {}".format(rdar))

        for c_bug in commit_bugs:
            # TODO: relate revert bug
            c_bug.open(why="{}, tracking revert in {}".format(issue.title, issue.link))

        return issue

    @classmethod
    def create_revert_commit_msg(cls, args, commit_objects, **kwargs):
        reverted_changeset = ''
        commit_identifiers = []
        # Retrieve information for commits to be reverted
        for commit in commit_objects:
            commit_title = None
            for line in commit.message.splitlines():
                if not commit_title:
                    commit_title = line
            bug_urls = [i.link for i in commit.issues]

            reverted_changeset += '\n{}\n'.format(commit_title)
            reverted_changeset += '\n'.join(bug_urls)
            if commit.identifier and commit.branch:
                commit_identifiers.append('{}@{}'.format(commit.identifier, commit.branch))
                reverted_changeset += '\nhttps://commits.webkit.org/{}@{}\n'.format(commit.identifier, commit.branch)
            else:
                sys.stderr.write('Could not find "{}"'.format(', '.join(args.commit)) + '\n')
                return None

        # Retrieve information for the issue tracking the revert
        revert_issue = Tracker.from_string(args.issue)
        if not revert_issue:
            sys.stderr.write('Could not find issue {}'.format(revert_issue))
            return None
        revert_issue_radar = None
        for bug in CommitProgram.bug_urls(revert_issue):
            for regex in cls.RES:
                if regex.match(bug):
                    revert_issue_radar = bug
                    break
            if revert_issue_radar:
                break

        env = os.environ
        env['COMMIT_MESSAGE_TITLE'] = cls.REVERT_TITLE_TEMPLATE.format(string_utils.join(commit_identifiers))
        env['COMMIT_MESSAGE_REVERT'] = '{}\n{}\n\n{}\n\n'.format(revert_issue.link, revert_issue_radar or 'Include a Radar link (OOPS!).', revert_issue.title)
        env['COMMIT_MESSAGE_REVERT'] += 'Reverted {}:\n{}'.format('changes' if len(commit_identifiers) > 1 else 'change', reverted_changeset)
        return commit_identifiers

    @classmethod
    def revert_commit(cls, args, repository, issue, commit_objects, commits_to_revert, **kwargs):
        result = run([repository.executable(), 'revert', '--no-commit'] + commits_to_revert, cwd=repository.root_path, capture_output=True)
        if result.returncode:
            # git revert will output nothing if this commit is already reverted
            if not result.stdout.strip():
                sys.stderr.write('The commit(s) you specified seem(s) to already be reverted.')
                return 2
            sys.stderr.write(result.stdout.decode('utf-8'))
            sys.stderr.write(result.stderr.decode('utf-8'))
            sys.stderr.write('If you have merge conflicts, after resolving them, please use git-webkit pfr to publish your pull request')
            return 1

        commits = [repr(c) for c in commit_objects]
        cls.write_branch_variables(
            repository, repository.branch,
            title=cls.REVERT_TITLE_TEMPLATE.format(', '.join(commits)),
            bug=[issue],
        )

        result = run([repository.executable(), 'commit', '--date=now'], cwd=repository.root_path, env=os.environ)
        if result.returncode:
            run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
            sys.stderr.write('Failed revert commit')
            return 1
        log.info('Reverted {}'.format(', '.join(commits)))
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        if repository.modified():
            sys.stderr.write('Please commit your changes or stash them before you revert commit: {}'.format(', '.join(args.commit)))
            return 1

        if not PullRequest.check_pull_request_args(repository, args):
            return 1

        commit_objects, commits_to_revert, commit_bugs = cls.get_commit_info(args, repository, **kwargs)
        if not commit_objects:
            return 1

        issue = cls.get_issue_info(args, repository, commit_objects, commit_bugs, **kwargs)
        if not issue:
            return 1

        commit_identifiers = cls.create_revert_commit_msg(args, commit_objects, **kwargs)
        if not commit_identifiers:
            return 1

        branch_point = PullRequest.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        result = cls.revert_commit(args, repository, issue, commit_objects, commits_to_revert, **kwargs)
        if result:
            return result

        if not args.pr:
            return result
        else:
            return PullRequest.create_pull_request(repository, args, branch_point)

