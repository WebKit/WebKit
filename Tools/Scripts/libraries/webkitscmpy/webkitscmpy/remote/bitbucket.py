# Copyright (C) 2021 Apple Inc. All rights reserved.
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
import requests
import six
import sys

from webkitcorepy import decorators
from webkitscmpy import Commit
from webkitscmpy.remote.scm import Scm


class BitBucket(Scm):
    URL_RE = re.compile(r'\Ahttps?://(?P<domain>\S+)/projects/(?P<project>\S+)/repos/(?P<repository>\S+)\Z')

    @classmethod
    def is_webserver(cls, url):
        return True if cls.URL_RE.match(url) else False

    def __init__(self, url, dev_branches=None, prod_branches=None, contributors=None):
        match = self.URL_RE.match(url)
        if not match:
            raise self.Exception("'{}' is not a valid BitBucket project".format(url))
        self.domain = match.group('domain')
        self.project = match.group('project')
        self.name = match.group('repository')

        super(BitBucket, self).__init__(url, dev_branches=dev_branches, prod_branches=prod_branches, contributors=contributors)

    @property
    def is_git(self):
        return True

    def request(self, path=None, params=None, headers=None, api=None, ignore_errors=False):
        headers = {key: value for key, value in headers.items()} if headers else dict()

        params = {key: value for key, value in params.items()} if params else dict()
        params['limit'] = params.get('limit', 500)
        params['start'] = 0
        url = 'https://{domain}/rest/{api}/projects/{project}/repos/{name}{path}'.format(
            api=api or 'api/1.0',
            domain=self.domain,
            project=self.project,
            name=self.name,
            path='/{}'.format(path) if path else '',
        )
        response = requests.get(url, params=params, headers=headers)
        if response.status_code != 200:
            if not ignore_errors:
                sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            return None
        response = response.json()
        result = response.get('values', None)
        if result is None:
            return response

        while not response.get('isLastPage', True):
            params['start'] += params['limit']
            response = requests.get(url, params=params, headers=headers)
            if response.status_code != 200:
                if ignore_errors:
                    break
                raise self.Exception("Failed to assemble pagination requests for '{}', failed on start {}".format(url, params['start']))
            response = response.json()
            result.extend(response.get('values', []))
        return result

    @decorators.Memoize()
    def _distance(self, ref, magnitude=None, condition=None):
        bound = [0, magnitude if magnitude else 65536]
        condition = condition or (lambda val: val)

        branches = self._branches_for('{}~{}'.format(ref, bound[1]), ignore_errors=True)
        while branches and condition(branches):
            bound = [bound[1], bound[1] * 2]
            branches = self._branches_for('{}~{}'.format(ref, bound[1]), ignore_errors=True)

        while True:
            current = bound[0] + int((bound[1] - bound[0]) / 2)

            branches = self._branches_for('{}~{}'.format(ref, current), ignore_errors=True)
            if branches and condition(branches):
                if bound[1] - bound[0] <= 1:
                    return current + 1
                bound = [current, bound[1]]
            else:
                if bound[1] - bound[0] <= 1:
                    return bound[1] + 1 if current == bound[0] else bound[0] + 1
                bound = [bound[0], current]

    def _branches_for(self, hash, ignore_errors=False):
        response = self.request('branches/info/{}'.format(hash), api='branch-utils/latest', ignore_errors=ignore_errors)
        if not response:
            return []
        return sorted([details.get('displayId') for details in response if details.get('displayId')])

    @property
    @decorators.Memoize()
    def default_branch(self):
        response = self.request('branches/default')
        if not response:
            raise self.Exception("Failed to query {} for {}'s default branch".format(self.domain, self.name))
        return response.get('displayId')

    @property
    def branches(self):
        response = self.request('branches')
        if not response:
            return [self.default_branch]
        return sorted([details.get('displayId') for details in response if details.get('displayId')])

    @property
    def tags(self):
        response = self.request('tags')
        if not response:
            return []
        return sorted([details.get('displayId') for details in response if details.get('displayId')])

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        if revision:
            raise self.Exception('Cannot map revisions to commits on BitBucket')

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

            commit_data = self.request('commits/{}'.format(branch), params=dict(limit=1))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch))
            base_ref = commit_data['id']

            if is_default:
                base_count = self._distance(base_ref)
            else:
                base_count = self._distance(base_ref, magnitude=256, condition=lambda val: self.default_branch not in val)

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

        elif branch or tag:
            if hash:
                raise ValueError('Cannot define both tag/branch and hash')
            if branch and tag:
                raise ValueError('Cannot define both tag and branch')
            commit_data = self.request('commits/{}'.format(branch or tag))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch or tag))

        else:
            hash = Commit._parse_hash(hash, do_assert=True)
            commit_data = self.request('commits/{}'.format(hash or self.default_branch))
            if not commit_data:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(hash or 'HEAD'))

        branches = self._branches_for(commit_data['id'])
        if branches:
            branch = self.prioritize_branches(branches)

        else:
            # A commit not on any branches cannot have an identifier
            identifier = None
            branch = None

        branch_point = None
        if include_identifier and branch and branch == self.default_branch:
            if not identifier:
                identifier = self._distance(commit_data['id'])

        elif include_identifier and branch:
            if not identifier:
                identifier = self._distance(commit_data['id'], magnitude=256, condition=lambda val: self.default_branch not in val)
            branch_point = self._distance(commit_data['id']) - identifier

        matches = self.GIT_SVN_REVISION.findall(commit_data['message'])
        revision = int(matches[-1].split('@')[0]) if matches else None

        return Commit(
            hash=commit_data['id'],
            revision=revision,
            branch_point=branch_point,
            identifier=identifier if include_identifier else None,
            branch=branch,
            timestamp=int(commit_data['committerTimestamp'] / 100),
            author=self.contributors.create(
                commit_data.get('committer', {}).get('displayName', None),
                commit_data.get('committer', {}).get('emailAddress', None),
            ), message=commit_data['message'] if include_log else None,
        )

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
        return self.commit(hash=commit_data['id'], include_log=include_log, include_identifier=include_identifier)
