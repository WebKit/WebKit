# Copyright (C) 2010-2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import itertools

from collections import Counter

BUILTIN_ATTRIBUTE = "Builtin"
ASYNC_ATTRIBUTE = "Async"

class MessageReceiver(object):
    def __init__(self, name, superclass, attributes, messages, condition):
        self.name = name
        self.superclass = superclass
        self.attributes = frozenset(attributes or [])
        self.messages = messages
        self.condition = condition

    def iterparameters(self):
        return itertools.chain((parameter for message in self.messages for parameter in message.parameters),
            (reply_parameter for message in self.messages if message.reply_parameters for reply_parameter in message.reply_parameters))

    def has_attribute(self, attribute):
        return attribute in self.attributes


class Message(object):
    def __init__(self, name, parameters, reply_parameters, attributes, condition):
        self.name = name
        self.parameters = parameters
        self.reply_parameters = reply_parameters
        self.attributes = frozenset(attributes or [])
        self.condition = condition

    def has_attribute(self, attribute):
        return attribute in self.attributes


class Parameter(object):
    def __init__(self, kind, type, name, attributes=None, condition=None):
        self.kind = kind
        self.type = type
        self.name = name
        self.attributes = frozenset(attributes or [])
        self.condition = condition

    def has_attribute(self, attribute):
        return attribute in self.attributes


ipc_receiver = MessageReceiver(name="IPC", superclass=None, attributes=[BUILTIN_ATTRIBUTE], messages=[
    Message('WrappedAsyncMessageForTesting', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('SyncMessageReply', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('InitializeConnection', [], [], attributes=[BUILTIN_ATTRIBUTE], condition="PLATFORM(COCOA)"),
    Message('LegacySessionState', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None)
], condition=None)


def check_global_model_inputs(receivers):
    errors = []
    receiver_counts = Counter([r.name for r in receivers])
    receiver_duplicates = [n for n, c in receiver_counts.items() if c > 1]
    if receiver_duplicates:
        errors.append('Duplicate message receiver names: %s' % (', '.join(receiver_duplicates)))

    return errors


def generate_global_model(receivers):
    async_reply_messages = []
    for receiver in receivers:
        for message in receiver.messages:
            if message.has_attribute(ASYNC_ATTRIBUTE):
                async_reply_messages.append(Message(name='%s_%sReply' % (receiver.name, message.name), parameters=message.reply_parameters, reply_parameters=[], attributes=None, condition=message.condition))
    async_reply_receiver = MessageReceiver(name='AsyncReply', superclass='None', attributes=[BUILTIN_ATTRIBUTE], messages=async_reply_messages, condition=None)
    return [ipc_receiver, async_reply_receiver] + receivers
