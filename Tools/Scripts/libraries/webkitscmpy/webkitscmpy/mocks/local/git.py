# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

from __future__ import annotations

import fnmatch
import hashlib
import json
import os
import re
import itertools
import time
from collections import OrderedDict
from datetime import datetime, timezone
from unittest.mock import patch
from typing import Any, Callable, Mapping, Sequence, TYPE_CHECKING, TypeVar

from webkitcorepy import OutputCapture, StringIO, decorators, mocks, string_utils

from webkitscmpy import Commit, Contributor, local
from webkitscmpy.program.canonicalize import IdentifierTrailer
from webkitscmpy.program.canonicalize.committer import main as committer_main
from webkitscmpy.program.canonicalize.message import main as message_main

if TYPE_CHECKING:
    from types import TracebackType
    from unittest.mock import _patch


# 'git config' spells listing two ways, and both appear in the wild
LIST_OPTION = re.compile(r'^(-l|--list)$')

GitType = TypeVar('GitType', bound='Git')


class Git(mocks.Subprocess):
    # Mock commits always have a hash, identifier, timestamp, author and message. Routes read those
    # fields inside lambdas and comprehensions, where they can't be narrowed, so those lines ignore
    # the errors mypy reports for the fields being Optional.
    # Parse a .git/config that looks like this
    # [core]
    #     repositoryformatversion = 0
    # [branch "main"]
    #     remote = origin
    #     merge = refs/heads/main
    RE_SINGLE_TOP = re.compile(r'^\[\s*(?P<key>\S+)\s*\]')
    RE_MULTI_TOP = re.compile(r'^\[\s*(?P<keya>\S+) "(?P<keyb>\S+)"\s*\]')
    RE_ELEMENT = re.compile(r'^\s+(?P<key>[^\s=]+)\s*=\s*(?P<value>.*\S+)')

    def __init__(
        self, path: str = '/.invalid-git', datafile: str | None = None,
        remote: str | None = None, tags: dict[str, Commit] | None = None,
        detached: bool | None = None, default_branch: str = 'main',
        git_svn: bool = False, remotes: dict[str, str] | None = None, editor: Callable[[str], Any] | None = None,
        is_worktree: bool = False,
    ) -> None:
        self.path = path
        self.default_branch = default_branch
        self.remote = remote or 'git@example.org:mock/{}'.format(os.path.basename(path))
        self.detached = detached or False
        self.is_worktree = is_worktree
        self.push_error: int | None = None

        self.tags: dict[str, Commit] = tags or {}

        try:
            self.executable = local.Git.executable()
        except (OSError, AssertionError):
            self.executable = '/usr/bin/git'

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'git-repo.json')) as file:
            data: dict[str, list[dict[str, Any]]] = json.load(file)
        self.commits: dict[str, list[Commit]] = {}
        for key, entries in data.items():
            commit_objs = []
            for kwargs in entries:
                changeFiles = None
                if 'changeFiles' in kwargs:
                    changeFiles = kwargs['changeFiles']
                    del kwargs['changeFiles']
                commit = Commit(**kwargs)
                if changeFiles:
                    setattr(commit, '__mock__changeFiles', changeFiles)
                commit_objs.append(commit)
            self.commits[key] = commit_objs
            if not git_svn:
                for commit in self.commits[key]:
                    commit.revision = None

        self.head = self.commits[self.default_branch][-1]
        self.remotes = {
            'origin/{}'.format(branch): commits[:] for branch, commits in self.commits.items()
            if not local.Git.DEV_BRANCHES.match(branch)
        }
        for name in (remotes or {}).keys():
            for branch, commits in self.commits.items():
                self.remotes['{}/{}'.format(name, branch)] = commits[:]

        self.tags = {}

        self.staged: dict[str, str] = {}
        self.modified: dict[str, str] = {}
        self.revert_message: str | None = None

        self.has_git_lfs = False

        def editor_generator(*args: str, **kwargs: Any) -> mocks.ProcessCompletion:
            if editor:
                editor(args[3])
            return mocks.ProcessCompletion(returncode=0)

        # If the directory provided actually exists, populate it
        if self.path != '/' and os.path.isdir(self.path):
            if not os.path.isdir(os.path.join(self.path, '.git')):
                os.mkdir(os.path.join(self.path, '.git'))
            with open(os.path.join(self.path, '.git', 'config'), 'w') as config:
                config.write(
                    '[core]\n'
                    '\trepositoryformatversion = 0\n'
                    '\tfilemode = true\n'
                    '\tbare = false\n'
                    '\tlogallrefupdates = true\n'
                    '\tignorecase = true\n'
                    '\tprecomposeunicode = true\n'
                    '{editor}'
                    '[pull]\n'
                    '\trebase = true\n'
                    '[remote "origin"]\n'
                    '\turl = {remote}\n'
                    '\tfetch = +refs/heads/*:refs/remotes/origin/*\n'
                    '[branch "{branch}"]\n'
                    '\tremote = origin\n'
                    '\tmerge = refs/heads/{branch}\n'.format(
                        remote=self.remote,
                        branch=self.default_branch,
                        editor='\teditor = /bin/Example\\ Program -n -w\n' if editor else '',
                    ))
                for name, url in (remotes or {}).items():
                    config.write(
                        '[remote "{name}"]\n'
                        '\turl = {url}\n'
                        '\tfetch = +refs/heads/*:refs/remotes/{name}/*\n'.format(
                            name=name, url=url,
                        )
                    )
                if git_svn:
                    domain = 'webkit.org'
                    if self.remote.startswith('https://'):
                        domain = self.remote.split('/')[2]
                    elif '@' in self.remote:
                        domain = self.remote.split('@')[1].split(':')[0]

                    config.write(
                        '[svn-remote "svn"]\n'
                        '    url = https://svn.{domain}/repository/webkit\n'
                        '    fetch = trunk:refs/remotes/origin/{branch}'.format(
                            domain=domain,
                            branch=self.default_branch,
                        )
                    )

        if git_svn:
            git_svn_routes = [
                mocks.Subprocess.Route(
                    self.executable, 'svn', 'find-rev', re.compile(r'r\d+'),
                    cwd=self.path,
                    generator=lambda *args, **kwargs:
                        mocks.ProcessCompletion(
                            returncode=0,
                            stdout=getattr(self.find(args[3][1:]), 'hash', '\n'),
                        )
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'info',
                    cwd=self.path,
                    generator=lambda *args, **kwargs:
                        mocks.ProcessCompletion(
                            returncode=0,
                            stdout=
                                'Path: .\n'
                                'URL: {remote}/{branch}\n'
                                'Repository Root: {remote}\n'
                                'Revision: {revision}\n'
                                'Node Kind: directory\n'
                                'Schedule: normal\n'
                                'Last Changed Author: {author}\n'
                                'Last Changed Rev: {revision}\n'
                                'Last Changed Date: {date}'.format(
                                    remote=self.remote,
                                    branch=self.head.branch,
                                    revision=self.head.revision,
                                    author=self.head.author.email,  # type: ignore[union-attr]
                                    date=datetime.fromtimestamp(self.head.timestamp).strftime('%Y-%m-%d %H:%M:%S'),  # type: ignore[arg-type]
                                ),
                        ),
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'fetch',
                    cwd=self.path,
                    completion=mocks.ProcessCompletion(returncode=0)
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'dcommit',
                    cwd=self.path,
                    generator=lambda *args, **kwargs: self.dcommit(),
                ),
            ]

        else:
            git_svn_routes = [mocks.Subprocess.Route(
                self.executable, 'svn',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=1, elapsed=2),
            )]

        diff_name_routes = [
            mocks.Subprocess.Route(
                self.executable, '-c', 'diff.renames=false', 'diff', *listing, '-z',
                cwd=self.path,
                generator=self.diff_names,
            ) for listing in (['--name-only'], ['--name-only', '--staged'], ['--name-status', '--staged'])
        ]

        super(Git, self).__init__(
            mocks.Subprocess.Route(
                self.executable, 'symbolic-ref', '-q', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=1 if self.detached else 0,
                        stdout='' if self.detached else 'refs/heads/{}\n'.format(self.branch)
                    ),
            ),
            mocks.Subprocess.Route(
                self.executable, 'status',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='''HEAD detached at b8a315ed93c
nothing to commit, working tree clean
''' if self.detached else ''''On branch {branch}
Your branch is up to date with 'origin/{branch}'.

nothing to commit, working tree clean
'''.format(branch=self.branch),
                    ),
            ),
            mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--show-toplevel',
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.path),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--git-common-dir',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format('/main-repo/.git' if self.is_worktree else '.git'),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--git-dir',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='.git\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--abbrev-ref', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format('HEAD' if self.detached else self.branch),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'remote', 'get-url', '.*',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.remote),
                ) if args[3] == 'origin' else mocks.ProcessCompletion(
                    returncode=128,
                    stderr="fatal: No such remote '{}'\n".format(args[3]),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'remote', 'add', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.add_remote(args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'remote',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(set([key.split('/')[0] for key in self.remotes.keys()]))),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-a', '--format', '.+', '--merged', '.+',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.branch_merged_to(args[6]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-a',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(['* ' + self.branch] + list(({default_branch} | set(self.commits.keys())) - {self.branch}))) +
                           '\nremotes/origin/HEAD -> origin/{}\n'.format(default_branch) + \
                           '\n'.join(['  remotes/{}'.format(name) for name in self.remotes.keys() if default_branch in name]) + '\n' + \
                           '\n'.join(['  remotes/{}'.format(name) for name in self.remotes.keys() if default_branch not in name]) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'for-each-ref', '--format', re.compile(r'.+'), '--contains', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.for_each_ref(args[3], args[5], *args[6:]),
            ), mocks.Subprocess.Route(
                self.executable, 'for-each-ref', '--format', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.for_each_ref(args[3], None, *args[4:]),
            ), mocks.Subprocess.Route(
                self.executable, 'tag',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.tags.keys())) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'ls-remote', '--tags', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        '{hash}\trefs/tags/{tag}\n{hash}\trefs/tags/{tag}^{{}}'.format(hash=commit.hash, tag=tag) for tag, commit in sorted(self.tags.items())
                    ]) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--abbrev-ref', 'origin/HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='origin/{}\n'.format(default_branch),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '.*',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(commit.hash),
                ) if (commit := self.find(args[2])) else mocks.ProcessCompletion(returncode=128)
            ), mocks.Subprocess.Route(
                self.executable, 'log', re.compile(r'.+'), '-1', '--no-decorate', '--date=unix',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=
                        'commit {hash} (HEAD -> {branch}, origin/{branch}, origin/HEAD)\n'
                        'Author: {author} <{email}>\n'
                        'Date:   {date}\n'
                        '\n{log}'.format(
                            hash=commit.hash,
                            branch=self.branch,
                            author=commit.author.name,  # type: ignore[union-attr]
                            email=commit.author.email,  # type: ignore[union-attr]
                            date=commit.timestamp,
                            log='\n'.join(
                                [
                                    ('    ' + line) if line else '' for line in commit.message.splitlines()  # type: ignore[union-attr]
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    commit.revision,
                                )] if git_svn else []),
                            )
                        ),
                ) if (commit := self.find(args[2])) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'log', '--format=fuller', '--no-decorate', '--date=unix', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        'commit {hash}\n'
                        'Author:     {author} <{email}>\n'
                        'AuthorDate: {date}\n'
                        'Commit:     {author} <{email}>\n'
                        'CommitDate: {date}\n'
                        '\n{log}\n'.format(
                            hash=commit.hash,
                            author=commit.author.name,  # type: ignore[union-attr]
                            email=commit.author.email,  # type: ignore[union-attr]
                            date=commit.timestamp,
                            log='\n'.join(
                                [
                                    ('    ' + line) if line else '' for line in commit.message.splitlines()  # type: ignore[union-attr]
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    commit.revision,
                                )] if git_svn else []),
                            )
                        ) for commit in list(self.rev_list(args[5]))
                    ])
                )
            ),
            # We don't have modified files for our mock commits, so we assume that every scope
            # applies to all odd commits
            mocks.Subprocess.Route(
                self.executable, 'log', '--pretty=%H', re.compile(r'.+'), '--', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        commit.hash for commit in list(self.rev_list(args[3])) if commit.identifier % 2  # type: ignore[misc, operator]
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'log', re.compile(r'--max-count=\d+'), '--follow', '--format=%H', '--', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([  # type: ignore[arg-type]
                        commit.hash for commit in self.commits[self.branch] if commit.identifier % 2  # type: ignore[operator]
                    ][:int(args[2].split('=')[-1])])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'log', '--first-parent', '--format=%ct', re.compile(r'--since=.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=''.join([
                        '{}\n'.format(int(commit.timestamp)) for commit in self.rev_list(args[5])
                    ])
                ) if self.find(args[5]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'log', '--oneline', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=''.join([
                        '{hash} {subject}\n'.format(
                            hash=commit.hash[:7],  # type: ignore[index]
                            subject=commit.message.splitlines()[0],  # type: ignore[union-attr]
                        ) for commit in self.rev_list(args[3])
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'log', '--abbrev-commit', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.log(args[3], args, path, git_svn)
            ), mocks.Subprocess.Route(
                self.executable, '--no-replace-objects', 'log', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.log(args[3], args, path, git_svn)
            ), mocks.Subprocess.Route(
                self.executable, 'log', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.log(args[2], args, path, git_svn)
            ), mocks.Subprocess.Route(
                self.executable, '--no-replace-objects', 'rev-list', '--count', '--no-merges', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.rev_list_count(args[5]),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-list', '--count', '--no-merges', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.rev_list_count(args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-list', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(map(lambda commit: commit.hash, self.rev_list(args[2])))  # type: ignore[arg-type, return-value]
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'show', '-s', '--format=%ct', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(
                        commit.timestamp,
                    )
                ) if (commit := self.find(args[4])) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '--contains', re.compile(r'.+'), '-a',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.branches_on(commit))) + '\n'
                ) if (commit := self.find(args[3])) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', '-b', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], source=args[4] if len(args) > 4 else None, create=True) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', '-B', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], source=args[4] if len(args) > 4 else None, create=True, force=True) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[2], source=args[4] if len(args) > 4 else None, create=False) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'rebase', 'HEAD', re.compile(r'.+'), '--autostash',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], source=args[4] if len(args) > 4 else None, create=False) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f', '--env-filter', re.compile(r'.*'), '--msg-filter',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.filter_branch(
                    args[-1],
                    identifier_trailer=IdentifierTrailer.from_json(kwargs['env'].get('WEBKITSCMPY_CANONICALIZE_IDENTIFIER_TRAILER')),
                    environment_shell=args[4] if args[3] == '--env-filter' and args[4] else None,
                )
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f', '--env-filter', re.compile(r'.*'), '--msg-filter', re.compile(r'sed .*'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.filter_branch(
                    args[-1],
                    environment_shell=args[4] if args[3] == '--env-filter' and args[4] else None,
                    sed=args[6].split('sed ')[-1] if args[5] == '--msg-filter' else None,
                )
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'svn', 'fetch', '--log-window-size=5000', '-r', re.compile(r'\d+:HEAD'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if git_svn or local.Git(self.path).is_svn else mocks.ProcessCompletion(returncode=-1),
            ), mocks.Subprocess.Route(
                self.executable, 'pull',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.pull(),
            ), mocks.Subprocess.Route(
                self.executable, 'pull', '--rebase=True', '--autostash',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.pull(autostash=True),
            ), mocks.Subprocess.Route(
                self.executable, 'config', LIST_OPTION,
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join([f'{key}={value}' for key, value in self.config_entries()])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '--get-regexp', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join(['{} {}'.format(key, value) for key, value in self.config().items() if key.startswith(args[3])])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', LIST_OPTION, '--file', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join([
                            f'{key}={value}' for key, value in self.config_entries(path=os.path.join(self.path, args[4]))
                        ])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', LIST_OPTION, '--global',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join(['{}={}'.format(key, value) for key, value in Git.config().items()])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '--add', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self.edit_config(args[3], args[4], add=True),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '--replace-all', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self.edit_config(args[3], args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '--unset', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self.edit_config(args[3], value=None),
            ), mocks.Subprocess.Route(
                self.executable, 'config', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self.edit_config(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'fetch', re.compile(r'.+'),
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                ),
            ), *diff_name_routes, mocks.Subprocess.Route(
                self.executable, 'diff', '--cached', '--quiet',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=1, stdout=''),
            ), mocks.Subprocess.Route(
                self.executable, 'check-ref-format', re.compile(r'.+'),
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if re.match(r'^[A-Za-z0-9-]+/[A-Za-z0-9/-]+$', args[2]) else mocks.ProcessCompletion(),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--date=now',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=False, env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--date=now', '--amend',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=True, env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--amend', '-m', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=True, message=args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '-a', '-m', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(message=args[4], env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '-m', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(message=args[3], env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'apply', '--index', re.compile(r'.+'), '-3',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.apply(),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--no-commit', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(commit_hashes=[args[3]], no_commit=True),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--continue', '--no-edit',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(revert_continue=True),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--abort',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(revert_abort=True),
            ), mocks.Subprocess.Route(
                self.executable, 'cherry-pick', '-e',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.cherry_pick(args[3], env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'restore', '--staged', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.restore(args[3], staged=True),
            ), mocks.Subprocess.Route(
                self.executable, 'add', '--all',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.add_all(),
            ), mocks.Subprocess.Route(
                self.executable, 'add', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.add(args[2]),
            ), mocks.Subprocess.Route(
                self.executable, 'show-ref', '--verify', '--quiet', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.show_ref_verify(args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'push', '--porcelain', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.push_porcelain(args[3], args[4], force='-f' in args),
            ), mocks.Subprocess.Route(
                self.executable, 'push', '-f', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'fetch', 'origin', re.compile(r'.+:.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self._fetch_with_refspec(args[3:]),
            ), mocks.Subprocess.Route(
                self.executable, 'rebase', '--onto', re.compile(r'.+'), re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.rebase(args[3], args[4], args[5]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-f', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.move_branch(args[3], args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-D', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.delete_branch(args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.move_branch(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'push', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.push(args[2], args[4 if args[3] == '--delete' else 3].split(':')[0]),
            ), mocks.Subprocess.Route(
                self.executable, 'diff', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        '--- a/ChangeLog\n+++ b/ChangeLog\n@@ -1,0 +1,0 @@\n{}'.format(
                            '\n'.join(['+{}'.format(line) for line in commit.message.splitlines()])  # type: ignore[union-attr]
                        ) for commit in list(self.rev_list(args[2] if '..' in args[2] else '{}..HEAD'.format(args[2])))
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'format-patch', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        'From {hash}\n'
                        'From: {author} <{email}>\n'
                        'Date: {date}\n'
                        'Subject: [PATCH] {message}\n'
                        '---\n'
                        'diff --git a/ChangeLog b/ChangeLog\n'
                        '--- a/ChangeLog\n'
                        '+++ b/ChangeLog\n'
                        '@@ -1,0 +1,0 @@\n'
                        '{content}'.format(
                            hash=commit.hash,
                            author=commit.author.name,  # type: ignore[union-attr]
                            email=commit.author.email,  # type: ignore[union-attr]
                            date=datetime.fromtimestamp(commit.timestamp + time.timezone, timezone.utc).strftime('%a %b %d %H:%M:%S %Y +0000'),  # type: ignore[operator]
                            message=commit.message.rstrip(),  # type: ignore[union-attr]
                            content='\n'.join(['+{}'.format(line) for line in commit.message.splitlines()]),  # type: ignore[union-attr]
                        ) for commit in list(self.rev_list(args[2] if '..' in args[2] else '{}..HEAD'.format(args[2])))
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'reset', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.reset(int(args[2].split('~')[-1]) if '~' in args[2] else None),
            ), mocks.Subprocess.Route(
                self.executable, 'reset', '--hard',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.reset(int(args[2].split('~')[-1]) if '~' in args[2] else None),
            ), mocks.Subprocess.Route(
                self.executable, 'reset', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.reset_commit(args[2]),
            ), mocks.Subprocess.Route(
                self.executable, 'show', re.compile(r'.+'), '--pretty=', '--name-only',
                cwd=self.path,
                # FIXME: All mock commits have the same set of files changed with this implementation
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='Source/main.cpp\nSource/main.h\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'show', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='commit {hash}\n'
                        'Author: {author} <{email}>\n'
                        'Date:   {date}\n'
                        '\n{log}\n\n'
                        'diff --git a/Source/main.cpp b/Source/main.cpp\n'
                        'index 2deba859a126..7b85f5cecd66 100644\n'
                        '--- a/Source/main.cpp\n'
                        '--- b/Source/main.cpp\n'
                        '@@ -2948,6 +2948,8 @@ Vector<CompositedClipData> RenderLayerCompositor::computeAncestorClippingStack(c\n'
                        '     auto backgroundClip = clippedLayer.backgroundClipRect(RenderLayer::ClipRectsContext(&clippingRoot, TemporaryClipRects, options));\n'
                        '     ASSERT(!backgroundClip.affectedByRadius());\n'
                        '     auto clipRect = backgroundClip.rect();\n'
                        '+    if (clipRect.isInfinite())\n'
                        '+        return;\n'
                        '    auto offset = layer.convertToLayerCoords(&clippingRoot, {{ }}, RenderLayer::AdjustForColumns);\n'
                        '    clipRect.moveBy(-offset);\n'.format(
                            hash=commit.hash,
                            author=commit.author.name,  # type: ignore[union-attr]
                            email=commit.author.email,  # type: ignore[union-attr]
                            date=commit.timestamp if '--date=unix' in args else datetime.fromtimestamp(commit.timestamp + time.timezone, timezone.utc).strftime('%a %b %d %H:%M:%S %Y +0000'),  # type: ignore[operator]
                            log='\n'.join(
                                [
                                    ('    ' + line) if line else '' for line in commit.message.splitlines()  # type: ignore[union-attr]
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    commit.revision,
                                )] if git_svn else [])
                            )
                        )
                ) if (commit := self.find(args[2])) else mocks.ProcessCompletion(returncode=128)
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '--set-upstream-to', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                ) if args[4] in self.remotes else mocks.ProcessCompletion(returncode=128, stderr="fatal: branch '{}' does not exist".format(args[4])),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '--track', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                ) if args[4] in self.remotes else mocks.ProcessCompletion(returncode=128, stderr="fatal: branch '{}' does not exist".format(args[4])),
            ), mocks.Subprocess.Route(
                self.executable, 'merge-base', '--is-ancestor', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.is_ancestor(args[3], args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'merge-base', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.merge_base(args[2], *args[3:]),
            ), mocks.Subprocess.Route(
                self.executable, 'update-ref', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.update_ref(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr='usage: git [--version] [--help]...\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'status',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout=''''On branch {branch}
Your branch is up to date with 'origin/{branch}'.

nothing to commit, working tree clean
'''.format(branch=self.branch),
                    ),
            ), mocks.Subprocess.Route(
                self.executable,
                completion=mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: not a git repository (or any parent up to mount point)\nStopping at filesystem boundary (GIT_DISCOVERY_ACROSS_FILESYSTEM not set).\n',
                ),
            ), mocks.Subprocess.Route(
                'sudo', 'sh', re.compile(r'.+/install.sh'),
                generator=lambda *args, **kwargs: self._install_git_lfs(),
            ), mocks.Subprocess.Route(
                self.executable, 'lfs', '--version',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='git-lfs/3.4.0 (???)\n',
                    ) if self.has_git_lfs else mocks.ProcessCompletion(
                        returncode=1,
                        stderr='usage: git [--version] [--help]...\n',
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'lfs', 'install',
                generator=lambda *args, **kwargs: self._configure_git_lfs(),
            ), mocks.Subprocess.Route(
                '/bin/Example Program', '-n', '-w',
                generator=editor_generator,
            ), *git_svn_routes
        )

    def __enter__(self: GitType) -> GitType:
        local.Git.executable.clear()  # Clear the memoized cache prior to patching
        p: _patch[Any] = patch('shutil.which', lambda cmd: self.executable if cmd == 'git' else p.temp_original(cmd))
        self.patches.append(p)
        return super(Git, self).__enter__()

    def __exit__(self, typ: type[BaseException] | None, exc: BaseException | None, tb: TracebackType | None) -> None:
        super().__exit__(typ, exc, tb)
        local.Git.executable.clear()  # Clear the memoized cache after patching

    def diff_names(self, *args: str, **kwargs: Any) -> mocks.ProcessCompletion:
        files = self.staged if '--staged' in args else {key: value for key, value in self.modified.items() if value.startswith('diff')}
        entries = [('M' if value.startswith('diff') else 'A', key) for key, value in sorted(files.items())]
        if '--name-status' in args:
            return mocks.ProcessCompletion(returncode=0, stdout=''.join('{}\0{}\0'.format(*entry) for entry in entries))
        return mocks.ProcessCompletion(returncode=0, stdout=''.join('{}\0'.format(key) for _, key in entries))

    @property
    def branch(self) -> str:
        assert self.head.branch is not None  # Every mock commit is on a branch
        return self.head.branch

    def find(self, something: str) -> Commit | None:
        if '~' in something:
            split = something.split('~')
            if len(split) == 2 and Commit.NUMBER_RE.match(split[1]):
                found = self.find(split[0])
                assert found is not None  # found.branch would raise an AttributeError
                difference = int(split[1])
                if split[0] in self.remotes:
                    all_commits = self.resolve_all_commits(found.branch, remote=split[0].replace('/{}'.format(found.branch), ''))
                else:
                    all_commits = self.resolve_all_commits(found.branch)
                head_index = None
                for i in range(len(all_commits)):
                    if found == all_commits[i]:
                        head_index = i
                if head_index is not None and head_index - difference >= 0:
                    return all_commits[head_index - difference]
                return None

        if something in self.commits.keys():
            return self.commits[something][-1]

        something = str(something).replace('refs/remotes/', '')
        something = str(something).replace('remotes/', '')
        if '..' in something:
            a, b = something.split('..')
            a_commit = self.find(a)
            b_commit = self.find(b)
            return b_commit if a_commit and b_commit else None

        if something == 'HEAD':
            return self.head
        if something in self.tags.keys():
            return self.tags[something]
        if something in self.remotes.keys():
            return self.remotes[something][-1]

        for branch, commits in self.commits.items():
            if branch == something:
                return commits[-1]
            for commit in commits:
                if something == str(commit.revision):
                    return commit
                assert commit.hash is not None  # Every mock commit has a hash
                if len(something) > 4 and commit.hash.startswith(str(something)):
                    return commit
        return None

    def count(self, something: str) -> int:
        rev_list = self.rev_list(something)
        return len(rev_list)

    def rev_list_count(self, ref: str) -> mocks.ProcessCompletion:
        """Helper for git rev-list --count --no-merges"""
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='{}\n'.format(self.count(ref))
        ) if self.find(ref) else mocks.ProcessCompletion(returncode=128)

    def decoration(self, commit: Commit) -> str:
        branches = []
        for branch, commits in self.commits.items():
            if commits[-1] == commit:
                branches.append(branch)
        if branches:
            return ' ({})'.format(', '.join(sorted(branches)))
        return ''

    def log(self, ref: str, args: tuple[str, ...], path: str, git_svn: bool) -> mocks.ProcessCompletion:
        """Helper for git log"""
        decorate = '--decorate' in args
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='\n'.join([
                'commit {hash}{decoration}\n'
                'Author: {author} <{email}>\n'
                'Date:   {date}\n'
                '\n{log}\n'.format(
                    hash=commit.hash[:7] if '--abbrev-commit' in args else commit.hash,  # type: ignore[index]
                    decoration=self.decoration(commit) if decorate else '',
                    author=commit.author.name,  # type: ignore[union-attr]
                    email=commit.author.email,  # type: ignore[union-attr]
                    date=commit.timestamp if '--date=unix' in args else datetime.fromtimestamp(commit.timestamp + time.timezone, timezone.utc).strftime('%a %b %d %H:%M:%S %Y +0000'),  # type: ignore[operator]
                    log='\n'.join(
                        [
                            ('    ' + line) if line else '' for line in commit.message.splitlines()  # type: ignore[union-attr]
                        ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                            self.remote.split('@')[-1].split(':')[0],
                            os.path.basename(path),
                            commit.revision,
                        )] if git_svn else [])
                    )
                ) for commit in self.rev_list(ref)
            ])
        )

    def branches_on(self, commit: Commit) -> set[str]:
        result = set()
        found_identifier = 0
        for branch in self.commits.keys():
            commits = self.resolve_all_commits(branch)
            if commit in commits:
                result.add(branch)
        for remote_branch in self.remotes.keys():
            remote, branch = remote_branch.split('/', 1)
            commits = self.resolve_all_commits(branch, remote)
            if commit in commits:
                result.add(f'remotes/{remote_branch}')
        return result

    def checkout(self, something: str, source: str | None = None, create: bool = False, force: bool = False) -> bool | mocks.ProcessCompletion:
        if not source or source.startswith('--'):
            source = something
        if source in self.modified:
            del self.modified[something]
            return mocks.ProcessCompletion(returncode=0, stdout='Updated 1 path from the index')

        commit = self.find(source)
        if commit and source in self.commits:
            self.commits[something] = self.commits[source]

        if create:
            if commit:
                if force:
                    if source == something:
                        # checkout -B branch (no start-point): reset to current HEAD
                        del self.commits[something]
                        # Fall through to create branch from current HEAD
                    else:
                        # checkout -B branch start-point: reset branch to start-point
                        self.head = commit
                        self.detached = False
                        return True
                else:
                    return False
            elif source != something:
                # Source was explicitly provided but doesn't exist: fail
                return False
            self.commits[something] = [Commit.from_json(Commit.Encoder().default(self.head))]
            # Copy one more to create a bridge commit
            self.commits[something].append(Commit.from_json(Commit.Encoder().default(self.head)))
            self.head = self.commits[something][-1]
            setattr(self.head, 'bridge_commit', True)
            self.head.branch = something
            if not self.head.branch_point:
                self.head.branch_point = self.head.identifier
                self.head.identifier = 0
            return True

        if commit:
            self.head = commit
            self.detached = something not in self.commits.keys()
        return True if commit else False

    def filter_branch(self, range: str, identifier_trailer: IdentifierTrailer | None = None, environment_shell: str | None = None, sed: str | None = None, autostash: bool = False) -> mocks.ProcessCompletion:
        if not autostash and (self.modified or self.staged):
            return mocks.ProcessCompletion(returncode=128)

        # We can't effectively mock the bash script in the command, but we can mock the python code that
        # script calls, which is where the program logic is.
        head_ref, start_ref = range.split('...')
        head = self.find(head_ref)
        start = self.find(start_ref)
        assert head is not None and head.branch is not None  # head.branch would raise an AttributeError
        assert start is not None and start.identifier is not None  # start.identifier would raise an AttributeError

        commits_to_edit: list[Commit] = []
        for commit in reversed(self.commits[head.branch]):
            assert commit.identifier is not None  # Every mock commit has an identifier
            if commit.branch == start.branch and commit.identifier <= start.identifier:
                break
            commits_to_edit.insert(0, commit)
        if head.branch != self.default_branch:
            for commit in reversed(self.commits[self.default_branch][:head.branch_point]):
                assert commit.identifier is not None  # Every mock commit has an identifier
                if commit.identifier <= start.identifier:
                    break
                commits_to_edit.insert(0, commit)

        stdout = StringIO()
        original_env = {key: os.environ.get('OLDPWD') for key in [
            'OLDPWD', 'GIT_COMMIT',
            'GIT_AUTHOR_NAME', 'GIT_AUTHOR_EMAIL',
            'GIT_COMMITTER_NAME', 'GIT_COMMITTER_EMAIL',
        ]}

        try:
            count = 0
            os.environ['OLDPWD'] = self.path
            for commit in commits_to_edit:
                assert commit.hash is not None and commit.author is not None and commit.author.email is not None  # os.environ only accepts strings
                count += 1
                os.environ['GIT_COMMIT'] = commit.hash
                os.environ['GIT_AUTHOR_NAME'] = commit.author.name
                os.environ['GIT_AUTHOR_EMAIL'] = commit.author.email
                os.environ['GIT_COMMITTER_NAME'] = commit.author.name
                os.environ['GIT_COMMITTER_EMAIL'] = commit.author.email

                stdout.write(
                    'Rewrite {hash} ({count}/{total}) (--- seconds passed, remaining --- predicted)\n'.format(
                        hash=commit.hash,
                        count=count,
                        total=len(commits_to_edit),
                    ))

                if identifier_trailer:
                    assert commit.message is not None  # write(None) would raise a TypeError
                    messagefile = StringIO()
                    messagefile.write(commit.message)
                    messagefile.seek(0)
                    with OutputCapture() as captured:
                        message_main(messagefile, identifier_trailer)
                    lines = captured.stdout.getvalue().splitlines()
                    if lines[-1].startswith('git-svn-id: https://svn'):
                        lines.pop(-1)
                    commit.message = '\n'.join(lines)

                if sed:
                    match = re.match(r'"s/(?P<re>.+)/(?P<value>.+)/g"', sed)
                    if match:
                        assert commit.message is not None  # re.sub() would raise a TypeError
                        commit.message = re.sub(
                            match.group('re').replace('(', '\\(').replace(')', '\\)'),
                            match.group('value'), commit.message,
                        )

                if not environment_shell:
                    continue
                if re.search(r'echo "Overwriting', environment_shell):
                    stdout.write('Overwriting {}\n'.format(commit.hash))

                match = re.search(r'(?P<json>\S+\.json)', environment_shell)
                if match:
                    with OutputCapture() as captured:
                        committer_main(match.group('json'))
                    captured.stdout.seek(0)
                    for line in captured.stdout.readlines():
                        line = line.rstrip()
                        os.environ[line.split(' ')[0]] = ' '.join(line.split(' ')[1:])

                commit.author = Contributor(name=os.environ['GIT_AUTHOR_NAME'], emails=[os.environ['GIT_AUTHOR_EMAIL']])

                if re.search(r'echo "\s+', environment_shell):
                    for key in ['GIT_AUTHOR_NAME', 'GIT_AUTHOR_EMAIL', 'GIT_COMMITTER_NAME', 'GIT_COMMITTER_EMAIL']:
                        stdout.write('    {}={}\n'.format(key, os.environ[key]))

        finally:
            for key, value in original_env.items():
                if value is not None:
                    os.environ[key] = value
                else:
                    del os.environ[key]

        return mocks.ProcessCompletion(
            returncode=0,
            stdout=stdout.getvalue(),
        )

    @decorators.hybridmethod
    def config(context: Any, path: str | None = None) -> OrderedDict[str, str]:
        if isinstance(context, type):
            return OrderedDict({
                'user.name': 'Tim Apple',
                'user.email': 'tapple@webkit.org',
                'sendemail.transferencoding': 'base64',
            })

        return OrderedDict(context.config_entries(path=path))

    def config_entries(self, path: str | None = None) -> list[tuple[str, str]]:
        """Every configured value in order, keeping the repeated keys git allows a single option."""
        result = list(Git.config().items())
        path = path or os.path.join(self.path, '.git', 'config')
        if not os.path.isfile(path):
            return result

        top = None
        with open(path, 'r') as configfile:
            for line in configfile.readlines():
                match = self.RE_MULTI_TOP.match(line)
                if match:
                    top = f"{match.group('keya')}.{match.group('keyb')}"
                    continue
                match = self.RE_SINGLE_TOP.match(line)
                if match:
                    top = match.group('key')
                    continue

                match = self.RE_ELEMENT.match(line)
                if top and match:
                    result.append((f"{top}.{match.group('key')}", match.group('value')))
        return result

    def edit_config(self, key: str, value: str | None, add: bool = False) -> mocks.ProcessCompletion:
        with open(os.path.join(self.path, '.git', 'config'), 'r') as configfile:
            lines = [line for line in configfile.readlines()]

        key_a = key.split('.')[0]
        key_b = '.'.join(key.split('.')[1:])

        did_print = False
        with open(os.path.join(self.path, '.git', 'config'), 'w') as configfile:
            for line in lines:
                match = self.RE_ELEMENT.match(line)
                if add or not match or match.group('key') != key_b:
                    configfile.write(line)
                match = self.RE_MULTI_TOP.match(line)
                if not match or '{}.{}'.format(match.group('keya'), match.group('keyb')) != key_a:
                    continue
                if value is not None:
                    configfile.write('\t{}={}\n'.format(key_b, value))
                did_print = True

            if not did_print and value is not None:
                configfile.write('[{}]\n'.format(key_a))
                configfile.write('\t{}={}\n'.format(key_b, value))

        return mocks.ProcessCompletion(returncode=0)

    def apply(self, patch: str | None = None) -> mocks.ProcessCompletion:
        self.staged['patch.txt'] = 'added'
        return mocks.ProcessCompletion(returncode=0)

    def commit(self, amend: bool = False, message: str | None = None, env: Mapping[str, str] | None = None) -> mocks.ProcessCompletion:
        env = env or dict()
        if not self.head:
            return mocks.ProcessCompletion(returncode=1, stdout='Allowed in git, but disallowed by reasonable workflows')
        if not self.staged and not amend:
            return mocks.ProcessCompletion(returncode=1, stdout='no changes added to commit (use "git add" and/or "git commit -a")\n')

        if not amend:
            # Remove the temp bridge commit
            if hasattr(self.head, 'bridge_commit'):
                assert self.head.branch is not None  # Every mock commit is on a branch
                self.commits[self.head.branch].remove(self.head)
            assert self.head.identifier is not None  # self.head.identifier + 1 would raise a TypeError
            self.head = Commit(
                branch=self.branch, repository_id=self.head.repository_id,
                timestamp=int(time.time()),
                identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                branch_point=self.head.branch_point or self.head.identifier,
            )
            self.commits[self.branch].append(self.head)

        self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
        if message:
            self.head.message = message
        else:
            title = env.get('COMMIT_MESSAGE_TITLE', '') or '[Testing] {} commits'.format('Amending' if amend else 'Creating')
            reviewed_by = '' if re.match(r'(Unreviewed|Versioning.)', title, re.IGNORECASE) else '\nReviewed by Jonathan Bedard'
            self.head.message = '{}{}{}\n\n * {}\n{}'.format(
                title,
                ('\n' + env.get('COMMIT_MESSAGE_BUG', '')) if env.get('COMMIT_MESSAGE_BUG', '') else '',
                reviewed_by,
                '\n * '.join(self.staged.keys()),
                env.get('COMMIT_MESSAGE_CONTENT', '')
            )
        self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
        self.staged = {}
        return mocks.ProcessCompletion(returncode=0)

    def revert(self, commit_hashes: list[str] = [], no_commit: bool = False, revert_continue: bool = False, revert_abort: bool = False) -> mocks.ProcessCompletion:
        if revert_continue:
            if not self.staged:
                return mocks.ProcessCompletion(returncode=1, stdout='error: no cherry-pick or revert in progress\nfatal: revert failed')
            self.staged = {}
            assert self.head.identifier is not None  # self.head.identifier + 1 would raise a TypeError
            self.head = Commit(
                branch=self.branch, repository_id=self.head.repository_id,
                timestamp=int(time.time()),
                identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                branch_point=self.head.branch_point or self.head.identifier,
                message=self.revert_message
            )
            self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
            assert self.head.message is not None  # sha256(None) would raise a TypeError
            self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
            self.commits[self.branch].append(self.head)
            self.revert_message = None
            return mocks.ProcessCompletion(returncode=0)

        if revert_abort:
            if not self.staged:
                return mocks.ProcessCompletion(returncode=1, stdout='error: no cherry-pick or revert in progress\nfatal: revert failed')
            self.staged = {}
            self.revert_message = None
            return mocks.ProcessCompletion(returncode=0)

        if self.modified:
            return mocks.ProcessCompletion(returncode=1, stdout='error: your local changes would be overwritten by revert.')

        is_reverted_something = False
        for hash in commit_hashes:
            commit_revert = self.find(hash)
            assert commit_revert is not None and commit_revert.message is not None  # commit_revert.message.splitlines() would raise an AttributeError
            if not no_commit:
                assert self.head.identifier is not None  # self.head.identifier + 1 would raise a TypeError
                self.head = Commit(
                    branch=self.branch, repository_id=self.head.repository_id,
                    timestamp=int(time.time()),
                    identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                    branch_point=self.head.branch_point or self.head.identifier,
                    message='Revert "{}"\n\nThis reverts commit {}'.format(commit_revert.message.splitlines()[0], hash)
                )
                self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
                assert self.head.message is not None  # Set just above
                self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
                self.commits[self.branch].append(self.head)
            else:
                self.staged['{}/some_file'.format(hash)] = 'modified'
                self.staged['{}/ChangeLog'.format(hash)] = 'modified'
                # git revert only generate one commit message
                self.revert_message = 'Revert "{}"\n\nThis reverts commit {}'.format(commit_revert.message.splitlines()[0], hash)

            is_reverted_something = True
        if not is_reverted_something:
            return mocks.ProcessCompletion(returncode=1, stdout='On branch {}\nnothing to commit, working tree clean'.format(self.branch))

        return mocks.ProcessCompletion(returncode=0)

    def cherry_pick(self, hash: str, env: Mapping[str, str] | None = None) -> mocks.ProcessCompletion:
        commit = self.find(hash)
        env = env or dict()

        if self.staged:
            return mocks.ProcessCompletion(returncode=1, stdout='error: your local changes would be overwritten by cherry-pick.\nfatal: cherry-pick failed\n')
        if not commit:
            return mocks.ProcessCompletion(returncode=128, stdout="fatal: bad revision '{}'\n".format(hash))
        assert commit.message is not None  # commit.message.splitlines() would raise an AttributeError
        assert self.head.identifier is not None  # self.head.identifier + 1 would raise a TypeError

        self.head = Commit(
            branch=self.branch, repository_id=self.head.repository_id,
            timestamp=int(time.time()),
            identifier=self.head.identifier + 1 if self.head.branch_point else 1,
            branch_point=self.head.branch_point or self.head.identifier,
            message='Cherry-pick {}. {}\n    {}\n'.format(
                env.get('GIT_WEBKIT_CHERRY_PICKED', '') or commit.hash,
                env.get('COMMIT_MESSAGE_BUG', '') or '<bug>',
                '\n    '.join(commit.message.splitlines()),
            ),
        )
        self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
        assert self.head.message is not None  # Set just above
        self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
        self.commits[self.branch].append(self.head)

        return mocks.ProcessCompletion(returncode=0)

    def restore(self, file: str, staged: bool = False) -> mocks.ProcessCompletion:
        if staged:
            if file in self.staged:
                self.modified[file] = self.staged[file]
                del self.staged[file]
                return mocks.ProcessCompletion(returncode=0)
            return mocks.ProcessCompletion(returncode=0)
        return mocks.ProcessCompletion(returncode=1)

    def add(self, file: str) -> mocks.ProcessCompletion:
        if file not in self.modified:
            return mocks.ProcessCompletion(returncode=128, stdout="fatal: pathspec '{}' did not match any files\n".format(file))
        for key, value in self.modified.items():
            self.staged[key] = value
        del self.modified[file]
        return mocks.ProcessCompletion(returncode=0)

    def add_all(self) -> mocks.ProcessCompletion:
        for key, value in self.modified.items():
            self.staged[key] = value
        self.modified = {}
        return mocks.ProcessCompletion(returncode=0)

    def rebase(self, target: str, base: str, head: str) -> mocks.ProcessCompletion:
        if target not in self.commits or base not in self.commits or head not in self.commits:
            return mocks.ProcessCompletion(returncode=1)

        base_commit = self.commits[target][-1]
        self.commits[head][0] = base_commit
        for commit in self.commits[head][1:]:
            commit.branch_point = base_commit.branch_point or base_commit.identifier
            if base_commit.branch_point:
                assert commit.identifier is not None and base_commit.identifier is not None  # += would raise a TypeError
                commit.identifier += base_commit.identifier
        return mocks.ProcessCompletion(returncode=0)

    def pull(self, autostash: bool = False) -> mocks.ProcessCompletion:
        if not autostash and (self.modified or self.staged):
            return mocks.ProcessCompletion(returncode=128)
        assert self.head.branch is not None  # Every mock commit is on a branch
        self.head = self.commits[self.head.branch][-1]
        return mocks.ProcessCompletion(returncode=0)

    def move_branch(self, to_be_moved: str, moved_to: str) -> mocks.ProcessCompletion:
        if moved_to.startswith('remotes/'):
            moved_to = moved_to.split('/', 2)[-1]
        if moved_to == self.default_branch:
            return mocks.ProcessCompletion(returncode=0)
        if to_be_moved != self.default_branch:
            self.commits[to_be_moved] = self.commits[moved_to]
            self.head = self.commits[to_be_moved][-1]
            return mocks.ProcessCompletion(returncode=0)
        self.commits[to_be_moved] += [
            Commit(
                branch=to_be_moved, repository_id=commit.repository_id,
                timestamp=commit.timestamp,
                identifier=commit.identifier + (commit.branch_point or 0), branch_point=None,  # type: ignore[operator]
                hash=commit.hash, revision=commit.revision,
                author=commit.author, message=commit.message,
            ) for commit in self.commits[moved_to]
        ]
        self.head = self.commits[to_be_moved][-1]
        return mocks.ProcessCompletion(returncode=0)

    def delete_branch(self, branch: str) -> mocks.ProcessCompletion:
        if branch in self.commits:
            del self.commits[branch]
            return mocks.ProcessCompletion(returncode=0)
        return mocks.ProcessCompletion(
            returncode=1,
            stdout="error: branch '{}' not found.\n".format(branch),
        )

    def push(self, remote: str, branch: str) -> mocks.ProcessCompletion:
        remote_branch = '{}/{}'.format(remote, branch)
        if branch in self.commits:
            self.remotes[remote_branch] = self.commits[branch][:]
        elif remote_branch in self.remotes:
            del self.remotes[remote_branch]
        return mocks.ProcessCompletion(returncode=0)

    def show_ref_verify(self, ref: str) -> mocks.ProcessCompletion:
        branch = ref.replace('refs/heads/', '')
        if branch in self.commits:
            return mocks.ProcessCompletion(returncode=0)
        return mocks.ProcessCompletion(returncode=1)

    def push_porcelain(self, remote: str, refspec: str, force: bool = False) -> mocks.ProcessCompletion:
        if self.push_error is not None:
            return mocks.ProcessCompletion(returncode=self.push_error)

        local_branch = refspec.split(':')[0]
        remote_ref = refspec.split(':')[-1] if ':' in refspec else local_branch
        remote_branch = '{}/{}'.format(remote, remote_ref)

        if not force and remote_branch in self.remotes:
            return mocks.ProcessCompletion(
                returncode=1,
                stdout='!\trefs/heads/{ref}:refs/heads/{ref}\t[rejected] (non-fast-forward)\n'.format(ref=remote_ref),
            )

        if local_branch in self.commits:
            self.remotes[remote_branch] = self.commits[local_branch][:]
        return mocks.ProcessCompletion(returncode=0)

    def _fetch_with_refspec(self, refspecs: Sequence[str]) -> mocks.ProcessCompletion:
        """Handle fetch with one or more refspecs like 'main:main'.

        Simulates git's behavior of refusing to fetch into a branch
        that is checked out in any worktree, and updates local refs
        from the remote.
        """
        for refspec in refspecs:
            if refspec == '--prune':
                continue
            if refspec.startswith('-'):
                raise ValueError('Negative refspecs are not supported by this mock: {}'.format(refspec))
            src, dst = refspec.lstrip('+').split(':', 1)
            if dst.startswith('refs/heads/'):
                branch = dst[len('refs/heads/'):]
            elif '/' not in dst:
                branch = dst
            else:
                continue

            if self.is_worktree:
                return mocks.ProcessCompletion(
                    returncode=1,
                    stderr="fatal: refusing to fetch into branch 'refs/heads/{}' checked out at '/other/worktree'\n".format(branch),
                )

            remote_key = 'origin/{}'.format(src if '/' not in src else src.split('/')[-1])
            if remote_key not in self.remotes:
                return mocks.ProcessCompletion(
                    returncode=128,
                    stderr="fatal: couldn't find remote ref {}\n".format(src),
                )
            self.commits[branch] = self.remotes[remote_key][:]

        return mocks.ProcessCompletion(returncode=0)

    def dcommit(self, remote: str = 'origin', branch: str | None = None) -> mocks.ProcessCompletion:
        branch = branch or self.default_branch
        self.remotes['{}/{}'.format(remote, branch)] = self.commits[branch][:]
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Committed r{}\n\tM\tFiles/Changed.txt\n'.format(self.commits[branch][-1].revision),
        )

    def reset_commit(self, something: str) -> mocks.ProcessCompletion:
        commit = self.find(something)
        pre_branch = self.branch
        rev_list = self.rev_list('HEAD...{}'.format(something))
        commits = self.commits[self.branch]
        if commit is not None:
            self.head = commit
        for commit in rev_list:
            if hasattr(commit, '__mock__changeFiles'):
                files = getattr(commit, '__mock__changeFiles')
                for file in files:
                    self.modified[file] = files[file]
            commits.remove(commit)
        if pre_branch != self.branch:
            # Add a fake commit to simulate a same commit in different branch
            bridge_commit = Commit(
                hash=self.head.hash, revision=self.head.revision,
                identifier=self.head.identifier, branch=pre_branch, branch_point=self.head.branch_point,
                timestamp=self.head.timestamp, author=self.head.author, message=self.head.message,
                order=self.head.order, repository_id=self.head.repository_id
            )
            setattr(bridge_commit, 'bridge_commit', True)
            commits.append(bridge_commit)
            self.head = commits[-1]
        return mocks.ProcessCompletion(returncode=0)

    def reset(self, index: int | None) -> mocks.ProcessCompletion:
        if index is None:
            self.modified = {}
            self.staged = {}
            return mocks.ProcessCompletion(returncode=0)

        assert self.head.branch is not None  # Every mock commit is on a branch
        self.head = self.commits[self.head.branch][-(index + 1)]
        return mocks.ProcessCompletion(returncode=0)

    def resolve_all_commits(self, branch: str | None, remote: str | None = None) -> list[Commit]:
        assert branch is not None  # self.commits[None] would raise a KeyError
        if not remote:
            all_commits = self.commits[branch][:]
        else:
            all_commits = self.remotes['{}/{}'.format(remote, branch)][:]
        last_commit = all_commits[0]
        while last_commit.branch != branch:
            assert last_commit.branch is not None  # Every mock commit is on a branch
            head_index = None
            if not remote:
                commits_part = self.commits[last_commit.branch]
            else:
                commits_part = self.remotes['{}/{}'.format(remote, last_commit.branch)]
            for i in range(len(commits_part)):
                if commits_part[i].hash == last_commit.hash:
                    head_index = i
                    break
            all_commits = commits_part[:head_index] + all_commits
            last_commit = all_commits[0]
            if last_commit.branch == self.default_branch and last_commit.identifier == 1:
                break
        if remote:
            for commit in all_commits:
                setattr(commit, '__mock__remotes', set([remote]))
        return all_commits

    def rev_list(self, something: str) -> list[Commit]:
        """
        A..B = A u B - A
        A...B = A u B - A n B
        """
        two_dots = False
        triple_dots = False
        a_commit = None
        a_remote = None
        b_commit = None
        b_remote = None
        if '...' in something:
            refs = something.split('...')
            triple_dots = True
            a_commit = self.find(refs[0])
            b_commit = self.find(refs[1])
            if refs[0] in self.remotes:
                assert a_commit is not None  # a_commit.branch would raise an AttributeError
                a_remote = refs[0].replace('/{}'.format(a_commit.branch), '')
            if refs[1] in self.remotes:
                assert b_commit is not None  # b_commit.branch would raise an AttributeError
                b_remote = refs[1].replace('/{}'.format(b_commit.branch), '')
        elif '..' in something:
            refs = something.split('..')
            two_dots = True
            a_commit = self.find(refs[0])
            b_commit = self.find(refs[1])
            if refs[0] in self.remotes:
                assert a_commit is not None  # a_commit.branch would raise an AttributeError
                a_remote = refs[0].replace('/{}'.format(a_commit.branch), '')
            if refs[1] in self.remotes:
                assert b_commit is not None  # b_commit.branch would raise an AttributeError
                b_remote = refs[1].replace('/{}'.format(b_commit.branch), '')
        else:
            a_commit = self.find(something)
            if something in self.remotes:
                assert a_commit is not None  # a_commit.branch would raise an AttributeError
                a_remote = something.replace('/{}'.format(a_commit.branch), '')

        a_commits = []
        a_branch_commits = self.resolve_all_commits(a_commit.branch, remote=a_remote) if a_commit else []
        for commit in a_branch_commits:
            assert a_commit is not None  # a_branch_commits is empty otherwise
            a_commits.append(commit)
            if commit.hash == a_commit.hash:
                break

        b_commits = []
        b_branch_commits = self.resolve_all_commits(b_commit.branch, remote=b_remote) if b_commit else []
        for commit in b_branch_commits:
            assert b_commit is not None  # b_branch_commits is empty otherwise
            b_commits.append(commit)
            if commit.hash == b_commit.hash:
                break

        if not two_dots and not triple_dots:
            return list(reversed(a_commits))

        res = []
        # To make things easier, we only mock that two branch will share same init commit
        assert a_commits[0].hash == b_commits[0].hash
        for i in range(max(len(a_commits), len(b_commits))):
            if i >= len(a_commits) and i < len(b_commits):
                res.append(b_commits[i])
            elif i >= len(b_commits) and i < len(a_commits):
                if triple_dots:
                    res.append(a_commits[i])
            elif i < len(b_commits) and i < len(a_commits) and a_commits[i].hash != b_commits[i].hash:
                if triple_dots:
                    res.append(a_commits[i])
                    res.append(b_commits[i])
                if two_dots:
                    res.append(b_commits[i])
        res.reverse()
        return res

    def _install_git_lfs(self) -> mocks.ProcessCompletion:
        self.has_git_lfs = True
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Git LFS initialized.\n',
        )

    def _configure_git_lfs(self) -> mocks.ProcessCompletion:
        if not self.has_git_lfs:
            return mocks.ProcessCompletion(
                returncode=1,
                stderr='usage: git [--version] [--help]...\n',
            )
        self.edit_config('lfs.repositoryformatversion', '0')
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Updated Git hooks.\nGit LFS initialized.\n',
        )

    def merge_base(self, *refs: str) -> mocks.ProcessCompletion:
        objs = [self.find(ref) for ref in refs]
        for i in range(len(objs)):
            if not refs[i] or not objs[i]:
                return mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: Not a valid object name {}\n'.format(refs[i]),
                )

        commits = [obj for obj in objs if obj]  # The loop above returned if any ref wasn't found

        def pair_base(*values: Commit) -> Commit:
            objs = list(values)
            if objs[0].branch != objs[1].branch:
                for i in [0, 1]:
                    if objs[i].branch == self.default_branch:
                        continue
                    branch_point = objs[i].branch_point
                    assert branch_point is not None  # branch_point - 1 would raise a TypeError
                    objs[i] = self.commits[self.default_branch][branch_point - 1]

            assert objs[0].identifier is not None and objs[1].identifier is not None  # < would raise a TypeError
            return objs[0] if objs[0].identifier < objs[1].identifier else objs[1]

        if len(commits) > 1:
            commits = [pair_base(commits[0], obj) for obj in commits[1:]]
        if len(commits) > 1:
            commits = sorted(commits, key=lambda obj: obj.identifier + (obj.branch_point or 0), reverse=True)  # type: ignore[operator]

        return mocks.ProcessCompletion(
            returncode=0,
            stdout='{}\n'.format(commits[0].hash),
        )

    def branch_merged_to(self, ref: str) -> mocks.ProcessCompletion:
        obj = self.find(ref)
        if not obj:
            return mocks.ProcessCompletion(
                returncode=128,
                stderr='fatal: Not a valid object name {}\n'.format(ref),
            )

        out = ''
        for branch, commits in self.commits.items():
            if commits and commits[-1].hash == obj.hash:
                out += ' refs/heads/{}\n'.format(branch)

        return mocks.ProcessCompletion(
            returncode=0,
            stdout=out or '\n',
        )

    def is_ancestor(self, ancestor: str, descendent: str) -> mocks.ProcessCompletion:
        ancestor_commit = self.find(ancestor)
        descendent_commit = self.find(descendent)
        for ref, commit in [(ancestor, ancestor_commit), (descendent, descendent_commit)]:
            if not commit:
                return mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: Not a valid object name {}\n'.format(ref),
                )
        assert ancestor_commit is not None  # Checked above

        return mocks.ProcessCompletion(returncode=0 if any(commit.hash == ancestor_commit.hash for commit in self.rev_list(descendent)) else 1)

    def update_ref(self, ref: str, value: str) -> mocks.ProcessCompletion:
        commit = self.find(value)
        if not commit:
            return mocks.ProcessCompletion(
                returncode=128,
                stderr=f'fatal: Not a valid object name {value}\n',
            )
        remote_ref = ref[len('refs/remotes/'):]
        if remote_ref not in self.remotes:
            return mocks.ProcessCompletion(
                returncode=128,
                stderr=f'fatal: Unable to find remote reference {ref}\n',
            )
        if commit not in self.remotes[remote_ref]:
            self.remotes[remote_ref] = list(reversed(self.rev_list(value)))
        return mocks.ProcessCompletion(returncode=0)

    def add_remote(self, name: str) -> mocks.ProcessCompletion:
        for existing in list(self.remotes.keys()):
            remote, branch = existing.split('/', 1)
            if remote == 'origin':
                self.remotes['{}/{}'.format(name, branch)] = self.remotes[existing][:]
        return mocks.ProcessCompletion(returncode=0)

    def for_each_ref(self, format: str, contains_commit: str | None, *patterns: str) -> mocks.ProcessCompletion:
        if contains_commit:
            commit = self.find(contains_commit)
            if commit is None:
                return mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n',
                )

            candidate_refs = sorted(
                f'refs/{branch}' if branch.startswith('remotes/') else f'refs/heads/{branch}'
                for branch in self.branches_on(commit)
            )
        else:
            candidate_refs = [f'refs/heads/{branch}' for branch in sorted(self.commits)] + [
                f'refs/remotes/{branch}' for branch in sorted(self.remotes)
            ]

        patterns_re = re.compile(
            '|'.join(
                itertools.chain.from_iterable(
                    (
                        fnmatch.translate(pattern),
                        re.escape(pattern) + r'\Z',
                        re.escape(pattern) + '/',
                    )
                    for pattern in patterns
                )
            )
        )

        refs = [ref for ref in candidate_refs if patterns_re.match(ref)]

        if format == '%(refname)':
            output = '\n'.join(refs)
        elif format == '%(objectname) %(refname)':
            output = '\n'.join(self.find(ref).hash + ' ' + ref for ref in refs)  # type: ignore[union-attr, operator]  # Only called for remote refs, which find() resolves to commits with hashes

        return mocks.ProcessCompletion(
            returncode=0,
            stdout=output + '\n' if refs else '',
        )
