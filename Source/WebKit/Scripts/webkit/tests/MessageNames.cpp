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
#include "MessageNames.h"

namespace IPC::Detail {

const MessageDescriptionsArray messageDescriptions {
#if USE(AVFOUNDATION)
    MessageDescription { "TestWithCVPixelBuffer_ReceiveCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithCVPixelBuffer_ReceiveCVPixelBufferReply"_s, ReceiverName::TestWithCVPixelBuffer, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithCVPixelBuffer_SendCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithDeferSendingOption_MultipleIndices"_s, ReceiverName::TestWithDeferSendingOption, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithDeferSendingOption_NoIndices"_s, ReceiverName::TestWithDeferSendingOption, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithDeferSendingOption_NoOptions"_s, ReceiverName::TestWithDeferSendingOption, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithDeferSendingOption_OneIndex"_s, ReceiverName::TestWithDeferSendingOption, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithDispatchedFromAndTo_AlwaysEnabled"_s, ReceiverName::TestWithDispatchedFromAndTo, false, false, false, ProcessName::WebContent, ProcessName::UI },
    MessageDescription { "TestWithEnabledByAndConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByAndConjunction, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithEnabledByOrConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByOrConjunction, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithEnabledBy_AlwaysEnabled"_s, ReceiverName::TestWithEnabledBy, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabled"_s, ReceiverName::TestWithEnabledBy, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabledAnd"_s, ReceiverName::TestWithEnabledBy, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabledOr"_s, ReceiverName::TestWithEnabledBy, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(COCOA) || PLATFORM(GTK)
    MessageDescription { "TestWithIfMessage_LoadURL"_s, ReceiverName::TestWithIfMessage, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithImageData_ReceiveImageData"_s, ReceiverName::TestWithImageData, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithImageData_ReceiveImageDataReply"_s, ReceiverName::TestWithImageData, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithImageData_SendImageData"_s, ReceiverName::TestWithImageData, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithLegacyReceiver_AddEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithLegacyReceiver_Close"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_CreatePlugin"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_CreatePluginReply"_s, ReceiverName::TestWithLegacyReceiver, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#if ENABLE(DEPRECATED_FEATURE)
    MessageDescription { "TestWithLegacyReceiver_DeprecatedOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
#if PLATFORM(MAC)
    MessageDescription { "TestWithLegacyReceiver_DidCreateWebProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithLegacyReceiver_DidReceivePolicyDecision"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if ENABLE(FEATURE_FOR_TESTING)
    MessageDescription { "TestWithLegacyReceiver_ExperimentalOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithLegacyReceiver_GetPlugins"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_GetPluginsReply"_s, ReceiverName::TestWithLegacyReceiver, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(MAC)
    MessageDescription { "TestWithLegacyReceiver_InterpretKeyEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_InterpretKeyEventReply"_s, ReceiverName::TestWithLegacyReceiver, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#endif
#if ENABLE(TOUCH_EVENTS)
    MessageDescription { "TestWithLegacyReceiver_LoadSomething"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_LoadSomethingElse"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithLegacyReceiver_LoadURL"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_OpaqueTypeSecurityAssertion"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply"_s, ReceiverName::TestWithLegacyReceiver, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_PreferencesDidChange"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_RunJavaScriptAlert"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_RunJavaScriptAlertReply"_s, ReceiverName::TestWithLegacyReceiver, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_SendDoubleAndFloat"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_SendInts"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_SetVideoLayerID"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_TemplateTest"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_TestParameterAttributes"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithLegacyReceiver_TouchEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithMultiLineExtendedAttributes_AlwaysEnabled"_s, ReceiverName::TestWithMultiLineExtendedAttributes, false, false, false, ProcessName::GPU, ProcessName::WebContent },
    MessageDescription { "TestWithSemaphore_ReceiveSemaphore"_s, ReceiverName::TestWithSemaphore, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSemaphore_ReceiveSemaphoreReply"_s, ReceiverName::TestWithSemaphore, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSemaphore_SendSemaphore"_s, ReceiverName::TestWithSemaphore, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSpanOfConst_TestSpanOfConstFloat"_s, ReceiverName::TestWithSpanOfConst, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSpanOfConst_TestSpanOfConstFloatSegments"_s, ReceiverName::TestWithSpanOfConst, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStreamBatched_SendString"_s, ReceiverName::TestWithStreamBatched, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStreamBuffer_SendStreamBuffer"_s, ReceiverName::TestWithStreamBuffer, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStreamServerConnectionHandle_SendStreamServerConnection"_s, ReceiverName::TestWithStreamServerConnectionHandle, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStream_CallWithIdentifier"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStream_CallWithIdentifierReply"_s, ReceiverName::TestWithStream, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(COCOA)
    MessageDescription { "TestWithStream_SendMachSendRight"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithStream_SendString"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStream_SendStringAsync"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStream_SendStringAsyncReply"_s, ReceiverName::TestWithStream, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclassAndWantsAsyncDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclassAndWantsDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_LoadURL"_s, ReceiverName::TestWithSuperclass, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if ENABLE(TEST_FEATURE)
    MessageDescription { "TestWithSuperclass_TestAsyncMessage"_s, ReceiverName::TestWithSuperclass, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageReply"_s, ReceiverName::TestWithSuperclass, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithConnection"_s, ReceiverName::TestWithSuperclass, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithConnectionReply"_s, ReceiverName::TestWithSuperclass, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithMultipleArguments"_s, ReceiverName::TestWithSuperclass, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply"_s, ReceiverName::TestWithSuperclass, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithNoArguments"_s, ReceiverName::TestWithSuperclass, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply"_s, ReceiverName::TestWithSuperclass, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithValidator_AlwaysEnabled"_s, ReceiverName::TestWithValidator, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithValidator_EnabledIfPassValidation"_s, ReceiverName::TestWithValidator, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation"_s, ReceiverName::TestWithValidator, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithValidator_MessageWithReply"_s, ReceiverName::TestWithValidator, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithValidator_MessageWithReplyReply"_s, ReceiverName::TestWithValidator, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithWantsAsyncDispatch_TestMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithWantsDispatchNoSyncMessages_TestMessage"_s, ReceiverName::TestWithWantsDispatchNoSyncMessages, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithWantsDispatch_TestMessage"_s, ReceiverName::TestWithWantsDispatch, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithoutAttributes_AddEvent"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithoutAttributes_Close"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_CreatePlugin"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_CreatePluginReply"_s, ReceiverName::TestWithoutAttributes, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#if ENABLE(DEPRECATED_FEATURE)
    MessageDescription { "TestWithoutAttributes_DeprecatedOperation"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
#if PLATFORM(MAC)
    MessageDescription { "TestWithoutAttributes_DidCreateWebProcessConnection"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithoutAttributes_DidReceivePolicyDecision"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if ENABLE(FEATURE_FOR_TESTING)
    MessageDescription { "TestWithoutAttributes_ExperimentalOperation"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithoutAttributes_GetPlugins"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_GetPluginsReply"_s, ReceiverName::TestWithoutAttributes, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(MAC)
    MessageDescription { "TestWithoutAttributes_InterpretKeyEvent"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_InterpretKeyEventReply"_s, ReceiverName::TestWithoutAttributes, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
#endif
#if ENABLE(TOUCH_EVENTS)
    MessageDescription { "TestWithoutAttributes_LoadSomething"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_LoadSomethingElse"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithoutAttributes_LoadURL"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_OpaqueTypeSecurityAssertion"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_OpaqueTypeSecurityAssertionReply"_s, ReceiverName::TestWithoutAttributes, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_PreferencesDidChange"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_RunJavaScriptAlert"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_RunJavaScriptAlertReply"_s, ReceiverName::TestWithoutAttributes, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_SendDoubleAndFloat"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_SendInts"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_SetVideoLayerID"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_TemplateTest"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_TestParameterAttributes"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithoutAttributes_TouchEvent"_s, ReceiverName::TestWithoutAttributes, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false, true, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "CancelSyncMessageReply"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(COCOA)
    MessageDescription { "InitializeConnection"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "LegacySessionState"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "ProcessOutOfStreamMessage"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "SetStreamDestinationID"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "SyncMessageReply"_s, ReceiverName::IPC, false, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_GetPluginProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithLegacyReceiver_TestMultipleAttributes"_s, ReceiverName::TestWithLegacyReceiver, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
#if PLATFORM(COCOA)
    MessageDescription { "TestWithStream_ReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithStream_SendAndReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
#endif
    MessageDescription { "TestWithStream_SendStringSync"_s, ReceiverName::TestWithStream, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclassAndWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestSyncMessage"_s, ReceiverName::TestWithSuperclass, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithSuperclass_TestSynchronousMessage"_s, ReceiverName::TestWithSuperclass, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsDispatch, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_GetPluginProcessConnection"_s, ReceiverName::TestWithoutAttributes, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "TestWithoutAttributes_TestMultipleAttributes"_s, ReceiverName::TestWithoutAttributes, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "WrappedAsyncMessageForTesting"_s, ReceiverName::IPC, true, false, false, ProcessName::Unknown, ProcessName::Unknown },
    MessageDescription { "<invalid message name>"_s, ReceiverName::Invalid, false, false, false, ProcessName::Unknown, ProcessName::Unknown }
};

} // namespace IPC::Detail

namespace IPC {

ASCIILiteral processLiteral(ProcessName name)
{
    switch (name) {
    case ProcessName::UI:
        return "UI";
    case ProcessName::Networking:
        return "Networking";
    case ProcessName::GPU:
        return "GPU";
    case ProcessName::WebContent:
        return "WebContent";
    case ProcessName::Model:
        return "Model";
    case ProcessName::Unknown:
        return "Unknown";
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
};

ASCIILiteral dispatchedFrom(MessageName name)
{
    return processLiteral(Detail::messageDescriptions[static_cast<size_t>(name)].dispatchedFrom);
};

ASCIILiteral dispatchedTo(MessageName name)
{
    return processLiteral(Detail::messageDescriptions[static_cast<size_t>(name)].dispatchedTo);
};

} // namespace IPC
