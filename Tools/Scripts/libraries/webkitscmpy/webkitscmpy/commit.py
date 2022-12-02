# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import json
import six
import re

from datetime import datetime
from webkitbugspy import Tracker
from webkitscmpy import Contributor


class Commit(object):
    HASH_RE = re.compile(r'^[a-f0-9A-F]+$')
    REVISION_RE = re.compile(r'^[Rr]?(?P<revision>\d+)$')
    IDENTIFIER_RE = re.compile(r'^((?P<branch_point>\d+)\.)?(?P<identifier>-?\d+)(@(?P<branch>\S*))?$')
    NUMBER_RE = re.compile(r'^-?\d*$')
    HASH_LABEL_SIZE = 12
    UUID_MULTIPLIER = 100

    class Encoder(json.JSONEncoder):

        def default(self, obj):
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, Commit):
                return super(Commit.Encoder, self).default(obj)

            result = dict()
            for attribute in ['hash', 'revision', 'branch', 'timestamp', 'order', 'message', 'repository_id']:
                value = getattr(obj, attribute, None)
                if value is not None:
                    result[attribute] = value

            if obj.author:
                result['author'] = Contributor.Encoder().default(obj.author)

            if obj.identifier is not None:
                result['identifier'] = str(obj)

            return result

    @classmethod
    def _parse_hash(cls, hash, do_assert=False):
        if hash is None:
            return None

        if not isinstance(hash, six.string_types):
            if do_assert:
                raise ValueError("Expected string type for hash, got '{}'".format(type(hash)))
            return None

        if not cls.HASH_RE.match(hash) or len(hash) > 40:
            if do_assert:
                raise ValueError("Provided string '{}' is not a git hash".format(hash))
            return None

        return hash.lower()

    @classmethod
    def _parse_revision(cls, revision, do_assert=False):
        if revision is None:
            return None

        if isinstance(revision, six.string_types):
            match = cls.REVISION_RE.match(revision)
            if match:
                revision = int(match.group('revision'))
            elif revision.isdigit():
                revision = int(revision)
            else:
                if do_assert:
                    raise ValueError("Provided string '{}' is not an SVN".format(revision))
                return None

        if not isinstance(revision, int):
            if do_assert:
                raise ValueError("Expected int type for revision, got '{}'".format(type(revision)))
            return None

        if revision <= 0:
            if do_assert:
                raise ValueError('SVN revisions must be positive integer')
            return None

        return revision

    @classmethod
    def _parse_identifier(cls, identifier, do_assert=False):
        if identifier is None:
            return None

        branch = None
        if isinstance(identifier, six.string_types):
            match = cls.IDENTIFIER_RE.match(identifier)
            if match:
                identifier = match.group('branch_point'), int(match.group('identifier'))
                if identifier[0]:
                    identifier = int(identifier[0]), identifier[1]
                branch = match.group('branch') or None
            elif cls.NUMBER_RE.match(identifier):
                identifier = None, int(identifier)
            else:
                if do_assert:
                    raise ValueError("Provided string '{}' is not an identifier".format(identifier))
                return None

        if isinstance(identifier, int):
            identifier = (None, identifier)

        if not (isinstance(identifier, tuple) and len(identifier) == 2 and all([isinstance(x, int) or x is None for x in identifier])):
            if do_assert:
                raise ValueError('Expected int type (or pair of ints) for identifier, got {}'.format(type(identifier)))
            return None

        return (identifier[0], identifier[1], branch)

    @classmethod
    def parse(cls, arg, do_assert=True):
        if cls._parse_identifier(arg):
            return Commit(identifier=arg)

        if cls._parse_revision(arg):
            return Commit(revision=arg)

        if cls._parse_hash(arg):
            return Commit(hash=arg)

        if do_assert:
            raise ValueError("'{}' cannot be converted to a commit object".format(arg))
        return None

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        hash_from_id = None
        revision_from_id = cls._parse_revision(data.get('id'))
        if not revision_from_id:
            hash_from_id = cls._parse_hash(data.get('id'))

        return cls(
            repository_id=data.get('repository_id'),
            branch=data.get('branch'),
            hash=data.get('hash', hash_from_id),
            revision=data.get('revision', revision_from_id),
            timestamp=data.get('timestamp'),
            identifier=data.get('identifier'),
            branch_point=data.get('branch_point'),
            order=data.get('order'),
            author=data.get('author', data.get('committer')),
            message=data.get('message'),
        )

    def __init__(
        self,
        hash=None,
        revision=None,
        identifier=None, branch=None, branch_point=None,
        timestamp=None, author=None, message=None, order=None, repository_id=None
    ):
        self.hash = self._parse_hash(hash, do_assert=True)
        self.revision = self._parse_revision(revision, do_assert=True)

        parsed_identifier = self._parse_identifier(identifier, do_assert=True)
        if parsed_identifier:
            parsed_branch_point, self.identifier, parsed_branch = parsed_identifier
            self.branch_point = parsed_branch_point or branch_point
            self.branch = parsed_branch or branch
        else:
            self.identifier = None
            self.branch_point = branch_point
            self.branch = branch

        if branch and not isinstance(branch, six.string_types):
            raise ValueError("Expected 'branch' to be a string")
        if branch and branch != self.branch:
            raise ValueError(
                "Caller passed both 'branch' and 'identifier', but specified different branches ({} and {})".format(
                    branch, self.branch,
                ),
            )

        if branch_point and not isinstance(branch_point, int):
            raise ValueError("Expected 'branch_point' to be an int")
        if branch_point and branch_point != self.branch_point:
            raise ValueError(
                "Caller passed both 'branch_point' and 'identifier', but specified different values ({} and {})".format(
                    branch_point, self.branch_point,
                ),
            )

        if isinstance(timestamp, six.string_types) and timestamp.isdigit():
            timestamp = int(timestamp)
        if timestamp and not isinstance(timestamp, int):
            raise TypeError("Expected 'timestamp' to be of type int, got '{}'".format(timestamp))
        self.timestamp = timestamp

        if isinstance(order, six.string_types) and order.isdigit():
            order = int(order)
        if order and not isinstance(order, int):
            raise TypeError("Expected 'order' to be of type int, got '{}'".format(order))
        self.order = order or 0

        if author and isinstance(author, dict) and author.get('name'):
            self.author = Contributor(author.get('name'), author.get('emails'))
        elif author and isinstance(author, six.string_types) and '@' in author:
            self.author = Contributor(author, [author])
        elif author and not isinstance(author, Contributor):
            raise TypeError("Expected 'author' to be of type {}, got '{}'".format(Contributor, author))
        else:
            self.author = author

        if message and not isinstance(message, six.string_types):
            raise ValueError("Expected 'message' to be a string, got '{}'".format(message))
        self.message = message

        if repository_id and not isinstance(repository_id, six.string_types):
            raise ValueError("Expected 'repository_id' to be a string, got '{}'".format(repository_id))
        self.repository_id = repository_id

        # Force a commit format check
        self.__repr__()

    def pretty_print(self, message=False):
        result = '{}\n'.format(self)
        if self.revision:
            result += '    SVN revision: r{}'.format(self.revision)
            if self.branch:
                result += ' on {}'.format(self.branch)
            result += '\n'
        if self.hash:
            result += '    git hash: {}'.format(self.hash[:self.HASH_LABEL_SIZE])
            if self.branch:
                result += ' on {}'.format(self.branch)
            result += '\n'
        if self.identifier:
            result += '    identifier: {}'.format(self.identifier)
            if self.branch:
                result += ' on {}'.format(self.branch)
            if self.branch_point:
                result += ' branched from {}'.format(self.branch_point)
            result += '\n'
        if self.author:
            result += '    by {}'.format(self.author)
            if self.timestamp:
                result += ' @ {}'.format(datetime.utcfromtimestamp(self.timestamp))
            result += '\n'

        if self.message and message:
            result += '\n'
            result += self.message

        return result

    @property
    def uuid(self):
        if self.timestamp is None:
            return None
        return self.timestamp * self.UUID_MULTIPLIER + self.order

    @property
    def issues(self):
        if not self.message:
            return []
        result = []
        links = set()
        seen_empty = False
        seen_first_line = False
        prepend = False

        for line in self.message.splitlines():
            if not line and seen_empty:
                break
            elif not line:
                seen_empty = True
                prepend = False
                continue
            words = line.split()
            for word in [words[0], words[-1]] if words[0] != words[-1] else [words[0]]:
                candidate = Tracker.from_string(word)
                if candidate and candidate.link not in links:
                    links.add(candidate.link)
                    if prepend:
                        result.insert(len(result) - 1, candidate)
                    else:
                        result.append(candidate)

                    if not seen_first_line:
                        prepend = True
            seen_first_line = True
        return result

    def __repr__(self):
        if self.branch_point and self.identifier is not None and self.branch:
            return '{}.{}@{}'.format(self.branch_point, self.identifier, self.branch)
        if self.identifier is not None and self.branch:
            return '{}@{}'.format(self.identifier, self.branch)
        if self.revision:
            return 'r{}'.format(self.revision)
        if self.hash:
            return self.hash[:self.HASH_LABEL_SIZE]
        if self.identifier is not None:
            return str(self.identifier)
        return '?'

    def __hash__(self):
        if self.identifier and self.branch:
            return hash(self.identifier) ^ hash(self.branch)
        if self.revision:
            return hash(self.revision)
        if self.hash:
            return hash(self.hash)
        if self.identifier:
            return hash(self.identifier)
        raise ValueError('Incomplete commit format')

    def __cmp__(self, other):
        if not isinstance(other, Commit):
            raise ValueError('Cannot compare commit and {}'.format(type(other)))
        if self.uuid and other.uuid:
            if self.uuid != other.uuid:
                return self.uuid - other.uuid
            if self.repository_id != other.repository_id:
                return 1 if self.repository_id > other.repository_id else -1
        if self.revision and other.revision:
            return self.revision - other.revision
        if self.identifier and other.identifier and self.branch == other.branch:
            return self.identifier - other.identifier
        raise ValueError('Cannot compare {} and {}'.format(self, other))

    def __eq__(self, other):
        return hash(self) == hash(other)

    def __ne__(self, other):
        return hash(self) != hash(other)

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0
