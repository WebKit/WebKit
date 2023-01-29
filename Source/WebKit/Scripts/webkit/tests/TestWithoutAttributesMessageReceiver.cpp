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
#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
#include "TestWithoutAttributes.h"

#include "ArgumentCoders.h" // NOLINT
#if PLATFORM(MAC)
#include "ArgumentCodersDarwin.h" // NOLINT
#endif
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
#include "TestWithoutAttributesMessages.h" // NOLINT
#include "WebCoreArgumentCoders.h" // NOLINT
#include "WebPreferencesStore.h" // NOLINT
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION)) || (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
#include "WebTouchEvent.h" // NOLINT
#endif
#include <WebCore/GraphicsLayer.h> // NOLINT
#if PLATFORM(MAC)
#include <WebCore/KeyboardEvent.h> // NOLINT
#endif
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

void TestWithoutAttributes::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };

    using AsyncMessageHandlerListType = IPC::MessageHandlerList<bool(*)(IPC::HandleMessageContext<IPC::Connection>, TestWithoutAttributes*), TestWithoutAttributes,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::AddEvent, &TestWithoutAttributes::addEvent>,
#endif
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::Close, &TestWithoutAttributes::close>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::CreatePlugin, &TestWithoutAttributes::createPlugin>,
#if ENABLE(DEPRECATED_FEATURE)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::DeprecatedOperation, &TestWithoutAttributes::deprecatedOperation>,
#endif
#if PLATFORM(MAC)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::DidCreateWebProcessConnection, &TestWithoutAttributes::didCreateWebProcessConnection>,
#endif
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::DidReceivePolicyDecision, &TestWithoutAttributes::didReceivePolicyDecision>,
#if ENABLE(FEATURE_FOR_TESTING)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::ExperimentalOperation, &TestWithoutAttributes::experimentalOperation>,
#endif
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::GetPlugins, &TestWithoutAttributes::getPlugins>,
#if PLATFORM(MAC)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::InterpretKeyEvent, &TestWithoutAttributes::interpretKeyEvent>,
#endif
#if ENABLE(TOUCH_EVENTS)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::LoadSomething, &TestWithoutAttributes::loadSomething>,
#endif
#if ENABLE(TOUCH_EVENTS)
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::LoadSomethingElse, &TestWithoutAttributes::loadSomethingElse>,
#endif
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::LoadURL, &TestWithoutAttributes::loadURL>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::PreferencesDidChange, &TestWithoutAttributes::preferencesDidChange>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::RunJavaScriptAlert, &TestWithoutAttributes::runJavaScriptAlert>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::SendDoubleAndFloat, &TestWithoutAttributes::sendDoubleAndFloat>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::SendInts, &TestWithoutAttributes::sendInts>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::SetVideoLayerID, &TestWithoutAttributes::setVideoLayerID>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::TemplateTest, &TestWithoutAttributes::templateTest>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::TestParameterAttributes, &TestWithoutAttributes::testParameterAttributes>,
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::TouchEvent, &TestWithoutAttributes::touchEvent>,
#endif
        IPC::MessageHandlerListEntry<void, nullptr>>;
    static_assert(AsyncMessageHandlerListType::valid());
    if (auto handler = AsyncMessageHandlerListType::messageHandler(decoder.messageName()))
        return std::void_t<>(handler({ connection, decoder }, this));

    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());
}

bool TestWithoutAttributes::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };

    using SyncMessageHandlerListType = IPC::MessageHandlerList<bool(*)(IPC::HandleMessageContext<IPC::Connection>, TestWithoutAttributes*), TestWithoutAttributes,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::GetPluginProcessConnection, &TestWithoutAttributes::getPluginProcessConnection>,
        IPC::MessageHandlerListEntry<Messages::TestWithoutAttributes::TestMultipleAttributes, &TestWithoutAttributes::testMultipleAttributes>,
        IPC::MessageHandlerListEntry<void, nullptr>>;
    static_assert(SyncMessageHandlerListType::valid());
    if (auto handler = SyncMessageHandlerListType::messageHandler(decoder.messageName()))
        return handler({ connection, decoder, replyEncoder }, this);

    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(replyEncoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return false;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()), decoder.destinationID());
    return false;
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadURL>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadURL::Arguments>(globalObject, decoder);
}
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomething>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadSomething::Arguments>(globalObject, decoder);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TouchEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TouchEvent::Arguments>(globalObject, decoder);
}
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_AddEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::AddEvent::Arguments>(globalObject, decoder);
}
#endif
#if ENABLE(TOUCH_EVENTS)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_LoadSomethingElse>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::LoadSomethingElse::Arguments>(globalObject, decoder);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidReceivePolicyDecision>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DidReceivePolicyDecision::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_Close>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::Close::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_PreferencesDidChange>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::PreferencesDidChange::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendDoubleAndFloat>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SendDoubleAndFloat::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SendInts>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SendInts::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_CreatePlugin>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::CreatePlugin::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_CreatePlugin>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::CreatePlugin::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::RunJavaScriptAlert::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_RunJavaScriptAlert>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::RunJavaScriptAlert::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPlugins>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPlugins::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPlugins>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPlugins::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPluginProcessConnection::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_GetPluginProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::GetPluginProcessConnection::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestMultipleAttributes::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_TestMultipleAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestMultipleAttributes::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TestParameterAttributes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TestParameterAttributes::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_TemplateTest>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::TemplateTest::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_SetVideoLayerID>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::SetVideoLayerID::Arguments>(globalObject, decoder);
}
#if PLATFORM(MAC)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DidCreateWebProcessConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DidCreateWebProcessConnection::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::InterpretKeyEvent::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutAttributes_InterpretKeyEvent>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::InterpretKeyEvent::ReplyArguments>(globalObject, decoder);
}
#endif
#if ENABLE(DEPRECATED_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_DeprecatedOperation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::DeprecatedOperation::Arguments>(globalObject, decoder);
}
#endif
#if ENABLE(FEATURE_FOR_TESTING)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutAttributes_ExperimentalOperation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutAttributes::ExperimentalOperation::Arguments>(globalObject, decoder);
}
#endif

}

#endif


#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
