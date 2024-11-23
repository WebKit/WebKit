/*
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

#include "config.h"
#include "MessageArgumentDescriptions.h"

#if ENABLE(IPC_TESTING_API) || !LOG_DISABLED

#include "AudioMediaStreamTrackRendererInternalUnitIdentifier.h"
#include "Connection.h"
#include "ContentWorldShared.h"
#include "DataTaskIdentifier.h"
#include "DisplayLinkObserverID.h"
#include "DownloadID.h"
#include "DrawingAreaInfo.h"
#include "GPUProcessConnectionIdentifier.h"
#include "GeneratedSerializers.h"
#include "GeolocationIdentifier.h"
#include "GraphicsContextGLIdentifier.h"
#include "IPCConnectionTesterIdentifier.h"
#include "IPCStreamTesterIdentifier.h"
#include "IdentifierTypes.h"
#include "JSIPCBinding.h"
#include "LegacyCustomProtocolID.h"
#include "LibWebRTCResolverIdentifier.h"
#include "LogStreamIdentifier.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "NetworkResourceLoadIdentifier.h"
#include "PDFPluginIdentifier.h"
#include "PlaybackSessionContextIdentifier.h"
#include "QuotaIncreaseRequestIdentifier.h"
#include "RemoteAudioDestinationIdentifier.h"
#include "RemoteAudioHardwareListenerIdentifier.h"
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
#include "RemoteAudioSessionIdentifier.h"
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMIdentifier.h"
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstanceIdentifier.h"
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstanceSessionIdentifier.h"
#endif
#include "RemoteImageBufferSetIdentifier.h"
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "RemoteLegacyCDMIdentifier.h"
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "RemoteLegacyCDMSessionIdentifier.h"
#endif
#include "RemoteMediaResourceIdentifier.h"
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
#include "RemoteMediaSourceIdentifier.h"
#endif
#include "RemoteRemoteCommandListenerIdentifier.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
#include "RemoteSourceBufferIdentifier.h"
#endif
#include "RemoteVideoFrameIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "RetrieveRecordResponseBodyCallbackIdentifier.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include "ScriptMessageHandlerIdentifier.h"
#include "ShapeDetectionIdentifier.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include "StorageNamespaceIdentifier.h"
#include "TransactionID.h"
#include "UserContentControllerIdentifier.h"
#include "UserScriptIdentifier.h"
#include "UserStyleSheetIdentifier.h"
#include "VideoDecoderIdentifier.h"
#include "VideoEncoderIdentifier.h"
#include "VisitedLinkTableIdentifier.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionControllerIdentifier.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionPortChannelIdentifier.h"
#include "WebExtensionTabIdentifier.h"
#include "WebExtensionWindowIdentifier.h"
#include "WebGPUIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include "WebTransportSession.h"
#include "WebURLSchemeHandlerIdentifier.h"
#if PLATFORM(COCOA)
#include <WebCore/AttributedString.h>
#endif
#include <WebCore/BackgroundFetchRecordIdentifier.h>
#include <WebCore/BroadcastChannelIdentifier.h>
#include <WebCore/DictationContext.h>
#include <WebCore/ElementIdentifier.h>
#include <WebCore/FetchIdentifier.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <WebCore/GlobalWindowIdentifier.h>
#include <WebCore/IDBDatabaseConnectionIdentifier.h>
#include <WebCore/IDBIndexIdentifier.h>
#include <WebCore/IDBObjectStoreIdentifier.h>
#include <WebCore/ImageDecoderIdentifier.h>
#include <WebCore/InbandGenericCueIdentifier.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <WebCore/MediaKeySystemRequestIdentifier.h>
#include <WebCore/MediaPlayerClientIdentifier.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/MediaUniqueIdentifier.h>
#include <WebCore/ModelPlayerIdentifier.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/PageIdentifier.h>
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/PlaybackTargetClientContextIdentifier.h>
#endif
#include <WebCore/PortIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/PushSubscriptionIdentifier.h>
#include <WebCore/RTCDataChannelLocalIdentifier.h>
#include <WebCore/RealtimeMediaSourceIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/SharedWorkerIdentifier.h>
#include <WebCore/SleepDisablerIdentifier.h>
#include <WebCore/SpeechRecognitionConnectionClientIdentifier.h>
#include <WebCore/TextCheckingRequestIdentifier.h>
#include <WebCore/TextManipulationItemIdentifier.h>
#include <WebCore/TextManipulationToken.h>
#include <WebCore/UserMediaRequestIdentifier.h>
#include <WebCore/WebSocketIdentifier.h>
#include "TestWithCVPixelBufferMessages.h" // NOLINT
#include "TestWithDeferSendingOptionMessages.h" // NOLINT
#include "TestWithEnabledByMessages.h" // NOLINT
#include "TestWithEnabledByAndConjunctionMessages.h" // NOLINT
#include "TestWithEnabledByOrConjunctionMessages.h" // NOLINT
#include "TestWithIfMessageMessages.h" // NOLINT
#include "TestWithImageDataMessages.h" // NOLINT
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithLegacyReceiverMessages.h" // NOLINT
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithoutAttributesMessages.h" // NOLINT
#endif
#include "TestWithoutUsingIPCConnectionMessages.h" // NOLINT
#include "TestWithSemaphoreMessages.h" // NOLINT
#include "TestWithStreamMessages.h" // NOLINT
#include "TestWithStreamBatchedMessages.h" // NOLINT
#include "TestWithStreamBufferMessages.h" // NOLINT
#include "TestWithStreamServerConnectionHandleMessages.h" // NOLINT
#include "TestWithSuperclassMessages.h" // NOLINT
#include "TestWithSuperclassAndWantsAsyncDispatchMessages.h" // NOLINT
#include "TestWithSuperclassAndWantsDispatchMessages.h" // NOLINT
#include "TestWithValidatorMessages.h" // NOLINT
#include "TestWithWantsAsyncDispatchMessages.h" // NOLINT
#include "TestWithWantsDispatchMessages.h" // NOLINT
#include "TestWithWantsDispatchNoSyncMessagesMessages.h" // NOLINT

namespace IPC {

#if ENABLE(IPC_TESTING_API)

std::optional<JSC::JSValue> jsValueForArguments(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)
{
    switch (name) {
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return jsValueForDecodedMessage<MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer>(globalObject, decoder);
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return jsValueForDecodedMessage<MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer>(globalObject, decoder);
#endif
    case MessageName::TestWithDeferSendingOption_NoOptions:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_NoOptions>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_NoIndices:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_NoIndices>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_OneIndex:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_OneIndex>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_MultipleIndices:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_MultipleIndices>(globalObject, decoder);
    case MessageName::TestWithEnabledBy_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledBy_AlwaysEnabled>(globalObject, decoder);
    case MessageName::TestWithEnabledBy_ConditionallyEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledBy_ConditionallyEnabled>(globalObject, decoder);
    case MessageName::TestWithEnabledBy_ConditionallyEnabledAnd:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledBy_ConditionallyEnabledAnd>(globalObject, decoder);
    case MessageName::TestWithEnabledBy_ConditionallyEnabledOr:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledBy_ConditionallyEnabledOr>(globalObject, decoder);
    case MessageName::TestWithEnabledByAndConjunction_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledByAndConjunction_AlwaysEnabled>(globalObject, decoder);
    case MessageName::TestWithEnabledByOrConjunction_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithEnabledByOrConjunction_AlwaysEnabled>(globalObject, decoder);
#if PLATFORM(COCOA)
    case MessageName::TestWithIfMessage_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(globalObject, decoder);
#endif
#if PLATFORM(GTK)
    case MessageName::TestWithIfMessage_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(globalObject, decoder);
#endif
    case MessageName::TestWithImageData_SendImageData:
        return jsValueForDecodedMessage<MessageName::TestWithImageData_SendImageData>(globalObject, decoder);
    case MessageName::TestWithImageData_ReceiveImageData:
        return jsValueForDecodedMessage<MessageName::TestWithImageData_ReceiveImageData>(globalObject, decoder);
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadURL>(globalObject, decoder);
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomething:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomething>(globalObject, decoder);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TouchEvent>(globalObject, decoder);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_AddEvent:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_AddEvent>(globalObject, decoder);
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomethingElse>(globalObject, decoder);
#endif
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_Close:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_Close>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_PreferencesDidChange>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendDoubleAndFloat>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_SendInts:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendInts>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_CreatePlugin>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPlugins>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestParameterAttributes>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_TemplateTest:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TemplateTest>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SetVideoLayerID>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(globalObject, decoder);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DeprecatedOperation>(globalObject, decoder);
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_ExperimentalOperation>(globalObject, decoder);
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadURL>(globalObject, decoder);
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomething:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomething>(globalObject, decoder);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_TouchEvent:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TouchEvent>(globalObject, decoder);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_AddEvent:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_AddEvent>(globalObject, decoder);
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomethingElse>(globalObject, decoder);
#endif
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidReceivePolicyDecision>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_Close:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_Close>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_PreferencesDidChange>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendDoubleAndFloat>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_SendInts:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendInts>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_CreatePlugin>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_GetPlugins:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPlugins>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestMultipleAttributes>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestParameterAttributes>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_TemplateTest:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TemplateTest>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SetVideoLayerID>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidCreateWebProcessConnection>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_InterpretKeyEvent>(globalObject, decoder);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DeprecatedOperation>(globalObject, decoder);
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_ExperimentalOperation>(globalObject, decoder);
#endif
#endif
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgument:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgument>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgument:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgument>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument>(globalObject, decoder);
    case MessageName::TestWithSemaphore_SendSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_SendSemaphore>(globalObject, decoder);
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_ReceiveSemaphore>(globalObject, decoder);
    case MessageName::TestWithStream_SendString:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendString>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringAsync:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendStringAsync>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringSync:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendStringSync>(globalObject, decoder);
    case MessageName::TestWithStream_CallWithIdentifier:
        return jsValueForDecodedMessage<MessageName::TestWithStream_CallWithIdentifier>(globalObject, decoder);
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_SendMachSendRight:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendMachSendRight>(globalObject, decoder);
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return jsValueForDecodedMessage<MessageName::TestWithStream_ReceiveMachSendRight>(globalObject, decoder);
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendAndReceiveMachSendRight>(globalObject, decoder);
#endif
    case MessageName::TestWithStreamBatched_SendString:
        return jsValueForDecodedMessage<MessageName::TestWithStreamBatched_SendString>(globalObject, decoder);
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return jsValueForDecodedMessage<MessageName::TestWithStreamBuffer_SendStreamBuffer>(globalObject, decoder);
    case MessageName::TestWithStreamServerConnectionHandle_SendStreamServerConnection:
        return jsValueForDecodedMessage<MessageName::TestWithStreamServerConnectionHandle_SendStreamServerConnection>(globalObject, decoder);
    case MessageName::TestWithSuperclass_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_LoadURL>(globalObject, decoder);
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(globalObject, decoder);
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSynchronousMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclassAndWantsAsyncDispatch_LoadURL>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsDispatch_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclassAndWantsDispatch_LoadURL>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithValidator_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithValidator_AlwaysEnabled>(globalObject, decoder);
    case MessageName::TestWithValidator_EnabledIfPassValidation:
        return jsValueForDecodedMessage<MessageName::TestWithValidator_EnabledIfPassValidation>(globalObject, decoder);
    case MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation:
        return jsValueForDecodedMessage<MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation>(globalObject, decoder);
    case MessageName::TestWithWantsAsyncDispatch_TestMessage:
        return jsValueForDecodedMessage<MessageName::TestWithWantsAsyncDispatch_TestMessage>(globalObject, decoder);
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithWantsAsyncDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithWantsDispatch_TestMessage:
        return jsValueForDecodedMessage<MessageName::TestWithWantsDispatch_TestMessage>(globalObject, decoder);
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return jsValueForDecodedMessage<MessageName::TestWithWantsDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithWantsDispatchNoSyncMessages_TestMessage:
        return jsValueForDecodedMessage<MessageName::TestWithWantsDispatchNoSyncMessages_TestMessage>(globalObject, decoder);
    default:
        break;
    }
    return std::nullopt;
}

std::optional<JSC::JSValue> jsValueForReplyArguments(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)
{
    switch (name) {
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return jsValueForDecodedMessageReply<MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer>(globalObject, decoder);
#endif
    case MessageName::TestWithImageData_ReceiveImageData:
        return jsValueForDecodedMessageReply<MessageName::TestWithImageData_ReceiveImageData>(globalObject, decoder);
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_CreatePlugin>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPlugins>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(globalObject, decoder);
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_CreatePlugin>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_GetPlugins:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPlugins>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_TestMultipleAttributes>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_InterpretKeyEvent>(globalObject, decoder);
#endif
#endif
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument>(globalObject, decoder);
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return jsValueForDecodedMessageReply<MessageName::TestWithSemaphore_ReceiveSemaphore>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringAsync:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringAsync>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringSync:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringSync>(globalObject, decoder);
    case MessageName::TestWithStream_CallWithIdentifier:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_CallWithIdentifier>(globalObject, decoder);
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_ReceiveMachSendRight>(globalObject, decoder);
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_SendAndReceiveMachSendRight>(globalObject, decoder);
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(globalObject, decoder);
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSynchronousMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithWantsAsyncDispatch_TestSyncMessage>(globalObject, decoder);
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return jsValueForDecodedMessageReply<MessageName::TestWithWantsDispatch_TestSyncMessage>(globalObject, decoder);
    default:
        break;
    }
    return std::nullopt;
}

Vector<ASCIILiteral> serializedIdentifiers()
{
    static_assert(sizeof(uint64_t) == sizeof(IPC::AsyncReplyID));
#if PLATFORM(COCOA)
    static_assert(sizeof(uint64_t) == sizeof(WebCore::AttributedStringTextListID));
#endif
#if PLATFORM(COCOA)
    static_assert(sizeof(uint64_t) == sizeof(WebCore::AttributedStringTextTableBlockID));
#endif
#if PLATFORM(COCOA)
    static_assert(sizeof(uint64_t) == sizeof(WebCore::AttributedStringTextTableID));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebCore::BackForwardItemIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::BackgroundFetchRecordIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::BroadcastChannelIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::DOMCacheIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::DictationContext));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ElementIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::FetchIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::FileSystemHandleIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::FileSystemSyncAccessHandleIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::FrameIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::IDBIndexIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::IDBObjectStoreIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ImageDecoderIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::InbandGenericCueIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::LayerHostingContextIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::LibWebRTCSocketIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::MediaKeySystemRequestIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::MediaPlayerClientIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::MediaPlayerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::MediaSessionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ModelPlayerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::MediaUniqueIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::NavigationIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::OpaqueOriginIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::PageIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::PlatformLayerIdentifierID));
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    static_assert(sizeof(uint64_t) == sizeof(WebCore::PlaybackTargetClientContextIdentifier));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebCore::PortIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ProcessIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::PushSubscriptionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::RTCDataChannelLocalIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::RealtimeMediaSourceIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::RenderingResourceIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ResourceLoaderIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::SWServerConnectionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ScrollingNodeIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ServiceWorkerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ServiceWorkerJobIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::ServiceWorkerRegistrationIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::SharedWorkerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::SharedWorkerObjectIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::SleepDisablerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::SpeechRecognitionConnectionClientIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::TextCheckingRequestIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::TextManipulationItemIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::TextManipulationTokenIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::IDBDatabaseConnectionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::UserGestureTokenIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::UserMediaRequestIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::WebLockIdentifierID));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::WebSocketIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebCore::WindowIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::AudioMediaStreamTrackRendererInternalUnitIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::AuthenticationChallengeIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::ContentWorldIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::DataTaskIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::DisplayLinkObserverID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::DownloadID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::DrawingAreaIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::GeolocationIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::GPUProcessConnectionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::GraphicsContextGLIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::IPCConnectionTesterIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::IPCStreamTesterIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::LegacyCustomProtocolID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::LibWebRTCResolverIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::LogStreamIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::MarkSurfacesAsVolatileRequestIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::NetworkResourceLoadIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::PDFPluginIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::PageGroupIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::PlaybackSessionContextIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::QuotaIncreaseRequestIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteAudioDestinationIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteAudioHardwareListenerIdentifier));
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteAudioSessionIdentifier));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteImageBufferSetIdentifier));
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteCDMIdentifier));
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteCDMInstanceIdentifier));
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteCDMInstanceSessionIdentifier));
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteLegacyCDMIdentifier));
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteLegacyCDMSessionIdentifier));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteMediaResourceIdentifier));
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteMediaSourceIdentifier));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteRemoteCommandListenerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteSerializedImageBufferIdentifier));
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteSourceBufferIdentifier));
#endif
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RemoteVideoFrameIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RenderingBackendIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RenderingUpdateID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::RetrieveRecordResponseBodyCallbackIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::SampleBufferDisplayLayerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::ScriptMessageHandlerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::ShapeDetectionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::StorageAreaIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::StorageAreaImplIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::StorageAreaMapIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::StorageNamespaceIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::TapIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::TextCheckerRequestID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::TransactionID));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::UserContentControllerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::UserScriptIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::UserStyleSheetIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::VideoDecoderIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::VideoEncoderIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::VisitedLinkTableIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionContextIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionControllerIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionFrameIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionPortChannelIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionTabIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebExtensionWindowIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebGPUIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebPageProxyIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebTransportSessionIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebTransportStreamIdentifier));
    static_assert(sizeof(uint64_t) == sizeof(WebKit::WebURLSchemeHandlerIdentifier));
    return {
        "IPC::AsyncReplyID"_s,
#if PLATFORM(COCOA)
        "WebCore::AttributedStringTextListID"_s,
#endif
#if PLATFORM(COCOA)
        "WebCore::AttributedStringTextTableBlockID"_s,
#endif
#if PLATFORM(COCOA)
        "WebCore::AttributedStringTextTableID"_s,
#endif
        "WebCore::BackForwardItemIdentifierID"_s,
        "WebCore::BackgroundFetchRecordIdentifier"_s,
        "WebCore::BroadcastChannelIdentifier"_s,
        "WebCore::DOMCacheIdentifierID"_s,
        "WebCore::DictationContext"_s,
        "WebCore::ElementIdentifier"_s,
        "WebCore::FetchIdentifier"_s,
        "WebCore::FileSystemHandleIdentifier"_s,
        "WebCore::FileSystemSyncAccessHandleIdentifier"_s,
        "WebCore::FrameIdentifierID"_s,
        "WebCore::IDBIndexIdentifier"_s,
        "WebCore::IDBObjectStoreIdentifier"_s,
        "WebCore::ImageDecoderIdentifier"_s,
        "WebCore::InbandGenericCueIdentifier"_s,
        "WebCore::LayerHostingContextIdentifier"_s,
        "WebCore::LibWebRTCSocketIdentifier"_s,
        "WebCore::MediaKeySystemRequestIdentifier"_s,
        "WebCore::MediaPlayerClientIdentifier"_s,
        "WebCore::MediaPlayerIdentifier"_s,
        "WebCore::MediaSessionIdentifier"_s,
        "WebCore::ModelPlayerIdentifier"_s,
        "WebCore::MediaUniqueIdentifier"_s,
        "WebCore::NavigationIdentifier"_s,
        "WebCore::OpaqueOriginIdentifier"_s,
        "WebCore::PageIdentifier"_s,
        "WebCore::PlatformLayerIdentifierID"_s,
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        "WebCore::PlaybackTargetClientContextIdentifier"_s,
#endif
        "WebCore::PortIdentifier"_s,
        "WebCore::ProcessIdentifier"_s,
        "WebCore::PushSubscriptionIdentifier"_s,
        "WebCore::RTCDataChannelLocalIdentifier"_s,
        "WebCore::RealtimeMediaSourceIdentifier"_s,
        "WebCore::RenderingResourceIdentifier"_s,
        "WebCore::ResourceLoaderIdentifier"_s,
        "WebCore::SWServerConnectionIdentifier"_s,
        "WebCore::ScrollingNodeIdentifier"_s,
        "WebCore::ServiceWorkerIdentifier"_s,
        "WebCore::ServiceWorkerJobIdentifier"_s,
        "WebCore::ServiceWorkerRegistrationIdentifier"_s,
        "WebCore::SharedWorkerIdentifier"_s,
        "WebCore::SharedWorkerObjectIdentifierID"_s,
        "WebCore::SleepDisablerIdentifier"_s,
        "WebCore::SpeechRecognitionConnectionClientIdentifier"_s,
        "WebCore::TextCheckingRequestIdentifier"_s,
        "WebCore::TextManipulationItemIdentifier"_s,
        "WebCore::TextManipulationTokenIdentifier"_s,
        "WebCore::IDBDatabaseConnectionIdentifier"_s,
        "WebCore::UserGestureTokenIdentifierID"_s,
        "WebCore::UserMediaRequestIdentifier"_s,
        "WebCore::WebLockIdentifierID"_s,
        "WebCore::WebSocketIdentifier"_s,
        "WebCore::WindowIdentifier"_s,
        "WebKit::AudioMediaStreamTrackRendererInternalUnitIdentifier"_s,
        "WebKit::AuthenticationChallengeIdentifier"_s,
        "WebKit::ContentWorldIdentifier"_s,
        "WebKit::DataTaskIdentifier"_s,
        "WebKit::DisplayLinkObserverID"_s,
        "WebKit::DownloadID"_s,
        "WebKit::DrawingAreaIdentifier"_s,
        "WebKit::GeolocationIdentifier"_s,
        "WebKit::GPUProcessConnectionIdentifier"_s,
        "WebKit::GraphicsContextGLIdentifier"_s,
        "WebKit::IPCConnectionTesterIdentifier"_s,
        "WebKit::IPCStreamTesterIdentifier"_s,
        "WebKit::LegacyCustomProtocolID"_s,
        "WebKit::LibWebRTCResolverIdentifier"_s,
        "WebKit::LogStreamIdentifier"_s,
        "WebKit::MarkSurfacesAsVolatileRequestIdentifier"_s,
        "WebKit::NetworkResourceLoadIdentifier"_s,
        "WebKit::PDFPluginIdentifier"_s,
        "WebKit::PageGroupIdentifier"_s,
        "WebKit::PlaybackSessionContextIdentifier"_s,
        "WebKit::QuotaIncreaseRequestIdentifier"_s,
        "WebKit::RemoteAudioDestinationIdentifier"_s,
        "WebKit::RemoteAudioHardwareListenerIdentifier"_s,
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
        "WebKit::RemoteAudioSessionIdentifier"_s,
#endif
        "WebKit::RemoteImageBufferSetIdentifier"_s,
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
        "WebKit::RemoteCDMIdentifier"_s,
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
        "WebKit::RemoteCDMInstanceIdentifier"_s,
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)
        "WebKit::RemoteCDMInstanceSessionIdentifier"_s,
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
        "WebKit::RemoteLegacyCDMIdentifier"_s,
#endif
#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
        "WebKit::RemoteLegacyCDMSessionIdentifier"_s,
#endif
        "WebKit::RemoteMediaResourceIdentifier"_s,
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
        "WebKit::RemoteMediaSourceIdentifier"_s,
#endif
        "WebKit::RemoteRemoteCommandListenerIdentifier"_s,
        "WebKit::RemoteSerializedImageBufferIdentifier"_s,
#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
        "WebKit::RemoteSourceBufferIdentifier"_s,
#endif
        "WebKit::RemoteVideoFrameIdentifier"_s,
        "WebKit::RenderingBackendIdentifier"_s,
        "WebKit::RenderingUpdateID"_s,
        "WebKit::RetrieveRecordResponseBodyCallbackIdentifier"_s,
        "WebKit::SampleBufferDisplayLayerIdentifier"_s,
        "WebKit::ScriptMessageHandlerIdentifier"_s,
        "WebKit::ShapeDetectionIdentifier"_s,
        "WebKit::StorageAreaIdentifier"_s,
        "WebKit::StorageAreaImplIdentifier"_s,
        "WebKit::StorageAreaMapIdentifier"_s,
        "WebKit::StorageNamespaceIdentifier"_s,
        "WebKit::TapIdentifier"_s,
        "WebKit::TextCheckerRequestID"_s,
        "WebKit::TransactionID"_s,
        "WebKit::UserContentControllerIdentifier"_s,
        "WebKit::UserScriptIdentifier"_s,
        "WebKit::UserStyleSheetIdentifier"_s,
        "WebKit::VideoDecoderIdentifier"_s,
        "WebKit::VideoEncoderIdentifier"_s,
        "WebKit::VisitedLinkTableIdentifier"_s,
        "WebKit::WebExtensionContextIdentifier"_s,
        "WebKit::WebExtensionControllerIdentifier"_s,
        "WebKit::WebExtensionFrameIdentifier"_s,
        "WebKit::WebExtensionPortChannelIdentifier"_s,
        "WebKit::WebExtensionTabIdentifier"_s,
        "WebKit::WebExtensionWindowIdentifier"_s,
        "WebKit::WebGPUIdentifier"_s,
        "WebKit::WebPageProxyIdentifier"_s,
        "WebKit::WebTransportSessionIdentifier"_s,
        "WebKit::WebTransportStreamIdentifier"_s,
        "WebKit::WebURLSchemeHandlerIdentifier"_s,
    };
}

#endif // ENABLE(IPC_TESTING_API)

std::optional<Vector<ArgumentDescription>> messageArgumentDescriptions(MessageName name)
{
    switch (name) {
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return Vector<ArgumentDescription> {
            { "s0"_s, "RetainPtr<CVPixelBufferRef>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return Vector<ArgumentDescription> { };
#endif
    case MessageName::TestWithDeferSendingOption_NoOptions:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithDeferSendingOption_NoIndices:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithDeferSendingOption_OneIndex:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithDeferSendingOption_MultipleIndices:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
            { "foo"_s, "int"_s, ASCIILiteral(), false },
            { "bar"_s, "int"_s, ASCIILiteral(), false },
            { "baz"_s, "int"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithEnabledBy_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithEnabledBy_ConditionallyEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithEnabledBy_ConditionallyEnabledAnd:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithEnabledBy_ConditionallyEnabledOr:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithEnabledByAndConjunction_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithEnabledByOrConjunction_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#if PLATFORM(COCOA)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#endif
#if PLATFORM(GTK)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
            { "value"_s, "int64_t"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithImageData_SendImageData:
        return Vector<ArgumentDescription> {
            { "s0"_s, "RefPtr<WebCore::ImageData>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> { };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s, ASCIILiteral(), false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_AddEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "listenerID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "policyAction"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store"_s, "WebKit::WebPreferencesStore"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d"_s, "double"_s, ASCIILiteral(), false },
            { "f"_s, "float"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_SendInts:
        return Vector<ArgumentDescription> {
            { "ints"_s, "Vector<uint64_t>"_s, ASCIILiteral(), false },
            { "intVectors"_s, "Vector<Vector<uint64_t>>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "parameters"_s, "WebKit::Plugin::Parameters"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "message"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh"_s, "bool"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "bar"_s, "double"_s, ASCIILiteral(), false },
            { "baz"_s, "double"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a"_s, "HashMap<String, std::pair<String, uint64_t>>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID"_s, "WebCore::PlatformLayerIdentifier"_s, ASCIILiteral(), false },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier"_s, "MachSendRight"_s, ASCIILiteral(), false },
            { "flags"_s, "OptionSet<WebKit::SelectionFlags>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s, ASCIILiteral(), false },
        };
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s, ASCIILiteral(), false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_AddEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "listenerID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "policyAction"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store"_s, "WebKit::WebPreferencesStore"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d"_s, "double"_s, ASCIILiteral(), false },
            { "f"_s, "float"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_SendInts:
        return Vector<ArgumentDescription> {
            { "ints"_s, "Vector<uint64_t>"_s, ASCIILiteral(), false },
            { "intVectors"_s, "Vector<Vector<uint64_t>>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "parameters"_s, "WebKit::Plugin::Parameters"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "message"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh"_s, "bool"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo"_s, "uint64_t"_s, ASCIILiteral(), false },
            { "bar"_s, "double"_s, ASCIILiteral(), false },
            { "baz"_s, "double"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a"_s, "HashMap<String, std::pair<String, uint64_t>>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID"_s, "WebCore::PlatformLayerIdentifier"_s, ASCIILiteral(), false },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier"_s, "MachSendRight"_s, ASCIILiteral(), false },
            { "flags"_s, "OptionSet<WebKit::SelectionFlags>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s, ASCIILiteral(), false },
        };
#endif
#endif
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgument:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgument:
        return Vector<ArgumentDescription> {
            { "argument"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return Vector<ArgumentDescription> {
            { "argument"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "argument"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSemaphore_SendSemaphore:
        return Vector<ArgumentDescription> {
            { "s0"_s, "IPC::Semaphore"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStream_SendString:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_SendStringAsync:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_SendStringSync:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_CallWithIdentifier:
        return Vector<ArgumentDescription> { };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_SendMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1"_s, "MachSendRight"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1"_s, "MachSendRight"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithStreamBatched_SendString:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return Vector<ArgumentDescription> {
            { "stream"_s, "IPC::StreamConnectionBuffer"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStreamServerConnectionHandle_SendStreamServerConnection:
        return Vector<ArgumentDescription> {
            { "handle"_s, "IPC::StreamServerConnectionHandle"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclass_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "twoStateEnum"_s, "bool"_s, "WebKit::TestTwoStateEnum"_s, false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "value"_s, "int"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "value"_s, "bool"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithValidator_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithValidator_EnabledIfPassValidation:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsDispatch_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsDispatchNoSyncMessages_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s, ASCIILiteral(), false },
        };
    default:
        break;
    }
    return std::nullopt;
}

std::optional<Vector<ArgumentDescription>> messageReplyArgumentDescriptions(MessageName name)
{
    switch (name) {
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return Vector<ArgumentDescription> {
            { "r0"_s, "RetainPtr<CVPixelBufferRef>"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> {
            { "r0"_s, "RefPtr<WebCore::ImageData>"_s, ASCIILiteral(), false },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle"_s, "IPC::Connection::Handle"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s, ASCIILiteral(), false },
        };
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle"_s, "IPC::Connection::Handle"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s, ASCIILiteral(), false },
        };
#endif
#endif
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> {
            { "r0"_s, "IPC::Semaphore"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_SendStringAsync:
        return Vector<ArgumentDescription> {
            { "returnValue"_s, "int64_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_SendStringSync:
        return Vector<ArgumentDescription> {
            { "returnValue"_s, "int64_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_CallWithIdentifier:
        return Vector<ArgumentDescription> { };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1"_s, "MachSendRight"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1"_s, "MachSendRight"_s, ASCIILiteral(), false },
        };
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "result"_s, "uint64_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s, ASCIILiteral(), false },
            { "value"_s, "uint64_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s, ASCIILiteral(), false },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "optionalReply"_s, "WebKit::TestClassName"_s, ASCIILiteral(), true },
        };
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s, ASCIILiteral(), false },
        };
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s, ASCIILiteral(), false },
        };
    default:
        break;
    }
    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API) || !LOG_DISABLED
