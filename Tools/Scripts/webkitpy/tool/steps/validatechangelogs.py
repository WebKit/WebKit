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

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.checkout.diff_parser import DiffParser
from webkitpy.common.system.deprecated_logging import error, log


# This is closely related to the ValidateReviewer step and the CommitterValidator class.
# We may want to unify more of this code in one place.
class ValidateChangeLogs(AbstractStep):
    def _check_changelog_diff(self, diff_file):
        if not self._tool.checkout().is_path_to_changelog(diff_file.filename):
            return True
        # Each line is a tuple, the first value is the deleted line number
        # Date, reviewer, bug title, bug url, and empty lines could all be
        # identical in the most recent entries.  If the diff starts any
        # later than that, assume that the entry is wrong.
        if diff_file.lines[0][0] < 8:
            return True
        if self._options.non_interactive:
            return False

        log("The diff to %s looks wrong.  Are you sure your ChangeLog entry is at the top of the file?" % (diff_file.filename))
        # FIXME: Do we need to make the file path absolute?
        self._tool.scm().diff_for_file(diff_file.filename)
        if self._tool.user.confirm("OK to continue?", default='n'):
            return True
        return False

    def run(self, state):
        parsed_diff = DiffParser(self.cached_lookup(state, "diff").splitlines())
        for filename, diff_file in parsed_diff.files.items():
            if not self._check_changelog_diff(diff_file):
                error("ChangeLog entry in %s is not at the top of the file." % diff_file.filename)
