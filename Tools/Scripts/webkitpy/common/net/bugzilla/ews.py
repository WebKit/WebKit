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

import requests

try:
    from urllib.parse import urljoin, urlsplit, urlunsplit
except ImportError:
    from urlparse import urljoin, urlsplit, urlunsplit


class EWS(object):
    def __init__(self, buildbot_url):
        self.buildbot_url = buildbot_url
        self._api_v2_url = urljoin(buildbot_url, "api/v2/")
        self._builders = {}
        self._session = requests.Session()

    @classmethod
    def from_webapp_url(cls, url):
        split = urlsplit(url)
        split.query = ""
        split.fragment = ""
        return cls(urlunsplit(split))

    def builders(self, invalidate_cache=False):
        if not invalidate_cache and self.buildbot_url in self._builders:
            return self._builders

        r = self._session.get(urljoin(self._api_v2_url, "builders"))
        r.raise_for_status()
        builders = r.json()["builders"]
        builders_by_builderid = {builder["builderid"]: builder for builder in builders}
        assert len(builders) == len(builders_by_builderid)
        self._builders = builders_by_builderid

        return self._builders

    def steps_from_build_number(self, builderid_or_buildername, build_number):
        r = self._session.get(
            urljoin(
                self._api_v2_url,
                "builders/{}/builds/{}/steps".format(
                    builderid_or_buildername, build_number
                ),
            )
        )
        r.raise_for_status()
        return r.json()["steps"]
