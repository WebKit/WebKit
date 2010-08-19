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

import logging
import os
import re
import stat

import webkitpy.common.config as config
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.system.executive import ScriptError
import webkitpy.thirdparty.autoinstalled.rietveld.upload as upload


class Rietveld(object):
    def __init__(self, executive, dryrun=False):
        self.dryrun = dryrun
        self._executive = executive

    def url_for_issue(self, codereview_issue):
        if not codereview_issue:
            return None
        return "%s%s" % (config.codereview_server_url, codereview_issue)

    def post(self, diff, patch_id, codereview_issue, message=None, cc=None):
        if not message:
            raise ScriptError("Rietveld requires a message.")

        # Rietveld has a 100 character limit on message length.
        if len(message) > 100:
            message = message[:100]

        args = [
            # First argument is empty string to mimic sys.argv.
            "",
            "--assume_yes",
            "--server=%s" % config.codereview_server_host,
            "--message=%s" % message,
            "--webkit_patch_id=%s" % patch_id,
        ]
        if codereview_issue:
            args.append("--issue=%s" % codereview_issue)
        if cc:
            args.append("--cc=%s" % cc)

        if self.dryrun:
            log("Would have run %s" % args)
            return

        # Use RealMain instead of calling upload from the commandline so that
        # we can pass in the diff ourselves. Otherwise, upload will just use
        # git diff for git checkouts, which doesn't respect --git-commit.
        issue, patchset = upload.RealMain(args, data=diff)
        return issue
