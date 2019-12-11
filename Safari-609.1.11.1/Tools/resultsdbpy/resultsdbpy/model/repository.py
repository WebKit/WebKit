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

import datetime
import contextlib
import json
import re
import requests
import threading
import urllib
import xmltodict

from resultsdbpy.controller.commit import Commit
from xml.parsers.expat import ExpatError


class SCMException(RuntimeError):
    pass


class Repository(object):
    DEFAULT_BRANCH = 'master'

    def __init__(self, key):
        self.key = key

    def commit_for_id(self, id, branch=None):
        raise NotImplementedError()

    def url_for_commit(self, commit):
        return None


class HTTPRepository(Repository):

    def __init__(self, key, url, redis=None, username=None, password=None):
        super(HTTPRepository, self).__init__(key)
        self.url = url
        self._username = username
        self._password = password
        self._session = None
        self._session_depth = 0
        self.redis = redis  # In many cases, we can greatly reduce the number of requests made by caching requests in Redis.

    @contextlib.contextmanager
    def session(self):
        if not self._session:
            self._session = requests.Session()
            if self._username and self._password:
                self._session.auth = (self._username, self._password)
            self._session.mount(self.url, requests.adapters.HTTPAdapter(max_retries=5))
        self._session_depth += 1

        yield

        self._session_depth -= 1
        if self._session_depth <= 0:
            self._session.close()
            self._session = None

    def request(self, url, method='GET', **kwargs):
        with self.session():
            return self._session.request(method=method, url=url, **kwargs)

    def cache_result(self, key, function, ex=60 * 60 * 24 * 2):
        result = None
        if self.redis:
            result = self.redis.get(key)
        if result is None:
            result = function()
        else:
            result = result.decode('utf-8')
        if result and self.redis:
            self.redis.set(key, result, ex=ex)
        return result


class StashRepository(HTTPRepository):

    # Stash's timestamps are accurate to the millisecond.
    COMMIT_TIMESTAMP_CONVERSION = 1000

    def __init__(self, url, **kwargs):
        PROJECTS = '/projects'
        if len(url.split(PROJECTS)) != 2:
            raise Exception(f'{url} is not a Stash URL')
        self.base_url = url
        super(StashRepository, self).__init__(None, ('/rest/api/1.0' + PROJECTS).join(url.split(PROJECTS)), **kwargs)

        with self.session():
            try:
                stash_data = self.get()
            except ValueError:
                stash_data = {}
            self.key = stash_data.get('slug', None)
            if not self.key:
                raise Exception(f'{self.url} is not an Stash repository')

    def get(self, args=None, cache=True):
        url = self.url + ('' if not args else f'/{args}')

        def callback(url=url, obj=self):
            response = obj.request(url)
            if response.status_code == 200:
                return response.text
            return None

        result = self.cache_result(
            f'repository:{url}',
            callback,
        ) if cache else callback(url)
        if result:
            return json.loads(result)
        return {}

    def commit_for_id(self, id, branch=Repository.DEFAULT_BRANCH):
        try:
            with self.session():
                commit_data = self.get(f'commits/{id}')
                if not commit_data:
                    raise SCMException(f'Commit {id} does not exist on branch {branch}')
                timestamp = int(commit_data['committerTimestamp']) // self.COMMIT_TIMESTAMP_CONVERSION

                branch_filter = urllib.parse.quote(f'refs/heads/{branch}')
                commits_between_commit_and_branch_head = self.get(f"commits?since={commit_data['id']}&until={branch_filter}&limit=1", cache=False)
                if commits_between_commit_and_branch_head['isLastPage'] and commits_between_commit_and_branch_head['size'] == 0:
                    # We may have missed the case the commit in question is the HEAD of the branch
                    commits_at_branch_head = self.get(f'commits?until={branch_filter}&limit=20', cache=False)
                    if all([commit['id'] != commit_data['id'] for commit in commits_at_branch_head['values']]):
                        raise SCMException(f'{id} exists, but not on {branch}')

                # Ordering commits by timestamp in git is a bit problematic because multiple commits can share a timestamp.
                # Generally, if your parent shares your timestamp, your order needs to be incremented.
                order = 0
                loop_data = commit_data
                while len(loop_data['parents']) == 1 and int(loop_data['parents'][0]['committerTimestamp']) // self.COMMIT_TIMESTAMP_CONVERSION == timestamp:
                    order += 1
                    loop_data = self.get(f"commits/{loop_data['parents'][0]['id']}")

                return Commit(
                    repository_id=self.key,
                    id=commit_data['id'],
                    branch=branch,
                    timestamp=timestamp,
                    order=order,
                    committer=commit_data['committer']['emailAddress'],
                    message=commit_data['message'],
                )
        except ValueError:
            raise SCMException(f'Failed to connect to {self.url}')

    def url_for_commit(self, commit):
        return f'{self.base_url}/commits/{commit}'


class SVNRepository(HTTPRepository):

    DEFAULT_BRANCH = 'trunk'
    DAV_XML = """<?xml version="1.0" encoding="utf-8"?>
<propfind xmlns="DAV:">
<prop>
<resourcetype xmlns="DAV:"/>
<getcontentlength xmlns="DAV:"/>
<deadprop-count xmlns="http://subversion.tigris.org/xmlns/dav/"/>
<version-name xmlns="DAV:"/>
<creationdate xmlns="DAV:"/>
<creator-displayname xmlns="DAV:"/>
</prop></propfind>"""

    def get(self, branch=DEFAULT_BRANCH, revision=None):
        url = self.url
        if revision:
            url = url + f"!svn/rvr/{revision}/{branch if branch == self.DEFAULT_BRANCH else 'branches/' + branch}"

        def callback(url=url, obj=self, with_revision=True if revision else False):
            if with_revision:
                # Constructed this from WireShark and an svn info command, modify with caution!
                response = self.request(
                    method='PROPFIND',
                    url=url,
                    headers={
                        'Content-Type': 'text/xml',
                        'Accept-Encoding': 'gzip',
                        'Depth': '0',
                    },
                    data=obj.DAV_XML)
            else:
                response = self.request(self.url)

            if response.status_code in [200, 207]:
                return response.text
            return None

        result = self.cache_result(
            f'repository:{url}',
            callback,
        )
        if result:
            return xmltodict.parse(result)
        return {}

    def __init__(self, url, trac_url=None, **kwargs):
        super(SVNRepository, self).__init__(None, url, **kwargs)
        self.trac_url = trac_url
        with self.session():
            self.key = self.get().get('svn', {}).get('index').get('@base')
            if not self.key:
                raise Exception(f'{self.url} is not an SVN repository')

    def commit_for_id(self, id, branch=DEFAULT_BRANCH):
        with self.session():
            try:
                commit_data = self.get(branch=branch, revision=id)['D:multistatus']['D:response']['D:propstat'][0]['D:prop']
                if commit_data['lp1:version-name'] != str(id):
                    raise SCMException(f'Revision {id} does not exist on branch {branch}')

                # Of the form '2018-09-24T22:03:34.436217Z'
                timestamp = datetime.datetime.strptime(commit_data['lp1:creationdate'], '%Y-%m-%dT%H:%M:%S.%fZ')

                return Commit(
                    repository_id=self.key,
                    id=str(id),
                    branch=branch,
                    timestamp=timestamp,
                    order=0,
                    committer=commit_data['lp1:creator-displayname'],
                )
            except ExpatError:
                raise SCMException(f'Failed to connect to {self.url}')
            except KeyError:
                raise SCMException(f'Revision {id} does not exist on branch {branch}')

    def url_for_commit(self, commit):
        return f'{self.url}/!svn/bc/{commit}/'


class WebKitRepository(SVNRepository):
    CHANGELOGS = (
        'ChangeLog',
        'Source/bmalloc/ChangeLog',
        'Source/JavaScriptCore/ChangeLog',
        'Source/ThirdParty/ANGLE/ChangeLog',
        'Source/ThirdParty/ChangeLog',
        'Source/ThirdParty/libwebrtc/ChangeLog',
        'Source/WebCore/ChangeLog',
        'Source/WebCore/PAL/ChangeLog',
        'Source/WebCore/platform/gtk/po/ChangeLog',
        'Source/WebDriver/ChangeLog',
        'Source/WebInspectorUI/ChangeLog',
        'Source/WebKit/ChangeLog',
        'Source/WebKitLegacy/cf/ChangeLog',
        'Source/WebKitLegacy/ChangeLog',
        'Source/WebKitLegacy/ios/ChangeLog',
        'Source/WebKitLegacy/mac/ChangeLog',
        'Source/WebKitLegacy/win/ChangeLog',
        'Source/WTF/ChangeLog',
        'Tools/ChangeLog',
        'Examples/ChangeLog',
        'JSTests/ChangeLog',
        'LayoutTests/ChangeLog',
        'LayoutTests/imported/mozilla/ChangeLog',
        'LayoutTests/imported/w3c/ChangeLog',
        'PerformanceTests/ChangeLog',
        'PerformanceTests/SunSpider/ChangeLog',
        'WebDriverTests/ChangeLog',
        'WebKitLibraries/ChangeLog',
        'Websites/browserbench.org/ChangeLog',
        'Websites/bugs.webkit.org/ChangeLog',
        'Websites/perf.webkit.org/ChangeLog',
        'Websites/planet.webkit.org/ChangeLog',
        'Websites/webkit.org/ChangeLog',
        'Websites/webkit.org/specs/CSSVisualEffects/ChangeLog',
    )
    CHANGE_LOG_THREADS = 6
    FIRST_LINE_REGEX = re.compile(r'(?P<date>\d+-\d+-\d+)\s+(?P<name>\S.+\S)\s+<(?P<email>\S+)>')

    def __init__(self, **kwargs):
        super(WebKitRepository, self).__init__('https://svn.webkit.org/repository/webkit/', **kwargs)

    def commit_for_id(self, id, branch=SVNRepository.DEFAULT_BRANCH):
        with self.session():
            result = super(WebKitRepository, self).commit_for_id(id, branch=branch)

            # Find the last commit on the given branch so we can diff the changelogs
            previous_id = self.get(branch=branch, revision=int(id) - 1).get(
                'D:multistatus', {}).get('D:response', {}).get('D:propstat', [{}])[0].get('D:prop', {}).get('lp1:version-name', None)
            branch_url = f"{self.url}{branch if branch == self.DEFAULT_BRANCH else 'branches/' + branch}"

            changelogs = {}
            request_threads = []

            # Compare changelog from previous commit to the changelog from this commit
            def diff_changelogs(changelog_list):
                for changelog in changelog_list:
                    current_url = f'{branch_url}/{changelog}/?p={id}'
                    previous_url = f'{branch_url}/{changelog}/?p={previous_id}' if previous_id else None

                    current_response = self.request(current_url)
                    if current_response.status_code != 200:
                        continue
                    previous_response = self.request(previous_url) if previous_url else None
                    if previous_response and previous_response.status_code != 200:
                        continue

                    previous_response_length = 0
                    if previous_response:
                        previous_response_length = len(previous_response.text)
                    if len(current_response.text) == previous_response_length:
                        continue
                    if not previous_response_length or len(current_response.text) < previous_response_length:
                        changelogs[changelog] = current_response.text
                    else:
                        changelogs[changelog] = current_response.text[0:-len(previous_response.text)]

            # Because we're mostly blocked on network I/O, threads can speed this up ~4x
            shards = [[] for _ in range(self.CHANGE_LOG_THREADS)]
            index = 0
            for changelog in self.CHANGELOGS:
                shards[index].append(changelog)
                index += 1
                if index >= len(shards):
                    index = 0
            for shard in shards:
                request_threads.append(threading.Thread(
                    target=diff_changelogs,
                    kwargs=dict(changelog_list=shard),
                ))
                request_threads[-1].start()
            for thread in request_threads:
                thread.join()

            commit_message = []
            for changelog in self.CHANGELOGS:
                partial_message = changelogs.get(changelog, None)
                if not partial_message:
                    continue
                lines_in_message = partial_message.split('\n')

                # The first line of the commit message might contain the real committer, in the case
                # commit queue committed a change on the behalf of someone
                start_of_message_shard = 0
                match = self.FIRST_LINE_REGEX.match(lines_in_message[0])
                if match:
                    if match.group('email') != result.committer:
                        result.committer = match.group('email')
                    start_of_message_shard += 1
                    while not lines_in_message[start_of_message_shard]:
                        start_of_message_shard += 1

                # WebKit commit messages are often split across multiple changelogs with the first few lines duplicated.
                start_of_full_message = 0
                while start_of_message_shard < len(lines_in_message) and start_of_full_message < len(commit_message):
                    if lines_in_message[start_of_message_shard].startswith(' ' * 8):
                        line = lines_in_message[start_of_message_shard][8:]
                    else:
                        line = lines_in_message[start_of_message_shard]
                    if line != commit_message[start_of_full_message]:
                        break
                    start_of_message_shard += 1
                    start_of_full_message += 1

                for line in lines_in_message[start_of_message_shard:]:
                    commit_message.append(line[8:] if line.startswith(' ' * 8) else line)

            result.message = str('\n'.join(commit_message)) if commit_message else None

            return result

    def url_for_commit(self, commit):
        return f'https://trac.webkit.org/changeset/{commit}/{self.key}'
