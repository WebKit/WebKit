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

const MessageDescription messageDescriptions[static_cast<size_t>(MessageName::Count) + 1] = {
#if USE(AVFOUNDATION)
    { "TestWithCVPixelBuffer_ReceiveCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false },
    { "TestWithCVPixelBuffer_SendCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false },
#endif
    { "TestWithEnabledByAndConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByAndConjunction, false, false },
    { "TestWithEnabledByOrConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByOrConjunction, false, false },
    { "TestWithEnabledBy_AlwaysEnabled"_s, ReceiverName::TestWithEnabledBy, false, false },
    { "TestWithEnabledBy_ConditionallyEnabled"_s, ReceiverName::TestWithEnabledBy, false, false },
    { "TestWithEnabledBy_ConditionallyEnabledAnd"_s, ReceiverName::TestWithEnabledBy, false, false },
    { "TestWithEnabledBy_ConditionallyEnabledOr"_s, ReceiverName::TestWithEnabledBy, false, false },
    { "TestWithEnabledIf_AlwaysEnabled"_s, ReceiverName::TestWithEnabledIf, false, false },
    { "TestWithEnabledIf_OnlyEnabledIfFeatureEnabled"_s, ReceiverName::TestWithEnabledIf, false, false },
#if PLATFORM(COCOA) || PLATFORM(GTK)
    { "TestWithIfMessage_LoadURL"_s, ReceiverName::TestWithIfMessage, false, false },
#endif
    { "TestWithImageData_ReceiveImageData"_s, ReceiverName::TestWithImageData, false, false },
    { "TestWithImageData_SendImageData"_s, ReceiverName::TestWithImageData, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithLegacyReceiver_AddEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_Close"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_CreatePlugin"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    { "TestWithLegacyReceiver_DeprecatedOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_DidCreateWebProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_DidReceivePolicyDecision"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    { "TestWithLegacyReceiver_ExperimentalOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_GetPlugins"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_InterpretKeyEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    { "TestWithLegacyReceiver_LoadSomething"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_LoadSomethingElse"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_LoadURL"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_PreferencesDidChange"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_RunJavaScriptAlert"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SendDoubleAndFloat"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SendInts"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SetVideoLayerID"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_TemplateTest"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_TestParameterAttributes"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithLegacyReceiver_TouchEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithSemaphore_ReceiveSemaphore"_s, ReceiverName::TestWithSemaphore, false, false },
    { "TestWithSemaphore_SendSemaphore"_s, ReceiverName::TestWithSemaphore, false, false },
    { "TestWithStreamBatched_SendString"_s, ReceiverName::TestWithStreamBatched, true, false },
    { "TestWithStreamBuffer_SendStreamBuffer"_s, ReceiverName::TestWithStreamBuffer, false, false },
    { "TestWithStreamServerConnectionHandle_SendStreamServerConnection"_s, ReceiverName::TestWithStreamServerConnectionHandle, false, false },
    { "TestWithStream_CallWithIdentifier"_s, ReceiverName::TestWithStream, true, false },
#if PLATFORM(COCOA)
    { "TestWithStream_SendMachSendRight"_s, ReceiverName::TestWithStream, true, false },
#endif
    { "TestWithStream_SendString"_s, ReceiverName::TestWithStream, true, false },
    { "TestWithStream_SendStringAsync"_s, ReceiverName::TestWithStream, true, false },
    { "TestWithSuperclassAndWantsAsyncDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, false, false },
    { "TestWithSuperclassAndWantsDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, false, false },
    { "TestWithSuperclass_LoadURL"_s, ReceiverName::TestWithSuperclass, false, false },
#if ENABLE(TEST_FEATURE)
    { "TestWithSuperclass_TestAsyncMessage"_s, ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithConnection"_s, ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithMultipleArguments"_s, ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithNoArguments"_s, ReceiverName::TestWithSuperclass, false, false },
#endif
    { "TestWithWantsAsyncDispatch_TestMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, false, false },
    { "TestWithWantsDispatchNoSyncMessages_TestMessage"_s, ReceiverName::TestWithWantsDispatchNoSyncMessages, false, false },
    { "TestWithWantsDispatch_TestMessage"_s, ReceiverName::TestWithWantsDispatch, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithoutAttributes_AddEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_Close"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_CreatePlugin"_s, ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    { "TestWithoutAttributes_DeprecatedOperation"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
#if PLATFORM(MAC)
    { "TestWithoutAttributes_DidCreateWebProcessConnection"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_DidReceivePolicyDecision"_s, ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    { "TestWithoutAttributes_ExperimentalOperation"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_GetPlugins"_s, ReceiverName::TestWithoutAttributes, false, false },
#if PLATFORM(MAC)
    { "TestWithoutAttributes_InterpretKeyEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    { "TestWithoutAttributes_LoadSomething"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_LoadSomethingElse"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_LoadURL"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_PreferencesDidChange"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_RunJavaScriptAlert"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SendDoubleAndFloat"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SendInts"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SetVideoLayerID"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_TemplateTest"_s, ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_TestParameterAttributes"_s, ReceiverName::TestWithoutAttributes, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithoutAttributes_TouchEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutUsingIPCConnection_MessageWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithoutArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    { "CancelSyncMessageReply"_s, ReceiverName::IPC, false, false },
#if PLATFORM(COCOA)
    { "InitializeConnection"_s, ReceiverName::IPC, false, false },
#endif
    { "LegacySessionState"_s, ReceiverName::IPC, false, false },
    { "ProcessOutOfStreamMessage"_s, ReceiverName::IPC, false, false },
    { "SetStreamDestinationID"_s, ReceiverName::IPC, false, false },
    { "SyncMessageReply"_s, ReceiverName::IPC, false, false },
#if USE(AVFOUNDATION)
    { "TestWithCVPixelBuffer_ReceiveCVPixelBufferReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithImageData_ReceiveImageDataReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_CreatePluginReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_GetPluginsReply"_s, ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_InterpretKeyEventReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithLegacyReceiver_RunJavaScriptAlertReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithSemaphore_ReceiveSemaphoreReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithStream_CallWithIdentifierReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithStream_SendStringAsyncReply"_s, ReceiverName::AsyncReply, false, false },
#if ENABLE(TEST_FEATURE)
    { "TestWithSuperclass_TestAsyncMessageReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithConnectionReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithoutAttributes_CreatePluginReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithoutAttributes_GetPluginsReply"_s, ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    { "TestWithoutAttributes_InterpretKeyEventReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithoutAttributes_RunJavaScriptAlertReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply"_s, ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_GetPluginProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, true, false },
    { "TestWithLegacyReceiver_TestMultipleAttributes"_s, ReceiverName::TestWithLegacyReceiver, true, false },
#if PLATFORM(COCOA)
    { "TestWithStream_ReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false },
    { "TestWithStream_SendAndReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false },
#endif
    { "TestWithStream_SendStringSync"_s, ReceiverName::TestWithStream, true, false },
    { "TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, true, false },
    { "TestWithSuperclassAndWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, true, false },
    { "TestWithSuperclass_TestSyncMessage"_s, ReceiverName::TestWithSuperclass, true, false },
    { "TestWithSuperclass_TestSynchronousMessage"_s, ReceiverName::TestWithSuperclass, true, false },
    { "TestWithWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, true, false },
    { "TestWithWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsDispatch, true, false },
    { "TestWithoutAttributes_GetPluginProcessConnection"_s, ReceiverName::TestWithoutAttributes, true, false },
    { "TestWithoutAttributes_TestMultipleAttributes"_s, ReceiverName::TestWithoutAttributes, true, false },
    { "WrappedAsyncMessageForTesting"_s, ReceiverName::IPC, true, false },
    { "<invalid message name>"_s, ReceiverName::Invalid, false, false }
};

} // namespace IPC::Detail
