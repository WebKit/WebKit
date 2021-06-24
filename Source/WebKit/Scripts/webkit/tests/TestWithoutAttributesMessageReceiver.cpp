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

#include "ArgumentCoders.h"
#include "Connection.h"
#include "Decoder.h"
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(EXPERIMENTAL_FEATURE)
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
#include "TestWithoutAttributesMessages.h"
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

namespace TestWithoutAttributes {

void GetPluginProcessConnection::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, const IPC::Connection::Handle& connectionHandle)
{
    encoder.get() << connectionHandle;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestMultipleAttributes::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace TestWithoutAttributes

} // namespace Messages

namespace WebKit {

void TestWithoutAttributes::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadURL>(decoder, this, &TestWithoutAttributes::loadURL);
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadSomething::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomething>(decoder, this, &TestWithoutAttributes::loadSomething);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithoutAttributes::TouchEvent::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TouchEvent>(decoder, this, &TestWithoutAttributes::touchEvent);
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithoutAttributes::AddEvent::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::AddEvent>(decoder, this, &TestWithoutAttributes::addEvent);
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadSomethingElse::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomethingElse>(decoder, this, &TestWithoutAttributes::loadSomethingElse);
#endif
    if (decoder.messageName() == Messages::TestWithoutAttributes::DidReceivePolicyDecision::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DidReceivePolicyDecision>(decoder, this, &TestWithoutAttributes::didReceivePolicyDecision);
    if (decoder.messageName() == Messages::TestWithoutAttributes::Close::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::Close>(decoder, this, &TestWithoutAttributes::close);
    if (decoder.messageName() == Messages::TestWithoutAttributes::PreferencesDidChange::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::PreferencesDidChange>(decoder, this, &TestWithoutAttributes::preferencesDidChange);
    if (decoder.messageName() == Messages::TestWithoutAttributes::SendDoubleAndFloat::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SendDoubleAndFloat>(decoder, this, &TestWithoutAttributes::sendDoubleAndFloat);
    if (decoder.messageName() == Messages::TestWithoutAttributes::SendInts::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SendInts>(decoder, this, &TestWithoutAttributes::sendInts);
    if (decoder.messageName() == Messages::TestWithoutAttributes::TestParameterAttributes::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TestParameterAttributes>(decoder, this, &TestWithoutAttributes::testParameterAttributes);
    if (decoder.messageName() == Messages::TestWithoutAttributes::TemplateTest::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::TemplateTest>(decoder, this, &TestWithoutAttributes::templateTest);
    if (decoder.messageName() == Messages::TestWithoutAttributes::SetVideoLayerID::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::SetVideoLayerID>(decoder, this, &TestWithoutAttributes::setVideoLayerID);
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithoutAttributes::DidCreateWebProcessConnection::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DidCreateWebProcessConnection>(decoder, this, &TestWithoutAttributes::didCreateWebProcessConnection);
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::TestWithoutAttributes::DeprecatedOperation::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::DeprecatedOperation>(decoder, this, &TestWithoutAttributes::deprecatedOperation);
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (decoder.messageName() == Messages::TestWithoutAttributes::ExperimentalOperation::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::ExperimentalOperation>(decoder, this, &TestWithoutAttributes::experimentalOperation);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, description(decoder.messageName()), decoder.destinationID());
}

bool TestWithoutAttributes::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithoutAttributes::CreatePlugin::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::CreatePlugin>(decoder, *replyEncoder, this, &TestWithoutAttributes::createPlugin);
    if (decoder.messageName() == Messages::TestWithoutAttributes::RunJavaScriptAlert::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::RunJavaScriptAlert>(decoder, *replyEncoder, this, &TestWithoutAttributes::runJavaScriptAlert);
    if (decoder.messageName() == Messages::TestWithoutAttributes::GetPlugins::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::GetPlugins>(decoder, *replyEncoder, this, &TestWithoutAttributes::getPlugins);
    if (decoder.messageName() == Messages::TestWithoutAttributes::GetPluginProcessConnection::name())
        return IPC::handleMessageSynchronous<Messages::TestWithoutAttributes::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &TestWithoutAttributes::getPluginProcessConnection);
    if (decoder.messageName() == Messages::TestWithoutAttributes::TestMultipleAttributes::name())
        return IPC::handleMessageSynchronousWantsConnection<Messages::TestWithoutAttributes::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &TestWithoutAttributes::testMultipleAttributes);
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithoutAttributes::InterpretKeyEvent::name())
        return IPC::handleMessage<Messages::TestWithoutAttributes::InterpretKeyEvent>(decoder, *replyEncoder, this, &TestWithoutAttributes::interpretKeyEvent);
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
