# Copyright (C) 2010 Google Inc. All rights reserved.
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

import os
import sys

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.prettypatch import PrettyPatch
from webkitpy.common.system import logutils
from webkitpy.common.system.executive import ScriptError

if sys.version_info > (3, 0):
    from urllib.request import pathname2url
else:
    from urllib import pathname2url

_log = logutils.get_logger(__file__)


class ConfirmDiff(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.confirm,
        ]

    def _show_pretty_diff(self, diff):
        if os.environ.get('WEBKIT_PATCH_PREFER_PAGER') or not self._tool.user.can_open_url():
            return None

        try:
            pretty_patch = PrettyPatch(self._tool.executive,
                                       self._tool.scm().checkout_root)
            pretty_diff_file = pretty_patch.pretty_diff_file(diff)
            url = "file://%s" % pathname2url(pretty_diff_file.name)
            self._tool.user.open_url(url)
            # We return the pretty_diff_file here because we need to keep the
            # file alive until the user has had a chance to confirm the diff.
            return pretty_diff_file
        except ScriptError:
            _log.warning("PrettyPatch failed.  :(")
        except OSError:
            _log.warning("PrettyPatch unavailable.")

    def run(self, state):
        if not self._options.confirm:
            return
        diff = self.cached_lookup(state, "diff")
        pretty_diff_file = self._show_pretty_diff(diff)
        if not pretty_diff_file:
            self._tool.user.page(diff)
        diff_correct = self._tool.user.confirm("Was that diff correct?")
        if pretty_diff_file:
            pretty_diff_file.close()
        if not diff_correct:
            self._exit(1)
