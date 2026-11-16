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
import re

from collections import Counter, defaultdict
from .opaque_ipc_types import is_opaque_type, opaque_ipc_types
from .swift_types import UnmappableSwiftType, strip_cpp_type_decorations, swift_type_and_coder
from .untrusted_origins import conveys_untrusted_value, is_privileged_receiver, unwrap_if_untrusted, unwrap_untrusted

BUILTIN_ATTRIBUTE = "Builtin"
MAINTHREADCALLBACK_ATTRIBUTE = "MainThreadCallback"
ANYTHREADCALLBACK_ATTRIBUTE = "AnyThread"
CALL_WITH_REPLY_ID_ATTRIBUTE = "CallWithReplyID"
ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE = "AllowedWhenWaitingForSyncReply"
ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE = "AllowedWhenWaitingForSyncReplyDuringUnboundedIPC"
SYNCHRONOUS_ATTRIBUTE = 'Synchronous'
STREAM_ATTRIBUTE = "Stream"
SWIFT_TYPES_ATTRIBUTE = 'SwiftTypes'
CXX_TYPES_ATTRIBUTE = 'CxxTypes'
NOT_USING_IPC_CONNECTION_ATTRIBUTE = 'NotUsingIPCConnection'
WANTS_DISPATCH_MESSAGE_ATTRIBUTE = 'WantsDispatchMessage'
WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE = 'WantsAsyncDispatchMessage'

# Names the generated Swift dispatch of a message that takes Swift types gives its own locals and
# stored properties, so a parameter cannot have them.
_SWIFT_TYPES_RESERVED_PARAMETER_NAMES = frozenset(['connection', 'decoder', 'replyEncoder', 'reply', 'target'])

class MessageReceiver(object):
    def __init__(self, name, superclass, attributes, receiver_enabled_by, receiver_enabled_by_exception, receiver_enabled_by_conjunction, receiver_dispatched_from, receiver_dispatched_from_exception, receiver_dispatched_to, receiver_dispatched_to_exception, shared_preferences_needs_connection, messages, condition, namespace, swift_receiver, swift_receiver_build_enabled_by, receiver_name, swift_types=False):
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
        self.swift_receiver = swift_receiver
        self.swift_receiver_build_enabled_by = swift_receiver_build_enabled_by
        self.receiver_name = receiver_name
        self.swift_types = swift_types

    def iterparameters(self):
        return itertools.chain((parameter for message in self.messages for parameter in message.parameters),
            (reply_parameter for message in self.messages if message.reply_parameters for reply_parameter in message.reply_parameters))

    def has_attribute(self, attribute):
        return attribute in self.attributes

    def is_swift_receiver(self):
        return bool(self.swift_receiver or self.swift_receiver_build_enabled_by)

    def takes_swift_types(self, message):
        """Whether the Swift receiver decodes this message's arguments, and encodes its reply, as Swift types."""
        if message.is_async_reply or not self.is_swift_receiver():
            return False
        if message.has_attribute(SWIFT_TYPES_ATTRIBUTE):
            return True
        if message.has_attribute(CXX_TYPES_ATTRIBUTE):
            return False
        return self.swift_types

    def enforce_swift_types_constraints(self):
        if self.swift_types and not self.is_swift_receiver():
            raise Exception("ERROR: Receiver %s has 'SwiftTypes' but is not a Swift receiver: it also needs 'SwiftReceiver' or 'SwiftReceiverBuildEnabledBy='" % self.name)
        if self.swift_types and self.has_attribute(STREAM_ATTRIBUTE):
            raise Exception("ERROR: Receiver %s has 'SwiftTypes', which stream receivers do not support" % self.name)
        for message in self.messages:
            self._enforce_swift_types_constraints_for_message(message)

    def _enforce_swift_types_constraints_for_message(self, message):
        description = '%s.%s' % (self.name, message.name)
        if message.has_attribute(SWIFT_TYPES_ATTRIBUTE) and message.has_attribute(CXX_TYPES_ATTRIBUTE):
            raise Exception("ERROR: Message %s cannot have both 'SwiftTypes' and 'CxxTypes'" % description)
        if message.has_attribute(SWIFT_TYPES_ATTRIBUTE) and not self.is_swift_receiver():
            raise Exception("ERROR: Message %s has 'SwiftTypes' but receiver %s is not a Swift receiver: it needs 'SwiftReceiver' or 'SwiftReceiverBuildEnabledBy='" % (description, self.name))
        if message.has_attribute(CXX_TYPES_ATTRIBUTE) and not self.swift_types:
            raise Exception("ERROR: Message %s has 'CxxTypes' but receiver %s does not have 'SwiftTypes'" % (description, self.name))
        if not self.takes_swift_types(message):
            return

        def unsupported(what):
            return Exception("ERROR: Message %s takes Swift types, which is not supported %s" % (description, what))

        if self.has_attribute(STREAM_ATTRIBUTE):
            raise unsupported('on a stream receiver')
        for attribute in (NOT_USING_IPC_CONNECTION_ATTRIBUTE, WANTS_DISPATCH_MESSAGE_ATTRIBUTE, WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE):
            if self.has_attribute(attribute):
                raise unsupported("on a receiver with '%s'" % attribute)
        if message.has_attribute(CALL_WITH_REPLY_ID_ATTRIBUTE):
            raise unsupported("together with '%s'" % CALL_WITH_REPLY_ID_ATTRIBUTE)
        if message.validator:
            raise unsupported("together with 'Validator=%s'" % message.validator)
        if message.has_attribute(SYNCHRONOUS_ATTRIBUTE) and message.reply_parameters is None:
            raise unsupported("for a '%s' message without a reply" % SYNCHRONOUS_ATTRIBUTE)

        for parameter in message.parameters:
            if parameter.name in _SWIFT_TYPES_RESERVED_PARAMETER_NAMES:
                raise Exception("ERROR: Message %s takes Swift types, so cannot have a parameter named '%s', which the generated Swift uses itself" % (description, parameter.name))
        parameters = [('parameter', parameter) for parameter in message.parameters] + [('reply parameter', parameter) for parameter in message.reply_parameters or []]
        for kind, parameter in parameters:
            if parameter.attributes:
                raise unsupported("with attributes on %s '%s': %s" % (kind, parameter.name, ', '.join(sorted(parameter.attributes))))
            if unwrap_untrusted(parameter.type):
                raise unsupported("with an IPC::Untrusted<> %s '%s'" % (kind, parameter.name))
            try:
                swift_type_and_coder(parameter.type)
            except UnmappableSwiftType as error:
                raise Exception("ERROR: Message %s takes Swift types, but cannot take %s '%s': %s" % (description, kind, parameter.name, error))
        for parameter in message.reply_parameters or []:
            # The reply to a message found to be invalid is built from default-constructed values.
            if re.match(r'(?:WTF::)?Ref<', strip_cpp_type_decorations(parameter.type)):
                raise Exception("ERROR: Message %s takes Swift types, so its reply parameter '%s' cannot be a Ref<>, which has no default value for the reply to an invalid message: use RefPtr<> or std::optional<> instead" % (description, parameter.name))

    def enforce_attribute_constraints(self):
        if not self.receiver_dispatched_from and not self.receiver_dispatched_from_exception:
            raise Exception("ERROR: %s not annotated with 'DispatchedFrom=' attribute" % self.name)
        if not self.receiver_dispatched_to and not self.receiver_dispatched_to_exception:
            raise Exception("ERROR: %s not annotated with 'DispatchedTo=' attribute" % self.name)

    def enforce_opaque_ipc_types_usage(self):
        for message in self.messages:
            for parameter in message.parameters:
                # An opaque type stays opaque inside IPC::Untrusted<>, so look through the wrapper.
                parameter_type = unwrap_if_untrusted(parameter.type)
                if is_opaque_type(parameter_type):
                    if not opaque_ipc_types.message_param_tracked(self.name, message.name, parameter.name, parameter_type):
                        raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] MessageParam {self.name}.{message.name} {parameter.name} {parameter_type}")
            if message.reply_parameters is not None:
                for parameter in message.reply_parameters:
                    parameter_type = unwrap_if_untrusted(parameter.type)
                    if is_opaque_type(parameter_type):
                        if not opaque_ipc_types.message_param_reply_tracked(self.name, message.name, parameter.name, parameter_type):
                            raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] MessageParamReply {self.name}.{message.name} {parameter.name} {parameter_type}")

    def enforce_untrusted_origin_usage(self):
        """A privileged process must not be handed a bare origin or URL by web content.
        """
        if not is_privileged_receiver(self):
            return
        for message in self.messages:
            for parameter in message.parameters:
                untrusted_type = conveys_untrusted_value(parameter.type)
                if untrusted_type is None or unwrap_untrusted(parameter.type):
                    continue
                raise Exception(
                    f"{self.name}.{message.name} passes {untrusted_type} from web content into the "
                    f"{self.receiver_dispatched_to} process as a bare value. Declare the parameter as "
                    f"IPC::Untrusted<{parameter.type}> and either validate it with one of the "
                    f"designated validation procedures or call unsafeExtractWithoutValidation() with a reason.")


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


ipc_receiver = MessageReceiver(name="IPC", superclass=None, attributes=[BUILTIN_ATTRIBUTE], receiver_enabled_by=None, receiver_enabled_by_exception=False, receiver_enabled_by_conjunction=None, receiver_dispatched_from=None, receiver_dispatched_from_exception=None, receiver_dispatched_to=None, receiver_dispatched_to_exception=None, shared_preferences_needs_connection=False, swift_receiver=False, swift_receiver_build_enabled_by=None, receiver_name=None, messages=[
    Message('WrappedAsyncMessageForTesting', [], None, attributes=[BUILTIN_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE], condition=None),
    Message('SyncMessageReply', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('CancelSyncMessageReply', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('CancelAsyncReply', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('InitializeConnection', [], None, attributes=[BUILTIN_ATTRIBUTE], condition="PLATFORM(COCOA)"),
    Message('LegacySessionState', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('SetStreamDestinationID', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('ProcessOutOfStreamMessage', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
    Message('InitializeStreamClientConnection', [], None, attributes=[BUILTIN_ATTRIBUTE], condition=None),
], condition=None, namespace="WebKit")


def check_global_model_inputs(receivers):
    errors = []
    receiver_counts = Counter([r.name for r in receivers])
    receiver_duplicates = [n for n, c in receiver_counts.items() if c > 1]
    if receiver_duplicates:
        errors.append('Duplicate message receiver names: %s' % (', '.join(receiver_duplicates)))

    # A message might be defined multiple times using ifdef conditions.
    # Certain attributes must match in this case. E.g. USE(COCOA) cannot have a sync message that
    # would be non-sync in USE(GTK).
    matching_attributes = [SYNCHRONOUS_ATTRIBUTE, SWIFT_TYPES_ATTRIBUTE, CXX_TYPES_ATTRIBUTE]
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
