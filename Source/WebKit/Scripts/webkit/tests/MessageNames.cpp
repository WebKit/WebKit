/*
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

#include "config.h"
#include "MessageNames.h"

namespace IPC::Detail {

const MessageDescription messageDescriptions[static_cast<size_t>(MessageName::Count) + 1] = {
#if USE(AVFOUNDATION)
    { "TestWithCVPixelBuffer_ReceiveCVPixelBuffer", ReceiverName::TestWithCVPixelBuffer, false, false },
    { "TestWithCVPixelBuffer_SendCVPixelBuffer", ReceiverName::TestWithCVPixelBuffer, false, false },
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    { "TestWithIfMessage_LoadURL", ReceiverName::TestWithIfMessage, false, false },
#endif
    { "TestWithImageData_ReceiveImageData", ReceiverName::TestWithImageData, false, false },
    { "TestWithImageData_SendImageData", ReceiverName::TestWithImageData, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithLegacyReceiver_AddEvent", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_Close", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_CreatePlugin", ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    { "TestWithLegacyReceiver_DeprecatedOperation", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_DidCreateWebProcessConnection", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_DidReceivePolicyDecision", ReceiverName::TestWithLegacyReceiver, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    { "TestWithLegacyReceiver_ExperimentalOperation", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_GetPlugins", ReceiverName::TestWithLegacyReceiver, false, false },
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_InterpretKeyEvent", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    { "TestWithLegacyReceiver_LoadSomething", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_LoadSomethingElse", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithLegacyReceiver_LoadURL", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_PreferencesDidChange", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_RunJavaScriptAlert", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SendDoubleAndFloat", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SendInts", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_SetVideoLayerID", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_TemplateTest", ReceiverName::TestWithLegacyReceiver, false, false },
    { "TestWithLegacyReceiver_TestParameterAttributes", ReceiverName::TestWithLegacyReceiver, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithLegacyReceiver_TouchEvent", ReceiverName::TestWithLegacyReceiver, false, false },
#endif
    { "TestWithSemaphore_ReceiveSemaphore", ReceiverName::TestWithSemaphore, false, false },
    { "TestWithSemaphore_SendSemaphore", ReceiverName::TestWithSemaphore, false, false },
    { "TestWithStreamBatched_SendString", ReceiverName::TestWithStreamBatched, true, false },
    { "TestWithStreamBuffer_SendStreamBuffer", ReceiverName::TestWithStreamBuffer, false, false },
#if PLATFORM(COCOA)
    { "TestWithStream_SendMachSendRight", ReceiverName::TestWithStream, true, false },
#endif
    { "TestWithStream_SendString", ReceiverName::TestWithStream, true, false },
    { "TestWithStream_SendStringAsync", ReceiverName::TestWithStream, true, false },
    { "TestWithSuperclass_LoadURL", ReceiverName::TestWithSuperclass, false, false },
#if ENABLE(TEST_FEATURE)
    { "TestWithSuperclass_TestAsyncMessage", ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithConnection", ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithMultipleArguments", ReceiverName::TestWithSuperclass, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithNoArguments", ReceiverName::TestWithSuperclass, false, false },
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithoutAttributes_AddEvent", ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_Close", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_CreatePlugin", ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(DEPRECATED_FEATURE)
    { "TestWithoutAttributes_DeprecatedOperation", ReceiverName::TestWithoutAttributes, false, false },
#endif
#if PLATFORM(MAC)
    { "TestWithoutAttributes_DidCreateWebProcessConnection", ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_DidReceivePolicyDecision", ReceiverName::TestWithoutAttributes, false, false },
#if ENABLE(FEATURE_FOR_TESTING)
    { "TestWithoutAttributes_ExperimentalOperation", ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_GetPlugins", ReceiverName::TestWithoutAttributes, false, false },
#if PLATFORM(MAC)
    { "TestWithoutAttributes_InterpretKeyEvent", ReceiverName::TestWithoutAttributes, false, false },
#endif
#if ENABLE(TOUCH_EVENTS)
    { "TestWithoutAttributes_LoadSomething", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_LoadSomethingElse", ReceiverName::TestWithoutAttributes, false, false },
#endif
    { "TestWithoutAttributes_LoadURL", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_PreferencesDidChange", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_RunJavaScriptAlert", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SendDoubleAndFloat", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SendInts", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_SetVideoLayerID", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_TemplateTest", ReceiverName::TestWithoutAttributes, false, false },
    { "TestWithoutAttributes_TestParameterAttributes", ReceiverName::TestWithoutAttributes, false, false },
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    { "TestWithoutAttributes_TouchEvent", ReceiverName::TestWithoutAttributes, false, false },
#endif
#if PLATFORM(COCOA)
    { "InitializeConnection", ReceiverName::IPC, false, false },
#endif
    { "LegacySessionState", ReceiverName::IPC, false, false },
    { "ProcessOutOfStreamMessage", ReceiverName::IPC, false, false },
    { "SetStreamDestinationID", ReceiverName::IPC, false, false },
    { "SyncMessageReply", ReceiverName::IPC, false, false },
    { "Terminate", ReceiverName::IPC, false, false },
#if USE(AVFOUNDATION)
    { "TestWithCVPixelBuffer_ReceiveCVPixelBufferReply", ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithImageData_ReceiveImageDataReply", ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_CreatePluginReply", ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_GetPluginsReply", ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    { "TestWithLegacyReceiver_InterpretKeyEventReply", ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithLegacyReceiver_RunJavaScriptAlertReply", ReceiverName::AsyncReply, false, false },
    { "TestWithSemaphore_ReceiveSemaphoreReply", ReceiverName::AsyncReply, false, false },
    { "TestWithStream_SendStringAsyncReply", ReceiverName::AsyncReply, false, false },
#if ENABLE(TEST_FEATURE)
    { "TestWithSuperclass_TestAsyncMessageReply", ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithConnectionReply", ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply", ReceiverName::AsyncReply, false, false },
    { "TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply", ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithoutAttributes_CreatePluginReply", ReceiverName::AsyncReply, false, false },
    { "TestWithoutAttributes_GetPluginsReply", ReceiverName::AsyncReply, false, false },
#if PLATFORM(MAC)
    { "TestWithoutAttributes_InterpretKeyEventReply", ReceiverName::AsyncReply, false, false },
#endif
    { "TestWithoutAttributes_RunJavaScriptAlertReply", ReceiverName::AsyncReply, false, false },
    { "TestWithLegacyReceiver_GetPluginProcessConnection", ReceiverName::TestWithLegacyReceiver, true, false },
    { "TestWithLegacyReceiver_TestMultipleAttributes", ReceiverName::TestWithLegacyReceiver, true, false },
#if PLATFORM(COCOA)
    { "TestWithStream_ReceiveMachSendRight", ReceiverName::TestWithStream, true, false },
    { "TestWithStream_SendAndReceiveMachSendRight", ReceiverName::TestWithStream, true, false },
#endif
    { "TestWithStream_SendStringSync", ReceiverName::TestWithStream, true, false },
    { "TestWithSuperclass_TestSyncMessage", ReceiverName::TestWithSuperclass, true, false },
    { "TestWithSuperclass_TestSynchronousMessage", ReceiverName::TestWithSuperclass, true, false },
    { "TestWithoutAttributes_GetPluginProcessConnection", ReceiverName::TestWithoutAttributes, true, false },
    { "TestWithoutAttributes_TestMultipleAttributes", ReceiverName::TestWithoutAttributes, true, false },
    { "WrappedAsyncMessageForTesting", ReceiverName::IPC, true, false },
    { "<invalid message name>", ReceiverName::Invalid, false, false }
};

} // namespace IPC::Detail
