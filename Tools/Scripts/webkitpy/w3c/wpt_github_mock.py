# Copyright 2017 The Chromium Authors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

from webkitpy.w3c.wpt_github import WPTGitHub


class MockWPTGitHub(object):

    # Some unused arguments may be included to match the real class's API.
    # pylint: disable=unused-argument

    def __init__(self, host, user=None, token=None):
        """Initializes a mock WPTGitHub.
        """
        self.calls = []
        self.pull_requests_created = []
        self.create_pr_index = 0
        self.create_pr_fail_index = -1

    def create_pr(self, remote_branch_name, desc_title, body):
        self.calls.append('create_pr')

        if self.create_pr_fail_index != self.create_pr_index:
            self.pull_requests_created.append((remote_branch_name, desc_title, body))

        self.create_pr_index += 1
        return 5678

    def add_label(self, _, label):
        self.calls.append('add_label "%s"' % label)

    def extract_metadata(self, tag, commit_body, all_matches=False):
        return WPTGitHub.extract_metadata(tag, commit_body, all_matches)
