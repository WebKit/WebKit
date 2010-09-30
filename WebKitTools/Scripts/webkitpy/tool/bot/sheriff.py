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

from webkitpy.common.checkout.changelog import view_source_url
from webkitpy.common.net.bugzilla import parse_bug_id
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.grammar import join_with_separators


class Sheriff(object):
    def __init__(self, tool, sheriffbot):
        self._tool = tool
        self._sheriffbot = sheriffbot

    def post_irc_warning(self, commit_info, builders):
        irc_nicknames = sorted([party.irc_nickname for
                                party in commit_info.responsible_parties()
                                if party.irc_nickname])
        irc_prefix = ": " if irc_nicknames else ""
        irc_message = "%s%s%s might have broken %s" % (
            ", ".join(irc_nicknames),
            irc_prefix,
            view_source_url(commit_info.revision()),
            join_with_separators([builder.name() for builder in builders]))

        self._tool.irc().post(irc_message)

    def post_rollout_patch(self, svn_revision, rollout_reason):
        # Ensure that svn_revision is a number (and not an option to
        # create-rollout).
        try:
            svn_revision = int(svn_revision)
        except:
            raise ScriptError(message="Invalid svn revision number \"%s\"."
                              % svn_revision)

        if rollout_reason.startswith("-"):
            raise ScriptError(message="The rollout reason may not begin "
                              "with - (\"%s\")." % rollout_reason)

        output = self._sheriffbot.run_webkit_patch([
            "create-rollout",
            "--force-clean",
            # In principle, we should pass --non-interactive here, but it
            # turns out that create-rollout doesn't need it yet.  We can't
            # pass it prophylactically because we reject unrecognized command
            # line switches.
            "--parent-command=sheriff-bot",
            svn_revision,
            rollout_reason,
        ])
        return parse_bug_id(output)

    def post_blame_comment_on_bug(self, commit_info, builders, tests):
        if not commit_info.bug_id():
            return
        comment = "%s might have broken %s" % (
            view_source_url(commit_info.revision()),
            join_with_separators([builder.name() for builder in builders]))
        if tests:
            comment += "\nThe following tests are not passing:\n"
            comment += "\n".join(tests)
        self._tool.bugs.post_comment_to_bug(commit_info.bug_id(),
                                            comment,
                                            cc=self._sheriffbot.watchers)
