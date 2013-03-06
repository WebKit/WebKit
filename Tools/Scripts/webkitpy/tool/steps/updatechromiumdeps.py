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

import logging
import sys
import urllib2

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.config import urls
from webkitpy.common.net.networktransaction import NetworkTimeout

_log = logging.getLogger(__name__)


class UpdateChromiumDEPS(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.non_interactive,
        ]

    # Notice that this method throws lots of exciting exceptions!
    def _fetch_last_known_good_revision(self):
        return int(self._tool.web.get_binary(urls.chromium_lkgr_url))

    @classmethod
    def _parse_revision_number(cls, revision):
        try:
            if isinstance(revision, int):
                return revision
            if isinstance(revision, str):
                return int(revision.lstrip('r'))
            return None
        except ValueError:
            return None

    def _validate_revisions(self, current_chromium_revision, new_chromium_revision):
        if new_chromium_revision < current_chromium_revision:
            message = "Current Chromium DEPS revision %s is newer than %s." % (current_chromium_revision, new_chromium_revision)
            if self._options.non_interactive:
                _log.error(message)
                sys.exit(1)
            _log.info(message)
            new_chromium_revision = self._tool.user.prompt("Enter new chromium revision (enter nothing to cancel):\n")
            try:
                new_chromium_revision = int(new_chromium_revision)
            except ValueError, TypeError:
                new_chromium_revision = None
            if not new_chromium_revision:
                _log.error("Unable to update Chromium DEPS.")
                sys.exit(1)

    def run(self, state):
        new_chromium_revision = state["chromium_revision"]
        if new_chromium_revision == "LKGR":
            try:
                new_chromium_revision = self._fetch_last_known_good_revision()
            except ValueError:
                _log.error("Unable to parse LKGR from: ", urls.chromium_lkgr_url)
                sys.exit(1)
            except NetworkTimeout:
                _log.error("Unable to reach LKGR source: ", urls.chromium_lkgr_url)
                sys.exit(1)
        else:
            new_chromium_revision = self._parse_revision_number(new_chromium_revision)
            if not new_chromium_revision:
                _log.error("Invalid revision number.")
                sys.exit(1)

        deps = self._tool.checkout().chromium_deps()
        current_chromium_revision = deps.read_variable("chromium_rev")
        self._validate_revisions(current_chromium_revision, new_chromium_revision)
        _log.info("Updating Chromium DEPS to %s" % new_chromium_revision)
        deps.write_variable("chromium_rev", new_chromium_revision)
