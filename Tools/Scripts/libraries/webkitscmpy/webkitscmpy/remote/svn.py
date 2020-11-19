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

import bisect
import calendar
import fasteners
import json
import os
import re
import requests
import tempfile
import xmltodict

from datetime import datetime

from webkitcorepy import log, run, decorators
from webkitscmpy.remote.scm import Scm
from webkitscmpy import Commit, Contributor, Version


class Svn(Scm):
    URL_RE = re.compile(r'\Ahttps?://svn.(?P<host>\S+)/repository/\S+\Z')
    HISTORY_RE = re.compile(b'<D:version-name>(?P<revision>\d+)</D:version-name>')
    CACHE_VERSION = Version(1)

    @classmethod
    def is_webserver(cls, url):
        return True if cls.URL_RE.match(url) else False

    def __init__(self, url, dev_branches=None, prod_branches=None):
        if url[-1] != '/':
            url += '/'
        if not self.is_webserver(url):
            raise self.Exception("'{}' is not a valid SVN webserver".format(url))
        super(Svn, self).__init__(url, dev_branches=dev_branches, prod_branches=prod_branches)

        if os.path.exists(self._cache_path):
            try:
                with self._cache_lock(), open(self._cache_path) as file:
                    self._metadata_cache = json.load(file)
            except BaseException:
                self._metadata_cache = dict(version=str(self.CACHE_VERSION))
        else:
            self._metadata_cache = dict(version=str(self.CACHE_VERSION))

    @property
    def is_svn(self):
        return True

    @decorators.Memoize(timeout=60)
    def _latest(self):
        response = requests.request(
            method='OPTIONS',
            url=self.url,
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<?xml version="1.0" encoding="utf-8"?>\n'
            '<D:options xmlns:D="DAV:">\n'
            '    <D:activity-collection-set></D:activity-collection-set>\n'
            '</D:options>\n',
        )
        if response.status_code != 200:
            return None
        return int(response.headers.get('SVN-Youngest-Rev'))

    @decorators.Memoize(cached=False)
    def info(self, branch=None, revision=None, tag=None):
        if tag and branch:
            raise ValueError('Cannot specify both branch and tag')
        if tag and revision:
            raise ValueError('Cannot specify both branch and tag')

        if not revision:
            branch = branch or self.default_branch
            revision = self._latest()
            if not revision:
                return None

        if not revision:
            raise ValueError('Failed to find the latest revision')

        url = '{}!svn/rvr/{}'.format(self.url, revision)
        if branch and branch != self.default_branch and '/' not in branch:
            url = '{}/branches/{}'.format(url, branch)
        elif tag:
            url = '{}/tags/{}'.format(url, tag)
        elif branch:
            url = '{}/{}'.format(url, branch or self.default_branch)

        response = requests.request(
            method='PROPFIND',
            url=url,
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<propfind xmlns="DAV:">\n'
            '    <prop>\n'
            '        <resourcetype xmlns="DAV:"/>\n'
            '        <getcontentlength xmlns="DAV:"/>\n'
            '        <deadprop-count xmlns="http://subversion.tigris.org/xmlns/dav/"/>\n'
            '        <version-name xmlns="DAV:"/>\n'
            '        <creationdate xmlns="DAV:"/>\n'
            '        <creator-displayname xmlns="DAV:"/>\n'
            '    </prop>\n'
            '</propfind>\n',
        )
        if response.status_code not in [200, 207]:
            return {}

        response = xmltodict.parse(response.text)
        response = response.get('D:multistatus', response).get('D:response', [])
        if not response:
            return {}

        response = response[0] if isinstance(response, list) else response
        response = response['D:propstat'][0]['D:prop']

        return {
            'Last Changed Rev': response['lp1:version-name'],
            'Last Changed Author': response['lp1:creator-displayname'],
            'Last Changed Date': ' '.join(response['lp1:creationdate'].split('T')).split('.')[0],
            'Revision': revision,
        }

    @property
    def default_branch(self):
        return 'trunk'

    def list(self, category):
        revision = self._latest()
        if not revision:
            return []

        response = requests.request(
            method='PROPFIND',
            url='{}!svn/rvr/{}/{}'.format(self.url, revision, category),
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<?xml version="1.0" encoding="utf-8"?>\n'
                '<propfind xmlns="DAV:">\n'
                '    <prop><resourcetype xmlns="DAV:"/></prop>\n'
                '</propfind>\n',
        )
        if response.status_code not in [200, 207]:
            return []

        responses = xmltodict.parse(response.text)
        responses = responses.get('D:multistatus', responses).get('D:response', [])

        results = []
        for response in responses:
            candidate = response['D:href'].split('!svn/rvr/{}/{}/'.format(revision, category))[-1].rstrip('/')
            if not candidate:
                continue
            results.append(candidate)

        return results

    @property
    def branches(self):
        return [self.default_branch] + self.list('branches')

    @property
    def tags(self):
        return self.list('tags')

    @property
    @decorators.Memoize()
    def _cache_path(self):
        return os.path.join(tempfile.gettempdir(), 'svn.{}'.format(self.URL_RE.match(self.url).group('host')), 'webkitscmpy-cache.json')

    def _cache_lock(self):
        return fasteners.InterProcessLock(os.path.join(os.path.dirname(self._cache_path), 'cache.lock'))

    def _cache_revisions(self, branch=None):
        branch = branch or self.default_branch
        is_default_branch = branch == self.default_branch
        if branch not in self._metadata_cache:
            self._metadata_cache[branch] = [0] if is_default_branch else []
        pos = len(self._metadata_cache[branch])

        # If we aren't on the default branch, we will need the default branch to determine when
        # our  branch  intersects with the default branch.
        if not is_default_branch:
            self._cache_revisions(branch=self.default_branch)

        did_warn = False
        count = 0

        latest = self._latest()
        with requests.request(
            method='REPORT',
            url='{}!svn/rvr/{}/{}'.format(
                self.url,
                latest,
                branch if is_default_branch or '/' in branch else 'branches/{}'.format(branch),
            ), stream=True,
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<S:log-report xmlns:S="svn:">\n'
                '<S:start-revision>{revision}</S:start-revision>\n'
                '<S:end-revision>0</S:end-revision>\n'
                '<S:revprop>svn:date</S:revprop>\n'
                '</S:log-report>\n'.format(revision=latest),
        ) as response:
            if response.status_code != 200:
                raise self.Exception("Failed to construct branch history for '{}'".format(branch))

            for line in response.iter_lines():
                match = self.HISTORY_RE.match(line)
                if not match:
                    continue

                if not did_warn:
                    count += 1
                    if count > 1000:
                        self.log('Caching commit data for {}, this will take a few minutes...'.format(branch))
                        did_warn = True

                revision = int(match.group('revision'))
                if pos > 0 and self._metadata_cache[branch][pos - 1] == revision:
                    break
                if not is_default_branch:
                    if revision in self._metadata_cache[self.default_branch]:
                        self._metadata_cache[branch].insert(pos, revision)
                        break
                self._metadata_cache[branch].insert(pos, revision)

        if self._metadata_cache[self.default_branch][0] == [0]:
            self._metadata_cache['identifier'] = len(self._metadata_cache[branch])

        try:
            if not os.path.isdir(os.path.dirname(self._cache_path)):
                os.makedirs(os.path.dirname(self._cache_path))
            with self._cache_lock(), open(self._cache_path, 'w') as file:
                json.dump(self._metadata_cache, file, indent=4)
        except (IOError, OSError):
            self.log("Failed to write SVN cache to '{}'".format(self._cache_path))

        return self._metadata_cache[branch]

    def _branch_for(self, revision):
        response = requests.request(
            method='REPORT',
            url='{}!svn/rvr/{}'.format(self.url, revision),
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<S:log-report xmlns:S="svn:">\n'
                '<S:start-revision>{revision}</S:start-revision>\n'
                '<S:end-revision>{revision}</S:end-revision>\n'
                '<S:limit>1</S:limit>\n'
                '<S:discover-changed-paths/>\n'
                '</S:log-report>\n'.format(revision=revision),
        )

        # If we didn't get a valid answer from the remote, but we found a matching candidate, we return that.
        # This is a bit risky because there is a chance the branch we have cached is not the canonical branch
        # for a revision, but this is pretty unlikely because it would require the n + 1 level branch to be cached
        # but not the n level branch.
        if response.status_code != 200:
            raise self.Exception("Failed to retrieve branch for '{}'".format(revision))

        partial = None
        items = xmltodict.parse(response.text)['S:log-report']['S:log-item']
        for group in (items.get('S:modified-path', []), items.get('S:added-path', [])):
            for item in group if isinstance(group, list) else [group]:
                if not partial:
                    partial = item['#text']
                while not item['#text'].startswith(partial):
                    partial = partial[:-1]

        candidate = partial.split('/')[2 if partial.startswith('/branches') else 1]

        # Tags are a unique case for SVN, because they're treated as branches in native SVN
        if candidate == 'tags':
            return partial[1:].rstrip('/')
        return candidate

    def _commit_count(self, revision=None, branch=None):
        branch = branch or self.default_branch

        if revision:
            if revision not in self._metadata_cache[branch]:
                raise self.Exception("Failed to find '{}' on '{}'".format(revision, branch))
            return bisect.bisect_left(self._metadata_cache[branch], int(revision))
        if branch == self.default_branch:
            return len(self._metadata_cache[branch])
        return self._commit_count(revision=self._metadata_cache[branch][0], branch=self.default_branch)

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None):
        if hash:
            raise ValueError('SVN does not support Git hashes')

        parsed_branch_point = None
        if identifier is not None:
            if revision:
                raise ValueError('Cannot define both revision and identifier')
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

            if branch == self.default_branch and parsed_branch_point:
                raise self.Exception('Cannot provide a branch point for a commit on the default branch')

            if not self._metadata_cache.get(branch, []) or identifier >= len(self._metadata_cache.get(branch, [])):
                if branch != self.default_branch:
                    self._cache_revisions(branch=self.default_branch)
                self._cache_revisions(branch=branch)
            if identifier > len(self._metadata_cache.get(branch, [])):
                raise self.Exception('Identifier {} cannot be found on the specified branch in the current checkout'.format(identifier))

            if identifier <= 0:
                if branch == self.default_branch:
                    raise self.Exception('Illegal negative identifier on the default branch')
                identifier = self._commit_count(branch=branch) + identifier
                if identifier < 0:
                    raise self.Exception('Identifier does not exist on the specified branch')

                branch = self.default_branch

            revision = self._metadata_cache[branch][identifier]
            info = self.info(cached=True, branch=branch, revision=revision)
            branch = self._branch_for(revision)
            if not self._metadata_cache.get(branch, []) or identifier >= len(self._metadata_cache.get(branch, [])):
                self._cache_revisions(branch=branch)

        elif revision:
            if branch:
                raise ValueError('Cannot define both branch and revision')
            if tag:
                raise ValueError('Cannot define both tag and revision')
            revision = Commit._parse_revision(revision, do_assert=True)
            branch = self._branch_for(revision)
            info = self.info(cached=True, branch=branch, revision=revision)

        else:
            if branch and tag:
                raise ValueError('Cannot define both branch and tag')

            branch = None if tag else branch or self.default_branch
            info = self.info(tag=tag) if tag else self.info(branch=branch)
            if not info:
                raise self.Exception("'{}' is not a recognized {}".format(
                    tag or branch,
                    'tag' if tag else 'branch',
                ))
            revision = int(info['Last Changed Rev'])
            if branch != self.default_branch:
                branch = self._branch_for(revision)

        date = datetime.strptime(info['Last Changed Date'], '%Y-%m-%d %H:%M:%S')

        if not identifier:
            if branch != self.default_branch and revision > self._metadata_cache.get(self.default_branch, [0])[-1]:
                self._cache_revisions(branch=self.default_branch)
            if revision not in self._metadata_cache.get(branch, []):
                self._cache_revisions(branch=branch)
            identifier = self._commit_count(revision=revision, branch=branch)

        branch_point = None if branch == self.default_branch else self._commit_count(branch=branch)
        if branch_point and parsed_branch_point and branch_point != parsed_branch_point:
            raise ValueError("Provided 'branch_point' does not match branch point of specified branch")

        response = requests.request(
            method='REPORT',
            url='{}!svn/rvr/{}'.format(self.url, revision),
            headers={
                'Content-Type': 'text/xml',
                'Accept-Encoding': 'gzip',
                'DEPTH': '1',
            }, data='<S:log-report xmlns:S="svn:">\n'
                    '<S:start-revision>{revision}</S:start-revision>\n'
                    '<S:end-revision>{revision}</S:end-revision>\n'
                    '<S:limit>1</S:limit>\n'
                    '</S:log-report>\n'.format(revision=revision),
        )

        if response.status_code == 200:
            response = xmltodict.parse(response.text)
            response = response.get('S:log-report', {}).get('S:log-item')

            name = response.get('D:creator-displayname')
            message = response.get('D:comment', None)
            if not name:
                raise self.Exception('Failed to find creator name')

        else:
            self.log('Failed to connect to remote, cannot compute commit message')
            message = None
            name = info['Last Changed Author']

        author = Contributor.by_email.get(
            name,
            Contributor.by_name.get(
                name,
                Contributor(name=name, emails=[name] if '@' in name else []),
            ),
        )

        return Commit(
            revision=int(revision),
            branch=branch,
            identifier=identifier,
            branch_point=branch_point,
            timestamp=int(calendar.timegm(date.timetuple())),
            author=author,
            message=message,
        )
