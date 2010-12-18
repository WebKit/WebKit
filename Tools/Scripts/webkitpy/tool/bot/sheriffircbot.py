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

import webkitpy.tool.bot.irc_command as irc_command

from webkitpy.common.net.irc.ircbot import IRCBotDelegate
from webkitpy.common.thread.threadedmessagequeue import ThreadedMessageQueue


class _IRCThreadTearoff(IRCBotDelegate):
    def __init__(self, password, message_queue, wakeup_event):
        self._password = password
        self._message_queue = message_queue
        self._wakeup_event = wakeup_event

    # IRCBotDelegate methods

    def irc_message_received(self, nick, message):
        self._message_queue.post([nick, message])
        self._wakeup_event.set()

    def irc_nickname(self):
        return "sheriffbot"

    def irc_password(self):
        return self._password


class SheriffIRCBot(object):
    def __init__(self, tool, sheriff):
        self._tool = tool
        self._sheriff = sheriff
        self._message_queue = ThreadedMessageQueue()

    def irc_delegate(self):
        return _IRCThreadTearoff(self._tool.irc_password,
                                 self._message_queue,
                                 self._tool.wakeup_event)

    def process_message(self, message):
        (nick, request) = message
        tokenized_request = request.strip().split(" ")
        if not tokenized_request:
            return
        command = irc_command.commands.get(tokenized_request[0])
        args = tokenized_request[1:]
        if not command:
            # Give the peoples someone to talk with.
            command = irc_command.Eliza
            args = tokenized_request
        response = command().execute(nick, args, self._tool, self._sheriff)
        if response:
            self._tool.irc().post(response)

    def process_pending_messages(self):
        (messages, is_running) = self._message_queue.take_all()
        for message in messages:
            self.process_message(message)
