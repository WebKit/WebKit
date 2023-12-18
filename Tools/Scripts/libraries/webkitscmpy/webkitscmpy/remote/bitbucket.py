# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
import sys

from datetime import datetime
from webkitcorepy import decorators, string_utils, CallByNeed
from webkitscmpy import Commit, Contributor, PullRequest
from webkitscmpy.remote.scm import Scm

requests = CallByNeed(lambda: __import__('requests'))


class BitBucket(Scm):
    URL_RE = re.compile(r'\Ahttps?://(?P<domain>\S+)/projects/(?P<project>\S+)/repos/(?P<repository>\S+)\Z')
    DIFF_CONTEXT = 3

    class PRGenerator(Scm.PRGenerator):
        TITLE_CHAR_LIMIT = 254
        BODY_CHAR_LIMIT = 32766

        def PullRequest(self, data):
            if not data:
                return None
            author = self.repository.contributors.create(
                data['author']['user']['displayName'],
                data['author']['user'].get('emailAddress', None),
                bitbucket=data['author']['user'].get('name', None),
            )

            result = PullRequest(
                number=data['id'],
                title=data.get('title'),
                body=data.get('description'),
                author=author,
                head=data['fromRef']['displayId'],
                hash=data['fromRef'].get('latestCommit', None),
                base=data['toRef']['displayId'],
                opened=True if data.get('open') else (False if data.get('closed') else None),
                generator=self,
                url='{}/pull-requests/{}/overview'.format(self.repository.url, data['id']),
                draft=False,
            )

            result._reviewers = []
            result._approvers = []
            result._blockers = []
            needs_status = Contributor.REVIEWER in self.repository.contributors.statuses
            for rdata in data.get('reviewers', []):
                reviewer = self.repository.contributors.create(
                    rdata['user']['displayName'],
                    rdata['user'].get('emailAddress', None),
                    bitbucket=rdata['user'].get('name', None),
                )
                result._reviewers.append(reviewer)
                if rdata.get('approved', False) and (not needs_status or reviewer.status == Contributor.REVIEWER):
                    result._approvers.append(reviewer)
                if rdata.get('status') == 'NEEDS_WORK':
                    result._blockers.append(reviewer)

            result._reviewers = sorted(result._reviewers)
            return result

        def get(self, number):
            return self.PullRequest(self.repository.request('pull-requests/{}'.format(int(number))))

        def find(self, opened=True, head=None, base=None):
            assert opened in (True, False, None)

            params = dict(
                limit=100,
                withProperties='false',
                withAttributes='false',
            )
            if opened is True:
                params['state'] = 'OPEN'
            elif not opened:
                params['state'] = ['DECLINED', 'MERGED', 'SUPERSEDED']

            if head:
                params['direction'] = 'OUTGOING'
                params['at'] = 'refs/heads/{}'.format(head)
            data = self.repository.request('pull-requests', params=params)
            for datum in data or []:
                if base and not datum['toRef']['id'].endswith(base):
                    continue
                yield self.PullRequest(datum)

            # Stash is bad at filter for open and closed PRs at the same time
            if opened is None:
                params['state'] = 'OPEN'
                data = self.repository.request('pull-requests', params=params)
                for datum in data or []:
                    if base and not datum['toRef']['id'].endswith(base):
                        continue
                    yield self.PullRequest(datum)

        def create(self, head, title, body=None, commits=None, base=None, draft=None):
            if draft:
                sys.stderr.write('Bitbucket does not support the concept of a "draft" pull request\n')

            for key, value in dict(head=head, title=title).items():
                if not value:
                    raise ValueError("Must define '{}' when creating pull-request".format(key))

            if len(title) > self.TITLE_CHAR_LIMIT:
                raise ValueError('Title length too long. Limit is: {}'.format(self.TITLE_CHAR_LIMIT))
            description = PullRequest.create_body(body, commits, linkify=False)
            if description and len(description) > self.BODY_CHAR_LIMIT:
                raise ValueError('Body length too long. Limit is: {}'.format(self.BODY_CHAR_LIMIT))
            fromRef = dict(
                id='refs/heads/{}'.format(head),
                repository=dict(
                    slug=self.repository.name,
                    project=dict(key=self.repository.project),
                ),
            )
            if commits:
                fromRef['latestCommit'] = commits[0].hash,
            response = requests.post(
                'https://{domain}/rest/api/1.0/projects/{project}/repos/{name}/pull-requests'.format(
                    domain=self.repository.domain,
                    project=self.repository.project,
                    name=self.repository.name,
                ), json=dict(
                    title=title,
                    description=PullRequest.create_body(body, commits, linkify=False),
                    fromRef=fromRef,
                    toRef=dict(
                        id='refs/heads/{}'.format(base or self.repository.default_branch),
                        repository=dict(
                            slug=self.repository.name,
                            project=dict(key=self.repository.project),
                        ),
                    ),
                ),
            )
            if response.status_code // 100 != 2:
                return None
            return self.PullRequest(response.json())

        def update(self, pull_request, head=None, title=None, body=None, commits=None, base=None, opened=None, draft=None):
            if not isinstance(pull_request, PullRequest):
                raise ValueError("Expected 'pull_request' to be of type '{}' not '{}'".format(PullRequest, type(pull_request)))

            if draft:
                sys.stderr.write('Bitbucket does not support the concept of a "draft" pull request\n')

            pr_url = 'https://{domain}/rest/api/1.0/projects/{project}/repos/{name}/pull-requests/{id}'.format(
                domain=self.repository.domain,
                project=self.repository.project,
                name=self.repository.name,
                id=pull_request.number,
            )

            if opened is not None:
                response = requests.get(pr_url)
                if response.status_code // 100 != 2:
                    return None
                response = requests.post(
                    '{}/{}'.format(pr_url, 'reopen' if opened else 'decline'),
                    json=dict(version=response.json().get('version', 0)),
                )
                if response.status_code // 100 != 2:
                    return None

                pull_request._opened = opened
                if not any((head, title, body, commits, base)):
                    return pull_request

            if not any((head, title, body, commits, base)):
                raise ValueError('No arguments to update pull-request provided')

            to_change = dict()
            if title:
                to_change['title'] = title
            if body or commits:
                to_change['description'] = PullRequest.create_body(body, commits, linkify=False)
            if head:
                to_change['fromRef'] = dict(
                    id='refs/heads/{}'.format(head),
                    repository=dict(
                        slug=self.repository.name,
                        project=dict(key=self.repository.project),
                    ),
                )
            if commits:
                if to_change.get('fromRef'):
                    to_change['fromRef']['latestCommit'] = commits[0].hash
                else:
                    to_change['fromRef'] = dict(latestCommit=commits[0].hash)
            if base:
                to_change['toRef'] = dict(
                    id='refs/heads/{}'.format(base or self.repository.default_branch),
                    repository=dict(
                        slug=self.repository.name,
                        project=dict(key=self.repository.project),
                    ),
                )

            response = requests.get(pr_url)
            if response.status_code // 100 != 2:
                return None
            data = response.json()
            data.pop('author', None)
            data.pop('participants', None)
            data.update(to_change)

            response = requests.put(pr_url, json=data)
            if response.status_code // 100 != 2:
                for error in response.json().get('errors', []):
                    sys.stderr.write('{}: {}\n'.format(error.get('context'), error.get('message')))
                return None
            data = response.json()

            pull_request.title = data.get('title', pull_request.title)
            if data.get('description'):
                pull_request.body, pull_request.commits = pull_request.parse_body(data.get('description'))
            user = data.get('author', {}).get('user', {})
            if user.get('displayName') and user.get('emailAddress'):
                pull_request.author = self.repository.contributors.create(
                    user['displayName'], user['emailAddress'],
                    bitbucket=user.get('name', None),
                )
            pull_request.head = data.get('fromRef', {}).get('displayId', pull_request.base)
            pull_request.base = data.get('toRef', {}).get('displayId', pull_request.base)
            pull_request.generator = self

            return pull_request

        def reviewers(self, pull_request):
            got = self.get(pull_request.number)
            pull_request._reviewers = got._reviewers if got else []
            pull_request._approvers = got._approvers if got else []
            return pull_request

        def comment(self, pull_request, content):
            response = requests.post(
                'https://{domain}/rest/api/1.0/projects/{project}/repos/{name}/pull-requests/{id}/comments'.format(
                    domain=self.repository.domain,
                    project=self.repository.project,
                    name=self.repository.name,
                    id=pull_request.number,
                ), json=dict(text=content),
            )
            if response.status_code // 100 != 2:
                sys.stderr.write("Failed to add comment to '{}'\n".format(pull_request))
                return None
            return pull_request

        def comments(self, pull_request):
            for action in reversed(self.repository.request('pull-requests/{}/activities'.format(pull_request.number)) or []):
                comment = action.get('comment', {})
                user = comment.get('author', {})
                if not comment or not user or not comment.get('text'):
                    continue

                author = self.repository.contributors.create(
                    user['displayName'], user['emailAddress'],
                    bitbucket=user.get('name', None),
                )
                yield PullRequest.Comment(
                    author=author,
                    timestamp=comment.get('updatedDate', comment.get('createdDate')) // 1000,
                    content=comment.get('text'),
                )

        def review(self, pull_request, comment=None, approve=None):
            failed = False
            if comment and not self.comment(pull_request, comment):
                failed = True

            user_slug = self.repository.whoami()
            if not user_slug:
                sys.stderr.write('Failed to determine Bitbucket username for current session\n')
                return None
            response = requests.put(
                'https://{domain}/rest/api/1.0/projects/{project}/repos/{name}/pull-requests/{id}/participants/{userSlug}'.format(
                    domain=self.repository.domain,
                    project=self.repository.project,
                    name=self.repository.name,
                    id=pull_request.number,
                    userSlug=user_slug,
                ), json=dict(
                    user=dict(name=user_slug),
                    approved=bool(approve),
                    status={
                        True: 'APPROVED',
                        False: 'NEEDS_WORK',
                    }.get(approve, 'UNAPPROVED'),
                ),
            )
            if response.status_code // 100 != 2:
                sys.stderr.write("Failed to {} '{}'\n".format(
                    'approve' if approve else 'reject',
                    pull_request,
                ))
                return None

            pull_request._approvers = None
            pull_request._blockers = None

            return None if failed else pull_request

        def statuses(self, pull_request):
            response = requests.get(
                'https://{domain}/rest/build-status/1.0/commits/{ref}'.format(
                    domain=self.repository.domain,
                    ref=pull_request.hash,
                ),
            )
            if response.status_code // 100 != 2:
                sys.stderr.write('Failed to fetch build-status for {}\n'.format(pull_request.hash[:Commit.HASH_LABEL_SIZE]))
                return
            for status in response.json().get('values') or []:
                yield PullRequest.Status(
                    name=status.get('name') or status.get('key'),
                    url=status.get('url'),
                    status=dict(
                        SUCCESSFUL='success',
                        INPROGRESS='pending',
                        FAILED='failure',
                    ).get(status.get('state'), 'error'),
                    description=status.get('description'),
                )


    @classmethod
    def is_webserver(cls, url):
        return True if cls.URL_RE.match(url) else False

    @classmethod
    def json_to_diff(cls, data):
        output = ''
        for diff in data.get('diffs', []):
            output += '--- a/{}\n'.format(diff['source']['toString'])
            output += '+++ b/{}\n'.format(diff['destination']['toString'])
            for hunk in diff.get('hunks', []):
                output += '@@ -{},{} +{},{} @@\n'.format(
                    hunk['sourceLine'], hunk['sourceSpan'],
                    hunk['destinationLine'], hunk['destinationSpan'],
                )
                for segment in hunk.get('segments', []):
                    leader = dict(
                        ADDED='+',
                        REMOVED='-',
                    ).get(segment['type'], ' ')
                    for line in segment.get('lines'):
                        output += '{}{}\n'.format(leader, line['line'])
        return output

    def __init__(self, url, dev_branches=None, prod_branches=None, contributors=None, id=None, classifier=None):
        match = self.URL_RE.match(url)
        if not match:
            raise self.Exception("'{}' is not a valid BitBucket project".format(url))
        self.domain = match.group('domain')
        self.project = match.group('project')
        self.name = match.group('repository')

        super(BitBucket, self).__init__(
            url,
            dev_branches=dev_branches, prod_branches=prod_branches,
            contributors=contributors,
            id=id or self.name.lower(),
            classifier=classifier,
        )

        self.pull_requests = self.PRGenerator(self)

    @decorators.Memoize()
    def whoami(self):
        url = 'https://{domain}/plugins/servlet/applinks/whoami'.format(domain=self.domain)
        response = requests.get(url)
        if response.status_code != 200:
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            return None
        return response.text.rstrip()

    def credentials(self, required=True, validate=False, save_in_keyring=None):
        return None, None

    @property
    def is_git(self):
        return True

    def checkout_url(self, ssh=False, http=False):
        if ssh and http:
            raise ValueError('Cannot specify request both a ssh and http URL')
        if http:
            return 'https://{}/scm/{}/{}.git'.format(self.domain, self.project, self.name)
        return 'git@{}/{}/{}.git'.format(self.domain, self.project, self.name)

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

    def tags(self):
        response = self.request('tags')
        if not response:
            return []
        return sorted([details.get('displayId') for details in response if details.get('displayId')])

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        if revision:
            raise self.Exception('Cannot map revisions to commits on BitBucket')

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
        branches = self._branches_for(commit_data['id'])
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
                identifier = self._distance(commit_data['id'])

        # Define identifiers on branches diverged from the default branch
        elif include_identifier and branch:
            if not identifier:
                identifier = self._distance(commit_data['id'], magnitude=256, condition=lambda val: self.default_branch not in val)
            branch_point = self._distance(commit_data['id']) - identifier

        # Check the commit log for a git-svn revision
        matches = self.GIT_SVN_REVISION.findall(commit_data['message'])
        revision = int(matches[-1].split('@')[0]) if matches else None

        # Comparing commits in different repositories involves comparing timestamps. This is problematic because it git,
        # it's possible for a series of commits to share a commit time. To handle this case, we assign each commit a
        # zero-indexed "order" within it's timestamp.
        timestamp = int(commit_data['committerTimestamp'] / 1000)
        order = 0
        while not identifier or order + 1 < identifier + (branch_point or 0):
            response = self.request('commits/{}'.format('{}~{}'.format(commit_data['id'], order + 1)))
            if not response:
                break
            parent_timestamp = int(response['committerTimestamp'] / 1000)
            if parent_timestamp != timestamp:
                break
            order += 1

        author = self.contributors.create(
            commit_data.get('committer', {}).get('displayName', None),
            commit_data.get('committer', {}).get('emailAddress', None),
            bitbucket=commit_data.get('committer', {}).get('name', None),
        )
        return Commit(
            repository_id=self.id,
            hash=commit_data['id'],
            revision=revision,
            branch_point=branch_point,
            identifier=identifier if include_identifier else None,
            branch=branch,
            timestamp=timestamp,
            order=order,
            author=author,
            message=commit_data['message'] if include_log else None,
        )

    def commits(self, begin=None, end=None, include_log=True, include_identifier=True):
        begin, end = self._commit_range(begin=begin, end=end, include_identifier=include_identifier, include_log=include_log)

        previous = end
        cached = [previous]
        while previous:
            response = self.request('commits/{}~1'.format(previous.hash))
            if not response:
                break
            branch_point = previous.branch_point
            identifier = previous.identifier
            if identifier is not None:
                identifier -= 1
            if not identifier:
                identifier = branch_point
                branch_point = None

            matches = self.GIT_SVN_REVISION.findall(response['message'])
            revision = int(matches[-1].split('@')[0]) if matches else None

            email_match = self.EMAIL_RE.match(response['author']['emailAddress'])

            previous = Commit(
                repository_id=self.id,
                hash=response['id'],
                revision=revision,
                branch=end.branch if identifier and branch_point else self.default_branch,
                identifier=identifier if include_identifier else None,
                branch_point=branch_point if include_identifier else None,
                timestamp=int(response['committerTimestamp'] / 1000),
                author=self.contributors.create(
                    response['author']['name'],
                    email_match.group('email') if email_match else None,
                ), order=0,
                message=response['message'] if include_log else None,
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
        if not isinstance(argument, string_utils.basestring):
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

    def diff(self, head='HEAD', base=None, include_log=False):
        if base:
            commits = list(self.commits(dict(argument=base), end=dict(argument=head), include_identifier=False))
        else:
            commits = [self.find(head, include_identifier=False), None]

        if not commits or not commits[0]:
            sys.stderr.write('Failed to find commits required to generate diff\n')
            return
        commits = commits[:-1]

        patch_count = 1
        for commit in commits:
            response = self.request('commits/{}/diff?contextLines={}'.format(commit.hash, self.DIFF_CONTEXT))
            if not response:
                sys.stderr.write('Failed to retrieve diff of {} with status code\n'.format(commit))
                return

            if include_log and commit.message:
                yield 'From {}'.format(commit.hash)
                yield 'From: {} <{}>'.format(commit.author.name, commit.author.email)
                yield 'Date: {}'.format(datetime.fromtimestamp(commit.timestamp).strftime('%a %b %d %H:%M:%S %Y'))
                if len(commits) <= 1:
                    subject = 'Subject: [PATCH]'
                else:
                    subject = 'Subject: [PATCH {}/{}]'.format(patch_count, len(commits))
                for line in commit.message.splitlines():
                    if subject is None:
                        yield line
                    elif line:
                        subject = '{} {}'.format(subject, line)
                    else:
                        yield subject
                        yield line
                        subject = None
                if subject:
                    yield subject
                yield '---'

            for line in self.json_to_diff(response).splitlines():
                yield line

            patch_count += 1

    def files_changed(self, argument=None):
        if not argument:
            raise ValueError('No argument provided')
        if not Commit.HASH_RE.match(argument):
            commit = self.find(argument, include_log=False, include_identifier=False)
            if not commit:
                raise ValueError("'{}' is not an argument recognized by git".format(argument))
            argument = commit.hash

        return [
            change.get('path', {}).get('toString')
            for change in self.request('commits/{}/changes'.format(argument))
            if change.get('path', {}).get('toString')
        ]
