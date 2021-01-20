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

void GetPluginProcessConnection::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection, const IPC::Connection::Handle& connectionHandle)
{
    *encoder << connectionHandle;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestMultipleAttributes::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace TestWithoutAttributes

} // namespace Messages

namespace WebKit {

void TestWithoutAttributes::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadURL::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::LoadURL>(decoder, this, &TestWithoutAttributes::loadURL);
        return;
    }
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadSomething::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomething>(decoder, this, &TestWithoutAttributes::loadSomething);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithoutAttributes::TouchEvent::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::TouchEvent>(decoder, this, &TestWithoutAttributes::touchEvent);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::TestWithoutAttributes::AddEvent::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::AddEvent>(decoder, this, &TestWithoutAttributes::addEvent);
        return;
    }
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::TestWithoutAttributes::LoadSomethingElse::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::LoadSomethingElse>(decoder, this, &TestWithoutAttributes::loadSomethingElse);
        return;
    }
#endif
    if (decoder.messageName() == Messages::TestWithoutAttributes::DidReceivePolicyDecision::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::DidReceivePolicyDecision>(decoder, this, &TestWithoutAttributes::didReceivePolicyDecision);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::Close::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::Close>(decoder, this, &TestWithoutAttributes::close);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::PreferencesDidChange::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::PreferencesDidChange>(decoder, this, &TestWithoutAttributes::preferencesDidChange);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::SendDoubleAndFloat::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::SendDoubleAndFloat>(decoder, this, &TestWithoutAttributes::sendDoubleAndFloat);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::SendInts::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::SendInts>(decoder, this, &TestWithoutAttributes::sendInts);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::TestParameterAttributes::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::TestParameterAttributes>(decoder, this, &TestWithoutAttributes::testParameterAttributes);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::TemplateTest::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::TemplateTest>(decoder, this, &TestWithoutAttributes::templateTest);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::SetVideoLayerID::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::SetVideoLayerID>(decoder, this, &TestWithoutAttributes::setVideoLayerID);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithoutAttributes::DidCreateWebProcessConnection::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::DidCreateWebProcessConnection>(decoder, this, &TestWithoutAttributes::didCreateWebProcessConnection);
        return;
    }
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::TestWithoutAttributes::DeprecatedOperation::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::DeprecatedOperation>(decoder, this, &TestWithoutAttributes::deprecatedOperation);
        return;
    }
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (decoder.messageName() == Messages::TestWithoutAttributes::ExperimentalOperation::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::ExperimentalOperation>(decoder, this, &TestWithoutAttributes::experimentalOperation);
        return;
    }
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    ASSERT_NOT_REACHED();
}

void TestWithoutAttributes::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithoutAttributes::CreatePlugin::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::CreatePlugin>(decoder, *replyEncoder, this, &TestWithoutAttributes::createPlugin);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::RunJavaScriptAlert::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::RunJavaScriptAlert>(decoder, *replyEncoder, this, &TestWithoutAttributes::runJavaScriptAlert);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::GetPlugins::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::GetPlugins>(decoder, *replyEncoder, this, &TestWithoutAttributes::getPlugins);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::GetPluginProcessConnection::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithoutAttributes::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &TestWithoutAttributes::getPluginProcessConnection);
        return;
    }
    if (decoder.messageName() == Messages::TestWithoutAttributes::TestMultipleAttributes::name()) {
        IPC::handleMessageSynchronousWantsConnection<Messages::TestWithoutAttributes::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &TestWithoutAttributes::testMultipleAttributes);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::TestWithoutAttributes::InterpretKeyEvent::name()) {
        IPC::handleMessage<Messages::TestWithoutAttributes::InterpretKeyEvent>(decoder, *replyEncoder, this, &TestWithoutAttributes::interpretKeyEvent);
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
