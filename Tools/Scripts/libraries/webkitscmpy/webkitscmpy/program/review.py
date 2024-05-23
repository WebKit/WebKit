# Copyright (C) 2024 Apple Inc. All rights reserved.
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

import difflib
import os
import re
import shutil
import sys
import tempfile

from .command import Command
from .pull_request import PullRequest

from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import local, log, remote


class Review(Command):
    name = 'review'
    help = "Run a command-line wizard to review a specific pull-request or commit in a project"
    DETAILS_RE = r'<details>.+</details>'
    INDENT_SIZE = 4
    PR_RE = re.compile(r'^\[?([Pp][Rr][ -]?)?(?P<number>\d+)]?$')
    PR_URL_RES = [
        re.compile(r'\A(?P<url>https?://\S+/projects/\S+/repos/\S+)/pull-requests/(?P<number>\d+)(/overview)?(/diff)?\Z'),
        re.compile(r'\A(?P<url>https?://github.\S+/\S+)/pull/(?P<number>\d+)(/files)?\Z'),
    ]
    SEPERATOR_SIZE = 80
    URL_RE = re.compile(r'\Ahttps?://')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a pull request (pr-#), commit or URL to review',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Specify remote to search for pull request from.',
        )
        parser.add_argument(
            '--dry-run', '-n', '--no-dry-run',
            dest='dry_run', default=False,
            help='View but do not edit a pull-request',
            action=arguments.NoAction,
        )

    @classmethod
    def editor(cls, repository):
        from_config = None
        if isinstance(repository, local.Git):
            from_config = repository.config().get('core.editor', None)
        if not from_config:
            from_config = local.Git.config().get('core.editor', None)
        if from_config:
            return from_config.split(' ')
        return [shutil.which('vim')]

    @classmethod
    def args_for_url(cls, url):
        url = url.split('?')[0]
        url = url.split('#')[0]
        for candidate in cls.PR_URL_RES:
            match = candidate.match(url)
            if match:
                return match.group('number'), remote.Scm.from_url(match.group('url'))
        return None, None

    @classmethod
    def invoke_wizard(
        cls, editor, name,
        header=None,
        messages=None,
        comments=None,
        diff=None,
    ):
        # Textual wizards are split into 4 sections, seperated by a line of '=' characters
        output = [[], [], [], []]
        edited_path = os.path.join(tempfile.gettempdir(), str(os.getpid()), '{}.diff'.format(name))
        if not os.path.exists(os.path.dirname(edited_path)):
            os.makedirs(os.path.dirname(edited_path))

        # 1st section: metadata
        sanitized_header = {
            key.rstrip().lstrip(): value.rstrip().lstrip() if value else None
            for key, value in (header or {}).items()
        }
        for key, value in sanitized_header.items():
            if not value:
                continue
            output[0].append('{}: {}'.format(key.rstrip().lstrip(), value.rstrip().lstrip()))

        # 2nd section: commit message
        for message in messages or []:
            for line in message.splitlines():
                output[1].append((' ' * cls.INDENT_SIZE + line).rstrip())
            output[1].append(' ' * cls.INDENT_SIZE + '-' * (cls.SEPERATOR_SIZE - cls.INDENT_SIZE))
        if output[1]:
            output[1].pop()

        # 3rd section: PR or commit comments
        comment_for_lines = {}
        for comment in comments or []:
            for line in comment.splitlines():
                comment_for_lines[len(output[2])] = comment
                output[2].append(line.rstrip())
            comment_for_lines[len(output[2])] = comment
            output[2].append('-' * cls.SEPERATOR_SIZE)
        if output[2]:
            output[2].pop()

        # 4th section: Diff
        for line in diff:
            output[3].append(line.rstrip())

        with open(edited_path, 'w') as ofile:
            cnt = 0
            while cnt < len(output):
                for line in output[cnt]:
                    ofile.write(line + '\n')
                if output[cnt] and cnt + 1 < len(output):
                    ofile.write('=' * cls.SEPERATOR_SIZE + '\n')
                cnt += 1

        run(editor + [edited_path])

        input = [[], [], [], []]
        with open(edited_path, 'r') as ifile:
            cnt = 0
            for line in ifile.readlines():
                line = line.rstrip()
                if cnt >= len(input):
                    sys.stderr.write('Exceeded number of expected blocks in textual input\n')
                    break
                if line.startswith('=' * cls.SEPERATOR_SIZE):
                    cnt += 1
                    while cnt < len(output) and not output[cnt]:
                        cnt += 1
                    continue
                input[cnt].append(line)

        return dict()

    @classmethod
    def main(cls, args, repository, **kwargs):
        editor = cls.editor(repository)

        target = args.argument[0]
        if cls.URL_RE.match(target):
            target, repository = cls.args_for_url(target)
            if not repository:
                sys.stderr.write("Failed to extract repository from '{}'\n".format(args.argument[0]))
                return 1
            if not target:
                sys.stderr.write("Failed to determine target from '{}'\n".format(args.argument[0]))
                return 1

        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        if isinstance(repository, local.Git):
            original = repository
            repository = repository.remote(name=args.remote)
            if not repository:
                sys.stderr.write("'{}' is not a remote in '{}'\n".format(args.remote, original.path))
                return 1
        elif args.remote:
            sys.stderr.write("User provided '--remote={}',\n".format(args.remote))
            sys.stderr.write("but '{}' is already a remote repository\n".format(repository.url))
            return 1

        if not isinstance(repository, remote.GitHub) and not isinstance(repository, remote.BitBucket):
            sys.stderr.write('Provided repository is not a known remote git repository with pull-requests\n')
            return 1

        if not repository.pull_requests:
            sys.stderr.write('No pull-requests associated with repository\n')
            return 1

        match = cls.PR_RE.match(target)
        if match:
            pull_request = repository.pull_requests.get(number=int(match.group('number')))
        else:
            pull_request = PullRequest.find_existing_pull_request(repository, rmt, branch=target)
        if not pull_request:
            sys.stderr.write("\nCannot extract PR number from '{}'\n".format(target))
            return 1

        header = {
            'Title': pull_request.title,
            'Status': ('Draft' if pull_request.draft else 'Opened') if pull_request.opened else ('Merged' if pull_request.merged else 'Closed'),
            'Author': str(pull_request.author),
            'Approved by': string_utils.join([person.name for person in pull_request.approvers]) if pull_request.approvers else None,
            'Blocked by': string_utils.join([person.name for person in pull_request.blockers]) if pull_request.blockers else None,
        }
        if pull_request._metadata and pull_request._metadata.get('issue'):
            pr_issue = pull_request._metadata['issue']
            if pr_issue.labels:
                header['Labels'] = string_utils.join(pr_issue.labels)

        print('Waiting for user to modify textual pull-request...')
        response = cls.invoke_wizard(
            editor=editor,
            name='pr-{}'.format(pull_request.number),
            header=header,
            messages=([pull_request.body] if pull_request.body else []) + [commit.message for commit in pull_request.commits or []],
            comments=[
                '{}: {}'.format(comment.author, re.sub(cls.DETAILS_RE, '', comment.content, flags=re.S))
                for comment in pull_request.comments
            ], diff=pull_request.diff(comments=True),
        )

        return 0
