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

import os
import sys

from .command import Command

from webkitbugspy import Tracker
from webkitcorepy import run, string_utils
from webkitscmpy import local


class Commit(Command):
    name = 'commit'
    help = 'Create a new commit containing the current contents of the index ' \
           'passing any provided issue to the commit message.'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'args', nargs='*',
            type=str, default=None,
            help='Arguments to be passed to `git commit`',
        )
        parser.add_argument(
            '-i', '--issue', '-b', '--bug', '-r',
            dest='issue', type=str,
            help='Number (or url) of the issue or bug to create branch for',
        )
        parser.add_argument(
            '-a', '--all',
            dest='all', action='store_true', default=False,
            help='Tell the command to automatically stage files that have been modified and '
                 'deleted, but new files you have not told Git about are not affected.',
        )
        parser.add_argument(
            '--amend',
            dest='amend', action='store_true', default=False,
            help='Replace the tip of the current branch by creating a new commit',
        )

    @classmethod
    def bug_urls(cls, issue):
        if not issue:
            return ''

        bug_urls = [issue.link]
        types = [type(issue.tracker)]
        for related in issue.references:
            if type(related.tracker) in types:
                continue
            bug_urls.append(related.link)
            types.append(type(related.tracker))
        return bug_urls

    @classmethod
    def main(cls, args, repository, command=None, representation=None, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        issue = None
        if args.issue:
            if string_utils.decode(args.issue).isnumeric() and Tracker.instance():
                issue = Tracker.instance().issue(int(args.issue))
            else:
                issue = Tracker.from_string(args.issue)
            if not issue:
                sys.stderr.write("'{}' cannot be converted to an issue\n".format(args.issue))
                return 1

        additional_args = []
        if args.all:
            additional_args += ['--all']
        if args.amend:
            additional_args += ['--amend']

        env = os.environ
        env['COMMIT_MESSAGE_TITLE'] = issue.title if issue else ''
        env['COMMIT_MESSAGE_BUG'] = '\n'.join(cls.bug_urls(issue))
        return run(
            [repository.executable(), 'commit', '--date=now'] + additional_args + args.args,
            cwd=repository.root_path,
            env=env,
        ).returncode
