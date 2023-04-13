# Copyright (C) 2023 Apple Inc. All rights reserved.
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

from webkitcorepy import mocks


class PartialProxy(mocks.ContextStack):
    top = None

    def __init__(self, hosts, http, https):
        super(PartialProxy, self).__init__(cls=PartialProxy)
        self.hosts = hosts
        self.http = http
        self.https = https

        self._temp_patches = None

    def __enter__(self):
        # Allow requests and mock to be managed via autoinstall
        import requests
        from mock import patch

        this = self

        class Session(requests.Session):
            def request(self, method, url, **kwargs):
                split = url.split('/')
                protocol = split[0]
                host = split[2] if len(split) >= 3 else None

                if host and protocol in ('http:', 'https:'):
                    current = PartialProxy.top
                    while current:
                        if host in current.hosts:
                            kwargs['proxies'] = dict(
                                http=current.http,
                                https=current.https,
                            )
                            break
                        current = current.previous

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
        return super(PartialProxy, self).__enter__()

    def __exit__(self, *args, **kwargs):
        super(PartialProxy, self).__exit__(*args, **kwargs)
        for patch in reversed(self._temp_patches):
            patch.__exit__(*args, **kwargs)
        self._temp_patches = None
