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
#include "Connection.h" // NOLINT
#include "Decoder.h" // NOLINT
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(FEATURE_FOR_TESTING)
#include "DummyType.h" // NOLINT
#endif
#if PLATFORM(MAC)
#include "GestureTypes.h" // NOLINT
#endif
#include "HandleMessage.h" // NOLINT
#include "Plugin.h" // NOLINT
#include "TestWithLegacyReceiverMessages.h" // NOLINT
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

void TestWithLegacyReceiver::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadURL::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadURL>(connection, decoder, this, &TestWithLegacyReceiver::loadURL);
        return;
    }
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomething::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomething>(connection, decoder, this, &TestWithLegacyReceiver::loadSomething);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TouchEvent::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TouchEvent>(connection, decoder, this, &TestWithLegacyReceiver::touchEvent);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::AddEvent::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::AddEvent>(connection, decoder, this, &TestWithLegacyReceiver::addEvent);
        return;
    }
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomethingElse::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomethingElse>(connection, decoder, this, &TestWithLegacyReceiver::loadSomethingElse);
        return;
    }
#endif
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision>(connection, decoder, this, &TestWithLegacyReceiver::didReceivePolicyDecision);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::Close::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::Close>(connection, decoder, this, &TestWithLegacyReceiver::close);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::PreferencesDidChange::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::PreferencesDidChange>(connection, decoder, this, &TestWithLegacyReceiver::preferencesDidChange);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendDoubleAndFloat::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SendDoubleAndFloat>(connection, decoder, this, &TestWithLegacyReceiver::sendDoubleAndFloat);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendInts::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SendInts>(connection, decoder, this, &TestWithLegacyReceiver::sendInts);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::CreatePlugin::name()) {
        IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::CreatePlugin>(connection, decoder, this, &TestWithLegacyReceiver::createPlugin);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::RunJavaScriptAlert::name()) {
        IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::RunJavaScriptAlert>(connection, decoder, this, &TestWithLegacyReceiver::runJavaScriptAlert);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPlugins::name()) {
        IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::GetPlugins>(connection, decoder, this, &TestWithLegacyReceiver::getPlugins);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestParameterAttributes::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TestParameterAttributes>(connection, decoder, this, &TestWithLegacyReceiver::testParameterAttributes);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TemplateTest::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TemplateTest>(connection, decoder, this, &TestWithLegacyReceiver::templateTest);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SetVideoLayerID::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SetVideoLayerID>(connection, decoder, this, &TestWithLegacyReceiver::setVideoLayerID);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::OpaqueTypeSecurityAssertion::name()) {
        IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::OpaqueTypeSecurityAssertion>(connection, decoder, this, &TestWithLegacyReceiver::opaqueTypeSecurityAssertion);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection>(connection, decoder, this, &TestWithLegacyReceiver::didCreateWebProcessConnection);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::InterpretKeyEvent::name()) {
        IPC::handleMessageAsync<Messages::TestWithLegacyReceiver::InterpretKeyEvent>(connection, decoder, this, &TestWithLegacyReceiver::interpretKeyEvent);
        return;
    }
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DeprecatedOperation::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DeprecatedOperation>(connection, decoder, this, &TestWithLegacyReceiver::deprecatedOperation);
        return;
    }
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::ExperimentalOperation::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::ExperimentalOperation>(connection, decoder, this, &TestWithLegacyReceiver::experimentalOperation);
        return;
    }
#endif
#if PLATFORM(MAC)
#endif
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

void TestWithLegacyReceiver::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPluginProcessConnection::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::getPluginProcessConnection);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestMultipleAttributes::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::testMultipleAttributes);
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(replyEncoder);
    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadURL>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadURL::Arguments>(globalObject, decoder);
}
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomething>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadSomething::Arguments>(globalObject, decoder);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TouchEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TouchEvent::Arguments>(globalObject, decoder);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_AddEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::AddEvent::Arguments>(globalObject, decoder);
}
#endif
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_LoadSomethingElse>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::LoadSomethingElse::Arguments>(globalObject, decoder);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_Close>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::Close::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_PreferencesDidChange>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::PreferencesDidChange::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendDoubleAndFloat>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SendDoubleAndFloat::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SendInts>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SendInts::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_CreatePlugin>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::CreatePlugin::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_CreatePlugin>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::CreatePlugin::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::RunJavaScriptAlert::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::RunJavaScriptAlert::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPlugins>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPlugins::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPlugins>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPlugins::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPluginProcessConnection::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPluginProcessConnection::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestMultipleAttributes::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestMultipleAttributes::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TestParameterAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TestParameterAttributes::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_TemplateTest>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::TemplateTest::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_SetVideoLayerID>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::SetVideoLayerID::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::OpaqueTypeSecurityAssertion::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::OpaqueTypeSecurityAssertion::ReplyArguments>(globalObject, decoder);
}
#if PLATFORM(MAC)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::InterpretKeyEvent::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithLegacyReceiver_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::InterpretKeyEvent::ReplyArguments>(globalObject, decoder);
}
#endif
#if ENABLE(DEPRECATED_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_DeprecatedOperation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::DeprecatedOperation::Arguments>(globalObject, decoder);
}
#endif
#if ENABLE(FEATURE_FOR_TESTING)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_ExperimentalOperation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::ExperimentalOperation::Arguments>(globalObject, decoder);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_CreatePluginReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::CreatePluginReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::RunJavaScriptAlertReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_GetPluginsReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::GetPluginsReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::OpaqueTypeSecurityAssertionReply::Arguments>(globalObject, decoder);
}
#if PLATFORM(MAC)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithLegacyReceiver_InterpretKeyEventReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithLegacyReceiver::InterpretKeyEventReply::Arguments>(globalObject, decoder);
}
#endif

}

#endif


#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
