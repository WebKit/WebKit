# Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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

from collections import Counter, defaultdict
from .opaque_ipc_types import is_opaque_type, opaque_ipc_types

BUILTIN_ATTRIBUTE = "Builtin"
MAINTHREADCALLBACK_ATTRIBUTE = "MainThreadCallback"
CALL_WITH_REPLY_ID_ATTRIBUTE = "CallWithReplyID"
ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE = "AllowedWhenWaitingForSyncReply"
ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE = "AllowedWhenWaitingForSyncReplyDuringUnboundedIPC"
SYNCHRONOUS_ATTRIBUTE = 'Synchronous'
STREAM_ATTRIBUTE = "Stream"

class MessageReceiver(object):
    def __init__(self, name, superclass, attributes, receiver_enabled_by, receiver_enabled_by_exception, receiver_enabled_by_conjunction, receiver_dispatched_from, receiver_dispatched_from_exception, receiver_dispatched_to, receiver_dispatched_to_exception, shared_preferences_needs_connection, messages, condition, namespace, wants_send_cancel_reply):
        self.name = name
        self.superclass = superclass
        self.attributes = frozenset(attributes or [])
        self.receiver_enabled_by = receiver_enabled_by
        self.receiver_enabled_by_exception = receiver_enabled_by_exception
        self.receiver_enabled_by_conjunction = receiver_enabled_by_conjunction
        self.receiver_dispatched_from = receiver_dispatched_from
        self.receiver_dispatched_from_exception = receiver_dispatched_from_exception
        self.receiver_dispatched_to = receiver_dispatched_to
        self.receiver_dispatched_to_exception = receiver_dispatched_to_exception
        self.shared_preferences_needs_connection = shared_preferences_needs_connection
        self.messages = messages
        self.condition = condition
        self.namespace = namespace
        self.wants_send_cancel_reply = wants_send_cancel_reply

    def iterparameters(self):
        return itertools.chain((parameter for message in self.messages for parameter in message.parameters),
            (reply_parameter for message in self.messages if message.reply_parameters for reply_parameter in message.reply_parameters))

    def has_attribute(self, attribute):
        return attribute in self.attributes

    def enforce_attribute_constraints(self):
        if not self.receiver_dispatched_from and not self.receiver_dispatched_from_exception:
            raise Exception("ERROR: %s not annotated with 'DispatchedFrom=' attribute" % self.name)
        if not self.receiver_dispatched_to and not self.receiver_dispatched_to_exception:
            raise Exception("ERROR: %s not annotated with 'DispatchedTo=' attribute" % self.name)

    def enforce_opaque_ipc_types_usage(self):
        for message in self.messages:
            for parameter in message.parameters:
                if is_opaque_type(parameter.type):
                    if not opaque_ipc_types.message_param_tracked(self.name, message.name, parameter.name, parameter.type):
                        raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] MessageParam {self.name}.{message.name} {parameter.name} {parameter.type}")
            if message.reply_parameters is not None:
                for parameter in message.reply_parameters:
                    if is_opaque_type(parameter.type):
                        if not opaque_ipc_types.message_param_reply_tracked(self.name, message.name, parameter.name, parameter.type):
                            raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] MessageParamReply {self.name}.{message.name} {parameter.name} {parameter.type}")


class Message(object):
    def __init__(self, name, parameters, reply_parameters, attributes, condition, validator=None, enabled_by=None, enabled_by_exception=False, enabled_by_conjunction=None, coalescing_key_indices=None, is_async_reply=False):
        self.name = name
        self.parameters = parameters
        self.reply_parameters = reply_parameters
        self.attributes = frozenset(attributes or [])
        self.condition = condition
        self.validator = validator
        self.enabled_by = enabled_by
        self.enabled_by_exception = enabled_by_exception
        self.enabled_by_conjunction = enabled_by_conjunction
        self.coalescing_key_indices = coalescing_key_indices
        self.is_async_reply = is_async_reply

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


ipc_receiver = MessageReceiver(name="IPC", superclass=None, attributes=[BUILTIN_ATTRIBUTE], receiver_enabled_by=None, receiver_enabled_by_exception=False, receiver_enabled_by_conjunction=None, receiver_dispatched_from=None, receiver_dispatched_from_exception=None, receiver_dispatched_to=None, receiver_dispatched_to_exception=None, shared_preferences_needs_connection=False, messages=[
    Message('WrappedAsyncMessageForTesting', [], [], attributes=[BUILTIN_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE], condition=None),
    Message('SyncMessageReply', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('CancelSyncMessageReply', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('InitializeConnection', [], [], attributes=[BUILTIN_ATTRIBUTE], condition="PLATFORM(COCOA)"),
    Message('LegacySessionState', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('SetStreamDestinationID', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('ProcessOutOfStreamMessage', [], [], attributes=[BUILTIN_ATTRIBUTE], condition=None),
], condition=None, namespace="WebKit", wants_send_cancel_reply=False)


def check_global_model_inputs(receivers):
    errors = []
    receiver_counts = Counter([r.name for r in receivers])
    receiver_duplicates = [n for n, c in receiver_counts.items() if c > 1]
    if receiver_duplicates:
        errors.append('Duplicate message receiver names: %s' % (', '.join(receiver_duplicates)))

    # A message might be defined multiple times using ifdef conditions.
    # Certain attributes must match in this case. E.g. USE(COCOA) cannot have a sync message that
    # would be non-sync in USE(GTK).
    matching_attributes = [SYNCHRONOUS_ATTRIBUTE]
    for receiver in receivers:
        receiver_messages = defaultdict(list)
        for message in receiver.messages:
            receiver_messages[message.name].append(message)
        for messages in receiver_messages.values():
            m0 = messages[0]
            for i in range(1, len(messages)):
                mi = messages[i]
                if any(m0.has_attribute(a) != mi.has_attribute(a) for a in matching_attributes):
                    errors.append('Receiver %s message %s attribute mismatch: %s (%s) != %s (%s))' % (receiver.name, message.name,
                                  m0.attributes, m0.condition, mi.attributes, mi.condition))
    return errors


def generate_global_model(receivers):
    for receiver in receivers:
        async_reply_messages = []
        for message in receiver.messages:
            if message.reply_parameters is not None and not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
                async_reply_messages.append(Message(name='%sReply' % message.name, parameters=message.reply_parameters, reply_parameters=None, attributes=None, condition=message.condition, enabled_by=message.enabled_by, enabled_by_exception=message.enabled_by_exception, enabled_by_conjunction=message.enabled_by_conjunction, coalescing_key_indices=message.coalescing_key_indices, is_async_reply=True))
        receiver.messages = receiver.messages + async_reply_messages

    return [ipc_receiver] + receivers
