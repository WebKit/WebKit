# Copyright (C) 2009 Google Inc. All rights reserved.
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

import os

from modules.mock import Mock
from modules.scm import CommitMessage


class MockBugzilla(Mock):
    patch1 = {
        "id" : 197,
        "bug_id" : 42,
        "url" : "http://example.com/197",
        "is_obsolete" : False,
        "reviewer" : "Reviewer1",
        "attacher_email" : "Contributer1",
    }
    patch2 = {
        "id" : 128,
        "bug_id" : 42,
        "url" : "http://example.com/128",
        "is_obsolete" : False,
        "reviewer" : "Reviewer2",
        "attacher_email" : "Contributer2",
    }
    bug_server_url = "http://example.com"

    def fetch_bug_ids_from_commit_queue(self):
        return [42, 75]

    def fetch_attachment_ids_from_review_queue(self):
        return [197, 128]

    def fetch_patches_from_commit_queue(self):
        return [self.patch1, self.patch2]

    def fetch_patches_from_pending_commit_list(self):
        return [self.patch1, self.patch2]

    def fetch_reviewed_patches_from_bug(self, bug_id):
        if bug_id == 42:
            return [self.patch1, self.patch2]
        return None

    def fetch_attachments_from_bug(self, bug_id):
        if bug_id == 42:
            return [self.patch1, self.patch2]
        return None

    def fetch_patches_from_bug(self, bug_id):
        if bug_id == 42:
            return [self.patch1, self.patch2]
        return None

    def fetch_attachment(self, attachment_id):
        if attachment_id == 197:
            return self.patch1
        if attachment_id == 128:
            return self.patch2
        raise Exception("Bogus attachment_id in fetch_attachment.")

    def bug_url_for_bug_id(self, bug_id):
        return "%s/%s" % (self.bug_server_url, bug_id)

    def attachment_url_for_id(self, attachment_id, action):
        action_param = ""
        if action and action != "view":
            action_param = "&action=%s" % action
        return "%s/%s%s" % (self.bug_server_url, attachment_id, action_param)


class MockBuildBot(Mock):
    def builder_statuses(self):
        return [{
            "name": "Builder1",
            "is_green": True
        }, {
            "name": "Builder2",
            "is_green": True
        }]

    def red_core_builders_names(self):
        return []

class MockSCM(Mock):
    def __init__(self):
        Mock.__init__(self)
        self.checkout_root = os.getcwd()

    def create_patch(self):
        return "Patch1"

    def commit_ids_from_commitish_arguments(self, args):
        return ["Commitish1", "Commitish2"]

    def commit_message_for_local_commit(self, commit_id):
        if commit_id == "Commitish1":
            return CommitMessage("CommitMessage1\nhttps://bugs.example.org/show_bug.cgi?id=42\n")
        if commit_id == "Commitish2":
            return CommitMessage("CommitMessage2\nhttps://bugs.example.org/show_bug.cgi?id=75\n")
        raise Exception("Bogus commit_id in commit_message_for_local_commit.")

    def create_patch_from_local_commit(self, commit_id):
        if commit_id == "Commitish1":
            return "Patch1"
        if commit_id == "Commitish2":
            return "Patch2"
        raise Exception("Bogus commit_id in commit_message_for_local_commit.")

    def modified_changelogs(self):
        # Ideally we'd return something more interesting here.
        # The problem is that LandDiff will try to actually read the path from disk!
        return []


class MockBugzillaTool():
    def __init__(self):
        self.bugs = MockBugzilla()
        self.buildbot = MockBuildBot()
        self.executive = Mock()
        self._scm = MockSCM()

    def scm(self):
        return self._scm

    def path(self):
        return "echo"
