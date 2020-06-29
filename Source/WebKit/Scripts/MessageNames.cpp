/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
    case MessageName::WebPage_LoadURL:
        return "WebPage::LoadURL";
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessage:
        return "WebPage::TestAsyncMessage";
    case MessageName::WebPage_TestAsyncMessageReply:
        return "WebPage::TestAsyncMessageReply";
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithNoArguments:
        return "WebPage::TestAsyncMessageWithNoArguments";
    case MessageName::WebPage_TestAsyncMessageWithNoArgumentsReply:
        return "WebPage::TestAsyncMessageWithNoArgumentsReply";
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithMultipleArguments:
        return "WebPage::TestAsyncMessageWithMultipleArguments";
    case MessageName::WebPage_TestAsyncMessageWithMultipleArgumentsReply:
        return "WebPage::TestAsyncMessageWithMultipleArgumentsReply";
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithConnection:
        return "WebPage::TestAsyncMessageWithConnection";
    case MessageName::WebPage_TestAsyncMessageWithConnectionReply:
        return "WebPage::TestAsyncMessageWithConnectionReply";
#endif
    case MessageName::WebPage_TestSyncMessage:
        return "WebPage::TestSyncMessage";
    case MessageName::WebPage_TestSynchronousMessage:
        return "WebPage::TestSynchronousMessage";
    case MessageName::WebPage_LoadURL:
        return "WebPage::LoadURL";
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomething:
        return "WebPage::LoadSomething";
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_TouchEvent:
        return "WebPage::TouchEvent";
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_AddEvent:
        return "WebPage::AddEvent";
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomethingElse:
        return "WebPage::LoadSomethingElse";
#endif
    case MessageName::WebPage_DidReceivePolicyDecision:
        return "WebPage::DidReceivePolicyDecision";
    case MessageName::WebPage_Close:
        return "WebPage::Close";
    case MessageName::WebPage_PreferencesDidChange:
        return "WebPage::PreferencesDidChange";
    case MessageName::WebPage_SendDoubleAndFloat:
        return "WebPage::SendDoubleAndFloat";
    case MessageName::WebPage_SendInts:
        return "WebPage::SendInts";
    case MessageName::WebPage_CreatePlugin:
        return "WebPage::CreatePlugin";
    case MessageName::WebPage_RunJavaScriptAlert:
        return "WebPage::RunJavaScriptAlert";
    case MessageName::WebPage_GetPlugins:
        return "WebPage::GetPlugins";
    case MessageName::WebPage_GetPluginProcessConnection:
        return "WebPage::GetPluginProcessConnection";
    case MessageName::WebPage_TestMultipleAttributes:
        return "WebPage::TestMultipleAttributes";
    case MessageName::WebPage_TestParameterAttributes:
        return "WebPage::TestParameterAttributes";
    case MessageName::WebPage_TemplateTest:
        return "WebPage::TemplateTest";
    case MessageName::WebPage_SetVideoLayerID:
        return "WebPage::SetVideoLayerID";
#if PLATFORM(MAC)
    case MessageName::WebPage_DidCreateWebProcessConnection:
        return "WebPage::DidCreateWebProcessConnection";
#endif
#if PLATFORM(MAC)
    case MessageName::WebPage_InterpretKeyEvent:
        return "WebPage::InterpretKeyEvent";
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::WebPage_DeprecatedOperation:
        return "WebPage::DeprecatedOperation";
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    case MessageName::WebPage_ExperimentalOperation:
        return "WebPage::ExperimentalOperation";
#endif
    case MessageName::WebPage_LoadURL:
        return "WebPage::LoadURL";
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomething:
        return "WebPage::LoadSomething";
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_TouchEvent:
        return "WebPage::TouchEvent";
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_AddEvent:
        return "WebPage::AddEvent";
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomethingElse:
        return "WebPage::LoadSomethingElse";
#endif
    case MessageName::WebPage_DidReceivePolicyDecision:
        return "WebPage::DidReceivePolicyDecision";
    case MessageName::WebPage_Close:
        return "WebPage::Close";
    case MessageName::WebPage_PreferencesDidChange:
        return "WebPage::PreferencesDidChange";
    case MessageName::WebPage_SendDoubleAndFloat:
        return "WebPage::SendDoubleAndFloat";
    case MessageName::WebPage_SendInts:
        return "WebPage::SendInts";
    case MessageName::WebPage_CreatePlugin:
        return "WebPage::CreatePlugin";
    case MessageName::WebPage_RunJavaScriptAlert:
        return "WebPage::RunJavaScriptAlert";
    case MessageName::WebPage_GetPlugins:
        return "WebPage::GetPlugins";
    case MessageName::WebPage_GetPluginProcessConnection:
        return "WebPage::GetPluginProcessConnection";
    case MessageName::WebPage_TestMultipleAttributes:
        return "WebPage::TestMultipleAttributes";
    case MessageName::WebPage_TestParameterAttributes:
        return "WebPage::TestParameterAttributes";
    case MessageName::WebPage_TemplateTest:
        return "WebPage::TemplateTest";
    case MessageName::WebPage_SetVideoLayerID:
        return "WebPage::SetVideoLayerID";
#if PLATFORM(MAC)
    case MessageName::WebPage_DidCreateWebProcessConnection:
        return "WebPage::DidCreateWebProcessConnection";
#endif
#if PLATFORM(MAC)
    case MessageName::WebPage_InterpretKeyEvent:
        return "WebPage::InterpretKeyEvent";
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::WebPage_DeprecatedOperation:
        return "WebPage::DeprecatedOperation";
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    case MessageName::WebPage_ExperimentalOperation:
        return "WebPage::ExperimentalOperation";
#endif
    case MessageName::WrappedAsyncMessageForTesting:
        return "IPC::WrappedAsyncMessageForTesting";
    case MessageName::SyncMessageReply:
        return "IPC::SyncMessageReply";
    case MessageName::InitializeConnection:
        return "IPC::InitializeConnection";
    case MessageName::LegacySessionState:
        return "IPC::LegacySessionState";
    }
    ASSERT_NOT_REACHED();
    return "<invalid message name>";
}

ReceiverName receiverName(MessageName messageName)
{
    switch (messageName) {
    case MessageName::WebPage_LoadURL:
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessage:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithNoArguments:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithMultipleArguments:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithConnection:
#endif
    case MessageName::WebPage_TestSyncMessage:
    case MessageName::WebPage_TestSynchronousMessage:
        return ReceiverName::WebPage;
    case MessageName::WebPage_LoadURL:
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomething:
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_TouchEvent:
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_AddEvent:
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomethingElse:
#endif
    case MessageName::WebPage_DidReceivePolicyDecision:
    case MessageName::WebPage_Close:
    case MessageName::WebPage_PreferencesDidChange:
    case MessageName::WebPage_SendDoubleAndFloat:
    case MessageName::WebPage_SendInts:
    case MessageName::WebPage_CreatePlugin:
    case MessageName::WebPage_RunJavaScriptAlert:
    case MessageName::WebPage_GetPlugins:
    case MessageName::WebPage_GetPluginProcessConnection:
    case MessageName::WebPage_TestMultipleAttributes:
    case MessageName::WebPage_TestParameterAttributes:
    case MessageName::WebPage_TemplateTest:
    case MessageName::WebPage_SetVideoLayerID:
#if PLATFORM(MAC)
    case MessageName::WebPage_DidCreateWebProcessConnection:
#endif
#if PLATFORM(MAC)
    case MessageName::WebPage_InterpretKeyEvent:
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::WebPage_DeprecatedOperation:
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    case MessageName::WebPage_ExperimentalOperation:
#endif
        return ReceiverName::WebPage;
    case MessageName::WebPage_LoadURL:
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomething:
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_TouchEvent:
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    case MessageName::WebPage_AddEvent:
#endif
#if ENABLE(TOUCH_EVENTS)
    case MessageName::WebPage_LoadSomethingElse:
#endif
    case MessageName::WebPage_DidReceivePolicyDecision:
    case MessageName::WebPage_Close:
    case MessageName::WebPage_PreferencesDidChange:
    case MessageName::WebPage_SendDoubleAndFloat:
    case MessageName::WebPage_SendInts:
    case MessageName::WebPage_CreatePlugin:
    case MessageName::WebPage_RunJavaScriptAlert:
    case MessageName::WebPage_GetPlugins:
    case MessageName::WebPage_GetPluginProcessConnection:
    case MessageName::WebPage_TestMultipleAttributes:
    case MessageName::WebPage_TestParameterAttributes:
    case MessageName::WebPage_TemplateTest:
    case MessageName::WebPage_SetVideoLayerID:
#if PLATFORM(MAC)
    case MessageName::WebPage_DidCreateWebProcessConnection:
#endif
#if PLATFORM(MAC)
    case MessageName::WebPage_InterpretKeyEvent:
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case MessageName::WebPage_DeprecatedOperation:
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    case MessageName::WebPage_ExperimentalOperation:
#endif
        return ReceiverName::WebPage;
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageReply:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithNoArgumentsReply:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithMultipleArgumentsReply:
#endif
#if ENABLE(TEST_FEATURE)
    case MessageName::WebPage_TestAsyncMessageWithConnectionReply:
#endif
        return ReceiverName::AsyncReply;
    case MessageName::WrappedAsyncMessageForTesting:
    case MessageName::SyncMessageReply:
    case MessageName::InitializeConnection:
    case MessageName::LegacySessionState:
        return ReceiverName::IPC;
    }
    ASSERT_NOT_REACHED();
    return ReceiverName::Invalid;
}

bool isValidMessageName(MessageName messageName)
{
    if (messageName == IPC::MessageName::WebPage_LoadURL)
        return true;
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessage)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithNoArguments)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithNoArgumentsReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithMultipleArguments)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithMultipleArgumentsReply)
        return true;
#endif
#if ENABLE(TEST_FEATURE)
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithConnection)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestAsyncMessageWithConnectionReply)
        return true;
#endif
    if (messageName == IPC::MessageName::WebPage_TestSyncMessage)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestSynchronousMessage)
        return true;
    if (messageName == IPC::MessageName::WebPage_LoadURL)
        return true;
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::WebPage_LoadSomething)
        return true;
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::WebPage_TouchEvent)
        return true;
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::WebPage_AddEvent)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::WebPage_LoadSomethingElse)
        return true;
#endif
    if (messageName == IPC::MessageName::WebPage_DidReceivePolicyDecision)
        return true;
    if (messageName == IPC::MessageName::WebPage_Close)
        return true;
    if (messageName == IPC::MessageName::WebPage_PreferencesDidChange)
        return true;
    if (messageName == IPC::MessageName::WebPage_SendDoubleAndFloat)
        return true;
    if (messageName == IPC::MessageName::WebPage_SendInts)
        return true;
    if (messageName == IPC::MessageName::WebPage_CreatePlugin)
        return true;
    if (messageName == IPC::MessageName::WebPage_RunJavaScriptAlert)
        return true;
    if (messageName == IPC::MessageName::WebPage_GetPlugins)
        return true;
    if (messageName == IPC::MessageName::WebPage_GetPluginProcessConnection)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestMultipleAttributes)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestParameterAttributes)
        return true;
    if (messageName == IPC::MessageName::WebPage_TemplateTest)
        return true;
    if (messageName == IPC::MessageName::WebPage_SetVideoLayerID)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::WebPage_DidCreateWebProcessConnection)
        return true;
#endif
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::WebPage_InterpretKeyEvent)
        return true;
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (messageName == IPC::MessageName::WebPage_DeprecatedOperation)
        return true;
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (messageName == IPC::MessageName::WebPage_ExperimentalOperation)
        return true;
#endif
    if (messageName == IPC::MessageName::WebPage_LoadURL)
        return true;
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::WebPage_LoadSomething)
        return true;
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::WebPage_TouchEvent)
        return true;
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (messageName == IPC::MessageName::WebPage_AddEvent)
        return true;
#endif
#if ENABLE(TOUCH_EVENTS)
    if (messageName == IPC::MessageName::WebPage_LoadSomethingElse)
        return true;
#endif
    if (messageName == IPC::MessageName::WebPage_DidReceivePolicyDecision)
        return true;
    if (messageName == IPC::MessageName::WebPage_Close)
        return true;
    if (messageName == IPC::MessageName::WebPage_PreferencesDidChange)
        return true;
    if (messageName == IPC::MessageName::WebPage_SendDoubleAndFloat)
        return true;
    if (messageName == IPC::MessageName::WebPage_SendInts)
        return true;
    if (messageName == IPC::MessageName::WebPage_CreatePlugin)
        return true;
    if (messageName == IPC::MessageName::WebPage_RunJavaScriptAlert)
        return true;
    if (messageName == IPC::MessageName::WebPage_GetPlugins)
        return true;
    if (messageName == IPC::MessageName::WebPage_GetPluginProcessConnection)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestMultipleAttributes)
        return true;
    if (messageName == IPC::MessageName::WebPage_TestParameterAttributes)
        return true;
    if (messageName == IPC::MessageName::WebPage_TemplateTest)
        return true;
    if (messageName == IPC::MessageName::WebPage_SetVideoLayerID)
        return true;
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::WebPage_DidCreateWebProcessConnection)
        return true;
#endif
#if PLATFORM(MAC)
    if (messageName == IPC::MessageName::WebPage_InterpretKeyEvent)
        return true;
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (messageName == IPC::MessageName::WebPage_DeprecatedOperation)
        return true;
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (messageName == IPC::MessageName::WebPage_ExperimentalOperation)
        return true;
#endif
    if (messageName == IPC::MessageName::WrappedAsyncMessageForTesting)
        return true;
    if (messageName == IPC::MessageName::SyncMessageReply)
        return true;
    if (messageName == IPC::MessageName::InitializeConnection)
        return true;
    if (messageName == IPC::MessageName::LegacySessionState)
        return true;
    return false;
};

} // namespace IPC
