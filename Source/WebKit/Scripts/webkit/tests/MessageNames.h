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

#pragma once

#include <algorithm>
#include <array>
#include <type_traits>
#include <wtf/EnumTraits.h>
#include <wtf/text/ASCIILiteral.h>

namespace IPC {

enum class ProcessName : uint8_t {
    UI,
    Networking,
    GPU,
    WebContent,
    Model,
    Unknown
};

enum class ReceiverName : uint8_t {
    TestWithCVPixelBuffer = 1
    , TestWithDeferSendingOption = 2
    , TestWithDispatchedFromAndTo = 3
    , TestWithEnabledBy = 4
    , TestWithEnabledByAndConjunction = 5
    , TestWithEnabledByOrConjunction = 6
    , TestWithIfMessage = 7
    , TestWithImageData = 8
    , TestWithLegacyReceiver = 9
    , TestWithMultiLineExtendedAttributes = 10
    , TestWithSemaphore = 11
    , TestWithSpanOfConst = 12
    , TestWithStream = 13
    , TestWithStreamBatched = 14
    , TestWithStreamBuffer = 15
    , TestWithStreamServerConnectionHandle = 16
    , TestWithSuperclass = 17
    , TestWithSuperclassAndWantsAsyncDispatch = 18
    , TestWithSuperclassAndWantsDispatch = 19
    , TestWithValidator = 20
    , TestWithWantsAsyncDispatch = 21
    , TestWithWantsDispatch = 22
    , TestWithWantsDispatchNoSyncMessages = 23
    , TestWithoutAttributes = 24
    , TestWithoutUsingIPCConnection = 25
    , IPC = 26
    , Invalid = 27
};

enum class MessageName : uint16_t {
#if USE(AVFOUNDATION)
    TestWithCVPixelBuffer_ReceiveCVPixelBuffer,
    TestWithCVPixelBuffer_ReceiveCVPixelBufferReply,
    TestWithCVPixelBuffer_SendCVPixelBuffer,
#endif
    TestWithDeferSendingOption_MultipleIndices,
    TestWithDeferSendingOption_NoIndices,
    TestWithDeferSendingOption_NoOptions,
    TestWithDeferSendingOption_OneIndex,
    TestWithDispatchedFromAndTo_AlwaysEnabled,
    TestWithEnabledByAndConjunction_AlwaysEnabled,
    TestWithEnabledByOrConjunction_AlwaysEnabled,
    TestWithEnabledBy_AlwaysEnabled,
    TestWithEnabledBy_ConditionallyEnabled,
    TestWithEnabledBy_ConditionallyEnabledAnd,
    TestWithEnabledBy_ConditionallyEnabledOr,
#if PLATFORM(COCOA) || PLATFORM(GTK)
    TestWithIfMessage_LoadURL,
#endif
    TestWithImageData_ReceiveImageData,
    TestWithImageData_ReceiveImageDataReply,
    TestWithImageData_SendImageData,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    TestWithLegacyReceiver_AddEvent,
#endif
    TestWithLegacyReceiver_Close,
    TestWithLegacyReceiver_CreatePlugin,
    TestWithLegacyReceiver_CreatePluginReply,
#if ENABLE(DEPRECATED_FEATURE)
    TestWithLegacyReceiver_DeprecatedOperation,
#endif
#if PLATFORM(MAC)
    TestWithLegacyReceiver_DidCreateWebProcessConnection,
#endif
    TestWithLegacyReceiver_DidReceivePolicyDecision,
#if ENABLE(FEATURE_FOR_TESTING)
    TestWithLegacyReceiver_ExperimentalOperation,
#endif
    TestWithLegacyReceiver_GetPlugins,
    TestWithLegacyReceiver_GetPluginsReply,
#if PLATFORM(MAC)
    TestWithLegacyReceiver_InterpretKeyEvent,
    TestWithLegacyReceiver_InterpretKeyEventReply,
#endif
#if ENABLE(TOUCH_EVENTS)
    TestWithLegacyReceiver_LoadSomething,
    TestWithLegacyReceiver_LoadSomethingElse,
#endif
    TestWithLegacyReceiver_LoadURL,
    TestWithLegacyReceiver_OpaqueTypeSecurityAssertion,
    TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply,
    TestWithLegacyReceiver_PreferencesDidChange,
    TestWithLegacyReceiver_RunJavaScriptAlert,
    TestWithLegacyReceiver_RunJavaScriptAlertReply,
    TestWithLegacyReceiver_SendDoubleAndFloat,
    TestWithLegacyReceiver_SendInts,
    TestWithLegacyReceiver_SetVideoLayerID,
    TestWithLegacyReceiver_TemplateTest,
    TestWithLegacyReceiver_TestParameterAttributes,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    TestWithLegacyReceiver_TouchEvent,
#endif
    TestWithMultiLineExtendedAttributes_AlwaysEnabled,
    TestWithSemaphore_ReceiveSemaphore,
    TestWithSemaphore_ReceiveSemaphoreReply,
    TestWithSemaphore_SendSemaphore,
    TestWithSpanOfConst_TestSpanOfConstFloat,
    TestWithSpanOfConst_TestSpanOfConstFloatSegments,
    TestWithStreamBatched_SendString,
    TestWithStreamBuffer_SendStreamBuffer,
    TestWithStreamServerConnectionHandle_SendStreamServerConnection,
    TestWithStream_CallWithIdentifier,
    TestWithStream_CallWithIdentifierReply,
#if PLATFORM(COCOA)
    TestWithStream_SendMachSendRight,
#endif
    TestWithStream_SendString,
    TestWithStream_SendStringAsync,
    TestWithStream_SendStringAsyncReply,
    TestWithSuperclassAndWantsAsyncDispatch_LoadURL,
    TestWithSuperclassAndWantsDispatch_LoadURL,
    TestWithSuperclass_LoadURL,
#if ENABLE(TEST_FEATURE)
    TestWithSuperclass_TestAsyncMessage,
    TestWithSuperclass_TestAsyncMessageReply,
    TestWithSuperclass_TestAsyncMessageWithConnection,
    TestWithSuperclass_TestAsyncMessageWithConnectionReply,
    TestWithSuperclass_TestAsyncMessageWithMultipleArguments,
    TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply,
    TestWithSuperclass_TestAsyncMessageWithNoArguments,
    TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply,
#endif
    TestWithValidator_AlwaysEnabled,
    TestWithValidator_EnabledIfPassValidation,
    TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation,
    TestWithValidator_MessageWithReply,
    TestWithValidator_MessageWithReplyReply,
    TestWithWantsAsyncDispatch_TestMessage,
    TestWithWantsDispatchNoSyncMessages_TestMessage,
    TestWithWantsDispatch_TestMessage,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    TestWithoutAttributes_AddEvent,
#endif
    TestWithoutAttributes_Close,
    TestWithoutAttributes_CreatePlugin,
    TestWithoutAttributes_CreatePluginReply,
#if ENABLE(DEPRECATED_FEATURE)
    TestWithoutAttributes_DeprecatedOperation,
#endif
#if PLATFORM(MAC)
    TestWithoutAttributes_DidCreateWebProcessConnection,
#endif
    TestWithoutAttributes_DidReceivePolicyDecision,
#if ENABLE(FEATURE_FOR_TESTING)
    TestWithoutAttributes_ExperimentalOperation,
#endif
    TestWithoutAttributes_GetPlugins,
    TestWithoutAttributes_GetPluginsReply,
#if PLATFORM(MAC)
    TestWithoutAttributes_InterpretKeyEvent,
    TestWithoutAttributes_InterpretKeyEventReply,
#endif
#if ENABLE(TOUCH_EVENTS)
    TestWithoutAttributes_LoadSomething,
    TestWithoutAttributes_LoadSomethingElse,
#endif
    TestWithoutAttributes_LoadURL,
    TestWithoutAttributes_OpaqueTypeSecurityAssertion,
    TestWithoutAttributes_OpaqueTypeSecurityAssertionReply,
    TestWithoutAttributes_PreferencesDidChange,
    TestWithoutAttributes_RunJavaScriptAlert,
    TestWithoutAttributes_RunJavaScriptAlertReply,
    TestWithoutAttributes_SendDoubleAndFloat,
    TestWithoutAttributes_SendInts,
    TestWithoutAttributes_SetVideoLayerID,
    TestWithoutAttributes_TemplateTest,
    TestWithoutAttributes_TestParameterAttributes,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    TestWithoutAttributes_TouchEvent,
#endif
    TestWithoutUsingIPCConnection_MessageWithArgument,
    TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply,
    TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply,
    TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument,
    TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply,
    TestWithoutUsingIPCConnection_MessageWithoutArgument,
    TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply,
    TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply,
    TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument,
    TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply,
    CancelSyncMessageReply,
#if PLATFORM(COCOA)
    InitializeConnection,
#endif
    LegacySessionState,
    ProcessOutOfStreamMessage,
    SetStreamDestinationID,
    SyncMessageReply,
    FirstSynchronous,
    LastAsynchronous = FirstSynchronous - 1,
    TestWithLegacyReceiver_GetPluginProcessConnection,
    TestWithLegacyReceiver_TestMultipleAttributes,
#if PLATFORM(COCOA)
    TestWithStream_ReceiveMachSendRight,
    TestWithStream_SendAndReceiveMachSendRight,
#endif
    TestWithStream_SendStringSync,
    TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage,
    TestWithSuperclassAndWantsDispatch_TestSyncMessage,
    TestWithSuperclass_TestSyncMessage,
    TestWithSuperclass_TestSynchronousMessage,
    TestWithWantsAsyncDispatch_TestSyncMessage,
    TestWithWantsDispatch_TestSyncMessage,
    TestWithoutAttributes_GetPluginProcessConnection,
    TestWithoutAttributes_TestMultipleAttributes,
    WrappedAsyncMessageForTesting,
    Count,
    Invalid = Count,
    Last = Count - 1
};

namespace Detail {
struct MessageDescription {
    ASCIILiteral description;
    ReceiverName receiverName;
    bool messageAllowedWhenWaitingForSyncReply : 1;
    bool messageAllowedWhenWaitingForUnboundedSyncReply : 1;
    bool isAsyncReply : 1;
    ProcessName dispatchedFrom;
    ProcessName dispatchedTo;
};

using MessageDescriptionsArray = std::array<MessageDescription, static_cast<size_t>(MessageName::Count) + 1>;
extern const MessageDescriptionsArray messageDescriptions;

}

inline ReceiverName receiverName(MessageName messageName)
{
    messageName = std::min(messageName, MessageName::Last);
    return Detail::messageDescriptions[static_cast<size_t>(messageName)].receiverName;
}

inline ASCIILiteral description(MessageName messageName)
{
    messageName = std::min(messageName, MessageName::Last);
    return Detail::messageDescriptions[static_cast<size_t>(messageName)].description;
}

inline bool messageAllowedWhenWaitingForSyncReply(MessageName messageName)
{
    messageName = std::min(messageName, MessageName::Last);
    return Detail::messageDescriptions[static_cast<size_t>(messageName)].messageAllowedWhenWaitingForSyncReply;
}

inline bool messageAllowedWhenWaitingForUnboundedSyncReply(MessageName messageName)
{
    messageName = std::min(messageName, MessageName::Last);
    return Detail::messageDescriptions[static_cast<size_t>(messageName)].messageAllowedWhenWaitingForUnboundedSyncReply;
}

inline bool isAsyncReply(MessageName messageName)
{
    messageName = std::min(messageName, MessageName::Last);
    return Detail::messageDescriptions[static_cast<size_t>(messageName)].isAsyncReply;
}

constexpr bool messageIsSync(MessageName name)
{
    return name >= MessageName::FirstSynchronous;
}

ASCIILiteral processLiteral(ProcessName);
ASCIILiteral dispatchedFrom(MessageName);
ASCIILiteral dispatchedTo(MessageName);

} // namespace IPC

namespace WTF {

template<> constexpr bool isValidEnum<IPC::MessageName>(std::underlying_type_t<IPC::MessageName> messageName)
{
    return messageName <= WTF::enumToUnderlyingType(IPC::MessageName::Last);
}

} // namespace WTF
