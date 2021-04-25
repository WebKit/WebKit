# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

from webkitscmpy import remote, Commit


class Repository(object):

    def __init__(self, key, default_branch=None):
        self.key = key
        self.default_branch = default_branch or 'main'

    def commit(self, ref=None, revision=None, hash=None, identifier=None, fast=False):
        raise NotImplementedError()

    def url_for_commit(self, commit):
        return None

    def representations(self):
        return []


class StashRepository(Repository):

    def __init__(self, url, key=None, default_branch=None):
        self.remote = remote.BitBucket(url)
        super(StashRepository, self).__init__(
            key=key or self.remote.name,
            default_branch=default_branch or self.remote.default_branch,
        )

    def commit(self, ref=None, revision=None, hash=None, identifier=None, fast=False):
        if identifier:
            fast = False
        if ref:
            return self.remote.find(ref, include_identifier=not fast)
        return self.remote.commit(revision=revision, hash=hash, identifier=identifier, include_identifier=not fast)

    def url_for_commit(self, commit):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        if commit.identifier or commit.hash:
            return f'{self.remote.url}/commits/{commit.hash}'
        return None

    def representations(self):
        return ['hash', 'identifier']


class WebKitRepository(Repository):

    def __init__(self):
        self.svn = remote.Svn('https://svn.webkit.org/repository/webkit')
        self.github = remote.GitHub('https://github.com/WebKit/WebKit')
        super(WebKitRepository, self).__init__(
            key='webkit',
            default_branch=self.github.default_branch,
        )

    def commit(self, ref=None, revision=None, hash=None, identifier=None, fast=False):
        if identifier:
            fast = False

        if ref:
            if isinstance(ref, int) or ref.isdigit():
                result = self.svn.find('r{}'.format(ref), include_identifier=not fast)
            else:
                result = self.github.find(ref, include_identifier=not fast)
        else:
            result = (self.svn if revision and not hash else self.github).commit(
                revision=None if hash else revision,
                hash=hash,
                identifier=None if not hash and not revision else identifier,
                include_identifier=not fast,
            )

        if not fast and result.hash and result.identifier:
            result = self.github.commit(identifier=str(result))

        if result.branch in self.svn.DEFAULT_BRANCHES:
            result.branch = self.default_branch
        return result

    def url_for_commit(self, commit):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        if commit.identifier or commit.hash:
            return f'https://commits.webkit.org/{commit}'
        if commit.revision:
            return f'https://commits.webkit.org/r{commit.revision}'
        return None


    def representations(self):
        return ['identifier', 'hash', 'revision']
