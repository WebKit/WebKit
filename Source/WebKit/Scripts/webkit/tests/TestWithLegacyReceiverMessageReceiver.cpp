/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

void GetPluginProcessConnection::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection, const IPC::Connection::Handle& connectionHandle)
{
    *encoder << connectionHandle;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestMultipleAttributes::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace TestWithLegacyReceiver

} // namespace Messages

namespace WebKit {

void TestWithLegacyReceiver::didReceiveTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadURL::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadURL>(decoder, this, &TestWithLegacyReceiver::loadURL);
        return;
    }
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomething::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomething>(decoder, this, &TestWithLegacyReceiver::loadSomething);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TouchEvent::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TouchEvent>(decoder, this, &TestWithLegacyReceiver::touchEvent);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::AddEvent::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::AddEvent>(decoder, this, &TestWithLegacyReceiver::addEvent);
        return;
    }
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::LoadSomethingElse::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::LoadSomethingElse>(decoder, this, &TestWithLegacyReceiver::loadSomethingElse);
        return;
    }
#endif
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidReceivePolicyDecision::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DidReceivePolicyDecision>(decoder, this, &TestWithLegacyReceiver::didReceivePolicyDecision);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::Close::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::Close>(decoder, this, &TestWithLegacyReceiver::close);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::PreferencesDidChange::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::PreferencesDidChange>(decoder, this, &TestWithLegacyReceiver::preferencesDidChange);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendDoubleAndFloat::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SendDoubleAndFloat>(decoder, this, &TestWithLegacyReceiver::sendDoubleAndFloat);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SendInts::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SendInts>(decoder, this, &TestWithLegacyReceiver::sendInts);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestParameterAttributes::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TestParameterAttributes>(decoder, this, &TestWithLegacyReceiver::testParameterAttributes);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TemplateTest::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::TemplateTest>(decoder, this, &TestWithLegacyReceiver::templateTest);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::SetVideoLayerID::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::SetVideoLayerID>(decoder, this, &TestWithLegacyReceiver::setVideoLayerID);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DidCreateWebProcessConnection>(decoder, this, &TestWithLegacyReceiver::didCreateWebProcessConnection);
        return;
    }
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::DeprecatedOperation::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::DeprecatedOperation>(decoder, this, &TestWithLegacyReceiver::deprecatedOperation);
        return;
    }
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::ExperimentalOperation::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::ExperimentalOperation>(decoder, this, &TestWithLegacyReceiver::experimentalOperation);
        return;
    }
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    ASSERT_NOT_REACHED();
}

void TestWithLegacyReceiver::didReceiveSyncTestWithLegacyReceiverMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::CreatePlugin::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::CreatePlugin>(decoder, *replyEncoder, this, &TestWithLegacyReceiver::createPlugin);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::RunJavaScriptAlert::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::RunJavaScriptAlert>(decoder, *replyEncoder, this, &TestWithLegacyReceiver::runJavaScriptAlert);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPlugins::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::GetPlugins>(decoder, *replyEncoder, this, &TestWithLegacyReceiver::getPlugins);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::GetPluginProcessConnection::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithLegacyReceiver::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::getPluginProcessConnection);
        return;
    }
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::TestMultipleAttributes::name()) {
        IPC::handleMessageSynchronousWantsConnection<Messages::TestWithLegacyReceiver::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &TestWithLegacyReceiver::testMultipleAttributes);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithLegacyReceiver::InterpretKeyEvent::name()) {
        IPC::handleMessage<Messages::TestWithLegacyReceiver::InterpretKeyEvent>(decoder, *replyEncoder, this, &TestWithLegacyReceiver::interpretKeyEvent);
        return;
    }
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(replyEncoder);
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
