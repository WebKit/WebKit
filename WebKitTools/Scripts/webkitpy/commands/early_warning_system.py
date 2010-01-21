#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
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

from StringIO import StringIO

from webkitpy.commands.queues import AbstractReviewQueue
from webkitpy.committers import CommitterList
from webkitpy.executive import ScriptError
from webkitpy.webkitport import WebKitPort
from webkitpy.queueengine import QueueEngine


class AbstractEarlyWarningSystem(AbstractReviewQueue):
    _build_style = "release"

    def __init__(self):
        AbstractReviewQueue.__init__(self)
        self.port = WebKitPort.port(self.port_name)

    def should_proceed_with_work_item(self, patch):
        try:
            self.run_webkit_patch([
                "build",
                self.port.flag(),
                "--build-style=%s" % self._build_style,
                "--force-clean",
                "--quiet"])
            self._update_status("Building", patch)
        except ScriptError, e:
            self._update_status("Unable to perform a build")
            return False
        return True

    def _review_patch(self, patch):
        self.run_webkit_patch([
            "build-attachment",
            self.port.flag(),
            "--build-style=%s" % self._build_style,
            "--force-clean",
            "--quiet",
            "--non-interactive",
            "--parent-command=%s" % self.name,
            "--no-update",
            patch.id()])

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        is_svn_apply = script_error.command_name() == "svn-apply"
        status_id = cls._update_status_for_script_error(tool, state, script_error, is_error=is_svn_apply)
        if is_svn_apply:
            QueueEngine.exit_after_handled_error(script_error)
        results_link = tool.status_server.results_url_for_status(status_id)
        message = "Attachment %s did not build on %s:\nBuild output: %s" % (state["patch"].id(), cls.port_name, results_link)
        tool.bugs.post_comment_to_bug(state["patch"].bug_id(), message, cc=cls.watchers)
        exit(1)


class GtkEWS(AbstractEarlyWarningSystem):
    name = "gtk-ews"
    port_name = "gtk"
    watchers = AbstractEarlyWarningSystem.watchers + [
        "gns@gnome.org",
        "xan.lopez@gmail.com",
    ]


class QtEWS(AbstractEarlyWarningSystem):
    name = "qt-ews"
    port_name = "qt"


class ChromiumEWS(AbstractEarlyWarningSystem):
    name = "chromium-ews"
    port_name = "chromium"
    watchers = AbstractEarlyWarningSystem.watchers + [
        "dglazkov@chromium.org",
    ]


# For platforms that we can't run inside a VM (like Mac OS X), we require
# patches to be uploaded by committers, who are generally trustworthy folk. :)
class AbstractCommitterOnlyEWS(AbstractEarlyWarningSystem):
    def __init__(self, committers=CommitterList()):
        AbstractEarlyWarningSystem.__init__(self)
        self._committers = committers

    def process_work_item(self, patch):
        if not self._committers.committer_by_email(patch.attacher_email()):
            self._did_error(patch, "%s cannot process patches from non-committers :(" % self.name)
            return
        AbstractEarlyWarningSystem.process_work_item(self, patch)


class MacEWS(AbstractCommitterOnlyEWS):
    name = "mac-ews"
    port_name = "mac"
