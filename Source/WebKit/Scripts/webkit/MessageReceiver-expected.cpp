/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "WebPage.h"

#include "ArgumentCoders.h"
#include "Connection.h"
#include "Decoder.h"
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(EXPERIMENTAL_FEATURE)
#include "DummyType.h"
#endif
#include "HandleMessage.h"
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#include "Plugin.h"
#include "WebCoreArgumentCoders.h"
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION)) || (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
#include "WebEvent.h"
#endif
#include "WebPageMessages.h"
#include "WebPreferencesStore.h"
#include <WebCore/GraphicsLayer.h>
#if PLATFORM(MAC)
#include <WebCore/KeyboardEvent.h>
#endif
#include <WebCore/PluginData.h>
#include <utility>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Messages {

namespace WebPage {

void GetPluginProcessConnection::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection, const IPC::Connection::Handle& connectionHandle)
{
    *encoder << connectionHandle;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestMultipleAttributes::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace WebPage

} // namespace Messages

namespace WebKit {

void WebPage::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::WebPage::LoadURL::name()) {
        IPC::handleMessage<Messages::WebPage::LoadURL>(decoder, this, &WebPage::loadURL);
        return;
    }
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::WebPage::LoadSomething::name()) {
        IPC::handleMessage<Messages::WebPage::LoadSomething>(decoder, this, &WebPage::loadSomething);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::WebPage::TouchEvent::name()) {
        IPC::handleMessage<Messages::WebPage::TouchEvent>(decoder, this, &WebPage::touchEvent);
        return;
    }
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    if (decoder.messageName() == Messages::WebPage::AddEvent::name()) {
        IPC::handleMessage<Messages::WebPage::AddEvent>(decoder, this, &WebPage::addEvent);
        return;
    }
#endif
#if ENABLE(TOUCH_EVENTS)
    if (decoder.messageName() == Messages::WebPage::LoadSomethingElse::name()) {
        IPC::handleMessage<Messages::WebPage::LoadSomethingElse>(decoder, this, &WebPage::loadSomethingElse);
        return;
    }
#endif
    if (decoder.messageName() == Messages::WebPage::DidReceivePolicyDecision::name()) {
        IPC::handleMessage<Messages::WebPage::DidReceivePolicyDecision>(decoder, this, &WebPage::didReceivePolicyDecision);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::Close::name()) {
        IPC::handleMessage<Messages::WebPage::Close>(decoder, this, &WebPage::close);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::PreferencesDidChange::name()) {
        IPC::handleMessage<Messages::WebPage::PreferencesDidChange>(decoder, this, &WebPage::preferencesDidChange);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::SendDoubleAndFloat::name()) {
        IPC::handleMessage<Messages::WebPage::SendDoubleAndFloat>(decoder, this, &WebPage::sendDoubleAndFloat);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::SendInts::name()) {
        IPC::handleMessage<Messages::WebPage::SendInts>(decoder, this, &WebPage::sendInts);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::TestParameterAttributes::name()) {
        IPC::handleMessage<Messages::WebPage::TestParameterAttributes>(decoder, this, &WebPage::testParameterAttributes);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::TemplateTest::name()) {
        IPC::handleMessage<Messages::WebPage::TemplateTest>(decoder, this, &WebPage::templateTest);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::SetVideoLayerID::name()) {
        IPC::handleMessage<Messages::WebPage::SetVideoLayerID>(decoder, this, &WebPage::setVideoLayerID);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::WebPage::DidCreateWebProcessConnection::name()) {
        IPC::handleMessage<Messages::WebPage::DidCreateWebProcessConnection>(decoder, this, &WebPage::didCreateWebProcessConnection);
        return;
    }
#endif
#if ENABLE(DEPRECATED_FEATURE)
    if (decoder.messageName() == Messages::WebPage::DeprecatedOperation::name()) {
        IPC::handleMessage<Messages::WebPage::DeprecatedOperation>(decoder, this, &WebPage::deprecatedOperation);
        return;
    }
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    if (decoder.messageName() == Messages::WebPage::ExperimentalOperation::name()) {
        IPC::handleMessage<Messages::WebPage::ExperimentalOperation>(decoder, this, &WebPage::experimentalOperation);
        return;
    }
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    ASSERT_NOT_REACHED();
}

void WebPage::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageName() == Messages::WebPage::CreatePlugin::name()) {
        IPC::handleMessage<Messages::WebPage::CreatePlugin>(decoder, *replyEncoder, this, &WebPage::createPlugin);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::RunJavaScriptAlert::name()) {
        IPC::handleMessage<Messages::WebPage::RunJavaScriptAlert>(decoder, *replyEncoder, this, &WebPage::runJavaScriptAlert);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::GetPlugins::name()) {
        IPC::handleMessage<Messages::WebPage::GetPlugins>(decoder, *replyEncoder, this, &WebPage::getPlugins);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::GetPluginProcessConnection::name()) {
        IPC::handleMessageSynchronous<Messages::WebPage::GetPluginProcessConnection>(connection, decoder, replyEncoder, this, &WebPage::getPluginProcessConnection);
        return;
    }
    if (decoder.messageName() == Messages::WebPage::TestMultipleAttributes::name()) {
        IPC::handleMessageSynchronousWantsConnection<Messages::WebPage::TestMultipleAttributes>(connection, decoder, replyEncoder, this, &WebPage::testMultipleAttributes);
        return;
    }
#if PLATFORM(MAC)
    if (decoder.messageName() == Messages::WebPage::InterpretKeyEvent::name()) {
        IPC::handleMessage<Messages::WebPage::InterpretKeyEvent>(decoder, *replyEncoder, this, &WebPage::interpretKeyEvent);
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
