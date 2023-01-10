# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

import json
import re
import sys

from collections import defaultdict
from webkitcorepy import string_utils


class Contributor(object):
    GIT_AUTHOR_RE = re.compile(r'Author: (?P<author>.*) <(?P<email>[^@]+@[^@]+)(@.*)?>')
    AUTOMATED_CHECKIN_RE = re.compile(r'Author: (?P<author>.*) <devnull>')
    UNKNOWN_AUTHOR = re.compile(r'Author: (?P<author>.*) <None>')
    EMPTY_AUTHOR = re.compile(r'Author: (?P<author>.*) <>')
    SVN_AUTHOR_RE = re.compile(r'r(?P<revision>\d+) \| (?P<email>.*) \| (?P<date>.*) \| \d+ lines?')
    SVN_AUTHOR_Q_RE = re.compile(r'r(?P<revision>\d+) \| (?P<email>.*) \| (?P<date>.*)')
    SVN_PATCH_FROM_RE = re.compile(r'Patch by (?P<author>.*) <(?P<email>.*)> on \d+-\d+-\d+')
    REVIEWER = 'reviewer'

    class Encoder(json.JSONEncoder):

        def default(self, obj):
            if not isinstance(obj, Contributor):
                return super(Contributor.Encoder, self).default(obj)

            result = dict(name=obj.name)
            if obj.status:
                result['status'] = obj.status
            if obj.emails:
                result['emails'] = [str(email) for email in obj.emails]
            if obj.github:
                result['github'] = obj.github
            if obj.bitbucket:
                result['bitbucket'] = obj.bitbucket

            return result

    class Mapping(defaultdict):
        @classmethod
        def load(cls, file):
            result = cls()
            contents = json.load(file)
            for contributor in contents:
                name = contributor.get('name', None)
                if not name:
                    continue
                created = result.create(name, *contributor.get('emails', []))
                created.status = contributor.get('status', created.status)
                created.github = contributor.get('github', created.github)
                created.bitbucket = contributor.get('bitbucket', created.bitbucket)

                result.statuses.add(created.status)

                if created.github:
                    result[created.github] = created
                if created.bitbucket:
                    result[created.bitbucket] = created

            for contributor in contents:
                constructed = result.get(contributor.get('name'))
                if not constructed:
                    continue
                for alias in contributor.get('aliases', []) + contributor.get('nicks', []):
                    if alias in result:
                        continue
                    result[alias] = constructed
            return result

        def __init__(self):
            super(Contributor.Mapping, self).__init__(lambda: None)
            self.statuses = set()

        def save(self, file):
            alias_to_name = defaultdict(list)
            for alias, contributor in self.items():
                if not contributor or alias in contributor.emails or alias == contributor.name:
                    continue
                alias_to_name[contributor.name].append(alias)

            contributors = []
            for alias, contributor in self.items():
                if not contributor or alias != contributor.name:
                    continue
                contributors.append(Contributor.Encoder().default(contributor))
                contributors[-1]['aliases'] = alias_to_name[contributor.name]

            json.dump(contributors, file)

        def add(self, contributor):
            if not isinstance(contributor, Contributor):
                raise ValueError("'{}' is not a Contributor object".format(type(contributor)))

            result = self.create(contributor.name, *contributor.emails)
            if not result:
                sys.stderr.write("Failed to create contributor {} ({})\n".format(contributor.name, ', '.join(contributor.emails)))
                return None

            result.status = contributor.status or result.status
            result.github = contributor.github or result.github
            result.bitbucket = contributor.bitbucket or result.bitbucket

            self.statuses.add(result.status)

            if result.github:
                self[result.github] = result
            if result.bitbucket:
                self[result.bitbucket] = result

            return result

        def create(self, name=None, *emails):
            emails = [email for email in emails or []]
            if not name and not emails:
                return None

            contributor = None
            for argument in [name] + (emails or []):
                contributor = self[argument]
                if contributor:
                    break

            if contributor:
                for email in emails or []:
                    if email not in contributor.emails:
                        contributor.emails.append(email)
                if contributor.name in contributor.emails and name:
                    contributor.name = name
            else:
                contributor = Contributor(name or emails[0], emails=emails)

            self[contributor.name] = contributor
            for email in contributor.emails or []:
                if not email:
                    continue
                self[email] = contributor
                self[email.lower()] = contributor
            return contributor

        def __iter__(self):
            yielded = set()
            for contributor in self.values():
                if contributor and contributor.name in yielded:
                    continue
                yielded.add(contributor.name)
                yield contributor


    @classmethod
    def from_scm_log(cls, line, contributors=None):
        email = None
        author = None

        for expression in [
            cls.GIT_AUTHOR_RE,
            cls.SVN_AUTHOR_RE,
            cls.SVN_PATCH_FROM_RE,
            cls.AUTOMATED_CHECKIN_RE,
            cls.UNKNOWN_AUTHOR,
            cls.EMPTY_AUTHOR,
            cls.SVN_AUTHOR_Q_RE,
        ]:
            match = expression.match(line)
            if match:
                if 'author' in expression.groupindex:
                    author = match.group('author').lstrip()
                    if '(no author)' in author or 'Automated Checkin' in author or 'Unknown' in author:
                        author = None
                if 'email' in expression.groupindex:
                    email = match.group('email')
                    if '(no author)' in email:
                        email = None
                break
        else:
            raise ValueError("'{}' does not match a known SCM log".format(line))

        if not email and not author:
            return None

        if contributors is not None:
            return contributors.create(author, email)
        return cls(author or email, emails=[email])

    def __init__(self, name, emails=None, status=None, github=None, bitbucket=None):
        self.name = string_utils.decode(name)
        self.emails = list(filter(string_utils.decode, emails or []))
        self.status = status
        self.github = github
        self.bitbucket = bitbucket

    @property
    def email(self):
        if not self.emails:
            return None
        return self.emails[0]

    def __repr__(self):
        return u'{} <{}>'.format(self.name, self.email or '?')

    def __hash__(self):
        return hash(self.name)

    def __cmp__(self, other):
        if isinstance(other, str):
            ref_value = other
        elif isinstance(other, Contributor):
            ref_value = other.name
        elif other is None:
            ref_value = ''
        else:
            raise ValueError('Cannot compare {} with {}'.format(Contributor, type(other)))
        for part in [self.name, self.emails, self.github, self.bitbucket]:
            if part == ref_value:
                return 0
        return 1 if self.name > ref_value else -1

    def __eq__(self, other):
        return self.__cmp__(other) == 0

    def __ne__(self, other):
        return self.__cmp__(other) != 0

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0
