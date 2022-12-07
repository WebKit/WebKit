# Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

import collections
import re
import sys

from webkit import parser
from webkit.model import BUILTIN_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE, MAINTHREADCALLBACK_ATTRIBUTE, STREAM_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, WANTS_CONNECTION_ATTRIBUTE, MessageReceiver, Message

_license_header = """/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"""

WANTS_DISPATCH_MESSAGE_ATTRIBUTE = 'WantsDispatchMessage'
WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE = 'WantsAsyncDispatchMessage'
LEGACY_RECEIVER_ATTRIBUTE = 'LegacyReceiver'
NOT_REFCOUNTED_RECEIVER_ATTRIBUTE = 'NotRefCounted'
NOT_STREAM_ENCODABLE_ATTRIBUTE = 'NotStreamEncodable'
NOT_STREAM_ENCODABLE_REPLY_ATTRIBUTE = 'NotStreamEncodableReply'
STREAM_BATCHED_ATTRIBUTE = 'StreamBatched'

attributes_to_generate_validators = {
    "messageAllowedWhenWaitingForSyncReply": [ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, STREAM_ATTRIBUTE],
    "messageAllowedWhenWaitingForUnboundedSyncReply": [ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE],
}

def receiver_enumerator_order_key(receiver_name):
    if receiver_name == 'IPC':
        return 1
    elif receiver_name == 'AsyncReply':
        return 2
    return 0


class MessageEnumerator(object):

    def __init__(self, receiver, messages):
        self.receiver = receiver
        self.messages = messages

    def __str__(self):
        if self.messages[0].has_attribute(BUILTIN_ATTRIBUTE):
            return self.messages[0].name
        if self.receiver.name == 'AsyncReply':
            return self.messages[0].name
        return '%s_%s' % (self.receiver.name, self.messages[0].name)

    @classmethod
    def sort_key(cls, obj):
        return obj.messages[0].has_attribute(SYNCHRONOUS_ATTRIBUTE), receiver_enumerator_order_key(obj.receiver.name), str(obj)


def get_message_enumerators(receivers):
    enumerators = []
    for receiver in receivers:
        receiver_enumerators = {}
        for message in receiver.messages:
            if message.name not in receiver_enumerators:
                receiver_enumerators[message.name] = MessageEnumerator(receiver, messages=[message])
            else:
                receiver_enumerators[message.name].messages.append(message)
        enumerators += receiver_enumerators.values()
    assert(len(enumerators) == len(set(enumerators)))
    return sorted(enumerators, key=MessageEnumerator.sort_key)


def get_receiver_enumerators(receivers):
    return sorted((r.name for r in receivers), key=lambda n: (receiver_enumerator_order_key(n), n))


def messages_header_filename(receiver):
    return '%sMessages.h' % receiver.name


def surround_in_condition(string, condition):
    if not condition:
        return string
    return '#if %s\n%s#endif\n' % (condition, string)


def function_parameter_type(type, kind):
    # Don't use references for built-in types.
    builtin_types = frozenset([
        'bool',
        'float',
        'double',
        'uint8_t',
        'uint16_t',
        'uint32_t',
        'uint64_t',
        'int8_t',
        'int16_t',
        'int32_t',
        'int64_t',
        'size_t',
    ])

    if type in builtin_types:
        return type

    if kind.startswith('enum:'):
        return type

    return 'const %s&' % type


def arguments_type(message):
    return 'std::tuple<%s>' % ', '.join(function_parameter_type(parameter.type, parameter.kind) for parameter in message.parameters)


def reply_arguments_type(message):
    return 'std::tuple<%s>' % (', '.join(parameter.type for parameter in message.reply_parameters))


def message_to_struct_declaration(receiver, message):
    result = []
    function_parameters = [(function_parameter_type(x.type, x.kind), x.name) for x in message.parameters]

    result.append('class %s {\n' % message.name)
    result.append('public:\n')
    result.append('    using Arguments = %s;\n' % arguments_type(message))
    result.append('\n')
    result.append('    static IPC::MessageName name() { return IPC::MessageName::%s_%s; }\n' % (receiver.name, message.name))
    result.append('    static constexpr bool isSync = %s;\n' % ('false', 'true')[message.reply_parameters is not None and message.has_attribute(SYNCHRONOUS_ATTRIBUTE)])
    if receiver.has_attribute(STREAM_ATTRIBUTE):
        result.append('    static constexpr bool isStreamEncodable = %s;\n' % ('true', 'false')[message.has_attribute(NOT_STREAM_ENCODABLE_ATTRIBUTE)])
        if message.reply_parameters is not None:
            result.append('    static constexpr bool isReplyStreamEncodable = %s;\n' % ('true', 'false')[message.has_attribute(NOT_STREAM_ENCODABLE_REPLY_ATTRIBUTE)])
            if message.has_attribute(STREAM_BATCHED_ATTRIBUTE):
                sys.stderr.write("Error: %s::%s has a reply but is marked as batched. Messages with replies are intended to be sent without latency.\n" % (receiver.name, message.name))
                sys.exit(1)
        result.append('    static constexpr bool isStreamBatched = %s;\n' % ('false', 'true')[message.has_attribute(STREAM_BATCHED_ATTRIBUTE)])

    result.append('\n')
    if message.reply_parameters != None:
        if not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
            result.append('    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::%s_%sReply; }\n' % (receiver.name, message.name))
        if message.has_attribute(MAINTHREADCALLBACK_ATTRIBUTE):
            result.append('    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::MainThread;\n')
        else:
            result.append('    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;\n')
        result.append('    using ReplyArguments = %s;\n' % reply_arguments_type(message))

    if len(function_parameters):
        result.append('    %s%s(%s)' % (len(function_parameters) == 1 and 'explicit ' or '', message.name, ', '.join([' '.join(x) for x in function_parameters])))
        result.append('\n        : m_arguments(%s)\n' % ', '.join([x[1] for x in function_parameters]))
        result.append('    {\n')
        result.append('    }\n\n')
    result.append('    const Arguments& arguments() const\n')
    result.append('    {\n')
    result.append('        return m_arguments;\n')
    result.append('    }\n')
    result.append('\n')
    result.append('private:\n')
    result.append('    Arguments m_arguments;\n')
    result.append('};\n')
    return surround_in_condition(''.join(result), message.condition)


def forward_declaration(namespace, kind_and_type):
    kind, type = kind_and_type

    qualified_name = '%s::%s' % (namespace, type)
    if kind == 'struct':
        return 'struct %s' % type
    elif kind.startswith('enum:'):
        return 'enum class %s : %s' % (type, kind[5:])
    else:
        return 'class %s' % type


def forward_declarations_for_namespace(namespace, kind_and_types):
    result = []
    result.append('namespace %s {\n' % namespace)
    result += ['%s;\n' % forward_declaration(namespace, x) for x in sorted(kind_and_types)]
    result.append('}\n')
    return ''.join(result)


# When updating this list, run "make -C Source/WebKit/Scripts/webkit/tests" to keep the webkitpy tests passing.
def serialized_identifiers():
    return [
        'WebCore::BroadcastChannelIdentifier',
        'WebCore::DOMCacheIdentifier',
        'WebCore::DictationContext',
        'WebCore::DisplayList::ItemBufferIdentifier',
        'WebCore::ElementIdentifier',
        'WebCore::FetchIdentifier',
        'WebCore::FileSystemHandleIdentifier',
        'WebCore::FileSystemSyncAccessHandleIdentifier',
        'WebCore::ImageDecoderIdentifier',
        'WebCore::LibWebRTCSocketIdentifier',
        'WebCore::MediaKeySystemRequestIdentifier',
        'WebCore::MediaPlayerIdentifier',
        'WebCore::MediaSessionIdentifier',
        'WebCore::PageIdentifier',
        'WebCore::PlaybackTargetClientContextIdentifier',
        'WebCore::PushSubscriptionIdentifier',
        'WebCore::ProcessIdentifier',
        'WebCore::RealtimeMediaSourceIdentifier',
        'WebCore::RenderingResourceIdentifier',
        'WebCore::ResourceLoaderIdentifier',
        'WebCore::SWServerConnectionIdentifier',
        'WebCore::ServiceWorkerIdentifier',
        'WebCore::ServiceWorkerJobIdentifier',
        'WebCore::ServiceWorkerRegistrationIdentifier',
        'WebCore::SharedWorkerIdentifier',
        'WebCore::SleepDisablerIdentifier',
        'WebCore::SpeechRecognitionConnectionClientIdentifier',
        'WebCore::UserMediaRequestIdentifier',
        'WebCore::WebSocketIdentifier',
        'WebKit::AudioMediaStreamTrackRendererInternalUnitIdentifier',
        'WebKit::AuthenticationChallengeIdentifier',
        'WebKit::ContentWorldIdentifier',
        'WebKit::DataTaskIdentifier',
        'WebKit::FormSubmitListenerIdentifier',
        'WebKit::GeolocationIdentifier',
        'WebKit::GraphicsContextGLIdentifier',
        'WebKit::IPCConnectionTesterIdentifier',
        'WebKit::IPCStreamTesterIdentifier',
        'WebKit::LibWebRTCResolverIdentifier',
        'WebKit::MDNSRegisterIdentifier',
        'WebKit::MarkSurfacesAsVolatileRequestIdentifier',
        'WebKit::MediaRecorderIdentifier',
        'WebKit::NetworkResourceLoadIdentifier',
        'WebKit::PDFPluginIdentifier',
        'WebKit::PageGroupIdentifier',
        'WebKit::PlaybackSessionContextIdentifier',
        'WebKit::QuotaIncreaseRequestIdentifier',
        'WebKit::RemoteAudioDestinationIdentifier',
        'WebKit::RemoteAudioHardwareListenerIdentifier',
        'WebKit::RemoteAudioSessionIdentifier',
        'WebKit::RemoteCDMIdentifier',
        'WebKit::RemoteCDMInstanceIdentifier',
        'WebKit::RemoteCDMInstanceSessionIdentifier',
        'WebKit::RemoteLegacyCDMIdentifier',
        'WebKit::RemoteLegacyCDMSessionIdentifier',
        'WebKit::RemoteMediaResourceIdentifier',
        'WebKit::RemoteVideoFrameIdentifier',
        'WebKit::RemoteRemoteCommandListenerIdentifier',
        'WebKit::RenderingBackendIdentifier',
        'WebKit::SampleBufferDisplayLayerIdentifier',
        'WebKit::StorageAreaIdentifier',
        'WebKit::StorageAreaImplIdentifier',
        'WebKit::StorageAreaMapIdentifier',
        'WebKit::StorageNamespaceIdentifier',
        'WebKit::TapIdentifier',
        'WebKit::TrackPrivateRemoteIdentifier',
        'WebKit::UserContentControllerIdentifier',
        'WebKit::VideoDecoderIdentifier',
        'WebKit::VideoEncoderIdentifier',
        'WebKit::WebExtensionContextIdentifier',
        'WebKit::WebExtensionControllerIdentifier',
        'WebKit::WebGPUIdentifier',
        'WebKit::WebPageProxyIdentifier',
        'WebKit::WebURLSchemeHandlerIdentifier',
    ]


def types_that_cannot_be_forward_declared():
    return frozenset([
        'CVPixelBufferRef',
        'GCGLint',
        'IPC::DataReference',
        'IPC::FilterReference',
        'IPC::FontReference',
        'IPC::Semaphore',
        'MachSendRight',
        'MediaTime',
        'PlatformXR::ReferenceSpaceType',
        'PlatformXR::SessionFeature',
        'PlatformXR::SessionMode',
        'PlatformXR::VisibilityState',
        'String',
        'WebCore::BackForwardItemIdentifier',
        'WebCore::DestinationColorSpace',
        'WebCore::DiagnosticLoggingDomain',
        'WebCore::DictationContext',
        'WebCore::DragApplicationFlags',
        'WebCore::FrameIdentifier',
        'WebCore::GraphicsContextGLAttributes',
        'WebCore::ModalContainerControlType',
        'WebCore::NativeImageReference',
        'WebCore::PluginLoadClientPolicy',
        'WebCore::PointerID',
        'WebCore::PolicyCheckIdentifier',
        'WebCore::RenderingMode',
        'WebCore::RenderingPurpose',
        'WebCore::ScriptExecutionContextIdentifier',
        'WebCore::ServiceWorkerOrClientData',
        'WebCore::ServiceWorkerOrClientIdentifier',
        'WebCore::SharedStringHash',
        'WebCore::SharedWorkerObjectIdentifier',
        'WebCore::SourceBufferAppendMode',
        'WebCore::StorageType',
        'WebCore::TransferredMessagePort',
        'WebCore::WebLockIdentifier',
        'WebKit::ActivityStateChangeID',
        'WebKit::DisplayLinkObserverID',
        'WebKit::DisplayListRecorderFlushIdentifier',
        'WebKit::DownloadID',
        'WebKit::FileSystemStorageError',
        'WebKit::ImageBufferBackendHandle',
        'WebKit::LayerHostingContextID',
        'WebKit::LegacyCustomProtocolID',
        'WebKit::RemoteMediaSourceIdentifier',
        'WebKit::RemoteSourceBufferIdentifier',
        'WebKit::RemoteVideoFrameWriteReference',
        'WebKit::RemoteVideoFrameReadReference',
        'WebKit::RenderingUpdateID',
        'WebKit::TextCheckerRequestID',
        'WebKit::TransactionID',
        'WebKit::WCLayerTreeHostIdentifier',
        'WebKit::WCContentBufferIdentifier',
        'WebKit::WebExtensionEventListenerType',
        'WebKit::XRDeviceIdentifier',
    ] + serialized_identifiers())


def conditions_for_header(header):
    conditions = {
        '"InputMethodState.h"': ["PLATFORM(GTK)", "PLATFORM(WPE)"],
        '"GestureTypes.h"': ["PLATFORM(IOS_FAMILY)"],
        '"SharedCARingBuffer.h"': ["PLATFORM(COCOA)"],
        '"WCContentBufferIdentifier.h"': ["USE(GRAPHICS_LAYER_WC)"],
        '"WCLayerTreeHostIdentifier.h"': ["USE(GRAPHICS_LAYER_WC)"],
        '<WebCore/CVUtilities.h>': ["PLATFORM(COCOA)", ],
        '<WebCore/DataDetectorType.h>': ["ENABLE(DATA_DETECTION)"],
        '<WebCore/MediaPlaybackTargetContext.h>': ["ENABLE(WIRELESS_PLAYBACK_TARGET)"],
        '<WebCore/VideoFrameCV.h>': ["PLATFORM(COCOA)", ],
    }
    if not header in conditions:
        return None
    return conditions[header]


def forward_declarations_and_headers(receiver):
    types_by_namespace = collections.defaultdict(set)

    headers = set([
        '"ArgumentCoders.h"',
        '"Connection.h"',
        '"MessageNames.h"',
        '<wtf/Forward.h>',
        '<wtf/ThreadSafeRefCounted.h>',
    ])

    non_template_wtf_types = frozenset([
        'MachSendRight',
        'MediaType',
        'String',
    ])

    no_forward_declaration_types = types_that_cannot_be_forward_declared()
    for parameter in receiver.iterparameters():
        kind = parameter.kind
        type = parameter.type

        if type.find('<') != -1 or type in no_forward_declaration_types:
            # Don't forward declare class templates.
            headers.update(headers_for_type(type))
            continue

        split = type.split('::')

        # Handle WTF types even if the WTF:: prefix is not given
        if split[0] in non_template_wtf_types:
            split.insert(0, 'WTF')

        if len(split) == 2:
            namespace = split[0]
            inner_type = split[1]
            types_by_namespace[namespace].add((kind, inner_type))
        elif len(split) > 2:
            # We probably have a nested struct, which means we can't forward declare it.
            # Include its header instead.
            headers.update(headers_for_type(type))

    forward_declarations = '\n'.join([forward_declarations_for_namespace(namespace, types) for (namespace, types) in sorted(types_by_namespace.items())])

    header_includes = []
    for header in sorted(headers):
        conditions = conditions_for_header(header)
        if conditions and not None in conditions:
            header_include = '#if %s\n' % ' || '.join(sorted(set(conditions)))
            header_include += '#include %s\n' % header
            header_include += '#endif\n'
            header_includes.append(header_include)
        else:
            header_includes.append('#include %s\n' % header)

    return (forward_declarations, header_includes)


def generate_messages_header(receiver):
    result = []

    result.append(_license_header)

    result.append('#pragma once\n')
    result.append('\n')

    if receiver.condition:
        result.append('#if %s\n\n' % receiver.condition)

    forward_declarations, headers = forward_declarations_and_headers(receiver)

    result += headers
    result.append('\n')

    result.append(forward_declarations)
    result.append('\n')

    result.append('namespace Messages {\nnamespace %s {\n' % receiver.name)
    result.append('\n')
    result.append('static inline IPC::ReceiverName messageReceiverName()\n')
    result.append('{\n')
    result.append('    return IPC::ReceiverName::%s;\n' % receiver.name)
    result.append('}\n')
    result.append('\n')
    result.append('\n'.join([message_to_struct_declaration(receiver, x) for x in receiver.messages]))
    result.append('\n')
    result.append('} // namespace %s\n} // namespace Messages\n' % receiver.name)

    if receiver.condition:
        result.append('\n#endif // %s\n' % receiver.condition)

    return ''.join(result)


def handler_function(receiver, message):
    if message.name.find('URL') == 0:
        return '%s::%s' % (receiver.name, 'url' + message.name[3:])
    return '%s::%s' % (receiver.name, message.name[0].lower() + message.name[1:])


def async_message_statement(receiver, message):
    dispatch_function_args = ['decoder', 'this', '&%s' % handler_function(receiver, message)]

    dispatch_function = 'handleMessage'
    if message.reply_parameters is not None and not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
        dispatch_function += 'Async'

    if message.has_attribute(WANTS_CONNECTION_ATTRIBUTE):
        dispatch_function += 'WantsConnection'

    connection = 'connection'
    if receiver.has_attribute(STREAM_ATTRIBUTE):
        connection = 'connection.connection()'

    result = []
    result.append('    if (decoder.messageName() == Messages::%s::%s::name())\n' % (receiver.name, message.name))
    result.append('        return IPC::%s<Messages::%s::%s>(%s, %s);\n' % (dispatch_function, receiver.name, message.name, connection, ', '.join(dispatch_function_args)))
    return surround_in_condition(''.join(result), message.condition)


def sync_message_statement(receiver, message):
    dispatch_function = 'handleMessage'
    if message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
        dispatch_function += 'Synchronous'
    elif message.reply_parameters is not None:
        dispatch_function += 'Async'
    if message.has_attribute(WANTS_CONNECTION_ATTRIBUTE):
        dispatch_function += 'WantsConnection'

    maybe_reply_encoder = ", *replyEncoder"
    if receiver.has_attribute(STREAM_ATTRIBUTE):
        maybe_reply_encoder = ''
    elif message.reply_parameters is not None:
        maybe_reply_encoder = ', replyEncoder'

    result = []
    result.append('    if (decoder.messageName() == Messages::%s::%s::name())\n' % (receiver.name, message.name))
    result.append('        return IPC::%s<Messages::%s::%s>(connection, decoder%s, this, &%s);\n' % (dispatch_function, receiver.name, message.name, maybe_reply_encoder, handler_function(receiver, message)))
    return surround_in_condition(''.join(result), message.condition)


def class_template_headers(template_string):
    template_string = template_string.strip()

    class_template_types = {
        'WebCore::RectEdges': {'headers': ['<WebCore/RectEdges.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Expected': {'headers': ['<wtf/Expected.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'HashMap': {'headers': ['<wtf/HashMap.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'HashSet': {'headers': ['<wtf/HashSet.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'OptionSet': {'headers': ['<wtf/OptionSet.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Vector': {'headers': ['<wtf/Vector.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::optional': {'headers': ['<optional>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::pair': {'headers': ['<utility>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'IPC::ArrayReference': {'headers': ['"ArrayReference.h"'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'IPC::ArrayReferenceTuple': {'headers': ['"ArrayReferenceTuple.h"'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Ref': {'headers': ['<wtf/Ref.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'RefPtr': {'headers': ['<wtf/RefCounted.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'RetainPtr': {'headers': ['<wtf/RetainPtr.h>'], 'argument_coder_headers': ['"ArgumentCodersCF.h"']},
    }

    match = re.match('(?P<template_name>.+?)<(?P<parameter_string>.+)>', template_string)
    if not match:
        return {'header_infos': [], 'types': [template_string]}

    template_name = match.groupdict()['template_name']
    if template_name not in class_template_types:
        sys.stderr.write("Error: no class template type is defined for '%s'\n" % (template_string))
        sys.exit(1)

    header_infos = [class_template_types[template_name]]
    types = []

    for parameter in parser.split_parameters_string(match.groupdict()['parameter_string']):
        parameter_header_infos_and_types = class_template_headers(parameter)

        header_infos += parameter_header_infos_and_types['header_infos']
        types += parameter_header_infos_and_types['types']

    return {'header_infos': header_infos, 'types': types}


def argument_coder_headers_for_type(type):
    header_infos_and_types = class_template_headers(type)

    special_cases = {
        'IPC::Connection::Handle': '"Connection.h"',
        'String': '"ArgumentCoders.h"',
        'MachSendRight': '"ArgumentCodersDarwin.h"',
        'WebKit::ScriptMessageHandlerHandle': '"WebScriptMessageHandler.h"',
    }

    headers = []
    for header_info in header_infos_and_types['header_infos']:
        headers += header_info['argument_coder_headers']

    for type in header_infos_and_types['types']:
        if type in special_cases:
            headers.append(special_cases[type])
            continue

        split = type.split('::')
        if len(split) < 2:
            continue
        if split[0] == 'WebCore':
            headers.append('"WebCoreArgumentCoders.h"')

    return headers


def headers_for_type(type):
    header_infos_and_types = class_template_headers(type)

    special_cases = {
        'CVPixelBufferRef': ['<WebCore/CVUtilities.h>'],
        'GCGLint': ['<WebCore/GraphicsTypesGL.h>'],
        'IPC::Semaphore': ['"IPCSemaphore.h"'],
        'Inspector::ExtensionError': ['"InspectorExtensionTypes.h"'],
        'Inspector::FrontendChannel::ConnectionType': ['<JavaScriptCore/InspectorFrontendChannel.h>'],
        'Inspector::InspectorTargetType': ['<JavaScriptCore/InspectorTarget.h>'],
        'JSC::MessageLevel': ['<JavaScriptCore/ConsoleTypes.h>'],
        'JSC::MessageSource': ['<JavaScriptCore/ConsoleTypes.h>'],
        'MachSendRight': ['<wtf/MachSendRight.h>'],
        'MediaTime': ['<wtf/MediaTime.h>'],
        'MonotonicTime': ['<wtf/MonotonicTime.h>'],
        'PAL::SessionID': ['<pal/SessionID.h>'],
        'PAL::WebGPU::AddressMode': ['<pal/graphics/WebGPU/WebGPUAddressMode.h>'],
        'PAL::WebGPU::BlendFactor': ['<pal/graphics/WebGPU/WebGPUBlendFactor.h>'],
        'PAL::WebGPU::BlendOperation': ['<pal/graphics/WebGPU/WebGPUBlendOperation.h>'],
        'PAL::WebGPU::BufferBindingType': ['<pal/graphics/WebGPU/WebGPUBufferBindingType.h>'],
        'PAL::WebGPU::BufferDynamicOffset': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::BufferUsageFlags': ['<pal/graphics/WebGPU/WebGPUBufferUsage.h>'],
        'PAL::WebGPU::CanvasCompositingAlphaMode': ['<pal/graphics/WebGPU/WebGPUCanvasCompositingAlphaMode.h>'],
        'PAL::WebGPU::ColorWriteFlags': ['<pal/graphics/WebGPU/WebGPUColorWrite.h>'],
        'PAL::WebGPU::CompareFunction': ['<pal/graphics/WebGPU/WebGPUCompareFunction.h>'],
        'PAL::WebGPU::CompilationMessageType': ['<pal/graphics/WebGPU/WebGPUCompilationMessageType.h>'],
        'PAL::WebGPU::ComputePassTimestampLocation': ['<pal/graphics/WebGPU/WebGPUComputePassTimestampLocation.h>'],
        'PAL::WebGPU::CullMode': ['<pal/graphics/WebGPU/WebGPUCullMode.h>'],
        'PAL::WebGPU::DepthBias': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::DeviceLostReason': ['<pal/graphics/WebGPU/WebGPUDeviceLostReason.h>'],
        'PAL::WebGPU::ErrorFilter': ['<pal/graphics/WebGPU/WebGPUErrorFilter.h>'],
        'PAL::WebGPU::FeatureName': ['<pal/graphics/WebGPU/WebGPUFeatureName.h>'],
        'PAL::WebGPU::FilterMode': ['<pal/graphics/WebGPU/WebGPUFilterMode.h>'],
        'PAL::WebGPU::FlagsConstant': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::FrontFace': ['<pal/graphics/WebGPU/WebGPUFrontFace.h>'],
        'PAL::WebGPU::Index32': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::IndexFormat': ['<pal/graphics/WebGPU/WebGPUIndexFormat.h>'],
        'PAL::WebGPU::IntegerCoordinate': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::LoadOp': ['<pal/graphics/WebGPU/WebGPULoadOp.h>'],
        'PAL::WebGPU::MapModeFlags': ['<pal/graphics/WebGPU/WebGPUMapMode.h>'],
        'PAL::WebGPU::PipelineStatisticName': ['<pal/graphics/WebGPU/WebGPUPipelineStatisticName.h>'],
        'PAL::WebGPU::PowerPreference': ['<pal/graphics/WebGPU/WebGPUPowerPreference.h>'],
        'PAL::WebGPU::PredefinedColorSpace': ['<pal/graphics/WebGPU/WebGPUPredefinedColorSpace.h>'],
        'PAL::WebGPU::PrimitiveTopology': ['<pal/graphics/WebGPU/WebGPUPrimitiveTopology.h>'],
        'PAL::WebGPU::QueryType': ['<pal/graphics/WebGPU/WebGPUQueryType.h>'],
        'PAL::WebGPU::RenderPassTimestampLocation': ['<pal/graphics/WebGPU/WebGPURenderPassTimestampLocation.h>'],
        'PAL::WebGPU::SampleMask': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::SamplerBindingType': ['<pal/graphics/WebGPU/WebGPUSamplerBindingType.h>'],
        'PAL::WebGPU::ShaderStageFlags': ['<pal/graphics/WebGPU/WebGPUShaderStage.h>'],
        'PAL::WebGPU::SignedOffset32': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::Size32': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::Size64': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::StencilOperation': ['<pal/graphics/WebGPU/WebGPUStencilOperation.h>'],
        'PAL::WebGPU::StencilValue': ['<pal/graphics/WebGPU/WebGPUIntegralTypes.h>'],
        'PAL::WebGPU::StorageTextureAccess': ['<pal/graphics/WebGPU/WebGPUStorageTextureAccess.h>'],
        'PAL::WebGPU::StoreOp': ['<pal/graphics/WebGPU/WebGPUStoreOp.h>'],
        'PAL::WebGPU::TextureAspect': ['<pal/graphics/WebGPU/WebGPUTextureAspect.h>'],
        'PAL::WebGPU::TextureDimension': ['<pal/graphics/WebGPU/WebGPUTextureDimension.h>'],
        'PAL::WebGPU::TextureFormat': ['<pal/graphics/WebGPU/WebGPUTextureFormat.h>'],
        'PAL::WebGPU::TextureSampleType': ['<pal/graphics/WebGPU/WebGPUTextureSampleType.h>'],
        'PAL::WebGPU::TextureUsageFlags': ['<pal/graphics/WebGPU/WebGPUTextureUsage.h>'],
        'PAL::WebGPU::TextureViewDimension': ['<pal/graphics/WebGPU/WebGPUTextureViewDimension.h>'],
        'PAL::WebGPU::VertexFormat': ['<pal/graphics/WebGPU/WebGPUVertexFormat.h>'],
        'PAL::WebGPU::VertexStepMode': ['<pal/graphics/WebGPU/WebGPUVertexStepMode.h>'],
        'PlatformXR::Device::FrameData': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::ReferenceSpaceType': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::SessionFeature': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::SessionMode': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::VisibilityState': ['<WebCore/PlatformXR.h>'],
        'Seconds': ['<wtf/Seconds.h>'],
        'String': ['<wtf/text/WTFString.h>'],
        'URL': ['<wtf/URLHash.h>'],
        'UUID': ['<wtf/UUID.h>'],
        'WallTime': ['<wtf/WallTime.h>'],
        'WebCore::ApplyTrackingPrevention': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::ArcData': ['<WebCore/InlinePathData.h>'],
        'WebCore::AutoplayEventFlags': ['<WebCore/AutoplayEvent.h>'],
        'WebCore::BackForwardItemIdentifier': ['<WebCore/ProcessQualified.h>', '<WebCore/BackForwardItemIdentifier.h>', '<wtf/ObjectIdentifier.h>'],
        'WebCore::BezierCurveData': ['<WebCore/InlinePathData.h>'],
        'WebCore::BlendMode': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::BrowsingContextGroupSwitchDecision': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::AlternativeTextType': ['<WebCore/AlternativeTextClient.h>'],
        'WebCore::COEPDisposition': ['<WebCore/CrossOriginEmbedderPolicy.h>'],
        'WebCore::COOPDisposition': ['<WebCore/CrossOriginOpenerPolicy.h>'],
        'WebCore::ColorSchemePreference': ['<WebCore/DocumentLoader.h>'],
        'WebCore::CompositeOperator': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::CreateNewGroupForHighlight': ['<WebCore/AppHighlight.h>'],
        'WebCore::DOMPasteAccessCategory': ['<WebCore/DOMPasteAccess.h>'],
        'WebCore::DOMPasteAccessResponse': ['<WebCore/DOMPasteAccess.h>'],
        'WebCore::DictationContext': ['<WebCore/DictationContext.h>'],
        'WebCore::DisplayList::ItemBufferIdentifier': ['<WebCore/DisplayList.h>'],
        'WebCore::DocumentMarkerLineStyle': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::DragApplicationFlags': ['<WebCore/DragData.h>'],
        'WebCore::DragHandlingMethod': ['<WebCore/DragActions.h>'],
        'WebCore::DragOperation': ['<WebCore/DragActions.h>'],
        'WebCore::DragSourceAction': ['<WebCore/DragActions.h>'],
        'WebCore::DynamicRangeMode': ['<WebCore/PlatformScreen.h>'],
        'WebCore::EventMakesGamepadsVisible': ['<WebCore/GamepadProviderClient.h>'],
        'WebCore::ExceptionDetails': ['<WebCore/JSDOMExceptionHandling.h>'],
        'WebCore::FileChooserSettings': ['<WebCore/FileChooser.h>'],
        'WebCore::FirstPartyWebsiteDataRemovalMode': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::FontChanges': ['<WebCore/FontAttributeChanges.h>'],
        'WebCore::FontSmoothingMode': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::FrameLoadType': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::GenericCueData': ['<WebCore/InbandGenericCue.h>'],
        'WebCore::GrammarDetail': ['<WebCore/TextCheckerClient.h>'],
        'WebCore::GraphicsContextGLActiveInfo': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::HasInsecureContent': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::HighlightRequestOriginatedInApp': ['<WebCore/AppHighlight.h>'],
        'WebCore::HighlightVisibility': ['<WebCore/HighlightVisibility.h>'],
        'WebCore::ISOWebVTTCue': ['<WebCore/ISOVTTCue.h>'],
        'WebCore::IncludeSecureCookies': ['<WebCore/CookieJar.h>'],
        'WebCore::IndexedDB::ObjectStoreOverwriteMode': ['<WebCore/IndexedDB.h>'],
        'WebCore::InputMode': ['<WebCore/InputMode.h>'],
        'WebCore::InspectorOverlay::Highlight': ['<WebCore/InspectorOverlay.h>'],
        'WebCore::KeyframeValueList': ['<WebCore/GraphicsLayer.h>'],
        'WebCore::KeypressCommand': ['<WebCore/KeyboardEvent.h>'],
        'WebCore::LastNavigationWasAppInitiated': ['<WebCore/ServiceWorkerClientData.h>'],
        'WebCore::LegacyCDMSessionClient::MediaKeyErrorCode': ['<WebCore/LegacyCDMSession.h>'],
        'WebCore::LineCap': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::LineData': ['<WebCore/InlinePathData.h>'],
        'WebCore::LineJoin': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::LockBackForwardList': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::MediaEngineSupportParameters': ['<WebCore/MediaPlayer.h>'],
        'WebCore::MediaProducerMediaCaptureKind': ['<WebCore/MediaProducer.h>'],
        'WebCore::MediaProducerMediaState': ['<WebCore/MediaProducer.h>'],
        'WebCore::MediaProducerMutedState': ['<WebCore/MediaProducer.h>'],
        'WebCore::MessagePortChannelProvider::HasActivity': ['<WebCore/MessagePortChannelProvider.h>'],
        'WebCore::ModalContainerControlType': ['<WebCore/ModalContainerTypes.h>'],
        'WebCore::ModalContainerDecision': ['<WebCore/ModalContainerTypes.h>'],
        'WebCore::MouseEventPolicy': ['<WebCore/DocumentLoader.h>'],
        'WebCore::MoveData': ['<WebCore/InlinePathData.h>'],
        'WebCore::NetworkTransactionInformation': ['<WebCore/NetworkLoadInformation.h>'],
        'WebCore::PasteboardCustomData': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardImage': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardURL': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardWebContent': ['<WebCore/Pasteboard.h>'],
        'WebCore::PixelFormat': ['<WebCore/ImageBufferBackend.h>'],
        'WebCore::PlatformTextTrackData': ['<WebCore/PlatformTextTrack.h>'],
        'WebCore::PlaybackSessionModel::PlaybackState': ['<WebCore/PlaybackSessionModel.h>'],
        'WebCore::PluginInfo': ['<WebCore/PluginData.h>'],
        'WebCore::PluginLoadClientPolicy': ['<WebCore/PluginData.h>'],
        'WebCore::PolicyAction': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::PolicyCheckIdentifier': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::PreserveResolution': ['<WebCore/ImageBufferBackend.h>'],
        'WebCore::ProcessIdentifier': ['<WebCore/ProcessIdentifier.h>'],
        'WebCore::PushSubscriptionIdentifier': ['<WebCore/PushSubscriptionIdentifier.h>'],
        'WebCore::QuadCurveData': ['<WebCore/InlinePathData.h>'],
        'WebCore::RecentSearch': ['<WebCore/SearchPopupMenu.h>'],
        'WebCore::ReloadOption': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::RenderingPurpose': ['<WebCore/RenderingMode.h>'],
        'WebCore::RequestStorageAccessResult': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::RouteSharingPolicy': ['<WebCore/AudioSession.h>'],
        'WebCore::SWServerConnectionIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::SameSiteStrictEnforcementEnabled': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::ScriptExecutionContextIdentifier': ['<WebCore/ProcessQualified.h>', '<WebCore/ScriptExecutionContextIdentifier.h>', '<wtf/ObjectIdentifier.h>'],
        'WebCore::ScrollGranularity': ['<WebCore/ScrollTypes.h>'],
        'WebCore::SecurityPolicyViolationEventInit': ['<WebCore/SecurityPolicyViolationEvent.h>'],
        'WebCore::SelectionDirection': ['<WebCore/VisibleSelection.h>'],
        'WebCore::SelectionGeometry': ['"EditorState.h"'],
        'WebCore::ServiceWorkerJobIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerOrClientData': ['<WebCore/ServiceWorkerTypes.h>', '<WebCore/ServiceWorkerClientData.h>', '<WebCore/ServiceWorkerData.h>'],
        'WebCore::ServiceWorkerOrClientIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerRegistrationIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerRegistrationState': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerState': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ShareDataWithParsedURL': ['<WebCore/ShareData.h>'],
        'WebCore::ShouldContinuePolicyCheck': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::ShouldNotifyWhenResolved': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ShouldSample': ['<WebCore/DiagnosticLoggingClient.h>'],
        'WebCore::SourceBufferAppendMode': ['<WebCore/SourceBufferPrivate.h>'],
        'WebCore::StorageAccessPromptWasShown': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::StorageAccessScope': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::StorageAccessWasGranted': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::SupportedPluginIdentifier': ['<WebCore/PluginData.h>'],
        'WebCore::SystemPreviewInfo': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::TextCheckingRequestData': ['<WebCore/TextChecking.h>'],
        'WebCore::TextCheckingResult': ['<WebCore/TextCheckerClient.h>'],
        'WebCore::TextCheckingType': ['<WebCore/TextChecking.h>'],
        'WebCore::TextIndicatorData': ['<WebCore/TextIndicator.h>'],
        'WebCore::ThirdPartyCookieBlockingMode': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::UsedLegacyTLS': ['<WebCore/ResourceResponseBase.h>'],
        'WebCore::VideoPresetData': ['<WebCore/VideoPreset.h>'],
        'WebCore::ViewportAttributes': ['<WebCore/ViewportArguments.h>'],
        'WebCore::WillContinueLoading': ['<WebCore/FrameLoaderTypes.h>'],
        'WebKit::ActivityStateChangeID': ['"DrawingAreaInfo.h"'],
        'WebKit::AllowOverwrite': ['"DownloadID.h"'],
        'WebKit::AppPrivacyReportTestingData': ['"AppPrivacyReport.h"'],
        'WebKit::AuthenticationChallengeIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::BackForwardListItemState': ['"SessionState.h"'],
        'WebKit::CallDownloadDidStart': ['"DownloadManager.h"'],
        'WebKit::ConsumerSharedCARingBuffer::Handle': ['"SharedCARingBuffer.h"'],
        'WebKit::ContentWorldIdentifier': ['"ContentWorldShared.h"'],
        'WebKit::DocumentEditingContextRequest': ['"DocumentEditingContext.h"'],
        'WebKit::FindDecorationStyle': ['"WebFindOptions.h"'],
        'WebKit::FindOptions': ['"WebFindOptions.h"'],
        'WebKit::FormSubmitListenerIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::GestureRecognizerState': ['"GestureTypes.h"'],
        'WebKit::GestureType': ['"GestureTypes.h"'],
        'WebKit::LastNavigationWasAppInitiated': ['"AppPrivacyReport.h"'],
        'WebKit::LayerHostingContextID': ['"LayerHostingContext.h"'],
        'WebKit::LayerHostingMode': ['"LayerTreeContext.h"'],
        'WebKit::PageGroupIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::PageState': ['"SessionState.h"'],
        'WebKit::PaymentSetupConfiguration': ['"PaymentSetupConfigurationWebKit.h"'],
        'WebKit::PaymentSetupFeatures': ['"ApplePayPaymentSetupFeaturesWebKit.h"'],
        'WebKit::PrepareBackingStoreBuffersInputData': ['"PrepareBackingStoreBuffersData.h"'],
        'WebKit::PrepareBackingStoreBuffersOutputData': ['"PrepareBackingStoreBuffersData.h"'],
        'WebKit::RespectSelectionAnchor': ['"GestureTypes.h"'],
        'WebKit::RemoteVideoFrameReadReference': ['"RemoteVideoFrameIdentifier.h"'],
        'WebKit::RemoteVideoFrameWriteReference': ['"RemoteVideoFrameIdentifier.h"'],
        'WebKit::SelectionFlags': ['"GestureTypes.h"'],
        'WebKit::SelectionTouch': ['"GestureTypes.h"'],
        'WebKit::TapIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::TextCheckerRequestID': ['"IdentifierTypes.h"'],
        'WebKit::WebGPU::BindGroupDescriptor': ['"WebGPUBindGroupDescriptor.h"'],
        'WebKit::WebGPU::BindGroupEntry': ['"WebGPUBindGroupEntry.h"'],
        'WebKit::WebGPU::BindGroupLayoutDescriptor': ['"WebGPUBindGroupLayoutDescriptor.h"'],
        'WebKit::WebGPU::BindGroupLayoutEntry': ['"WebGPUBindGroupLayoutEntry.h"'],
        'WebKit::WebGPU::BlendComponent': ['"WebGPUBlendComponent.h"'],
        'WebKit::WebGPU::BlendState': ['"WebGPUBlendState.h"'],
        'WebKit::WebGPU::BufferBinding': ['"WebGPUBufferBinding.h"'],
        'WebKit::WebGPU::BufferBindingLayout': ['"WebGPUBufferBindingLayout.h"'],
        'WebKit::WebGPU::BufferDescriptor': ['"WebGPUBufferDescriptor.h"'],
        'WebKit::WebGPU::CanvasConfiguration': ['"WebGPUCanvasConfiguration.h"'],
        'WebKit::WebGPU::Color': ['"WebGPUColor.h"'],
        'WebKit::WebGPU::ColorTargetState': ['"WebGPUColorTargetState.h"'],
        'WebKit::WebGPU::CommandBufferDescriptor': ['"WebGPUCommandBufferDescriptor.h"'],
        'WebKit::WebGPU::CommandEncoderDescriptor': ['"WebGPUCommandEncoderDescriptor.h"'],
        'WebKit::WebGPU::CompilationMessage': ['"WebGPUCompilationMessage.h"'],
        'WebKit::WebGPU::ComputePassDescriptor': ['"WebGPUComputePassDescriptor.h"'],
        'WebKit::WebGPU::ComputePassTimestampWrites': ['"WebGPUComputePassTimestampWrites.h"'],
        'WebKit::WebGPU::ComputePipelineDescriptor': ['"WebGPUComputePipelineDescriptor.h"'],
        'WebKit::WebGPU::ConvertFromBackingContext': ['"WebGPUConvertFromBackingContext.h"'],
        'WebKit::WebGPU::ConvertToBackingContext': ['"WebGPUConvertToBackingContext.h"'],
        'WebKit::WebGPU::DepthStencilState': ['"WebGPUDepthStencilState.h"'],
        'WebKit::WebGPU::DeviceDescriptor': ['"WebGPUDeviceDescriptor.h"'],
        'WebKit::WebGPU::Error': ['"WebGPUError.h"'],
        'WebKit::WebGPU::Extent3D': ['"WebGPUExtent3D.h"'],
        'WebKit::WebGPU::ExternalTextureBindingLayout': ['"WebGPUExternalTextureBindingLayout.h"'],
        'WebKit::WebGPU::ExternalTextureDescriptor': ['"WebGPUExternalTextureDescriptor.h"'],
        'WebKit::WebGPU::FragmentState': ['"WebGPUFragmentState.h"'],
        'WebKit::WebGPU::Identifier': ['"WebGPUIdentifier.h"'],
        'WebKit::WebGPU::ImageCopyBuffer': ['"WebGPUImageCopyBuffer.h"'],
        'WebKit::WebGPU::ImageCopyExternalImage': ['"WebGPUImageCopyExternalImage.h"'],
        'WebKit::WebGPU::ImageCopyTexture': ['"WebGPUImageCopyTexture.h"'],
        'WebKit::WebGPU::ImageCopyTextureTagged': ['"WebGPUImageCopyTextureTagged.h"'],
        'WebKit::WebGPU::ImageDataLayout': ['"WebGPUImageDataLayout.h"'],
        'WebKit::WebGPU::MultisampleState': ['"WebGPUMultisampleState.h"'],
        'WebKit::WebGPU::ObjectDescriptorBase': ['"WebGPUObjectDescriptorBase.h"'],
        'WebKit::WebGPU::Origin2D': ['"WebGPUOrigin2D.h"'],
        'WebKit::WebGPU::Origin3D': ['"WebGPUOrigin3D.h"'],
        'WebKit::WebGPU::OutOfMemoryError': ['"WebGPUOutOfMemoryError.h"'],
        'WebKit::WebGPU::PipelineDescriptorBase': ['"WebGPUPipelineDescriptorBase.h"'],
        'WebKit::WebGPU::PipelineLayoutDescriptor': ['"WebGPUPipelineLayoutDescriptor.h"'],
        'WebKit::WebGPU::PrimitiveState': ['"WebGPUPrimitiveState.h"'],
        'WebKit::WebGPU::ProgrammableStage': ['"WebGPUProgrammableStage.h"'],
        'WebKit::WebGPU::QuerySetDescriptor': ['"WebGPUQuerySetDescriptor.h"'],
        'WebKit::WebGPU::RenderBundleDescriptor': ['"WebGPURenderBundleDescriptor.h"'],
        'WebKit::WebGPU::RenderBundleEncoderDescriptor': ['"WebGPURenderBundleEncoderDescriptor.h"'],
        'WebKit::WebGPU::RenderPassColorAttachment': ['"WebGPURenderPassColorAttachment.h"'],
        'WebKit::WebGPU::RenderPassDepthStencilAttachment': ['"WebGPURenderPassDepthStencilAttachment.h"'],
        'WebKit::WebGPU::RenderPassDescriptor': ['"WebGPURenderPassDescriptor.h"'],
        'WebKit::WebGPU::RenderPassLayout': ['"WebGPURenderPassLayout.h"'],
        'WebKit::WebGPU::RenderPassTimestampWrites': ['"WebGPURenderPassTimestampWrites.h"'],
        'WebKit::WebGPU::RenderPipelineDescriptor': ['"WebGPURenderPipelineDescriptor.h"'],
        'WebKit::WebGPU::RequestAdapterOptions': ['"WebGPURequestAdapterOptions.h"'],
        'WebKit::WebGPU::SamplerBindingLayout': ['"WebGPUSamplerBindingLayout.h"'],
        'WebKit::WebGPU::SamplerDescriptor': ['"WebGPUSamplerDescriptor.h"'],
        'WebKit::WebGPU::ShaderModuleDescriptor': ['"WebGPUShaderModuleDescriptor.h"'],
        'WebKit::WebGPU::StencilFaceState': ['"WebGPUStencilFaceState.h"'],
        'WebKit::WebGPU::StorageTextureBindingLayout': ['"WebGPUStorageTextureBindingLayout.h"'],
        'WebKit::WebGPU::SupportedFeatures': ['"WebGPUSupportedFeatures.h"'],
        'WebKit::WebGPU::SupportedLimits': ['"WebGPUSupportedLimits.h"'],
        'WebKit::WebGPU::SurfaceDescriptor': ['"WebGPUSurfaceDescriptor.h"'],
        'WebKit::WebGPU::SwapChainDescriptor': ['"WebGPUSwapChainDescriptor.h"'],
        'WebKit::WebGPU::TextureBindingLayout': ['"WebGPUTextureBindingLayout.h"'],
        'WebKit::WebGPU::TextureDescriptor': ['"WebGPUTextureDescriptor.h"'],
        'WebKit::WebGPU::TextureViewDescriptor': ['"WebGPUTextureViewDescriptor.h"'],
        'WebKit::WebGPU::ValidationError': ['"WebGPUValidationError.h"'],
        'WebKit::WebGPU::VertexAttribute': ['"WebGPUVertexAttribute.h"'],
        'WebKit::WebGPU::VertexBufferLayout': ['"WebGPUVertexBufferLayout.h"'],
        'WebKit::WebGPU::VertexState': ['"WebGPUVertexState.h"'],
        'WebCore::Cookie': ['<WebCore/Cookie.h>'],
        'WebCore::ElementContext': ['<WebCore/ElementContext.h>'],
        'WebCore::VideoPlaybackQualityMetrics': ['<WebCore/VideoPlaybackQualityMetrics.h>'],
        'WebKit::WebScriptMessageHandlerData': ['"WebUserContentControllerDataTypes.h"'],
        'WebKit::WebUserScriptData': ['"WebUserContentControllerDataTypes.h"'],
        'WebKit::WebUserStyleSheetData': ['"WebUserContentControllerDataTypes.h"'],
        'webrtc::WebKitEncodedFrameInfo': ['"RTCWebKitEncodedFrameInfo.h"', '<WebCore/LibWebRTCEnumTraits.h>'],
    }

    headers = []
    for header_info in header_infos_and_types['header_infos']:
        headers += header_info['headers']

    for type in header_infos_and_types['types']:
        if type in special_cases:
            headers += special_cases[type]
            continue

        # We assume that we must include a header for a type iff it has a scope
        # resolution operator (::).
        split = type.split('::')
        if len(split) < 2:
            continue

        if split[0] == 'WebKit' or split[0] == 'IPC':
            headers.append('"%s.h"' % split[1])
        else:
            headers.append('<%s/%s.h>' % tuple(split[0:2]))

    return headers


def collect_header_conditions_for_receiver(receiver, header_conditions):
    type_conditions = {}
    for parameter in receiver.iterparameters():
        if not parameter.type in type_conditions:
            type_conditions[parameter.type] = []

        if not parameter.condition in type_conditions[parameter.type]:
            type_conditions[parameter.type].append(parameter.condition)

    for parameter in receiver.iterparameters():
        type = parameter.type
        conditions = type_conditions[type]

        argument_encoder_headers = argument_coder_headers_for_type(type)
        if argument_encoder_headers:
            for header in argument_encoder_headers:
                if header not in header_conditions:
                    header_conditions[header] = []
                header_conditions[header].extend(conditions)

        type_headers = headers_for_type(type)
        for header in type_headers:
            if header not in header_conditions:
                header_conditions[header] = []
            header_conditions[header].extend(conditions)

    for message in receiver.messages:
        if message.reply_parameters is not None:
            for reply_parameter in message.reply_parameters:
                type = reply_parameter.type
                argument_encoder_headers = argument_coder_headers_for_type(type)
                if argument_encoder_headers:
                    for header in argument_encoder_headers:
                        if header not in header_conditions:
                            header_conditions[header] = []
                        header_conditions[header].append(message.condition)

                type_headers = headers_for_type(type)
                for header in type_headers:
                    if header not in header_conditions:
                        header_conditions[header] = []
                    header_conditions[header].append(message.condition)

    return header_conditions


def generate_header_includes_from_conditions(header_conditions):
    result = []
    # FIXME(https://bugs.webkit.org/show_bug.cgi?id=241854): NOLINT due to order not as WebKit expects.
    for header in sorted(header_conditions):
        if header_conditions[header] and not None in header_conditions[header]:
            result.append('#if %s\n' % ' || '.join(sorted(set(header_conditions[header]))))
            result += ['#include %s // NOLINT\n' % header]
            result.append('#endif\n')
        else:
            result += ['#include %s // NOLINT\n' % header]
    return result


def generate_message_handler(receiver):
    header_conditions = {
        '"%s"' % messages_header_filename(receiver): [None],
        '"HandleMessage.h"': [None],
        '"Decoder.h"': [None],
    }

    collect_header_conditions_for_receiver(receiver, header_conditions)

    result = []

    result.append(_license_header)
    result.append('#include "config.h"\n')

    if receiver.condition:
        result.append('#if %s\n' % receiver.condition)

    result.append('#include "%s.h"\n\n' % receiver.name)
    result += generate_header_includes_from_conditions(header_conditions)
    result.append('\n')

    result.append('#if ENABLE(IPC_TESTING_API)\n')
    result.append('#include "JSIPCBinding.h"\n')
    result.append("#endif\n\n")

    result.append('namespace WebKit {\n\n')

    async_messages = []
    sync_messages = []
    for message in receiver.messages:
        if message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
            sync_messages.append(message)
        else:
            async_messages.append(message)

    if receiver.has_attribute(STREAM_ATTRIBUTE):
        result.append('void %s::didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Decoder& decoder)\n' % (receiver.name))
        result.append('{\n')
        assert(receiver.has_attribute(NOT_REFCOUNTED_RECEIVER_ATTRIBUTE))
        assert(not receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE))
        assert(not receiver.has_attribute(WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE))

        result += [async_message_statement(receiver, message) for message in async_messages]
        result += [sync_message_statement(receiver, message) for message in sync_messages]

        if (receiver.superclass):
            result.append('    %s::didReceiveStreamMessage(connection, decoder);\n' % (receiver.superclass))
        else:
            result.append('    UNUSED_PARAM(decoder);\n')
            result.append('    UNUSED_PARAM(connection);\n')
            result.append('#if ENABLE(IPC_TESTING_API)\n')
            result.append('    if (connection.connection().ignoreInvalidMessageForTesting())\n')
            result.append('        return;\n')
            result.append('#endif // ENABLE(IPC_TESTING_API)\n')
            result.append('    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled stream message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());\n')
        result.append('}\n')
    else:
        receive_variant = receiver.name if receiver.has_attribute(LEGACY_RECEIVER_ATTRIBUTE) else ''
        result.append('void %s::didReceive%sMessage(IPC::Connection& connection, IPC::Decoder& decoder)\n' % (receiver.name, receive_variant))
        result.append('{\n')
        if not (receiver.has_attribute(NOT_REFCOUNTED_RECEIVER_ATTRIBUTE) or receiver.has_attribute(STREAM_ATTRIBUTE)):
            result.append('    Ref protectedThis { *this };\n')

        result += [async_message_statement(receiver, message) for message in async_messages]

        if receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE) or receiver.has_attribute(WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE):
            result.append('    if (dispatchMessage(connection, decoder))\n')
            result.append('        return;\n')
        if (receiver.superclass):
            result.append('    %s::didReceive%sMessage(connection, decoder);\n' % (receiver.superclass, receive_variant))
        else:
            result.append('    UNUSED_PARAM(connection);\n')
            result.append('    UNUSED_PARAM(decoder);\n')
            result.append('#if ENABLE(IPC_TESTING_API)\n')
            result.append('    if (connection.ignoreInvalidMessageForTesting())\n')
            result.append('        return;\n')
            result.append('#endif // ENABLE(IPC_TESTING_API)\n')
            result.append('    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());\n')
        result.append('}\n')

    if not receiver.has_attribute(STREAM_ATTRIBUTE) and (sync_messages or receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE)):
        result.append('\n')
        result.append('bool %s::didReceiveSync%sMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)\n' % (receiver.name, receiver.name if receiver.has_attribute(LEGACY_RECEIVER_ATTRIBUTE) else ''))
        result.append('{\n')
        if not receiver.has_attribute(NOT_REFCOUNTED_RECEIVER_ATTRIBUTE):
            result.append('    Ref protectedThis { *this };\n')
        result += [sync_message_statement(receiver, message) for message in sync_messages]
        if receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE):
            result.append('    if (dispatchSyncMessage(connection, decoder, replyEncoder))\n')
            result.append('        return true;\n')
        result.append('    UNUSED_PARAM(connection);\n')
        result.append('    UNUSED_PARAM(decoder);\n')
        result.append('    UNUSED_PARAM(replyEncoder);\n')
        result.append('#if ENABLE(IPC_TESTING_API)\n')
        result.append('    if (connection.ignoreInvalidMessageForTesting())\n')
        result.append('        return false;\n')
        result.append('#endif // ENABLE(IPC_TESTING_API)\n')
        result.append('    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()), decoder.destinationID());\n')
        result.append('    return false;\n')
        result.append('}\n')

    result.append('\n} // namespace WebKit\n')

    result.append('\n')
    result.append('#if ENABLE(IPC_TESTING_API)\n\n')
    result.append('namespace IPC {\n\n')
    previous_message_condition = None
    for message in receiver.messages:
        if previous_message_condition != message.condition:
            if previous_message_condition:
                result.append('#endif\n')
            if message.condition:
                result.append('#if %s\n' % message.condition)
            previous_message_condition = message.condition
        result.append('template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::%s_%s>(JSC::JSGlobalObject* globalObject, Decoder& decoder)\n' % (receiver.name, message.name))
        result.append('{\n')
        result.append('    return jsValueForDecodedArguments<Messages::%s::%s::%s>(globalObject, decoder);\n' % (receiver.name, message.name, 'Arguments'))
        result.append('}\n')
        has_reply = message.reply_parameters is not None
        if not has_reply:
            continue
        result.append('template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::%s_%s>(JSC::JSGlobalObject* globalObject, Decoder& decoder)\n' % (receiver.name, message.name))
        result.append('{\n')
        result.append('    return jsValueForDecodedArguments<Messages::%s::%s::%s>(globalObject, decoder);\n' % (receiver.name, message.name, 'ReplyArguments'))
        result.append('}\n')
    if previous_message_condition:
        result.append('#endif\n')
    result.append('\n}\n\n')
    result.append('#endif\n\n')

    if receiver.condition:
        result.append('\n#endif // %s\n' % receiver.condition)

    return ''.join(result)


def generate_message_names_header(receivers):
    result = []
    result.append(_license_header)
    result.append('#pragma once\n')
    result.append('\n')
    result.append('#include <wtf/EnumTraits.h>\n')
    result.append('\n')
    result.append('namespace IPC {\n')
    result.append('\n')
    result.append('enum class ReceiverName : uint8_t {')
    result.append('\n    ')
    enums = ['%s = %d' % (e, v) for v, e in enumerate(get_receiver_enumerators(receivers), 1)]
    result.append('\n    , '.join(enums))
    result.append('\n    , Invalid = %d' % (len(enums) + 1))
    result.append('\n};\n')
    result.append('\n')

    message_enumerators = get_message_enumerators(receivers)

    result.append('enum class MessageName : uint16_t {')
    result.append('\n    ')
    result.append('\n    , '.join(str(e) for e in message_enumerators))
    result.append('\n    , Last = %s' % message_enumerators[-1])
    result.append('\n};\n')
    result.append('\n')
    result.append('ReceiverName receiverName(MessageName);\n')
    result.append('const char* description(MessageName);\n')
    result.append('constexpr bool messageIsSync(MessageName name)\n')
    result.append('{\n')
    first_synchronous = next((e for e in message_enumerators if e.messages[0].has_attribute(SYNCHRONOUS_ATTRIBUTE)), None)
    if first_synchronous:
        result.append('    return name >= MessageName::%s;\n' % first_synchronous)
    else:
        result.append('    UNUSED_PARAM(name);\n')
        result.append('    return false;\n')
    result.append('}\n')
    result.append('\n')

    for fname, _ in sorted(attributes_to_generate_validators.items()):
        result.append('bool %s(MessageName);\n' % fname)

    result.append('} // namespace IPC\n')
    result.append('\n')
    result.append('namespace WTF {\n')
    result.append('\n')
    result.append('template<> bool isValidEnum<IPC::MessageName, void>(std::underlying_type_t<IPC::MessageName>);\n')
    result.append('\n')
    result.append('} // namespace WTF\n')
    return ''.join(result)


def generate_message_names_implementation(receivers):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"\n')
    result.append('#include "MessageNames.h"\n')
    result.append('\n')
    result.append('namespace IPC {\n')
    result.append('\n')
    result.append('const char* description(MessageName name)\n')
    result.append('{\n')
    result.append('    switch (name) {\n')

    message_enumerators = get_message_enumerators(receivers)
    for enumerator in message_enumerators:
        result.append('    case MessageName::%s:\n' % enumerator)
        result.append('        return "%s";\n' % enumerator)
    result.append('    }\n')
    result.append('    ASSERT_NOT_REACHED();\n')
    result.append('    return "<invalid message name>";\n')
    result.append('}\n')
    result.append('\n')
    result.append('ReceiverName receiverName(MessageName messageName)\n')
    result.append('{\n')
    result.append('    switch (messageName) {\n')
    prev_enumerator = None
    for enumerator in message_enumerators:
        if prev_enumerator and prev_enumerator.receiver != enumerator.receiver:
            result.append('        return ReceiverName::%s;\n' % prev_enumerator.receiver.name)
        result.append('    case MessageName::%s:\n' % enumerator)
        prev_enumerator = enumerator
    if prev_enumerator:
        result.append('        return ReceiverName::%s;\n' % prev_enumerator.receiver.name)
    result.append('    }\n')
    result.append('    ASSERT_NOT_REACHED();\n')
    result.append('    return ReceiverName::Invalid;\n')
    result.append('}\n')
    for fnam, attr_list in sorted(attributes_to_generate_validators.items()):
        result.append('bool %s(MessageName name)\n' % fnam)
        result.append('{\n')
        result.append('    switch (name) {\n')
        for e in message_enumerators:
            if set(attr_list).intersection(set(e.messages[0].attributes).union(set(e.receiver.attributes))):
                result.append('    case MessageName::%s:\n' % e)
        if [e for e in message_enumerators if set(attr_list).intersection(set(e.messages[0].attributes).union(set(e.receiver.attributes)))]:
            result.append('        return true;\n')
        result.append('    default:\n')
        result.append('        return false;\n')
        result.append('    }\n')
        result.append('}\n')
        result.append('\n')
    result.append('\n')
    result.append('} // namespace IPC\n')
    result.append('\n')
    result.append('namespace WTF {\n')
    result.append('template<> bool isValidEnum<IPC::MessageName, void>(std::underlying_type_t<IPC::MessageName> underlyingType)\n')
    result.append('{\n')
    result.append('    auto messageName = static_cast<IPC::MessageName>(underlyingType);\n')
    for enumerator in message_enumerators:
        for message in enumerator.messages:
            if message.condition:
                result.append('#if %s\n' % message.condition)
            result.append('    if (messageName == IPC::MessageName::%s)\n' % enumerator)
            result.append('        return true;\n')
            if message.condition:
                result.append('#endif\n')
    result.append('    return false;\n')
    result.append('};\n')
    result.append('\n')
    result.append('} // namespace WTF\n')
    return ''.join(result)


def generate_js_value_conversion_function(result, receivers, function_name, decoder_function_name, argument_type, predicate=lambda message: True):
    result.append('std::optional<JSC::JSValue> %s(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)\n' % function_name)
    result.append('{\n')
    result.append('    switch (name) {\n')
    for receiver in receivers:
        if receiver.has_attribute(BUILTIN_ATTRIBUTE):
            continue
        if receiver.condition:
            result.append('#if %s\n' % receiver.condition)
        previous_message_condition = None
        for message in receiver.messages:
            if not predicate(message):
                continue
            if previous_message_condition != message.condition:
                if previous_message_condition:
                    result.append('#endif\n')
                if message.condition:
                    result.append('#if %s\n' % message.condition)
            previous_message_condition = message.condition
            result.append('    case MessageName::%s_%s:\n' % (receiver.name, message.name))
            result.append('        return %s<MessageName::%s_%s>(globalObject, decoder);\n' % (decoder_function_name, receiver.name, message.name))
        if previous_message_condition:
            result.append('#endif\n')
        if receiver.condition:
            result.append('#endif\n')
    result.append('    default:\n')
    result.append('        break;\n')
    result.append('    }\n')
    result.append('    return std::nullopt;\n')
    result.append('}\n')


def generate_js_argument_descriptions(receivers, function_name, arguments_from_message):
    result = []
    result.append('std::optional<Vector<ArgumentDescription>> %s(MessageName name)\n' % function_name)
    result.append('{\n')
    result.append('    switch (name) {\n')
    for receiver in receivers:
        if receiver.has_attribute(BUILTIN_ATTRIBUTE):
            continue
        if receiver.condition:
            result.append('#if %s\n' % receiver.condition)
        previous_message_condition = None
        for message in receiver.messages:
            if message.has_attribute(BUILTIN_ATTRIBUTE):
                continue
            argument_list = arguments_from_message(message)
            if argument_list is None:
                continue
            if previous_message_condition != message.condition:
                if previous_message_condition:
                    result.append('#endif\n')
                if message.condition:
                    result.append('#if %s\n' % message.condition)
            previous_message_condition = message.condition
            result.append('    case MessageName::%s_%s:\n' % (receiver.name, message.name))

            if not len(argument_list):
                result.append('        return Vector<ArgumentDescription> { };\n')
                continue

            result.append('        return Vector<ArgumentDescription> {\n')
            for argument in argument_list:
                argument_type = argument.type
                enum_type = None
                is_optional = False
                if argument.kind.startswith('enum:'):
                    enum_type = '"%s"' % argument_type
                    argument_type = argument.kind[5:]
                if argument_type.startswith('std::optional<') and argument_type.endswith('>'):
                    argument_type = argument_type[14:-1]
                    is_optional = True
                result.append('            { "%s", "%s", %s, %s },\n' % (argument.name, argument_type, enum_type or 'nullptr', 'true' if is_optional else 'false'))
            result.append('        };\n')
        if previous_message_condition:
            result.append('#endif\n')
        if receiver.condition:
            result.append('#endif\n')
    result.append('    default:\n')
    result.append('        break;\n')
    result.append('    }\n')
    result.append('    return std::nullopt;\n')
    result.append('}\n')
    return result


def generate_message_argument_description_implementation(receivers, receiver_headers):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"\n')
    result.append('#include "MessageArgumentDescriptions.h"\n')
    result.append('\n')
    result.append('#if ENABLE(IPC_TESTING_API) || !LOG_DISABLED\n')
    result.append('\n')
    all_headers = ['"JSIPCBinding.h"']
    for identifier in serialized_identifiers():
        for header in headers_for_type(identifier):
            all_headers.append(header)
    all_headers = sorted(list(dict.fromkeys(all_headers)))
    for header in all_headers:
        conditions = conditions_for_header(header)
        if conditions and None not in conditions:
            result.append('#if %s\n' % ' || '.join(sorted(set(conditions))))
            result.append('#include %s\n' % header)
            result.append('#endif\n')
        else:
            result.append('#include %s\n' % header)

    for receiver in receivers:
        if receiver.has_attribute(BUILTIN_ATTRIBUTE):
            continue
        if receiver.condition:
            result.append('#if %s\n' % receiver.condition)
        header_conditions = {
            '"%s"' % messages_header_filename(receiver): [None]
        }
        result += generate_header_includes_from_conditions(header_conditions)
        if receiver.condition:
            result.append('#endif\n')

    result.append('\n')

    result.append('namespace IPC {\n')
    result.append('\n')
    result.append('#if ENABLE(IPC_TESTING_API)\n')
    result.append('\n')

    generate_js_value_conversion_function(result, receivers, 'jsValueForArguments', 'jsValueForDecodedMessage', 'Arguments')

    result.append('\n')

    generate_js_value_conversion_function(result, receivers, 'jsValueForReplyArguments', 'jsValueForDecodedMessageReply', 'ReplyArguments', lambda message: message.reply_parameters is not None)

    result.append('\n')
    result.append('Vector<ASCIILiteral> serializedIdentifiers()\n')
    result.append('{\n')
    for identifier in serialized_identifiers():
        result.append('    static_assert(sizeof(uint64_t) == sizeof(' + identifier + '));\n')
    result.append('    return {\n')
    for identifier in serialized_identifiers():
        result.append('        "' + identifier + '"_s,\n')
    result.append('    };\n')
    result.append('}\n')

    result.append('\n')
    result.append('#endif // ENABLE(IPC_TESTING_API)\n')
    result.append('\n')

    result += generate_js_argument_descriptions(receivers, 'messageArgumentDescriptions', lambda message: message.parameters)

    result.append('\n')

    result += generate_js_argument_descriptions(receivers, 'messageReplyArgumentDescriptions', lambda message: message.reply_parameters)

    result.append('\n')

    result.append('} // namespace WebKit\n')
    result.append('\n')
    result.append('#endif // ENABLE(IPC_TESTING_API) || !LOG_DISABLED\n')
    return ''.join(result)
