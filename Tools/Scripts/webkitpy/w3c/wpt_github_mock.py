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

from webkitpy.w3c.wpt_github import MergeError, WPTGitHub


class MockWPTGitHub(object):

    # Some unused arguments may be included to match the real class's API.
    # pylint: disable=unused-argument

    def __init__(self, pull_requests, unsuccessful_merge_index=-1, create_pr_fail_index=-1, merged_index=-1):
        """Initializes a mock WPTGitHub.

        Args:
            pull_requests: A list of wpt_github.PullRequest.
            unsuccessful_merge_index: The index to the PR in pull_requests that
                cannot be merged. (-1 means all can be merged.)
            create_pr_fail_index: The 0-based index of which PR creation request
                will fail. (-1 means all will succeed.)
            merged_index: The index to the PR in pull_requests that is already
                merged. (-1 means none is merged.)
        """
        self.pull_requests = pull_requests
        self.calls = []
        self.pull_requests_created = []
        self.pull_requests_merged = []
        self.unsuccessful_merge_index = unsuccessful_merge_index
        self.create_pr_index = 0
        self.create_pr_fail_index = create_pr_fail_index
        self.merged_index = merged_index

    def all_pull_requests(self, limit=30):
        self.calls.append('all_pull_requests')
        return self.pull_requests

    def is_pr_merged(self, number):
        for index, pr in enumerate(self.pull_requests):
            if pr.number == number:
                return index == self.merged_index
        return False

    def merge_pr(self, number):
        self.calls.append('merge_pr')

        for index, pr in enumerate(self.pull_requests):
            if pr.number == number and index == self.unsuccessful_merge_index:
                raise MergeError(number)

        self.pull_requests_merged.append(number)

    def create_pr(self, remote_branch_name, desc_title, body):
        self.calls.append('create_pr')

        if self.create_pr_fail_index != self.create_pr_index:
            self.pull_requests_created.append((remote_branch_name, desc_title, body))

        self.create_pr_index += 1
        return 5678

    def update_pr(self, pr_number, desc_title, body):
        self.calls.append('update_pr')
        return 5678

    def delete_remote_branch(self, _):
        self.calls.append('delete_remote_branch')

    def add_label(self, _, label):
        self.calls.append('add_label "%s"' % label)

    def remove_label(self, _, label):
        self.calls.append('remove_label "%s"' % label)

    def get_pr_branch(self, number):
        self.calls.append('get_pr_branch')
        return 'fake_branch_PR_%d' % number

    def pr_for_chromium_commit(self, commit):
        self.calls.append('pr_for_chromium_commit')
        for pr in self.pull_requests:
            if commit.change_id() in pr.body:
                return pr
        return None

    def pr_with_position(self, position):
        self.calls.append('pr_with_position')
        for pr in self.pull_requests:
            if position in pr.body:
                return pr
        return None

    def pr_with_change_id(self, change_id):
        self.calls.append('pr_with_change_id')
        for pr in self.pull_requests:
            if change_id in pr.body:
                return pr
        return None

    def extract_metadata(self, tag, commit_body, all_matches=False):
        return WPTGitHub.extract_metadata(tag, commit_body, all_matches)
