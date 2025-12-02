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
from .commit import Commit
from .branch import Branch
from .install_hooks import InstallHooks
from .squash import Squash

from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, run, string_utils, Terminal, OutputCapture
from webkitscmpy import local, log, remote


class PullRequest(Command):
    name = 'pull-request'
    aliases = ['pr', 'pfr', 'upload']
    help = 'Push the current checkout state as a pull-request'
    BLOCKED_LABEL = 'merging-blocked'
    SKIP_EWS_LABEL = 'skip-ews'
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
            '--commit', '--no-commit',
            dest='commit', default=None,
            help='When creating a pull request, create (or do not create) a commit from the set of staged changes',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--with-history', '--no-history',
            dest='history', default=None,
            help='Create numbered branches to track the history of a change',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--set-upstream', '--no-set-upstream',
            dest='set_upstream', default=None,
            help='Set the upstream of the local branch when pushing',
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
        parser.add_argument(
            '--ews', '--skip-ews', '--no-ews',
            dest='ews', default=None,
            help='Explicitly enable or disable EWS on the PR',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--update-title', '--no-update-title',
            dest='update_title', default=True,
            help="When updating a pull request, update (or do not update) its title with the commits' common prefix.",
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--no-issue', '--no-bug',
            dest='update_issue', default=True,
            help='Disable automatic bug creation and updates',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--security', '--redacted',
            dest='redact', action='store_true',
            default=False,
            help='Force the a PR onto a secure remote, regardless of the current branch and issue state.',
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
        if getattr(args, '_title', None):
            env['COMMIT_MESSAGE_TITLE'] = getattr(args, '_title')
        if bug_urls:
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
        if args.redact and len(repository.source_remotes()) <= 1:
            sys.stderr.write('No secure remotes found in the current checkout\n')
            return None
        if args.redact and repository.source_remotes()[0] == args.remote:
            sys.stderr.write("'{}' is not a secure remote\n".format(args.remote))
            sys.stderr.write("'--remote={}' is incompatible with '--redacted'\n".format(args.remote))
            return None

        branch_point = repository.branch_point()
        if not branch_point:
            sys.stderr.write('Failed to determine where pull-request diverged from production branch\n')
            return None
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
            if source_remote != repository.default_remote:
                print("Making pull request against '{}' because that is where the branch point is from".format(source_remote))
                if branch_point.branch in repository.DEFAULT_BRANCHES:
                    sys.stderr.write('Branch point is on the default branch\n')
                    sys.stderr.write("Local record of '{}' may be out of date\n".format(repository.default_remote))
                    sys.stderr.write("Update with 'git fetch {}'\n".format(repository.default_remote))
            if source_remote and source_remote != repository.default_remote:
                args.remote = source_remote
        if not source_remote:
            source_remote = repository.default_remote
        if args.redact and source_remote == repository.default_remote:
            source_remote = repository.source_remotes()[-1]
            args.remote = source_remote

        if not repository.is_suitable_branch_for_pull_request(repository.branch, source_remote):
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
            error = "Creating a pull-request for '{}' but we're on '{}'\n".format(args.issue, repository.branch)
            if not repository.dev_branches.match(repository.branch):
                sys.stderr.write(error)
                return None

            if string_utils.decode(args.issue).isnumeric():
                issue = Tracker.instance().issue(int(args.issue))
            else:
                issue = Tracker.from_string(args.issue)
            if not issue:
                sys.stderr.write(error)
                return None
            if not repository.branch.endswith('/{}'.format(issue.id)) and not repository.branch.endswith('/{}'.format(Branch.to_branch_name(issue.title))):
                sys.stderr.write(error)
                return None

            if not issue.tracker.hide_title:
                args._title = issue.title
            args._bug_urls = Commit.bug_urls(issue)

        if not repository.config().get('remote.{}.url'.format(source_remote)):
            sys.stderr.write("'{}' is not a remote in this repository\n".format(source_remote))
            return None

        did_local_branch_diverge = bool(run([
            repository.executable(), 'merge-base', '--is-ancestor',
            branch_point.branch,
            'remotes/{}/{}'.format(source_remote, branch_point.branch),
        ], cwd=repository.root_path).returncode)
        if did_local_branch_diverge and run([
            repository.executable(), 'branch', '-f',
            branch_point.branch,
            'remotes/{}/{}'.format(source_remote, branch_point.branch),
        ], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to match '{}' to it's remote '{}'\n".format(branch_point.branch, source_remote))
            return None
        return branch_point

    @classmethod
    def find_existing_pull_request(cls, repository, remote, branch=None):
        branch = branch or repository.branch
        existing_pr = None
        user, _ = remote.credentials(required=False)
        for pr in remote.pull_requests.find(opened=None, head=branch):
            existing_pr = pr
            if not existing_pr.opened:
                continue
            if branch != pr.head:
                continue
            if user and existing_pr.author == user:
                break
        return existing_pr

    @classmethod
    def pre_pr_checks(cls, repository, add_edits=True):
        num_checks = 0
        log.info('Running pre-PR checks...')
        for key, path in repository.config().items():
            if not key.startswith('webkitscmpy.pre-pr.'):
                continue
            num_checks += 1
            name = key.split('.')[-1]
            log.info('    Running {}...'.format(name))
            while True:
                command_line = path.split(' ')
                if command_line[0] == 'python3' and os.name == 'nt':
                    command_line[0] = sys.executable
                command = run(command_line, cwd=repository.root_path)
                if command.returncode == 0:
                    log.info('    Ran {}!'.format(name))
                    break
                options = ['Yes', 'Retry', 'No']
                response = Terminal.choose(
                    '{} failed!\nRetry will amend the commit with your changes. Continue uploading pull request?'.format(name),
                    options=options,
                    default='No',
                )
                if response == 'No':
                    sys.stderr.write('Pre-PR check {} failed\n'.format(name))
                    return False
                if response == 'Yes':
                    log.info('    {} failed, continuing PR upload anyway'.format(name))
                    break

                modified = [] if add_edits is False else repository.modified()
                if add_edits:
                    modified = list(set(modified).union(set(repository.modified(staged=False))))
                for file in set(modified) - set(repository.modified(staged=True)):
                    log.info('    Adding {}...'.format(file))
                    if run([repository.executable(), 'add', file], cwd=repository.root_path).returncode:
                        sys.stderr.write("Failed to add '{}'\n".format(file))
                        return False

                if modified and run(
                    [repository.executable(), 'commit', '--amend', '--date=now', '--no-edit'],
                    cwd=repository.root_path,
                ).returncode:
                    sys.stderr.write('Pre-PR check {} failed, and commit amend \n'.format(name))
                    return False

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
    def add_comment_to_issue(cls, issue, pr):
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

    @classmethod
    def create_pull_request(cls, repository, args, branch_point, callback=None, unblock=True, update_issue=None):
        if update_issue is None:
            update_issue = getattr(args, 'update_issue', True)
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
        if args.checks and not cls.pre_pr_checks(repository, add_edits=not (args.will_add is False)):
            sys.stderr.write('Checks have failed, aborting pull request.\n')
            return 1

        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))
        issues = [
            issue
            for commit in commits
            for issue in commit.issues
        ]

        radar_issue = next(iter(filter(lambda issue: isinstance(issue.tracker, radar.Tracker), issues)), None)
        not_radar = next(iter(filter(lambda issue: not isinstance(issue.tracker, radar.Tracker), issues)), None)
        radar_cc_default = repository.config().get('webkitscmpy.cc-radar', 'true') == 'true'
        if update_issue and radar_issue and not_radar and radar_issue.tracker.radarclient() and (args.cc_radar or (radar_cc_default and args.cc_radar is not False)):
            try:
                not_radar.cc_radar(radar=radar_issue)
            except ValueError:
                sys.stderr.write('Aborting pull request.\n')
                return 1

        redaction_exemption = None
        redacted_issue = None
        for candidate in issues:
            if getattr(candidate.redacted, 'exemption', False):
                redaction_exemption = candidate
            elif candidate.redacted:
                redacted_issue = candidate
        if redaction_exemption:
            print('A commit you are uploading references {}'.format(redaction_exemption.link))
            print("{} {}".format(redaction_exemption.link, redaction_exemption.redacted))
            if redacted_issue:
                sys.stderr.write("Redaction exemption overrides the redaction of {}\n".format(redacted_issue.link))
                sys.stderr.write("{} {}\n".format(redacted_issue.link, redacted_issue.redacted))
            redacted_issue = None
        issue = issues[0] if issues else None

        remote_repo = repository.remote(name=source_remote)
        if isinstance(remote_repo, remote.GitHub) and redacted_issue and args.remote is None:
            print('A commit you are uploading references {}'.format(redacted_issue.link))
            print("{} {}".format(redacted_issue.link, redacted_issue.redacted))
            print("Pull request needs to be sent to a secure remote for review")
            original_remote = source_remote
            if len(repository.source_remotes()) < 2:
                sys.stderr.write('Error. You do not have access to a secure remote to make a pull request for a redacted issue\n')
                sys.stderr.write('Please consult repository administers to gain access to a secure remote to make this fix against\n')
                return 1
            else:
                source_remote = repository.source_remotes()[-1]
                if args.defaults or Terminal.choose(
                    "Would you like to make a pull request against '{}' instead of '{}'? \n".format(source_remote, original_remote),
                    default='Yes', options=('Yes', 'Cancel')
                ) == 'Cancel':
                    sys.stderr.write("User declined to create a pull request against the secure remote '{}'\n".format(source_remote))
                    return 1
                remote_repo = repository.remote(name=source_remote)
                print("Making PR against '{}' instead of '{}'".format(source_remote, original_remote))

        if not remote_repo:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1

        previous_target = repository.config().get('branch.{}.target'.format(repository.branch))
        if previous_target and previous_target != source_remote:
            if args.remote:
                sys.stderr.write("'{}' was previously made against the '{}' remote\n".format(repository.branch, previous_target))
                sys.stderr.write("User over-rode and is now making that PR against '{}'\n".format(args.remote))
            elif args.defaults:
                sys.stderr.write("'{}' was previously made against the '{}' remote\n".format(repository.branch, previous_target))
                sys.stderr.write("Prevailing issue indicates it should be made against '{}'\n".format(source_remote))
                sys.stderr.write("Cannot automatically determine which is correct, canceling pull-request\n")
                return 1
            else:
                response = Terminal.choose(
                    "'{}' was previously made against the '{}' remote, but the prevailing issue indicates it should be made against '{}'\n"
                    "Which remote would you like to make your pull request against?".format(repository.branch, previous_target, source_remote),
                    options=('Cancel', 'Use {} (previous)'.format(previous_target), 'Use {} (new)'.format(source_remote)),
                    default='Cancel', numbered=True
                )
                match = re.match(r'Use (.+) \((previous|new)\)', response)
                if not match:
                    sys.stderr.write("User canceled pull-request because new remote target '{}' did not match previous remote target '{}'\n".format(
                        source_remote, previous_target,
                    ))
                    return 1
                source_remote = match.group(1)
                print("Making the PR against the '{}' remote".format(source_remote))

        if run(
            [repository.executable(), 'config', 'branch.{}.target'.format(repository.branch), source_remote],
            cwd=repository.root_path, capture_output=True,
        ).returncode:
            sys.stderr.write("Failed to set the target of '{}' to '{}'\n".format(repository.branch, source_remote))

        if isinstance(remote_repo, remote.GitHub):
            target = 'fork' if source_remote == repository.default_remote else '{}-fork'.format(source_remote)
            if not repository.config().get('remote.{}.url'.format(target)):
                sys.stderr.write("'{}' is not a remote in this repository. Have you run `{} setup` yet?\n".format(
                    source_remote, os.path.basename(sys.argv[0]),
                ))
                return 1
        else:
            target = source_remote

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
            labels_to_remove = cls.MERGE_LABELS + cls.UNSAFE_MERGE_LABELS + ([cls.BLOCKED_LABEL] if unblock else [])
            if args.ews is not False:
                # if --no-ews argument is not passed then remove any existing SKIP_EWS_LABEL
                labels_to_remove += [cls.SKIP_EWS_LABEL]

            for to_remove in labels_to_remove:
                if to_remove in labels:
                    log.info("Removing '{}' from PR #{}...".format(to_remove, existing_pr.number))
                    labels.remove(to_remove)
                    did_remove = True
            if did_remove:
                pr_issue.set_labels(labels)

        set_upstream = (
            args.set_upstream
            if args.set_upstream is not None
            else repository.config().get(
                "webkitscmpy.set-upstream-on-push",
                "false",
            )
            == "true"
        )

        push_env = os.environ.copy()
        push_env["VERBOSITY"] = str(args.verbose)

        if target.endswith('fork') and repository.config().get('webkitscmpy.update-fork', 'false') == 'true':
            # If our remote is a GitHub repository, we can use the API and save ourselves a push
            fork_remote = repository.remote(name=target)
            did_update_branch = False
            if isinstance(fork_remote, remote.GitHub):
                log.info("Updating '{}' on '{}'".format(branch_point.branch, fork_remote.url))
                with OutputCapture():
                    did_update_branch = fork_remote.request(
                        method='POST', path='merge-upstream',
                        json=dict(branch=branch_point.branch),
                        authenticated=True,
                    ) is not None
                if did_update_branch and run([repository.executable(), 'fetch', target, branch_point.branch], cwd=repository.root_path, capture_output=True).returncode:
                    sys.stderr.write("Failed to fetch '{}' for '{}.' Error is non fatal, continuing...\n".format(target, branch_point.branch))
            if not did_update_branch and rebasing:
                log.info("Syncing '{}' to remote '{}'".format(branch_point.branch, target))
                if run([repository.executable(), 'push', target, '{branch}:{branch}'.format(branch=branch_point.branch)], cwd=repository.root_path, env=push_env).returncode:
                    sys.stderr.write("Failed to sync '{}' to '{}.' Error is non fatal, continuing...\n".format(branch_point.branch, target))

        log.info("Pushing '{}' to '{}'...".format(repository.branch, target))
        if run(
            [repository.executable(), "push", "-f"]
            + (["-u"] if set_upstream else [])
            + [target, repository.branch],
            cwd=repository.root_path,
            env=push_env,
        ).returncode:
            sys.stderr.write("Failed to push '{}' to '{}' (alias of '{}')\n".format(repository.branch, target, repository.url(name=target)))
            sys.stderr.write("Your checkout may be mis-configured, try re-running 'git-webkit setup' or\n")
            sys.stderr.write("your checkout may not have permission to push to '{}'\n".format(repository.url(name=target)))
            return 1

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
            ], cwd=repository.root_path, env=push_env).returncode:
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
                title=cls.title_for(commits) if args.update_title else existing_pr.title,
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
            if not args.update_title:
                sys.stderr.write("'--no-update-title' cannot be used when creating a new pull-request.\n")
                return 1
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
            if cls.is_revert_commit(commits[0]) and update_issue:
                cls.add_comment_to_reverted_commit_bug_tracker(repository, args, pr, commits[0])

        if issue and update_issue and isinstance(issue.tracker, radar.Tracker) and not_radar:
            cls.add_comment_to_issue(not_radar, pr)
        elif issue and update_issue:
            cls.add_comment_to_issue(issue, pr)

        if issue and pr._metadata and pr._metadata.get('issue'):
            log.info('Syncing PR labels with issue component...')
            pr_issue = pr._metadata['issue']
            project = pr_issue.tracker.name
            component = issue.component
            if pr_issue.component == component or component not in pr_issue.tracker.projects.get(project, {}).get('components', {}):
                component = None
            if component:
                pr_issue.set_component(component=component)
                log.info('Synced PR labels with issue component!')
            else:
                log.info('No label syncing required')
            if args.ews is False:
                # Add SKIP_EWS_LABEL if --no-ews argument was passed
                labels = pr_issue.labels
                labels.append(cls.SKIP_EWS_LABEL)
                pr_issue.set_labels(labels)

        if pr.url:
            print(pr.url)

            if args.open:
                Terminal.open_url(pr.url)

        if callback:
            return callback(pr)
        return 0

    @classmethod
    def main(cls, args, repository, hooks=None, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1
        if not cls.check_pull_request_args(repository, args):
            return 1
        if hooks and InstallHooks.hook_needs_update(repository, os.path.join(hooks, 'pre-push')):
            sys.stderr.write("Cannot run a command which invokes `git push` with an out-of-date pre-push hook\n")
            sys.stderr.write("Please re-run `git-webkit setup` to update all local hooks\n")
            return 1

        branch_point = cls.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1

        if args.commit is None:
            args.commit = repository.config().get('webkitscmpy.auto-create-commit') == 'true'
        if args.commit:
            result = cls.create_commit(args, repository, **kwargs)
            if result:
                return result
        if args.squash:
            result = Squash.squash_commit(args, repository, branch_point, **kwargs)
            if result:
                return result

        return cls.create_pull_request(repository, args, branch_point)
