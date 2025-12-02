# Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
import itertools
import re
import sys

from webkit.opaque_ipc_types import opaque_ipc_types
from webkit import parser
from webkit.model import BUILTIN_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE, ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE, MAINTHREADCALLBACK_ATTRIBUTE, STREAM_ATTRIBUTE, CALL_WITH_REPLY_ID_ATTRIBUTE, MessageReceiver, Message

_license_header = """/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
NOT_STREAM_ENCODABLE_ATTRIBUTE = 'NotStreamEncodable'
NOT_STREAM_ENCODABLE_REPLY_ATTRIBUTE = 'NotStreamEncodableReply'
STREAM_BATCHED_ATTRIBUTE = 'StreamBatched'
CAN_DISPATCH_OUT_OF_ORDER_ATTRIBUTE = 'CanDispatchOutOfOrder'
REPLY_CAN_DISPATCH_OUT_OF_ORDER_ATTRIBUTE = 'ReplyCanDispatchOutOfOrder'
NOT_USING_IPC_CONNECTION_ATTRIBUTE = 'NotUsingIPCConnection'

PROCESS_NAMES = ["UI", "Networking", "GPU", "WebContent", "Model"]

attributes_to_generate_validators = {
    "messageAllowedWhenWaitingForSyncReply": [ALLOWEDWHENWAITINGFORSYNCREPLY_ATTRIBUTE, SYNCHRONOUS_ATTRIBUTE, STREAM_ATTRIBUTE],
    "messageAllowedWhenWaitingForUnboundedSyncReply": [ALLOWEDWHENWAITINGFORSYNCREPLYDURINGUNBOUNDEDIPC_ATTRIBUTE],
}

def receiver_enumerator_order_key(receiver_name):
    if receiver_name == 'IPC':
        return 1
    return 0


class MessageEnumerator(object):

    def __init__(self, receiver, messages):
        self.receiver = receiver
        self.messages = messages

    def __str__(self):
        if self.messages[0].has_attribute(BUILTIN_ATTRIBUTE):
            return self.messages[0].name
        return '%s_%s' % (self.receiver.name, self.messages[0].name)

    def condition(self):
        conditions = [message.condition for message in self.messages]
        if any([condition is None for condition in conditions]):
            return None
        return " || ".join(conditions)

    def synchronous(self):
        is_synchronous = self.messages[0].has_attribute(SYNCHRONOUS_ATTRIBUTE)
        assert(all([message.has_attribute(SYNCHRONOUS_ATTRIBUTE) == is_synchronous for message in self.messages]))
        return is_synchronous

    @classmethod
    def sort_key(cls, obj):
        return obj.synchronous(), receiver_enumerator_order_key(obj.receiver.name), str(obj)


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


def types_that_must_be_moved():
    return [
        'IPC::ConnectionHandle',
        'IPC::Signal',
        'IPC::StreamServerConnectionHandle',
        'MachSendRight',
        'std::optional<WebKit::SharedVideoFrame>',
        'Vector<WebCore::SharedMemory::Handle>',
        'WebKit::WebGPU::ExternalTextureDescriptor',
        'WebCore::GraphicsContextGL::ExternalImageSource',
        'WebCore::GraphicsContextGL::ExternalSyncSource',
        'WebCore::ProcessIdentity',
        'std::optional<WebCore::ProcessIdentity>',
        'WebKit::ConsumerSharedCARingBufferHandle',
        'WebKit::GPUProcessConnectionParameters',
        'WebKit::ModelProcessConnectionParameters',
        'WebKit::ImageBufferBackendHandle',
        'std::optional<WebKit::ImageBufferBackendHandle>',
        'WebKit::SandboxExtensionHandle',
        'Vector<WebKit::SandboxExtensionHandle>',
        'std::optional<WebKit::SandboxExtensionHandle>',
        'std::optional<Vector<WebKit::SandboxExtensionHandle>>',
        'WebKit::NetworkSessionCreationParameters',
        'WebKit::FullScreenMediaDetails',
        'WebKit::GPUProcessCreationParameters',
        'WebKit::ModelProcessCreationParameters',
        'WebKit::NetworkProcessCreationParameters',
        'WebKit::NetworkResourceLoadParameters',
        'WebKit::WebsiteDataStoreParameters',
        'WebKit::GPUProcessSessionParameters',
        'WebKit::GoToBackForwardItemParameters',
        'WebKit::ResourceLoadStatisticsParameters',
        'WebKit::WebPageCreationParameters',
        'WebKit::WebProcessDataStoreParameters',
        'WebKit::MediaDeviceSandboxExtensions',
        'WebKit::WebIDBResult',
        'WebKit::LoadParameters',
        'WebKit::AdditionalFonts',
        'WebCore::ShareableBitmapHandle',
        'WebCore::ShareableResourceHandle',
        'WebCore::SharedMemory::Handle',
        'WebKit::SharedVideoFrame',
        'WebKit::SharedVideoFrame::Buffer',
        'WebKit::UpdateInfo',
        'WebKit::WebProcessCreationParameters',
        'WebKit::RemoteLayerBackingStoreProperties',
        'Win32Handle',
        'std::optional<MachSendRight>',
        'std::optional<WebCore::ShareableBitmapHandle>',
        'std::optional<WebKit::ShareableResource::Handle>',
        'std::optional<WebCore::SharedMemory::Handle>',
        'std::optional<WebKit::SharedVideoFrame::Buffer>',
        'std::optional<Win32Handle>',
        'WebKit::ImageBufferSetPrepareBufferForDisplayOutputData',
        'HashMap<WebKit::ImageBufferSetIdentifier, std::unique_ptr<WebKit::BufferSetBackendHandle>>',
        'std::optional<WebCore::DMABufBufferAttributes>',
    ]


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
    'WebCore::TrackID',
])

def function_parameter_type(type, kind, for_reply=False):
    # Don't use references for built-in types.
    if type in builtin_types:
        return type

    if kind.startswith('enum:'):
        return type

    if type in types_that_must_be_moved() or for_reply:
        return '%s&&' % type

    return 'const %s&' % type


def function_parameter_requires_suppress_forward_decl(type, kind, for_reply=False):
    return not (
        type in builtin_types or
        kind.startswith('enum:') or
        type in types_that_cannot_be_forward_declared()
    )


def arguments_constructor_name(type, name):
    if type in types_that_must_be_moved():
        return 'WTFMove(%s)' % name

    return name

def message_to_struct_declaration(receiver, message):
    result = []
    function_parameters = [(function_parameter_type(x.type, x.kind), x.name) for x in message.parameters]
    requires_suppress_forward_decl = [function_parameter_requires_suppress_forward_decl(x.type, x.kind) for x in message.parameters]

    result.append('class %s {\n' % message.name)
    result.append('public:\n')
    result.append('    using Arguments = std::tuple<%s>;\n' % ', '.join([parameter.type for parameter in message.parameters]))
    result.append('\n')
    result.append('    static IPC::MessageName name() { return IPC::MessageName::%s_%s; }\n' % (receiver.name, message.name))
    result.append('    static constexpr bool isSync = %s;\n' % ('false', 'true')[message.reply_parameters is not None and message.has_attribute(SYNCHRONOUS_ATTRIBUTE)])
    result.append('    static constexpr bool canDispatchOutOfOrder = %s;\n' % ('false', 'true')[message.has_attribute(CAN_DISPATCH_OUT_OF_ORDER_ATTRIBUTE)])
    result.append('    static constexpr bool replyCanDispatchOutOfOrder = %s;\n' % ('false', 'true')[message.reply_parameters is not None and message.has_attribute(REPLY_CAN_DISPATCH_OUT_OF_ORDER_ATTRIBUTE)])
    result.append(f'    static constexpr bool deferSendingIfSuspended = {"true" if message.coalescing_key_indices is not None else "false"};\n')
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
        result.append('    using ReplyArguments = std::tuple<%s>;\n' % ', '.join([parameter.type for parameter in message.reply_parameters]))
        result.append('    using Reply = CompletionHandler<void(%s)>;\n' % ', '.join([function_parameter_type(x.type, x.kind, True) for x in message.reply_parameters]))
        if not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
            if len(message.reply_parameters) == 0:
                result.append('    using Promise = WTF::NativePromise<void, IPC::Error>;\n')
            elif len(message.reply_parameters) == 1:
                result.append('    using Promise = WTF::NativePromise<%s, IPC::Error>;\n' % message.reply_parameters[0].type)
            else:
                result.append('    using Promise = WTF::NativePromise<std::tuple<%s>, IPC::Error>;\n' % ', '.join([parameter.type for parameter in message.reply_parameters]))

    result.append('    %s%s(%s)\n' % (len(message.parameters) == 1 and 'explicit ' or '', message.name, ', '.join([' '.join(x) for x in function_parameters])))
    for i in range(len(message.parameters)):
        parameter = message.parameters[i]
        result.append('        %s m_%s(%s)\n' % (',' if i else ':', parameter.name, arguments_constructor_name(parameter.type, parameter.name)))
    result.append('    {\n')
    result += generate_dispatched_for_x(receiver.receiver_dispatched_from, spacing='        ')
    result.append('    }\n\n')

    if message.coalescing_key_indices is not None:
        if message.coalescing_key_indices:
            result.append('    void encodeCoalescingKey(IPC::Encoder& encoder) const\n')
            result.append('    {\n')
            for i in message.coalescing_key_indices:
                result.append('        ')
                if requires_suppress_forward_decl[i]:
                    result.append('SUPPRESS_FORWARD_DECL_ARG ')
                result.append('encoder << m_%s;\n' % message.parameters[i].name)
        else:
            result.append('    void encodeCoalescingKey(IPC::Encoder&) const\n')
            result.append('    {\n')
        result.append('    }\n')
        result.append('\n')

    result.append('    template<typename Encoder>\n')
    result.append('    void encode(Encoder& encoder)\n')
    result.append('    {\n')
    for i in range(len(message.parameters)):
        parameter = message.parameters[i]
        if message.is_async_reply:
            # Extract original message name (remove 'Reply' suffix)
            original_message_name = message.name[:-5] if message.name.endswith('Reply') else message.name
            if not opaque_ipc_types.reply_webcontent_dispatchable(receiver.name, original_message_name, parameter.name, parameter.type):
                result.append('        ASSERT(!isInWebProcess());\n')
        else:
            if not opaque_ipc_types.webcontent_dispatchable(receiver.name, message.name, parameter.name, parameter.type):
                result.append('        ASSERT(!isInWebProcess());\n')
        result.append('        ')
        if requires_suppress_forward_decl[i]:
            result.append('SUPPRESS_FORWARD_DECL_ARG ')
        if parameter.type in types_that_must_be_moved():
            result.append('encoder << WTFMove(m_%s);\n' % parameter.name)
        else:
            result.append('encoder << m_%s;\n' % parameter.name)
    result.append('    }\n')
    result.append('\n')
    result.append('private:\n')
    for i in range(len(function_parameters)):
        parameter = function_parameters[i]
        result.append('    ')
        if requires_suppress_forward_decl[i]:
            result.append('SUPPRESS_FORWARD_DECL_MEMBER ')
        result.append('%s m_%s;\n' % parameter)
    result.append('};\n')
    return surround_in_condition(''.join(result), message.condition)


def atomic_object_identifier(type):
    # FIXME: This can be derived from *.serialization.in files.
    atomic_object_identifiers = [
        'WebCore::FileSystemHandleIdentifier',
        'WebCore::FileSystemSyncAccessHandleIdentifier',
        'WebCore::FileSystemWritableFileStreamIdentifier',
        'WebCore::IDBDatabaseConnectionIdentifier',
        'WebCore::IDBIndexIdentifier',
        'WebCore::IDBObjectStoreIdentifierType',
        'WebCore::IDBObjectStoreIdentifier',
        'WebCore::LibWebRTCSocketIdentifier',
        'WebCore::RenderingResourceIdentifier',
        'WebCore::ResourceLoaderIdentifier',
        'WebCore::SamplesRendererTrackIdentifier',
        'WebCore::ServiceWorkerIdentifier',
        'WebCore::ServiceWorkerJobIdentifier',
        'WebCore::ServiceWorkerRegistrationIdentifier',
        'WebCore::WebSocketIdentifier',
        'WebKit::DDModelIdentifier',
        'WebKit::GPUProcessConnectionIdentifier',
        'WebKit::LibWebRTCResolverIdentifier',
        'WebKit::LogStreamIdentifier',
        'WebKit::QuotaIncreaseRequestIdentifier',
        'WebKit::RemoteRenderingBackendIdentifier',
        'WebKit::RemoteGraphicsContextIdentifier',
        'WebKit::RemoteGradientIdentifier',
        'WebKit::RemoteDisplayListIdentifier',
        'WebKit::RemoteDisplayListRecorderIdentifier',
        'WebKit::RemoteSerializedImageBufferIdentifier',
        'WebKit::RemoteSnapshotIdentifier',
        'WebKit::RemoteSnapshotRecorderIdentifier',
        'WebKit::RemoteVideoFrameIdentifier',
        'WebKit::StorageAreaIdentifier',
        'WebKit::WebGPUIdentifier',
        'WebKit::RemoteRenderingBackendIdentifier',
        'WebKit::RemoteGraphicsContextGLIdentifier',
        'WebKit::VideoEncoderIdentifier',
        'WebKit::VideoDecoderIdentifier',
    ]
    if type in atomic_object_identifiers:
        return 'Atomic'
    return ''

def forward_declaration(namespace, kind_and_type):
    kind, type = kind_and_type

    qualified_name = '%s::%s' % (namespace, type)
    if kind == 'struct':
        return 'struct %s' % type
    elif kind.startswith('enum:'):
        return 'enum class %s : %s' % (type, kind[5:])
    elif qualified_name in serialized_identifiers():
        return 'struct ' + type + 'Type;\nusing ' + type + ' = ' + atomic_object_identifier(qualified_name) + 'ObjectIdentifier<' + type + 'Type>'
    elif qualified_name in serialized_identifiers():
        return 'struct ' + type + 'Type;\nusing ' + type + ' = ' + atomic_object_identifier(qualified_name) + 'ObjectIdentifier<' + type + 'Type>'
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
    # FIXME: This can be derived from *.serialization.in files.
    return [
        'IPC::AsyncReplyID',
        'WebCore::AttributedStringTextListID',
        'WebCore::AttributedStringTextTableBlockID',
        'WebCore::AttributedStringTextTableID',
        'WebCore::BackForwardFrameItemIdentifierID',
        'WebCore::BackForwardItemIdentifierID',
        'WebCore::BackgroundFetchRecordIdentifier',
        'WebCore::DOMCacheIdentifierID',
        'WebCore::DictationContext',
        'WebCore::NodeIdentifier',
        'WebCore::FetchIdentifier',
        'WebCore::FileSystemHandleIdentifier',
        'WebCore::FileSystemSyncAccessHandleIdentifier',
        'WebCore::FileSystemWritableFileStreamIdentifier',
        'WebCore::FrameIdentifier',
        'WebCore::IDBIndexIdentifier',
        'WebCore::IDBObjectStoreIdentifier',
        'WebCore::ImageDecoderIdentifier',
        'WebCore::InbandGenericCueIdentifier',
        'WebCore::LayerHostingContextIdentifier',
        'WebCore::LibWebRTCSocketIdentifier',
        'WebCore::MediaKeySystemRequestIdentifier',
        'WebCore::MediaPlayerClientIdentifier',
        'WebCore::MediaPlayerIdentifier',
        'WebCore::MediaSessionGroupIdentifier',
        'WebCore::MediaSessionIdentifier',
        'WebCore::ModelPlayerIdentifier',
        'WebCore::MediaUniqueIdentifier',
        'WebCore::NavigationIdentifier',
        'WebCore::OpaqueOriginIdentifier',
        'WebCore::PageIdentifier',
        'WebCore::PlatformLayerIdentifierID',
        'WebCore::PlaybackTargetClientContextID',
        'WebCore::PortIdentifier',
        'WebCore::ProcessIdentifier',
        'WebCore::PushSubscriptionIdentifier',
        'WebCore::RTCDataChannelLocalIdentifier',
        'WebCore::RealtimeMediaSourceIdentifier',
        'WebCore::RenderingResourceIdentifier',
        'WebCore::ResourceLoaderIdentifier',
        'WebCore::SWServerConnectionIdentifier',
        'WebCore::SamplesRendererTrackIdentifier',
        'WebCore::ScrollingNodeIdentifier',
        'WebCore::ServiceWorkerIdentifier',
        'WebCore::ServiceWorkerJobIdentifier',
        'WebCore::ServiceWorkerRegistrationIdentifier',
        'WebCore::SharedWorkerIdentifier',
        'WebCore::SharedWorkerObjectIdentifierID',
        'WebCore::SleepDisablerIdentifier',
        'WebCore::SpeechRecognitionConnectionClientIdentifier',
        'WebCore::TextCheckingRequestIdentifier',
        'WebCore::TextManipulationItemIdentifier',
        'WebCore::TextManipulationTokenIdentifier',
        'WebCore::TimelineIdentifier',
        'WebCore::IDBDatabaseConnectionIdentifier',
        'WebCore::IDBResourceObjectIdentifier',
        'WebCore::UserGestureTokenIdentifierID',
        'WebCore::UserMediaRequestIdentifier',
        'WebCore::WebLockIdentifierID',
        'WebCore::WebProcessJSHandleIdentifier',
        'WebCore::WebSocketIdentifier',
        'WebCore::WebTransportSendGroupIdentifier',
        'WebCore::WebTransportStreamIdentifier',
        'WebCore::WindowIdentifier',
        'WebKit::AudioMediaStreamTrackRendererInternalUnitIdentifier',
        'WebKit::AuthenticationChallengeIdentifier',
        'WebKit::DDModelIdentifier',
        'WebKit::DataTaskIdentifier',
        'WebKit::DisplayLinkObserverID',
        'WebKit::DownloadID',
        'WebKit::DrawingAreaIdentifier',
        'WebKit::GeolocationIdentifier',
        'WebKit::GPUProcessConnectionIdentifier',
        'WebKit::ImageBufferSetIdentifier',
        'WebKit::RemoteGraphicsContextGLIdentifier',
        'WebKit::RiceBackendIdentifier',
        'WebKit::IPCConnectionTesterIdentifier',
        'WebKit::IPCStreamTesterIdentifier',
        'WebKit::JSObjectID',
        'WebKit::LegacyCustomProtocolID',
        'WebKit::LibWebRTCResolverIdentifier',
        'WebKit::LogStreamIdentifier',
        'WebKit::MarkSurfacesAsVolatileRequestIdentifier',
        'WebKit::MessageBatchIdentifier',
        'WebKit::NetworkResourceLoadIdentifier',
        'WebKit::NonProcessQualifiedContentWorldIdentifier',
        'WebKit::PDFPluginIdentifier',
        'WebKit::PageGroupIdentifier',
        'WebKit::QuotaIncreaseRequestIdentifier',
        'WebKit::RemoteAudioDestinationIdentifier',
        'WebKit::RemoteAudioHardwareListenerIdentifier',
        'WebKit::RemoteAudioVideoRendererIdentifier',
        'WebKit::RemoteCDMIdentifier',
        'WebKit::RemoteCDMInstanceIdentifier',
        'WebKit::RemoteCDMInstanceSessionIdentifier',
        'WebKit::RemoteGraphicsContextIdentifier',
        'WebKit::RemoteGradientIdentifier',
        'WebKit::RemoteDisplayListIdentifier',
        'WebKit::RemoteDisplayListRecorderIdentifier',
        'WebKit::RemoteLegacyCDMIdentifier',
        'WebKit::RemoteLegacyCDMSessionIdentifier',
        'WebKit::RemoteMediaResourceIdentifier',
        'WebKit::RemoteMediaSourceIdentifier',
        'WebKit::RemoteRemoteCommandListenerIdentifier',
        'WebKit::RemoteSerializedImageBufferIdentifier',
        'WebKit::RemoteSnapshotIdentifier',
        'WebKit::RemoteSnapshotRecorderIdentifier',
        'WebKit::RemoteSourceBufferIdentifier',
        'WebKit::RemoteVideoFrameIdentifier',
        'WebKit::RemoteRenderingBackendIdentifier',
        'WebKit::RenderingUpdateID',
        'WebKit::RetrieveRecordResponseBodyCallbackIdentifier',
        'WebKit::SampleBufferDisplayLayerIdentifier',
        'WebKit::ScriptMessageHandlerIdentifier',
        'WebKit::ShapeDetectionIdentifier',
        'WebKit::StorageAreaIdentifier',
        'WebKit::StorageAreaImplIdentifier',
        'WebKit::StorageAreaMapIdentifier',
        'WebKit::StorageNamespaceIdentifier',
        'WebKit::TapIdentifier',
        'WebKit::TextCheckerRequestID',
        'WebKit::UserContentControllerIdentifier',
        'WebKit::UserScriptIdentifier',
        'WebKit::UserStyleSheetIdentifier',
        'WebKit::VideoDecoderIdentifier',
        'WebKit::VideoEncoderIdentifier',
        'WebKit::VisitedLinkTableIdentifier',
        'WebKit::WebExtensionContextIdentifier',
        'WebKit::WebExtensionControllerIdentifier',
        'WebKit::WebExtensionFrameIdentifier',
        'WebKit::WebExtensionPortChannelIdentifier',
        'WebKit::WebExtensionTabIdentifier',
        'WebKit::WebExtensionWindowIdentifier',
        'WebKit::WebGPUIdentifier',
        'WebKit::WebPageProxyIdentifier',
        'WebKit::WebTransportSessionIdentifier',
        'WebKit::WebURLSchemeHandlerIdentifier',
    ]


def types_that_cannot_be_forward_declared():
    return frozenset([
        'CVPixelBufferRef',
        'GCGLint',
        'IPC::AsyncReplyID',
        'IPC::FontReference',
        'IPC::Semaphore',
        'IPC::Signal',
        'Inspector::ExtensionAppearance',
        'Inspector::ExtensionTabID',
        'MachSendRight',
        'MediaTime',
        'PlatformXR::ReferenceSpaceType',
        'PlatformXR::HitTestOptions',
        'PlatformXR::HitTestSource',
        'PlatformXR::LayerHandle',
        'PlatformXR::Layout',
        'PlatformXR::SessionFeature',
        'PlatformXR::SessionMode',
        'PlatformXR::TransientInputHitTestOptions',
        'PlatformXR::TransientInputHitTestSource',
        'PlatformXR::VisibilityState',
        'String',
        'WebCore::BackForwardFrameItemIdentifier',
        'WebCore::BackForwardItemIdentifier',
        'WebCore::ControlStyle',
        'WebCore::DOMCacheIdentifier',
        'WebCore::DOMCacheEngine::CacheIdentifierOrError',
        'WebCore::DOMCacheEngine::RemoveCacheIdentifierOrError',
        'WebCore::DestinationColorSpace',
        'WebCore::DiagnosticLoggingDomain',
        'WebCore::DictationContext',
        'WebCore::DragApplicationFlags',
        'WebCore::FloatBoxExtent',
        'WebCore::GCGLExtension',
        'WebCore::GlyphBufferAdvance',
        'WebCore::GlyphBufferGlyph',
        'WebCore::GraphicsContextGL::ExternalImageSource',
        'WebCore::GraphicsContextGL::ExternalSyncSource',
        'WebCore::GraphicsContextGLAttributes',
        'WebCore::GraphicsStyle',
        'WebCore::HostingContext',
        'WebCore::IDBKeyPath',
        'WebCore::IntDegrees',
        'WebCore::IndexIDToIndexKeyMap',
        'WebCore::JSHandleIdentifier',
        'WebCore::MediaAccessDenialReason',
        'WebCore::MediaPlayerPitchCorrectionAlgorithm',
        'WebCore::ModalContainerControlType',
        'WebCore::NativeImageReference',
        'WebCore::NotificationPayload',
        'WebCore::PathArc',
        'WebCore::PathClosedArc',
        'WebCore::PathDataBezierCurve',
        'WebCore::PathDataLine',
        'WebCore::PathDataQuadCurve',
        'WebCore::PatternParameters',
        'WebCore::PlatformLayerIdentifier',
        'WebCore::PlatformMediaError',
        'WebCore::PlaybackTargetClientContextIdentifier',
        'WebCore::PointerID',
        'WebCore::RTCDataChannelIdentifier',
        'WebCore::ReferrerPolicy',
        'WebCore::RenderingMode',
        'WebCore::RenderingPurpose',
        'WebCore::SandboxFlags',
        'WebCore::ScriptExecutionContextIdentifier',
        'WebCore::ScrollingNodeID',
        'WebCore::ServiceWorkerOrClientData',
        'WebCore::ServiceWorkerOrClientIdentifier',
        'WebCore::SharedStringHash',
        'WebCore::SharedWorkerObjectIdentifier',
        'WebCore::SourceBufferAppendMode',
        'WebCore::StorageType',
        'WebCore::TextDrawingModeFlags',
        'WebCore::TrackID',
        'WebCore::TransferredMessagePort',
        'WebCore::UserGestureTokenIdentifier',
        'WebCore::WebLockIdentifier',
        'WebKit::ActivityStateChangeID',
        'WebKit::ContentWorldIdentifier',
        'WebKit::DisplayLinkObserverID',
        'WebKit::DisplayListRecorderFlushIdentifier',
        'WebKit::EditorStateIdentifier',
        'WebKit::FileSystemStorageError',
        'WebKit::FileSystemSyncAccessHandleInfo',
        'WebKit::FocusedElementInformation',
        'WebKit::GPUProcessConnectionInfo',
        'WebKit::ImageBufferSetIdentifier',
        'WebKit::LayerHostingContextID',
        'WebKit::LegacyCustomProtocolID',
        'WebKit::PlaybackSessionContextIdentifier',
        'WebKit::RemoteMediaSourceIdentifier',
        'WebKit::RemoteSourceBufferIdentifier',
        'WebKit::RemoteVideoFrameReadReference',
        'WebKit::RemoteVideoFrameWriteReference',
        'WebKit::RenderingUpdateID',
        'WebKit::TextCheckerRequestID',
        'WebKit::TransactionID',
        'WebKit::WCContentBufferIdentifier',
        'WebKit::WCLayerTreeHostIdentifier',
        'WebKit::WebExtensionActionClickBehavior',
        'WebKit::WebExtensionCommandParameters',
        'WebKit::WebExtensionContentWorldType',
        'WebKit::WebExtensionDataType',
        'WebKit::WebExtensionEventListenerType',
        'WebKit::WebExtensionFrameParameters',
        'WebKit::WebExtensionMenuItemContextParameters',
        'WebKit::WebExtensionMenuItemParameters',
        'WebKit::WebExtensionRegisteredScriptParameters',
        'WebKit::WebExtensionScriptInjectionParameters',
        'WebKit::WebExtensionScriptInjectionResultParameters',
        'WebKit::WebExtensionStorageAccessLevel',
        'WebKit::WebExtensionTabParameters',
        'WebKit::WebExtensionTabQueryParameters',
        'WebKit::WebExtensionWindowParameters',
        'WebKit::WebTransportSessionIdentifier',
        'WebKit::XRDeviceIdentifier',
        'WTF::SystemMemoryPressureStatus',
    ] + types_that_must_be_moved())


def conditions_for_header(header):
    conditions = {
        '"AvailableInputDevices.h"': ["PLATFORM(GTK)", "PLATFORM(WPE)"],
        '"CoreIPCAuditToken.h"': ["HAVE(AUDIT_TOKEN)"],
        '"DataDetectionResult.h"': ["PLATFORM(COCOA)"],
        '"DynamicViewportSizeUpdate.h"': ["PLATFORM(IOS_FAMILY)"],
        '"RendererBufferFormat.h"': ["PLATFORM(GTK)", "PLATFORM(WPE)"],
        '"GestureTypes.h"': ["PLATFORM(IOS_FAMILY)"],
        '"InputMethodState.h"': ["PLATFORM(GTK)", "PLATFORM(WPE)"],
        '"MediaPlaybackTargetContextSerialized.h"': ["ENABLE(WIRELESS_PLAYBACK_TARGET)"],
        '"MediaPlayerPrivateRemote.h"': ["ENABLE(GPU_PROCESS) && ENABLE(VIDEO)"],
        '"RemoteAudioSessionIdentifier.h"': ["ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)"],
        '"RemoteAudioVideoRendererIdentifier.h"': ["ENABLE(VIDEO)"],
        '"RemoteCDMIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)"],
        '"RemoteCDMInstanceIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)"],
        '"RemoteCDMInstanceSessionIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)"],
        '"RemoteLegacyCDMIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)"],
        '"RemoteLegacyCDMSessionIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)"],
        '"RemoteMediaSourceIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)"],
        '"RemoteSourceBufferIdentifier.h"': ["ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)"],
        '"SoupCookiePersistentStorageType.h"': ["USE(SOUP)"],
        '"SharedCARingBuffer.h"': ["PLATFORM(COCOA)"],
        '"UserMessage.h"': ["USE(GLIB)"],
        '"WCContentBufferIdentifier.h"': ["USE(GRAPHICS_LAYER_WC)"],
        '"WCLayerTreeHostIdentifier.h"': ["USE(GRAPHICS_LAYER_WC)"],
        '<WebCore/AttributedString.h>': ["PLATFORM(COCOA)", ],
        '<WebCore/CVUtilities.h>': ["PLATFORM(COCOA)", ],
        '<WebCore/CurlProxySettings.h>': ["USE(CURL)"],
        '<WebCore/DMABufBuffer.h>': ["USE(GBM)"],
        '<WebCore/DataDetectorType.h>': ["ENABLE(DATA_DETECTION)"],
        '<WebCore/DynamicContentScalingDisplayList.h>': ["ENABLE(RE_DYNAMIC_CONTENT_SCALING)"],
        '<WebCore/ImageUtilities.h>': ["PLATFORM(COCOA)"],
        '<WebCore/MediaPlaybackTarget.h>': ["ENABLE(WIRELESS_PLAYBACK_TARGET)"],
        '<WebCore/ModelPlayerIdentifier.h>': ["ENABLE(MODEL_PROCESS)"],
        '<WebCore/PlaybackTargetClientContextIdentifier.h>': ["ENABLE(WIRELESS_PLAYBACK_TARGET)"],
        '<WebCore/SoupNetworkProxySettings.h>': ["USE(SOUP)"],
        '<WebCore/SelectionData.h>': ["PLATFORM(GTK)", "PLATFORM(WPE)"],
        '<WebCore/TimelineIdentifier.h>': ["ENABLE(THREADED_ANIMATIONS)"],
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
        '<wtf/RuntimeApplicationChecks.h>',
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

        if type.startswith('std::optional<') and type.endswith('>'):
            type = type[14: len(type) - 1]

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
    result.append('\n')
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
    if (receiver.receiver_dispatched_to):
        result.append('#if ASSERT_ENABLED\n')
        result.append('    static std::once_flag onceFlag;\n')
        result.append('    std::call_once(\n')
        result.append('        onceFlag,\n')
        result.append('        [&] {\n')
        result += generate_dispatched_for_x(receiver.receiver_dispatched_to, spacing='            ')
        result.append('        }\n')
        result.append('    );\n')
        result.append('#endif\n')
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
    if message.name.startswith('URL'):
        return '%s::%s' % (receiver.name, 'url' + message.name[3:])
    if message.name.startswith('GPU'):
        return '%s::%s' % (receiver.name, 'gpu' + message.name[3:])
    return '%s::%s' % (receiver.name, message.name[0].lower() + message.name[1:])

def generate_enabled_by(receiver, enabled_by, enabled_by_conjunction):
    conjunction = ' %s ' % (enabled_by_conjunction or '&&')
    return conjunction.join(['sharedPreferences->' + preference[0].lower() + preference[1:] for preference in enabled_by])

def generate_runtime_enablement(receiver, message):
    if not message.enabled_by:
        return
    runtime_enablement = generate_enabled_by(receiver, message.enabled_by, message.enabled_by_conjunction)
    if len(message.enabled_by) > 1:
        return 'sharedPreferences && (%s)' % runtime_enablement
    return 'sharedPreferences && %s' % runtime_enablement

def async_message_statement(receiver, message):
    if receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE) and message.reply_parameters is not None and not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
        dispatch_function_args = ['decoder', 'WTFMove(replyHandler)', 'this', '&%s' % handler_function(receiver, message)]
    else:
        dispatch_function_args = ['decoder', 'this', '&%s' % handler_function(receiver, message)]

    dispatch_function = 'handleMessage'
    if message.reply_parameters is not None and not message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
        dispatch_function += 'Async'
    if message.has_attribute(CALL_WITH_REPLY_ID_ATTRIBUTE):
        dispatch_function += 'WithReplyID'
    if receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE):
        dispatch_function += 'WithoutUsingIPCConnection'

    connection = 'connection, '
    if receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE):
        connection = ''

    result = []
    message_runtime_enablement = True if message.enabled_by or message.enabled_by_exception else False
    receiver_runtime_enablement = True if receiver.receiver_enabled_by or receiver.receiver_enabled_by_exception else False
    receiver_dispatched_to_webcontent = True if receiver.receiver_dispatched_to == 'WebContent' else False
    if not message_runtime_enablement and not receiver_runtime_enablement and not receiver_dispatched_to_webcontent:
        return '#error "Receiver %s or message %s must be annotated with \'EnabledBy=[FeatureFlag]\' in messages.in file\n' % (receiver.name, message.name)

    runtime_enablement = generate_runtime_enablement(receiver, message)
    if runtime_enablement or message.validator:
        result.append('    if (decoder.messageName() == Messages::%s::%s::name()) {\n' % (receiver.name, message.name))
        if runtime_enablement:
            result.append('        if (!(%s)) {\n' % runtime_enablement)
            result.append('            RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message endpoint", IPC::description(decoder.messageName()).characters());\n')
            result.append('            decoder.markInvalid();\n')
            result.append('            return;\n')
            result.append('        }\n')
        if message.validator:
            result.append('        if (!%s(decoder)) {\n' % message.validator)
            result.append('            RELEASE_LOG_ERROR(IPC, "Message %s fails validation", IPC::description(decoder.messageName()).characters());\n')
            result.append('            decoder.markInvalid();\n')
            result.append('            return;\n')
            result.append('        }\n')
        result.append('        IPC::%s<Messages::%s::%s>(%s%s);\n' % (dispatch_function, receiver.name, message.name, connection, ', '.join(dispatch_function_args)))
        result.append('        return;\n')
        result.append('    }\n')
    else:
        result.append('    if (decoder.messageName() == Messages::%s::%s::name()) {\n' % (receiver.name, message.name))
        result.append('        IPC::%s<Messages::%s::%s>(%s%s);\n' % (dispatch_function, receiver.name, message.name, connection, ', '.join(dispatch_function_args)))
        result.append('        return;\n')
        result.append('    }\n')
    return result


def sync_message_statement(receiver, message):
    dispatch_function = 'handleMessage'
    if message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
        dispatch_function += 'Synchronous'
    elif message.reply_parameters is not None:
        dispatch_function += 'Async'

    maybe_reply_encoder = ", *replyEncoder"
    if receiver.has_attribute(STREAM_ATTRIBUTE):
        maybe_reply_encoder = ''
    elif message.reply_parameters is not None:
        maybe_reply_encoder = ', replyEncoder'

    result = []
    message_runtime_enablement = True if message.enabled_by or message.enabled_by_exception else False
    receiver_runtime_enablement = True if receiver.receiver_enabled_by or receiver.receiver_enabled_by_exception else False
    receiver_dispatched_to_webcontent = True if receiver.receiver_dispatched_to == 'WebContent' else False
    if not message_runtime_enablement and not receiver_runtime_enablement and not receiver_dispatched_to_webcontent:
        return '#error "Receiver %s or message %s must be annotated with \'EnabledBy=[FeatureFlag]\' in messages.in file\n' % (receiver.name, message.name)

    runtime_enablement = generate_runtime_enablement(receiver, message)
    if runtime_enablement:
        result.append('    if (decoder.messageName() == Messages::%s::%s::name() && %s) {\n' % (receiver.name, message.name, runtime_enablement))
    else:
        result.append('    if (decoder.messageName() == Messages::%s::%s::name()) {\n' % (receiver.name, message.name))
    result.append('        IPC::%s<Messages::%s::%s>(connection, decoder%s, this, &%s);\n' % (dispatch_function, receiver.name, message.name, maybe_reply_encoder, handler_function(receiver, message)))
    result.append('        return;\n')
    result.append('    }\n')
    return result


def class_template_headers(template_string):
    template_string = template_string.strip()

    class_template_types = {
        'WebCore::RectEdges': {'headers': ['<WebCore/RectEdges.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Expected': {'headers': ['<wtf/Expected.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'HashCountedSet': {'headers': ['<wtf/HashCountedSet.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'UncheckedKeyHashMap': {'headers': ['<wtf/HashMap.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'HashMap': {'headers': ['<wtf/HashMap.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'HashSet': {'headers': ['<wtf/HashSet.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'KeyValuePair': {'headers': ['<wtf/KeyValuePair.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Markable': {'headers': ['<wtf/Markable.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'OptionSet': {'headers': ['<wtf/OptionSet.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Vector': {'headers': ['<wtf/Vector.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::optional': {'headers': ['<optional>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::pair': {'headers': ['<utility>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::span': {'headers': ['<span>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Variant': {'headers': ['<wtf/Variant.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'IPC::ArrayReferenceTuple': {'headers': ['"ArrayReferenceTuple.h"'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'Ref': {'headers': ['<wtf/Ref.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'RefPtr': {'headers': ['<wtf/RefCounted.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'RetainPtr': {'headers': ['<wtf/RetainPtr.h>'], 'argument_coder_headers': []},
        'WebCore::ProcessQualified': {'headers': ['<WebCore/ProcessQualified.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'std::unique_ptr': {'headers': ['<memory>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
        'FixedVector': {'headers': ['<wtf/FixedVector.h>'], 'argument_coder_headers': ['"ArgumentCoders.h"']},
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
        'IPC::Signal': '"IPCEvent.h"',
        'String': '"ArgumentCoders.h"',
        'WebKit::ScriptMessageHandlerHandle': '"WebScriptMessageHandler.h"',
    }

    headers = []
    for header_info in header_infos_and_types['header_infos']:
        headers += header_info['argument_coder_headers']

    for type in header_infos_and_types['types']:
        if type in special_cases:
            headers.append(special_cases[type])

    return headers


def headers_for_type(type, for_implementation_file=False):
    header_infos_and_types = class_template_headers(type)

    special_cases = {
        'CVPixelBufferRef': ['<WebCore/CVUtilities.h>'],
        'GCGLint': ['<WebCore/GraphicsTypesGL.h>'],
        'Inspector::ExtensionAppearance': ['"InspectorExtensionTypes.h"'],
        'Inspector::ExtensionError': ['"InspectorExtensionTypes.h"'],
        'Inspector::ExtensionTabID': ['"InspectorExtensionTypes.h"'],
        'Inspector::FrontendChannel::ConnectionType': ['<JavaScriptCore/InspectorFrontendChannel.h>'],
        'Inspector::InspectorTargetType': ['<JavaScriptCore/InspectorTarget.h>'],
        'IPC::AsyncReplyID': ['"Connection.h"'],
        'IPC::Signal': ['"IPCEvent.h"'],
        'IPC::Semaphore': ['"IPCSemaphore.h"'],
        'IPC::StreamServerConnectionHandle': ['"StreamServerConnection.h"'],
        'JSC::MessageLevel': ['<JavaScriptCore/ConsoleTypes.h>'],
        'JSC::MessageSource': ['<JavaScriptCore/ConsoleTypes.h>'],
        'JSC::MessageType': ['<JavaScriptCore/ConsoleTypes.h>'],
        'MachSendRight': ['<wtf/MachSendRight.h>'],
        'MachSendRightAnnotated': ['<wtf/MachSendRightAnnotated.h>'],
        'MediaTime': ['<wtf/MediaTime.h>'],
        'MonotonicTime': ['<wtf/MonotonicTime.h>'],
        'PAL::SessionID': ['<pal/SessionID.h>'],
        'PAL::UserInterfaceIdiom': ['<pal/system/ios/UserInterfaceIdiom.h>'],
        'PlatformXR::FrameData': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::HitTestOptions': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::HitTestSource': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::LayerHandle': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::Layout': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::RateMapDescription': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::ReferenceSpaceType': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::RequestData': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::SessionFeature': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::SessionMode': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::TransientInputHitTestOptions': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::TransientInputHitTestSource': ['<WebCore/PlatformXR.h>'],
        'PlatformXR::VisibilityState': ['<WebCore/PlatformXR.h>'],
        'Seconds': ['<wtf/Seconds.h>'],
        'String': ['<wtf/text/WTFString.h>'],
        'std::monostate': [],
        'URL': ['<wtf/URLHash.h>'],
        'WTF::UUID': ['<wtf/UUID.h>'],
        'WallTime': ['<wtf/WallTime.h>'],
        'WebCore::AXDebugInfo': ['<WebCore/AXObjectCache.h>'],
        'WebCore::AriaNotifyData': ['<WebCore/AXObjectCache.h>'],
        'WebCore::LiveRegionAnnouncementData': ['<WebCore/AXObjectCache.h>'],
        'WebCore::AlternativeTextType': ['<WebCore/AlternativeTextClient.h>'],
        'WebCore::ApplyTrackingPrevention': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::AttachmentAssociatedElementType': ['<WebCore/AttachmentAssociatedElement.h>'],
        'WebCore::AttributedStringTextListID': ['<WebCore/AttributedString.h>'],
        'WebCore::AttributedStringTextTableID': ['<WebCore/AttributedString.h>'],
        'WebCore::AttributedStringTextTableBlockID': ['<WebCore/AttributedString.h>'],
        'WebCore::AudioInfo': ['<WebCore/TrackInfo.h>'],
        'WebCore::AudioSessionCategory': ['<WebCore/AudioSession.h>'],
        'WebCore::AudioSessionMode': ['<WebCore/AudioSession.h>'],
        'WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged': ['<WebCore/AudioSession.h>'],
        'WebCore::AudioSessionRoutingArbitrationError': ['<WebCore/AudioSession.h>'],
        'WebCore::AudioSessionMayResume': ['<WebCore/AudioSession.h>'],
        'WebCore::AudioSessionSoundStageSize': ['<WebCore/AudioSession.h>'],
        'WebCore::AutocorrectionResponse': ['<WebCore/AlternativeTextClient.h>'],
        'WebCore::AutoplayEventFlags': ['<WebCore/AutoplayEvent.h>'],
        'WebCore::BackForwardFrameItemIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::BackForwardFrameItemIdentifier': ['<WebCore/ProcessQualified.h>', '<WebCore/BackForwardFrameItemIdentifier.h>', '<wtf/ObjectIdentifier.h>'],
        'WebCore::BackForwardItemIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::BackForwardItemIdentifier': ['<WebCore/ProcessQualified.h>', '<WebCore/BackForwardItemIdentifier.h>', '<wtf/ObjectIdentifier.h>'],
        'WebCore::BlendMode': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::BrowsingContextGroupSwitchDecision': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::CaptionUserPreferencesDisplayMode': ['<WebCore/CaptionUserPreferences.h>'],
        'WebCore::CaptureSourceError': ['<WebCore/RealtimeMediaSource.h>'],
        'WebCore::CaretAnimatorType': ['<WebCore/CaretAnimator.h>'],
        'WebCore::CDMInstanceSessionLoadFailure': ['<WebCore/CDMInstanceSession.h>'],
        'WebCore::CDMInstanceSuccessValue': ['<WebCore/CDMInstanceSession.h>'],
        'WebCore::CDMInstanceAllowDistinctiveIdentifiers': ['<WebCore/CDMInstanceSession.h>'],
        'WebCore::CDMInstanceAllowPersistentState': ['<WebCore/CDMInstanceSession.h>'],
        'WebCore::CDMInstanceHDCPStatus': ['<WebCore/CDMInstanceSession.h>'],
        'WebCore::CDMPrivateLocalStorageAccess': ['<WebCore/CDMPrivate.h>'],
        'WebCore::COEPDisposition': ['<WebCore/CrossOriginEmbedderPolicy.h>'],
        'WebCore::ColorSchemePreference': ['<WebCore/DocumentLoader.h>'],
        'WebCore::CompositeMode': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::CompositeOperator': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::CryptoKey::Data': ['<WebCore/CryptoKey.h>'],
        'WebCore::COOPDisposition': ['<WebCore/CrossOriginOpenerPolicy.h>'],
        'WebCore::CreateNewGroupForHighlight': ['<WebCore/AppHighlight.h>'],
        'WebCore::CrossOriginOpenerPolicyValue': ['<WebCore/CrossOriginOpenerPolicy.h>'],
        'WebCore::CrossSiteNavigationDataTransferFlag': ['<WebCore/CrossSiteNavigationDataTransfer.h>'],
        'WebCore::DDModel::DDFloat3': ['<WebCore/DDFloat3.h>'],
        'WebCore::DDModel::DDFloat4x4': ['<WebCore/DDFloat4x4.h>'],
        'WebCore::DDModel::DDMeshDescriptor': ['<WebCore/DDMeshDescriptor.h>'],
        'WebCore::DDModel::DDUpdateMeshDescriptor': ['<WebCore/DDUpdateMeshDescriptor.h>'],
        'WebCore::DDModel::DDImageAsset': ['<WebCore/DDImageAsset.h>'],
        'WebCore::DDModel::DDImageAssetSwizzle': ['<WebCore/DDImageAssetSwizzle.h>'],
        'WebCore::DDModel::DDUpdateTextureDescriptor': ['<WebCore/DDUpdateTextureDescriptor.h>'],
        'WebCore::DDModel::DDMaterialDescriptor': ['<WebCore/DDMaterialDescriptor.h>'],
        'WebCore::DDModel::DDUpdateMaterialDescriptor': ['<WebCore/DDUpdateMaterialDescriptor.h>'],
        'WebCore::DDModel::DDMeshPart': ['<WebCore/DDMeshPart.h>'],
        'WebCore::DDModel::DDVertexAttributeFormat': ['<WebCore/DDVertexAttributeFormat.h>'],
        'WebCore::DDModel::DDVertexLayout': ['<WebCore/DDVertexLayout.h>'],
        'WebCore::DiagnosticLoggingDictionary': ['<WebCore/DiagnosticLoggingClient.h>'],
        'WebCore::DiagnosticLoggingDomain': ['<WebCore/DiagnosticLoggingDomain.h>'],
        'WebCore::DictationContext': ['<WebCore/DictationContext.h>'],
        'WebCore::DMABufBufferAttributes': ['<WebCore/DMABufBuffer.h>'],
        'WebCore::DocumentMarkerLineStyle': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::DocumentSyncSerializationData': ['<WebCore/DocumentSyncData.h>'],
        'WebCore::DOMCacheIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::DOMCacheEngine::CacheIdentifierOrError': ['<WebCore/DOMCacheEngine.h>'],
        'WebCore::DOMCacheEngine::RemoveCacheIdentifierOrError': ['<WebCore/DOMCacheEngine.h>'],
        'WebCore::DOMPasteAccessCategory': ['<WebCore/DOMPasteAccess.h>'],
        'WebCore::DOMPasteAccessResponse': ['<WebCore/DOMPasteAccess.h>'],
        'WebCore::DragApplicationFlags': ['<WebCore/DragData.h>'],
        'WebCore::DragHandlingMethod': ['<WebCore/DragActions.h>'],
        'WebCore::DragOperation': ['<WebCore/DragActions.h>'],
        'WebCore::DragSourceAction': ['<WebCore/DragActions.h>'],
        'WebCore::DynamicRangeMode': ['<WebCore/PlatformScreen.h>'],
        'WebCore::ElementAnimationContext': ['<WebCore/ElementAnimationContext.h>'],
        'WebCore::ElementContext': ['<WebCore/ElementContext.h>'],
        'WebCore::EncryptionBoxType': ['<WebCore/TrackInfo.h>'],
        'WebCore::EventMakesGamepadsVisible': ['<WebCore/GamepadProviderClient.h>'],
        'WebCore::ExceptionDetails': ['<WebCore/JSDOMExceptionHandling.h>'],
        'WebCore::FontInternalAttributes': ['<WebCore/Font.h>'],
        'WebCore::FileChooserSettings': ['<WebCore/FileChooser.h>'],
        'WebCore::FillLightMode': ['<WebCore/FillLightMode.h>'],
        'WebCore::FirstPartyWebsiteDataRemovalMode': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::FontChanges': ['<WebCore/FontAttributeChanges.h>'],
        'WebCore::FontPlatformDataAttributes': ['<WebCore/FontPlatformData.h>'],
        'WebCore::FontCustomPlatformSerializedData': ['<WebCore/FontCustomPlatformData.h>'],
        'WebCore::FontSmoothingMode': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::FoundElementInRemoteFrame': ['<WebCore/FocusControllerTypes.h>'],
        'WebCore::FragmentedSharedBuffer': ['<WebCore/SharedBuffer.h>'],
        'WebCore::FrameIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::FrameLoadType': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::FrameTreeSyncSerializationData': ['<WebCore/FrameTreeSyncData.h>'],
        'WebCore::FloatBoxExtent': ['"PageClient.h"'],
        'WebCore::FromDownloadAttribute': ['<WebCore/LocalFrameLoaderClient.h>'],
        'WebCore::GCGLExtension': ['<WebCore/GCGLExtension.h>'],
        'WebCore::GenericCueData': ['<WebCore/InbandGenericCue.h>'],
        'WebCore::GlyphBufferAdvance': ['<WebCore/GlyphBufferMembers.h>'],
        'WebCore::GlyphBufferGlyph': ['<WebCore/GlyphBufferMembers.h>'],
        'WebCore::GrammarDetail': ['<WebCore/TextCheckerClient.h>'],
        'WebCore::GlyphBufferAdvance': ['<WebCore/GlyphBufferMembers.h>'],
        'WebCore::GlyphBufferGlyph': ['<WebCore/GlyphBufferMembers.h>'],
        'WebCore::GraphicsContextGL::ExternalImageSource': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::GraphicsContextGL::ExternalSyncSource': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::GCGLAttribActiveInfo': ['<WebCore/GraphicsContextGLActiveInfo.h>'],
        'WebCore::GCGLUniformActiveInfo': ['<WebCore/GraphicsContextGLActiveInfo.h>'],
        'WebCore::GCGLTransformFeedbackActiveInfo': ['<WebCore/GraphicsContextGLActiveInfo.h>'],
        'WebCore::GraphicsContextGLFlipY': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::GraphicsContextGLSimulatedEventForTesting': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::GraphicsContextGLSurfaceBuffer': ['<WebCore/GraphicsContextGL.h>'],
        'WebCore::GraphicsDropShadow': ['<WebCore/GraphicsStyle.h>'],
        'WebCore::HasAvailableTargets': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::HasInsecureContent': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::HasOrShouldIgnoreUserGesture': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::Headroom': ['<WebCore/ImageTypes.h>'],
        'WebCore::HighlightRequestOriginatedInApp': ['<WebCore/AppHighlight.h>'],
        'WebCore::HighlightVisibility': ['<WebCore/HighlightVisibility.h>'],
        'WebCore::InterpolationQuality': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::ImageBufferParameters': ['<WebCore/ImageBuffer.h>'],
        'WebCore::ImageDecoderFrameInfo': ['<WebCore/ImageDecoder.h>'],
        'WebCore::ImageDecodingError': ['<WebCore/ImageUtilities.h>'],
        'WebCore::InbandTextTrackPrivateMode': ['<WebCore/InbandTextTrackPrivate.h>'],
        'WebCore::IncludeSecureCookies': ['<WebCore/CookieJar.h>'],
        'WebCore::IndexIDToIndexKeyMap': ['<WebCore/IndexKey.h>'],
        'WebCore::IndexedDB::ObjectStoreOverwriteMode': ['<WebCore/IndexedDB.h>'],
        'WebCore::InputMode': ['<WebCore/InputMode.h>'],
        'WebCore::InspectorBackendClientDeveloperPreference': ['<WebCore/InspectorBackendClient.h>'],
        'WebCore::InspectorFrontendClientAppearance': ['<WebCore/InspectorFrontendClient.h>'],
        'WebCore::InspectorFrontendClientSaveData': ['<WebCore/InspectorFrontendClient.h>'],
        'WebCore::InspectorOverlayHighlight': ['<WebCore/InspectorOverlay.h>'],
        'WebCore::IsLoggedIn': ['<WebCore/IsLoggedIn.h>'],
        'WebCore::IDBResourceObjectIdentifier': ['<WebCore/IDBResourceIdentifier.h>'],
        'WebCore::ISOWebVTTCue': ['<WebCore/ISOVTTCue.h>'],
        'WebCore::KeyframeValueList': ['<WebCore/GraphicsLayer.h>'],
        'WebCore::KeypressCommand': ['<WebCore/KeyboardEvent.h>'],
        'WebCore::LastNavigationWasAppInitiated': ['<WebCore/ServiceWorkerClientData.h>'],
        'WebCore::LegacyCDMSessionClient::MediaKeyErrorCode': ['<WebCore/LegacyCDMSession.h>'],
        'WebCore::LineCap': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::LineJoin': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::PackedColor::RGBA': ['<WebCore/ColorTypes.h>'],
        'WebCore::PaginationMode': ['<WebCore/Pagination.h>'],
        'WebCore::PlatformLayerIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::PlatformMediaSessionRemoteControlCommandType': ['<WebCore/PlatformMediaSession.h>'],
        'WebCore::PlatformMediaSessionRemoteCommandArgument': ['<WebCore/PlatformMediaSession.h>'],
        'WebCore::PlayingToAutomotiveHeadUnit': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::PlaybackSessionModelExternalPlaybackTargetType': ['<WebCore/PlaybackSessionModel.h>'],
        'WebCore::LockBackForwardList': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::MediaPlaybackTargetMockState': ['<WebCore/MediaPlaybackTargetMock.h>'],
        'WebCore::MediaPlayerBufferingPolicy': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerMediaEngineIdentifier': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerPitchCorrectionAlgorithm': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerPreload': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerSupportsType': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerVideoGravity': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaEngineSupportParameters': ['<WebCore/MediaPlayer.h>'],
        'WebCore::MediaPlayerLoadOptions': ['<WebCore/MediaPlayer.h>'],
        'WebCore::MediaPlayerReadyState': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaPlayerSoundStageSize': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::MediaProducerMediaCaptureKind': ['<WebCore/MediaProducer.h>'],
        'WebCore::MediaProducerMediaState': ['<WebCore/MediaProducer.h>'],
        'WebCore::MediaProducerMutedState': ['<WebCore/MediaProducer.h>'],
        'WebCore::MediaPromise::Result': ['<WebCore/MediaPromiseTypes.h>'],
        'WebCore::MediaSamplesBlock::MediaSampleItem': ['<WebCore/MediaSample.h>'],
        'WebCore::MediaSettingsRange': ['<WebCore/MediaSettingsRange.h>'],
        'WebCore::MediaSourcePrivateAddStatus': ['<WebCore/MediaSourcePrivate.h>'],
        'WebCore::MediaSourcePrivateEndOfStreamStatus': ['<WebCore/MediaSourcePrivate.h>'],
        'WebCore::MessagePortChannelProvider::HasActivity': ['<WebCore/MessagePortChannelProvider.h>'],
        'WebCore::ModalContainerControlType': ['<WebCore/ModalContainerTypes.h>'],
        'WebCore::ModalContainerDecision': ['<WebCore/ModalContainerTypes.h>'],
        'WebCore::MouseEventPolicy': ['<WebCore/DocumentLoader.h>'],
        'WebCore::NetworkTransactionInformation': ['<WebCore/NetworkLoadInformation.h>'],
        'WebCore::NowPlayingMetadata': ['<WebCore/NowPlayingInfo.h>'],
        'WebCore::OpaqueOriginIdentifier': ['<WebCore/SecurityOriginData.h>'],
        'WebCore::PasteboardCustomData': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardBuffer': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardImage': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardURL': ['<WebCore/Pasteboard.h>'],
        'WebCore::PasteboardWebContent': ['<WebCore/Pasteboard.h>'],
        'WebCore::PathArc': ['<WebCore/PathSegmentData.h>'],
        'WebCore::PathClosedArc': ['<WebCore/PathSegmentData.h>'],
        'WebCore::PathDataBezierCurve': ['<WebCore/PathSegmentData.h>'],
        'WebCore::PathDataLine': ['<WebCore/PathSegmentData.h>'],
        'WebCore::PathDataQuadCurve': ['<WebCore/PathSegmentData.h>'],
        'WebCore::PatternParameters': ['<WebCore/Pattern.h>'],
        'WebCore::PixelFormat': ['<WebCore/ImageBufferBackend.h>'],
        'WebCore::PhotoCapabilitiesOrError': ['<WebCore/RealtimeMediaSource.h>'],
        'WebCore::PlatformTextTrackData': ['<WebCore/PlatformTextTrack.h>'],
        'WebCore::PlatformEventModifier': ['<WebCore/PlatformEvent.h>'],
        'WebCore::PlatformWheelEventPhase': ['<WebCore/PlatformWheelEvent.h>'],
        'WebCore::PlaybackSessionModelPlaybackState': ['<WebCore/PlaybackSessionModel.h>'],
        'WebCore::PlaybackTargetClientContextID': ['<WebCore/PlaybackTargetClientContextIdentifier.h>'],
        'WebCore::PluginInfo': ['<WebCore/PluginData.h>'],
        'WebCore::PolicyAction': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::PreserveResolution': ['<WebCore/ImageBufferBackend.h>'],
        'WebCore::ProcessIdentifier': ['<WebCore/ProcessIdentifier.h>'],
        'WebCore::PushSubscriptionIdentifier': ['<WebCore/PushSubscriptionIdentifier.h>'],
        'WebCore::PixelBufferSourceView': ['<WebCore/PixelBuffer.h>'],
        'WebCore::ReasonForDismissingAlternativeText': ['<WebCore/AlternativeTextClient.h>'],
        'WebCore::RecentSearch': ['<WebCore/SearchPopupMenu.h>'],
        'WebCore::RedEyeReduction': ['<WebCore/RedEyeReduction.h>'],
        'WebCore::ResourceResponseSource': ['<WebCore/ResourceResponseBase.h>'],
        'WebCore::ReloadOption': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::RenderAsTextFlag': ['<WebCore/RenderTreeAsText.h>'],
        'WebCore::RenderingPurpose': ['<WebCore/RenderingMode.h>'],
        'WebCore::RequestStorageAccessResult': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::RequiresClipToRect': ['<WebCore/GraphicsContext.h>'],
        'WebCore::RequiresScriptTrackingPrivacy': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::RouteSharingPolicy': ['<WebCore/AudioSession.h>'],
        'WebCore::RubberBandingBehavior': ['<WebCore/ScrollTypes.h>'],
        'WebCore::SameSiteStrictEnforcementEnabled': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::SamplesRendererTrackIdentifier':  ['<WebCore/AudioVideoRenderer.h>'],
        'WebCore::ScriptExecutionContextIdentifier': ['<WebCore/ProcessQualified.h>', '<WebCore/ScriptExecutionContextIdentifier.h>', '<wtf/ObjectIdentifier.h>'],
        'WebCore::ScriptTrackingPrivacyFlag': ['<WebCore/ScriptTrackingPrivacyCategory.h>'],
        'WebCore::ScheduleLocationChangeResult': ['<WebCore/NavigationScheduler.h>'],
        'WebCore::ScrollUpdate': ['<WebCore/ScrollingCoordinatorTypes.h>'],
        'WebCore::ScrollbarMode': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollbarOverlayStyle': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollDirection': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollIsAnimated': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollGranularity': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollPinningBehavior': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollbarOrientation': ['<WebCore/ScrollTypes.h>'],
        'WebCore::ScrollingNodeIdentifier': ['"GeneratedSerializers.h"'],
        'WebCore::SecurityPolicyViolationEventInit': ['<WebCore/SecurityPolicyViolationEvent.h>'],
        'WebCore::SeekTarget': ['<WebCore/MediaPlayer.h>'],
        'WebCore::SelectionDirection': ['<WebCore/VisibleSelection.h>'],
        'WebCore::SelectionGeometry': ['"EditorState.h"'],
        'WebCore::ServiceWorkerIsInspectable': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerJobIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerOrClientData': ['<WebCore/ServiceWorkerTypes.h>', '<WebCore/ServiceWorkerClientData.h>', '<WebCore/ServiceWorkerData.h>'],
        'WebCore::ServiceWorkerOrClientIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerRegistrationIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerRegistrationState': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ServiceWorkerState': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ShapeDetection::BarcodeDetectorOptions': ['<WebCore/BarcodeDetectorOptionsInterface.h>'],
        'WebCore::ShapeDetection::BarcodeFormat': ['<WebCore/BarcodeFormatInterface.h>'],
        'WebCore::ShapeDetection::DetectedBarcode': ['<WebCore/DetectedBarcodeInterface.h>'],
        'WebCore::ShapeDetection::DetectedFace': ['<WebCore/DetectedFaceInterface.h>'],
        'WebCore::ShapeDetection::DetectedText': ['<WebCore/DetectedTextInterface.h>'],
        'WebCore::ShapeDetection::FaceDetectorOptions': ['<WebCore/FaceDetectorOptionsInterface.h>'],
        'WebCore::ShareableResourceHandle': ['<WebCore/ShareableResource.h>'],
        'WebCore::SharedWorkerObjectIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::ShareDataWithParsedURL': ['<WebCore/ShareData.h>'],
        'WebCore::ShouldContinuePolicyCheck': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::ShouldGoToHistoryItem': ['<WebCore/LocalFrameLoaderClient.h>'],
        'WebCore::ShouldNotifyWhenResolved': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::ShouldPause': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::ShouldSample': ['<WebCore/DiagnosticLoggingClient.h>'],
        'WebCore::SourceBufferAppendMode': ['<WebCore/SourceBufferPrivate.h>'],
        'WebCore::SourceBufferEvictionData': ['<WebCore/SourceBufferPrivateClient.h>'],
        'WebCore::StageModeOperation': ['<WebCore/StageModeOperations.h>'],
        'WebCore::StorageAccessPromptWasShown': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::StorageAccessScope': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::StorageAccessWasGranted': ['<WebCore/DocumentStorageAccess.h>'],
        'WebCore::StrokeStyle': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::SupportedPluginIdentifier': ['<WebCore/PluginData.h>'],
        'WebCore::SupportsAirPlayVideo': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::SupportsSpatialAudioPlayback': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::SuspendedUnderLock': ['<WebCore/MediaSessionHelperIOS.h>'],
        'WebCore::SWServerConnectionIdentifier': ['<WebCore/ServiceWorkerTypes.h>'],
        'WebCore::TargetedElementAdjustment': ['<WebCore/ElementTargetingTypes.h>'],
        'WebCore::TargetedElementInfo': ['<WebCore/ElementTargetingTypes.h>'],
        'WebCore::TargetedElementRequest': ['<WebCore/ElementTargetingTypes.h>'],
        'WebCore::TextAnimationData': ['<WebCore/TextAnimationTypes.h>'],
        'WebCore::TextAnimationRunMode': ['<WebCore/TextAnimationTypes.h>'],
        'WebCore::TextAnimationType': ['<WebCore/TextAnimationTypes.h>'],
        'WebCore::TextCheckingRequestData': ['<WebCore/TextChecking.h>'],
        'WebCore::TextCheckingResult': ['<WebCore/TextCheckerClient.h>'],
        'WebCore::TextCheckingType': ['<WebCore/TextChecking.h>'],
        'WebCore::TextDrawingModeFlags': ['<WebCore/GraphicsTypes.h>'],
        'WebCore::TextExtraction::Item': ['<WebCore/TextExtractionTypes.h>'],
        'WebCore::TextIndicatorData': ['<WebCore/TextIndicator.h>'],
        'WebCore::TextIndicatorLifetime': ['<WebCore/TextIndicator.h>'],
        'WebCore::TextManipulationControllerManipulationResult': ['<WebCore/TextManipulationControllerManipulationFailure.h>'],
        'WebCore::TextManipulationTokenIdentifier': ['<WebCore/TextManipulationToken.h>'],
        'WebCore::ThirdPartyCookieBlockingMode': ['<WebCore/NetworkStorageSession.h>'],
        'WebCore::TrackID': ['<WebCore/TrackBase.h>'],
        'WebCore::TrackInfo::TrackType': ['<WebCore/TrackInfo.h>'],
        'WebCore::TrackInfoTrackType': ['<WebCore/TrackInfo.h>'],
        'WebCore::UserGestureTokenIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::VideoInfo': ['<WebCore/TrackInfo.h>'],
        'WebCore::VideoRendererPreference': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::VideoRendererPreferences': ['<WebCore/MediaPlayerEnums.h>'],
        'WebCore::WindowIdentifier': ['<WebCore/GlobalWindowIdentifier.h>'],
        'WebCore::WritingTools::Context': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::ContextID': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Action': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::TextSuggestion': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Behavior': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Session': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Session::Type': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Session::CompositionType': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::Session::ID': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::TextSuggestion::ID': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::WritingTools::TextSuggestionState': ['<WebCore/WritingToolsTypes.h>'],
        'WebCore::UsedLegacyTLS': ['<WebCore/ResourceResponseBase.h>'],
        'WebCore::VideoFrameRotation': ['<WebCore/VideoFrame.h>'],
        'WebCore::VideoPlaybackQualityMetrics': ['<WebCore/VideoPlaybackQualityMetrics.h>'],
        'WebCore::VideoPresetData': ['<WebCore/VideoPreset.h>'],
        'WebCore::JSHandleIdentifier': ['<WebCore/WebKitJSHandle.h>'],
        'WebCore::WebGPU::AddressMode': ['<WebCore/WebGPUAddressMode.h>'],
        'WebCore::WebGPU::BlendFactor': ['<WebCore/WebGPUBlendFactor.h>'],
        'WebCore::WebGPU::BlendOperation': ['<WebCore/WebGPUBlendOperation.h>'],
        'WebCore::WebGPU::BufferBindingType': ['<WebCore/WebGPUBufferBindingType.h>'],
        'WebCore::WebGPU::BufferDynamicOffset': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::BufferUsageFlags': ['<WebCore/WebGPUBufferUsage.h>'],
        'WebCore::WebGPU::CanvasAlphaMode': ['<WebCore/WebGPUCanvasAlphaMode.h>'],
        'WebCore::WebGPU::CanvasToneMappingMode': ['<WebCore/WebGPUCanvasToneMappingMode.h>'],
        'WebCore::WebGPU::ColorWriteFlags': ['<WebCore/WebGPUColorWrite.h>'],
        'WebCore::WebGPU::CompareFunction': ['<WebCore/WebGPUCompareFunction.h>'],
        'WebCore::WebGPU::CompilationMessageType': ['<WebCore/WebGPUCompilationMessageType.h>'],
        'WebCore::WebGPU::CullMode': ['<WebCore/WebGPUCullMode.h>'],
        'WebCore::WebGPU::DepthBias': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::DeviceLostReason': ['<WebCore/WebGPUDeviceLostReason.h>'],
        'WebCore::WebGPU::ErrorFilter': ['<WebCore/WebGPUErrorFilter.h>'],
        'WebCore::WebGPU::FeatureName': ['<WebCore/WebGPUFeatureName.h>'],
        'WebCore::WebGPU::FilterMode': ['<WebCore/WebGPUFilterMode.h>'],
        'WebCore::WebGPU::FlagsConstant': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::FrontFace': ['<WebCore/WebGPUFrontFace.h>'],
        'WebCore::WebGPU::Index32': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::IndexFormat': ['<WebCore/WebGPUIndexFormat.h>'],
        'WebCore::WebGPU::IntegerCoordinate': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::LoadOp': ['<WebCore/WebGPULoadOp.h>'],
        'WebCore::WebGPU::MapModeFlags': ['<WebCore/WebGPUMapMode.h>'],
        'WebCore::WebGPU::PipelineStatisticName': ['<WebCore/WebGPUPipelineStatisticName.h>'],
        'WebCore::WebGPU::PowerPreference': ['<WebCore/WebGPUPowerPreference.h>'],
        'WebCore::WebGPU::PredefinedColorSpace': ['<WebCore/WebGPUPredefinedColorSpace.h>'],
        'WebCore::WebGPU::PrimitiveTopology': ['<WebCore/WebGPUPrimitiveTopology.h>'],
        'WebCore::WebGPU::QueryType': ['<WebCore/WebGPUQueryType.h>'],
        'WebCore::WebGPU::SampleMask': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::SamplerBindingType': ['<WebCore/WebGPUSamplerBindingType.h>'],
        'WebCore::WebGPU::ShaderStageFlags': ['<WebCore/WebGPUShaderStage.h>'],
        'WebCore::WebGPU::SignedOffset32': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::Size32': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::Size64': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::StencilOperation': ['<WebCore/WebGPUStencilOperation.h>'],
        'WebCore::WebGPU::StencilValue': ['<WebCore/WebGPUIntegralTypes.h>'],
        'WebCore::WebGPU::StorageTextureAccess': ['<WebCore/WebGPUStorageTextureAccess.h>'],
        'WebCore::WebGPU::StoreOp': ['<WebCore/WebGPUStoreOp.h>'],
        'WebCore::WebGPU::TextureAspect': ['<WebCore/WebGPUTextureAspect.h>'],
        'WebCore::WebGPU::TextureDimension': ['<WebCore/WebGPUTextureDimension.h>'],
        'WebCore::WebGPU::TextureFormat': ['<WebCore/WebGPUTextureFormat.h>'],
        'WebCore::WebGPU::TextureSampleType': ['<WebCore/WebGPUTextureSampleType.h>'],
        'WebCore::WebGPU::TextureUsageFlags': ['<WebCore/WebGPUTextureUsage.h>'],
        'WebCore::WebGPU::TextureViewDimension': ['<WebCore/WebGPUTextureViewDimension.h>'],
        'WebCore::WebGPU::VertexFormat': ['<WebCore/WebGPUVertexFormat.h>'],
        'WebCore::WebGPU::VertexStepMode': ['<WebCore/WebGPUVertexStepMode.h>'],
        'WebCore::WebGPU::XREye': ['<WebCore/WebGPUXREye.h>'],
        'WebCore::WebLockIdentifierID': ['"GeneratedSerializers.h"'],
        'WebCore::WheelEventProcessingSteps': ['<WebCore/ScrollingCoordinatorTypes.h>'],
        'WebCore::WheelEventTestMonitorDeferReason': ['<WebCore/WheelEventTestMonitor.h>'],
        'WebCore::WheelScrollGestureState': ['<WebCore/PlatformWheelEvent.h>'],
        'WebCore::WillContinueLoading': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::WillInternallyHandleFailure': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::WindowProxyProperty': ['<WebCore/FrameLoaderTypes.h>'],
        'WebCore::WebTransportStreamIdentifier': ['"WebTransportSession.h"'],
        'WebCore::WebTransportSendGroupIdentifier': ['<WebCore/WebTransportSendGroup.h>'],
        'WebKit::ActivityStateChangeID': ['"DrawingAreaInfo.h"'],
        'WebKit::AllowOverwrite': ['"DownloadID.h"'],
        'WebKit::AppPrivacyReportTestingData': ['"AppPrivacyReport.h"'],
        'WebKit::AuthenticationChallengeIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::BufferInSetType': ['"BufferIdentifierSet.h"'],
        'WebKit::BufferSetBackendHandle': ['"BufferAndBackendInfo.h"'],
        'WebKit::CallDownloadDidStart': ['"DownloadManager.h"'],
        'WebKit::ConsumerSharedCARingBufferHandle': ['"SharedCARingBuffer.h"'],
        'WebKit::ContentWorldIdentifier': ['"ContentWorldShared.h"'],
        'WebKit::ContentWorldData': ['"ContentWorldData.h"'],
        'WebKit::ContentWorldOption': ['"ContentWorldShared.h"'],
        'WebKit::DocumentEditingContextRequest': ['"DocumentEditingContext.h"'],
        'WebKit::DrawingAreaIdentifier': ['"DrawingAreaInfo.h"'],
        'WebKit::FindDecorationStyle': ['"WebFindOptions.h"'],
        'WebKit::FindOptions': ['"WebFindOptions.h"'],
        'WebKit::FrameState': ['"SessionState.h"'],
        'WebKit::GestureRecognizerState': ['"GestureTypes.h"'],
        'WebKit::GestureType': ['"GestureTypes.h"'],
        'WebKit::RiceBackendIdentifier': ['"RiceBackend.h"'],
        'WebKit::JSObjectID': ['"JavaScriptEvaluationResult.h"'],
        'WebKit::SnapshotOption': ['"ImageOptions.h"'],
        'WebKit::LastNavigationWasAppInitiated': ['"AppPrivacyReport.h"'],
        'WebKit::LayerHostingContextID': ['"LayerHostingContext.h"'],
        'WebKit::MediaTimeUpdateData': ['"MediaPlayerPrivateRemote.h"'],
        'WebKit::MessageBatchIdentifier': ['"NetworkConnectionToWebProcess.h"'],
        'WebKit::PageGroupIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::PaymentSetupConfiguration': ['"PaymentSetupConfigurationWebKit.h"'],
        'WebKit::PaymentSetupFeatures': ['"ApplePayPaymentSetupFeaturesWebKit.h"'],
        'WebKit::ImageBufferSetPrepareBufferForDisplayInputData': ['"PrepareBackingStoreBuffersData.h"'],
        'WebKit::ImageBufferSetPrepareBufferForDisplayOutputData': ['"PrepareBackingStoreBuffersData.h"'],
        'WebKit::WebRTCNetwork::EcnMarking': ['"RTCNetwork.h"'],
        'WebKit::WebRTCNetwork::IPAddress': ['"RTCNetwork.h"'],
        'WebKit::WebRTCNetwork::SocketAddress': ['"RTCNetwork.h"'],
        'WebKit::ReloadFromOrigin': ['"WebExtensionTab.h"'],
        'WebKit::RemoteLayerBackingStoreProperties': ['"RemoteLayerBackingStore.h"'],
        'WebKit::RemoteVideoFrameReadReference': ['"RemoteVideoFrameIdentifier.h"'],
        'WebKit::RemoteVideoFrameWriteReference': ['"RemoteVideoFrameIdentifier.h"'],
        'WebKit::RespectSelectionAnchor': ['"GestureTypes.h"'],
        'WebKit::SandboxExtensionHandle': ['"SandboxExtension.h"'],
        'WebKit::ScriptTrackingPrivacyHost': ['"ScriptTrackingPrivacyFilter.h"'],
        'WebKit::ScriptTrackingPrivacyRules': ['"ScriptTrackingPrivacyFilter.h"'],
        'WebKit::SelectionFlags': ['"GestureTypes.h"'],
        'WebKit::SelectionTouch': ['"GestureTypes.h"'],
        'WebKit::SwapBuffersDisplayRequirement': ['"PrepareBackingStoreBuffersData.h"'],
        'WebKit::TapIdentifier': ['"IdentifierTypes.h"'],
        'WebKit::TextCheckerRequestID': ['"IdentifierTypes.h"'],
        'WebKit::TextInteractionSource': ['"GestureTypes.h"'],
        'WebKit::WebEventType': ['"WebEvent.h"'],
        'WebKit::WebExtensionContextInstallReason': ['"WebExtensionContext.h"'],
        'WebKit::WebExtensionCookieFilterParameters': ['"WebExtensionCookieParameters.h"'],
        'WebKit::WebExtensionError': ['"WebExtensionError.h"'],
        'WebKit::WebExtensionTabImageFormat': ['"WebExtensionTab.h"'],
        'WebKit::WebExtensionWindowTypeFilter': ['"WebExtensionWindow.h"'],
        'WebKit::DDModel::Identifier': ['"DDModelIdentifier.h"'],
        'WebKit::DDModel::ObjectDescriptorBase': ['"ModelObjectDescriptorBase.h"'],
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
        'WebKit::WebGPU::InternalError': ['"WebGPUInternalError.h"'],
        'WebKit::WebGPU::MultisampleState': ['"WebGPUMultisampleState.h"'],
        'WebKit::WebGPU::ObjectDescriptorBase': ['"WebGPUObjectDescriptorBase.h"'],
        'WebKit::WebGPU::Origin2D': ['"WebGPUOrigin2D.h"'],
        'WebKit::WebGPU::Origin3D': ['"WebGPUOrigin3D.h"'],
        'WebKit::WebGPU::OutOfMemoryError': ['"WebGPUOutOfMemoryError.h"'],
        'WebKit::WebGPU::PipelineDescriptorBase': ['"WebGPUPipelineDescriptorBase.h"'],
        'WebKit::WebGPU::PipelineLayoutDescriptor': ['"WebGPUPipelineLayoutDescriptor.h"'],
        'WebKit::WebGPU::PresentationContextDescriptor': ['"WebGPUPresentationContextDescriptor.h"'],
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
        'WebKit::WebGPU::TextureBindingLayout': ['"WebGPUTextureBindingLayout.h"'],
        'WebKit::WebGPU::TextureDescriptor': ['"WebGPUTextureDescriptor.h"'],
        'WebKit::WebGPU::TextureViewDescriptor': ['"WebGPUTextureViewDescriptor.h"'],
        'WebKit::WebGPU::ValidationError': ['"WebGPUValidationError.h"'],
        'WebKit::WebGPU::VertexAttribute': ['"WebGPUVertexAttribute.h"'],
        'WebKit::WebGPU::VertexBufferLayout': ['"WebGPUVertexBufferLayout.h"'],
        'WebKit::WebGPU::VertexState': ['"WebGPUVertexState.h"'],
        'WebKit::WebGPU::XREye': ['"WebGPUXREye.h"'],
        'WebKit::WebJSBufferData': ['"WebUserContentControllerDataTypes.h"'],
        'WebKit::WebPushD::PushMessageForTesting': ['"PushMessageForTesting.h"'],
        'WebKit::WebPushD::WebPushDaemonConnectionConfiguration': ['"WebPushDaemonConnectionConfiguration.h"'],
        'WebKit::WebScriptMessageHandlerData': ['"WebUserContentControllerDataTypes.h"'],
        'WebKit::WebTransportSessionIdentifier': ['"WebTransportSession.h"'],
        'WebKit::WebUserScriptData': ['"WebUserContentControllerDataTypes.h"'],
        'WebKit::WebUserStyleSheetData': ['"WebUserContentControllerDataTypes.h"'],
        'WTF::UnixFileDescriptor': ['<wtf/unix/UnixFileDescriptor.h>'],
        'WTF::SystemMemoryPressureStatus': ['<wtf/MemoryPressureHandler.h>'],
        'webrtc::WebKitEncodedFrameInfo': ['"RTCWebKitEncodedFrameInfo.h"'],
    }

    headers = []
    for header_info in header_infos_and_types['header_infos']:
        headers += header_info['headers']

    for type in header_infos_and_types['types']:
        if type.startswith("const "):
            type = type[6:]
        if type in special_cases:
            headers += special_cases[type]
            continue

        if not for_implementation_file and type not in types_that_cannot_be_forward_declared():
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

    if receiver.receiver_enabled_by:
        header_conditions['"SharedPreferencesForWebProcess.h"'] = [None]

    for parameter in receiver.iterparameters():
        type = parameter.type
        conditions = type_conditions[type]

        argument_encoder_headers = argument_coder_headers_for_type(type)
        if argument_encoder_headers:
            for header in argument_encoder_headers:
                if header not in header_conditions:
                    header_conditions[header] = []
                header_conditions[header].extend(conditions)

        type_headers = headers_for_type(type, True)
        for header in type_headers:
            if header not in header_conditions:
                header_conditions[header] = []
            header_conditions[header].extend(conditions)

    for message in receiver.messages:
        if message.enabled_by:
            header_conditions['"SharedPreferencesForWebProcess.h"'] = [None]
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


def generate_dispatched_for_x(dispatched_x, spacing='    '):
    if dispatched_x == "WebContent":
        return ['%sASSERT(isInWebProcess());\n' % spacing]
    elif dispatched_x == "Networking":
        return ['%sASSERT(isInNetworkProcess());\n' % spacing]
    elif dispatched_x == "GPU":
        return ['%sASSERT(isInGPUProcess());\n' % spacing]
    elif dispatched_x == "UI":
        return ['%sASSERT(!isInAuxiliaryProcess());\n' % spacing]
    elif dispatched_x == "Model":
        return ['%sASSERT(isInModelProcess());\n' % spacing]
    return []


def generate_enabled_by_for_receiver(receiver, messages):
    enabled_by = receiver.receiver_enabled_by
    enabled_by_conjunction = receiver.receiver_enabled_by_conjunction
    shared_preferences_retrieval = [
        '    auto sharedPreferences = sharedPreferencesForWebProcess(%s);\n' % ('connection' if receiver.shared_preferences_needs_connection else ''),
        '    UNUSED_VARIABLE(sharedPreferences);\n'
    ]
    result = []
    if not enabled_by:
        if any([message.enabled_by for message in messages]):
            result += shared_preferences_retrieval
        return result

    runtime_enablement = generate_enabled_by(receiver, enabled_by, enabled_by_conjunction)
    return shared_preferences_retrieval + [
        '    if (!sharedPreferences || !%s) {\n' % ('(%s)' % runtime_enablement if len(enabled_by) > 1 else runtime_enablement),
        '        RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message receiver %s", IPC::description(decoder.messageName()).characters());\n' % ('%s', receiver.name),
        '        decoder.markInvalid();\n',
        '        return;\n',
        '    }\n',
    ]


def generate_message_handler(receiver):
    header_conditions = {
        '"%s"' % messages_header_filename(receiver): [None],
        '"HandleMessage.h"': [None],
        '"Decoder.h"': [None],
    }

    collect_header_conditions_for_receiver(receiver, header_conditions)

    result = []

    result.append(_license_header)
    result.append('\n')
    result.append('#include "config.h"\n')

    if receiver.condition:
        result.append('#if %s\n' % receiver.condition)

    result.append('#include "%s.h"\n\n' % receiver.name)
    result += generate_header_includes_from_conditions(header_conditions)
    result.append('\n')

    result.append('#if ENABLE(IPC_TESTING_API)\n')
    result.append('#include "JSIPCBinding.h"\n')
    result.append("#endif\n\n")

    result.append('namespace %s {\n\n' % receiver.namespace)

    async_messages = []
    sync_messages = []
    for message in receiver.messages:
        if message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
            sync_messages.append(message)
        else:
            async_messages.append(message)

    def collect_message_statements(messages, message_statement_function):
        result = []
        for condition, messages in itertools.groupby(messages, lambda m: m.condition):
            if condition:
                result.append('#if %s\n' % condition)
            for message in messages:
                if not message.is_async_reply:
                    result += message_statement_function(receiver, message)
            if condition:
                result.append('#endif\n')
        return result

    async_message_statements = collect_message_statements(async_messages, async_message_statement)
    sync_message_statements = collect_message_statements(sync_messages, sync_message_statement)

    if receiver.has_attribute(STREAM_ATTRIBUTE):
        result.append('void %s::didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Decoder& decoder)\n' % (receiver.name))
        result.append('{\n')
        result += generate_enabled_by_for_receiver(receiver, receiver.messages)
        assert(not receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE))
        assert(not receiver.has_attribute(WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE))
        result.append('    Ref protectedThis { *this };\n')
        result += async_message_statements
        result += sync_message_statements
        if (receiver.superclass):
            result.append('    %s::didReceiveStreamMessage(connection, decoder);\n' % (receiver.superclass))
        else:
            result.append('    RELEASE_LOG_ERROR(IPC, "Unhandled stream message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());\n')
            result.append('    decoder.markInvalid();\n')
        result.append('}\n')
    else:
        if receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE):
            result.append('void %s::didReceiveMessageWithReplyHandler(IPC::Decoder& decoder, Function<void(UniqueRef<IPC::Encoder>&&)>&& replyHandler)\n' % (receiver.name))
        else:
            result.append('void %s::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)\n' % (receiver.name))
        result.append('{\n')
        enable_by_statement = generate_enabled_by_for_receiver(receiver, async_messages)
        result += enable_by_statement
        result.append('    Ref protectedThis { *this };\n')
        result += async_message_statements
        if receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE) or receiver.has_attribute(WANTS_ASYNC_DISPATCH_MESSAGE_ATTRIBUTE):
            result.append('    if (dispatchMessage(connection, decoder))\n')
            result.append('        return;\n')
        if (receiver.superclass):
            result.append('    %s::didReceiveMessage(connection, decoder);\n' % (receiver.superclass))
        else:
            if not receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE):
                result.append('    UNUSED_PARAM(connection);\n')
            result.append('    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());\n')
            result.append('    decoder.markInvalid();\n')
        result.append('}\n')

    if not receiver.has_attribute(STREAM_ATTRIBUTE) and (sync_messages or receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE)):
        result.append('\n')
        result.append('void %s::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)\n' % (receiver.name))
        result.append('{\n')
        result += generate_dispatched_for_x(receiver.receiver_dispatched_to)
        result += generate_enabled_by_for_receiver(receiver, sync_messages)
        result.append('    Ref protectedThis { *this };\n')
        result += sync_message_statements
        if receiver.has_attribute(WANTS_DISPATCH_MESSAGE_ATTRIBUTE):
            result.append('    if (dispatchSyncMessage(connection, decoder, replyEncoder))\n')
            result.append('        return;\n')
        if (receiver.superclass):
            result.append('    %s::didReceiveSyncMessage(connection, decoder, replyEncoder);\n' % (receiver.superclass))
        else:
            if not receiver.has_attribute(NOT_USING_IPC_CONNECTION_ATTRIBUTE):
                result.append('    UNUSED_PARAM(connection);\n')
            result.append('    UNUSED_PARAM(replyEncoder);\n')
            result.append('    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());\n')
            result.append('    decoder.markInvalid();\n')
        result.append('}\n')

    if receiver.wants_send_cancel_reply:
        result.append('\n')
        result.append('void %s::sendCancelReply(IPC::Connection& connection, IPC::Decoder& decoder)\n' % (receiver.name))
        result.append('{\n')
        result.append('    ASSERT(decoder.messageReceiverName() == IPC::ReceiverName::%s);\n' % (receiver.name))
        result.append('    switch (decoder.messageName()) {\n')
        for message in receiver.messages:
            if message.reply_parameters is None or message.has_attribute(SYNCHRONOUS_ATTRIBUTE):
                continue
            result.append('    case IPC::MessageName::%s_%s: {\n' % (receiver.name, message.name))
            result.append('        auto arguments = decoder.decode<typename Messages::%s::%s::Arguments>();\n' % (receiver.name, message.name))
            result.append('        if (!arguments) [[unlikely]]\n')
            result.append('            return;\n')
            result.append('        auto replyID = decoder.decode<IPC::AsyncReplyID>();\n')
            result.append('        if (!replyID) [[unlikely]]\n')
            result.append('            return;\n')
            result.append('        connection.sendAsyncReply<Messages::%s::%s>(*replyID\n' % (receiver.name, message.name))
            for parameter in message.reply_parameters:
                result.append('            , IPC::AsyncReplyError<%s>::create()\n' % (parameter.type))
            result.append('        );\n')
            result.append('        return;\n')
            result.append('    }\n')
        result.append('    default:\n')
        result.append('        // No reply to send.\n')
        result.append('        return;\n')
        result.append('    }\n')
        result.append('}\n')

    result.append('\n')
    result.append('} // namespace WebKit\n')

    result.append('\n')
    result.append('#if ENABLE(IPC_TESTING_API)\n')
    result.append('\n')
    result.append('namespace IPC {\n')
    result.append('\n')
    for condition, messages in itertools.groupby(receiver.messages, lambda m: m.condition):
        if condition:
            result.append('#if %s\n' % condition)
        for message in messages:
            result.append('template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::%s_%s>(JSC::JSGlobalObject* globalObject, Decoder& decoder)\n' % (receiver.name, message.name))
            result.append('{\n')
            result.append('    return jsValueForDecodedArguments<Messages::%s::%s::%s>(globalObject, decoder);\n' % (receiver.name, message.name, 'Arguments'))
            result.append('}\n')
            has_reply = message.reply_parameters is not None
            if has_reply:
                result.append('template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::%s_%s>(JSC::JSGlobalObject* globalObject, Decoder& decoder)\n' % (receiver.name, message.name))
                result.append('{\n')
                result.append('    return jsValueForDecodedArguments<Messages::%s::%s::%s>(globalObject, decoder);\n' % (receiver.name, message.name, 'ReplyArguments'))
                result.append('}\n')
        if condition:
            result.append('#endif\n')
    result.append('\n')
    result.append('}\n')
    result.append('\n')
    result.append('#endif\n')
    result.append('\n')

    if receiver.condition:
        result.append('\n')
        result.append('#endif // %s\n' % receiver.condition)

    return ''.join(result)


def generate_message_names_header(receivers):
    result = []
    result.append(_license_header)
    result.append('\n')
    result.append('#pragma once\n')
    result.append('\n')
    result.append('#include <algorithm>\n')
    result.append('#include <array>\n')
    result.append('#include <type_traits>\n')
    result.append('#include <wtf/EnumTraits.h>\n')
    result.append('#include <wtf/text/ASCIILiteral.h>\n')
    result.append('\n')
    result.append('namespace IPC {\n')
    result.append('\n')
    result.append('enum class ProcessName : uint8_t {\n')
    for name in PROCESS_NAMES:
        result.append('    %s,\n' % name)
    result.append('    Unknown\n')
    result.append('};\n')
    result.append('\n')
    result.append('enum class ReceiverName : uint8_t {')
    result.append('\n    ')
    enums = ['%s = %d' % (e, v) for v, e in enumerate(get_receiver_enumerators(receivers), 1)]
    result.append('\n    , '.join(enums))
    result.append('\n    , Invalid = %d' % (len(enums) + 1))
    result.append('\n};\n')
    result.append('\n')

    result.append('enum class MessageName : uint16_t {\n')
    message_enumerators = get_message_enumerators(receivers)
    seen_synchronous = False
    for (condition, synchronous), enumerators in itertools.groupby(message_enumerators, lambda e: (e.condition(), e.synchronous())):
        if synchronous and not seen_synchronous:
            result.append('    FirstSynchronous,\n')
            result.append('    LastAsynchronous = FirstSynchronous - 1,\n')
            seen_synchronous = True
        if condition:
            result.append('#if %s\n' % condition)
        for enumerator in enumerators:
            result.append('    %s,\n' % enumerator)
        if condition:
            result.append('#endif\n')
    result.append('    Count,\n')
    result.append('    Invalid = Count,\n')
    result.append('    Last = Count - 1\n')
    result.append('};\n')
    result.append('\n')
    result.append('namespace Detail {\n')
    result.append('struct MessageDescription {\n')
    result.append('    ASCIILiteral description;\n')
    result.append('    ReceiverName receiverName;\n')
    for fname, _ in sorted(attributes_to_generate_validators.items()):
        result.append('    bool %s : 1;\n' % fname)
    result.append('    bool isAsyncReply : 1;\n')
    result.append('    ProcessName dispatchedFrom;\n')
    result.append('    ProcessName dispatchedTo;\n')
    result.append('};\n')
    result.append('\n')
    result.append('using MessageDescriptionsArray = std::array<MessageDescription, static_cast<size_t>(MessageName::Count) + 1>;\n')
    result.append('extern const MessageDescriptionsArray messageDescriptions;\n\n')
    result.append('}\n')
    result.append('\n')
    fnames = [('ReceiverName', 'receiverName'), ('ASCIILiteral', 'description')]
    fnames += [('bool', fname) for fname, _ in sorted(attributes_to_generate_validators.items())]
    for returnType, fname in fnames:
        result.append('inline %s %s(MessageName messageName)\n' % (returnType, fname))
        result.append('{\n')
        result.append('    messageName = std::min(messageName, MessageName::Last);\n')
        result.append('    return Detail::messageDescriptions[static_cast<size_t>(messageName)].%s;\n' % fname)
        result.append('}\n')
        result.append('\n')
    result.append('inline bool isAsyncReply(MessageName messageName)\n')
    result.append('{\n')
    result.append('    messageName = std::min(messageName, MessageName::Last);\n')
    result.append('    return Detail::messageDescriptions[static_cast<size_t>(messageName)].isAsyncReply;\n')
    result.append('}\n')
    result.append('\n')
    result.append('constexpr bool messageIsSync(MessageName name)\n')
    result.append('{\n')
    if seen_synchronous:
        result.append('    return name >= MessageName::FirstSynchronous;\n')
    else:
        result.append('    UNUSED_PARAM(name);\n')
        result.append('    return false;\n')
    result.append('}\n')
    result.append('\n')
    result.append('ASCIILiteral processLiteral(ProcessName);\n')
    result.append('ASCIILiteral dispatchedFrom(MessageName);\n')
    result.append('ASCIILiteral dispatchedTo(MessageName);\n')
    result.append('\n')
    result.append('} // namespace IPC\n')
    result.append('\n')
    result.append('namespace WTF {\n')
    result.append('\n')
    result.append('template<> constexpr bool isValidEnum<IPC::MessageName>(std::underlying_type_t<IPC::MessageName> messageName)\n')
    result.append('{\n')
    result.append('    return messageName <= WTF::enumToUnderlyingType(IPC::MessageName::Last);\n')
    result.append('}\n')
    result.append('\n')
    result.append('} // namespace WTF\n')
    return ''.join(result)


def generate_message_names_implementation(receivers):
    result = []
    result.append(_license_header)
    result.append('\n')
    result.append('#include "config.h"\n')
    result.append('#include "MessageNames.h"\n')
    result.append('\n')
    result.append('namespace IPC::Detail {\n')
    result.append('\n')
    result.append('const MessageDescriptionsArray messageDescriptions {\n')

    message_enumerators = get_message_enumerators(receivers)
    for condition, enumerators in itertools.groupby(message_enumerators, lambda e: e.condition()):
        if condition:
            result.append('#if %s\n' % condition)
        for enumerator in enumerators:
            result.append('    MessageDescription { "%s"_s, ReceiverName::%s' % (enumerator, enumerator.receiver.name))
            for attr_list in sorted(attributes_to_generate_validators.values()):
                value = "true" if (set(attr_list).intersection(set(enumerator.messages[0].attributes).union(set(enumerator.receiver.attributes))) and not enumerator.messages[0].is_async_reply) else "false"
                result.append(', %s' % value)
            result.append(', %s' % ("true" if enumerator.messages[0].is_async_reply else "false"))
            if enumerator.messages[0].is_async_reply:
                result.append(', ProcessName::%s' % (enumerator.receiver.receiver_dispatched_to or "Unknown"))
                result.append(', ProcessName::%s' % (enumerator.receiver.receiver_dispatched_from or "Unknown"))
            else:
                result.append(', ProcessName::%s' % (enumerator.receiver.receiver_dispatched_from or "Unknown"))
                result.append(', ProcessName::%s' % (enumerator.receiver.receiver_dispatched_to or "Unknown"))
            result.append(' },\n')
        if condition:
            result.append('#endif\n')
    result.append('    MessageDescription { "<invalid message name>"_s, ReceiverName::Invalid%s, false, ProcessName::Unknown, ProcessName::Unknown }\n' % (", false" * len(attributes_to_generate_validators)))
    result.append('};\n')
    result.append('\n')
    result.append('} // namespace IPC::Detail\n')
    result.append('\n')
    result.append('namespace IPC {\n')
    result.append('\n')
    result.append('ASCIILiteral processLiteral(ProcessName name)\n')
    result.append('{\n')
    result.append('    switch (name) {\n')
    for name in PROCESS_NAMES:
        result.append('    case ProcessName::%s:\n' % name)
        result.append('        return "%s";\n' % name)
    result.append('    case ProcessName::Unknown:\n')
    result.append('        return "Unknown";\n')
    result.append('    default:\n')
    result.append('        RELEASE_ASSERT_NOT_REACHED();\n')
    result.append('    }\n')
    result.append('};\n')
    result.append('\n')
    result.append('ASCIILiteral dispatchedFrom(MessageName name)\n')
    result.append('{\n')
    result.append('    return processLiteral(Detail::messageDescriptions[static_cast<size_t>(name)].dispatchedFrom);\n')
    result.append('};\n')
    result.append('\n')
    result.append('ASCIILiteral dispatchedTo(MessageName name)\n')
    result.append('{\n')
    result.append('    return processLiteral(Detail::messageDescriptions[static_cast<size_t>(name)].dispatchedTo);\n')
    result.append('};\n')
    result.append('\n')
    result.append('} // namespace IPC\n')
    return ''.join(result)


def generate_js_value_conversion_function(result, receivers, function_name, decoder_function_name, argument_type, predicate=lambda message: True):
    result.append('std::optional<JSC::JSValue> %s(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)' % function_name)
    result.append('{')
    result.append('    switch (name) {')
    for receiver in receivers:
        if receiver.has_attribute(BUILTIN_ATTRIBUTE):
            continue
        if receiver.condition:
            result.append('#if %s' % receiver.condition)
        previous_message_condition = None
        for message in receiver.messages:
            if not predicate(message):
                continue
            if previous_message_condition != message.condition:
                if previous_message_condition:
                    result.append('#endif')
                if message.condition:
                    result.append('#if %s' % message.condition)
            previous_message_condition = message.condition
            result.append('    case MessageName::%s_%s:' % (receiver.name, message.name))
            result.append('        return %s<MessageName::%s_%s>(globalObject, decoder);' % (decoder_function_name, receiver.name, message.name))
        if previous_message_condition:
            result.append('#endif')
        if receiver.condition:
            result.append('#endif')
    result.append('    default:')
    result.append('        break;')
    result.append('    }')
    result.append('    return std::nullopt;')
    result.append('}')


def generate_js_argument_descriptions(receivers, function_name, arguments_from_message):
    result = []
    result.append('std::optional<Vector<ArgumentDescription>> %s(MessageName name)' % function_name)
    result.append('{')
    result.append('    switch (name) {')
    for receiver in receivers:
        if receiver.has_attribute(BUILTIN_ATTRIBUTE):
            continue
        if receiver.condition:
            result.append('#if %s' % receiver.condition)
        previous_message_condition = None
        for message in receiver.messages:
            if message.has_attribute(BUILTIN_ATTRIBUTE):
                continue
            argument_list = arguments_from_message(message)
            if argument_list is None:
                continue
            if previous_message_condition != message.condition:
                if previous_message_condition:
                    result.append('#endif')
                if message.condition:
                    result.append('#if %s' % message.condition)
            previous_message_condition = message.condition
            result.append('    case MessageName::%s_%s:' % (receiver.name, message.name))

            if not len(argument_list):
                result.append('        return Vector<ArgumentDescription> { };')
                continue

            result.append('        return Vector<ArgumentDescription> {')
            for argument in argument_list:
                result.append('            { "%s"_s, "%s"_s },' % (argument.name, argument.type))
            result.append('        };')
        if previous_message_condition:
            result.append('#endif')
        if receiver.condition:
            result.append('#endif')
    result.append('    default:')
    result.append('        break;')
    result.append('    }')
    result.append('    return std::nullopt;')
    result.append('}')
    return result


def header_and_condition_from_parameter(parameter, message, receiver):
    type = parameter.type
    if type.startswith('std::optional<') and type.endswith('>'):
        type = type[14: len(type) - 1]
    headers = []
    for header in headers_for_type(type, True):
        conditions = []
        if message.condition is not None:
            conditions.append(message.condition)
        if receiver.condition is not None:
            conditions.append(receiver.condition)
        headers.append((header, ' && '.join(conditions)))
    return headers


def generate_message_argument_description_implementation(receivers, receiver_headers):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "MessageArgumentDescriptions.h"')
    result.append('')
    result.append('#include "JSIPCBinding.h"')
    result.append('#include "MessageNames.h"')
    result.append('')
    result.append('#if ENABLE(IPC_TESTING_API) || !LOG_DISABLED')
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    result.append('#if ENABLE(IPC_TESTING_API)')
    result.append('')

    generate_js_value_conversion_function(result, receivers, 'jsValueForArguments', 'jsValueForDecodedMessage', 'Arguments')

    result.append('')

    generate_js_value_conversion_function(result, receivers, 'jsValueForReplyArguments', 'jsValueForDecodedMessageReply', 'ReplyArguments', lambda message: message.reply_parameters is not None)

    result.append('')
    result.append('Vector<ASCIILiteral> serializedIdentifiers()')
    result.append('{')
    result.append('    return {')

    for identifier in serialized_identifiers():
        result.append('        "' + identifier + '"_s,')

    result.append('    };')
    result.append('}')

    result.append('')
    result.append('#endif // ENABLE(IPC_TESTING_API)')
    result.append('')

    result += generate_js_argument_descriptions(receivers, 'messageArgumentDescriptions', lambda message: message.parameters)

    result.append('')

    result += generate_js_argument_descriptions(receivers, 'messageReplyArgumentDescriptions', lambda message: message.reply_parameters)

    result.append('')

    result.append('} // namespace WebKit')
    result.append('')
    result.append('#endif // ENABLE(IPC_TESTING_API) || !LOG_DISABLED')
    result.append('')
    return '\n'.join(result)
