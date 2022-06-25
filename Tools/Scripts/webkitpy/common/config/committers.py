# Copyright (c) 2011, 2021 Apple Inc. All rights reserved.
# Copyright (c) 2009, 2011, 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# WebKit's Python module for committer and reviewer validation.

import fnmatch
import json
import os
import sys

from webkitcorepy import string_utils, unicode

from functools import reduce
from webkitpy.common.editdistance import edit_distance
from webkitpy.common.system.filesystem import FileSystem


class Contributor(object):
    def __init__(self, name, email_or_emails, irc_nickname_or_nicknames=None, alias_or_aliases=None, expertise=None, github=None):
        assert(name)
        assert(email_or_emails)
        self.full_name = name
        if isinstance(email_or_emails, str):
            self.emails = [email_or_emails]
        else:
            self.emails = email_or_emails
        self._case_preserved_emails = self.emails
        self.emails = list(map(lambda email: email.lower(), self.emails))  # Emails are case-insensitive.
        self.github = github

        if isinstance(irc_nickname_or_nicknames, str):
            self.irc_nicknames = [irc_nickname_or_nicknames]
        else:
            self.irc_nicknames = irc_nickname_or_nicknames

        if isinstance(alias_or_aliases, str):
            self.aliases = [alias_or_aliases]
        else:
            self.aliases = alias_or_aliases

        self.expertise = expertise
        self.can_commit = False
        self.can_review = False
        self.is_bot = False

    def bugzilla_email(self):
        # FIXME: We're assuming the first email is a valid bugzilla email,
        # which might not be right.
        return self.emails[0]

    @property
    def email(self):
        self.bugzilla_email()

    def __str__(self):
        return string_utils.encode(u'"{}" <{}>'.format(unicode(self.full_name), unicode(self.emails[0])), target_type=str)

    def __unicode__(self):
        return u'"{}" <{}>'.format(unicode(self.full_name), unicode(self.emails[0]))

    def __hash__(self):
        return hash(self.full_name) \
            ^ reduce(lambda a, b: hash(a) ^ hash(b), (self.emails or []) + [0]) \
            ^ reduce(lambda a, b: hash(a) ^ hash(b), (self._case_preserved_emails or []) + [0]) \
            ^ reduce(lambda a, b: hash(a) ^ hash(b), (self.irc_nicknames or []) + [0]) \
            ^ hash(self.expertise) \
            ^ hash(self.can_commit) \
            ^ hash(self.can_review)

    def __cmp__(self, other):
        if other is None:
            return 1

        for member in ('full_name', 'emails', '_case_preserved_emails', 'irc_nicknames', 'can_commit', 'can_review', 'expertise'):
            if member in ('emails', '_case_preserved_emails', 'irc_nicknames'):
                self_list = sorted(getattr(self, member) or [])
                other_list = sorted(getattr(other, member) or [])
                for i in range(min(len(self_list), len(other_list))):
                    if self_list[i] != other_list[i]:
                        return 1 if self_list[i] > other_list[i] else -1
                if len(self_list) != len(other_list):
                    return len(self_list) - len(other_list)

            else:
                if getattr(self, member) != getattr(other, member):
                    return 1 if getattr(self, member) > getattr(other, member) else -1

        return 0

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

    def contains_string(self, search_string):
        string = search_string.lower()
        if string in self.full_name.lower():
            return True
        if self.irc_nicknames:
            for nickname in self.irc_nicknames:
                if string in nickname.lower():
                    return True
        if self.aliases:
            for alias in self.aliases:
                if string in alias.lower():
                    return True
        for email in self.emails:
            if string in email:
                return True
        return False

    def mentioned_in_text(self, text):
        lower_text = text.lower()
        if self.full_name.lower() in lower_text:
            return True
        if self.irc_nicknames:
            for nickname in self.irc_nicknames:
                if nickname.lower() in lower_text:
                    return True
        if self.aliases:
            for alias in self.aliases:
                if alias.lower() in lower_text:
                    return True
        for email in self.emails:
            if email in lower_text:
                return True
        return False

    def matches_glob(self, glob_string):
        if fnmatch.fnmatch(self.full_name, glob_string):
            return True
        if self.irc_nicknames:
            for nickname in self.irc_nicknames:
                if fnmatch.fnmatch(nickname, glob_string):
                    return True
        for email in self.emails:
            if fnmatch.fnmatch(email, glob_string):
                return True
        return False

    def as_dict(self):
        info = dict(
            name=self.full_name,
            emails=self._case_preserved_emails,
        )
        if self.github:
            info["github"] = self.github
        if self.aliases:
            info["aliases"] = self.aliases

        if self.can_review:
            info["status"] = "reviewer"
        elif self.can_commit:
            info["status"] = "committer"

        if self.irc_nicknames:
            info["nicks"] = self.irc_nicknames

        if self.expertise:
            info["expertise"] = self.expertise

        if self.is_bot:
            info["status"] = "bot"

        return info


class Committer(Contributor):
    def __init__(self, name, email_or_emails, irc_nickname=None, alias_or_aliases=None, expertise=None, github=None):
        Contributor.__init__(self, name, email_or_emails, irc_nickname, alias_or_aliases, expertise, github)
        self.can_commit = True


class Reviewer(Committer):
    def __init__(self, name, email_or_emails, irc_nickname=None, alias_or_aliases=None, expertise=None, github=None):
        Committer.__init__(self, name, email_or_emails, irc_nickname, alias_or_aliases, expertise, github)
        self.can_review = True


class Bot(Contributor):
    def __init__(self, name, email_or_emails, irc_nickname=None, alias_or_aliases=None, expertise=None, github=None):
        Contributor.__init__(self, name, email_or_emails, irc_nickname, alias_or_aliases, expertise, github)
        self.is_bot = True


class CommitterList(object):

    # Committers and reviewers are passed in to allow easy testing
    def __init__(self,
                 committers=[],
                 reviewers=[],
                 contributors=[]):
        if committers or reviewers or contributors:
            self.load_test_data(committers, reviewers, contributors)
        else:
            self.load_json()

        self._contributors_by_name = {}
        self._accounts_by_email = {}
        self._accounts_by_login = {}

    def load_json(self):
        json_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))))),
            'metadata', 'contributors.json',
        )
        try:
            contributors = json.loads(FileSystem().read_text_file(json_path))
        except ValueError as e:
            sys.exit('contributors.json is malformed: ' + str(e))

        self._contributors = []
        self._committers = []
        self._reviewers = []

        for data in contributors:
            name = data.get('name')
            status = data.get('status')
            if not name:
                continue
            if status == "reviewer":
                contributor = Reviewer(name, data.get('emails'), data.get('nicks'), data.get('aliases'), data.get('expertise'), data.get('github'))
                self._reviewers.append(contributor)
                self._committers.append(contributor)
            elif status == "committer":
                contributor = Committer(name, data.get('emails'), data.get('nicks'), data.get('aliases'), data.get('expertise'), data.get('github'))
                self._committers.append(contributor)
            elif status == 'bot':
                contributor = Bot(name, data.get('emails'), data.get('nicks'), data.get('aliases'), data.get('expertise'), data.get('github'))
            else:
                contributor = Contributor(name, data.get('emails'), data.get('nicks'), data.get('aliases'), data.get('expertise'), data.get('github'))

            self._contributors.append(contributor)

    def load_test_data(self, committers, reviewers, contributors):
        self._contributors = contributors + committers + reviewers
        self._committers = committers + reviewers
        self._reviewers = reviewers

    def as_json(self):
        return json.dumps(
            [contributor.as_dict() for contributor in sorted(self._contributors)],
            sort_keys=True, indent=3, separators=(',', ' : '),
        )

    def reformat_in_place(self):
        json_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))))),
            'metadata', 'contributors.json',
        )
        FileSystem().write_text_file(json_path, self.as_json())

    # Contributors who are not in any other category.
    def _exclusive_contributors(self):
        return list(filter(lambda contributor: not (contributor.can_commit or contributor.can_review), self._contributors))

    # Committers who are not reviewers.
    def _exclusive_committers(self):
        return list(filter(lambda contributor: contributor.can_commit and not contributor.can_review, self._committers))

    # This is the superset of contributors + committers + reviewers
    def contributors(self):
        return self._contributors

    # This is the superset of committers + reviewers
    def committers(self):
        return self._committers

    def reviewers(self):
        return self._reviewers

    def _name_to_contributor_map(self):
        if not len(self._contributors_by_name):
            for contributor in self._contributors:
                assert contributor.full_name
                assert contributor.full_name.lower() not in self._contributors_by_name  # We should never have duplicate names.
                self._contributors_by_name[contributor.full_name.lower()] = contributor
                if contributor.aliases is None:
                    continue
                for alias in contributor.aliases:
                    assert alias.lower() not in self._contributors_by_name
                    self._contributors_by_name[alias.lower()] = contributor
        return self._contributors_by_name

    def _email_to_account_map(self):
        if not len(self._accounts_by_email):
            for account in self._contributors:
                for email in account.emails:
                    assert(email not in self._accounts_by_email)  # We should never have duplicate emails.
                    self._accounts_by_email[email] = account
        return self._accounts_by_email

    def _login_to_account_map(self):
        if not len(self._accounts_by_login):
            for account in self._contributors:
                if account.emails:
                    login = account.bugzilla_email()
                    assert(login not in self._accounts_by_login)  # We should never have duplicate emails.
                    self._accounts_by_login[login] = account
        return self._accounts_by_login

    def _committer_only(self, record):
        if record and not record.can_commit:
            return None
        return record

    def _reviewer_only(self, record):
        if record and not record.can_review:
            return None
        return record

    def contributor_by_irc_nickname(self, irc_nickname):
        for contributor in self.contributors():
            # FIXME: This should do case-insensitive comparison or assert that all IRC nicknames are in lowercase
            if contributor.irc_nicknames and irc_nickname in contributor.irc_nicknames:
                return contributor
        return None

    def contributors_by_search_string(self, string):
        glob_matches = list(filter(lambda contributor: contributor.matches_glob(string), self.contributors()))
        return glob_matches or list(filter(lambda contributor: contributor.contains_string(string), self.contributors()))

    def contributors_by_email_username(self, string):
        string = string + '@'
        result = []
        for contributor in self.contributors():
            for email in contributor.emails:
                if email.startswith(string):
                    result.append(contributor)
                    break
        return result

    def _contributor_name_shorthands(self, contributor):
        if ' ' not in contributor.full_name:
            return []
        split_fullname = contributor.full_name.split()
        first_name = split_fullname[0]
        last_name = split_fullname[-1]
        return first_name, last_name, first_name + last_name[0], first_name + ' ' + last_name[0]

    def _tokenize_contributor_name(self, contributor):
        full_name_in_lowercase = contributor.full_name.lower()
        tokens = [full_name_in_lowercase] + full_name_in_lowercase.split()
        if contributor.irc_nicknames:
            return tokens + [nickname.lower() for nickname in contributor.irc_nicknames if len(nickname) > 5]
        return tokens

    def contributors_by_fuzzy_match(self, string):
        string_in_lowercase = string.lower()

        # 1. Exact match for fullname, email and irc_nicknames
        account = self.contributor_by_name(string_in_lowercase) or self.contributor_by_email(string_in_lowercase) or self.contributor_by_irc_nickname(string_in_lowercase)
        if account:
            return [account], 0

        # 2. Exact match for email username (before @)
        accounts = self.contributors_by_email_username(string_in_lowercase)
        if accounts and len(accounts) == 1:
            return accounts, 0

        # 3. Exact match for first name, last name, and first name + initial combinations such as "Dan B" and "Tim H"
        accounts = [contributor for contributor in self.contributors() if string in self._contributor_name_shorthands(contributor)]
        if accounts and len(accounts) == 1:
            return accounts, 0

        # 4. Finally, fuzzy-match using edit-distance
        string = string_in_lowercase
        contributorWithMinDistance = []
        minDistance = len(string) / 2 - 1
        for contributor in self.contributors():
            tokens = self._tokenize_contributor_name(contributor)
            editdistances = [edit_distance(token, string) for token in tokens if abs(len(token) - len(string)) <= minDistance]
            if not editdistances:
                continue
            distance = min(editdistances)
            if distance == minDistance:
                contributorWithMinDistance.append(contributor)
            elif distance < minDistance:
                contributorWithMinDistance = [contributor]
                minDistance = distance
        if not len(contributorWithMinDistance):
            return [], len(string)
        return contributorWithMinDistance, minDistance

    def contributor_by_email(self, email):
        return self._email_to_account_map().get(email.lower()) if email else None

    def contributor_by_name(self, name):
        return self._name_to_contributor_map().get(name.lower()) if name else None

    def committer_by_email(self, email):
        return self._committer_only(self.contributor_by_email(email))

    def committer_by_name(self, name):
        return self._committer_only(self.contributor_by_name(name))

    def reviewer_by_email(self, email):
        return self._reviewer_only(self.contributor_by_email(email))

    def reviewer_by_name(self, email):
        return self._reviewer_only(self.contributor_by_name(email))
