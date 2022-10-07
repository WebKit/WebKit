# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
from .branch import Branch
from .squash import Squash

from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log, remote


class PullRequest(Command):
    name = 'pull-request'
    aliases = ['pr', 'pfr', 'upload']
    help = 'Push the current checkout state as a pull-request'
    BLOCKED_LABEL = 'merging-blocked'
    MERGE_LABELS = ['merge-queue']
    UNSAFE_MERGE_LABELS = ['unsafe-merge-queue']

    @classmethod
    def parser(cls, parser, loggers=None):
        Branch.parser(parser, loggers=loggers)
        Squash.parser(parser, loggers=loggers)
        parser.add_argument(
            '--add', '--no-add',
            dest='will_add', default=None,
            help='When drafting a change, add (or never add) modified files to set of staged changes to be committed',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--rebase', '--no-rebase', '--update', '--no-update',
            dest='rebase', default=None,
            help='Rebase (or do not rebase) the pull-request on the source branch before pushing',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--squash', '--no-squash',
            dest='squash', default=None,
            help='Combine all commits on the current development branch into a single commit before pushing',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )
        parser.add_argument(
            '--overwrite', '--amend', action='store_const', const='overwrite',
            dest='technique', default=None,
            help='When creating a pull request, overwrite the existing commit by default',
        )
        parser.add_argument(
            '--append', action='store_const', const='append',
            dest='technique', default=None,
            help='When creating a pull request, append a new commit on the existing branch by default',
        )
        parser.add_argument(
            '--with-history', '--no-history',
            dest='history', default=None,
            help='Create numbered branches to track the history of a change',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--draft', dest='draft', action='store_true', default=None,
            help='Mark a pull request as a draft when creating it',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Make a pull request against a specific remote',
        )
        parser.add_argument(
            '--checks', '--no-checks',
            dest='checks', default=None,
            help='Explicitly enable or disable automatic pre-flight checks',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '-o', '--open',
            dest='open', default=None,
            help='Automatically open the PR after creating it.',
            action=arguments.NoAction,
        )

    @classmethod
    def create_commit(cls, args, repository, **kwargs):
        # First, find the set of files to be modified
        modified = [] if args.will_add is False else repository.modified()
        if args.will_add:
            modified = list(set(modified).union(set(repository.modified(staged=False))))

        # Next, add all modified file
        for file in set(modified) - set(repository.modified(staged=True)):
            log.info('    Adding {}...'.format(file))
            if run([repository.executable(), 'add', file], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to add '{}'\n".format(file))
                return 1

        # Then, see if we already have a commit associated with this branch we need to modify
        has_commit = repository.commit(include_log=False, include_identifier=False).branch == repository.branch and repository.branch != repository.default_branch
        if not modified and has_commit:
            log.info('Using committed changes...')
            return 0

        bug_urls = getattr(args, '_bug_urls', None) or ''
        if isinstance(bug_urls, (list, tuple)):
            bug_urls = '\n'.join(bug_urls)

        # Otherwise, we need to create a commit
        will_amend = has_commit and args.technique == 'overwrite'
        if not modified:
            sys.stderr.write('No modified files\n')
            return 1
        log.info('Amending commit...' if will_amend else 'Creating commit...')
        env = os.environ
        env['COMMIT_MESSAGE_TITLE'] = getattr(args, '_title', None) or ''
        env['COMMIT_MESSAGE_BUG'] = bug_urls
        if run(
            [repository.executable(), 'commit', '--date=now'] + (['--amend'] if will_amend else []),
            cwd=repository.root_path,
            env=env,
        ).returncode:
            sys.stderr.write('Failed to generate commit\n')
            return 1

        return 0

    @classmethod
    def title_for(cls, commits):
        title = os.path.commonprefix([commit.message.splitlines()[0] for commit in commits if commit.message])
        if not title:
            title = commits[0].message.splitlines()[0] if commits[0].message else '???'
        title = title.rstrip().lstrip()
        return title[:-5].rstrip() if title.endswith('(Part') else title

    @classmethod
    def check_pull_request_args(cls, repository, args):
        if not args.technique:
            args.technique = repository.config()['webkitscmpy.pull-request']
        if args.history is None:
            args.history = dict(
                always=True,
                disabled=False,
                never=False,
            ).get(repository.config()['webkitscmpy.history'])
        if args.history and repository.config()['webkitscmpy.history'] == 'never':
            sys.stderr.write('History retention was requested, but repository configuration forbids it\n')
            return False
        return True

    @classmethod
    def pull_request_branch_point(cls, repository, args, **kwargs):
        branch_point = Branch.branch_point(repository)
        source_remote = args.remote
        if not source_remote:
            bp_remotes = set(repository.branches_for(hash=branch_point.hash, remote=None).keys())
            if len(bp_remotes) == 1:
                # If there is only one remote, that means the branch point doesn't exist on any remote
                # In that case, pick the remote with the most updated version of the branch in question
                remote_head = None
                for remote in repository.source_remotes():
                    try:
                        candidate = repository.find('remotes/{}/{}'.format(remote, branch_point.branch), include_log=False)
                        if not remote_head or candidate.identifier > remote_head.identifier:
                            remote_head = candidate
                            source_remote = remote
                    except ValueError:
                        pass
            else:
                for remote in repository.source_remotes():
                    if remote in bp_remotes:
                        source_remote = remote
                        break
            if source_remote and source_remote != 'origin':
                args.remote = source_remote
        if not source_remote:
            source_remote = repository.default_remote

        if repository.branch is None or repository.branch in repository.DEFAULT_BRANCHES or repository.PROD_BRANCHES.match(repository.branch):
            if not args.issue:
                head = repository.commit(include_log=True, include_identifier=False)
                if run([
                    repository.executable(), 'merge-base', '--is-ancestor',
                    head.hash, 'remotes/{}/{}'.format(source_remote, branch_point.branch),
                ], capture_output=True, cwd=repository.root_path).returncode:
                    if head.issues:
                        args.issue = head.issues[0].link

            if Branch.main(
                args, repository,
                why="'{}' is not a pull request branch".format(repository.branch),
                redact=source_remote != repository.default_remote,
                target_remote='fork' if source_remote == repository.default_remote else '{}-fork'.format(source_remote),
                **kwargs
            ):
                sys.stderr.write("Abandoning pushing pull-request because '{}' could not be created\n".format(args.issue))
                return None
        elif args.issue and repository.branch != args.issue:
            sys.stderr.write("Creating a pull-request for '{}' but we're on '{}'\n".format(args.issue, repository.branch))
            return None

        if not repository.config().get('remote.{}.url'.format(source_remote)):
            sys.stderr.write("'{}' is not a remote in this repository\n".format(source_remote))
            return None

        if run([
            repository.executable(), 'branch', '-f',
            branch_point.branch,
            'remotes/{}/{}'.format(source_remote, branch_point.branch),
        ], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to match '{}' to it's remote '{}'\n".format(branch_point.branch, source_remote))
            return None
        return branch_point

    @classmethod
    def find_existing_pull_request(cls, repository, remote):
        existing_pr = None
        user, _ = remote.credentials(required=False)
        for pr in remote.pull_requests.find(opened=None, head=repository.branch):
            existing_pr = pr
            if not existing_pr.opened:
                continue
            if user and existing_pr.author == user:
                break
        return existing_pr

    @classmethod
    def pre_pr_checks(cls, repository):
        num_checks = 0
        log.info('Running pre-PR checks...')
        for key, path in repository.config().items():
            if not key.startswith('webkitscmpy.pre-pr.'):
                continue
            num_checks += 1
            name = key.split('.')[-1]
            log.info('    Running {}...'.format(name))
            command = run(path.split(' '), cwd=repository.root_path)
            if command.returncode:
                if Terminal.choose(
                    '{} failed, continue uploading pull request?'.format(name),
                    default='No',
                ) == 'No':
                    sys.stderr.write('Pre-PR check {} failed\n'.format(name))
                    return False
                else:
                    log.info('    {} failed, continuing PR upload anyway'.format(name))
            else:
                log.info('    Ran {}!'.format(name))

        if num_checks:
            log.info('All pre-PR checks run!')
        else:
            log.info('No pre-PR checks to run')
        return True

    @classmethod
    def is_revert_commit(cls, commit):
        if not commit.message:
            return False
        msg = commit.message.split()
        if not len(msg):
            return False
        title = msg[0]
        return title.startswith('Revert')

    @classmethod
    def add_comment_to_reverted_commit_bug_tracker(cls, repository, args, pr, commit):
        source_remote = args.remote or repository.default_remote
        rmt = repository.remote(name=source_remote)
        if not rmt:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1
        if not rmt.pull_requests:
            sys.stderr.write("'{}' cannot generate pull-requests\n".format(rmt.url))
            return 1

        log.info('Adding comment for reverted commits...')
        for issue in commit.issues:
            issue.open(why='Reverted by {}'.format(pr.url))
        return 0

    @classmethod
    def create_pull_request(cls, repository, args, branch_point, callback=None, unblock=True, update_issue=True):
        source_remote = args.remote or repository.default_remote
        if not repository.config().get('remote.{}.url'.format(source_remote)):
            sys.stderr.write("'{}' is not a remote in this repository\n".format(source_remote))
            return 1

        rebasing = args.rebase if args.rebase is not None else repository.config().get(
            'webkitscmpy.auto-rebase-branch',
            repository.config().get('pull.rebase', 'true'),
        ) == 'true'

        if rebasing:
            log.info("Rebasing '{}' on '{}'...".format(repository.branch, branch_point.branch))
            if repository.pull(rebase=True, branch=branch_point.branch, remote=source_remote):
                sys.stderr.write("Failed to rebase '{}' on '{},' please resolve conflicts\n".format(repository.branch, branch_point.branch))
                return 1
            log.info("Rebased '{}' on '{}!'".format(repository.branch, branch_point.branch))
            branch_point = repository.commit(branch=branch_point.branch)

        if args.checks is None:
            args.checks = repository.config().get('webkitscmpy.auto-check', 'false') == 'true'
        if args.checks and not cls.pre_pr_checks(repository):
            sys.stderr.write('Checks have failed, aborting pull request.\n')
            return 1

        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))
        issues = commits[0].issues

        radar_issue = next(iter(filter(lambda issue: isinstance(issue.tracker, radar.Tracker), issues)), None)
        not_radar = next(iter(filter(lambda issue: not isinstance(issue.tracker, radar.Tracker), issues)), None)
        if radar_issue and not_radar and radar_issue.tracker.radarclient():
            not_radar.cc_radar(radar=radar_issue)
        issue = issues[0] if issues else None

        remote_repo = repository.remote(name=source_remote)
        if isinstance(remote_repo, remote.GitHub):
            if issue and issue.redacted and args.remote is None:
                print("Your issue is redacted, diverting to a secure, non-origin remote you have access to.")
                original_remote = source_remote
                if len(repository.source_remotes()) < 2:
                    print("Error. You do not have access to a secure, non-origin remote")
                    if args.defaults or Terminal.choose(
                        "Would you like to proceed anyways? \n",
                        default='No',
                    ) == 'No':
                        sys.stderr.write("Failed to create pull request due to non-suitable remote\n")
                        return 1
                else:
                    source_remote = repository.source_remotes()[1]
                    remote_repo = repository.remote(name=source_remote)
                    print("Making PR against '{}' instead of '{}'".format(source_remote, original_remote))
            target = 'fork' if source_remote == repository.default_remote else '{}-fork'.format(source_remote)

            if not repository.config().get('remote.{}.url'.format(target)):
                sys.stderr.write("'{}' is not a remote in this repository. Have you run `{} setup` yet?\n".format(
                    source_remote, os.path.basename(sys.argv[0]),
                ))
                return 1
        else:
            target = source_remote

        if not remote_repo:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1

        existing_pr = None
        if remote_repo.pull_requests:
            user, _ = remote_repo.credentials(required=False)

            log.info("Checking if PR already exists...")
            existing_pr = cls.find_existing_pull_request(repository, remote_repo)
            log.info("PR #{} found.".format(existing_pr.number) if existing_pr else "PR not found.")
            if existing_pr and not existing_pr.opened and not args.defaults and (
                args.defaults is False or Terminal.choose(
                    "'{}' is already associated with '{}', which is closed.\nWould you like to create a new pull-request?".format(repository.branch, existing_pr),
                    default='No',
            ) == 'Yes'):
                existing_pr = None

            if existing_pr and user and existing_pr.author != user and (args.defaults or Terminal.choose(
                "'{}' is owned by '{}'\nYou can either".format(existing_pr, existing_pr.author),
                options=('Create a new pull request', 'Overwrite PR-{} and assign to yourself'.format(existing_pr.number)),
                default='Create a new pull request',
                numbered=True,
            ) == 'Create a new pull request'):
                if target == source_remote:
                    sys.stderr.write("'{}' already exists on '{}', creating a pull-request would overwrite it\n".format(
                        repository.branch, target,
                    ))
                    return 1
                existing_pr = None

            if user and existing_pr and isinstance(remote_repo, remote.GitHub) and existing_pr._metadata.get('full_name'):
                pr_target = existing_pr._metadata['full_name']
                if not pr_target.startswith('{}/'.format(user)):
                    target, repo_name = pr_target.split('/')
                    if '-' in repo_name:
                        target = '{}-{}'.format(target, repo_name.split('-')[-1])
                    base_url = repository.url(name=source_remote)
                    if '://' in base_url:
                        base_url = '/'.join(base_url.split('/')[:3]) + '/'
                    else:
                        base_url = base_url.split(':')[0] + ':'
                    if target not in repository.source_remotes(personal=True) and run(
                        [repository.executable(), 'remote', 'add', target, '{}{}.git'.format(base_url, pr_target)],
                        capture_output=True, cwd=repository.root_path,
                    ).returncode not in [0, 3]:
                        sys.stderr.write("Failed to add '{}' remote\n".format(target))
                        return 1

        # Remove any active labels
        if existing_pr and existing_pr._metadata and existing_pr._metadata.get('issue'):
            log.info("Checking PR labels for active labels...")
            pr_issue = existing_pr._metadata['issue']
            labels = pr_issue.labels
            did_remove = False
            for to_remove in cls.MERGE_LABELS + cls.UNSAFE_MERGE_LABELS + ([cls.BLOCKED_LABEL] if unblock else []):
                if to_remove in labels:
                    log.info("Removing '{}' from PR #{}...".format(to_remove, existing_pr.number))
                    labels.remove(to_remove)
                    did_remove = True
            if did_remove:
                pr_issue.set_labels(labels)

        log.info("Pushing '{}' to '{}'...".format(repository.branch, target))
        if run([repository.executable(), 'push', '-f', target, repository.branch], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to push '{}' to '{}' (alias of '{}')\n".format(repository.branch, target, repository.url(name=target)))
            sys.stderr.write("Your checkout may be mis-configured, try re-running 'git-webkit setup' or\n")
            sys.stderr.write("your checkout may not have permission to push to '{}'\n".format(repository.url(name=target)))
            return 1

        if rebasing and target.endswith('fork') and repository.config().get('webkitscmpy.update-fork', 'false') == 'true':
            log.info("Syncing '{}' to remote '{}'".format(branch_point.branch, target))
            if run([repository.executable(), 'push', target, '{branch}:{branch}'.format(branch=branch_point.branch)], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to sync '{}' to '{}.' Error is non fatal, continuing...\n".format(branch_point.branch, target))

        if args.history or (target != source_remote and args.history is None and args.technique == 'overwrite'):
            regex = re.compile(r'^{}-(?P<count>\d+)$'.format(repository.branch))
            count = max([
                int(regex.match(branch).group('count')) if regex.match(branch) else 0 for branch in
                repository.branches_for(remote=target)
            ] + [0]) + 1

            history_branch = '{}-{}'.format(repository.branch, count)
            log.info("Creating '{}' as a reference branch".format(history_branch))
            if run([
                repository.executable(), 'branch', history_branch, repository.branch,
            ], cwd=repository.root_path).returncode or run([
                repository.executable(), 'push', '-f', target, history_branch,
            ], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to create and push '{}' to '{}'\n".format(history_branch, target))
                return 1

        if not remote_repo.pull_requests:
            sys.stderr.write("'{}' cannot generate pull-requests\n".format(remote_repo.url))
            return 1
        if args.draft and not remote_repo.pull_requests.SUPPORTS_DRAFTS:
            sys.stderr.write("'{}' does not support draft pull requests, aborting\n".format(remote_repo.url))
            return 1


        if existing_pr:
            log.info("Updating pull-request for '{}'...".format(repository.branch))
            pr = remote_repo.pull_requests.update(
                pull_request=existing_pr,
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
                opened=None if existing_pr.opened else True,
                draft=args.draft,
            )
            if not pr:
                sys.stderr.write("Failed to update pull-request '{}'\n".format(existing_pr))
                return 1
            print("Updated '{}'!".format(pr))
        else:
            log.info("Creating pull-request for '{}'...".format(repository.branch))
            pr = remote_repo.pull_requests.create(
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
                draft=args.draft,
            )
            if not pr:
                sys.stderr.write("Failed to create pull-request for '{}'\n".format(repository.branch))
                return 1
            print("Created '{}'!".format(pr))
            if cls.is_revert_commit(commits[0]):
                cls.add_comment_to_reverted_commit_bug_tracker(repository, args, pr, commits[0])

        if issue and update_issue:
            log.info('Checking issue assignee...')
            if issue.assignee != issue.tracker.me():
                issue.assign(issue.tracker.me())
                print('Assigning associated issue to {}'.format(issue.tracker.me()))
            log.info('Checking for pull request link in associated issue...')
            if pr.url and not any([pr.url in comment.content for comment in issue.comments]):
                if issue.opened:
                    issue.add_comment('Pull request: {}'.format(pr.url))
                else:
                    issue.open(why='Re-opening for pull request {}'.format(pr.url))
                print('Posted pull request link to {}'.format(issue.link))

        if issue and pr._metadata and pr._metadata.get('issue'):
            log.info('Syncing PR labels with issue component...')
            pr_issue = pr._metadata['issue']
            project = pr_issue.tracker.name
            component = issue.component
            if pr_issue.component == component or component not in pr_issue.tracker.projects.get(project, {}).get('components', {}):
                component = None
            version = issue.version
            if pr_issue.version == version or version not in pr_issue.tracker.projects.get(project, {}).get('versions', []):
                version = None
            if component or version:
                pr_issue.set_component(component=component, version=version)
                log.info('Synced PR labels with issue component!')
            else:
                log.info('No label syncing required')

        if pr.url:
            print(pr.url)

            if args.open:
                Terminal.open_url(pr.url)

        if callback:
            return callback(pr)
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1
        if not cls.check_pull_request_args(repository, args):
            return 1

        branch_point = cls.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        result = cls.create_commit(args, repository, **kwargs)
        if result:
            return result
        if args.squash:
            result = Squash.squash_commit(args, repository, branch_point, **kwargs)
            if result:
                return result

        return cls.create_pull_request(repository, args, branch_point)
