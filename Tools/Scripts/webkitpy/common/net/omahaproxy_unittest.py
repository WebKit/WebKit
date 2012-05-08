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

# Unit test for omahaproxy.py

import unittest

from webkitpy.common.net.omahaproxy import OmahaProxy


class MockOmahaProxy(OmahaProxy):
    def __init__(self, json):
        self._get_json = lambda: json
        OmahaProxy.__init__(self)


class OmahaProxyTest(unittest.TestCase):
    example_omahaproxy_json = """[
        {"os": "win",
         "versions": [
                {"base_webkit_revision": "116185",
                 "v8_ver": "3.10.8.1",
                 "wk_ver": "536.11",
                 "base_trunk_revision": 135598,
                 "prev_version": "20.0.1128.0",
                 "version": "20.0.1129.0",
                 "date": "05\/07\/12",
                 "prev_date": "05\/06\/12",
                 "true_branch": "trunk",
                 "channel": "canary",
                 "branch_revision": "NA"},
                {"base_webkit_revision": "115687",
                 "v8_ver": "3.10.6.0",
                 "wk_ver": "536.10",
                 "base_trunk_revision": 134666,
                 "prev_version": "20.0.1123.1",
                 "version": "20.0.1123.4",
                 "date": "05\/04\/12",
                 "prev_date": "05\/02\/12",
                 "true_branch": "1123",
                 "channel": "dev",
                 "branch_revision": 135092}]},
        {"os": "linux",
         "versions": [
                {"base_webkit_revision": "115688",
                 "v8_ver": "3.10.6.0",
                 "wk_ver": "536.10",
                 "base_trunk_revision": 134666,
                 "prev_version": "20.0.1123.2",
                 "version": "20.0.1123.4",
                 "date": "05\/04\/12",
                 "prev_date": "05\/02\/12",
                 "true_branch": "1123",
                 "channel": "dev",
                 "branch_revision": 135092},
                {"base_webkit_revision": "112327",
                 "v8_ver": "3.9.24.17",
                 "wk_ver": "536.5",
                 "base_trunk_revision": 129376,
                 "prev_version": "19.0.1084.36",
                 "version": "19.0.1084.41",
                 "date": "05\/03\/12",
                 "prev_date": "04\/25\/12",
                 "true_branch": "1084",
                 "channel": "beta",
                 "branch_revision": 134854},
                {"base_webkit_revision": "*",
                 "v8_ver": "3.9.24.17",
                 "wk_ver": "536.5",
                 "base_trunk_revision": 129376,
                 "prev_version": "19.0.1084.36",
                 "version": "19.0.1084.41",
                 "date": "05\/03\/12",
                 "prev_date": "04\/25\/12",
                 "true_branch": "1084",
                 "channel": "release",
                 "branch_revision": 134854}]}]"""

    expected_revisions = [
        {"commit": 116185, "channel": "canary", "platform": "Windows", "date": "05/07/12"},
        {"commit": 115687, "channel": "dev", "platform": "Windows", "date": "05/04/12"},
        {"commit": 115688, "channel": "dev", "platform": "Linux", "date": "05/04/12"},
        {"commit": 112327, "channel": "beta", "platform": "Linux", "date": "05/03/12"},
    ]

    def test_get_revisions(self):
        omahaproxy = MockOmahaProxy(self.example_omahaproxy_json)
        revisions = omahaproxy.get_revisions()
        self.assertEqual(len(revisions), 4)
        for revision in revisions:
            self.assertTrue("commit" in revision)
            self.assertTrue("channel" in revision)
            self.assertTrue("platform" in revision)
            self.assertTrue("date" in revision)
            self.assertEqual(len(revision.keys()), 4)
        self.assertEqual(revisions, self.expected_revisions)

if __name__ == '__main__':
    unittest.main()
