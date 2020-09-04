# Copyright (C) 2020 Apple Inc. All rights reserved.
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


class Response(object):
    @staticmethod
    def fromText(data, url=None):
        assert isinstance(data, str)
        return Response(text=data, url=url)

    @staticmethod
    def fromJson(data, url=None):
        assert isinstance(data, list) or isinstance(data, dict)
        return Response(text=json.dumps(data), url=url)

    @staticmethod
    def create404(url=None):
        return Response(status_code=404, url=url)

    def __init__(self, status_code=None, text=None, url=None):
        if status_code is not None:
            self.status_code = status_code
        elif text is not None:
            self.status_code = 200
        else:
            self.status_code = 204  # No content
        self.text = text
        self.url = url

    def json(self):
        return json.loads(self.text)

    def iter_content(self, chunk_size=4096):
        for i in range(0, len(self.text), chunk_size):
            yield self.text[i:i + chunk_size]

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        pass
