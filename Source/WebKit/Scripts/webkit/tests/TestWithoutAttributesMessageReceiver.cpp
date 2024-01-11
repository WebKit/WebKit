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
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithoutAttributes.h"

#include "ArgumentCoders.h" // NOLINT
#if PLATFORM(MAC)
#include "ArgumentCodersDarwin.h" // NOLINT
#endif
#include "Connection.h" // NOLINT
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(FEATURE_FOR_TESTING)
#include "DummyType.h" // NOLINT
#endif
#if PLATFORM(MAC)
#include "GestureTypes.h" // NOLINT
#endif
#include "HandleMessage.h" // NOLINT
#include "Message.h" // NOLINT
#include "Plugin.h" // NOLINT
#include "TestWithoutAttributesMessages.h" // NOLINT
#include "WebCoreArgumentCoders.h" // NOLINT
#include "WebPreferencesStore.h" // NOLINT
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION)) || (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
#include "WebTouchEvent.h" // NOLINT
#endif
#if PLATFORM(MAC)
#include <WebCore/KeyboardEvent.h> // NOLINT
#endif
#include <WebCore/PlatformLayerIdentifier.h> // NOLINT
#include <WebCore/PluginData.h> // NOLINT
#include <utility> // NOLINT
#include <wtf/HashMap.h> // NOLINT
#if PLATFORM(MAC)
#include <wtf/MachSendRight.h> // NOLINT
#endif
#if PLATFORM(MAC)
#include <wtf/OptionSet.h> // NOLINT
#endif
#include <wtf/Vector.h> // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithoutAttributes::didReceiveMessage(IPC::Connection& connection, IPC::Message&& message)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithoutAttributes::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadURL>(connection, WTFMove(message), this, &TestWithoutAttributes::loadURL);
#if ENABLE(TOUCH_EVENTS)
    if (message.messageName() == Messages::TestWithoutAttributes::LoadSomething::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomething>(connection, WTFMove(message), this, &TestWithoutAttributes::loadSomething);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (message.messageName() == Messages::TestWithoutAttributes::TouchEvent::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TouchEvent>(connection, WTFMove(message), this, &TestWithoutAttributes::touchEvent);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (message.messageName() == Messages::TestWithoutAttributes::AddEvent::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::AddEvent>(connection, WTFMove(message), this, &TestWithoutAttributes::addEvent);
#endif
#if ENABLE(TOUCH_EVENTS)
    if (message.messageName() == Messages::TestWithoutAttributes::LoadSomethingElse::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomethingElse>(connection, WTFMove(message), this, &TestWithoutAttributes::loadSomethingElse);
#endif
    if (message.messageName() == Messages::TestWithoutAttributes::DidReceivePolicyDecision::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DidReceivePolicyDecision>(connection, WTFMove(message), this, &TestWithoutAttributes::didReceivePolicyDecision);
    if (message.messageName() == Messages::TestWithoutAttributes::Close::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::Close>(connection, WTFMove(message), this, &TestWithoutAttributes::close);
    if (message.messageName() == Messages::TestWithoutAttributes::PreferencesDidChange::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::PreferencesDidChange>(connection, WTFMove(message), this, &TestWithoutAttributes::preferencesDidChange);
    if (message.messageName() == Messages::TestWithoutAttributes::SendDoubleAndFloat::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SendDoubleAndFloat>(connection, WTFMove(message), this, &TestWithoutAttributes::sendDoubleAndFloat);
    if (message.messageName() == Messages::TestWithoutAttributes::SendInts::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SendInts>(connection, WTFMove(message), this, &TestWithoutAttributes::sendInts);
    if (message.messageName() == Messages::TestWithoutAttributes::CreatePlugin::name())
        return IPC::handleMessageAsync<Messages::TestWithoutAttributes::CreatePlugin>(connection, WTFMove(message), this, &TestWithoutAttributes::createPlugin);
    if (message.messageName() == Messages::TestWithoutAttributes::RunJavaScriptAlert::name())
        return IPC::handleMessageAsync<Messages::TestWithoutAttributes::RunJavaScriptAlert>(connection, WTFMove(message), this, &TestWithoutAttributes::runJavaScriptAlert);
    if (message.messageName() == Messages::TestWithoutAttributes::GetPlugins::name())
        return IPC::handleMessageAsync<Messages::TestWithoutAttributes::GetPlugins>(connection, WTFMove(message), this, &TestWithoutAttributes::getPlugins);
    if (message.messageName() == Messages::TestWithoutAttributes::TestParameterAttributes::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TestParameterAttributes>(connection, WTFMove(message), this, &TestWithoutAttributes::testParameterAttributes);
    if (message.messageName() == Messages::TestWithoutAttributes::TemplateTest::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TemplateTest>(connection, WTFMove(message), this, &TestWithoutAttributes::templateTest);
    if (message.messageName() == Messages::TestWithoutAttributes::SetVideoLayerID::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SetVideoLayerID>(connection, WTFMove(message), this, &TestWithoutAttributes::setVideoLayerID);
#if PLATFORM(MAC)
    if (message.messageName() == Messages::TestWithoutAttributes::DidCreateWebProcessConnection::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DidCreateWebProcessConnection>(connection, WTFMove(message), this, &TestWithoutAttributes::didCreateWebProcessConnection);
    if (message.messageName() == Messages::TestWithoutAttributes::InterpretKeyEvent::name())
        return IPC::handleMessageAsync<Messages::TestWithoutAttributes::InterpretKeyEvent>(connection, WTFMove(message), this, &TestWithoutAttributes::interpretKeyEvent);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (message.messageName() == Messages::TestWithoutAttributes::DeprecatedOperation::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DeprecatedOperation>(connection, WTFMove(message), this, &TestWithoutAttributes::deprecatedOperation);
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    if (message.messageName() == Messages::TestWithoutAttributes::ExperimentalOperation::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::ExperimentalOperation>(connection, WTFMove(message), this, &TestWithoutAttributes::experimentalOperation);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(message);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(message.messageName()), message.destinationID);
}

bool TestWithoutAttributes::didReceiveSyncMessage(IPC::Connection& connection, IPC::Message& message, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithoutAttributes::GetPluginProcessConnection::name())
        return IPC::handleMessageSynchronous<Messages::TestWithoutAttributes::GetPluginProcessConnection>(connection, WTFMove(message), replyEncoder, this, &TestWithoutAttributes::getPluginProcessConnection);
    if (message.messageName() == Messages::TestWithoutAttributes::TestMultipleAttributes::name())
        return IPC::handleMessageSynchronous<Messages::TestWithoutAttributes::TestMultipleAttributes>(connection, WTFMove(message), replyEncoder, this, &TestWithoutAttributes::testMultipleAttributes);
    UNUSED_PARAM(connection);
    UNUSED_PARAM(message);
    UNUSED_PARAM(replyEncoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return false;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled synchronous message %s to %" PRIu64, description(message.messageName()), message.destinationID);
    return false;
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadURL>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadURL::Arguments>(globalObject, message);
}
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomething>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadSomething::Arguments>(globalObject, message);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TouchEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TouchEvent::Arguments>(globalObject, message);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_AddEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::AddEvent::Arguments>(globalObject, message);
}
#endif
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomethingElse>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadSomethingElse::Arguments>(globalObject, message);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidReceivePolicyDecision>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DidReceivePolicyDecision::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_Close>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::Close::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_PreferencesDidChange>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::PreferencesDidChange::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendDoubleAndFloat>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SendDoubleAndFloat::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendInts>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SendInts::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_CreatePlugin>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::CreatePlugin::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_CreatePlugin>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::CreatePlugin::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::RunJavaScriptAlert::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::RunJavaScriptAlert::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPlugins>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPlugins::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPlugins>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPlugins::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPluginProcessConnection::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPluginProcessConnection::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestMultipleAttributes::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestMultipleAttributes::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestParameterAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestParameterAttributes::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TemplateTest>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TemplateTest::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SetVideoLayerID>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SetVideoLayerID::Arguments>(globalObject, message);
}
#if PLATFORM(MAC)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidCreateWebProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DidCreateWebProcessConnection::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::InterpretKeyEvent::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::InterpretKeyEvent::ReplyArguments>(globalObject, message);
}
#endif
#if ENABLE(DEPRECATED_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DeprecatedOperation>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DeprecatedOperation::Arguments>(globalObject, message);
}
#endif
#if ENABLE(FEATURE_FOR_TESTING)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_ExperimentalOperation>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::ExperimentalOperation::Arguments>(globalObject, message);
}
#endif

}

#endif


#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
