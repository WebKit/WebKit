# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
import os
import re
import requests
import six
import sys

from datetime import datetime
from requests.auth import HTTPBasicAuth
from webkitbugspy import User
from webkitbugspy.github import Tracker
from webkitcorepy import decorators
from webkitscmpy import Commit, Contributor, PullRequest
from webkitscmpy.remote.scm import Scm
from xml.dom import minidom


class GitHub(Scm):
    URL_RE = re.compile(r'\Ahttps?://github.(?P<domain>\S+)/(?P<owner>\S+)/(?P<repository>\S+)\Z')
    EMAIL_RE = re.compile(r'(?P<email>[^@]+@[^@]+)(@.*)?')
    ACCEPT_HEADER = Tracker.ACCEPT_HEADER

    class PRGenerator(Scm.PRGenerator):
        SUPPORTS_DRAFTS = True

        def PullRequest(self, data):
            if not data:
                return None
            issue_ref = data.get('_links', {}).get('issue', {}).get('href')
            # We can construct an issue ref given the url and PR number
            if issue_ref:
                issue = self.repository.tracker.from_string(issue_ref)
            else:
                issue = self.repository.tracker.issue(data['number'])

            return PullRequest(
                number=data['number'],
                title=data.get('title'),
                body=data.get('body'),
                author=self.repository.contributors.create((data.get('user') or data.get('author'))['login']),
                head=(data.get('head') or {}).get('ref', data.get('headRefName')),
                hash=(data.get('head') or {}).get('sha') or ((data.get('headRef') or {}).get('target') or {}).get('oid'),
                base=(data.get('base') or {}).get('ref', data.get('baseRefName')),
                opened=dict(
                    open=True,
                    closed=False,
                    merged=False,
                ).get(data.get('state').lower(), None),
                generator=self,
                metadata=dict(
                    issue=issue,
                    full_name=((data.get('head') or {}).get('repo') or {}).get('full_name') or (data.get('headRepository') or {}).get('nameWithOwner')
                ), url='{}/pull/{}'.format(self.repository.url, data['number']),
                draft=data.get('draft', data.get('isDraft', False)),
            )

        def get(self, number):
            return self.PullRequest(self.repository.request('pulls/{}'.format(int(number))))

        def find(self, opened=True, head=None, base=None):
            assert opened in (True, False, None)

            search = 'repo:{}/{} is:pr'.format(self.repository.owner, self.repository.name)
            if head:
                search += ' head:{}'.format(head)
            if base:
                search += ' base:{}'.format(base)
            if opened is not None:
                search += ' is:open' if opened else ' is:closed'

            data = self.repository.graphql('''query {{
  search(query: "{}", type: ISSUE, last: 100) {{
    edges {{
      node {{
        ... on PullRequest {{
          number
          state
          title
          body
          isDraft
          author {{
            login
          }}
          baseRefName
          headRefName
          headRef {{
            target {{
              oid
            }}
          }}
          headRepository {{
            nameWithOwner
          }}
        }}
      }}
    }}
  }}
}}'''.format(search)
            )
            if not data:
                return

            for node in data.get('data', {}).get('search', {}).get("edges", []):
                nodeData = node.get('node')
                if not nodeData:
                    continue
                yield self.PullRequest(nodeData)
            return

        def create(self, head, title, body=None, commits=None, base=None, draft=None):
            draft = False if draft is None else draft
            for key, value in dict(head=head, title=title).items():
                if not value:
                    raise ValueError("Must define '{}' when creating pull-request".format(key))

            user, _ = self.repository.credentials(required=True)
            url = '{api_url}/repos/{owner}/{name}/pulls'.format(
                api_url=self.repository.api_url,
                owner=self.repository.owner,
                name=self.repository.name,
            )
            response = self.repository.session.post(
                url, auth=HTTPBasicAuth(*self.repository.credentials(required=True)),
                headers=dict(Accept=self.repository.ACCEPT_HEADER),
                json=dict(
                    title=title,
                    body=PullRequest.create_body(body, commits),
                    base=base or self.repository.default_branch,
                    head='{}:{}'.format(user, head),
                    draft=draft,
                ),
            )
            if response.status_code == 422:
                sys.stderr.write(self.repository.tracker.parse_error(response.json()))
                return None
            if response.status_code // 100 != 2:
                sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
                sys.stderr.write(self.repository.tracker.parse_error(response.json()))
                sys.stderr.write(Tracker.REFRESH_TOKEN_PROMPT)
                return None
            result = self.PullRequest(response.json())

            issue = result._metadata.get('issue')
            if not issue or not issue.tracker or not issue.assign(issue.tracker.me()):
                sys.stderr.write("Failed to assign '{}' to '{}'\n".format(result, user))
            return result

        def update(self, pull_request, head=None, title=None, body=None, commits=None, base=None, opened=None, draft=None):
            if not isinstance(pull_request, PullRequest):
                raise ValueError("Expected 'pull_request' to be of type '{}' not '{}'".format(PullRequest, type(pull_request)))
            if not any((head, title, body, commits, base)) and opened is None:
                raise ValueError('No arguments to update pull-request provided')
            if draft is not None:
                sys.stderr.write('GitHub does not allow editing draft state via API\n')

            user, _ = self.repository.credentials(required=True)
            updates = dict(
                title=title or pull_request.title,
                base=base or pull_request.base,
            )
            if head:
                updates['head'] = '{}:{}'.format(user, head)
            if body or commits:
                updates['body'] = PullRequest.create_body(body, commits)
            if opened is not None:
                updates['state'] = 'open' if opened else 'closed'
            url = '{api_url}/repos/{owner}/{name}/pulls/{number}'.format(
                api_url=self.repository.api_url,
                owner=self.repository.owner,
                name=self.repository.name,
                number=pull_request.number,
            )
            response = self.repository.session.post(
                url, auth=HTTPBasicAuth(*self.repository.credentials(required=True)),
                headers=dict(Accept=self.repository.ACCEPT_HEADER),
                json=updates,
            )
            if response.status_code == 422:
                sys.stderr.write(self.repository.tracker.parse_error(response.json()))
                pull_request._opened = False
                return pull_request
            if response.status_code // 100 != 2:
                sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
                sys.stderr.write(self.repository.tracker.parse_error(response.json()))
                sys.stderr.write(Tracker.REFRESH_TOKEN_PROMPT)
                return None
            data = response.json()

            pull_request.title = data.get('title', pull_request.title)
            if data.get('body'):
                pull_request.body, pull_request.commits = pull_request.parse_body(data.get('body'))
            if data.get('user', {}).get('login'):
                pull_request.author = self.repository.contributors.create(data['user']['login'])
            pull_request.head = data.get('head', {}).get('displayId', pull_request.base)
            pull_request.base = data.get('base', {}).get('displayId', pull_request.base)
            pull_request._opened = dict(
                open=True,
                closed=False,
                merged=False,
            ).get(data.get('state'), None)
            pull_request.generator = self
            issue_ref = data.get('_links', {}).get('issue', {}).get('href')
            pull_request._metadata = dict(
                issue=self.repository.tracker.from_string(issue_ref) if issue_ref else None,
            )

            assignees = [node.get('login') for node in data.get('assignees', []) if node.get('login')]
            if user not in assignees:
                issue = pull_request._metadata.get('issue')
                if not issue or not issue.tracker or not issue.assign(issue.tracker.me()):
                    sys.stderr.write("Failed to assign '{}' to '{}'\n".format(pull_request, user))

            return pull_request

        def _contributor(self, username):
            result = self.repository.contributors.get(username, None)
            if result:
                return result

            found = self.repository.tracker.user(username=username)
            if found:
                result = self.repository.contributors.create(found.name, found.email)
            else:
                result = self.repository.contributors.create(username)
            result.github = username
            self.repository.contributors[username] = result
            return result

        def reviewers(self, pull_request):
            response = self.repository.request('pulls/{}/requested_reviewers'.format(pull_request.number))
            pull_request._reviewers = [self._contributor(user['login']) for user in response.get('users', [])]
            pull_request._approvers = []
            pull_request._blockers = []

            state_for = {}
            for review in self.repository.request('pulls/{}/reviews'.format(pull_request.number)):
                state_for[self._contributor(review['user']['login'])] = review.get('state')

            needs_status = Contributor.REVIEWER in self.repository.contributors.statuses
            for contributor, status in state_for.items():
                pull_request._reviewers.append(contributor)
                if status == 'APPROVED' and (not needs_status or contributor.status == Contributor.REVIEWER):
                    pull_request._approvers.append(contributor)
                elif status == 'CHANGES_REQUESTED':
                    pull_request._blockers.append(contributor)

            pull_request._reviewers = sorted(pull_request._reviewers)
            return pull_request

        def comment(self, pull_request, content):
            issue = pull_request._metadata.get('issue')
            if not issue:
                old = pull_request
                pull_request = self.get(old.number)
                pull_request._reviewers = old._reviewers
                pull_request._approvers = old._approvers
                pull_request._blockers = old._blockers
                issue = pull_request._metadata.get('issue')
            if not issue:
                raise self.repository.Exception('Failed to find issue underlying pull-request')
            issue.add_comment(content)

        def comments(self, pull_request):
            issue = pull_request._metadata.get('issue')
            if not issue:
                old = pull_request
                pull_request = self.get(old.number)
                pull_request._reviewers = old._reviewers
                pull_request._approvers = old._approvers
                pull_request._blockers = old._blockers
                issue = pull_request._metadata.get('issue')
            if not issue:
                raise self.repository.Exception('Failed to find issue underlying pull-request')
            for comment in issue.comments:
                yield PullRequest.Comment(
                    author=self._contributor(comment.user.username),
                    timestamp=comment.timestamp,
                    content=comment.content,
                )

        def review(self, pull_request, comment=None, approve=None):
            if not comment and approve is None:
                raise self.repository.Exception('No review comment or approval provided')

            body = dict(
                event={
                    True: 'APPROVE',
                    False: 'REQUEST_CHANGES',
                }.get(approve, 'COMMENT')
            )
            if comment:
                body['body'] = comment

            url = '{api_url}/repos/{owner}/{name}/pulls/{number}/reviews'.format(
                api_url=self.repository.api_url,
                owner=self.repository.owner,
                name=self.repository.name,
                number=pull_request.number,
            )
            response = self.repository.session.post(
                url, auth=HTTPBasicAuth(*self.repository.credentials(required=True)),
                headers=dict(Accept=self.repository.ACCEPT_HEADER),
                json=body,
            )

            if response.status_code // 100 != 2:
                sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
                sys.stderr.write(self.repository.tracker.parse_error(response.json()))
                if response.status_code != 422:
                    sys.stderr.write(Tracker.REFRESH_TOKEN_PROMPT)
                return None

            me = self._contributor(self.repository.credentials(required=True)[0])
            if approve and pull_request._approvers:
                if me in (pull_request._blockers or []):
                    pull_request._blockers.remove(me)
                if me not in pull_request._approvers:
                    pull_request._approvers.append(me)
            if approve is False and pull_request._blockers:
                if me in (pull_request._approvers or []):
                    pull_request._approvers.remove(me)
                if me not in pull_request._blockers:
                    pull_request._blockers.append(me)
            if comment and pull_request._comments:
                pull_request._comments.append(PullRequest.Comment(
                    author=me,
                    timestamp=time.time(),
                    content=comment,
                ))

            return pull_request


    @classmethod
    def is_webserver(cls, url):
        return True if cls.URL_RE.match(url) else False

    def __init__(self, url, dev_branches=None, prod_branches=None, contributors=None, id=None, proxies=None):
        match = self.URL_RE.match(url)
        if not match:
            raise self.Exception("'{}' is not a valid GitHub project".format(url))
        self.api_url = 'https://api.github.{}'.format(match.group('domain'))
        self.owner = match.group('owner')
        self.name = match.group('repository')
        self.session = requests.Session()
        if proxies:
            self.session.proxies = proxies
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

        self.pull_requests = self.PRGenerator(self)
        users = User.Mapping()
        for contributor in contributors or []:
            if contributor.github:
                users.create(contributor.name, contributor.github, contributor.emails)
        self.tracker = Tracker(url, users=users, session=self.session)

    def credentials(self, required=True, validate=False, save_in_keyring=None):
        return self.tracker.credentials(required=required, validate=validate, save_in_keyring=save_in_keyring)

    @property
    def is_git(self):
        return True

    def request(self, path=None, params=None, headers=None, authenticated=None, paginate=True, json=None, method='GET', endpoint_url=None, files=None, data=None, stream=False):
        headers = {key: value for key, value in headers.items()} if headers else dict()
        headers['Accept'] = headers.get('Accept', self.ACCEPT_HEADER)

        username, access_token = self.credentials(required=bool(authenticated))
        auth = HTTPBasicAuth(username, access_token) if username and access_token else None
        if authenticated is False:
            auth = None
        if authenticated and not auth:
            raise self.Exception('Request requires authentication, none provided')

        params = {key: value for key, value in params.items()} if params else dict()
        if paginate:
            params['per_page'] = params.get('per_page', 100)
            params['page'] = params.get('page', 1)

        json = {key: value for key, value in json.items()} if json else None

        url = endpoint_url
        if not url:
            url = '{api_url}/repos/{owner}/{name}{path}'.format(
                api_url=self.api_url,
                owner=self.owner,
                name=self.name,
                path='/{}'.format(path) if path else '',
            )
        response = self.session.request(method, url, params=params, json=json, headers=headers, auth=auth, files=files, data=data, stream=stream)
        is_json_response = response.headers.get('Content-Type', '').split(';')[0] in ['application/json', 'text/json']
        if authenticated is None and not auth and response.status_code // 100 == 4:
            return self.request(path=path, params=params, headers=headers, authenticated=True, paginate=paginate, json=json, method=method, endpoint_url=endpoint_url, files=files, data=data, stream=stream)
        if response.status_code not in [200, 201]:
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            message = response.json().get('message') if is_json_response else ''
            if message:
                sys.stderr.write('Message: {}\n'.format(message))
            if auth:
                sys.stderr.write(Tracker.REFRESH_TOKEN_PROMPT)
            return None

        if not is_json_response:
            return response

        result = response.json()

        while paginate and isinstance(response.json(), list) and len(response.json()) == params['per_page']:
            params['page'] += 1
            response = self.session.get(url, params=params, headers=headers, auth=auth)
            if response.status_code != 200:
                raise self.Exception("Failed to assemble pagination requests for '{}', failed on page {}".format(url, params['page']))
            result += response.json()
        return result

    def graphql(self, query):
        url = '{}/graphql'.format(self.api_url)
        response = self.session.post(
            url, json=dict(query=query),
            auth=HTTPBasicAuth(*self.credentials(required=True)),
        )
        if response.status_code != 200:
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            message = response.json().get('message')
            if message:
                sys.stderr.write('Message: {}\n'.format(message))
            sys.stderr.write(Tracker.REFRESH_TOKEN_PROMPT)
            return None
        return response.json()

    def _count_for_ref(self, ref=None):
        ref = ref or self.default_branch

        # We need the number of parents a commit has to construct identifiers, which is not something GitHub's
        # API lets us find, although the UI does have the information
        response = self.session.get('{}/tree/{}'.format(self.url, ref))
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
        response = self.session.get('{}/branch_commits/{}'.format(self.url, hash))
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

    def files_changed(self, argument=None):
        if not argument:
            return self.modified()
        commit = self.find(argument, include_log=False, include_identifier=False)
        if not commit:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))

        return [
            file.get('filename')
            for file in self.request('commits/{}'.format(commit.hash)).get('files', [])
            if file.get('filename')
        ]

    def create_release(self, name, tag_name, target_commitish='main', body='', draft=False, prerelease=False):
        data = {
            'name': name,
            'tag_name': tag_name,
            'target_commitish': target_commitish,
            'body': body,
            'draft': draft,
            'prerelease': prerelease
        }
        return self.request('releases', json=data, paginate=False, authenticated=True, method='POST')

    def upload_release_asset(self, release_tag_name, source_filename, mime_type, asset_name=None, asset_label=None, file_like_object=None):
        source_filename = os.path.abspath(os.path.realpath(os.path.expanduser(source_filename)))
        source_basename = os.path.basename(source_filename)
        asset_name = asset_name if asset_name else source_basename
        asset_label = asset_label if asset_label else source_basename
        headers = dict()
        headers['Content-Type'] = mime_type
        params = dict(name=asset_name, label=asset_label)
        release_info = self.request('releases/tags/{tag}'.format(tag=release_tag_name), authenticated=True)
        upload_url = release_info['upload_url'][0:release_info['upload_url'].rfind('{?name,label}')]
        file_object = file_like_object if file_like_object else open(source_filename, 'rb')
        try:
            return self.request(
                params=params,
                authenticated=True,
                paginate=False,
                method='POST',
                endpoint_url=upload_url,
                headers=headers,
                data=file_object
            )
        finally:
            if not file_like_object:
                file_object.close()

    def download_release_assets(self, destination_directory, release_tag_name=None):
        assert os.path.isdir(destination_directory), '`{destination_directory}` must be a directory'.format(destination_directory=destination_directory)
        path = 'releases/tags/{release_tag_name}'.format(release_tag_name=release_tag_name) if release_tag_name else 'releases/latest'
        release_info = self.request(path, authenticated=True, paginate=False)
        release_assets = self.request(
            'releases/{id}/assets'.format(id=release_info['id']),
            authenticated=True,
            paginate=False
        )
        headers = dict(Accept='application/octet-stream')
        for asset_info in release_assets:
            with self.request(
                'releases/assets/{id}'.format(id=asset_info['id']),
                headers=headers,
                authenticated=True,
                paginate=False,
                stream=True
            ) as response:
                with open(os.path.join(destination_directory, asset_info['name']), 'wb') as file_object:
                    for chunk in response.iter_content(chunk_size=10240):
                        file_object.write(chunk)
