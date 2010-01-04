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

from webkitpy.mock import Mock
from webkitpy.scm import CommitMessage
from webkitpy.bugzilla import Bug

def _id_to_object_dictionary(*objects):
    dictionary = {}
    for thing in objects:
        dictionary[thing["id"]] = thing
    return dictionary

# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.
_patch1 = {
    "id" : 197,
    "bug_id" : 42,
    "url" : "http://example.com/197",
    "is_obsolete" : False,
    "is_patch" : True,
    "reviewer" : "Reviewer1",
    "attacher_email" : "Contributer1",
}
_patch2 = {
    "id" : 128,
    "bug_id" : 42,
    "url" : "http://example.com/128",
    "is_obsolete" : False,
    "is_patch" : True,
    "reviewer" : "Reviewer2",
    "attacher_email" : "eric@webkit.org",
}

# This must be defined before we define the bugs, thus we don't use MockBugzilla.unassigned_email directly.
_unassigned_email = "unassigned@example.com"

# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.
_bug1 = {
    "id" : 42,
    "assigned_to_email" : _unassigned_email,
    "attachments" : [_patch1, _patch2],
}
_bug2 = {
    "id" : 75,
    "assigned_to_email" : "foo@foo.com",
    "attachments" : [],
}
_bug3 = {
    "id" : 76,
    "assigned_to_email" : _unassigned_email,
    "attachments" : [],
}

class MockBugzillaQueries(Mock):
    def fetch_bug_ids_from_commit_queue(self):
        return [42, 75]

    def fetch_attachment_ids_from_review_queue(self):
        return [197, 128]

    def fetch_patches_from_commit_queue(self, reject_invalid_patches=False):
        return [_patch1, _patch2]

    def fetch_bug_ids_from_pending_commit_list(self):
        return [42, 75, 76]
    
    def fetch_patches_from_pending_commit_list(self):
        return [_patch1, _patch2]


class MockBugzilla(Mock):
    bug_server_url = "http://example.com"
    unassigned_email = _unassigned_email
    bug_cache = _id_to_object_dictionary(_bug1, _bug2, _bug3)
    attachment_cache = _id_to_object_dictionary(_patch1, _patch2)
    queries = MockBugzillaQueries()

    def fetch_bug(self, bug_id):
        return Bug(self.bug_cache.get(bug_id))

    def fetch_reviewed_patches_from_bug(self, bug_id):
        return self.fetch_patches_from_bug(bug_id) # Return them all for now.

    def fetch_attachment(self, attachment_id):
        return self.attachment_cache[attachment_id] # This could be changed to .get() if we wish to allow failed lookups.

    # NOTE: Functions below this are direct copies from bugzilla.py
    def fetch_patches_from_bug(self, bug_id):
        return self.fetch_bug(bug_id).patches()
    
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

    def diff_for_revision(self, revision):
        return "DiffForRevision%s\nhttp://bugs.webkit.org/show_bug.cgi?id=12345" % revision

    def modified_changelogs(self):
        # Ideally we'd return something more interesting here.
        # The problem is that LandDiff will try to actually read the path from disk!
        return []


class MockUser(object):
    def prompt(self, message):
        return "Mock user response"

    def edit(self, files):
        pass

    def page(self, message):
        pass

    def confirm(self, message=None):
        return True

    def open_url(self, url):
        pass


class MockStatusServer(object):
    def __init__(self):
        self.host = "example.com"

    def patch_status(self, queue_name, patch_id):
        return None

    def update_status(self, queue_name, status, patch=None, results_file=None):
        return 187


class MockBugzillaTool():
    def __init__(self):
        self.bugs = MockBugzilla()
        self.buildbot = MockBuildBot()
        self.executive = Mock()
        self.user = MockUser()
        self._scm = MockSCM()
        self.status_server = MockStatusServer()

    def scm(self):
        return self._scm

    def path(self):
        return "echo"
