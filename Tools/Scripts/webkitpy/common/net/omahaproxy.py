# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This is the client to query http://omahaproxy.appspot.com/ to retrieve
# chrome versions associated with WebKit commits.

from webkitpy.common.net.networktransaction import NetworkTransaction
from webkitpy.common.config import urls

import json
import urllib2


class OmahaProxy(object):
    default_url = urls.omahaproxy_url

    chrome_platforms = {"linux": "Linux",
                        "win": "Windows",
                        "mac": "Mac",
                        "cros": "Chrome OS",
                        "cf": "Chrome Frame"}
    chrome_channels = ["canary", "dev", "beta", "stable"]

    def __init__(self, url=default_url, browser=None):
        self._chrome_channels = set(self.chrome_channels)
        self.set_url(url)
        from webkitpy.thirdparty.autoinstalled.mechanize import Browser
        self._browser = browser or Browser()

    def set_url(self, url):
        self.url = url

    def _json_url(self):
        return "%s/all.json" % self.url

    def _get_json(self):
        return NetworkTransaction().run(lambda: urllib2.urlopen(self._json_url()).read())

    def get_revisions(self):
        revisions_json = json.loads(self._get_json())
        revisions = []
        for platform in revisions_json:
            for version in platform["versions"]:
                try:
                    row = {
                        "commit": int(version["base_webkit_revision"]),
                        "channel": version["channel"],
                        "platform": self.chrome_platforms[platform["os"]],
                        "date": version["date"],
                    }
                    assert(version["channel"] in self._chrome_channels)
                    revisions.append(row)
                except ValueError:
                    next
        return revisions
