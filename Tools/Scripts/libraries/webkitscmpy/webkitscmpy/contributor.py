# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import re


class Contributor(object):
    GIT_AUTHOR_RE = re.compile(r'Author: (?P<author>.*) <(?P<email>.*)>')
    SVN_AUTHOR_RE = re.compile(r'r\d+ \| (?P<email>.*) \| (?P<date>.*) \| \d+ lines')
    SVN_PATCH_FROM_RE = re.compile(r'Patch by (?P<author>.*) <(?P<email>.*)> on \d+-\d+-\d+')

    by_email = dict()
    by_name = dict()

    @classmethod
    def clear(cls):
        cls.by_email = dict()
        cls.by_name = dict()

    @classmethod
    def from_scm_log(cls, line):
        author = None

        for expression in [cls.GIT_AUTHOR_RE, cls.SVN_AUTHOR_RE, cls.SVN_PATCH_FROM_RE]:
            match = expression.match(line)
            if match:
                if 'author' in expression.groupindex:
                    author = match.group('author')
                email = match.group('email')
                break
        else:
            raise ValueError("'{}' does not match a known SCM log")

        contributor = cls.by_name.get(author or email)
        if not contributor:
            contributor = cls.by_email.get(email)

        if not contributor:
            contributor = cls(author or email, emails=[email])
            cls.by_name[contributor.name] = contributor
        elif email not in contributor.emails:
            contributor.emails.append(email)

        cls.by_name[email] = contributor
        return contributor

    def __init__(self, name, emails=None):
        self.name = name
        self.emails = emails or []

    @property
    def email(self):
        if not self.emails:
            return None
        return self.emails[0]

    def __repr__(self):
        return '{} <{}>'.format(self.name, self.email)

    def __hash__(self):
        return hash(self.name)

    def __cmp__(self, other):
        if isinstance(other, str):
            ref_value = other
        elif isinstance(other, Contributor):
            ref_value = other.name
        else:
            raise ValueError('Cannot compare {} with {}'.format(Contributor, type(other)))
        if self.name == ref_value:
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
