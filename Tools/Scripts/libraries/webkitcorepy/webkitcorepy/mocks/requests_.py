# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json

from webkitcorepy import string_utils
from webkitcorepy.mocks import ContextStack


class Response(object):
    @staticmethod
    def fromText(data, url=None, headers=None):
        assert isinstance(data, str)
        return Response(text=data, url=url, headers=headers)

    @staticmethod
    def fromJson(data, url=None, headers=None, status_code=None):
        assert isinstance(data, list) or isinstance(data, dict)

        headers = headers or {}
        if 'Content-Type' not in headers:
            headers['Content-Type'] = 'text/json'

        return Response(text=json.dumps(data), url=url, headers=headers, status_code=status_code)

    @staticmethod
    def create404(url=None, headers=None):
        return Response(status_code=404, url=url, headers=headers)

    def __init__(self, status_code=None, text=None, content=None, url=None, headers=None):
        if status_code is not None:
            self.status_code = status_code
        elif text is not None:
            self.status_code = 200
        else:
            self.status_code = 204  # No content

        if text and content:
            raise ValueError("Cannot define both 'text' and 'content'")
        elif text:
            self.content = string_utils.encode(text)
        else:
            self.content = content or b''

        self.url = url
        self.headers = headers or {}

        if 'Content-Type' not in self.headers:
            self.headers['Content-Type'] = 'text'
        if 'Content-Length' not in self.headers:
            self.headers['Content-Length'] = len(self.content) if self.content else 0

    @property
    def text(self):
        return string_utils.decode(self.content)

    def json(self):
        return json.loads(self.text)

    def iter_content(self, chunk_size=4096):
        for i in range(0, len(self.text), chunk_size):
            yield self.text[i:i + chunk_size]

    def iter_lines(self):
        for line in self.text.splitlines() if self.text else []:
            yield string_utils.encode(line)

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        pass


class Requests(ContextStack):
    top = None

    def __init__(self, *hosts, **kwargs):
        super(Requests, self).__init__(cls=Requests)
        self.hosts = hosts
        self._temp_patches = None
        self._responses = kwargs

    def request(self, method, url, **kwargs):
        stripped_url = url.split('://')[-1]
        candidate = self._responses.get('/'.join(stripped_url.split('/')[1:]))
        if isinstance(candidate, Response):
            return candidate
        if candidate:
            return candidate(method, url, **kwargs)
        return Response.create404(url)

    def __enter__(self):
        # Allow requests and mock to be managed via autoinstall
        import requests
        from mock import patch

        this = self

        class Session(requests.Session):
            def request(self, method, url, **kwargs):
                for host in this.hosts:
                    for candidate in ['https://{}'.format(host), 'http://{}'.format(host)]:
                        if url == candidate:
                            return this.request(method, url, **kwargs)
                        if url.startswith('{}/'.format(candidate)) or url.startswith('{}?'.format(candidate)):
                            return this.request(method, url, **kwargs)
                return super(Session, self).request(method, url, **kwargs)

        self._temp_patches = [
            patch('requests.Session', new=Session),
            patch('requests.request', new=lambda *args, **kwargs: Session().request(*args, **kwargs)),
            patch('requests.get', new=lambda *args, **kwargs: Session().request('GET', *args, **kwargs)),
            patch('requests.head', new=lambda *args, **kwargs: Session().request('HEAD', *args, **kwargs)),
            patch('requests.post', new=lambda *args, **kwargs: Session().request('POST', *args, **kwargs)),
            patch('requests.put', new=lambda *args, **kwargs: Session().request('PUT', *args, **kwargs)),
            patch('requests.patch', new=lambda *args, **kwargs: Session().request('PATCH', *args, **kwargs)),
            patch('requests.delete', new=lambda *args, **kwargs: Session().request('DELETE', *args, **kwargs)),
        ]
        for patch in self._temp_patches:
            patch.__enter__()
        return super(Requests, self).__enter__()

    def __exit__(self, *args, **kwargs):
        super(Requests, self).__exit__(*args, **kwargs)
        for patch in reversed(self._temp_patches):
            patch.__exit__(*args, **kwargs)
        self._temp_patches = None
