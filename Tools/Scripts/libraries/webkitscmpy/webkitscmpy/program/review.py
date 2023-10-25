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

import difflib
import os
import re
import sys
import tempfile

from .command import Command
from .checkout import Checkout
from .pull_request import PullRequest

from webkitcorepy import run, string_utils, Terminal
from webkitscmpy import local, remote


class Review(Command):
    name = 'review'
    help = "Run a command-line wizard to review a specific pull-request or commit in a project"

    SEPERATOR_SIZE = 80
    INDENT_SIZE = 4
    DETAILS_RE = r'<details>.+</details>'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a pull request (pr-#) or commit to review',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Specify remote to search for pull request from.',
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

        from whichcraft import which
        return [which('vi')]

    @classmethod
    def _truncate_strs(cls, d):
        if not isinstance(d, dict):
            return
        for key in d.keys():
            if isinstance(d[key], str):
                d[key] = d[key].rstrip()
            else:
                cls._truncate_strs(d[key])

    @classmethod
    def invoke_editor(
            cls, editor, name,
            header=None,
            messages=None, comments=None,
            diff=None,
    ):
        # Textual wizards are split into 4 sections, seperated by a line of '=' characters
        output = [[], [], [], []]
        edited_path = os.path.join(tempfile.gettempdir(), '{}.txt'.format(name))

        # 1st section: metadata
        sanitized_header ={
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
        for comment in comments or []:
            for line in comment.splitlines():
                output[2].append(line.rstrip())
            output[2].append('-' * cls.SEPERATOR_SIZE)
        if output[2]:
            output[2].pop()

        # 4th section: Diff
        for line in (diff or '').splitlines():
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

        # Users can:
        # - Change metadata
        # - Add global comments
        # - Add diff comments
        response = dict(
            metadata=dict(),
            comments=[],
            diff=dict(),
        )

        # First section: metadata
        resolved_metadata = dict()
        for line in input[0]:
            key, value = line.split(':', maxsplit=1)
            resolved_metadata[key.rstrip().lstrip()] = value.rstrip().lstrip()
        for key in resolved_metadata.keys():
            if (resolved_metadata[key] or '') != (sanitized_header.get(key) or ''):
                response['metadata'][key] = resolved_metadata[key] or ''
        for key, value in sanitized_header.items():
            if value and key not in resolved_metadata:
                response['metadata'][key] = ''
        if not response['metadata']:
            del response['metadata']

        # Second section: commit message
        previous_line = None
        response['comments'].append('')
        for line in difflib.ndiff(output[1], input[1]):
            if line[0] in ('-', '?'):
                continue
            if line[0] == '+':
                if previous_line:
                    if response['comments'][-1]:
                        response['comments'][-1] += '\n'
                    response['comments'][-1] += '> {}\n'.format(previous_line)
                    previous_line = None
                response['comments'][-1] += line[2:].lstrip().rstrip() + '\n'
                continue
            if line.rstrip():
                previous_line = line[6:]
        if response['comments'][-1]:
            response['comments'][-1] = response['comments'][-1].rstrip()
        else:
            response['comments'].pop()

        # Third section: comments
        previous_line = None
        response['comments'].append('')
        for line in difflib.ndiff(output[2], input[2]):
            if line[2:] == '-' * cls.SEPERATOR_SIZE:
                previous_line = None
                if response['comments'][-1]:
                    response['comments'][-1] = response['comments'][-1].rstrip()
                    response['comments'].append('')
                continue
            if line[0] in ('-', '?'):
                continue
            if line[0] == '+':
                if previous_line:
                    if response['comments'][-1]:
                        response['comments'][-1] += '\n'
                    response['comments'][-1] += '> {}\n'.format(previous_line)
                    previous_line = None
                response['comments'][-1] += line[2:].lstrip().rstrip() + '\n'
                continue
            if not previous_line:
                line = '  {}'.format(line.split(':', maxsplit=1)[-1].lstrip())
            if line.rstrip():
                previous_line = line[2:]
        if response['comments'][-1]:
            response['comments'][-1] = response['comments'][-1].rstrip()
        else:
            response['comments'].pop()

        if not response['comments']:
            del response['comments']

        # Fourth section: diff
        file = None
        position = None
        in_comment = False
        for line in difflib.ndiff(output[3], input[3]):
            if line.startswith('  diff') or line.startswith('  index'):
                position = None
                file = None
                continue
            if line.startswith('  --- a/') or line.startswith('  +++ b/'):
                position = None
                file = line[8:]
                continue
            if file is not None and position is None and line.startswith('  @@'):
                position = 0
                continue

            if line.startswith('+ '):
                if not file:
                    response['diff']['comment'] = response['diff'].get('comment', '') + line[2:].lstrip().rstrip() + '\n'
                    continue
                response['diff']['file_comments'] = response['diff'].get('file_comments', {})
                response['diff']['file_comments'][file] = response['diff']['file_comments'].get(file, {})
                key = position or 0
                response['diff']['file_comments'][file][key] = response['diff']['file_comments'][file].get(key, '') + line[2:].lstrip().rstrip() + '\n'
                continue

            if line.startswith('  >>>>'):
                in_comment = True
            if not in_comment and position is not None:
                position += 1
            if line.startswith('  <<<<'):
                in_comment = False

        cls._truncate_strs(response['diff'])
        if not response['diff']:
            del response['diff']

        return response

    @classmethod
    def _user_delta(cls, pr, original, value):
        returncode = 0
        modified = []
        for name in string_utils.split(value):
            user = pr.generator.repository.contributors.get(name)
            if not user:
                sys.stderr.write("'{}' is not a recognized user\n".format(name))
                returncode += 1
            else:
                modified.append(user)
        added = [user for user in modified if user not in original]
        removed = [user for user in original if user not in modified]

        user, _ = pr.generator.repository.credentials(required=False)
        added_no_me = [candidate for candidate in added if not user or candidate != user]

        return len(added_no_me) != len(added), added_no_me, removed, returncode

    @classmethod
    def for_pr(cls, pr, editor):
        print('Waiting for user to modify textual pull-request...')
        delta = cls.invoke_editor(
            editor=editor, name='pr-{}'.format(pr.number),
            header={
                'Title': pr.title,
                'Status': 'Draft' if pr.draft else ('Opened' if pr.opened else 'Closed'),
                'Author': str(pr.author),
                'Approved by': string_utils.join([person.name for person in pr.approvers]) if pr.approvers else None,
                'Blocked by': string_utils.join([person.name for person in pr.blockers]) if pr.blockers else None,
            },
            messages=([pr.body] if pr.body else []) + [commit.message for commit in pr.commits or []],
            comments=[
                '{}: {}'.format(comment.author, re.sub(cls.DETAILS_RE, '', comment.content, flags=re.S))
                for comment in pr.comments
            ], diff=pr.diff(comments=True),
        )
        if not delta:
            print('No local pull-request changes detected')

        returncode = 0
        if False and delta and not pr.generator:
            sys.stderr.write('No PR generator to push changes to\n')
            return 1

        did_approve = None

        for key, value in (delta.get('metadata') or {}).items():
            if key == 'Title':
                if not pr.generator.update(pr, title=value):
                    returncode += 1
                continue
            if key == 'Status':
                if value.lower() == 'open':
                    if not pr.open():
                        returncode += 1
                    continue
                if value.lower() == 'close':
                    if not pr.close():
                        returncode += 1
                    continue
                sys.stderr.write("Cannot change PR status to '{}'\n".format(value))
                returncode += 1
                continue
            if key in ('Approved by', 'Blocked by'):
                original = pr.approvers if key == 'Approved by' else pr.blockers
                is_me, added, removed, rc = cls._user_delta(pr, original, value)
                if is_me:
                    did_approve = key == 'Approved by'
                returncode += rc
                if added + removed and not pr.add_reviewers(added + removed):
                    sys.stderr.write('Failed to add {} as reviewer{}\n'.format(
                        string_utils.join([reviewer.name for reviewer in added + removed]),
                        's' if len(added + removed) != 1 else '',
                    ))
                    returncode += 1
                continue

            sys.stderr.write("'{}' cannot be modified\n".format(key))
            returncode += 1

        for comment in (delta.get('comments') or []):
            pr.comment(comment)

        user, _ = pr.generator.repository.credentials(required=False)
        if did_approve is None and user and pr.author != user:
            response = Terminal.choose(
                'Would you like approve this change?',
                default='No', options=('Yes', 'No', 'Block'),
            )
            did_approve = dict(Yes=True, Block=False).get(response, None)
        elif user and did_approve is not None:
            sys.stderr.write('Cannot {} your own pull request\n'.format('approve' if did_approve else 'block'))
            did_approve = None
            returncode += 1
        diff = delta.get('diff') or {}
        if did_approve is not None or diff:
            if not pr.review(approve=did_approve, **diff):
                sys.stderr.write('Failed to post review and diff comments\n')
                returncode += 1

        if delta and returncode:
            sys.stderr.write('Not all pull-request changes could be publicized\n')
        elif delta:
            print('All pull-request changes publicized')
        print('See {}'.format(pr.url))

        return returncode

    @classmethod
    def for_commit(cls, repository, remote, commit, editor):
        print('Waiting for user to modify textual commit...')
        name = str(commit)
        if '/' in name:
            name = commit.hash
        delta = cls.invoke_editor(
            editor=editor, name=name,
            header={
                'Title': commit.message.splitlines()[0],
                'Author': str(commit.author),
            }, messages=[commit.message],
            diff=repository.diff(commit.hash),
        )

        returncode = 0
        if not delta:
            print('No commit data to publish')
            return 0
        if delta and not remote:
            sys.stderr.write('No remote to publish data to\n')
            return 1
        if 'metadata' in delta:
            sys.stderr.write('Cannot modify metadata on existing commit\n')
            returncode += 1

        if delta and returncode:
            sys.stderr.write('Not all commit data could be publicized\n')
        elif delta:
            print('All commit changes publicized')

        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git) and not isinstance(repository, remote.GitHub) and not isinstance(repository, remote.BitBucket):
            sys.stderr.write('Provided repository is not a know local or remote git repository\n')
            return 1

        editor = cls.editor(repository)

        if repository.path:
            rmt = repository.remote(name=args.remote)
            if args.remote and not rmt:
                sys.stderr.write("'{}' is not a remote in '{}'\n".format(args.remote, repository.path))
                return 1
        else:
            if args.remote:
                sys.stderr.write("User provided '--remote={}',\n".format(args.remote))
                sys.stderr.write("but '{}' is already a remote repository\n".format(repository.url))
                return 1
            rmt = repository

        target = args.argument[0]
        match = Checkout.PR_RE.match(target)

        if not match and rmt and rmt.pull_requests:
            candidate = PullRequest.find_existing_pull_request(repository, rmt, branch=target)
            if candidate:
                target = 'pr-{}'.format(candidate.number)
                match = Checkout.PR_RE.match(target)

        if match:
            if not rmt:
                sys.stderr.write('Repository does not have associated remote\n')
                return 1
            if not rmt.pull_requests:
                sys.stderr.write('No pull-requests associated with repository\n')
                return 1
            pr = rmt.pull_requests.get(number=int(match.group('number')))
            if not pr:
                sys.stderr.write("Failed to find 'PR-{}' associated with this repository\n".format(match.group('number')))
                return 1
            return cls.for_pr(pr, editor=editor)

        commit = repository.find(target)
        if not commit:
            sys.stderr.write("No commit found matching '{}'\n".format(target))
            return 1
        return cls.for_commit(repository, remote, commit, editor=editor)
