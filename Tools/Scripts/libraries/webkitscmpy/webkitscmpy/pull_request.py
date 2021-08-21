# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re

from .commit import Commit


class PullRequest(object):
    COMMIT_BODY_RE = re.compile(r'\A#### (?P<hash>[0-9a-f]+)\n```\n(?P<message>.+)\n```\n?\Z', flags=re.DOTALL)
    DIVIDER_LEN = 70

    class State(object):
        OPENED = 'opened'
        CLOSED = 'closed'

    @classmethod
    def create_body(cls, body, commits):
        body = body or ''
        if not commits:
            return body
        if body:
            body = '{}\n\n{}\n'.format(body.rstrip(), '-' * cls.DIVIDER_LEN)
        return body + '\n{}\n'.format('-' * cls.DIVIDER_LEN).join([
            '#### {}\n```\n{}\n```'.format(commit.hash, commit.message.rstrip() if commit.message else '???') for commit in commits
        ])

    @classmethod
    def parse_body(cls, body):
        if not body:
            return None, []
        parts = [part.rstrip().lstrip() for part in body.split('-' * cls.DIVIDER_LEN)]
        body = ''
        commits = []

        for part in parts:
            match = cls.COMMIT_BODY_RE.match(part)
            if match:
                commits.append(Commit(
                    hash=match.group('hash'),
                    message=match.group('message') if match.group('message') != '???' else None,
                ))
            elif body:
                body = '{}\n{}\n{}\n'.format(body.rstrip(), '-' * cls.DIVIDER_LEN, part.rstrip().lstrip())
            else:
                body = part.rstrip().lstrip()
        return body or None, commits

    def __init__(self, number, title=None, body=None, author=None, head=None, base=None):
        self.number = number
        self.title = title
        self.body, self.commits = self.parse_body(body)
        self.author = author
        self.head = head
        self.base = base

    def __repr__(self):
        return 'PR {}{}'.format(self.number, ' | {}'.format(self.title) if self.title else '')
