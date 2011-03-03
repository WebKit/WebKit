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

import random
from webkitpy.common.config import irc as config_irc

from webkitpy.common.config import urls
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.net.bugzilla import parse_bug_id
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.bot.queueengine import TerminateQueue
from webkitpy.tool.grammar import join_with_separators

# FIXME: Merge with Command?
class IRCCommand(object):
    def execute(self, nick, args, tool, sheriff):
        raise NotImplementedError, "subclasses must implement"


class LastGreenRevision(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        return "%s: %s" % (nick,
            urls.view_revision_url(tool.buildbot.last_green_revision()))


class Restart(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        tool.irc().post("Restarting...")
        raise TerminateQueue()


class Rollout(IRCCommand):
    def _parse_args(self, args):
        read_revision = True
        rollout_reason = []
        # the first argument must be a revision number
        svn_revision_list = [args[0].lstrip("r")]
        if not svn_revision_list[0].isdigit():
            read_revision = False

        for arg in args[1:]:
            if arg.lstrip("r").isdigit() and read_revision:
                svn_revision_list.append(arg.lstrip("r"))
            else:
                read_revision = False
                rollout_reason.append(arg)

        return svn_revision_list, rollout_reason

    def execute(self, nick, args, tool, sheriff):
        svn_revision_list, rollout_reason = self._parse_args(args)

        if (len(svn_revision_list) == 0) or (len(rollout_reason) == 0):
            tool.irc().post("%s: Usage: SVN_REVISION [SVN_REVISIONS] REASON" % nick)
            return

        rollout_reason = " ".join(rollout_reason)

        tool.irc().post("Preparing rollout for %s..." %
                        join_with_separators(["r" + str(revision) for revision in svn_revision_list]))
        try:
            complete_reason = "%s (Requested by %s on %s)." % (
                rollout_reason, nick, config_irc.channel)
            bug_id = sheriff.post_rollout_patch(svn_revision_list, complete_reason)
            bug_url = tool.bugs.bug_url_for_bug_id(bug_id)
            tool.irc().post("%s: Created rollout: %s" % (nick, bug_url))
        except ScriptError, e:
            tool.irc().post("%s: Failed to create rollout patch:" % nick)
            tool.irc().post("%s" % e)
            bug_id = parse_bug_id(e.output)
            if bug_id:
                tool.irc().post("Ugg...  Might have created %s" %
                    tool.bugs.bug_url_for_bug_id(bug_id))


class Help(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        return "%s: Available commands: %s" % (nick, ", ".join(commands.keys()))


class Hi(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        quips = tool.bugs.quips()
        quips.append('"Only you can prevent forest fires." -- Smokey the Bear')
        return random.choice(quips)


class Whois(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        if len(args) != 1:
            return "%s: Usage: BUGZILLA_EMAIL" % nick
        email = args[0]
        committer = CommitterList().committer_by_email(email)
        if not committer:
            return "%s: Sorry, I don't know %s. Maybe you could introduce me?" % (nick, email)
        if not committer.irc_nickname:
            return "%s: %s hasn't told me their nick. Boo hoo :-(" % (nick, email)
        return "%s: %s is %s. Why do you ask?" % (nick, email, committer.irc_nickname)


class Eliza(IRCCommand):
    therapist = None

    def __init__(self):
        if not self.therapist:
            import webkitpy.thirdparty.autoinstalled.eliza as eliza
            Eliza.therapist = eliza.eliza()

    def execute(self, nick, args, tool, sheriff):
        return "%s: %s" % (nick, self.therapist.respond(" ".join(args)))


# FIXME: Lame.  We should have an auto-registering CommandCenter.
commands = {
    "help": Help,
    "hi": Hi,
    "last-green-revision": LastGreenRevision,
    "restart": Restart,
    "rollout": Rollout,
    "whois": Whois,
}
