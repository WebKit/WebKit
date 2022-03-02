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
#include "TestWithLegacyReceiver.h"

#include "ArgumentCoders.h"
#include "Connection.h"
#include "Decoder.h"
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(FEATURE_FOR_TESTING)
#include "DummyType.h"
#endif
#if PLATFORM(MAC)
#include "GestureTypes.h"
#endif
#include "HandleMessage.h"
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#include "Plugin.h"
#include "TestWithLegacyReceiverMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPreferencesStore.h"
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION)) || (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
#include "WebTouchEvent.h"
#endif
#include <WebCore/GraphicsLayer.h>
#if PLATFORM(MAC)
#include <WebCore/KeyboardEvent.h>
#endif
#include <WebCore/PluginData.h>
#include <utility>
#include <wtf/HashMap.h>
#if PLATFORM(MAC)
#include <wtf/OptionSet.h>
#endif
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Messages {

namespace TestWithLegacyReceiver {

void GetPluginProcessConnection::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, const IPC::Connection::Handle& connectionHandle)
{
    encoder.get() << connectionHandle;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestMultipleAttributes::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace TestWithLegacyReceiver

} // namespace Messages

namespace WebKit {

void TestWithLegacyReceiver::didReceiveTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadURL>(connection, decoder, this, &TestWithLegacyReceiver::loadURL);
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomething::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomething>(connection, decoder, this, &TestWithLegacyReceiver::loadSomething);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TouchEvent::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TouchEvent>(connection, decoder, this, &TestWithLegacyReceiver::touchEvent);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::AddEvent::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::AddEvent>(connection, decoder, this, &TestWithLegacyReceiver::addEvent);
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomethingElse::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomethingElse>(connection, decoder, this, &TestWithLegacyReceiver::loadSomethingElse);
#endif
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision>(connection, decoder, this, &TestWithLegacyReceiver::didReceivePolicyDecision);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::Close::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::Close>(connection, decoder, this, &TestWithLegacyReceiver::close);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::PreferencesDidChange::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::PreferencesDidChange>(connection, decoder, this, &TestWithLegacyReceiver::preferencesDidChange);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendDoubleAndFloat::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SendDoubleAndFloat>(connection, decoder, this, &TestWithLegacyReceiver::sendDoubleAndFloat);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendInts::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SendInts>(connection, decoder, this, &TestWithLegacyReceiver::sendInts);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestParameterAttributes::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TestParameterAttributes>(connection, decoder, this, &TestWithLegacyReceiver::testParameterAttributes);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TemplateTest::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::TemplateTest>(connection, decoder, this, &TestWithLegacyReceiver::templateTest);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SetVideoLayerID::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::SetVideoLayerID>(connection, decoder, this, &TestWithLegacyReceiver::setVideoLayerID);
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection>(connection, decoder, this, &TestWithLegacyReceiver::didCreateWebProcessConnection);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DeprecatedOperation::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::DeprecatedOperation>(connection, decoder, this, &TestWithLegacyReceiver::deprecatedOperation);
#endif
#if ENABLE(FEATURE_FOR_TESTING)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::ExperimentalOperation::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::ExperimentalOperation>(connection, decoder, this, &TestWithLegacyReceiver::experimentalOperation);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());
}

bool TestWithLegacyReceiver::didReceiveSyncTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::CreatePlugin::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::CreatePlugin>(connection, decoder, *replyEncoder, this, &TestWithLegacyReceiver::createPlugin);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::RunJavaScriptAlert::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::RunJavaScriptAlert>(connection, decoder, *replyEncoder, this, &TestWithLegacyReceiver::runJavaScriptAlert);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPlugins::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::GetPlugins>(connection, decoder, *replyEncoder, this, &TestWithLegacyReceiver::getPlugins);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPluginProcessConnection::name())
        return IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::getPluginProcessConnection);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestMultipleAttributes::name())
        return IPC::handleMessageSynchronousWantsConnection<Messages::TestWithLegacyReceiver::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::testMultipleAttributes);
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::InterpretKeyEvent::name())
        return IPC::handleMessage<Messages::TestWithLegacyReceiver::InterpretKeyEvent>(connection, decoder, *replyEncoder, this, &TestWithLegacyReceiver::interpretKeyEvent);
#endif
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

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
