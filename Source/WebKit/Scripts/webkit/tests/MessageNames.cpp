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

namespace IPC {

const char* description(MessageName name)
{
    switch (name) {
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return "TestWithCVPixelBuffer_ReceiveCVPixelBuffer";
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return "TestWithCVPixelBuffer_SendCVPixelBuffer";
    case MessageName::TestWithIfMessage_LoadURL:
        return "TestWithIfMessage_LoadURL";
    case MessageName::TestWithImageData_ReceiveImageData:
        return "TestWithImageData_ReceiveImageData";
    case MessageName::TestWithImageData_SendImageData:
        return "TestWithImageData_SendImageData";
    case MessageName::TestWithLegacyReceiver_AddEvent:
        return "TestWithLegacyReceiver_AddEvent";
    case MessageName::TestWithLegacyReceiver_Close:
        return "TestWithLegacyReceiver_Close";
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return "TestWithLegacyReceiver_CreatePlugin";
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
        return "TestWithLegacyReceiver_DeprecatedOperation";
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
        return "TestWithLegacyReceiver_DidCreateWebProcessConnection";
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
        return "TestWithLegacyReceiver_DidReceivePolicyDecision";
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
        return "TestWithLegacyReceiver_ExperimentalOperation";
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return "TestWithLegacyReceiver_GetPlugins";
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return "TestWithLegacyReceiver_InterpretKeyEvent";
    case MessageName::TestWithLegacyReceiver_LoadSomething:
        return "TestWithLegacyReceiver_LoadSomething";
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
        return "TestWithLegacyReceiver_LoadSomethingElse";
    case MessageName::TestWithLegacyReceiver_LoadURL:
        return "TestWithLegacyReceiver_LoadURL";
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
        return "TestWithLegacyReceiver_PreferencesDidChange";
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return "TestWithLegacyReceiver_RunJavaScriptAlert";
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
        return "TestWithLegacyReceiver_SendDoubleAndFloat";
    case MessageName::TestWithLegacyReceiver_SendInts:
        return "TestWithLegacyReceiver_SendInts";
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
        return "TestWithLegacyReceiver_SetVideoLayerID";
    case MessageName::TestWithLegacyReceiver_TemplateTest:
        return "TestWithLegacyReceiver_TemplateTest";
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
        return "TestWithLegacyReceiver_TestParameterAttributes";
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return "TestWithLegacyReceiver_TouchEvent";
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return "TestWithSemaphore_ReceiveSemaphore";
    case MessageName::TestWithSemaphore_SendSemaphore:
        return "TestWithSemaphore_SendSemaphore";
    case MessageName::TestWithStreamBatched_SendString:
        return "TestWithStreamBatched_SendString";
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return "TestWithStreamBuffer_SendStreamBuffer";
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return "TestWithStream_ReceiveMachSendRight";
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return "TestWithStream_SendAndReceiveMachSendRight";
    case MessageName::TestWithStream_SendMachSendRight:
        return "TestWithStream_SendMachSendRight";
    case MessageName::TestWithStream_SendString:
        return "TestWithStream_SendString";
    case MessageName::TestWithStream_SendStringSynchronized:
        return "TestWithStream_SendStringSynchronized";
    case MessageName::TestWithSuperclass_LoadURL:
        return "TestWithSuperclass_LoadURL";
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return "TestWithSuperclass_TestAsyncMessage";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return "TestWithSuperclass_TestAsyncMessageWithConnection";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return "TestWithSuperclass_TestAsyncMessageWithMultipleArguments";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return "TestWithSuperclass_TestAsyncMessageWithNoArguments";
    case MessageName::TestWithoutAttributes_AddEvent:
        return "TestWithoutAttributes_AddEvent";
    case MessageName::TestWithoutAttributes_Close:
        return "TestWithoutAttributes_Close";
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return "TestWithoutAttributes_CreatePlugin";
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
        return "TestWithoutAttributes_DeprecatedOperation";
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
        return "TestWithoutAttributes_DidCreateWebProcessConnection";
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
        return "TestWithoutAttributes_DidReceivePolicyDecision";
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
        return "TestWithoutAttributes_ExperimentalOperation";
    case MessageName::TestWithoutAttributes_GetPlugins:
        return "TestWithoutAttributes_GetPlugins";
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return "TestWithoutAttributes_InterpretKeyEvent";
    case MessageName::TestWithoutAttributes_LoadSomething:
        return "TestWithoutAttributes_LoadSomething";
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
        return "TestWithoutAttributes_LoadSomethingElse";
    case MessageName::TestWithoutAttributes_LoadURL:
        return "TestWithoutAttributes_LoadURL";
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
        return "TestWithoutAttributes_PreferencesDidChange";
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return "TestWithoutAttributes_RunJavaScriptAlert";
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
        return "TestWithoutAttributes_SendDoubleAndFloat";
    case MessageName::TestWithoutAttributes_SendInts:
        return "TestWithoutAttributes_SendInts";
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
        return "TestWithoutAttributes_SetVideoLayerID";
    case MessageName::TestWithoutAttributes_TemplateTest:
        return "TestWithoutAttributes_TemplateTest";
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
        return "TestWithoutAttributes_TestParameterAttributes";
    case MessageName::TestWithoutAttributes_TouchEvent:
        return "TestWithoutAttributes_TouchEvent";
    case MessageName::InitializeConnection:
        return "InitializeConnection";
    case MessageName::LegacySessionState:
        return "LegacySessionState";
    case MessageName::ProcessOutOfStreamMessage:
        return "ProcessOutOfStreamMessage";
    case MessageName::SetStreamDestinationID:
        return "SetStreamDestinationID";
    case MessageName::SyncMessageReply:
        return "SyncMessageReply";
    case MessageName::Terminate:
        return "Terminate";
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply:
        return "TestWithCVPixelBuffer_ReceiveCVPixelBufferReply";
    case MessageName::TestWithImageData_ReceiveImageDataReply:
        return "TestWithImageData_ReceiveImageDataReply";
    case MessageName::TestWithLegacyReceiver_CreatePluginReply:
        return "TestWithLegacyReceiver_CreatePluginReply";
    case MessageName::TestWithLegacyReceiver_GetPluginsReply:
        return "TestWithLegacyReceiver_GetPluginsReply";
    case MessageName::TestWithLegacyReceiver_InterpretKeyEventReply:
        return "TestWithLegacyReceiver_InterpretKeyEventReply";
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply:
        return "TestWithLegacyReceiver_RunJavaScriptAlertReply";
    case MessageName::TestWithSemaphore_ReceiveSemaphoreReply:
        return "TestWithSemaphore_ReceiveSemaphoreReply";
    case MessageName::TestWithStream_ReceiveMachSendRightReply:
        return "TestWithStream_ReceiveMachSendRightReply";
    case MessageName::TestWithStream_SendAndReceiveMachSendRightReply:
        return "TestWithStream_SendAndReceiveMachSendRightReply";
    case MessageName::TestWithStream_SendStringSynchronizedReply:
        return "TestWithStream_SendStringSynchronizedReply";
    case MessageName::TestWithSuperclass_TestAsyncMessageReply:
        return "TestWithSuperclass_TestAsyncMessageReply";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply:
        return "TestWithSuperclass_TestAsyncMessageWithConnectionReply";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply:
        return "TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply";
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply:
        return "TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply";
    case MessageName::TestWithoutAttributes_CreatePluginReply:
        return "TestWithoutAttributes_CreatePluginReply";
    case MessageName::TestWithoutAttributes_GetPluginsReply:
        return "TestWithoutAttributes_GetPluginsReply";
    case MessageName::TestWithoutAttributes_InterpretKeyEventReply:
        return "TestWithoutAttributes_InterpretKeyEventReply";
    case MessageName::TestWithoutAttributes_RunJavaScriptAlertReply:
        return "TestWithoutAttributes_RunJavaScriptAlertReply";
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return "TestWithLegacyReceiver_GetPluginProcessConnection";
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return "TestWithLegacyReceiver_TestMultipleAttributes";
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return "TestWithSuperclass_TestSyncMessage";
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return "TestWithSuperclass_TestSynchronousMessage";
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return "TestWithoutAttributes_GetPluginProcessConnection";
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return "TestWithoutAttributes_TestMultipleAttributes";
    case MessageName::WrappedAsyncMessageForTesting:
        return "WrappedAsyncMessageForTesting";
    }
    ASSERT_NOT_REACHED();
    return "<invalid message name>";
}

ReceiverName receiverName(MessageName messageName)
{
    switch (messageName) {
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return ReceiverName::TestWithCVPixelBuffer;
    case MessageName::TestWithIfMessage_LoadURL:
        return ReceiverName::TestWithIfMessage;
    case MessageName::TestWithImageData_ReceiveImageData:
    case MessageName::TestWithImageData_SendImageData:
        return ReceiverName::TestWithImageData;
    case MessageName::TestWithLegacyReceiver_AddEvent:
    case MessageName::TestWithLegacyReceiver_Close:
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
    case MessageName::TestWithLegacyReceiver_GetPlugins:
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
    case MessageName::TestWithLegacyReceiver_LoadSomething:
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
    case MessageName::TestWithLegacyReceiver_LoadURL:
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
    case MessageName::TestWithLegacyReceiver_SendInts:
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
    case MessageName::TestWithLegacyReceiver_TemplateTest:
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return ReceiverName::TestWithLegacyReceiver;
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
    case MessageName::TestWithSemaphore_SendSemaphore:
        return ReceiverName::TestWithSemaphore;
    case MessageName::TestWithStreamBatched_SendString:
        return ReceiverName::TestWithStreamBatched;
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return ReceiverName::TestWithStreamBuffer;
    case MessageName::TestWithStream_ReceiveMachSendRight:
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
    case MessageName::TestWithStream_SendMachSendRight:
    case MessageName::TestWithStream_SendString:
    case MessageName::TestWithStream_SendStringSynchronized:
        return ReceiverName::TestWithStream;
    case MessageName::TestWithSuperclass_LoadURL:
    case MessageName::TestWithSuperclass_TestAsyncMessage:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return ReceiverName::TestWithSuperclass;
    case MessageName::TestWithoutAttributes_AddEvent:
    case MessageName::TestWithoutAttributes_Close:
    case MessageName::TestWithoutAttributes_CreatePlugin:
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
    case MessageName::TestWithoutAttributes_GetPlugins:
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
    case MessageName::TestWithoutAttributes_LoadSomething:
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
    case MessageName::TestWithoutAttributes_LoadURL:
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
    case MessageName::TestWithoutAttributes_SendInts:
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
    case MessageName::TestWithoutAttributes_TemplateTest:
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
    case MessageName::TestWithoutAttributes_TouchEvent:
        return ReceiverName::TestWithoutAttributes;
    case MessageName::InitializeConnection:
    case MessageName::LegacySessionState:
    case MessageName::ProcessOutOfStreamMessage:
    case MessageName::SetStreamDestinationID:
    case MessageName::SyncMessageReply:
    case MessageName::Terminate:
        return ReceiverName::IPC;
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply:
    case MessageName::TestWithImageData_ReceiveImageDataReply:
    case MessageName::TestWithLegacyReceiver_CreatePluginReply:
    case MessageName::TestWithLegacyReceiver_GetPluginsReply:
    case MessageName::TestWithLegacyReceiver_InterpretKeyEventReply:
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply:
    case MessageName::TestWithSemaphore_ReceiveSemaphoreReply:
    case MessageName::TestWithStream_ReceiveMachSendRightReply:
    case MessageName::TestWithStream_SendAndReceiveMachSendRightReply:
    case MessageName::TestWithStream_SendStringSynchronizedReply:
    case MessageName::TestWithSuperclass_TestAsyncMessageReply:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply:
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply:
    case MessageName::TestWithoutAttributes_CreatePluginReply:
    case MessageName::TestWithoutAttributes_GetPluginsReply:
    case MessageName::TestWithoutAttributes_InterpretKeyEventReply:
    case MessageName::TestWithoutAttributes_RunJavaScriptAlertReply:
        return ReceiverName::AsyncReply;
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return ReceiverName::TestWithLegacyReceiver;
    case MessageName::TestWithSuperclass_TestSyncMessage:
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return ReceiverName::TestWithSuperclass;
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return ReceiverName::TestWithoutAttributes;
    case MessageName::WrappedAsyncMessageForTesting:
        return ReceiverName::IPC;
    }
    ASSERT_NOT_REACHED();
    return ReceiverName::Invalid;
}
bool messageAllowedWhenWaitingForSyncReply(MessageName name)
{
    switch (name) {
    case MessageName::TestWithStreamBatched_SendString:
    case MessageName::TestWithStream_ReceiveMachSendRight:
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
    case MessageName::TestWithStream_SendMachSendRight:
    case MessageName::TestWithStream_SendString:
    case MessageName::TestWithStream_SendStringSynchronized:
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
    case MessageName::TestWithSuperclass_TestSyncMessage:
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
    case MessageName::WrappedAsyncMessageForTesting:
        return true;
    default:
        return false;
    }
}

bool messageAllowedWhenWaitingForUnboundedSyncReply(MessageName name)
{
    switch (name) {
    default:
        return false;
    }
}


} // namespace IPC

namespace WTF {
template<> bool isValidEnum<IPC::MessageName, void>(std::underlying_type_t<IPC::MessageName> underlyingType)
{
    auto messageName = static_cast<IPC::MessageName>(underlyingType);
#if USE(AVFOUNDATION)
    if (messageName == IPC::MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer)
        return true;
#endif
#if USE(AVFOUNDATION)
    if (messageName == IPC::MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer)
        return true;
#endif
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithIfMessage_LoadURL)
        return true;
#endif
#if PLATFORM(GTK)
    if (messageName == IPC::MessageName::TestWithIfMessage_LoadURL)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithImageData_ReceiveImageData)
        return true;
    if (messageName == IPC::MessageName::TestWithImageData_SendImageData)
        return true;
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_AddEvent)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_Close)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_CreatePlugin)
        return true;
#if ENABLE(DEPRECATED_FEATURE)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_DeprecatedOperation)
        return true;
#endif
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision)
        return true;
#if ENABLE(FEATURE_FOR_TESTING)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_ExperimentalOperation)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_GetPlugins)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEvent)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_LoadSomething)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_LoadSomethingElse)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_LoadURL)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_PreferencesDidChange)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlert)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_SendDoubleAndFloat)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_SendInts)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_SetVideoLayerID)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_TemplateTest)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_TestParameterAttributes)
        return true;
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_TouchEvent)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithSemaphore_ReceiveSemaphore)
        return true;
    if (messageName == IPC::MessageName::TestWithSemaphore_SendSemaphore)
        return true;
    if (messageName == IPC::MessageName::TestWithStreamBatched_SendString)
        return true;
    if (messageName == IPC::MessageName::TestWithStreamBuffer_SendStreamBuffer)
        return true;
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithStream_ReceiveMachSendRight)
        return true;
#endif
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithStream_SendAndReceiveMachSendRight)
        return true;
#endif
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithStream_SendMachSendRight)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithStream_SendString)
        return true;
    if (messageName == IPC::MessageName::TestWithStream_SendStringSynchronized)
        return true;
    if (messageName == IPC::MessageName::TestWithSuperclass_LoadURL)
        return true;
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessage)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnection)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments)
        return true;
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::TestWithoutAttributes_AddEvent)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_Close)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_CreatePlugin)
        return true;
#if ENABLE(DEPRECATED_FEATURE)
    if (messageName == IPC::MessageName::TestWithoutAttributes_DeprecatedOperation)
        return true;
#endif
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithoutAttributes_DidCreateWebProcessConnection)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_DidReceivePolicyDecision)
        return true;
#if ENABLE(FEATURE_FOR_TESTING)
    if (messageName == IPC::MessageName::TestWithoutAttributes_ExperimentalOperation)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_GetPlugins)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithoutAttributes_InterpretKeyEvent)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::TestWithoutAttributes_LoadSomething)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::TestWithoutAttributes_LoadSomethingElse)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_LoadURL)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_PreferencesDidChange)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_RunJavaScriptAlert)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_SendDoubleAndFloat)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_SendInts)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_SetVideoLayerID)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_TemplateTest)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_TestParameterAttributes)
        return true;
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::TestWithoutAttributes_TouchEvent)
        return true;
#endif
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::InitializeConnection)
        return true;
#endif
    if (messageName == IPC::MessageName::LegacySessionState)
        return true;
    if (messageName == IPC::MessageName::ProcessOutOfStreamMessage)
        return true;
    if (messageName == IPC::MessageName::SetStreamDestinationID)
        return true;
    if (messageName == IPC::MessageName::SyncMessageReply)
        return true;
    if (messageName == IPC::MessageName::Terminate)
        return true;
#if USE(AVFOUNDATION)
    if (messageName == IPC::MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithImageData_ReceiveImageDataReply)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_CreatePluginReply)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_GetPluginsReply)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEventReply)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply)
        return true;
    if (messageName == IPC::MessageName::TestWithSemaphore_ReceiveSemaphoreReply)
        return true;
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithStream_ReceiveMachSendRightReply)
        return true;
#endif
#if PLATFORM(COCOA)
    if (messageName == IPC::MessageName::TestWithStream_SendAndReceiveMachSendRightReply)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithStream_SendStringSynchronizedReply)
        return true;
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_CreatePluginReply)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_GetPluginsReply)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::TestWithoutAttributes_InterpretKeyEventReply)
        return true;
#endif
    if (messageName == IPC::MessageName::TestWithoutAttributes_RunJavaScriptAlertReply)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_GetPluginProcessConnection)
        return true;
    if (messageName == IPC::MessageName::TestWithLegacyReceiver_TestMultipleAttributes)
        return true;
    if (messageName == IPC::MessageName::TestWithSuperclass_TestSyncMessage)
        return true;
    if (messageName == IPC::MessageName::TestWithSuperclass_TestSynchronousMessage)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_GetPluginProcessConnection)
        return true;
    if (messageName == IPC::MessageName::TestWithoutAttributes_TestMultipleAttributes)
        return true;
    if (messageName == IPC::MessageName::WrappedAsyncMessageForTesting)
        return true;
    return false;
};

} // namespace WTF
