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
import os
import re

from .command import Command
from .branch import Branch
from .pull_request import PullRequest
from .commit import Commit as CommitProgram
from .land import Land

from webkitbugspy import Tracker, bugzilla, radar
from webkitcorepy import arguments, run, Terminal, string_utils
from webkitscmpy import local, log
from ..commit import Commit


class Revert(Command):
    name = 'revert'
    help = 'Revert provided list of commits and create a pull-request with this revert commit'
    REVERT_TITLE_TEMPLATE = 'Unreviewed, reverting {}'
    REVERT_TITLE_RE = re.compile(r'^Unreviewed, reverting {}'.format(Commit.IDENTIFIER_RE.pattern))

    @classmethod
    def parser(cls, parser, loggers=None):
        Land.parser(parser, loggers=loggers)
        parser.add_argument(
            'commit_id',
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
            '--pr', '--no-pr',
            default=None,
            action=arguments.NoAction,
            help='Create a pull request (or do not) after reverting'
        )

    @classmethod
    def get_commit_info(cls, args, repository, **kwargs):
        commit_objects = []
        commit_issues = {}

        for c in args.commit_id:
            try:
                commit = repository.find(c, include_log=True)
            except (local.Scm.Exception, ValueError) as exception:
                sys.stderr.write('Could not find "{}"'.format(c) + '\n')
                return None, None, None

            commit_objects.append(commit)
            for i in commit.issues:
                if not commit_issues.get(i.tracker.NAME, None):
                    commit_issues[i.tracker.NAME] = set()
                commit_issues[i.tracker.NAME].add(i)

        return commit_objects, commit_issues

    @classmethod
    def get_issue_info(cls, args, repository, commit_objects, commit_issues, **kwargs):
        # Can give either a bug URL or a title of the new issue
        if not args.issue and not args.reason:
            print('This issue will track the revert and should not be the issue of the commit(s) to be reverted.')
            prompt = 'Enter issue URL or title of new issue (reason for the revert): '
            args.issue = Terminal.input(prompt, alert_after=2 * Terminal.RING_INTERVAL)
        elif args.reason:
            args.issue = args.reason

        issue = Tracker.from_string(args.issue)

        # Create a new bug if no issue exists
        if not issue and Tracker.instance() and getattr(args, 'update_issue', True):
            if getattr(Tracker.instance(), 'credentials', None):
                Tracker.instance().credentials(required=True, validate=True)

            # Automatically set project, component, and version of new bug
            print('Setting bug properties...')
            commit_issues_list = list(commit_issues.get(Tracker.instance().NAME))
            commit_bug = commit_issues_list.pop()
            project = commit_bug.project
            component = commit_bug.component
            version = commit_bug.version

            # Check whether properties are the same if multiple commits are reverted
            while len(commit_issues_list):
                commit_bug = commit_issues_list.pop()
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
        elif not issue or not issue.title:
            sys.stderr.write('Could not fetch {} from link. Please verify that the issue exists.\n'.format(issue.tracker.NAME))
            return None

        rdar = Branch.cc_radar(args, repository, issue)
        if rdar:
            log.info("Created {}".format(rdar))

        return issue

    @classmethod
    def create_revert_commit_msg(cls, args, commit_objects, **kwargs):
        reverted_changeset = ''
        reverted_commits = []
        # Retrieve information for commits to be reverted
        for commit in commit_objects:
            commit_title = None
            for line in commit.message.splitlines():
                if not commit_title:
                    commit_title = line
            bug_urls = ['    {}'.format(i.link) for i in commit.issues]

            reverted_changeset += '\n    {}\n'.format(commit_title)
            reverted_changeset += '\n'.join(bug_urls)
            if commit.identifier and commit.branch:
                commit_repr = '{}@{} ({})'.format(commit.identifier, commit.branch, commit.hash[:commit.HASH_LABEL_SIZE])
                reverted_commits.append(commit_repr)
                reverted_changeset += '\n    {}\n'.format(commit_repr)
            else:
                sys.stderr.write('Could not find "{}"'.format(', '.join(args.commit_id)) + '\n')
                return None, None

        # Retrieve information for the issue tracking the revert
        revert_radar_link, revert_bug_link, revert_issue = None, None, None
        if args.update_issue:
            revert_issue = Tracker.from_string(args.issue)
            if not revert_issue:
                sys.stderr.write('Could not find issue {}'.format(revert_issue))
                return None, None

            for issue_url in CommitProgram.bug_urls(revert_issue):
                issue = Tracker.from_string(issue_url)
                if not issue:
                    continue
                if isinstance(issue.tracker, radar.Tracker):
                    revert_radar_link = issue_url
                if isinstance(issue.tracker, bugzilla.Tracker):
                    revert_bug_link = issue_url

        # FIXME: Add support for radar-based repositories
        is_redacted = revert_issue and (revert_issue.redacted or isinstance(revert_issue.tracker, radar.Tracker))
        if not args.reason and (not args.update_issue or is_redacted):
            prompt = 'Enter a reason for the revert: '
            revert_reason = Terminal.input(prompt, alert_after=2 * Terminal.RING_INTERVAL)
        else:
            revert_reason = args.reason or revert_issue.title

        env = os.environ
        env['COMMIT_MESSAGE_TITLE'] = cls.REVERT_TITLE_TEMPLATE.format(string_utils.join(reverted_commits))
        env['COMMIT_MESSAGE_REVERT'] = '{}\n{}\n\n{}\n\n'.format(revert_bug_link or 'Include a Bugzilla link (OOPS!).', revert_radar_link or 'Include a Radar link (OOPS!).', revert_reason)
        env['COMMIT_MESSAGE_REVERT'] += 'Reverted {}:\n{}'.format('changes' if len(reverted_commits) > 1 else 'change', reverted_changeset)
        return reverted_commits, revert_reason

    @classmethod
    def revert_commit(cls, args, repository, issue, commit_objects, **kwargs):
        commits_to_revert = [c.hash for c in commit_objects]
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

        if args.commit:
            result = run([repository.executable(), 'commit', '--date=now'], cwd=repository.root_path, env=os.environ)
        if result.returncode:
            run([repository.executable(), 'revert', '--abort'], cwd=repository.root_path)
            sys.stderr.write('Failed revert commit')
            return 1
        log.info('Reverted {}'.format(', '.join(commits)))

        return 0

    @classmethod
    def relate_issues(cls, args, repository, issue, commit_issues, revert_reason):
        log.info("Automatically relating issues...")
        rdar = Branch.cc_radar(args, repository, issue)
        if rdar:
            log.info("Created {}".format(rdar))

        for r_link in CommitProgram.bug_urls(issue):
            r_issue = Tracker.from_string(r_link)
            for c_issue in commit_issues.get(r_issue.tracker.NAME, []):
                # FIXME: Reopen radars after rdar://124165667
                if not isinstance(c_issue.tracker, radar.Tracker):
                    try:
                        c_issue.open(why="Reopened {}.\n{}, tracking revert in {}.".format(r_issue.tracker.NAME, revert_reason, r_issue.link))
                    except radar.Tracker.radarclient().exceptions.UnsuccessfulResponseException as e:
                        sys.stderr.write('Failed to re-open {}\n'.format(c_issue.link))
                        sys.stderr.write('{}\n'.format(str(e)))
                        c_issue.add_comment('{}, tracking revert in {}.'.format(revert_reason, r_issue.link))

                # Revert tracking bug blocks commit bugs, revert tracking radar caused by commit radars
                if isinstance(c_issue.tracker, bugzilla.Tracker):
                    r = r_issue.relate(blocks=c_issue)
                elif isinstance(c_issue.tracker, radar.Tracker):
                    try:
                        r = c_issue.relate(cause_of=r_issue)
                    except radar.Tracker.radarclient().exceptions.UnsuccessfulResponseException:
                        r = None
                if not r:
                    sys.stderr.write('Failed to relate {} and {}.\n'.format(c_issue.link, r_issue.link))
                else:
                    log.info('Related {} and {}.'.format(c_issue.link, r_issue.link))
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        if repository.modified():
            sys.stderr.write('Please commit your changes or stash them before you revert commit: {}'.format(', '.join(args.commit_id)))
            return 1

        if not PullRequest.check_pull_request_args(repository, args):
            return 1

        commit_objects, commit_issues = cls.get_commit_info(args, repository, **kwargs)
        if not commit_objects:
            return 1

        # Overrides PR args.commit to set default as True
        if args.commit is None:
            args.commit = True
        if not args.commit:
            return cls.revert_commit(args, repository, None, commit_objects, **kwargs)

        if args.update_issue:
            issue = cls.get_issue_info(args, repository, commit_objects, commit_issues, **kwargs)
            if not issue:
                return 1
        else:
            issue = None

        commit_identifiers, revert_reason = cls.create_revert_commit_msg(args, commit_objects, **kwargs)
        if not commit_identifiers:
            return 1

        if not args.issue:
            args.issue = revert_reason
        branch_point = PullRequest.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        if cls.revert_commit(args, repository, issue, commit_objects, **kwargs):
            return 1

        if args.update_issue:
            if cls.relate_issues(args, repository, issue, commit_issues, revert_reason):
                return 1

        if args.safe is not None:
            if args.pr is False:
                response = Terminal.choose(
                    '--no-pr is set. Do you want to create a PR and land the revert?', options=('Yes', 'No'), default='No',
                )
                if response == 'No':
                    return 0
                if response == 'Yes':
                    log.info('Creating PR...')
            return Land.main(args, repository, identifier_template=None, canonical_svn=False, hooks=None)

        if args.pr:
            return PullRequest.create_pull_request(repository, args, branch_point)

        return 0
