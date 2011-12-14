# Copyright (C) 2011 Google Inc. All rights reserved.
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

import urllib2

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.config import urls
from webkitpy.common.system.deprecated_logging import log, error


class UpdateChromiumDEPS(AbstractStep):
    # Notice that this method throws lots of exciting exceptions!
    def _fetch_last_known_good_revision(self):
        return int(urllib2.urlopen(urls.chromium_lkgr_url).read())

    def _validate_revisions(self, current_chromium_revision, new_chromium_revision):
        if new_chromium_revision < current_chromium_revision:
            log("Current Chromium DEPS revision %s is newer than %s." % (current_chromium_revision, new_chromium_revision))
            new_chromium_revision = self._tool.user.prompt("Enter new chromium revision (enter nothing to cancel):\n")
            try:
                new_chromium_revision = int(new_chromium_revision)
            except ValueError, TypeError:
                new_chromium_revision = None
            if not new_chromium_revision:
                error("Unable to update Chromium DEPS")


    def run(self, state):
        # Note that state["chromium_revision"] must be defined, but can be None.
        new_chromium_revision = state["chromium_revision"]
        if not new_chromium_revision:
            new_chromium_revision = self._fetch_last_known_good_revision()

        deps = self._tool.checkout().chromium_deps()
        current_chromium_revision = deps.read_variable("chromium_rev")
        self._validate_revisions(current_chromium_revision, new_chromium_revision)
        log("Updating Chromium DEPS to %s" % new_chromium_revision)
        deps.write_variable("chromium_rev", new_chromium_revision)
