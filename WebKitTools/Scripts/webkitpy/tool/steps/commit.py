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

from webkitpy.common.checkout.changelog import view_source_url
from webkitpy.common.checkout.scm import AuthenticationError, AmbiguousCommitError
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.user import User
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options


class Commit(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.git_commit,
        ]

    def _commit_warning(self, error):
        working_directory_message = "" if error.working_directory_is_clean else " and working copy changes"
        return ('There are %s local commits%s. Everything will be committed as a single commit. '
                'To avoid this prompt, set "git config webkit-patch.commit-should-always-squash true".' % (
                error.num_local_commits, working_directory_message))

    def run(self, state):
        self._commit_message = self._tool.checkout().commit_message_for_this_commit(self._options.git_commit).message()
        if len(self._commit_message) < 50:
            raise Exception("Attempted to commit with a commit message shorter than 50 characters.  Either your patch is missing a ChangeLog or webkit-patch may have a bug.")

        self._state = state

        username = None
        force_squash = False

        num_tries = 0
        while num_tries < 3:
            num_tries += 1

            try:
                scm = self._tool.scm()
                commit_text = scm.commit_with_message(self._commit_message, git_commit=self._options.git_commit, username=username, force_squash=force_squash)
                svn_revision = scm.svn_revision_from_commit_text(commit_text)
                log("Committed r%s: <%s>" % (svn_revision, view_source_url(svn_revision)))
                self._state["commit_text"] = commit_text
                break;
            except AmbiguousCommitError, e:
                if self._tool.user.confirm(self._commit_warning(e)):
                    force_squash = True
                else:
                    # This will correctly interrupt the rest of the commit process.
                    raise ScriptError(message="Did not commit")
            except AuthenticationError, e:
                username = self._tool.user.prompt("%s login: " % e.server_host, repeat=5)
                if not username:
                    raise ScriptError("You need to specify the username on %s to perform the commit as." % self.svn_server_host)
