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

import six
import re

from datetime import datetime
from webkitscmpy import Contributor


class Commit(object):
    HASH_RE = re.compile(r'^[a-f0-9A-F]+$')
    REVISION_RE = re.compile(r'^[Rr]?(?P<revision>\d+)$')
    IDENTIFIER_RE = re.compile(r'^[Ii]?((?P<branch_point>\d+)\.)?(?P<identifier>-?\d+)(@(?P<branch>\S+))?$')
    NUMBER_RE = re.compile(r'^-?\d*$')

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
                branch = match.group('branch')
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
    def parse(cls, arg):
        if cls._parse_identifier(arg):
            return Commit(identifier=arg)

        if cls._parse_revision(arg):
            return Commit(revision=arg)

        if cls._parse_hash(arg):
            return Commit(hash=arg)

        raise ValueError("'{}' cannot be converted to a commit object".format(arg))

    def __init__(
        self,
        hash=None,
        revision=None,
        identifier=None, branch=None, branch_point=None,
        timestamp=None, author=None, message=None,
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

        if timestamp and not isinstance(timestamp, int):
            raise TypeError("Expected 'timestamp' to be of type int, got '{}'".format(timestamp))
        self.timestamp = timestamp

        if author and isinstance(author, six.string_types):
            self.author = Contributor.by_email.get(
                author,
                Contributor.by_name.get(author),
            )
            if not self.author:
                raise ValueError("'{}' does not match a known contributor")
        elif author and not isinstance(author, Contributor):
            raise TypeError("Expected 'author' to be of type {}, got '{}'".format(Contributor, author))
        else:
            self.author = author

        if message and not isinstance(message, six.string_types):
            raise ValueError("Expected 'message' to be a string, got '{}'".format(message))
        self.message = message

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
            result += '    git hash: {}'.format(self.hash[:12])
            if self.branch:
                result += ' on {}'.format(self.branch)
            result += '\n'
        if self.identifier:
            result += '    identifier: i{}'.format(self.identifier)
            if self.identifier:
                result += ' on {}'.format(self.branch)
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

    def __repr__(self):
        if self.branch_point and self.identifier and self.branch:
            return 'i{}.{}@{}'.format(self.branch_point, self.identifier, self.branch)
        if self.identifier and self.branch:
            return 'i{}@{}'.format(self.identifier, self.branch)
        if self.revision:
            return 'r{}'.format(self.revision)
        if self.hash:
            return self.hash[:12]
        if self.identifier:
            return 'i{}'.format(self.identifier)
        raise ValueError('Incomplete commit format')

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
        if self.timestamp and other.timestamp and self.timestamp != other.timestamp:
            return self.timestamp - other.timestamp
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
