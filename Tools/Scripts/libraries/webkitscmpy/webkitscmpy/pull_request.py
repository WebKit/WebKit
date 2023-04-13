# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
import six

from .commit import Commit
from datetime import datetime
from webkitscmpy import Contributor


class PullRequest(object):
    class Exception(RuntimeError):
        pass

    class Comment(object):
        def __init__(self, author, timestamp, content):
            if author and isinstance(author, dict) and author.get('name'):
                self.author = Contributor(author.get('name'), author.get('emails'))
            elif author and isinstance(author, six.string_types) and '@' in author:
                self.author = Contributor(author, [author])
            elif author and not isinstance(author, Contributor):
                raise TypeError("Expected 'author' to be of type {}, got '{}'".format(Contributor, author))
            else:
                self.author = author

            if isinstance(timestamp, six.string_types) and timestamp.isdigit():
                timestamp = int(timestamp)
            if timestamp and not isinstance(timestamp, int):
                raise TypeError("Expected 'timestamp' to be of type int, got '{}'".format(timestamp))
            self.timestamp = timestamp

            if content and not isinstance(content, six.string_types):
                raise ValueError("Expected 'content' to be a string, got '{}'".format(content))
            self.content = content

        def __repr__(self):
            return '({} @ {}) {}'.format(
                self.author,
                datetime.utcfromtimestamp(self.timestamp) if self.timestamp else '-',
                self.content,
            )


    COMMIT_BODY_RES = [
        dict(
            re=re.compile(r'\A#### (?P<hash>[0-9a-f]+)\n```\n(?P<message>.+)\n```\n?\Z', flags=re.DOTALL),
            escaped=False,
        ), dict(
            re=re.compile(r'\A#### (?P<hash>[0-9a-f]+)\n<pre>\n(?P<message>.+)\n</pre>\n?\Z', flags=re.DOTALL),
            escaped=True,
        ),
    ]
    DIVIDER_LEN = 70
    ESCAPE_TABLE = {
        '"': '&quot;',
        "'": '&apos;',
        '>': '&gt;',
        '<': '&lt;',
        '&': '&amp;',
    }

    @classmethod
    def escape_html(cls, message):
        message = ''.join(cls.ESCAPE_TABLE.get(c, c) for c in message)
        return re.sub(r'(https?://[^\s<>,:;]+?)(?=[\s<>,:;]|(&gt))', r'<a href="\1">\1</a>', message)

    @classmethod
    def unescape_html(cls, message):
        message = re.sub(r'<a href=".+">(https?://[^\s<>,:;]+)</a>', r'\1', message)
        for c, escaped in cls.ESCAPE_TABLE.items():
            message = message.replace(escaped, c)
        return message

    @classmethod
    def create_body(cls, body, commits, linkify=True):
        body = body or ''
        if not commits:
            return body
        if body:
            body = '{}\n\n{}\n'.format(body.rstrip(), '-' * cls.DIVIDER_LEN)
        if linkify:
            return body + '\n{}\n'.format('-' * cls.DIVIDER_LEN).join([
                '#### {}\n<pre>\n{}\n</pre>'.format(
                    commit.hash,
                    cls.escape_html(commit.message.rstrip() if commit.message else '???'),
                ) for commit in commits
            ])
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
            for obj in cls.COMMIT_BODY_RES:
                match = obj['re'].match(part)
                if match:
                    message = cls.unescape_html(match.group('message')) if obj.get('escaped') else match.group('message')
                    commits.append(Commit(
                        hash=match.group('hash'),
                        message=message if message != '???' else None,
                    ))
                    break
            else:
                if body:
                    body = '{}\n{}\n{}\n'.format(body.rstrip(), '-' * cls.DIVIDER_LEN, part.rstrip().lstrip())
                else:
                    body = part.rstrip().lstrip()
        return body or None, commits

    def __init__(
        self, number, title=None,
        body=None, author=None,
        head=None, base=None,
        opened=None, generator=None, metadata=None,
        url=None, draft=None, hash=None,
    ):
        self.number = number
        self.title = title
        self.body, self.commits = self.parse_body(body)
        self.author = author
        self.head = head
        self.base = base
        self.draft = draft
        self.hash = hash
        self._opened = opened
        self._reviewers = None
        self._approvers = None
        self._blockers = None
        self._metadata = metadata
        self._comments = None
        self.generator = generator
        self.url = url

    @property
    def reviewers(self):
        if self._reviewers is None and self.generator:
            self.generator.reviewers(self)
        return self._reviewers

    @property
    def approvers(self):
        if self._approvers is None and self.generator:
            self.generator.reviewers(self)
        return self._approvers

    @property
    def blockers(self):
        if self._blockers is None and self.generator:
            self.generator.reviewers(self)
        return self._blockers

    @property
    def opened(self):
        if self._opened is None:
            return '?'
        return self._opened

    def open(self):
        if self.opened is True:
            return self
        if not self.generator:
            raise self.Exception('No associated pull-request generator')
        return self.generator.update(self, opened=True)

    def close(self):
        if self.opened is False:
            return self
        if not self.generator:
            raise self.Exception('No associated pull-request generator')
        return self.generator.update(self, opened=False)

    def comment(self, content):
        if not self.generator:
            raise self.Exception('No associated pull-request generator')
        self.generator.comment(self, content)
        self._comments = None
        return self

    @property
    def comments(self):
        if self._comments is None and self.generator:
            self._comments = list(self.generator.comments(self))
        return self._comments

    def review(self, comment=None, approve=None):
        if not self.generator:
            raise self.Exception('No associated pull-request generator')
        return self.generator.review(self, comment=comment, approve=approve)

    def __repr__(self):
        return 'PR {}{}'.format(self.number, ' | {}'.format(self.title) if self.title else '')
