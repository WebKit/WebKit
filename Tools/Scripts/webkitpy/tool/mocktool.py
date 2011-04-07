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
import threading

from webkitpy.common.config.committers import CommitterList, Reviewer
from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.checkout.scm import CommitMessage
from webkitpy.common.net.bugzilla import Bug, Attachment
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.thirdparty.mock import Mock


def _id_to_object_dictionary(*objects):
    dictionary = {}
    for thing in objects:
        dictionary[thing["id"]] = thing
    return dictionary

# Testing

# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.


_patch1 = {
    "id": 197,
    "bug_id": 42,
    "url": "http://example.com/197",
    "name": "Patch1",
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
    "name": "Patch2",
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
    "name": "Patch3",
    "is_obsolete": False,
    "is_patch": True,
    "review": "?",
    "attacher_email": "eric@webkit.org",
}


_patch4 = {
    "id": 104,
    "bug_id": 77,
    "url": "http://example.com/103",
    "name": "Patch3",
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
    "name": "Patch5",
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
    "name": "ROLLOUT of r3489",
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
    "name": "Patch7",
    "is_obsolete": True,
    "is_patch": True,
    "review": "+",
    "reviewer_email": "foo@bar.com",
    "attacher_email": "eric@webkit.org",
}


# This matches one of Bug.unassigned_emails
_unassigned_email = "webkit-unassigned@lists.webkit.org"
# This is needed for the FlakyTestReporter to believe the bug
# was filed by one of the webkitpy bots.
_commit_queue_email = "commit-queue@webkit.org"


# FIXME: The ids should be 1, 2, 3 instead of crazy numbers.


_bug1 = {
    "id": 42,
    "title": "Bug with two r+'d and cq+'d patches, one of which has an "
             "invalid commit-queue setter.",
    "reporter_email": "foo@foo.com",
    "assigned_to_email": _unassigned_email,
    "attachments": [_patch1, _patch2],
    "bug_status": "UNCONFIRMED",
}


_bug2 = {
    "id": 75,
    "title": "Bug with a patch needing review.",
    "reporter_email": "foo@foo.com",
    "assigned_to_email": "foo@foo.com",
    "attachments": [_patch3],
    "bug_status": "ASSIGNED",
}


_bug3 = {
    "id": 76,
    "title": "The third bug",
    "reporter_email": "foo@foo.com",
    "assigned_to_email": _unassigned_email,
    "attachments": [_patch7],
    "bug_status": "NEW",
}


_bug4 = {
    "id": 77,
    "title": "The fourth bug",
    "reporter_email": "foo@foo.com",
    "assigned_to_email": "foo@foo.com",
    "attachments": [_patch4, _patch5, _patch6],
    "bug_status": "REOPENED",
}


_bug5 = {
    "id": 78,
    "title": "The fifth bug",
    "reporter_email": _commit_queue_email,
    "assigned_to_email": "foo@foo.com",
    "attachments": [],
    "bug_status": "RESOLVED",
    "dup_id": 76,
}


# FIXME: This should not inherit from Mock
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

    def fetch_bugs_matching_search(self, search_string, author_email=None):
        return [self._bugzilla.fetch_bug(78), self._bugzilla.fetch_bug(77)]

_mock_reviewer = Reviewer("Foo Bar", "foo@bar.com")


# FIXME: Bugzilla is the wrong Mock-point.  Once we have a BugzillaNetwork
#        class we should mock that instead.
# Most of this class is just copy/paste from Bugzilla.
# FIXME: This should not inherit from Mock
class MockBugzilla(Mock):

    bug_server_url = "http://example.com"

    bug_cache = _id_to_object_dictionary(_bug1, _bug2, _bug3, _bug4, _bug5)

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
        self.committers = CommitterList(reviewers=[_mock_reviewer])
        self._override_patch = None

    def create_bug(self,
                   bug_title,
                   bug_description,
                   component=None,
                   diff=None,
                   patch_description=None,
                   cc=None,
                   blocked=None,
                   mark_for_review=False,
                   mark_for_commit_queue=False):
        log("MOCK create_bug")
        log("bug_title: %s" % bug_title)
        log("bug_description: %s" % bug_description)
        if component:
            log("component: %s" % component)
        if cc:
            log("cc: %s" % cc)
        if blocked:
            log("blocked: %s" % blocked)
        return 78

    def quips(self):
        return ["Good artists copy. Great artists steal. - Pablo Picasso"]

    def fetch_bug(self, bug_id):
        return Bug(self.bug_cache.get(bug_id), self)

    def set_override_patch(self, patch):
        self._override_patch = patch

    def fetch_attachment(self, attachment_id):
        if self._override_patch:
            return self._override_patch

        attachment_dictionary = self.attachment_cache.get(attachment_id)
        if not attachment_dictionary:
            print "MOCK: fetch_attachment: %s is not a known attachment id" % attachment_id
            return None
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

    def set_flag_on_attachment(self,
                               attachment_id,
                               flag_name,
                               flag_value,
                               comment_text=None,
                               additional_comment_text=None):
        log("MOCK setting flag '%s' to '%s' on attachment '%s' with comment '%s' and additional comment '%s'" % (
            flag_name, flag_value, attachment_id, comment_text, additional_comment_text))

    def post_comment_to_bug(self, bug_id, comment_text, cc=None):
        log("MOCK bug comment: bug_id=%s, cc=%s\n--- Begin comment ---\n%s\n--- End comment ---\n" % (
            bug_id, cc, comment_text))

    def add_attachment_to_bug(self,
                              bug_id,
                              file_or_string,
                              description,
                              filename=None,
                              comment_text=None):
        log("MOCK add_attachment_to_bug: bug_id=%s, description=%s filename=%s" % (bug_id, description, filename))
        if comment_text:
            log("-- Begin comment --")
            log(comment_text)
            log("-- End comment --")

    def add_patch_to_bug(self,
                         bug_id,
                         diff,
                         description,
                         comment_text=None,
                         mark_for_review=False,
                         mark_for_commit_queue=False,
                         mark_for_landing=False):
        log("MOCK add_patch_to_bug: bug_id=%s, description=%s, mark_for_review=%s, mark_for_commit_queue=%s, mark_for_landing=%s" %
            (bug_id, description, mark_for_review, mark_for_commit_queue, mark_for_landing))
        if comment_text:
            log("-- Begin comment --")
            log(comment_text)
            log("-- End comment --")


class MockBuilder(object):
    def __init__(self, name):
        self._name = name

    def name(self):
        return self._name

    def results_url(self):
        return "http://example.com/builders/%s/results/" % self.name()

    def force_build(self, username, comments):
        log("MOCK: force_build: name=%s, username=%s, comments=%s" % (
            self._name, username, comments))


class MockFailureMap(object):
    def __init__(self, buildbot):
        self._buildbot = buildbot

    def is_empty(self):
        return False

    def filter_out_old_failures(self, is_old_revision):
        pass

    def failing_revisions(self):
        return [29837]

    def builders_failing_for(self, revision):
        return [self._buildbot.builder_with_name("Builder1")]

    def tests_failing_for(self, revision):
        return ["mock-test-1"]


class MockBuildBot(object):
    buildbot_host = "dummy_buildbot_host"
    def __init__(self):
        self._mock_builder1_status = {
            "name": "Builder1",
            "is_green": True,
            "activity": "building",
        }
        self._mock_builder2_status = {
            "name": "Builder2",
            "is_green": True,
            "activity": "idle",
        }

    def builder_with_name(self, name):
        return MockBuilder(name)

    def builder_statuses(self):
        return [
            self._mock_builder1_status,
            self._mock_builder2_status,
        ]

    def red_core_builders_names(self):
        if not self._mock_builder2_status["is_green"]:
            return [self._mock_builder2_status["name"]]
        return []

    def red_core_builders(self):
        if not self._mock_builder2_status["is_green"]:
            return [self._mock_builder2_status]
        return []

    def idle_red_core_builders(self):
        if not self._mock_builder2_status["is_green"]:
            return [self._mock_builder2_status]
        return []

    def last_green_revision(self):
        return 9479

    def light_tree_on_fire(self):
        self._mock_builder2_status["is_green"] = False

    def failure_map(self):
        return MockFailureMap(self)


# FIXME: This should not inherit from Mock
class MockSCM(Mock):

    fake_checkout_root = os.path.realpath("/tmp") # realpath is needed to allow for Mac OS X's /private/tmp

    def __init__(self, filesystem=None):
        Mock.__init__(self)
        # FIXME: We should probably use real checkout-root detection logic here.
        # os.getcwd() can't work here because other parts of the code assume that "checkout_root"
        # will actually be the root.  Since getcwd() is wrong, use a globally fake root for now.
        self.checkout_root = self.fake_checkout_root
        self.added_paths = set()
        self._filesystem = filesystem

    def add(self, destination_path, return_exit_code=False):
        self.added_paths.add(destination_path)
        if return_exit_code:
            return 0

    def changed_files(self, git_commit=None):
        return ["MockFile1"]

    def create_patch(self, git_commit, changed_files=None):
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

    def diff_for_file(self, path, log=None):
        return path + '-diff'

    def diff_for_revision(self, revision):
        return "DiffForRevision%s\n" \
               "http://bugs.webkit.org/show_bug.cgi?id=12345" % revision

    def show_head(self, path):
        return path

    def svn_revision_from_commit_text(self, commit_text):
        return "49824"

    def delete(self, path):
        if not self._filesystem:
            return
        if self._filesystem.exists(path):
            self._filesystem.remove(path)


class MockDEPS(object):
    def read_variable(self, name):
        return 6564

    def write_variable(self, name, value):
        log("MOCK: MockDEPS.write_variable(%s, %s)" % (name, value))


class MockCheckout(object):

    _committer_list = CommitterList()

    def commit_info_for_revision(self, svn_revision):
        # The real Checkout would probably throw an exception, but this is the only way tests have to get None back at the moment.
        if not svn_revision:
            return None
        return CommitInfo(svn_revision, "eric@webkit.org", {
            "bug_id": 42,
            "author_name": "Adam Barth",
            "author_email": "abarth@webkit.org",
            "author": self._committer_list.committer_by_email("abarth@webkit.org"),
            "reviewer_text": "Darin Adler",
            "reviewer": self._committer_list.committer_by_name("Darin Adler"),
        })

    def bug_id_for_revision(self, svn_revision):
        return 12345

    def recent_commit_infos_for_files(self, paths):
        return [self.commit_info_for_revision(32)]

    def modified_changelogs(self, git_commit, changed_files=None):
        # Ideally we'd return something more interesting here.  The problem is
        # that LandDiff will try to actually read the patch from disk!
        return []

    def commit_message_for_this_commit(self, git_commit, changed_files=None):
        commit_message = Mock()
        commit_message.message = lambda:"This is a fake commit message that is at least 50 characters."
        return commit_message

    def chromium_deps(self):
        return MockDEPS()

    def apply_patch(self, patch, force=False):
        pass

    def apply_reverse_diffs(self, revision):
        pass

    def suggested_reviewers(self, git_commit, changed_files=None):
        return [_mock_reviewer]


class MockUser(object):

    @classmethod
    def prompt(cls, message, repeat=1, raw_input=raw_input):
        return "Mock user response"

    @classmethod
    def prompt_with_list(cls, list_title, list_items, can_choose_multiple=False, raw_input=raw_input):
        pass

    def __init__(self):
        self.opened_urls = []

    def edit(self, files):
        pass

    def edit_changelog(self, files):
        pass

    def page(self, message):
        pass

    def confirm(self, message=None, default='y'):
        log(message)
        return default == 'y'

    def can_open_url(self):
        return True

    def open_url(self, url):
        self.opened_urls.append(url)
        if url.startswith("file://"):
            log("MOCK: user.open_url: file://...")
            return
        log("MOCK: user.open_url: %s" % url)


class MockIRC(object):

    def post(self, message):
        log("MOCK: irc.post: %s" % message)

    def disconnect(self):
        log("MOCK: irc.disconnect")


class MockStatusServer(object):

    def __init__(self, bot_id=None, work_items=None):
        self.host = "example.com"
        self.bot_id = bot_id
        self._work_items = work_items or []

    def patch_status(self, queue_name, patch_id):
        return None

    def svn_revision(self, svn_revision):
        return None

    def next_work_item(self, queue_name):
        if not self._work_items:
            return None
        return self._work_items.pop(0)

    def release_work_item(self, queue_name, patch):
        log("MOCK: release_work_item: %s %s" % (queue_name, patch.id()))

    def update_work_items(self, queue_name, work_items):
        self._work_items = work_items
        log("MOCK: update_work_items: %s %s" % (queue_name, work_items))

    def submit_to_ews(self, patch_id):
        log("MOCK: submit_to_ews: %s" % (patch_id))

    def update_status(self, queue_name, status, patch=None, results_file=None):
        log("MOCK: update_status: %s %s" % (queue_name, status))
        return 187

    def update_svn_revision(self, svn_revision, broken_bot):
        return 191

    def results_url_for_status(self, status_id):
        return "http://dummy_url"


# FIXME: This should not inherit from Mock
# FIXME: Unify with common.system.executive_mock.MockExecutive.
class MockExecutive(Mock):
    def __init__(self, should_log):
        self.should_log = should_log

    def run_and_throw_if_fail(self, args, quiet=False):
        if self.should_log:
            log("MOCK run_and_throw_if_fail: %s" % args)
        return "MOCK output of child process"

    def run_command(self,
                    args,
                    cwd=None,
                    input=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True,
                    decode_output=False):
        if self.should_log:
            log("MOCK run_command: %s" % args)
        return "MOCK output of child process"


class MockOptions(object):
    """Mock implementation of optparse.Values."""

    def __init__(self, **kwargs):
        # The caller can set option values using keyword arguments. We don't
        # set any values by default because we don't know how this
        # object will be used. Generally speaking unit tests should
        # subclass this or provider wrapper functions that set a common
        # set of options.
        for key, value in kwargs.items():
            self.__dict__[key] = value


class MockPort(Mock):
    def name(self):
        return "MockPort"

    def layout_tests_results_path(self):
        return "/mock/results.html"

    def check_webkit_style_command(self):
        return ["mock-check-webkit-style"]

    def update_webkit_command(self):
        return ["mock-update-webkit"]


class MockTestPort1(object):

    def skips_layout_test(self, test_name):
        return test_name in ["media/foo/bar.html", "foo"]


class MockTestPort2(object):

    def skips_layout_test(self, test_name):
        return test_name == "media/foo/bar.html"


class MockPortFactory(object):

    def get_all(self, options=None):
        return {"test_port1": MockTestPort1(), "test_port2": MockTestPort2()}


class MockPlatformInfo(object):
    def display_name(self):
        return "MockPlatform 1.0"


class MockWorkspace(object):
    def find_unused_filename(self, directory, name, extension, search_limit=10):
        return "%s/%s.%s" % (directory, name, extension)

    def create_zip(self, zip_path, source_path):
        pass


class MockTool(object):

    def __init__(self, log_executive=False):
        self.wakeup_event = threading.Event()
        self.bugs = MockBugzilla()
        self.buildbot = MockBuildBot()
        self.executive = MockExecutive(should_log=log_executive)
        self.filesystem = MockFileSystem()
        self.workspace = MockWorkspace()
        self._irc = None
        self.user = MockUser()
        self._scm = MockSCM()
        self._checkout = MockCheckout()
        self.status_server = MockStatusServer()
        self.irc_password = "MOCK irc password"
        self.port_factory = MockPortFactory()
        self.platform = MockPlatformInfo()

    def scm(self):
        return self._scm

    def checkout(self):
        return self._checkout

    def ensure_irc_connected(self, delegate):
        if not self._irc:
            self._irc = MockIRC()

    def irc(self):
        return self._irc

    def path(self):
        return "echo"

    def port(self):
        return MockPort()


class MockBrowser(object):
    params = {}

    def open(self, url):
        pass

    def select_form(self, name):
        pass

    def __setitem__(self, key, value):
        self.params[key] = value

    def submit(self):
        return Mock(file)
