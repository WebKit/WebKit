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
#include "TestWithLegacyReceiver.h"

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
#include "TestWithLegacyReceiverMessages.h" // NOLINT
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

void TestWithLegacyReceiver::didReceiveTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Message&& message)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithLegacyReceiver::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadURL>(connection, WTFMove(message), this, &TestWithLegacyReceiver::loadURL);
#if ENABLE(TOUCH_EVENTS)
    if (message.messageName() == Messages::TestWithLegacyReceiver::LoadSomething::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomething>(connection, WTFMove(message), this, &TestWithLegacyReceiver::loadSomething);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (message.messageName() == Messages::TestWithLegacyReceiver::TouchEvent::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TouchEvent>(connection, WTFMove(message), this, &TestWithLegacyReceiver::touchEvent);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (message.messageName() == Messages::TestWithLegacyReceiver::AddEvent::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::AddEvent>(connection, WTFMove(message), this, &TestWithLegacyReceiver::addEvent);
#endif
#if ENABLE(TOUCH_EVENTS)
    if (message.messageName() == Messages::TestWithLegacyReceiver::LoadSomethingElse::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomethingElse>(connection, WTFMove(message), this, &TestWithLegacyReceiver::loadSomethingElse);
#endif
    if (message.messageName() == Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision>(connection, WTFMove(message), this, &TestWithLegacyReceiver::didReceivePolicyDecision);
    if (message.messageName() == Messages::TestWithLegacyReceiver::Close::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::Close>(connection, WTFMove(message), this, &TestWithLegacyReceiver::close);
    if (message.messageName() == Messages::TestWithLegacyReceiver::PreferencesDidChange::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::PreferencesDidChange>(connection, WTFMove(message), this, &TestWithLegacyReceiver::preferencesDidChange);
    if (message.messageName() == Messages::TestWithLegacyReceiver::SendDoubleAndFloat::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SendDoubleAndFloat>(connection, WTFMove(message), this, &TestWithLegacyReceiver::sendDoubleAndFloat);
    if (message.messageName() == Messages::TestWithLegacyReceiver::SendInts::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SendInts>(connection, WTFMove(message), this, &TestWithLegacyReceiver::sendInts);
    if (message.messageName() == Messages::TestWithLegacyReceiver::CreatePlugin::name())
        return IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::CreatePlugin>(connection, WTFMove(message), this, &TestWithLegacyReceiver::createPlugin);
    if (message.messageName() == Messages::TestWithLegacyReceiver::RunJavaScriptAlert::name())
        return IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::RunJavaScriptAlert>(connection, WTFMove(message), this, &TestWithLegacyReceiver::runJavaScriptAlert);
    if (message.messageName() == Messages::TestWithLegacyReceiver::GetPlugins::name())
        return IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::GetPlugins>(connection, WTFMove(message), this, &TestWithLegacyReceiver::getPlugins);
    if (message.messageName() == Messages::TestWithLegacyReceiver::TestParameterAttributes::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TestParameterAttributes>(connection, WTFMove(message), this, &TestWithLegacyReceiver::testParameterAttributes);
    if (message.messageName() == Messages::TestWithLegacyReceiver::TemplateTest::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TemplateTest>(connection, WTFMove(message), this, &TestWithLegacyReceiver::templateTest);
    if (message.messageName() == Messages::TestWithLegacyReceiver::SetVideoLayerID::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SetVideoLayerID>(connection, WTFMove(message), this, &TestWithLegacyReceiver::setVideoLayerID);
#if PLATFORM(MAC)
    if (message.messageName() == Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection>(connection, WTFMove(message), this, &TestWithLegacyReceiver::didCreateWebProcessConnection);
    if (message.messageName() == Messages::TestWithLegacyReceiver::InterpretKeyEvent::name())
        return IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::InterpretKeyEvent>(connection, WTFMove(message), this, &TestWithLegacyReceiver::interpretKeyEvent);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (message.messageName() == Messages::TestWithLegacyReceiver::DeprecatedOperation::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DeprecatedOperation>(connection, WTFMove(message), this, &TestWithLegacyReceiver::deprecatedOperation);
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    if (message.messageName() == Messages::TestWithLegacyReceiver::ExperimentalOperation::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::ExperimentalOperation>(connection, WTFMove(message), this, &TestWithLegacyReceiver::experimentalOperation);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(message);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(message.messageName()), message.destinationID);
}

bool TestWithLegacyReceiver::didReceiveSyncTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Message& message, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithLegacyReceiver::GetPluginProcessConnection::name())
        return IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::GetPluginProcessConnection>(connection, WTFMove(message), replyEncoder, this, &TestWithLegacyReceiver::getPluginProcessConnection);
    if (message.messageName() == Messages::TestWithLegacyReceiver::TestMultipleAttributes::name())
        return IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::TestMultipleAttributes>(connection, WTFMove(message), replyEncoder, this, &TestWithLegacyReceiver::testMultipleAttributes);
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

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadURL>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadURL::Arguments>(globalObject, message);
}
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomething>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadSomething::Arguments>(globalObject, message);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TouchEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TouchEvent::Arguments>(globalObject, message);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_AddEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::AddEvent::Arguments>(globalObject, message);
}
#endif
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomethingElse>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadSomethingElse::Arguments>(globalObject, message);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_Close>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::Close::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_PreferencesDidChange>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::PreferencesDidChange::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendDoubleAndFloat>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SendDoubleAndFloat::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendInts>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SendInts::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_CreatePlugin>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::CreatePlugin::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_CreatePlugin>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::CreatePlugin::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::RunJavaScriptAlert::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::RunJavaScriptAlert::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPlugins>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPlugins::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPlugins>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPlugins::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPluginProcessConnection::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPluginProcessConnection::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestMultipleAttributes::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestMultipleAttributes::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestParameterAttributes>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestParameterAttributes::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TemplateTest>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TemplateTest::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SetVideoLayerID>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SetVideoLayerID::Arguments>(globalObject, message);
}
#if PLATFORM(MAC)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::InterpretKeyEvent::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::InterpretKeyEvent::ReplyArguments>(globalObject, message);
}
#endif
#if ENABLE(DEPRECATED_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DeprecatedOperation>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DeprecatedOperation::Arguments>(globalObject, message);
}
#endif
#if ENABLE(FEATURE_FOR_TESTING)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_ExperimentalOperation>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::ExperimentalOperation::Arguments>(globalObject, message);
}
#endif

}

#endif


#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
