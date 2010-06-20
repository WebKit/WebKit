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
import webkitpy.common.config.irc as config_irc

from webkitpy.common.checkout.changelog import view_source_url
from webkitpy.tool.bot.queueengine import TerminateQueue
from webkitpy.common.net.bugzilla import parse_bug_id
from webkitpy.common.system.executive import ScriptError

# FIXME: Merge with Command?
class IRCCommand(object):
    def execute(self, nick, args, tool, sheriff):
        raise NotImplementedError, "subclasses must implement"


class LastGreenRevision(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        return "%s: %s" % (nick,
            view_source_url(tool.buildbot.last_green_revision()))


class Restart(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        tool.irc().post("Restarting...")
        raise TerminateQueue()


class Rollout(IRCCommand):
    def execute(self, nick, args, tool, sheriff):
        if len(args) < 2:
            tool.irc().post("%s: Usage: SVN_REVISION REASON" % nick)
            return
        svn_revision = args[0].lstrip("r")
        rollout_reason = " ".join(args[1:])
        tool.irc().post("Preparing rollout for r%s..." % svn_revision)
        try:
            complete_reason = "%s (Requested by %s on %s)." % (
                rollout_reason, nick, config_irc.channel)
            bug_id = sheriff.post_rollout_patch(svn_revision, complete_reason)
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
    "last-green-revision": LastGreenRevision,
    "restart": Restart,
    "rollout": Rollout,
    "help": Help,
    "hi": Hi,
}
