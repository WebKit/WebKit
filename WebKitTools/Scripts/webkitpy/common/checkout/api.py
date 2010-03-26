# Copyright (c) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.checkout.scm import CommitMessage
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.deprecated_logging import log


# This class represents the WebKit-specific parts of the checkout (like
# ChangeLogs).
# FIXME: Move a bunch of ChangeLog-specific processing from SCM to this object.
class Checkout(object):
    def __init__(self, scm):
        self._scm = scm

    def commit_info_for_revision(self, svn_revision):
        return CommitInfo.commit_info_for_revision(self._scm, svn_revision)

    # FIXME: Requires unit test
    # FIXME: commit_message_for_this_commit and modified_changelogs don't
    #        really belong here.  We should have a separate module for
    #        handling ChangeLogs.
    def commit_message_for_this_commit(self):
        changelog_paths = self._scm.modified_changelogs()
        if not len(changelog_paths):
            raise ScriptError(message="Found no modified ChangeLogs, cannot create a commit message.\n"
                              "All changes require a ChangeLog.  See:\n"
                              "http://webkit.org/coding/contributing.html")

        changelog_messages = []
        for changelog_path in changelog_paths:
            log("Parsing ChangeLog: %s" % changelog_path)
            changelog_entry = ChangeLog(changelog_path).latest_entry()
            if not changelog_entry:
                raise ScriptError(message="Failed to parse ChangeLog: " + os.path.abspath(changelog_path))
            changelog_messages.append(changelog_entry.contents())

        # FIXME: We should sort and label the ChangeLog messages like commit-log-editor does.
        return CommitMessage("".join(changelog_messages).splitlines())
