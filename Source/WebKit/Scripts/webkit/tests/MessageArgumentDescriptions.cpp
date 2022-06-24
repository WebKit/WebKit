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
#include "MessageArgumentDescriptions.h"

#if ENABLE(IPC_TESTING_API) || !LOG_DISABLED

#include "JSIPCBinding.h"
#include "TestWithSuperclassMessages.h" // NOLINT
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithLegacyReceiverMessages.h" // NOLINT
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithoutAttributesMessages.h" // NOLINT
#endif
#include "TestWithIfMessageMessages.h" // NOLINT
#include "TestWithSemaphoreMessages.h" // NOLINT
#include "TestWithImageDataMessages.h" // NOLINT
#include "TestWithStreamMessages.h" // NOLINT
#include "TestWithStreamBatchedMessages.h" // NOLINT
#include "TestWithStreamBufferMessages.h" // NOLINT
#include "TestWithCVPixelBufferMessages.h" // NOLINT

namespace IPC {

#if ENABLE(IPC_TESTING_API)

std::optional<JSC::JSValue> jsValueForArguments(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)
{
    switch (name) {
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
#if PLATFORM(COCOA)
    case MessageName::TestWithIfMessage_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(globalObject, decoder);
#endif
#if PLATFORM(GTK)
    case MessageName::TestWithIfMessage_LoadURL:
        return jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(globalObject, decoder);
#endif
    case MessageName::TestWithSemaphore_SendSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_SendSemaphore>(globalObject, decoder);
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_ReceiveSemaphore>(globalObject, decoder);
    case MessageName::TestWithImageData_SendImageData:
        return jsValueForDecodedMessage<MessageName::TestWithImageData_SendImageData>(globalObject, decoder);
    case MessageName::TestWithImageData_ReceiveImageData:
        return jsValueForDecodedMessage<MessageName::TestWithImageData_ReceiveImageData>(globalObject, decoder);
    case MessageName::TestWithStream_SendString:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendString>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringSynchronized:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendStringSynchronized>(globalObject, decoder);
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
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return jsValueForDecodedMessage<MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer>(globalObject, decoder);
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return jsValueForDecodedMessage<MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer>(globalObject, decoder);
#endif
    default:
        break;
    }
    return std::nullopt;
}

std::optional<JSC::JSValue> jsValueForReplyArguments(JSC::JSGlobalObject* globalObject, MessageName name, Decoder& decoder)
{
    switch (name) {
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
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return jsValueForDecodedMessageReply<MessageName::TestWithSemaphore_ReceiveSemaphore>(globalObject, decoder);
    case MessageName::TestWithImageData_ReceiveImageData:
        return jsValueForDecodedMessageReply<MessageName::TestWithImageData_ReceiveImageData>(globalObject, decoder);
    case MessageName::TestWithStream_SendStringSynchronized:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringSynchronized>(globalObject, decoder);
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_ReceiveMachSendRight>(globalObject, decoder);
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return jsValueForDecodedMessageReply<MessageName::TestWithStream_SendAndReceiveMachSendRight>(globalObject, decoder);
#endif
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return jsValueForDecodedMessageReply<MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer>(globalObject, decoder);
#endif
    default:
        break;
    }
    return std::nullopt;
}

#endif // ENABLE(IPC_TESTING_API)

std::optional<Vector<ArgumentDescription>> messageArgumentDescriptions(MessageName name)
{
    switch (name) {
    case MessageName::TestWithSuperclass_LoadURL:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "twoStateEnum", "bool", "WebKit::TestTwoStateEnum", false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "value", "int", nullptr, false },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param", "uint32_t", nullptr, false },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "value", "bool", nullptr, false },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_LoadURL:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event", "WebKit::WebTouchEvent", nullptr, false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_AddEvent:
        return Vector<ArgumentDescription> {
            { "event", "WebKit::WebTouchEvent", nullptr, false },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#endif
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID", "uint64_t", nullptr, false },
            { "listenerID", "uint64_t", nullptr, false },
            { "policyAction", "uint32_t", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store", "WebKit::WebPreferencesStore", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d", "double", nullptr, false },
            { "f", "float", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_SendInts:
        return Vector<ArgumentDescription> {
            { "ints", "Vector<uint64_t>", nullptr, false },
            { "intVectors", "Vector<Vector<uint64_t>>", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID", "uint64_t", nullptr, false },
            { "parameters", "WebKit::Plugin::Parameters", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID", "uint64_t", nullptr, false },
            { "message", "String", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh", "bool", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath", "String", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo", "uint64_t", nullptr, false },
            { "bar", "double", nullptr, false },
            { "baz", "double", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a", "HashMap<String, std::pair<String, uint64_t>>", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID", "WebCore::GraphicsLayer::PlatformLayerID", nullptr, false },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier", "MachSendRight", nullptr, false },
            { "flags", "OptionSet<WebKit::SelectionFlags>", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type", "uint32_t", nullptr, false },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy", "IPC::DummyType", nullptr, false },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy", "IPC::DummyType", nullptr, false },
        };
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_LoadURL:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event", "WebKit::WebTouchEvent", nullptr, false },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_AddEvent:
        return Vector<ArgumentDescription> {
            { "event", "WebKit::WebTouchEvent", nullptr, false },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#endif
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID", "uint64_t", nullptr, false },
            { "listenerID", "uint64_t", nullptr, false },
            { "policyAction", "uint32_t", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store", "WebKit::WebPreferencesStore", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d", "double", nullptr, false },
            { "f", "float", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_SendInts:
        return Vector<ArgumentDescription> {
            { "ints", "Vector<uint64_t>", nullptr, false },
            { "intVectors", "Vector<Vector<uint64_t>>", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID", "uint64_t", nullptr, false },
            { "parameters", "WebKit::Plugin::Parameters", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID", "uint64_t", nullptr, false },
            { "message", "String", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh", "bool", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath", "String", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo", "uint64_t", nullptr, false },
            { "bar", "double", nullptr, false },
            { "baz", "double", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a", "HashMap<String, std::pair<String, uint64_t>>", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID", "WebCore::GraphicsLayer::PlatformLayerID", nullptr, false },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier", "MachSendRight", nullptr, false },
            { "flags", "OptionSet<WebKit::SelectionFlags>", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type", "uint32_t", nullptr, false },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy", "IPC::DummyType", nullptr, false },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy", "IPC::DummyType", nullptr, false },
        };
#endif
#endif
#if PLATFORM(COCOA)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#endif
#if PLATFORM(GTK)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
            { "value", "int64_t", nullptr, false },
        };
#endif
    case MessageName::TestWithSemaphore_SendSemaphore:
        return Vector<ArgumentDescription> {
            { "s0", "IPC::Semaphore", nullptr, false },
        };
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithImageData_SendImageData:
        return Vector<ArgumentDescription> {
            { "s0", "RefPtr<WebCore::ImageData>", nullptr, false },
        };
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStream_SendString:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
    case MessageName::TestWithStream_SendStringSynchronized:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_SendMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1", "MachSendRight", nullptr, false },
        };
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1", "MachSendRight", nullptr, false },
        };
#endif
    case MessageName::TestWithStreamBatched_SendString:
        return Vector<ArgumentDescription> {
            { "url", "String", nullptr, false },
        };
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return Vector<ArgumentDescription> {
            { "stream", "IPC::StreamConnectionBuffer", nullptr, false },
        };
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_SendCVPixelBuffer:
        return Vector<ArgumentDescription> {
            { "s0", "RetainPtr<CVPixelBufferRef>", nullptr, false },
        };
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return Vector<ArgumentDescription> { };
#endif
    default:
        break;
    }
    return std::nullopt;
}

std::optional<Vector<ArgumentDescription>> messageReplyArgumentDescriptions(MessageName name)
{
    switch (name) {
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "result", "uint64_t", nullptr, false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> {
            { "flag", "bool", nullptr, false },
            { "value", "uint64_t", nullptr, false },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "flag", "bool", nullptr, false },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply", "uint8_t", nullptr, false },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "optionalReply", "WebKit::TestClassName", nullptr, true },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result", "bool", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins", "Vector<WebCore::PluginInfo>", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle", "IPC::Connection::Handle", nullptr, false },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName", "Vector<WebCore::KeypressCommand>", nullptr, false },
        };
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result", "bool", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins", "Vector<WebCore::PluginInfo>", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle", "IPC::Connection::Handle", nullptr, false },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName", "Vector<WebCore::KeypressCommand>", nullptr, false },
        };
#endif
#endif
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> {
            { "r0", "IPC::Semaphore", nullptr, false },
        };
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> {
            { "r0", "RefPtr<WebCore::ImageData>", nullptr, false },
        };
    case MessageName::TestWithStream_SendStringSynchronized:
        return Vector<ArgumentDescription> {
            { "returnValue", "int64_t", nullptr, false },
        };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1", "MachSendRight", nullptr, false },
        };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1", "MachSendRight", nullptr, false },
        };
#endif
#if USE(AVFOUNDATION)
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return Vector<ArgumentDescription> {
            { "r0", "RetainPtr<CVPixelBufferRef>", nullptr, false },
        };
#endif
    default:
        break;
    }
    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API) || !LOG_DISABLED
