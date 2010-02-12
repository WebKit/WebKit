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

from webkitpy.bugzilla import Bug, Attachment
from webkitpy.committers import CommitterList, Reviewer
from webkitpy.mock import Mock
from webkitpy.scm import CommitMessage
from webkitpy.webkit_logging import log


def _id_to_object_dictionary(*objects):
    dictionary = {}
    for thing in objects:
        dictionary[thing["id"]] = thing
    return dictionary


# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.


_patch1 = {
    "id": 197,
    "bug_id": 42,
    "url": "http://example.com/197",
    "is_obsolete": False,
    "is_patch": True,
    "review": "+",
    "reviewer_email": "foo@bar.com",
    "commit-queue": "+",
    "committer_email": "foo@bar.com",
    "attacher_email": "Contributer1",
}


_patch2 = {
    "id": 128,
    "bug_id": 42,
    "url": "http://example.com/128",
    "is_obsolete": False,
    "is_patch": True,
    "review": "+",
    "reviewer_email": "foo@bar.com",
    "commit-queue": "+",
    "committer_email": "non-committer@example.com",
    "attacher_email": "eric@webkit.org",
}


_patch3 = {
    "id": 103,
    "bug_id": 75,
    "url": "http://example.com/103",
    "is_obsolete": False,
    "is_patch": True,
    "review": "?",
    "attacher_email": "eric@webkit.org",
}


_patch4 = {
    "id": 104,
    "bug_id": 77,
    "url": "http://example.com/103",
    "is_obsolete": False,
    "is_patch": True,
    "review": "+",
    "commit-queue": "?",
    "reviewer_email": "foo@bar.com",
    "attacher_email": "Contributer2",
}


_patch5 = {
    "id": 105,
    "bug_id": 77,
    "url": "http://example.com/103",
    "is_obsolete": False,
    "is_patch": True,
    "review": "+",
    "reviewer_email": "foo@bar.com",
    "attacher_email": "eric@webkit.org",
}


_patch6 = { # Valid committer, but no reviewer.
    "id": 106,
    "bug_id": 77,
    "url": "http://example.com/103",
    "is_obsolete": False,
    "is_patch": True,
    "commit-queue": "+",
    "committer_email": "foo@bar.com",
    "attacher_email": "eric@webkit.org",
}


_patch7 = { # Valid review, patch is marked obsolete.
    "id": 107,
    "bug_id": 76,
    "url": "http://example.com/103",
    "is_obsolete": True,
    "is_patch": True,
    "review": "+",
    "reviewer_email": "foo@bar.com",
    "attacher_email": "eric@webkit.org",
}


# This must be defined before we define the bugs, thus we don't use
# MockBugzilla.unassigned_email directly.
_unassigned_email = "unassigned@example.com"


# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.


_bug1 = {
    "id": 42,
    "title": "Bug with two r+'d and cq+'d patches, one of which has an "
             "invalid commit-queue setter.",
    "assigned_to_email": _unassigned_email,
    "attachments": [_patch1, _patch2],
}


_bug2 = {
    "id": 75,
    "title": "Bug with a patch needing review.",
    "assigned_to_email": "foo@foo.com",
    "attachments": [_patch3],
}


_bug3 = {
    "id": 76,
    "title": "The third bug",
    "assigned_to_email": _unassigned_email,
    "attachments": [_patch7],
}


_bug4 = {
    "id": 77,
    "title": "The fourth bug",
    "assigned_to_email": "foo@foo.com",
    "attachments": [_patch4, _patch5, _patch6],
}


class MockBugzillaQueries(Mock):

    def __init__(self, bugzilla):
        Mock.__init__(self)
        self._bugzilla = bugzilla

    def _all_bugs(self):
        return map(lambda bug_dictionary: Bug(bug_dictionary, self._bugzilla),
                   self._bugzilla.bug_cache.values())

    def fetch_bug_ids_from_commit_queue(self):
        bugs_with_commit_queued_patches = filter(
                lambda bug: bug.commit_queued_patches(),
                self._all_bugs())
        return map(lambda bug: bug.id(), bugs_with_commit_queued_patches)

    def fetch_attachment_ids_from_review_queue(self):
        unreviewed_patches = sum([bug.unreviewed_patches()
                                  for bug in self._all_bugs()], [])
        return map(lambda patch: patch.id(), unreviewed_patches)

    def fetch_patches_from_commit_queue(self):
        return sum([bug.commit_queued_patches()
                    for bug in self._all_bugs()], [])

    def fetch_bug_ids_from_pending_commit_list(self):
        bugs_with_reviewed_patches = filter(lambda bug: bug.reviewed_patches(),
                                            self._all_bugs())
        bug_ids = map(lambda bug: bug.id(), bugs_with_reviewed_patches)
        # NOTE: This manual hack here is to allow testing logging in
        # test_assign_to_committer the real pending-commit query on bugzilla
        # will return bugs with patches which have r+, but are also obsolete.
        return bug_ids + [76]

    def fetch_patches_from_pending_commit_list(self):
        return sum([bug.reviewed_patches() for bug in self._all_bugs()], [])


# FIXME: Bugzilla is the wrong Mock-point.  Once we have a BugzillaNetwork
#        class we should mock that instead.
# Most of this class is just copy/paste from Bugzilla.


class MockBugzilla(Mock):

    bug_server_url = "http://example.com"

    unassigned_email = _unassigned_email

    bug_cache = _id_to_object_dictionary(_bug1, _bug2, _bug3, _bug4)

    attachment_cache = _id_to_object_dictionary(_patch1,
                                                _patch2,
                                                _patch3,
                                                _patch4,
                                                _patch5,
                                                _patch6,
                                                _patch7)

    def __init__(self):
        Mock.__init__(self)
        self.queries = MockBugzillaQueries(self)
        self.committers = CommitterList(reviewers=[Reviewer("Foo Bar",
                                                            "foo@bar.com")])

    def fetch_bug(self, bug_id):
        return Bug(self.bug_cache.get(bug_id), self)

    def fetch_attachment(self, attachment_id):
        # This could be changed to .get() if we wish to allow failed lookups.
        attachment_dictionary = self.attachment_cache[attachment_id]
        bug = self.fetch_bug(attachment_dictionary["bug_id"])
        for attachment in bug.attachments(include_obsolete=True):
            if attachment.id() == int(attachment_id):
                return attachment

    def bug_url_for_bug_id(self, bug_id):
        return "%s/%s" % (self.bug_server_url, bug_id)

    def fetch_bug_dictionary(self, bug_id):
        return self.bug_cache.get(bug_id)

    def attachment_url_for_id(self, attachment_id, action="view"):
        action_param = ""
        if action and action != "view":
            action_param = "&action=%s" % action
        return "%s/%s%s" % (self.bug_server_url, attachment_id, action_param)


class MockBuildBot(Mock):

    def builder_statuses(self):
        return [{
            "name": "Builder1",
            "is_green": True,
        }, {
            "name": "Builder2",
            "is_green": True,
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
            return CommitMessage("CommitMessage1\n" \
                "https://bugs.example.org/show_bug.cgi?id=42\n")
        if commit_id == "Commitish2":
            return CommitMessage("CommitMessage2\n" \
                "https://bugs.example.org/show_bug.cgi?id=75\n")
        raise Exception("Bogus commit_id in commit_message_for_local_commit.")

    def create_patch_from_local_commit(self, commit_id):
        if commit_id == "Commitish1":
            return "Patch1"
        if commit_id == "Commitish2":
            return "Patch2"
        raise Exception("Bogus commit_id in commit_message_for_local_commit.")

    def diff_for_revision(self, revision):
        return "DiffForRevision%s\n" \
               "http://bugs.webkit.org/show_bug.cgi?id=12345" % revision

    def svn_revision_from_commit_text(self, commit_text):
        return "49824"

    def modified_changelogs(self):
        # Ideally we'd return something more interesting here.  The problem is
        # that LandDiff will try to actually read the path from disk!
        return []


class MockUser(object):

    @staticmethod
    def prompt(message, repeat=1, raw_input=raw_input):
        return "Mock user response"

    def edit(self, files):
        pass

    def page(self, message):
        pass

    def confirm(self, message=None):
        return True

    def open_url(self, url):
        log("MOCK: user.open_url: %s" % url)
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
