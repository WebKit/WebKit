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
    MessageDescription { "TestWithCVPixelBuffer_ReceiveCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false },
    MessageDescription { "TestWithCVPixelBuffer_SendCVPixelBuffer"_s, ReceiverName::TestWithCVPixelBuffer, false, false },
#endif
    MessageDescription { "TestWithEnabledByAndConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByAndConjunction, false, false },
    MessageDescription { "TestWithEnabledByOrConjunction_AlwaysEnabled"_s, ReceiverName::TestWithEnabledByOrConjunction, false, false },
    MessageDescription { "TestWithEnabledBy_AlwaysEnabled"_s, ReceiverName::TestWithEnabledBy, false, false },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabled"_s, ReceiverName::TestWithEnabledBy, false, false },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabledAnd"_s, ReceiverName::TestWithEnabledBy, false, false },
    MessageDescription { "TestWithEnabledBy_ConditionallyEnabledOr"_s, ReceiverName::TestWithEnabledBy, false, false },
    MessageDescription { "TestWithEnabledIf_AlwaysEnabled"_s, ReceiverName::TestWithEnabledIf, false, false },
    MessageDescription { "TestWithEnabledIf_OnlyEnabledIfFeatureEnabled"_s, ReceiverName::TestWithEnabledIf, false, false },
#if PLATFORM(COCOA) || PLATFORM(GTK)
    MessageDescription { "TestWithIfMessage_LoadURL"_s, ReceiverName::TestWithIfMessage, false, false },
#endif
    MessageDescription { "TestWithImageData_ReceiveImageData"_s, ReceiverName::TestWithImageData, false, false },
    MessageDescription { "TestWithImageData_SendImageData"_s, ReceiverName::TestWithImageData, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithLegacyReceiver_AddEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    MessageDescription { "TestWithLegacyReceiver_Close"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_CreatePlugin"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    MessageDescription { "TestWithLegacyReceiver_DeprecatedOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if PLATFORM(MAC)
    MessageDescription { "TestWithLegacyReceiver_DidCreateWebProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    MessageDescription { "TestWithLegacyReceiver_DidReceivePolicyDecision"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    MessageDescription { "TestWithLegacyReceiver_ExperimentalOperation"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    MessageDescription { "TestWithLegacyReceiver_GetPlugins"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if PLATFORM(MAC)
    MessageDescription { "TestWithLegacyReceiver_InterpretKeyEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    MessageDescription { "TestWithLegacyReceiver_LoadSomething"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_LoadSomethingElse"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    MessageDescription { "TestWithLegacyReceiver_LoadURL"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_PreferencesDidChange"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_RunJavaScriptAlert"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_SendDoubleAndFloat"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_SendInts"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_SetVideoLayerID"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_TemplateTest"_s, ReceiverName::TestWithLegacyReceiver, false, false },
    MessageDescription { "TestWithLegacyReceiver_TestParameterAttributes"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithLegacyReceiver_TouchEvent"_s, ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    MessageDescription { "TestWithSemaphore_ReceiveSemaphore"_s, ReceiverName::TestWithSemaphore, false, false },
    MessageDescription { "TestWithSemaphore_SendSemaphore"_s, ReceiverName::TestWithSemaphore, false, false },
    MessageDescription { "TestWithStreamBatched_SendString"_s, ReceiverName::TestWithStreamBatched, true, false },
    MessageDescription { "TestWithStreamBuffer_SendStreamBuffer"_s, ReceiverName::TestWithStreamBuffer, false, false },
    MessageDescription { "TestWithStreamServerConnectionHandle_SendStreamServerConnection"_s, ReceiverName::TestWithStreamServerConnectionHandle, false, false },
    MessageDescription { "TestWithStream_CallWithIdentifier"_s, ReceiverName::TestWithStream, true, false },
#if PLATFORM(COCOA)
    MessageDescription { "TestWithStream_SendMachSendRight"_s, ReceiverName::TestWithStream, true, false },
#endif
    MessageDescription { "TestWithStream_SendString"_s, ReceiverName::TestWithStream, true, false },
    MessageDescription { "TestWithStream_SendStringAsync"_s, ReceiverName::TestWithStream, true, false },
    MessageDescription { "TestWithSuperclassAndWantsAsyncDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, false, false },
    MessageDescription { "TestWithSuperclassAndWantsDispatch_LoadURL"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, false, false },
    MessageDescription { "TestWithSuperclass_LoadURL"_s, ReceiverName::TestWithSuperclass, false, false },
#if ENABLE(TEST_FEATURE)
    MessageDescription { "TestWithSuperclass_TestAsyncMessage"_s, ReceiverName::TestWithSuperclass, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithConnection"_s, ReceiverName::TestWithSuperclass, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithMultipleArguments"_s, ReceiverName::TestWithSuperclass, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithNoArguments"_s, ReceiverName::TestWithSuperclass, false, false },
#endif
    MessageDescription { "TestWithWantsAsyncDispatch_TestMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, false, false },
    MessageDescription { "TestWithWantsDispatchNoSyncMessages_TestMessage"_s, ReceiverName::TestWithWantsDispatchNoSyncMessages, false, false },
    MessageDescription { "TestWithWantsDispatch_TestMessage"_s, ReceiverName::TestWithWantsDispatch, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithoutAttributes_AddEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_Close"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_CreatePlugin"_s, ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    MessageDescription { "TestWithoutAttributes_DeprecatedOperation"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
#if PLATFORM(MAC)
    MessageDescription { "TestWithoutAttributes_DidCreateWebProcessConnection"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_DidReceivePolicyDecision"_s, ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    MessageDescription { "TestWithoutAttributes_ExperimentalOperation"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_GetPlugins"_s, ReceiverName::TestWithoutAttributes, false, false },
#if PLATFORM(MAC)
    MessageDescription { "TestWithoutAttributes_InterpretKeyEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    MessageDescription { "TestWithoutAttributes_LoadSomething"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_LoadSomethingElse"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_LoadURL"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_PreferencesDidChange"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_RunJavaScriptAlert"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_SendDoubleAndFloat"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_SendInts"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_SetVideoLayerID"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_TemplateTest"_s, ReceiverName::TestWithoutAttributes, false, false },
    MessageDescription { "TestWithoutAttributes_TestParameterAttributes"_s, ReceiverName::TestWithoutAttributes, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    MessageDescription { "TestWithoutAttributes_TouchEvent"_s, ReceiverName::TestWithoutAttributes, false, false },
#endif
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument"_s, ReceiverName::TestWithoutUsingIPCConnection, false, false },
    MessageDescription { "CancelSyncMessageReply"_s, ReceiverName::IPC, false, false },
#if PLATFORM(COCOA)
    MessageDescription { "InitializeConnection"_s, ReceiverName::IPC, false, false },
#endif
    MessageDescription { "LegacySessionState"_s, ReceiverName::IPC, false, false },
    MessageDescription { "ProcessOutOfStreamMessage"_s, ReceiverName::IPC, false, false },
    MessageDescription { "SetStreamDestinationID"_s, ReceiverName::IPC, false, false },
    MessageDescription { "SyncMessageReply"_s, ReceiverName::IPC, false, false },
#if USE(AVFOUNDATION)
    MessageDescription { "TestWithCVPixelBuffer_ReceiveCVPixelBufferReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    MessageDescription { "TestWithImageData_ReceiveImageDataReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithLegacyReceiver_CreatePluginReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithLegacyReceiver_GetPluginsReply"_s, ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    MessageDescription { "TestWithLegacyReceiver_InterpretKeyEventReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    MessageDescription { "TestWithLegacyReceiver_RunJavaScriptAlertReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithSemaphore_ReceiveSemaphoreReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithStream_CallWithIdentifierReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithStream_SendStringAsyncReply"_s, ReceiverName::AsyncReply, false, false },
#if ENABLE(TEST_FEATURE)
    MessageDescription { "TestWithSuperclass_TestAsyncMessageReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithConnectionReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_CreatePluginReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithoutAttributes_GetPluginsReply"_s, ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    MessageDescription { "TestWithoutAttributes_InterpretKeyEventReply"_s, ReceiverName::AsyncReply, false, false },
#endif
    MessageDescription { "TestWithoutAttributes_RunJavaScriptAlertReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply"_s, ReceiverName::AsyncReply, false, false },
    MessageDescription { "TestWithLegacyReceiver_GetPluginProcessConnection"_s, ReceiverName::TestWithLegacyReceiver, true, false },
    MessageDescription { "TestWithLegacyReceiver_TestMultipleAttributes"_s, ReceiverName::TestWithLegacyReceiver, true, false },
#if PLATFORM(COCOA)
    MessageDescription { "TestWithStream_ReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false },
    MessageDescription { "TestWithStream_SendAndReceiveMachSendRight"_s, ReceiverName::TestWithStream, true, false },
#endif
    MessageDescription { "TestWithStream_SendStringSync"_s, ReceiverName::TestWithStream, true, false },
    MessageDescription { "TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsAsyncDispatch, true, false },
    MessageDescription { "TestWithSuperclassAndWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithSuperclassAndWantsDispatch, true, false },
    MessageDescription { "TestWithSuperclass_TestSyncMessage"_s, ReceiverName::TestWithSuperclass, true, false },
    MessageDescription { "TestWithSuperclass_TestSynchronousMessage"_s, ReceiverName::TestWithSuperclass, true, false },
    MessageDescription { "TestWithWantsAsyncDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsAsyncDispatch, true, false },
    MessageDescription { "TestWithWantsDispatch_TestSyncMessage"_s, ReceiverName::TestWithWantsDispatch, true, false },
    MessageDescription { "TestWithoutAttributes_GetPluginProcessConnection"_s, ReceiverName::TestWithoutAttributes, true, false },
    MessageDescription { "TestWithoutAttributes_TestMultipleAttributes"_s, ReceiverName::TestWithoutAttributes, true, false },
    MessageDescription { "WrappedAsyncMessageForTesting"_s, ReceiverName::IPC, true, false },
    MessageDescription { "<invalid message name>"_s, ReceiverName::Invalid, false, false }
};

} // namespace IPC::Detail
