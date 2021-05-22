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

import calendar
import re
import requests
import six
import sys

from datetime import datetime
from requests.auth import HTTPBasicAuth
from webkitcorepy import credentials, decorators
from webkitscmpy import Commit
from webkitscmpy.remote.scm import Scm
from xml.dom import minidom


class GitHub(Scm):
    URL_RE = re.compile(r'\Ahttps?://github.(?P<domain>\S+)/(?P<owner>\S+)/(?P<repository>\S+)\Z')
    EMAIL_RE = re.compile(r'(?P<email>[^@]+@[^@]+)(@.*)?')

    @classmethod
    def is_webserver(cls, url):
        return True if cls.URL_RE.match(url) else False

    def __init__(self, url, dev_branches=None, prod_branches=None, contributors=None, id=None):
        match = self.URL_RE.match(url)
        if not match:
            raise self.Exception("'{}' is not a valid GitHub project".format(url))
        self.api_url = 'https://api.github.{}'.format(match.group('domain'))
        self.owner = match.group('owner')
        self.name = match.group('repository')
        self._hash_link_re = re.compile(r'/{owner}/{name}/[^/]*commit[^/]*/(?P<hash>[0-9a-f]+)'.format(
            owner=self.owner,
            name=self.name,
        ))
        self._cached_credentials = None

        super(GitHub, self).__init__(
            url,
            dev_branches=dev_branches, prod_branches=prod_branches,
            contributors=contributors,
            id=id or self.name.lower(),
        )

    def credentials(self, required=True):
        return credentials(
            url=self.api_url,
            required=required,
            name=self.url.split('/')[2].replace('.', '_').upper(),
            prompt="GitHub's API\nPlease generate a 'Personal access token' via 'Developer settings' for your user",
            key_name='token',
        )

    @property
    def is_git(self):
        return True

    def request(self, path=None, params=None, headers=None, authenticated=None, paginate=True):
        headers = {key: value for key, value in headers.items()} if headers else dict()
        headers['Accept'] = headers.get('Accept', 'application/vnd.github.v3+json')

        username, access_token = self.credentials(required=bool(authenticated))
        auth = HTTPBasicAuth(username, access_token) if username and access_token else None
        if authenticated is False:
            auth = None
        if authenticated and not auth:
            raise self.Exception('Request requires authentication, none provided')

        params = {key: value for key, value in params.items()} if params else dict()
        params['per_page'] = params.get('per_page', 100)
        params['page'] = params.get('page', 1)

        url = '{api_url}/repos/{owner}/{name}{path}'.format(
            api_url=self.api_url,
            owner=self.owner,
            name=self.name,
            path='/{}'.format(path) if path else '',
        )
        response = requests.get(url, params=params, headers=headers, auth=auth)
        if response.status_code != 200:
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            message = response.json().get('message')
            if message:
                sys.stderr.write('Message: {}\n'.format(message))
            return None
        result = response.json()

        while paginate and isinstance(response.json(), list) and len(response.json()) == params['per_page']:
            params['page'] += 1
            response = requests.get(url, params=params, headers=headers, auth=auth)
            if response.status_code != 200:
                raise self.Exception("Failed to assemble pagination requests for '{}', failed on page {}".format(url, params['page']))
            result += response.json()
        return result

    def _count_for_ref(self, ref=None):
        ref = ref or self.default_branch

        # We need the number of parents a commit has to construct identifiers, which is not something GitHub's
        # API lets us find, although the UI does have the information
        response = requests.get('{}/tree/{}'.format(self.url, ref))
        if response.status_code != 200:
            raise self.Exception("Failed to query {}'s UI to find the number of parents {} has".format(self.url, ref))

        # This parsing is pretty brittle, but the webpage may not be valid xml
        count = None
        hash = None
        previous = None
        for line in response.text.splitlines():
            match = self._hash_link_re.search(line)
            if match:
                hash = match.group('hash')
            elif 'aria-label="Commits on ' in line:
                count = int(minidom.parseString(previous).documentElement.childNodes[0].data.replace(',', ''))
                break
            previous = line

        if not hash or not count:
            raise self.Exception("Failed to compute the number of parents to '{}'".format(ref))

        return count, hash

    def _difference(self, reference, head):
        response = self.request('compare/{}...{}'.format(reference, head), headers=dict(Accept='application/vnd.github.VERSION.sha'))
        if not response or not response.get('status') in ['diverged', 'ahead', 'behind']:
            raise self.Exception('Failed to request the difference between {} and {}'.format(reference, head))
        return int(response['behind_by' if response.get('status') == 'behind' else 'ahead_by'])

    def _branches_for(self, hash):
        # We need to find the branch that a commit is on. GitHub's UI provides this information, but the only way to
        # retrieve this information via the API would be to check all branches for the commit, so we scrape the UI.
        response = requests.get('{}/branch_commits/{}'.format(self.url, hash))
        if response.status_code != 200:
            return []

        result = []
        for line in response.text.splitlines():
            if 'class="branch"' not in line:
                continue
            result.append(minidom.parseString(line).documentElement.childNodes[0].childNodes[0].data)

        return result

    @property
    @decorators.Memoize()
    def default_branch(self):
        response = self.request()
        if not response:
            raise self.Exception("Failed to query {} for {}'s default branch".format(self.url, self.name))
        return response.get('default_branch', 'master')

    @property
    def branches(self):
        response = self.request('branches')
        if not response:
            return [self.default_branch]
        return sorted([details.get('name') for details in response if details.get('name')])

    @property
    def tags(self):
        response = self.request('tags')
        if not response:
            return []
        return sorted([details.get('name') for details in response if details.get('name')])

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        if revision:
            raise self.Exception('Cannot map revisions to commits on GitHub')

        # Determine the commit data and branch for a given identifier
        if identifier is not None:
            if revision:
                raise ValueError('Cannot define both revision and identifier')
            if hash:
                raise ValueError('Cannot define both hash and identifier')
            if tag:
                raise ValueError('Cannot define both tag and identifier')

            parsed_branch_point, identifier, parsed_branch = Commit._parse_identifier(identifier, do_assert=True)
            if parsed_branch:
                if branch and branch != parsed_branch:
                    raise ValueError(
                        "Caller passed both 'branch' and 'identifier', but specified different branches ({} and {})".format(
                            branch, parsed_branch,
                        ),
                    )
                branch = parsed_branch

            branch = branch or self.default_branch
            is_default = branch == self.default_branch

            if is_default and parsed_branch_point:
                raise self.Exception('Cannot provide a branch point for a commit on the default branch')

            if is_default:
                base_count, base_ref = self._count_for_ref(ref=self.default_branch)
            else:
                _, base_ref = self._count_for_ref(ref=branch)
                base_count = self._difference(self.default_branch, base_ref)

            if identifier > base_count:
                raise self.Exception('Identifier {} cannot be found on {}'.format(identifier, branch))

            # Negative identifiers are actually commits on the default branch, we will need to re-compute the identifier
            if identifier < 0 and is_default:
                raise self.Exception('Illegal negative identifier on the default branch')

            commit_data = self.request('commits/{}~{}'.format(base_ref, base_count - identifier))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}@{}'".format(identifier, branch or 'HEAD'))

            # If an identifier is negative, unset it so we re-compute before constructing the commit.
            if identifier <= 0:
                identifier = None

        # Determine the commit data for a given branch or tag
        elif branch or tag:
            if hash:
                raise ValueError('Cannot define both tag/branch and hash')
            if branch and tag:
                raise ValueError('Cannot define both tag and branch')
            commit_data = self.request('commits/{}'.format(branch or tag))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch or tag))

        # Determine the commit data for a given hash
        else:
            hash = Commit._parse_hash(hash, do_assert=True)
            commit_data = self.request('commits/{}'.format(hash or self.default_branch))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(hash or 'HEAD'))

        # A commit is often on multiple branches, the canonical branch is the one with the highest priority
        branches = self._branches_for(commit_data['sha'])
        if branches:
            branch = self.prioritize_branches(branches)
        else:
            # A commit not on any branches cannot have an identifier
            identifier = None
            branch = None

        # Define identifiers on default branch
        branch_point = None
        if include_identifier and branch and branch == self.default_branch:
            if not identifier:
                result = self._count_for_ref(ref=commit_data['sha'])
                if not result:
                    raise Exception('{} {}'.format(result, commit_data['sha']))
                identifier, _ = result

        # Define identifiers on branches diverged from the default branch
        elif include_identifier and branch:
            if not identifier:
                identifier = self._difference(self.default_branch, commit_data['sha'])
            branch_point = self._count_for_ref(ref=commit_data['sha'])[0] - identifier

        # Check the commit log for a git-svn revision
        matches = self.GIT_SVN_REVISION.findall(commit_data['commit']['message'])
        revision = int(matches[-1].split('@')[0]) if matches else None

        email_match = self.EMAIL_RE.match(commit_data['commit']['author']['email'])
        timestamp = int(calendar.timegm(datetime.strptime(
            commit_data['commit']['committer']['date'], '%Y-%m-%dT%H:%M:%SZ',
        ).timetuple()))

        # Comparing commits in different repositories involves comparing timestamps. This is problematic because it git,
        # it's possible for a series of commits to share a commit time. To handle this case, we assign each commit a
        # zero-indexed "order" within it's timestamp.
        order = 0
        lhash = commit_data['sha']
        while lhash:
            response = self.request('commits', paginate=False, params=dict(sha=lhash, per_page=20))
            if len(response) <= 1:
                break
            for c in response:
                if lhash == c['sha']:
                    continue
                parent_timestamp = int(calendar.timegm(datetime.strptime(
                    c['commit']['committer']['date'], '%Y-%m-%dT%H:%M:%SZ',
                ).timetuple()))
                if parent_timestamp != timestamp:
                    lhash = None
                    break
                lhash = c['sha']
                order += 1

        return Commit(
            repository_id=self.id,
            hash=commit_data['sha'],
            revision=revision,
            branch_point=branch_point,
            identifier=identifier if include_identifier else None,
            branch=branch,
            timestamp=timestamp,
            order=order,
            author=self.contributors.create(
                commit_data['commit']['author']['name'],
                email_match.group('email') if email_match else None,
            ), message=commit_data['commit']['message'] if include_log else None,
        )

    def commits(self, begin=None, end=None, include_log=True, include_identifier=True):
        begin, end = self._commit_range(begin=begin, end=end, include_identifier=include_identifier)

        previous = end
        cached = [previous]
        while previous:
            response = self.request('commits', paginate=False, params=dict(sha=previous.hash))
            if not response:
                break
            for commit_data in response:
                branch_point = previous.branch_point
                identifier = previous.identifier
                if commit_data['sha'] == previous.hash:
                    cached = cached[:-1]
                else:
                    identifier -= 1

                if not identifier:
                    identifier = branch_point
                    branch_point = None

                matches = self.GIT_SVN_REVISION.findall(commit_data['commit']['message'])
                revision = int(matches[-1].split('@')[0]) if matches else None

                email_match = self.EMAIL_RE.match(commit_data['commit']['author']['email'])
                timestamp = int(calendar.timegm(datetime.strptime(
                    commit_data['commit']['committer']['date'], '%Y-%m-%dT%H:%M:%SZ',
                ).timetuple()))

                previous = Commit(
                    repository_id=self.id,
                    hash=commit_data['sha'],
                    revision=revision,
                    branch=end.branch if identifier and branch_point else self.default_branch,
                    identifier=identifier if include_identifier else None,
                    branch_point=branch_point if include_identifier else None,
                    timestamp=timestamp,
                    author=self.contributors.create(
                        commit_data['commit']['author']['name'],
                        email_match.group('email') if email_match else None,
                    ), order=0,
                    message=commit_data['commit']['message'] if include_log else None,
                )
                if not cached or cached[0].timestamp != previous.timestamp:
                    for c in cached:
                        yield c
                    cached = [previous]
                else:
                    for c in cached:
                        c.order += 1
                    cached.append(previous)

                if previous.hash == begin.hash or previous.timestamp < begin.timestamp:
                    previous = None
                    break

        for c in cached:
            c.order += begin.order
            yield c


    def find(self, argument, include_log=True, include_identifier=True):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        if argument in self.DEFAULT_BRANCHES:
            argument = self.default_branch

        parsed_commit = Commit.parse(argument, do_assert=False)
        if parsed_commit:
            if parsed_commit.branch in self.DEFAULT_BRANCHES:
                parsed_commit.branch = self.default_branch

            return self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
                include_log=include_log,
                include_identifier=include_identifier,
            )

        commit_data = self.request('commits/{}'.format(argument))
        if not commit_data:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))
        return self.commit(hash=commit_data['sha'], include_log=include_log, include_identifier=include_identifier)
