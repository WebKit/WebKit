# Copyright (C) 2019 Apple Inc. All rights reserved.
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

import contextlib
import json
import re
import urllib
import xmltodict

from collections import defaultdict
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.repository import StashRepository, SVNRepository, WebKitRepository


class MockRequest(object):

    def __init__(self, text='', status_code=200):
        self.text = text
        self.status_code = status_code


class MockStashRepository(StashRepository):

    COMMIT_RE = re.compile(r'commits/(?P<hash>[0-9a-f]+)')
    COMMIT_FOR_BRANCH_RE = re.compile(r'commits\?since\=(?P<hash>[0-9a-f]+)\&until\=(?P<branch>[^&]+)\&limit\=1')
    COMMIT_AT_HEAD_RE = re.compile(r'commits\?until\=(?P<branch>[^&]+)\&limit\=\d+')
    DOES_NOT_EXIST_RESULT = {'errors': [{
        'context': None,
        'message': 'Commit \'?\' does not exist in repository.',
        'exceptionName': 'com.atlassian.bitbucket.commit.NoSuchCommitException',
    }]}

    @staticmethod
    def safari(redis=None):
        result = MockStashRepository('https://fake-stash-instance.apple.com/projects/BROWSER/repos/safari', name='safari', redis=redis)
        result.add_commit(Commit(
            repository_id=result.name, branch='master', id='bb6bda5f44dd24d0b54539b8ff6e8c17f519249a',
            timestamp=1537810281, order=0,
            committer='person1@apple.com',
            message='Change 1 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='master', id='336610a84fdcf14ddcf1db65075af95480516fda',
            timestamp=1537809818, order=0,
            committer='person2@apple.com',
            message='Change 2 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='master', id='336610a40c3fecb728871e12ca31482ca715b383',
            timestamp=1537566386, order=0,
            committer='person3@apple.com',
            message='<rdar://problem/99999999> Change 3 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='master', id='e64810a40c3fecb728871e12ca31482ca715b383',
            timestamp=1537550685, order=0,
            committer='person4@apple.com',
            message=u'Change 4 \u2014 (Part 1) description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='master', id='7be4084258a452e8fe22f36287c5b321e9c8249b',
            timestamp=1537550685, order=1,
            committer='person4@apple.com',
            message=u'Change 4 \u2014 (Part 2) description.\nReviewed by person.',
        ))

        result.add_commit(Commit(
            repository_id=result.name, branch='safari-606-branch', id='79256c32a855ac8612112279008334d90e901c55',
            timestamp=1537897367, order=0,
            committer='person5@apple.com',
            message='Change 5 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='safari-606-branch', id='d85222d9407fdbbf47406509400a9cecb73ac6de',
            timestamp=1537563383, order=0,
            committer='person6@apple.com',
            message='Change 6 description.',
        ))
        return result

    def __init__(self, url, name, **kwargs):
        self.name = name
        self.commits = defaultdict(list)

        super(MockStashRepository, self).__init__(url, **kwargs)

    @contextlib.contextmanager
    def session(self):
        self._session_depth += 1
        yield
        self._session_depth -= 1

    def add_commit(self, commit):
        copied_commit = Commit.from_json(commit.to_json())
        copied_commit.repository_id = self.name
        self.commits[copied_commit.branch].append(copied_commit)
        self.commits[copied_commit.branch] = sorted(self.commits[copied_commit.branch])

    def request(self, url, method='GET', **kwargs):
        if not url.startswith(self.url) or method != 'GET':
            return MockRequest(status_code=404)

        path = url[len(self.url) + 1:]
        if not path:
            return MockRequest(json.dumps({'slug': self.name}))

        match = self.COMMIT_RE.match(path)
        if match:
            index = -1
            branch = None
            for key, commits in self.commits.items():
                for i in range(len(commits)):
                    if commits[i].id.startswith(match.group('hash')):
                        if index != -1:
                            return MockRequest(json.dumps(self.DOES_NOT_EXIST_RESULT), status_code=401)
                        index = i
                        branch = key
            if index == -1 or not branch:
                return MockRequest(json.dumps(self.DOES_NOT_EXIST_RESULT), status_code=401)
            return MockRequest(json.dumps({
                'id': self.commits[branch][index].id,
                'committer': {'emailAddress': self.commits[branch][index].committer},
                'committerTimestamp': self.commits[branch][index].timestamp_as_epoch() * self.COMMIT_TIMESTAMP_CONVERSION,
                'message': self.commits[branch][index].message,
                'parents': [] if index == 0 else [{
                    'id': self.commits[branch][index - 1].id,
                    'committerTimestamp': self.commits[branch][index - 1].timestamp_as_epoch() * self.COMMIT_TIMESTAMP_CONVERSION,
                }],
            }))

        match = self.COMMIT_FOR_BRANCH_RE.match(path)
        if match:
            branch = urllib.parse.unquote(match.group('branch'))[len('refs/heads') + 1:]
            has_found = False
            if self.commits.get(branch):
                for commit in self.commits[branch][:-1]:
                    if commit.id.startswith(match.group('hash')):
                        has_found = True
                        break
            return MockRequest(json.dumps({'size': 1 if has_found else 0, 'isLastPage': True}))

        match = self.COMMIT_AT_HEAD_RE.match(path)
        if match:
            branch = match.group('branch').replace('%2F', '/')[len('refs/heads') + 1:]
            if not self.commits.get(branch, []):
                return MockRequest(json.dumps(self.DOES_NOT_EXIST_RESULT), status_code=401)
            return MockRequest(json.dumps({'values': [{'id': self.commits[branch][-1].id}], 'size': 1, 'isLastPage': True}))

        return MockRequest(status_code=404)


# Uses multiple inheritance instead of duplicating the functions in this class.
class _SVNMock(object):
    SVN_URL_RE = re.compile(r'\!svn/rvr/(?P<revision>[0-9]+)/(?P<branch>.*)')

    def __init__(self, name, url=None):
        self.commits = defaultdict(list)
        self.name = name

        # Should be overriden by the SVNRepository class
        self.url = url if url else ''
        self._session_depth = 0

    @contextlib.contextmanager
    def session(self):
        self._session_depth += 1
        yield
        self._session_depth -= 1

    def add_commit(self, commit):
        copied_commit = Commit.from_json(commit.to_json())
        copied_commit.repository_id = self.name
        self.commits[copied_commit.branch].append(copied_commit)
        self.commits[copied_commit.branch] = sorted(self.commits[copied_commit.branch])

    def request(self, url, method='GET', **kwargs):
        if not url.startswith(self.url):
            return MockRequest(status_code=404)

        if method == 'GET' and url == self.url:
            return MockRequest(text=xmltodict.unparse({'svn': {'index': {'@base': self.name}}}), status_code=200)
        if method != 'PROPFIND':
            return MockRequest(status_code=404)

        path = url[len(self.url):]
        match = self.SVN_URL_RE.match(path)
        if not match:
            return MockRequest(status_code=404)
        revision = match.group('revision')
        branch = match.group('branch')
        if branch != 'trunk':
            branch = branch[len('branches/'):]

        if branch not in self.commits:
            return MockRequest(status_code=404)

        for commit in self.commits[branch]:
            if commit.id == revision:
                return MockRequest(
                    text=xmltodict.unparse({
                        'D:multistatus': {'D:response': {'D:propstat': [
                            {'D:prop': {
                                'lp1:version-name': commit.id,
                                'lp1:creationdate': commit.timestamp.strftime('%Y-%m-%dT%H:%M:%S.%fZ'),
                                'lp1:creator-displayname': commit.committer,
                            }},
                            {'D:status': 'HTTP/1.1 200 OK'},
                        ]}},
                    }),
                    status_code=207,
                )
        return MockRequest(status_code=404)


class MockSVNRepository(_SVNMock, SVNRepository):

    @staticmethod
    def webkit(redis=None):
        result = MockWebKitRepository(redis=redis)
        result.add_commit(Commit(
            repository_id=result.name, branch='trunk', id=236544,
            timestamp=1538052408, order=0,
            committer='person1@webkit.org',
            message='Change 1 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='trunk', id=236543,
            timestamp=1538050458, order=0,
            committer='person2@webkit.org',
            message='Change 2 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='trunk', id=236542,
            timestamp=1538049108, order=0,
            committer='person3@webkit.org',
            message='Change 3 description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='trunk', id=236541,
            timestamp=1538041792, order=0,
            committer='person4@webkit.org',
            message='Change 4 (Part 2) description.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='trunk', id=236540,
            timestamp=1538029479, order=0,
            committer='person4@webkit.org',
            message='Change 4 (Part 1) description.',
        ))

        result.add_commit(Commit(
            repository_id=result.name, branch='safari-606-branch', id=236335,
            timestamp=1538029480, order=0,
            committer='integrator@webkit.org',
            message='Integration 1.',
        ))
        result.add_commit(Commit(
            repository_id=result.name, branch='safari-606-branch', id=236334,
            timestamp=1538029479, order=0,
            committer='integrator@webkit.org',
            message='Integration 2.',
        ))
        return result

    def __init__(self, url, name, **kwargs):
        _SVNMock.__init__(self, name=name, url=url)
        SVNRepository.__init__(self, url=url, **kwargs)


class MockWebKitRepository(_SVNMock, WebKitRepository):

    CHANGELOG_URL_RE = re.compile(r'(?P<branch>trunk|branches\/.+)\/(?P<path>.+)\/\?p=(?P<revision>[0-9]+)')

    def __init__(self, **kwargs):
        _SVNMock.__init__(self, name='webkit')
        WebKitRepository.__init__(self, **kwargs)

    def request(self, url, method='GET', **kwargs):
        if not url.startswith(self.url):
            return MockRequest(status_code=404)

        # The default behavior is to fallback to _SVNMock
        if method != 'GET':
            return _SVNMock.request(self, url, method=method, **kwargs)

        path = url[len(self.url):]
        match = self.CHANGELOG_URL_RE.match(path)
        if not match:
            return _SVNMock.request(self, url, method=method, **kwargs)

        if match.group('path') not in WebKitRepository.CHANGELOGS:
            return MockRequest(status_code=404)
        if match.group('path') not in ('ChangeLog', 'Tools/ChangeLog'):
            return MockRequest(status_code=200)

        revision = match.group('revision')
        branch = match.group('branch')
        if branch != 'trunk':
            branch = branch[len('branches/'):]

        changelog_for_commit = ''
        for commit in self.commits[branch]:
            changelog_for_commit = f"""{commit.timestamp.strftime('%Y-%m-%d')}  Jon Committer  <{commit.committer}>

        {commit.message}
""" + changelog_for_commit

            if commit.id == revision:
                return MockRequest(text=changelog_for_commit, status_code=200)

        return MockRequest(status_code=404)
