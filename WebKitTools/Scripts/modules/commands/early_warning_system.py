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

from modules.commands.queues import AbstractReviewQueue
from modules.executive import ScriptError
from modules.webkitport import WebKitPort

class AbstractEarlyWarningSystem(AbstractReviewQueue):
    def __init__(self):
        AbstractReviewQueue.__init__(self)
        self.port = WebKitPort.port(self.port_name)

    def should_proceed_with_work_item(self, patch):
        try:
            self.run_bugzilla_tool(["build", self.port.flag(), "--force-clean", "--quiet"])
        except ScriptError, e:
            return (False, "Unable to perform a build.", None)
        return (True, "Building patch %s on bug %s." % (patch["id"], patch["bug_id"]), patch)

    def process_work_item(self, patch):
        self.run_bugzilla_tool([
            "build-attachment",
            self.port.flag(),
            "--force-clean",
            "--quiet",
            "--non-interactive",
            "--parent-command=%s" % self.name,
            "--no-update",
            patch["id"]])
        self._patches.did_pass(patch)


class QtEWS(AbstractEarlyWarningSystem):
    name = "qt-ews"
    port_name = "qt"


class ChromiumEWS(AbstractEarlyWarningSystem):
    name = "chromium-ews"
    port_name = "chromium"
