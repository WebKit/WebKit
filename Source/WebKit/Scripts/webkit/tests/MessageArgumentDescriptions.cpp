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

#include "JSIPCBinding.h"
#include "MessageNames.h"

#if ENABLE(IPC_TESTING_API) || !LOG_DISABLED

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
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply:
        return jsValueForDecodedMessage<MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply>(globalObject, decoder);
#endif
    case MessageName::TestWithDeferSendingOption_NoOptions:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_NoOptions>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_NoIndices:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_NoIndices>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_OneIndex:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_OneIndex>(globalObject, decoder);
    case MessageName::TestWithDeferSendingOption_MultipleIndices:
        return jsValueForDecodedMessage<MessageName::TestWithDeferSendingOption_MultipleIndices>(globalObject, decoder);
    case MessageName::TestWithDispatchedFromAndTo_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithDispatchedFromAndTo_AlwaysEnabled>(globalObject, decoder);
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
    case MessageName::TestWithImageData_ReceiveImageDataReply:
        return jsValueForDecodedMessage<MessageName::TestWithImageData_ReceiveImageDataReply>(globalObject, decoder);
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
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion>(globalObject, decoder);
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
    case MessageName::TestWithLegacyReceiver_CreatePluginReply:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_CreatePluginReply>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_GetPluginsReply:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPluginsReply>(globalObject, decoder);
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEventReply:
        return jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_InterpretKeyEventReply>(globalObject, decoder);
#endif
#endif
    case MessageName::TestWithMultiLineExtendedAttributes_AlwaysEnabled:
        return jsValueForDecodedMessage<MessageName::TestWithMultiLineExtendedAttributes_AlwaysEnabled>(globalObject, decoder);
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
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion>(globalObject, decoder);
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
    case MessageName::TestWithoutAttributes_CreatePluginReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_CreatePluginReply>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_RunJavaScriptAlertReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_RunJavaScriptAlertReply>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_GetPluginsReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPluginsReply>(globalObject, decoder);
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertionReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertionReply>(globalObject, decoder);
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEventReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutAttributes_InterpretKeyEventReply>(globalObject, decoder);
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
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply>(globalObject, decoder);
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply:
        return jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply>(globalObject, decoder);
    case MessageName::TestWithSemaphore_SendSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_SendSemaphore>(globalObject, decoder);
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_ReceiveSemaphore>(globalObject, decoder);
    case MessageName::TestWithSemaphore_ReceiveSemaphoreReply:
        return jsValueForDecodedMessage<MessageName::TestWithSemaphore_ReceiveSemaphoreReply>(globalObject, decoder);
    case MessageName::TestWithSpanOfConst_TestSpanOfConstFloat:
        return jsValueForDecodedMessage<MessageName::TestWithSpanOfConst_TestSpanOfConstFloat>(globalObject, decoder);
    case MessageName::TestWithSpanOfConst_TestSpanOfConstFloatSegments:
        return jsValueForDecodedMessage<MessageName::TestWithSpanOfConst_TestSpanOfConstFloatSegments>(globalObject, decoder);
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
    case MessageName::TestWithStream_SendStringAsyncReply:
        return jsValueForDecodedMessage<MessageName::TestWithStream_SendStringAsyncReply>(globalObject, decoder);
    case MessageName::TestWithStream_CallWithIdentifierReply:
        return jsValueForDecodedMessage<MessageName::TestWithStream_CallWithIdentifierReply>(globalObject, decoder);
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
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessageReply:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageReply>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply>(globalObject, decoder);
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply:
        return jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply>(globalObject, decoder);
#endif
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
    case MessageName::TestWithValidator_MessageWithReply:
        return jsValueForDecodedMessage<MessageName::TestWithValidator_MessageWithReply>(globalObject, decoder);
    case MessageName::TestWithValidator_MessageWithReplyReply:
        return jsValueForDecodedMessage<MessageName::TestWithValidator_MessageWithReplyReply>(globalObject, decoder);
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
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion:
        return jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion>(globalObject, decoder);
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
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion:
        return jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion>(globalObject, decoder);
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
    case MessageName::TestWithValidator_MessageWithReply:
        return jsValueForDecodedMessageReply<MessageName::TestWithValidator_MessageWithReply>(globalObject, decoder);
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
    return {
        "IPC::AsyncReplyID"_s,
        "WebCore::AttributedStringTextListID"_s,
        "WebCore::AttributedStringTextTableBlockID"_s,
        "WebCore::AttributedStringTextTableID"_s,
        "WebCore::BackForwardFrameItemIdentifierID"_s,
        "WebCore::BackForwardItemIdentifierID"_s,
        "WebCore::BackgroundFetchRecordIdentifier"_s,
        "WebCore::DOMCacheIdentifierID"_s,
        "WebCore::DictationContext"_s,
        "WebCore::NodeIdentifier"_s,
        "WebCore::FetchIdentifier"_s,
        "WebCore::FileSystemHandleIdentifier"_s,
        "WebCore::FileSystemSyncAccessHandleIdentifier"_s,
        "WebCore::FileSystemWritableFileStreamIdentifier"_s,
        "WebCore::FrameIdentifier"_s,
        "WebCore::IDBIndexIdentifier"_s,
        "WebCore::IDBObjectStoreIdentifier"_s,
        "WebCore::ImageDecoderIdentifier"_s,
        "WebCore::InbandGenericCueIdentifier"_s,
        "WebCore::LayerHostingContextIdentifier"_s,
        "WebCore::LibWebRTCSocketIdentifier"_s,
        "WebCore::MediaKeySystemRequestIdentifier"_s,
        "WebCore::MediaPlayerClientIdentifier"_s,
        "WebCore::MediaPlayerIdentifier"_s,
        "WebCore::MediaSessionGroupIdentifier"_s,
        "WebCore::MediaSessionIdentifier"_s,
        "WebCore::ModelPlayerIdentifier"_s,
        "WebCore::MediaUniqueIdentifier"_s,
        "WebCore::NavigationIdentifier"_s,
        "WebCore::OpaqueOriginIdentifier"_s,
        "WebCore::PageIdentifier"_s,
        "WebCore::PlatformLayerIdentifierID"_s,
        "WebCore::PlaybackTargetClientContextID"_s,
        "WebCore::PortIdentifier"_s,
        "WebCore::ProcessIdentifier"_s,
        "WebCore::PushSubscriptionIdentifier"_s,
        "WebCore::RTCDataChannelLocalIdentifier"_s,
        "WebCore::RealtimeMediaSourceIdentifier"_s,
        "WebCore::RenderingResourceIdentifier"_s,
        "WebCore::ResourceLoaderIdentifier"_s,
        "WebCore::SWServerConnectionIdentifier"_s,
        "WebCore::SamplesRendererTrackIdentifier"_s,
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
        "WebCore::TimelineIdentifier"_s,
        "WebCore::IDBDatabaseConnectionIdentifier"_s,
        "WebCore::IDBResourceObjectIdentifier"_s,
        "WebCore::UserGestureTokenIdentifierID"_s,
        "WebCore::UserMediaRequestIdentifier"_s,
        "WebCore::WebLockIdentifierID"_s,
        "WebCore::WebProcessJSHandleIdentifier"_s,
        "WebCore::WebSocketIdentifier"_s,
        "WebCore::WebTransportSendGroupIdentifier"_s,
        "WebCore::WebTransportStreamIdentifier"_s,
        "WebCore::WindowIdentifier"_s,
        "WebKit::AudioMediaStreamTrackRendererInternalUnitIdentifier"_s,
        "WebKit::AuthenticationChallengeIdentifier"_s,
        "WebKit::DDModelIdentifier"_s,
        "WebKit::DataTaskIdentifier"_s,
        "WebKit::DisplayLinkObserverID"_s,
        "WebKit::DownloadID"_s,
        "WebKit::DrawingAreaIdentifier"_s,
        "WebKit::GeolocationIdentifier"_s,
        "WebKit::GPUProcessConnectionIdentifier"_s,
        "WebKit::ImageBufferSetIdentifier"_s,
        "WebKit::RemoteGraphicsContextGLIdentifier"_s,
        "WebKit::RiceBackendIdentifier"_s,
        "WebKit::IPCConnectionTesterIdentifier"_s,
        "WebKit::IPCStreamTesterIdentifier"_s,
        "WebKit::JSObjectID"_s,
        "WebKit::LegacyCustomProtocolID"_s,
        "WebKit::LibWebRTCResolverIdentifier"_s,
        "WebKit::LogStreamIdentifier"_s,
        "WebKit::MarkSurfacesAsVolatileRequestIdentifier"_s,
        "WebKit::MessageBatchIdentifier"_s,
        "WebKit::NetworkResourceLoadIdentifier"_s,
        "WebKit::NonProcessQualifiedContentWorldIdentifier"_s,
        "WebKit::PDFPluginIdentifier"_s,
        "WebKit::PageGroupIdentifier"_s,
        "WebKit::QuotaIncreaseRequestIdentifier"_s,
        "WebKit::RemoteAudioDestinationIdentifier"_s,
        "WebKit::RemoteAudioHardwareListenerIdentifier"_s,
        "WebKit::RemoteAudioVideoRendererIdentifier"_s,
        "WebKit::RemoteCDMIdentifier"_s,
        "WebKit::RemoteCDMInstanceIdentifier"_s,
        "WebKit::RemoteCDMInstanceSessionIdentifier"_s,
        "WebKit::RemoteGraphicsContextIdentifier"_s,
        "WebKit::RemoteGradientIdentifier"_s,
        "WebKit::RemoteDisplayListIdentifier"_s,
        "WebKit::RemoteDisplayListRecorderIdentifier"_s,
        "WebKit::RemoteLegacyCDMIdentifier"_s,
        "WebKit::RemoteLegacyCDMSessionIdentifier"_s,
        "WebKit::RemoteMediaResourceIdentifier"_s,
        "WebKit::RemoteMediaSourceIdentifier"_s,
        "WebKit::RemoteRemoteCommandListenerIdentifier"_s,
        "WebKit::RemoteSerializedImageBufferIdentifier"_s,
        "WebKit::RemoteSnapshotIdentifier"_s,
        "WebKit::RemoteSnapshotRecorderIdentifier"_s,
        "WebKit::RemoteSourceBufferIdentifier"_s,
        "WebKit::RemoteVideoFrameIdentifier"_s,
        "WebKit::RemoteRenderingBackendIdentifier"_s,
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
            { "s0"_s, "RetainPtr<CVPixelBufferRef>"_s },
        };
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBuffer:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithCVPixelBuffer_ReceiveCVPixelBufferReply:
        return Vector<ArgumentDescription> {
            { "r0"_s, "RetainPtr<CVPixelBufferRef>"_s },
        };
#endif
    case MessageName::TestWithDeferSendingOption_NoOptions:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithDeferSendingOption_NoIndices:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithDeferSendingOption_OneIndex:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithDeferSendingOption_MultipleIndices:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
            { "foo"_s, "int"_s },
            { "bar"_s, "int"_s },
            { "baz"_s, "int"_s },
        };
    case MessageName::TestWithDispatchedFromAndTo_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithEnabledBy_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithEnabledBy_ConditionallyEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithEnabledBy_ConditionallyEnabledAnd:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithEnabledBy_ConditionallyEnabledOr:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithEnabledByAndConjunction_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithEnabledByOrConjunction_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#if PLATFORM(COCOA)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#endif
#if PLATFORM(GTK)
    case MessageName::TestWithIfMessage_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
            { "value"_s, "int64_t"_s },
        };
#endif
    case MessageName::TestWithImageData_SendImageData:
        return Vector<ArgumentDescription> {
            { "s0"_s, "RefPtr<WebCore::ImageData>"_s },
        };
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithImageData_ReceiveImageDataReply:
        return Vector<ArgumentDescription> {
            { "r0"_s, "RefPtr<WebCore::ImageData>"_s },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithLegacyReceiver_AddEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithLegacyReceiver_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#endif
    case MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s },
            { "listenerID"_s, "uint64_t"_s },
            { "policyAction"_s, "uint32_t"_s },
        };
    case MessageName::TestWithLegacyReceiver_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store"_s, "WebKit::WebPreferencesStore"_s },
        };
    case MessageName::TestWithLegacyReceiver_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d"_s, "double"_s },
            { "f"_s, "float"_s },
        };
    case MessageName::TestWithLegacyReceiver_SendInts:
        return Vector<ArgumentDescription> {
            { "ints"_s, "Vector<uint64_t>"_s },
            { "intVectors"_s, "Vector<Vector<uint64_t>>"_s },
        };
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID"_s, "uint64_t"_s },
            { "parameters"_s, "WebKit::Plugin::Parameters"_s },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s },
            { "message"_s, "String"_s },
        };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh"_s, "bool"_s },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath"_s, "String"_s },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo"_s, "uint64_t"_s },
            { "bar"_s, "double"_s },
            { "baz"_s, "double"_s },
        };
    case MessageName::TestWithLegacyReceiver_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a"_s, "HashMap<String, std::pair<String, uint64_t>>"_s },
        };
    case MessageName::TestWithLegacyReceiver_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID"_s, "WebCore::PlatformLayerIdentifier"_s },
        };
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion:
        return Vector<ArgumentDescription> {
            { "ping"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier"_s, "MachSendRight"_s },
            { "flags"_s, "OptionSet<WebKit::SelectionFlags>"_s },
        };
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type"_s, "uint32_t"_s },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithLegacyReceiver_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithLegacyReceiver_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s },
        };
#endif
    case MessageName::TestWithLegacyReceiver_CreatePluginReply:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_GetPluginsReply:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s },
        };
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply:
        return Vector<ArgumentDescription> {
            { "pong"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEventReply:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s },
        };
#endif
#endif
    case MessageName::TestWithMultiLineExtendedAttributes_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomething:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_TouchEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s },
        };
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::TestWithoutAttributes_AddEvent:
        return Vector<ArgumentDescription> {
            { "event"_s, "WebKit::WebTouchEvent"_s },
        };
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::TestWithoutAttributes_LoadSomethingElse:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#endif
    case MessageName::TestWithoutAttributes_DidReceivePolicyDecision:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s },
            { "listenerID"_s, "uint64_t"_s },
            { "policyAction"_s, "uint32_t"_s },
        };
    case MessageName::TestWithoutAttributes_Close:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_PreferencesDidChange:
        return Vector<ArgumentDescription> {
            { "store"_s, "WebKit::WebPreferencesStore"_s },
        };
    case MessageName::TestWithoutAttributes_SendDoubleAndFloat:
        return Vector<ArgumentDescription> {
            { "d"_s, "double"_s },
            { "f"_s, "float"_s },
        };
    case MessageName::TestWithoutAttributes_SendInts:
        return Vector<ArgumentDescription> {
            { "ints"_s, "Vector<uint64_t>"_s },
            { "intVectors"_s, "Vector<Vector<uint64_t>>"_s },
        };
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "pluginInstanceID"_s, "uint64_t"_s },
            { "parameters"_s, "WebKit::Plugin::Parameters"_s },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> {
            { "frameID"_s, "uint64_t"_s },
            { "message"_s, "String"_s },
        };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "refresh"_s, "bool"_s },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "pluginPath"_s, "String"_s },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_TestParameterAttributes:
        return Vector<ArgumentDescription> {
            { "foo"_s, "uint64_t"_s },
            { "bar"_s, "double"_s },
            { "baz"_s, "double"_s },
        };
    case MessageName::TestWithoutAttributes_TemplateTest:
        return Vector<ArgumentDescription> {
            { "a"_s, "HashMap<String, std::pair<String, uint64_t>>"_s },
        };
    case MessageName::TestWithoutAttributes_SetVideoLayerID:
        return Vector<ArgumentDescription> {
            { "videoLayerID"_s, "WebCore::PlatformLayerIdentifier"_s },
        };
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion:
        return Vector<ArgumentDescription> {
            { "ping"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_DidCreateWebProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionIdentifier"_s, "MachSendRight"_s },
            { "flags"_s, "OptionSet<WebKit::SelectionFlags>"_s },
        };
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "type"_s, "uint32_t"_s },
        };
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::TestWithoutAttributes_DeprecatedOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s },
        };
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    case MessageName::TestWithoutAttributes_ExperimentalOperation:
        return Vector<ArgumentDescription> {
            { "dummy"_s, "IPC::DummyType"_s },
        };
#endif
    case MessageName::TestWithoutAttributes_CreatePluginReply:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlertReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_GetPluginsReply:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s },
        };
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertionReply:
        return Vector<ArgumentDescription> {
            { "pong"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEventReply:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s },
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
            { "argument"_s, "String"_s },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return Vector<ArgumentDescription> {
            { "argument"_s, "String"_s },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "argument"_s, "String"_s },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReplyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgumentReply:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReplyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgumentReply:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
        };
    case MessageName::TestWithSemaphore_SendSemaphore:
        return Vector<ArgumentDescription> {
            { "s0"_s, "IPC::Semaphore"_s },
        };
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSemaphore_ReceiveSemaphoreReply:
        return Vector<ArgumentDescription> {
            { "r0"_s, "IPC::Semaphore"_s },
        };
    case MessageName::TestWithSpanOfConst_TestSpanOfConstFloat:
        return Vector<ArgumentDescription> {
            { "floats"_s, "std::span<const float>"_s },
        };
    case MessageName::TestWithSpanOfConst_TestSpanOfConstFloatSegments:
        return Vector<ArgumentDescription> {
            { "floatSegments"_s, "std::span<const WebCore::FloatSegment>"_s },
        };
    case MessageName::TestWithStream_SendString:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithStream_SendStringAsync:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithStream_SendStringSync:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithStream_CallWithIdentifier:
        return Vector<ArgumentDescription> { };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_SendMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1"_s, "MachSendRight"_s },
        };
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "a1"_s, "MachSendRight"_s },
        };
#endif
    case MessageName::TestWithStream_SendStringAsyncReply:
        return Vector<ArgumentDescription> {
            { "returnValue"_s, "int64_t"_s },
        };
    case MessageName::TestWithStream_CallWithIdentifierReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithStreamBatched_SendString:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithStreamBuffer_SendStreamBuffer:
        return Vector<ArgumentDescription> {
            { "stream"_s, "IPC::StreamConnectionBuffer"_s },
        };
    case MessageName::TestWithStreamServerConnectionHandle_SendStreamServerConnection:
        return Vector<ArgumentDescription> {
            { "handle"_s, "IPC::StreamServerConnectionHandle"_s },
        };
    case MessageName::TestWithSuperclass_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "twoStateEnum"_s, "WebKit::TestTwoStateEnum"_s },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "value"_s, "int"_s },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "value"_s, "bool"_s },
        };
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessageReply:
        return Vector<ArgumentDescription> {
            { "result"_s, "uint64_t"_s },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s },
            { "value"_s, "uint64_t"_s },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s },
        };
#endif
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_LoadURL:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s },
        };
    case MessageName::TestWithValidator_AlwaysEnabled:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithValidator_EnabledIfPassValidation:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithValidator_MessageWithReply:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithValidator_MessageWithReplyReply:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
            { "value"_s, "double"_s },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s },
        };
    case MessageName::TestWithWantsDispatch_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
        };
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "param"_s, "uint32_t"_s },
        };
    case MessageName::TestWithWantsDispatchNoSyncMessages_TestMessage:
        return Vector<ArgumentDescription> {
            { "url"_s, "String"_s },
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
            { "r0"_s, "RetainPtr<CVPixelBufferRef>"_s },
        };
#endif
    case MessageName::TestWithImageData_ReceiveImageData:
        return Vector<ArgumentDescription> {
            { "r0"_s, "RefPtr<WebCore::ImageData>"_s },
        };
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithLegacyReceiver_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s },
        };
    case MessageName::TestWithLegacyReceiver_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s },
        };
    case MessageName::TestWithLegacyReceiver_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle"_s, "IPC::Connection::Handle"_s },
        };
    case MessageName::TestWithLegacyReceiver_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion:
        return Vector<ArgumentDescription> {
            { "pong"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithLegacyReceiver_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s },
        };
#endif
#endif
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
    case MessageName::TestWithoutAttributes_CreatePlugin:
        return Vector<ArgumentDescription> {
            { "result"_s, "bool"_s },
        };
    case MessageName::TestWithoutAttributes_RunJavaScriptAlert:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_GetPlugins:
        return Vector<ArgumentDescription> {
            { "plugins"_s, "Vector<WebCore::PluginInfo>"_s },
        };
    case MessageName::TestWithoutAttributes_GetPluginProcessConnection:
        return Vector<ArgumentDescription> {
            { "connectionHandle"_s, "IPC::Connection::Handle"_s },
        };
    case MessageName::TestWithoutAttributes_TestMultipleAttributes:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutAttributes_OpaqueTypeSecurityAssertion:
        return Vector<ArgumentDescription> {
            { "pong"_s, "NotDispatchableFromWebContent"_s },
        };
#if PLATFORM(MAC)
    case MessageName::TestWithoutAttributes_InterpretKeyEvent:
        return Vector<ArgumentDescription> {
            { "commandName"_s, "Vector<WebCore::KeypressCommand>"_s },
        };
#endif
#endif
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
        };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
        };
    case MessageName::TestWithSemaphore_ReceiveSemaphore:
        return Vector<ArgumentDescription> {
            { "r0"_s, "IPC::Semaphore"_s },
        };
    case MessageName::TestWithStream_SendStringAsync:
        return Vector<ArgumentDescription> {
            { "returnValue"_s, "int64_t"_s },
        };
    case MessageName::TestWithStream_SendStringSync:
        return Vector<ArgumentDescription> {
            { "returnValue"_s, "int64_t"_s },
        };
    case MessageName::TestWithStream_CallWithIdentifier:
        return Vector<ArgumentDescription> { };
#if PLATFORM(COCOA)
    case MessageName::TestWithStream_ReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1"_s, "MachSendRight"_s },
        };
    case MessageName::TestWithStream_SendAndReceiveMachSendRight:
        return Vector<ArgumentDescription> {
            { "r1"_s, "MachSendRight"_s },
        };
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::TestWithSuperclass_TestAsyncMessage:
        return Vector<ArgumentDescription> {
            { "result"_s, "uint64_t"_s },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments:
        return Vector<ArgumentDescription> { };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s },
            { "value"_s, "uint64_t"_s },
        };
    case MessageName::TestWithSuperclass_TestAsyncMessageWithConnection:
        return Vector<ArgumentDescription> {
            { "flag"_s, "bool"_s },
        };
#endif
    case MessageName::TestWithSuperclass_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s },
        };
    case MessageName::TestWithSuperclass_TestSynchronousMessage:
        return Vector<ArgumentDescription> {
            { "optionalReply"_s, "std::optional<WebKit::TestClassName>"_s },
        };
    case MessageName::TestWithSuperclassAndWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s },
        };
    case MessageName::TestWithSuperclassAndWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s },
        };
    case MessageName::TestWithValidator_MessageWithReply:
        return Vector<ArgumentDescription> {
            { "reply"_s, "String"_s },
            { "value"_s, "double"_s },
        };
    case MessageName::TestWithWantsAsyncDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s },
        };
    case MessageName::TestWithWantsDispatch_TestSyncMessage:
        return Vector<ArgumentDescription> {
            { "reply"_s, "uint8_t"_s },
        };
    default:
        break;
    }
    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API) || !LOG_DISABLED
